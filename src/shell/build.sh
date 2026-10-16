#!/bin/bash
# Builds build/JojoShell.app (arm64, macOS 14+), ad-hoc signs it, and checks the result.
# Uses only the Xcode command line tools (clang, swiftc, metal, metallib). Nothing is downloaded.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PORT_ROOT="$(cd "$HERE/.." && pwd)"
PLATFORM="$PORT_ROOT/platform"
SRC="$HERE/Sources"
SHADERS="$HERE/Shaders"
OUT="$HERE/build"
OBJ="$OUT/obj"
APP="$OUT/JojoShell.app"
MIN_MACOS="14.0"
TARGET="arm64-apple-macos$MIN_MACOS"

SDK="$(xcrun --sdk macosx --show-sdk-path)"
SWIFTC="$(xcrun --find swiftc)"
CLANG="$(xcrun --find clang)"

echo "SDK:    $SDK"
echo "swiftc: $("$SWIFTC" --version 2>&1 | head -1)"
echo "clang:  $("$CLANG" --version | head -1)"

rm -rf "$OUT"
mkdir -p "$OBJ" "$APP/Contents/MacOS" "$APP/Contents/Resources"

# 1. Platform C code. pad.c is called from Swift. The gpu_* modules back GpuMetal.swift (the published GPU snapshot).
#    file_layer.h is exposed through the bridging header and compiled by swiftc, but file_layer.c is not linked yet.
for name in pad gpu_gp0 gpu_raster gpu_vram gpu_soft; do
    echo "== clang: platform/$name.c"
    "$CLANG" -c -arch arm64 -isysroot "$SDK" -mmacosx-version-min="$MIN_MACOS" \
        -std=c99 -O2 -Wall -Wextra -Werror \
        "$PLATFORM/$name.c" -o "$OBJ/$name.o"
done

# 2. Metal shaders. The bundle's default.metallib is what GpuMetalRenderer loads with makeDefaultLibrary().
echo "== metal: Shaders/jojo.metal"
xcrun --sdk macosx metal -std=metal3.0 -mmacosx-version-min="$MIN_MACOS" -c \
    "$SHADERS/jojo.metal" -o "$OBJ/jojo.air"
xcrun --sdk macosx metallib "$OBJ/jojo.air" -o "$APP/Contents/Resources/default.metallib"

# 3. Swift shell. -swift-version 5 avoids Swift 6 strict-concurrency errors in the AppKit and GameController code.
echo "== swiftc: Sources/*.swift"
"$SWIFTC" \
    -target "$TARGET" \
    -sdk "$SDK" \
    -swift-version 5 \
    -O \
    -module-name JojoShell \
    -import-objc-header "$SRC/JojoShell-Bridging-Header.h" \
    -Xcc -I"$PLATFORM" \
    -framework AppKit \
    -framework Metal \
    -framework MetalKit \
    -framework GameController \
    "$SRC"/main.swift "$SRC"/AppDelegate.swift "$SRC"/GameView.swift "$SRC"/GameFolder.swift \
    "$SRC"/PadInput.swift "$SRC"/GpuMetal.swift \
    "$OBJ/pad.o" "$OBJ/gpu_gp0.o" "$OBJ/gpu_raster.o" "$OBJ/gpu_vram.o" "$OBJ/gpu_soft.o" \
    -o "$APP/Contents/MacOS/JojoShell"

# 4. Bundle metadata.
cp "$HERE/Info.plist" "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"
plutil -lint "$APP/Contents/Info.plist"

# 5. Ad-hoc signature. Good enough to run on this Mac. Not a Developer ID or notarised build.
codesign --force --sign - --timestamp=none "$APP"
codesign --verify --strict --verbose=2 "$APP"

echo "== result"
lipo -archs "$APP/Contents/MacOS/JojoShell"
echo "built: $APP"
