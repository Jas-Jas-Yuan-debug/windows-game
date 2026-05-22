#!/usr/bin/env bash
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
RAYLIB_ROOT="$ROOT/third_party/raylib/raylib-5.5_win64_mingw-w64"
SQLITE_DIR="$ROOT/third_party/sqlite"
BUILD="$ROOT/build-cross-win64"
TOOLCHAIN="$HERE/mingw-w64-x86_64.cmake"
if [ ! -f "$RAYLIB_ROOT/lib/libraylib.a" ]; then
    echo "raylib mingw prebuilt missing at $RAYLIB_ROOT" >&2
    echo "Run windows/fetch-deps.sh first or follow README_WINDOWS.md" >&2
    exit 1
fi
if [ ! -f "$SQLITE_DIR/sqlite3.c" ]; then
    echo "sqlite3 amalgamation missing at $SQLITE_DIR" >&2
    exit 1
fi
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DRAYLIB_DIR="$RAYLIB_ROOT" \
    -DCMAKE_PREFIX_PATH="$RAYLIB_ROOT"
cmake --build "$BUILD" -j
echo
echo "Output:"
echo "  $BUILD/claudegame.exe"
echo "  $BUILD/claudegame_server.exe"
