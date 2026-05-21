#!/usr/bin/env bash
# Build ClaudeGame.app as a release-ready universal Mac App Store candidate.
# Doesn't sign or upload — that needs your Apple Developer Program team ID.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BUILD="$ROOT/build-mac"
need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[error] required tool '$1' not found." >&2
        case "$1" in
            cmake)
                echo "        Install with: brew install cmake" >&2
                echo "        Or download CMake.app from https://cmake.org/download/" >&2
                echo "        and add /Applications/CMake.app/Contents/bin to PATH." >&2
                ;;
            clang++|git|make)
                echo "        Install Xcode Command Line Tools: xcode-select --install" >&2
                ;;
        esac
        exit 1
    fi
}
need clang++
need git
need cmake
if [ ! -f "$ROOT/third_party/raylib-mac/lib/libraylib.a" ]; then
    echo "[step] building universal raylib"
    bash "$HERE/build-raylib-universal.sh"
fi
if [ ! -f "$HERE/AppIcon.icns" ]; then
    echo "[step] generating placeholder icon"
    bash "$HERE/make-icon.sh"
fi
echo "[step] configuring CMake"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
echo "[step] building"
cmake --build "$BUILD" --target claudegame -j
APP="$BUILD/ClaudeGame.app"
echo
echo "Built: $APP"
echo
lipo -info "$APP/Contents/MacOS/ClaudeGame"
du -sh "$APP"
echo
echo "Next steps for App Store:"
echo "  1. Replace mac/icon-source.png with real 1024x1024 art, then re-run."
echo "  2. codesign --deep --options runtime --entitlements mac/ClaudeGame.entitlements \\"
echo "         --sign \"3rd Party Mac Developer Application: YOUR TEAM\" $APP"
echo "  3. productbuild --component $APP /Applications \\"
echo "         --sign \"3rd Party Mac Developer Installer: YOUR TEAM\" ClaudeGame.pkg"
echo "  4. xcrun altool --upload-app -f ClaudeGame.pkg -t macos -u APPLE_ID -p APP_SPECIFIC_PWD"
