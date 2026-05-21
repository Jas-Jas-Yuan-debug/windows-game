#!/usr/bin/env bash
# Build raylib 5.5 as a universal (arm64 + x86_64) static library so the .app can
# be a fat binary that runs on both Apple Silicon and Intel Macs.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
DEST="$ROOT/third_party/raylib-mac"
RAYLIB_VER=5.5
SRC="/tmp/raylib-$RAYLIB_VER"
if [ -f "$DEST/lib/libraylib.a" ]; then
    if lipo -info "$DEST/lib/libraylib.a" | grep -q 'arm64.*x86_64\|x86_64.*arm64'; then
        echo "[raylib] universal libraylib.a already present at $DEST"
        exit 0
    fi
fi
rm -rf "$SRC"
git clone --depth 1 --branch "$RAYLIB_VER" https://github.com/raysan5/raylib.git "$SRC"
cd "$SRC"
cmake -B build-uni \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_GAMES=OFF \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-uni -j
mkdir -p "$DEST/include" "$DEST/lib"
cp build-uni/raylib/libraylib.a "$DEST/lib/"
cp src/raylib.h src/raymath.h src/rlgl.h "$DEST/include/"
echo "[raylib] installed universal raylib at $DEST"
lipo -info "$DEST/lib/libraylib.a"
