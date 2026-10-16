import AppKit
import Metal
import MetalKit

/// MTKView backed by a CAMetalLayer. Swallows key events so the game keys do not beep.
/// PadInput reads the keyboard through an NSEvent local monitor, so this view only needs to take focus.
final class GameView: MTKView {
    override var acceptsFirstResponder: Bool { true }

    override func keyDown(with event: NSEvent) {}
    override func keyUp(with event: NSEvent) {}
}

/// Per-frame driver. Pushes pad state first, then presents the latest published PS1 GPU frame (GpuMetal.swift).
/// Until the game publishes a frame, the GPU model's VRAM is all zero, so the window shows black.
final class Renderer: NSObject, MTKViewDelegate {
    private let commandQueue: MTLCommandQueue
    private let pads: PadInput
    private let gpu: GpuMetalRenderer?

    init?(view: MTKView, pads: PadInput) {
        guard let device = view.device, let queue = device.makeCommandQueue() else { return nil }
        self.commandQueue = queue
        self.pads = pads
        self.gpu = GpuMetalRenderer(device: device)
        if gpu == nil {
            NSLog("JojoShell: GPU renderer unavailable (default.metallib missing or shader failed); frames are cleared only.")
        }
        super.init()
    }

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

    func draw(in view: MTKView) {
        pads.pushFrame()
#if JOJO_HOST
        maybeDumpFrame()
#endif

        guard let drawable = view.currentDrawable,
              let commandBuffer = commandQueue.makeCommandBuffer() else {
            return
        }
        if let gpu = gpu {
            gpu.render(to: view, drawable: drawable, commandBuffer: commandBuffer)
        } else if let passDescriptor = view.currentRenderPassDescriptor,
                  let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: passDescriptor) {
            encoder.endEncoding()
            commandBuffer.present(drawable)
        }
        commandBuffer.commit()
    }

#if JOJO_HOST
    private let hostSettings = HostSettings.fromEnvironment()
    private var dumped = false

    /// Writes one PNG of the published frame once the game reaches JOJO_SHELL_DUMP_FRAME. Waits for its own command buffer,
    /// so it runs before this draw's presentation encode.
    private func maybeDumpFrame() {
        guard let gpu = gpu, hostSettings.dumpFrame > 0, !dumped else { return }
        let frame = HostClock.frame
        guard frame >= UInt64(hostSettings.dumpFrame) else { return }
        dumped = true
        guard let buffer = commandQueue.makeCommandBuffer(),
              let image = gpu.encodeFrame(commandBuffer: buffer) else {
            NSLog("JojoShell: no published frame to dump at game frame %llu", frame)
            return
        }
        buffer.commit()
        buffer.waitUntilCompleted()
        let ok = HostFrameDump.write(image, queue: commandQueue, to: hostSettings.dumpPath)
        NSLog("JojoShell: dumped game frame %llu to %@ ok=%d", frame, hostSettings.dumpPath, ok ? 1 : 0)
    }
#endif
}

enum GameViewFactory {
    static func make(pads: PadInput) -> (view: GameView, renderer: Renderer)? {
        guard let device = MTLCreateSystemDefaultDevice() else { return nil }
        let view = GameView(frame: NSRect(x: 0, y: 0, width: 640, height: 480), device: device)
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.05, green: 0.02, blue: 0.10, alpha: 1.0)
        view.preferredFramesPerSecond = 60
        view.autoResizeDrawable = true
        guard let renderer = Renderer(view: view, pads: pads) else { return nil }
        view.delegate = renderer
        return (view, renderer)
    }
}
