# JoJoShell

The macOS (Apple Silicon) application shell for the JoJo native port. It owns the window, the Metal view, controller and keyboard input, and the choice of game folder. It has no game engine yet.

## Layout

```
shell/
  Info.plist                      bundle metadata (copied into the .app)
  build.sh                        compiles C, Metal and Swift, bundles, ad-hoc signs -> build/JojoShell.app
  smoke-test.sh                   launch, wait 3 s, quit, check no process is left
  offscreen-test.sh               builds offscreen/main.swift and runs the GPU renderer test (no window)
  Sources/
    main.swift                    entry point, NSApplication setup
    AppDelegate.swift             window, menu (Game > Change Game Folder...), first-launch chooser
    GameView.swift                MTKView subclass (CAMetalLayer) and the per-frame Renderer
    GameFolder.swift              game folder in UserDefaults, NSOpenPanel, index.tsv check
    PadInput.swift                GameController and keyboard -> PS1 button word -> plat_pad_set
    GpuMetal.swift                GpuMetalRenderer: published GPU snapshot -> 384x240 target -> drawable
    JojoShell-Bridging-Header.h   imports platform/pad.h, file_layer.h and gpu_raster.h
  Shaders/
    jojo.metal                    base (VRAM display), item (draw items) and present shaders
  offscreen/
    main.swift                    offscreen self-test: hand-made GP0 items, readback, hand-computed values
  build/                          generated (build.sh deletes and recreates it)
```

Nothing under `JojoPS1/` or `platform/` is modified by this shell. It reads the headers and compiles `pad.c`, `gpu_gp0.c`, `gpu_raster.c` and `gpu_vram.c`.

## Build and test

```sh
./build.sh            # needs only the Xcode command line tools (clang, swiftc, metal, metallib)
./smoke-test.sh       # run after build.sh; launches the shell only, never the PS1 game
./offscreen-test.sh   # run after build.sh; prints PASS/FAIL per check and exits 1 on any FAIL
```

Target: `arm64-apple-macos14.0`, Swift 5 language mode, no third-party code.

`offscreen-test.sh` feeds draw items through the same `gpu_raster_*` handoff the emulator uses, renders into the 384x240 target without a window, reads it back and compares each value with a hand-computed expectation. It covers a flat triangle, a Gouraud quad, a 4-bit textured rect and a 4-bit textured quad, both with a CLUT (including the mask-bit and transparent texels), a texture window, all four semi-transparency modes, the dither gating and pattern, a horizontal line and the mask set/check bits. The expected values are in `offscreen/main.swift`. It does not cover 8-bit or 16-bit textures or the 24-bit display path.

## Game folder

The game folder is the output of `tools-phase1/extract_disc.py`. The shell identifies it by `index.tsv` in its root.

- On first launch, or when the stored folder is no longer valid, an open panel asks for it.
- The path is stored in UserDefaults under `gameFolderPath`.
- `Game > Change Game Folder...` (Cmd-O) changes it.
- Launching with `-gameFolderPath <dir>` overrides the stored value for one run. `smoke-test.sh` uses this.

The app is not sandboxed, so a plain path is enough. If sandboxing is added later, store a security-scoped bookmark instead.

## Input

Each frame, `PadInput.pushFrame()` calls `plat_pad_set` once for each of the two ports. The word is active low, as `platform/pad.h` defines it.

| PS1 | Controller | Keyboard |
|---|---|---|
| Cross | bottom face button (A on Xbox layout, Cross on DualSense) | Z |
| Circle | right face button | X |
| Square | left face button | A |
| Triangle | top face button | S |
| D-pad | D-pad, and the left stick past 50% | arrow keys |
| L1, R1 | shoulder buttons | none |
| L2, R2 | triggers | none |
| L3, R3 | stick clicks | none |
| Start | Menu button | Return or keypad Enter |
| Select | Options button | Right Shift |

- Port 0 is the first connected controller ANDed with the keyboard. The keyboard therefore always works, even with a controller attached.
- Port 1 is the second connected controller. It is reported as disconnected when there is none.
- Both ports are digital (`analog = 0`). The sticks are centred in the report, and the left stick acts as a d-pad.
- Face buttons are positional, so the layout is the same on any brand of controller.

## Hosted runtime (Phase 1, `build-host.sh`)

`build-host.sh` builds `build/host/JojoShell.app`: the same shell with the PS1 runtime linked in. The game runs in-process on its own thread, headless (no SDL window, no SDL audio, no emulated GPU, SPU or pad as the presenter).

- **Runtime build.** `JoJoPS1/build-shell` is configured with `-DJOJO_SHELL_LIB=ON` (the last block of `JoJoPS1/CMakeLists.txt`, off by default). There, `main.cpp` is compiled with `main=psxrecomp_main`. `hybrid/jojo_host_main.c` keeps the `JoJoPS1` executable linkable. `build-host.sh` takes the object files and link inputs from ninja's own link line for `JoJoPS1`, minus that `main`, so there is no separate library to keep in step.
- **Entry.** `int jojo_host_run(int argc, char **argv)` (`hybrid/jojo_host_shell.h`) calls the renamed `main`. The shell calls it from a 16 MB-stack thread with `--headless --debug-port <port> --memcard-dir <dir>`.
- **Wrapping without edits.** `jojo_pad.c` and `jojo_audio.c` are compiled with their vblank and pull entry points renamed `*_impl`. `hybrid/jojo_host_entry.c` supplies the strong definitions:
  - `jojo_pad_frame` ANDs the shell's controller word into the SIO model for each port (the runtime's own value is kept, so the debug server's input route and press still apply), then calls the adapter.
  - `jojo_audio_frame` (game thread, under a lock) and `jojo_audio_pull` (CoreAudio thread, try-lock, zero-fill on contention) serialise the native audio ring.
- **Presentation.** `GpuMetal.swift` presents the published GPU stream (`gpu_raster_copy_published`) at 384x240, scaled to the window, as before. `HostRuntime.swift` plays `jojo_audio_pull` through `AVAudioSourceNode` (44.1 kHz stereo) and writes PNG dumps.
- **Environment (all optional).** `JOJO_SHELL_DEBUG_PORT` (4430), `JOJO_SHELL_MEMCARD_DIR`, `JOJO_SHELL_AUTOQUIT_FRAMES` (exit 0 at that vblank, after a stats line), `JOJO_SHELL_DUMP_FRAME` and `JOJO_SHELL_DUMP_PATH` (one PNG of the presented frame), `JOJO_SHELL_PACE` (0 = unpaced), `JOJO_PHASE1` (default all eight pieces), `JOJO_FILE_LAYER` (default 1). `JOJO_EXTRACTED_DIR` is set from the chosen game folder.
- **Smoke test.** `host_smoke.py` runs the app with the boot-to-fight route on port 4430, reads the fight block at 0x800CD268 at frame 9400, checks the autoquit exit status and the PNG, and leaves no process behind. Each run writes `runs/phase1-shell-NNN/`.
- **Staging.** The runtime's `bios/`, `assets/`, `mods/`, `game.toml` and `game_options.toml` are copied beside the executable (the runtime looks there). The disc image is not copied: `game.toml` names the user's `.cue` by path.
- **Standalone build unchanged.** `build.sh` still builds the Metal-only shell. The host code is under `#if JOJO_HOST`.

## Files used from the port

| File | Used for |
|---|---|
| `platform/pad.h`, `platform/pad.c` | `PlatPad`, `plat_pad_set`. `pad.c` is compiled and linked by `build.sh`. |
| `platform/file_layer.h` | Exposed through the bridging header only. Not called yet. |
| `platform/gpu_gp0.c`, `gpu_raster.c`, `gpu_vram.c` | The published GPU snapshot read by `GpuMetal.swift`. Compiled by `build.sh`. The shell does not decode GP0 itself. |

## Planned next steps

1. **Link the rest of `platform/*.c`.** Add `file_layer.c` to the `clang` step in `build.sh` when the shell starts calling `jfl_open` (to read `index.tsv`) and later the game's loader. Other platform modules are added the same way: compile to `build/obj/`, then pass the `.o` files to `swiftc`.
2. **Metal renderer.** `Renderer.draw(in:)` now presents each published GPU frame through `GpuMetalRenderer`. Until the game publishes a frame, the window is black. Still missing: the platform adapter (`hybrid/jojo_gpu.c`) applies fills, copies and CPU-to-VRAM loads to the CPU VRAM model, but draws are never written into it. The shell therefore composites this frame's draws over the end-of-frame VRAM, and textured draws sample that end-of-frame image. The present pass has no exact-size mode, and VRAM is uploaded whole each frame rather than by dirty rectangle.
3. **Analog mode.** `analog` is 0 for now. When the game's pad code needs DualShock-style reports, set `analog = 1` and fill the stick bytes (they are already centred).

## Info.plist keys

| Key | Value | Why |
|---|---|---|
| `LSApplicationCategoryType` | `public.app-category.games` | Launch Services category. |
| `GCSupportsGameMode` | `true` | Opts into macOS Game Mode. Needs macOS 14 or later, so `LSMinimumSystemVersion` is 14.0. |
| `GCSupportsControllerUserInteraction` | `true` | Lets controllers deliver input while the app is not frontmost, so pads keep working when the window is in the background. |
| `NSHighResolutionCapable` | `true` | Renders at the backing scale factor on Retina displays. |
| `LSMinimumSystemVersion` | `14.0` | Minimum for the Game Mode key. |
| `LSArchitecturePriority` | `arm64` | Apple Silicon only. |

`LSSupportsGameMode` is not included. The local Xcode 27 SDK does not define it, and `GCSupportsGameMode` is the key Apple documents for Game Mode.
