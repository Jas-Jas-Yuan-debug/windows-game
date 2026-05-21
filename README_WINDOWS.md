# ClaudeGame — Windows 11

This is the Windows mirror of `/Users/jason/Documents/VSCODE/claudegame`. Source files are identical (single portable codebase using `#ifdef _WIN32`); only the build instructions, helper scripts, and bundled dependencies differ.

For game design, controls, protocol, schema, etc., see [`README.md`](README.md).

## Pick a build path

You have two choices on Windows. **MSVC + vcpkg** is the standard for native Windows C++ development. **MinGW-w64 / MSYS2** is the GCC route and is also what gets used when cross-compiling from macOS.

### Option A: MSVC + vcpkg (recommended for Windows-native development)

Requirements:
- Visual Studio 2022 with "Desktop development with C++"
- [CMake](https://cmake.org/download/) on `PATH`
- [vcpkg](https://github.com/microsoft/vcpkg) checked out somewhere

Steps from a Developer Command Prompt:

```bat
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\vcpkg
%VCPKG_ROOT%\vcpkg install raylib openssl sqlite3 --triplet x64-windows
cd path\to\claudegamewindows
windows\build-msvc.bat
```

Output:
- `build-msvc\Release\claudegame.exe`
- `build-msvc\Release\claudegame_server.exe`

Run `claudegame_server.exe` first; then `claudegame.exe` and type `127.0.0.1` + port `27015` on the connect screen.

### Option B: MinGW-w64 via MSYS2

Requirements: [MSYS2](https://www.msys2.org/). From the **MSYS2 MINGW64** shell:

```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-raylib mingw-w64-x86_64-openssl \
                   mingw-w64-x86_64-sqlite3
cd /c/path/to/claudegamewindows
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j
```

Output:
- `build-mingw\claudegame.exe`
- `build-mingw\claudegame_server.exe`

### Option C: Cross-compile from a Mac (advanced)

You can build a Windows `.exe` from macOS using mingw-w64. This is what was used to verify the port — see `windows/cross-build-from-mac.sh` for the exact commands. Output ends up under `build-cross-win64/`. You'll need to fetch prebuilt Windows raylib and copy `sqlite3.c` + `sqlite3.h` into `third_party/sqlite/` first.

## Assets

`assets/Inter-Regular.ttf`, `assets/Inter-Bold.ttf`, `assets/NotoSansSC-Regular.otf`, and `assets/NotoSansSC-Bold.otf` are the same files as the Mac version. All four ship under the **SIL Open Font License v1.1** — see `assets/NOTICE_FONTS.md` for full attribution. Inter renders English/Latin UI; Noto Sans SC handles the Chinese mode (`中文` toggle).

## Firewall on Windows 11

The first time `claudegame_server.exe` binds, Windows Defender Firewall will prompt for permission. Allow both **TCP** and **UDP** on the port you bound (default `27015`). For LAN/Internet hosting see the main README's "Hosting on the public internet" section — the same NAT/port-forward steps apply.

## Differences from the Mac source

None at the source level — `src/`, `include/`, `server/`, `assets/`, and `CMakeLists.txt` are byte-identical with the Mac project. All platform-specific code lives behind `#ifdef _WIN32` (mainly in `include/PlatformNet.h`, `src/NetClient.cpp`, `server/Server.cpp`, `server/main.cpp`).

## Sync policy

This folder is kept in lockstep with `claudegame/`. Any feature or bugfix is applied to both at the same time.
