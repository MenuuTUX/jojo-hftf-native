# JoJo's Bizarre Adventure: Heritage for the Future, native port

A native macOS build of the PlayStation version of *JoJo's Bizarre Adventure* (Capcom, 1999, US release SLUS-01060).

The game is a static recompilation on [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp). On top of it, the game's own code is being replaced with native C, one function at a time. Each replacement has been compared against the original on thousands of captured game states before it is allowed to run.

Built with LLM agents (Claude Code and Codex), directed and reviewed by a human.

## Status

- The 1v1 fight engine (shared fight code, the main loop, scheduling, hit tests, animation) runs natively: 918 functions, checked against 97,634 captured states with no mismatches.
- All 22 selectable characters, the 4 encounter-only bosses (as the second player), all 33 stages, VS and training mode and their select screens have native code. 3,573 of those 7,188 functions are verified so far. The others still run as recompiled MIPS until a test route reaches them.
- Story and Arcade mode run on the recompiled code.
- macOS on Apple Silicon is the only tested platform.

## Building

See [INSTALL.md](INSTALL.md). You need your own copy of the game disc.

## Layout

```
CMakeLists.txt     build (psxrecomp project)
game.toml          game configuration for the runtime
generated/         the recompiled game code (psxrecomp output)
src/game/          native game code: generated from the decompilation (gen_*.c) and hand-written (manual/)
src/hybrid/        glue between the runtime and the native code
src/platform/      platform layer (files, CD streaming, audio)
src/shell/         macOS app (Swift, Metal)
assets/disc/       files extracted from the disc (see NOTICE.md)
assets/xa/         streamed audio extracted from the disc
psxrecomp/         the recompiler and runtime (git submodule)
patches/           local changes to psxrecomp, applied by setup.sh
docs/              architecture and file format notes
disc/              where your disc image goes
```

## How it works

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains the recompilation, how native functions replace recompiled ones, and how they are checked. The tools that generate and verify the native code (decompiler scripts, the C generator, the capture and replay harness) and the full phase reports are in the development repository, jojo-hftf-native-tools.

## Legal

Not affiliated with Capcom or with the psxrecomp authors. See [NOTICE.md](NOTICE.md).
