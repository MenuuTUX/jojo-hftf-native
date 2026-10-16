#!/bin/bash
# Builds offscreen/main.swift against Sources/GpuMetal.swift and the platform gpu_* modules, then runs it.
# The test loads build/JojoShell.app/Contents/Resources/default.metallib, so run build.sh first.
# No window is opened and the PS1 game is not run. Output is one PASS or FAIL line per check.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PORT_ROOT="$(cd "$HERE/.." && pwd)"
PLATFORM="$PORT_ROOT/platform"
SRC="$HERE/Sources"
TEST="$HERE/offscreen"
OUT="$HERE/build/offscreen"
OBJ="$OUT/obj"
METALLIB="$HERE/build/JojoShell.app/Contents/Resources/default.metallib"
MIN_MACOS="14.0"
TARGET="arm64-apple-macos$MIN_MACOS"

if [ ! -f "$METALLIB" ]; then
    echo "FAIL: $METALLIB not found. Run build.sh first."
    exit 1
fi

SDK="$(xcrun --sdk macosx --show-sdk-path)"
SWIFTC="$(xcrun --find swiftc)"
CLANG="$(xcrun --find clang)"

rm -rf "$OUT"
mkdir -p "$OBJ"

for name in gpu_gp0 gpu_raster gpu_vram gpu_soft; do
    "$CLANG" -c -arch arm64 -isysroot "$SDK" -mmacosx-version-min="$MIN_MACOS" \
        -std=c99 -O2 -Wall -Wextra -Werror \
        "$PLATFORM/$name.c" -o "$OBJ/$name.o"
done

"$SWIFTC" \
    -target "$TARGET" \
    -sdk "$SDK" \
    -swift-version 5 \
    -O \
    -module-name OffscreenTest \
    -import-objc-header "$SRC/JojoShell-Bridging-Header.h" \
    -Xcc -I"$PLATFORM" \
    -framework Metal \
    -framework MetalKit \
    "$SRC/GpuMetal.swift" "$TEST/main.swift" \
    "$OBJ/gpu_gp0.o" "$OBJ/gpu_raster.o" "$OBJ/gpu_vram.o" "$OBJ/gpu_soft.o" \
    -o "$OUT/offscreen-test"

set +e
"$OUT/offscreen-test" "$METALLIB"
STATUS=$?
set -e
echo "exit status: $STATUS"
exit "$STATUS"
