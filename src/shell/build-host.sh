#!/bin/bash
# Builds build/host/JojoShell.app: the Swift shell with the PS1 runtime linked in (hosted mode, README.md "Hosted runtime").
# The runtime comes from JoJoPS1/build-shell, configured with -DJOJO_SHELL_LIB=ON (main renamed to psxrecomp_main).
# Objects and libraries are taken from ninja's own link line for the JoJoPS1 executable, minus hybrid/jojo_host_main.c.o
# (the executable's main). Nothing is downloaded. Ad-hoc signed, like build.sh.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PORT_ROOT="$(cd "$HERE/../.." && pwd)"
JOJO="$PORT_ROOT"
BSHELL="$JOJO/build-shell"
PLATFORM="$PORT_ROOT/src/platform"
HYBRID="$PORT_ROOT/src/hybrid"
SRC="$HERE/Sources"
SHADERS="$HERE/Shaders"
OUT="$HERE/build/host"
OBJ="$OUT/obj"
APP="$OUT/JojoShell.app"
MIN_MACOS="14.0"
TARGET="arm64-apple-macos$MIN_MACOS"

SDK="$(xcrun --sdk macosx --show-sdk-path)"
SWIFTC="$(xcrun --find swiftc)"
CLANG="$(xcrun --find clang)"

if [ ! -f "$BSHELL/build.ninja" ]; then
    echo "FAIL: $BSHELL is not configured. Configure it with -DJOJO_SHELL_LIB=ON (see README.md, Hosted runtime)."
    exit 1
fi
if ! grep -q 'JOJO_SHELL_LIB:BOOL=ON' "$BSHELL/CMakeCache.txt"; then
    echo "FAIL: $BSHELL was not configured with -DJOJO_SHELL_LIB=ON."
    exit 1
fi

echo "== cmake: JoJoPS1 (build-shell)"
cmake --build "$BSHELL" --target JoJoPS1

# The link line of the executable: objects, then libraries. Split with shlex so quoted paths survive.
LINK_LINE="$(cd "$BSHELL" && ninja -t commands JoJoPS1 | grep -- ' -o JoJoPS1 ' | tail -n 1)"
if [ -z "$LINK_LINE" ]; then
    echo "FAIL: no link command for JoJoPS1 in build-shell."
    exit 1
fi
TOKENS=()
while IFS= read -r tok; do TOKENS+=("$tok"); done < <(python3 -I -c 'import shlex,sys; print("\n".join(shlex.split(sys.argv[1])))' "$LINK_LINE")

LAST_OBJ=-1
for i in "${!TOKENS[@]}"; do
    case "${TOKENS[$i]}" in *.o) LAST_OBJ=$i ;; esac
done
if [ "$LAST_OBJ" -lt 0 ]; then
    echo "FAIL: the link command has no object files."
    exit 1
fi

OBJECTS=()
LIBS=()
SKIP_NEXT=0
for i in "${!TOKENS[@]}"; do
    tok="${TOKENS[$i]}"
    if [ "$SKIP_NEXT" -eq 1 ]; then
        # The name after -framework (already passed through with it).
        SKIP_NEXT=0
        continue
    fi
    if [ "$i" -le "$LAST_OBJ" ]; then
        case "$tok" in
            *.o)
                case "$tok" in
                    *jojo_host_main.c.o) echo "   skip executable main: $tok" ;;
                    *) OBJECTS+=("$BSHELL/$tok") ;;
                esac ;;
        esac
        continue
    fi
    # The link line is followed by the target's post-build commands ("&& cd ... && cmake -E ..."). They are not inputs.
    if [ "$tok" = "&&" ]; then break; fi
    case "$tok" in
        -Wl,*)
            # -Wl,-rpath,/x becomes -Xlinker -rpath -Xlinker /x for swiftc.
            IFS=',' read -r -a parts <<< "$tok"
            for part in "${parts[@]:1}"; do LIBS+=("-Xlinker" "$part"); done ;;
        -framework) LIBS+=("$tok" "${TOKENS[$((i + 1))]}"); SKIP_NEXT=1 ;;
        -lz|-lc++) LIBS+=("$tok") ;;
        -o) SKIP_NEXT=1 ;;            # the executable's output name (JoJoPS1), not an input
        -*) : ;;
        /*) LIBS+=("$tok") ;;
        *) LIBS+=("$BSHELL/$tok") ;;
    esac
done
LIBS+=("-lc++")   # the runtime is C++ (main.cpp, SDL and texture-pack code); swiftc does not add it
echo "   runtime objects: ${#OBJECTS[@]}  link inputs: ${#LIBS[@]}"

rm -rf "$OUT"
mkdir -p "$OBJ" "$APP/Contents/MacOS" "$APP/Contents/Resources"

# Metal shaders: the same library build.sh makes (GpuMetalRenderer loads default.metallib from the bundle).
echo "== metal: Shaders/jojo.metal"
xcrun --sdk macosx metal -std=metal3.0 -mmacosx-version-min="$MIN_MACOS" -c \
    "$SHADERS/jojo.metal" -o "$OBJ/jojo.air"
xcrun --sdk macosx metallib "$OBJ/jojo.air" -o "$APP/Contents/Resources/default.metallib"

# Swift shell with JOJO_HOST: HostRuntime.swift is compiled in; the C side comes from the runtime objects (they already
# hold platform/pad.c and platform/gpu_*.c, so the standalone build's copies are not linked here).
echo "== swiftc: Sources/*.swift (JOJO_HOST) + runtime"
"$SWIFTC" \
    -target "$TARGET" \
    -sdk "$SDK" \
    -swift-version 5 \
    -O \
    -module-name JojoShell \
    -D JOJO_HOST \
    -import-objc-header "$SRC/JojoShell-Bridging-Header.h" \
    -Xcc -DJOJO_HOST \
    -Xcc -I"$PLATFORM" \
    -Xcc -I"$HYBRID" \
    -framework AppKit \
    -framework Metal \
    -framework MetalKit \
    -framework GameController \
    -framework AVFoundation \
    "$SRC"/*.swift \
    "${OBJECTS[@]}" \
    "${LIBS[@]}" \
    -o "$APP/Contents/MacOS/JojoShell"

# Runtime support. The runtime looks for bios/, assets/, mods/ and game.toml beside the executable (on macOS its directory).
# The files live in Contents/Resources, which codesign seals as resources, and are linked into Contents/MacOS (plain
# files there make strict codesign verification fail). The disc image is not copied: game.toml names the user's .cue.
for name in bios assets mods game.toml game_options.toml; do
    if [ -e "$BSHELL/$name" ]; then
        cp -R "$BSHELL/$name" "$APP/Contents/Resources/"
        ln -s "../Resources/$name" "$APP/Contents/MacOS/$name"
    fi
done

cp "$HERE/Info.plist" "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
plutil -lint "$APP/Contents/Info.plist"

codesign --force --sign - --timestamp=none "$APP"
codesign --verify --strict --verbose=2 "$APP"

echo "== result"
lipo -archs "$APP/Contents/MacOS/JojoShell"
echo "built: $APP"
