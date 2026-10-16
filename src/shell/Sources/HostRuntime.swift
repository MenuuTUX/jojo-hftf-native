#if JOJO_HOST
// In-process host (build-host.sh, JOJO_HOST). The runtime runs headless on its own thread; this file starts it,
// plays its audio ring on the CoreAudio render thread and writes the frame dump. GpuMetal.swift presents the frames and
// PadInput.swift feeds the controllers (jojo_host_set_pad). See README.md "Hosted runtime".
import AppKit
import AVFoundation
import CoreGraphics
import Foundation
import ImageIO

/// Settings from the environment. Every one is optional.
///   JOJO_SHELL_DEBUG_PORT       TCP debug server port of the runtime (default 4430; the lead's rule allows 4430/4431 only)
///   JOJO_SHELL_MEMCARD_DIR      where the runtime keeps its saves (default: ~/Library/Application Support/JojoShell/memcards)
///   JOJO_SHELL_AUTOQUIT_FRAMES  exit(0) at this game frame (0 = never)
///   JOJO_SHELL_DUMP_FRAME       write a PNG of the presented frame at this game frame (0 = never)
///   JOJO_SHELL_DUMP_PATH        PNG path for the dump (default: ./jojo-shell-frame.png)
///   JOJO_SHELL_PACE             0 to run the game unpaced (measurement only); default 1, 60 vblanks per second
///   JOJO_PHASE1                 pieces to switch on (default: all eight); JOJO_FILE_LAYER is set to 1 unless already set
struct HostSettings {
    let debugPort: Int
    let memcardDir: String
    let autoquitFrames: Int
    let dumpFrame: Int
    let dumpPath: String

    static func fromEnvironment() -> HostSettings {
        let env = ProcessInfo.processInfo.environment
        func int(_ key: String, _ def: Int) -> Int { Int(env[key] ?? "") ?? def }
        let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory())
        let memcard = env["JOJO_SHELL_MEMCARD_DIR"]
            ?? support.appendingPathComponent("JojoShell/memcards", isDirectory: true).path
        return HostSettings(debugPort: int("JOJO_SHELL_DEBUG_PORT", 4430),
                            memcardDir: memcard,
                            autoquitFrames: int("JOJO_SHELL_AUTOQUIT_FRAMES", 0),
                            dumpFrame: int("JOJO_SHELL_DUMP_FRAME", 0),
                            dumpPath: env["JOJO_SHELL_DUMP_PATH"] ?? "jojo-shell-frame.png")
    }
}

/// Starts the runtime on a thread with a large stack. The runtime's main thread stack is 8 MB, and it expects one.
enum GameHost {
    private(set) static var started = false

    static func start(gameFolder: URL, settings: HostSettings) {
        guard !started else { return }
        started = true

        // The adapters read these once, on first use. Set before the thread starts.
        setenv("JOJO_EXTRACTED_DIR", gameFolder.path, 1)
        if getenv("JOJO_FILE_LAYER") == nil { setenv("JOJO_FILE_LAYER", "1", 1) }
        if getenv("JOJO_PHASE1") == nil { setenv("JOJO_PHASE1", "all", 1) }
        try? FileManager.default.createDirectory(atPath: settings.memcardDir, withIntermediateDirectories: true)

        let exe = Bundle.main.executablePath ?? CommandLine.arguments[0]
        let args = [exe, "--headless",
                    "--debug-port", String(settings.debugPort),
                    "--memcard-dir", settings.memcardDir]
        let argv = UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>.allocate(capacity: args.count + 1)
        for (i, arg) in args.enumerated() { argv[i] = strdup(arg) }
        argv[args.count] = nil
        let argc = Int32(args.count)

        let thread = Thread {
            let status = jojo_host_run(argc, argv)
            NSLog("JojoShell: runtime returned %d", status)
        }
        thread.stackSize = 16 << 20
        thread.name = "jojo-game"
        thread.start()
    }
}

/// Reads the game frame counter that the vblank hook publishes.
enum HostClock {
    static var frame: UInt64 {
        var stats = JojoHostStats()
        jojo_host_stats(&stats)
        return stats.frame
    }
}

/// CoreAudio output. The render thread pulls the runtime's native audio ring (jojo_audio_pull), 44.1 kHz stereo.
final class HostAudio {
    private let engine = AVAudioEngine()
    private var source: AVAudioSourceNode?
    private static let capacity = 8192
    private let scratch: UnsafeMutablePointer<Int16>

    init() {
        scratch = UnsafeMutablePointer<Int16>.allocate(capacity: HostAudio.capacity * 2)
        scratch.initialize(repeating: 0, count: HostAudio.capacity * 2)
    }

    deinit {
        scratch.deallocate()
    }

    func start() {
        guard let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 2) else { return }
        let scratch = self.scratch
        let node = AVAudioSourceNode(format: format) { _, _, frameCount, bufferList -> OSStatus in
            let frames = min(Int(frameCount), HostAudio.capacity)
            _ = jojo_audio_pull(scratch, frames)
            let buffers = UnsafeMutableAudioBufferListPointer(bufferList)
            guard buffers.count >= 2,
                  let left = buffers[0].mData?.assumingMemoryBound(to: Float.self),
                  let right = buffers[1].mData?.assumingMemoryBound(to: Float.self) else { return noErr }
            for i in 0..<frames {
                left[i] = Float(scratch[2 * i]) / 32768.0
                right[i] = Float(scratch[2 * i + 1]) / 32768.0
            }
            return noErr
        }
        source = node
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: format)
        do {
            try engine.start()
        } catch {
            NSLog("JojoShell: audio engine did not start: %@", error.localizedDescription)
        }
    }
}

/// Writes a PNG of one 384x240 presented frame (GpuMetalRenderer.encodeFrame output, 8 bits per channel, colours are k/31).
enum HostFrameDump {
    static func write(_ source: MTLTexture, queue: MTLCommandQueue, to path: String) -> Bool {
        // The GPU model's targets are private storage, which the CPU cannot read (getBytes on them faults). Copy the
        // frame into a shared texture with a blit, wait for it, then read that.
        let width = source.width
        let height = source.height
        let bytesPerRow = width * 4
        let desc = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: source.pixelFormat,
                                                            width: width, height: height, mipmapped: false)
        desc.storageMode = .shared
        guard let staging = source.device.makeTexture(descriptor: desc),
              let buffer = queue.makeCommandBuffer(),
              let blit = buffer.makeBlitCommandEncoder() else { return false }
        blit.copy(from: source, sourceSlice: 0, sourceLevel: 0,
                  sourceOrigin: MTLOrigin(x: 0, y: 0, z: 0),
                  sourceSize: MTLSize(width: width, height: height, depth: 1),
                  to: staging, destinationSlice: 0, destinationLevel: 0,
                  destinationOrigin: MTLOrigin(x: 0, y: 0, z: 0))
        blit.endEncoding()
        buffer.commit()
        buffer.waitUntilCompleted()

        var pixels = [UInt8](repeating: 0, count: bytesPerRow * height)
        pixels.withUnsafeMutableBytes { raw in
            staging.getBytes(raw.baseAddress!, bytesPerRow: bytesPerRow,
                             from: MTLRegionMake2D(0, 0, width, height), mipmapLevel: 0)
        }
        guard let provider = CGDataProvider(data: Data(pixels) as CFData),
              let image = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
                                  bytesPerRow: bytesPerRow, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue),
                                  provider: provider, decode: nil, shouldInterpolate: false,
                                  intent: .defaultIntent),
              let destination = CGImageDestinationCreateWithURL(URL(fileURLWithPath: path) as CFURL,
                                                                "public.png" as CFString, 1, nil) else { return false }
        CGImageDestinationAddImage(destination, image, nil)
        return CGImageDestinationFinalize(destination)
    }
}
#endif
