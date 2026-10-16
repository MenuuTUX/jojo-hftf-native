# Building

Tested on macOS on Apple Silicon.

## Requirements

- Xcode command line tools
- CMake 3.20 or newer, Ninja, Python 3, Git, Git LFS: `brew install cmake ninja git-lfs`
- Your own copy of *JoJo's Bizarre Adventure* (US, SLUS-01060) as a `.cue` and `.bin`

## Steps

```bash
git clone --recursive https://github.com/MenuuTUX/jojo-hftf-native.git
cd jojo-hftf-native
git lfs pull
./setup.sh
```

`setup.sh` checks out psxrecomp at the pinned commit and applies `patches/psxrecomp-local.patch`.

Copy your disc into `disc/` with these names:

```
disc/JoJo's Bizarre Adventure (USA).cue
disc/JoJo's Bizarre Adventure (USA).bin
```

Build and run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/JoJoPS1
```

## Native code switches

The build contains both the recompiled code and the native code. Environment variables choose what runs:

- No `JOJO_*` variables: everything runs as recompiled MIPS.
- `JOJO_PHASE1=all`: native PlayStation library replacements (pad, CD, GPU, sound, memory card, kernel).
- `JOJO_GAME=<groups>`: native game code, by group. The fight engine is `anim,sched,hit,acmain,text,threads`. Characters are `plXX` and `plXXx` (slot A and slot B), modes are `gamevs`, `gametr`, `ac_twop`, `vs_sel`, `tr_sel`.

Only functions verified in testing run natively. The others in a group fall back to the recompiled code.

The macOS app in `src/shell` has its own build script (`src/shell/build.sh`).
