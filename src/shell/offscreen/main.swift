import Foundation
import Metal

// Offscreen self-test for GpuMetalRenderer. Hand-made GP0 draw items go through the same handoff the emulator uses
// (gpu_raster_*), are rendered into the 384x240 target without a window, are read back, and are compared with values
// worked out by hand. Each expectation's arithmetic is in its comment. Colour channels are 5-bit PS1 values.
// Usage: offscreen-test <path to default.metallib>. Exits 0 only when every check passes.

let W = 384
let H = 240

// GpItemType values (platform/gpu_gp0.h)
let TRI: UInt8 = 1, QUAD: UInt8 = 2, LINE: UInt8 = 3, RECT: UInt8 = 4
// flags (platform/gpu_gp0.h)
let GOURAUD: UInt16 = 0x0001, TEX: UInt16 = 0x0002, SEMI: UInt16 = 0x0004, RAW: UInt16 = 0x0008
let MASKSET: UInt16 = 0x0080, MASKCHECK: UInt16 = 0x0100, DITHER: UInt16 = 0x0200

struct Vtx { var x: Int; var y: Int; var r: Int; var g: Int; var b: Int; var u: Int; var v: Int }

func vtx(_ x: Int, _ y: Int, _ r: Int = 0, _ g: Int = 0, _ b: Int = 0, u: Int = 0, v: Int = 0) -> Vtx {
    Vtx(x: x, y: y, r: r, g: g, b: b, u: u, v: v)
}

/// Stores one element of a homogeneous C array (imported as a tuple).
func put<T, V>(_ tuple: inout T, _ index: Int, _ value: V) {
    withUnsafeMutableBytes(of: &tuple) { raw in
        raw.storeBytes(of: value, toByteOffset: index * MemoryLayout<V>.stride, as: V.self)
    }
}

func makeItem(_ type: UInt8, flags: UInt16, verts: [Vtx], w: Int = 0, h: Int = 0,
              semi: Int = 0, tpage: Int = 0, clut: Int = 0, window: Int = 0) -> GpItem {
    var it = GpItem()
    it.type = type
    it.vert_count = UInt8(verts.count)
    it.flags = flags
    it.semi_mode = UInt8(semi)
    it.tpage = UInt16(tpage)
    it.clut = UInt16(clut)
    it.tex_window = UInt32(window)
    it.w = UInt16(w)
    it.h = UInt16(h)
    it.off_x = 0
    it.off_y = 0
    it.area_x0 = 0; it.area_y0 = 0; it.area_x1 = 1023; it.area_y1 = 511
    for (i, vt) in verts.enumerated() {
        put(&it.x, i, Int16(vt.x)); put(&it.y, i, Int16(vt.y))
        put(&it.r, i, UInt8(vt.r)); put(&it.g, i, UInt8(vt.g)); put(&it.b, i, UInt8(vt.b))
        put(&it.u, i, UInt8(vt.u)); put(&it.v, i, UInt8(vt.v))
    }
    return it
}

// VRAM contents used by the textured cases.
// 4-bit texture at page (0,0): texels 0..3 of row 0 are indices 1, 2, 3, 15; texel 4 is index 0 (transparent).
let textureLoad: (Int, Int, Int, Int, [UInt32]) = (0, 0, 2, 1, [0x0000_F321])
// CLUT row 480 (clut word = 480 << 6): entry 1 = red, 2 = green, 3 = blue, 15 = 0x8010 (red 16, mask bit set).
var clutHalf = [UInt16](repeating: 0, count: 16)
clutHalf[1] = 0x001F; clutHalf[2] = 0x03E0; clutHalf[3] = 0x7C00; clutHalf[15] = 0x8010
var clutWords: [UInt32] = []
for i in 0..<8 { clutWords.append(UInt32(clutHalf[2 * i]) | (UInt32(clutHalf[2 * i + 1]) << 16)) }
let clutLoad: (Int, Int, Int, Int, [UInt32]) = (0, 480, 16, 1, clutWords)
let clutWord = 480 << 6

let bayer: [[Int]] = [[-4, 0, -3, 1], [2, -2, 3, -1], [-3, 1, -4, 0], [3, -1, 2, -2]]   // psx-spx / audit axis5_gpu.md

let device = MTLCreateSystemDefaultDevice()!
let queue = device.makeCommandQueue()!
let libraryPath = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "build/JojoShell.app/Contents/Resources/default.metallib"
let library: MTLLibrary
do {
    library = try device.makeLibrary(URL: URL(fileURLWithPath: libraryPath))
} catch {
    print("FAIL setup: cannot load \(libraryPath): \(error)")
    exit(1)
}
guard let renderer = GpuMetalRenderer(device: device, library: library) else {
    print("FAIL setup: GpuMetalRenderer could not be created")
    exit(1)
}

/// Renders one frame through the platform handoff and returns the RGBA8 bytes of the 384x240 target.
func render(_ items: [GpItem], loads: [(Int, Int, Int, Int, [UInt32])] = []) -> [UInt8]? {
    gpu_raster_reset()
    var display = GpuDisplayEnv(x: 0, y: 0, w: 384, h: 240, enabled: 1, is_rgb24: 0, interlace: 0, reverse: 0)
    gpu_raster_set_display(&display)
    for l in loads {
        gpu_raster_load_words(Int32(l.0), Int32(l.1), Int32(l.2), Int32(l.3), l.4, 0)
    }
    if !items.isEmpty { _ = gpu_raster_append(items, items.count) }
    gpu_raster_publish_frame()

    guard let cb = queue.makeCommandBuffer(), let result = renderer.encodeFrame(commandBuffer: cb) else { return nil }
    let bytesPerRow = W * 4
    let length = bytesPerRow * H
    guard let staging = device.makeBuffer(length: length, options: .storageModeShared),
          let blit = cb.makeBlitCommandEncoder() else { return nil }
    blit.copy(from: result, sourceSlice: 0, sourceLevel: 0, sourceOrigin: MTLOrigin(x: 0, y: 0, z: 0),
              sourceSize: MTLSize(width: W, height: H, depth: 1),
              to: staging, destinationOffset: 0, destinationBytesPerRow: bytesPerRow,
              destinationBytesPerImage: length)
    blit.endEncoding()
    cb.commit()
    cb.waitUntilCompleted()
    if cb.status == .error { return nil }
    let base = staging.contents().bindMemory(to: UInt8.self, capacity: length)
    return Array(UnsafeBufferPointer(start: base, count: length))
}

/// Decodes one target pixel: 5-bit colour channels and the mask bit.
func pixel(_ bytes: [UInt8], _ x: Int, _ y: Int) -> (Int, Int, Int, Int) {
    let i = (y * W + x) * 4
    func q(_ v: UInt8) -> Int { Int((Double(v) / 255.0 * 31.0).rounded()) }
    return (q(bytes[i]), q(bytes[i + 1]), q(bytes[i + 2]), bytes[i + 3] >= 128 ? 1 : 0)
}

var passes = 0
var failures = 0
func record(_ name: String, _ ok: Bool, _ detail: String) {
    if ok { passes += 1 } else { failures += 1 }
    print("\(ok ? "PASS" : "FAIL") \(name): \(detail)")
}

/// Compares every pixel of a rectangle with want(x, y). Reports how many matched and the first mismatch.
func expectRegion(_ name: String, _ bytes: [UInt8], x0: Int, y0: Int, w: Int, h: Int,
                  want: (Int, Int) -> (Int, Int, Int, Int)) {
    var matched = 0, total = 0
    var first = ""
    for y in y0..<(y0 + h) {
        for x in x0..<(x0 + w) {
            total += 1
            let got = pixel(bytes, x, y)
            let expected = want(x, y)
            if got == expected {
                matched += 1
            } else if first.isEmpty {
                first = "; first mismatch (\(x),\(y)) got \(got) want \(expected)"
            }
        }
    }
    record(name, matched == total, "matched \(matched)/\(total) pixels\(first)")
}

func fmt(_ p: (Int, Int, Int, Int)) -> String { "(\(p.0),\(p.1),\(p.2),a\(p.3))" }

// ---------------------------------------------------------------------------------------------
// Case 1. Flat triangle, red, vertices (10,10) (50,10) (10,50). Colour 255 -> 255/8 = 31.875 -> 31.
// Inside: (15,15). Outside: (45,45) is beyond the hypotenuse x+y=60.
if let b = render([makeItem(TRI, flags: 0, verts: [vtx(10, 10, 255, 0, 0), vtx(50, 10, 255, 0, 0), vtx(10, 50, 255, 0, 0)])]) {
    expectRegion("flat tri inside", b, x0: 15, y0: 15, w: 1, h: 1) { _, _ in (31, 0, 0, 0) }
    expectRegion("flat tri outside", b, x0: 45, y0: 45, w: 1, h: 1) { _, _ in (0, 0, 0, 0) }
} else {
    record("flat tri", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 2. Gouraud quad, vertices (100,10) red, (160,10) green, (100,70) blue, (160,70) white.
// Packet order 0,1,2,3 is split as (0,1,2) and (1,3,2), as the shader's index list does.
// Pixel (119,29), centre (119.5,29.5), is in triangle (0,1,2) at a = b = 19.5/60 = 0.325:
//   R = 255*(1-0.65) = 89.25 -> 89/8 -> 11; G = 255*0.325 = 82.875 -> 10; B = 10.
// Pixel (150,60), centre (150.5,60.5), is in triangle (1,3,2) at s = 0.68333, t = 0.15833:
//   R = 255*s = 174.25 -> 21; G = 255 - 255*t = 214.625 -> 26; B = 255*(s+t) = 214.625 -> 26.
if let b = render([makeItem(QUAD, flags: GOURAUD, verts: [
    vtx(100, 10, 255, 0, 0), vtx(160, 10, 0, 255, 0), vtx(100, 70, 0, 0, 255), vtx(160, 70, 255, 255, 255)])]) {
    expectRegion("gouraud quad tri A", b, x0: 119, y0: 29, w: 1, h: 1) { _, _ in (11, 10, 10, 0) }
    expectRegion("gouraud quad tri B", b, x0: 150, y0: 60, w: 1, h: 1) { _, _ in (21, 26, 26, 0) }
    expectRegion("gouraud quad outside", b, x0: 170, y0: 40, w: 1, h: 1) { _, _ in (0, 0, 0, 0) }
} else {
    record("gouraud quad", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 3. 4-bit textured rect, 5 texels wide at (200,10), uv starts at (0,0), colour 128 (neutral modulation),
// CLUT at (0,480). Texel values: index 1 -> red, 2 -> green, 3 -> blue, 15 -> 0x8010 (red 16, mask bit), 0 -> skipped.
// Modulation: texel5 * 128 / 128 = texel5, so the expected values are the raw CLUT entries.
if let b = render([makeItem(RECT, flags: TEX, verts: [vtx(200, 10, 128, 128, 128, u: 0, v: 0)],
                            w: 5, h: 1, tpage: 0, clut: clutWord)],
                  loads: [textureLoad, clutLoad]) {
    expectRegion("4bit clut texel 1 (red)", b, x0: 200, y0: 10, w: 1, h: 1) { _, _ in (31, 0, 0, 0) }
    expectRegion("4bit clut texel 2 (green)", b, x0: 201, y0: 10, w: 1, h: 1) { _, _ in (0, 31, 0, 0) }
    expectRegion("4bit clut texel 3 (blue)", b, x0: 202, y0: 10, w: 1, h: 1) { _, _ in (0, 0, 31, 0) }
    expectRegion("4bit clut texel 15 (mask bit)", b, x0: 203, y0: 10, w: 1, h: 1) { _, _ in (16, 0, 0, 1) }
    expectRegion("4bit clut texel 0 (transparent)", b, x0: 204, y0: 10, w: 1, h: 1) { _, _ in (0, 0, 0, 0) }
} else {
    record("4bit clut rect", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 4. Texture window (GP0 E2) with mask X = 1 (8 texels) and offset X = 0. The rect starts at u = 8, so each
// texel u maps to (u & ~8) | (0 & 1)*8 = u - 8. The expected texels are therefore 0..3 again.
if let b = render([makeItem(RECT, flags: TEX, verts: [vtx(220, 10, 128, 128, 128, u: 8, v: 0)],
                            w: 4, h: 1, tpage: 0, clut: clutWord, window: 0x0000_0001)],
                  loads: [textureLoad, clutLoad]) {
    expectRegion("texture window x=220 (red)", b, x0: 220, y0: 10, w: 1, h: 1) { _, _ in (31, 0, 0, 0) }
    expectRegion("texture window x=221 (green)", b, x0: 221, y0: 10, w: 1, h: 1) { _, _ in (0, 31, 0, 0) }
    expectRegion("texture window x=222 (blue)", b, x0: 222, y0: 10, w: 1, h: 1) { _, _ in (0, 0, 31, 0) }
    expectRegion("texture window x=223 (mask texel)", b, x0: 223, y0: 10, w: 1, h: 1) { _, _ in (16, 0, 0, 1) }
} else {
    record("texture window rect", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 3b. 4-bit textured QUAD (two triangles), 5 wide and 2 tall at (240,10). uv runs 0..5 across and is 0 down,
// so pixel centre x+0.5 gives u = 0.5 .. 4.5, texel u = 0..4, and rows 10 and 11 sample the same texels.
// Expected: same texels as case 3 (red, green, blue, mask-red 16, transparent) on both rows.
if let b = render([makeItem(QUAD, flags: TEX, verts: [
    vtx(240, 10, 128, 128, 128, u: 0, v: 0), vtx(245, 10, 128, 128, 128, u: 5, v: 0),
    vtx(240, 12, 128, 128, 128, u: 0, v: 0), vtx(245, 12, 128, 128, 128, u: 5, v: 0)],
    tpage: 0, clut: clutWord)],
    loads: [textureLoad, clutLoad]) {
    expectRegion("4bit textured quad row 10", b, x0: 240, y0: 10, w: 5, h: 1) { x, _ in
        [(31, 0, 0, 0), (0, 31, 0, 0), (0, 0, 31, 0), (16, 0, 0, 1), (0, 0, 0, 0)][x - 240]
    }
    expectRegion("4bit textured quad row 11", b, x0: 240, y0: 11, w: 5, h: 1) { x, _ in
        [(31, 0, 0, 0), (0, 31, 0, 0), (0, 0, 31, 0), (16, 0, 0, 1), (0, 0, 0, 0)][x - 240]
    }
} else {
    record("4bit textured quad", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 5. Semi-transparency, all four modes. Background 8-bit (160,80,32) = 5-bit (20,10,4), opaque.
// Front 8-bit (48,96,208) = 5-bit F (6,12,26), semi-transparent rect over the same pixels.
//   mode 0  B/2 + F/2   -> (13, 11, 15)
//   mode 1  B + F       -> (26, 22, 30)
//   mode 2  B - F       -> (14, 0, 0)     (clamped at 0)
//   mode 3  B + F/4     -> (21, 13, 10)   (F/4 = 1, 3, 6)
var semiItems: [GpItem] = []
let semiExpect: [(Int, Int, Int, Int)] = [(13, 11, 15, 0), (26, 22, 30, 0), (14, 0, 0, 0), (21, 13, 10, 0)]
for mode in 0..<4 {
    let x0 = 10 + mode * 30
    semiItems.append(makeItem(RECT, flags: 0, verts: [vtx(x0, 100, 160, 80, 32)], w: 10, h: 4))
    semiItems.append(makeItem(RECT, flags: SEMI, verts: [vtx(x0, 100, 48, 96, 208)], w: 10, h: 4, semi: mode))
}
if let b = render(semiItems) {
    for mode in 0..<4 {
        let x0 = 10 + mode * 30
        let want = semiExpect[mode]
        expectRegion("semi mode \(mode)", b, x0: x0 + 5, y0: 102, w: 1, h: 1) { _, _ in want }
    }
} else {
    record("semi modes", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 6. Dither. Colour 8-bit R=8, G=B=0, so 5-bit value = (8 + d) >> 3 = 1 when d >= 0 and 0 when d < 0.
// Gouraud triangle with dither on: the pattern must be the 4x4 bayer matrix sign. Block x 200..203, y 150..153.
// Triangle (196,146) (240,146) (196,190) covers it.
let ditherRed = [vtx(196, 146, 8, 0, 0), vtx(240, 146, 8, 0, 0), vtx(196, 190, 8, 0, 0)]
if let b = render([makeItem(TRI, flags: GOURAUD | DITHER, verts: ditherRed)]) {
    expectRegion("dither gouraud 4x4 pattern", b, x0: 200, y0: 150, w: 4, h: 4) { x, y in
        (bayer[y & 3][x & 3] >= 0 ? 1 : 0, 0, 0, 0)
    }
} else {
    record("dither gouraud", false, "render returned no frame")
}

// Flat triangle with dither on: no dithering on flat polygons, so every pixel is 1.
if let b = render([makeItem(TRI, flags: DITHER, verts: [vtx(260, 146, 8, 0, 0), vtx(300, 146, 8, 0, 0), vtx(260, 190, 8, 0, 0)])]) {
    expectRegion("dither off for flat tri", b, x0: 262, y0: 150, w: 4, h: 4) { _, _ in (1, 0, 0, 0) }
} else {
    record("dither flat", false, "render returned no frame")
}

// Rect with dither on: sprites and rects are never dithered, so every pixel is 1.
if let b = render([makeItem(RECT, flags: DITHER, verts: [vtx(330, 146, 8, 0, 0)], w: 8, h: 8)]) {
    expectRegion("dither off for rect", b, x0: 332, y0: 150, w: 4, h: 4) { _, _ in (1, 0, 0, 0) }
} else {
    record("dither rect", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 7. Horizontal line from (300,200) to (320,200), white. A line has zero height in its bounding box.
// Count the columns x 301..318 that are lit in row 199 or 200. All 18 interior columns must be lit.
if let b = render([makeItem(LINE, flags: 0, verts: [vtx(300, 200, 255, 255, 255), vtx(320, 200, 255, 255, 255)])]) {
    var lit = 0
    for x in 301...318 where pixel(b, x, 199).0 > 0 || pixel(b, x, 200).0 > 0 { lit += 1 }
    record("horizontal line", lit == 18, "lit columns \(lit)/18 (interior of 300..320)")
} else {
    record("horizontal line", false, "render returned no frame")
}

// ---------------------------------------------------------------------------------------------
// Case 8. Mask bit. A white rect with set-mask writes alpha 1. A later black rect with check-mask must not overwrite it.
if let b = render([
    makeItem(RECT, flags: MASKSET, verts: [vtx(340, 180, 255, 255, 255)], w: 6, h: 4),
    makeItem(RECT, flags: MASKCHECK, verts: [vtx(340, 180, 0, 0, 0)], w: 6, h: 4)]) {
    expectRegion("mask set + check keeps pixel", b, x0: 342, y0: 181, w: 1, h: 1) { _, _ in (31, 31, 31, 1) }
} else {
    record("mask set/check", false, "render returned no frame")
}

print("")
print("SUMMARY: \(passes) passed, \(failures) failed")
exit(failures == 0 ? 0 : 1)
