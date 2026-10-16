import Foundation
import Metal
import MetalKit

private struct JojoGpuVertex {
    var position: SIMD2<Float>
    var uv: SIMD2<Float>
    var color: SIMD4<Float>
}

private struct JojoBaseUniforms {
    var display: SIMD4<UInt32>
    var target: SIMD4<UInt32>
}

private struct JojoItemUniforms {
    var display: SIMD4<UInt32>
    var target: SIMD4<UInt32>
    var texture: SIMD4<UInt32>
    var control: SIMD4<UInt32>
}

private struct JojoDraw {
    var start: Int
    var count: Int
    var type: MTLPrimitiveType
    var minX: Int
    var minY: Int
    var maxX: Int
    var maxY: Int
    var uniforms: JojoItemUniforms
}

/// Metal presentation for the published PS1 GPU model. It only reads the locked C snapshot (gpu_raster_copy_published).
///
/// Pixel contract of the 384x240 target (rgba8Unorm): each colour channel holds a 5-bit PS1 value k as k/31,
/// alpha holds the mask bit. encodeFrame returns that target without presenting it, so an offscreen test can read it.
final class GpuMetalRenderer {
    static let width = 384
    static let height = 240
    private static let vramWidth = 1024
    private static let vramHeight = 512
    private static let itemCapacity = 8192
    private static let vramPixelCount = vramWidth * vramHeight

    private let device: MTLDevice
    private let vramTexture: MTLTexture
    private let targetA: MTLTexture
    private let targetB: MTLTexture
    private let basePipeline: MTLRenderPipelineState
    private let itemPipeline: MTLRenderPipelineState
    private let presentPipeline: MTLRenderPipelineState
    private let sampler: MTLSamplerState
    private let items: UnsafeMutablePointer<GpItem>
    private let vram: UnsafeMutablePointer<UInt16>

    /// `library` defaults to the bundle's default.metallib. Tests pass the metallib they load from disk.
    init?(device: MTLDevice, library: MTLLibrary? = nil) {
        guard let library = library ?? device.makeDefaultLibrary(),
              let baseVertex = library.makeFunction(name: "jojo_fullscreen_vertex"),
              let itemVertex = library.makeFunction(name: "jojo_gpu_vertex"),
              let baseFragment = library.makeFunction(name: "jojo_base_fragment"),
              let itemFragment = library.makeFunction(name: "jojo_item_fragment"),
              let presentFragment = library.makeFunction(name: "jojo_present_fragment") else { return nil }

        // Private storage: the CPU image reaches it through a blit in the same command buffer, so the GPU
        // never reads a texture that the CPU is writing for a later frame.
        let vramDesc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .r16Uint,
                                                                 width: Self.vramWidth,
                                                                 height: Self.vramHeight,
                                                                 mipmapped: false)
        vramDesc.storageMode = .private
        vramDesc.usage = [.shaderRead]
        guard let vramTexture = device.makeTexture(descriptor: vramDesc) else { return nil }

        let targetDesc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rgba8Unorm,
                                                                   width: Self.width,
                                                                   height: Self.height,
                                                                   mipmapped: false)
        targetDesc.storageMode = .private
        targetDesc.usage = [.renderTarget, .shaderRead]
        guard let targetA = device.makeTexture(descriptor: targetDesc),
              let targetB = device.makeTexture(descriptor: targetDesc) else { return nil }

        let basePipeline = Self.makePipeline(device: device, vertex: baseVertex,
                                             fragment: baseFragment, format: .rgba8Unorm)
        let itemPipeline = Self.makePipeline(device: device, vertex: itemVertex,
                                             fragment: itemFragment, format: .rgba8Unorm)
        let presentPipeline = Self.makePipeline(device: device, vertex: baseVertex,
                                                fragment: presentFragment, format: .bgra8Unorm)
        guard let basePipeline, let itemPipeline, let presentPipeline else { return nil }

        let samplerDesc = MTLSamplerDescriptor()
        samplerDesc.minFilter = .nearest
        samplerDesc.magFilter = .nearest
        guard let sampler = device.makeSamplerState(descriptor: samplerDesc) else { return nil }

        self.device = device
        self.vramTexture = vramTexture
        self.targetA = targetA
        self.targetB = targetB
        self.basePipeline = basePipeline
        self.itemPipeline = itemPipeline
        self.presentPipeline = presentPipeline
        self.sampler = sampler
        self.items = UnsafeMutablePointer<GpItem>.allocate(capacity: Self.itemCapacity)
        self.items.initialize(repeating: GpItem(), count: Self.itemCapacity)
        self.vram = UnsafeMutablePointer<UInt16>.allocate(capacity: Self.vramPixelCount)
        self.vram.initialize(repeating: 0, count: Self.vramPixelCount)
    }

    deinit {
        items.deinitialize(count: Self.itemCapacity)
        items.deallocate()
        vram.deinitialize(count: Self.vramPixelCount)
        vram.deallocate()
    }

    /// Encodes one frame into commandBuffer and returns the 384x240 result (not presented). nil when no snapshot is
    /// published yet or a Metal object could not be created.
    func encodeFrame(commandBuffer: MTLCommandBuffer) -> MTLTexture? {
        var count = 0
        var sequence: UInt64 = 0
        var display = GpuDisplayEnv()
        let copied = gpu_raster_copy_published(items, Self.itemCapacity, &count,
                                               vram, Self.vramPixelCount, &display, &sequence)
        guard copied != 0, count >= 0, count <= Self.itemCapacity else { return nil }
        _ = sequence

        // Upload the whole 1024x512 VRAM image through a fresh staging buffer, inside this command buffer.
        let vramBytes = Self.vramPixelCount * MemoryLayout<UInt16>.stride
        guard let staging = device.makeBuffer(bytes: vram, length: vramBytes, options: .storageModeShared),
              let upload = commandBuffer.makeBlitCommandEncoder() else { return nil }
        upload.copy(from: staging, sourceOffset: 0,
                    sourceBytesPerRow: Self.vramWidth * MemoryLayout<UInt16>.stride,
                    sourceBytesPerImage: vramBytes,
                    sourceSize: MTLSize(width: Self.vramWidth, height: Self.vramHeight, depth: 1),
                    to: vramTexture, destinationSlice: 0, destinationLevel: 0,
                    destinationOrigin: MTLOrigin(x: 0, y: 0, z: 0))
        upload.endEncoding()

        let dispX = Int(display.x)
        let dispY = Int(display.y)
        let dispW = max(Int(display.w), 1)
        let dispH = max(Int(display.h), 1)
        var base = JojoBaseUniforms(
            display: SIMD4(UInt32(dispX), UInt32(dispY), UInt32(dispW), UInt32(dispH)),
            target: SIMD4(UInt32(Self.width), UInt32(Self.height), UInt32(display.enabled), UInt32(display.is_rgb24)))

        guard let basePass = renderPass(targetA, clear: true),
              let baseEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: basePass) else { return nil }
        baseEncoder.setRenderPipelineState(basePipeline)
        baseEncoder.setFragmentTexture(vramTexture, index: 0)
        baseEncoder.setFragmentSamplerState(sampler, index: 0)
        baseEncoder.setFragmentBytes(&base, length: MemoryLayout<JojoBaseUniforms>.stride, index: 0)
        baseEncoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
        baseEncoder.endEncoding()

        var current = targetA
        var next = targetB
        if display.enabled != 0 {
            var draws: [JojoDraw] = []
            draws.reserveCapacity(count)
            var vertices: [JojoGpuVertex] = []
            vertices.reserveCapacity(count * 6)
            for i in 0..<count {
                append(item: items[i], display: display, draws: &draws, vertices: &vertices)
            }
            if !draws.isEmpty, vertices.count <= Self.itemCapacity * 6,
               let vertexBuffer = makeVertexBuffer(vertices) {
                for draw in draws {
                    guard draw.maxX > draw.minX, draw.maxY > draw.minY else { continue }
                    guard let blit = commandBuffer.makeBlitCommandEncoder() else { return nil }
                    blit.copy(from: current, sourceSlice: 0, sourceLevel: 0,
                              sourceOrigin: MTLOrigin(x: 0, y: 0, z: 0),
                              sourceSize: MTLSize(width: Self.width, height: Self.height, depth: 1),
                              to: next, destinationSlice: 0, destinationLevel: 0,
                              destinationOrigin: MTLOrigin(x: 0, y: 0, z: 0))
                    blit.endEncoding()

                    guard let pass = renderPass(next, clear: false),
                          let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) else { return nil }
                    encoder.setRenderPipelineState(itemPipeline)
                    encoder.setVertexBuffer(vertexBuffer, offset: 0, index: 0)
                    var uniforms = draw.uniforms
                    encoder.setVertexBytes(&uniforms, length: MemoryLayout<JojoItemUniforms>.stride, index: 1)
                    encoder.setFragmentBytes(&uniforms, length: MemoryLayout<JojoItemUniforms>.stride, index: 0)
                    encoder.setFragmentTexture(vramTexture, index: 0)
                    encoder.setFragmentTexture(current, index: 1)
                    encoder.setFragmentSamplerState(sampler, index: 0)
                    encoder.setFragmentSamplerState(sampler, index: 1)
                    encoder.setScissorRect(MTLScissorRect(x: draw.minX, y: draw.minY,
                                                          width: draw.maxX - draw.minX,
                                                          height: draw.maxY - draw.minY))
                    encoder.drawPrimitives(type: draw.type, vertexStart: draw.start, vertexCount: draw.count)
                    encoder.endEncoding()
                    swap(&current, &next)
                }
            }
        }
        return current
    }

    /// Encodes the frame, scales the 384x240 result to the drawable with nearest sampling, and presents it.
    /// When no frame can be encoded, the drawable is cleared and presented so the window does not keep stale pixels.
    func render(to view: MTKView, drawable: CAMetalDrawable, commandBuffer: MTLCommandBuffer) {
        guard let presentPass = view.currentRenderPassDescriptor else { return }
        guard let image = encodeFrame(commandBuffer: commandBuffer),
              let output = commandBuffer.makeRenderCommandEncoder(descriptor: presentPass) else {
            if let clear = commandBuffer.makeRenderCommandEncoder(descriptor: presentPass) { clear.endEncoding() }
            commandBuffer.present(drawable)
            return
        }
        var size = SIMD2<UInt32>(UInt32(max(view.drawableSize.width, 1)), UInt32(max(view.drawableSize.height, 1)))
        output.setRenderPipelineState(presentPipeline)
        output.setFragmentTexture(image, index: 0)
        output.setFragmentSamplerState(sampler, index: 0)
        output.setFragmentBytes(&size, length: MemoryLayout<SIMD2<UInt32>>.stride, index: 0)
        output.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
        output.endEncoding()
        commandBuffer.present(drawable)
    }

    private func makeVertexBuffer(_ vertices: [JojoGpuVertex]) -> MTLBuffer? {
        vertices.withUnsafeBytes { bytes in
            guard let base = bytes.baseAddress else { return nil }
            return device.makeBuffer(bytes: base, length: bytes.count, options: .storageModeShared)
        }
    }

    private static func makePipeline(device: MTLDevice, vertex: MTLFunction,
                                     fragment: MTLFunction, format: MTLPixelFormat) -> MTLRenderPipelineState? {
        let descriptor = MTLRenderPipelineDescriptor()
        descriptor.vertexFunction = vertex
        descriptor.fragmentFunction = fragment
        descriptor.colorAttachments[0].pixelFormat = format
        descriptor.colorAttachments[0].isBlendingEnabled = false
        return try? device.makeRenderPipelineState(descriptor: descriptor)
    }

    private func renderPass(_ texture: MTLTexture, clear: Bool) -> MTLRenderPassDescriptor? {
        let descriptor = MTLRenderPassDescriptor()
        guard let attachment = descriptor.colorAttachments[0] else { return nil }
        attachment.texture = texture
        attachment.loadAction = clear ? .clear : .load
        attachment.storeAction = .store
        attachment.clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 0)
        return descriptor
    }

    private func append(item: GpItem, display: GpuDisplayEnv,
                        draws: inout [JojoDraw], vertices: inout [JojoGpuVertex]) {
        let kind = Int(item.type)
        let xs = [Int(item.x.0), Int(item.x.1), Int(item.x.2), Int(item.x.3)]
        let ys = [Int(item.y.0), Int(item.y.1), Int(item.y.2), Int(item.y.3)]
        let rs = [Int(item.r.0), Int(item.r.1), Int(item.r.2), Int(item.r.3)]
        let gs = [Int(item.g.0), Int(item.g.1), Int(item.g.2), Int(item.g.3)]
        let bs = [Int(item.b.0), Int(item.b.1), Int(item.b.2), Int(item.b.3)]
        let us = [Int(item.u.0), Int(item.u.1), Int(item.u.2), Int(item.u.3)]
        let vs = [Int(item.v.0), Int(item.v.1), Int(item.v.2), Int(item.v.3)]
        // GP_ITEM_FILL, COPY_*, ENV and the rest return here: the CPU VRAM model has already applied them.
        var order: [Int]
        var primitive: MTLPrimitiveType
        switch kind {
        case 1: order = [0, 1, 2]; primitive = .triangle                 // GP_ITEM_TRI
        case 2: order = [0, 1, 2, 1, 3, 2]; primitive = .triangle        // GP_ITEM_QUAD
        case 3: order = [0, 1]; primitive = .line                        // GP_ITEM_LINE
        case 4:                                                          // GP_ITEM_RECT
            if item.w == 0 || item.h == 0 { return }
            order = [0, 1, 2, 3]; primitive = .triangleStrip
        default: return
        }

        let flags = UInt32(item.flags)
        let textured = (flags & 0x0002) != 0
        let raw = (flags & 0x0008) != 0
        let gouraud = (flags & 0x0001) != 0
        // Dither follows GP0(E1) bit 9 (flag 0x0200), but only where the hardware dithers (psxrecomp/accuracy/axis5_gpu.md, D1):
        // Gouraud polygons, modulated textured polygons and lines. Flat polygons, rects and sprites are never dithered.
        var dither = (flags & 0x0200) != 0
        switch kind {
        case 1, 2: dither = dither && (gouraud || (textured && !raw))
        case 3: break
        default: dither = false
        }
        let start = vertices.count
        var minX = Int.max, minY = Int.max, maxX = Int.min, maxY = Int.min
        for index in order {
            // A RECT packet carries one vertex (origin, colour, uv) plus a size. Its corners are derived here:
            // index 1 = (+w, 0), 2 = (0, +h), 3 = (+w, +h). UVs are not wrapped here; the shader masks them to 0..255.
            let corner = (index & 1, index >> 1)
            let src = kind == 4 ? 0 : index
            var x = Float(xs[src])
            var y = Float(ys[src])
            var u = Float(us[src])
            var v = Float(vs[src])
            if kind == 4 {
                x = Float(xs[0] + corner.0 * Int(item.w))
                y = Float(ys[0] + corner.1 * Int(item.h))
                u = Float(us[0] + corner.0 * Int(item.w))
                v = Float(vs[0] + corner.1 * Int(item.h))
            }
            let sx = (x - Float(display.x)) * Float(Self.width) / Float(max(Int(display.w), 1))
            let sy = (y - Float(display.y)) * Float(Self.height) / Float(max(Int(display.h), 1))
            minX = min(minX, Int(floor(sx))); maxX = max(maxX, Int(ceil(sx)))
            minY = min(minY, Int(floor(sy))); maxY = max(maxY, Int(ceil(sy)))
            vertices.append(JojoGpuVertex(position: SIMD2(sx, sy), uv: SIMD2(u, v),
                                          color: SIMD4(Float(rs[src]) / 255.0,
                                                       Float(gs[src]) / 255.0,
                                                       Float(bs[src]) / 255.0, 1.0)))
        }

        // A one-pixel-wide line can have zero extent on one axis; without a margin its scissor is empty and it is dropped.
        if kind == 3 { minX -= 1; minY -= 1; maxX += 1; maxY += 1 }
        let areaX0 = (Int(item.area_x0) - Int(display.x)) * Self.width / max(Int(display.w), 1)
        let areaY0 = (Int(item.area_y0) - Int(display.y)) * Self.height / max(Int(display.h), 1)
        let areaX1 = (Int(item.area_x1) + 1 - Int(display.x)) * Self.width / max(Int(display.w), 1)
        let areaY1 = (Int(item.area_y1) + 1 - Int(display.y)) * Self.height / max(Int(display.h), 1)
        minX = max(0, max(minX, areaX0)); minY = max(0, max(minY, areaY0))
        maxX = min(Self.width, min(maxX, areaX1)); maxY = min(Self.height, min(maxY, areaY1))
        let uniforms = JojoItemUniforms(
            display: SIMD4(UInt32(display.x), UInt32(display.y), UInt32(display.w), UInt32(display.h)),
            target: SIMD4(UInt32(Self.width), UInt32(Self.height), 1, dither ? 1 : 0),
            texture: SIMD4(flags, UInt32(item.tpage), UInt32(item.clut), UInt32(item.tex_window)),
            control: SIMD4(UInt32(item.semi_mode), UInt32((item.tpage >> 7) & 3),
                           (flags & 0x0080) != 0 ? 1 : 0,
                           (flags & 0x0100) != 0 ? 1 : 0))
        draws.append(JojoDraw(start: start, count: order.count, type: primitive,
                              minX: minX, minY: minY, maxX: maxX, maxY: maxY, uniforms: uniforms))
    }
}
