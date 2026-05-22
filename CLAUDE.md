# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Sync policy (read this first)

This directory is the **Windows mirror** of `/Users/jason/Documents/VSCODE/claudegame/`. The two trees keep **code** in lockstep — `src/`, `include/`, `server/`, `assets/`, `CMakeLists.txt`, and `third_party/mbedtls/` are byte-identical between them. All platform differences live behind `#ifdef _WIN32` / `__APPLE__` inside those shared sources.

**Non-code files do NOT auto-mirror.** README.md, LICENSE, CLAUDE.md and similar docs can legitimately diverge — e.g. each repo has its own GitHub URL (Mac is `Jas-Jas-Yuan-debug/mac-game-by-claude-code`, Windows is `Jas-Jas-Yuan-debug/windows-game`). When you change a non-code file in one tree, **ask the user whether to mirror it** instead of copying blindly.

Code changes: copy to the same path under the sibling tree in the **same session**. Files unique to each tree:
- `claudegamewindows/windows/` (vcpkg/MinGW/cross-compile helpers — Windows only)
- `claudegamewindows/third_party/raylib/`, `third_party/sqlite/` (Windows-only vendored deps)
- `claudegamewindows/README_WINDOWS.md`
- `claudegame/mac/` (build-mac-app.sh, AppIcon, Info.plist — Mac only)
- `claudegame/third_party/raylib-mac/` (universal arm64+x86_64 raylib static lib)

There is also a third sibling repo `/Users/jason/Documents/VSCODE/claudegame-site/` that hosts the downloads page.

## Build commands

### macOS (the source-of-truth tree under `claudegame/`)
```bash
brew install cmake                                  # only tool Apple doesn't ship
bash mac/build-mac-app.sh                           # full universal .app → build-mac/ClaudeGame.app
# or just the two binaries:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

### Windows (this tree) — MSVC + vcpkg
```bat
set VCPKG_ROOT=C:\vcpkg
%VCPKG_ROOT%\vcpkg install raylib openssl sqlite3 --triplet x64-windows
windows\build-msvc.bat
:: → build-msvc\Release\claudegame{,_server}.exe
```

### Windows — MinGW-w64 from MSYS2
```bash
pacman -S --needed mingw-w64-x86_64-{toolchain,cmake,raylib,openssl,sqlite3}
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j
```

### Cross-compile Windows from a Mac (what's used to verify the port from this workstation)
```bash
bash windows/cross-build-from-mac.sh
# → build-cross-win64/claudegame{,_server}.exe
# Requires: brew install mingw-w64, and prebuilt raylib mingw + sqlite3 amalgamation
# already vendored under third_party/raylib/ and third_party/sqlite/
```

This is the typical iteration cycle on this Mac workstation: edit sources, run `cmake --build build` (Mac build) and `cmake --build build-cross-win64` (Windows verify) in parallel.

### CMake quirks
- The project enables both **C** and **CXX** because `third_party/sqlite/sqlite3.c` (Windows-only) is built into the targets when present.
- mbedTLS is built via `add_subdirectory(third_party/mbedtls EXCLUDE_FROM_ALL)`. `MBEDTLS_FATAL_WARNINGS=OFF` is required for GCC 15 / MinGW because of `-Werror=unterminated-string-initialization`.
- Mac builds are forced **universal** via `CMAKE_OSX_ARCHITECTURES=arm64;x86_64` and target `MACOSX_DEPLOYMENT_TARGET=11.0`.

### There are no tests
This codebase has no unit-test target. "Verify" means: build both targets, then `./build/claudegame_server` + connect a `./build/claudegame` and play through one match.

## High-level architecture

This is a 5v5 CS-style FPS, **two C++17 binaries from one tree**:
- `claudegame` (client) — raylib renderer + thin networking
- `claudegame_server` (headless) — authoritative simulation + SQLite DB

`CMakeLists.txt` compiles them as separate executables but **the client links the server sources too** so it can run an embedded server in-process (`HOST LOCAL GAME` button → `EmbeddedServer.cpp` spins up `Server::run` on a worker thread).

### Network model (the most important thing to internalize)

The server binds **TCP and UDP on the same port** and multiplexes both in a single `select()` loop (`server/Server.cpp::Server::run`). The two protocols carry different things:

| Channel | Used for                                                    | Encryption       |
| ------- | ----------------------------------------------------------- | ---------------- |
| **TCP** | Login, chat, lobby, match metadata, store, leaderboards     | **TLS 1.2/1.3** via vendored mbedTLS |
| **UDP** | Per-tick INPUT (client→server) and STATE (server→client)    | **HMAC-SHA256** signed, source-IP pinned |

TLS is **not** OpenSSL — mbedTLS is vendored at `third_party/mbedtls/` (15 MB after pruning, static link). `src/TlsConn.cpp` wraps non-blocking BIO callbacks that translate POSIX/Winsock `EAGAIN` to `MBEDTLS_ERR_SSL_WANT_READ/WRITE` so the `select()` loop keeps working. Self-signed RSA-2048 cert auto-generated at server first run; stored in `userDataDir()` (`~/Library/Application Support/ClaudeGame/` on macOS, `%APPDATA%\ClaudeGame\` on Windows). Fingerprint logged to stderr on startup.

UDP integrity: server hands the client a fresh 32-byte HMAC key in the `LOGIN_OK` payload (TCP-delivered). Every UDP `INPUT` is HMAC-tagged; server splits decoded fields, reconstructs via `proto::encodeLine(fields_without_tag)`, verifies. Bad-HMAC streak of 5 auto-disconnects.

### Wire protocol

Defined entirely in `include/Protocol.h` + `src/Protocol.cpp`. Newline-delimited messages on TCP, datagrams on UDP. Each line is pipe-separated, URL-encoded fields; **type is the first field**. Both client and server reuse the same `proto::encodeLine` / `proto::decodeLine` — never reimplement field parsing elsewhere.

Common message type constants: `proto::kT_Login`, `kT_LoginOk`, `kT_Hello`, `kT_Reconnected`, `kU_Input`, `kU_State`. See `Protocol.h` for the full list.

### Server tick model

`server/Match.cpp` runs the authoritative simulation. Match state is updated at **30 Hz** by `Match::tick()` called from `Server::tickMatches`. Per-client input is sampled at the start of each tick; positions/hits/scores are computed server-side and broadcast as `STATE`. **Movement is capped at 30 Hz × 6 m/s** regardless of how fast the client sends, fire rate is per-weapon cooldown, and **PVS culls** enemies the client can't legitimately see (head + chest LOS both blocked) — anti-wallhack.

### Reconnect into a running match

If a client's TCP drops mid-match, the slot is preserved for **30 seconds** via `Server::orphanSlots_` (keyed by userId). If the same user logs back in within the window, `tryReattachOrphan()` restores the slot, sends `MATCH_START` + `RECONNECTED`. After 30 s the slot forfeits. The orphan map is swept by `expireOrphans()` every loop iteration.

### Auth / passwords

**Two-stage PBKDF2-SHA256** (no SHA-256-only path remains):
1. Client computes `PBKDF2(password, "claudegame|" + lowercase(username), 50000)` and sends the hex digest as the "password" over TLS.
2. Server stores `PBKDF2(received_proof, random_16_byte_salt, 100000)`.

Plaintext never leaves the client. Backed by **CommonCrypto** on macOS, **BCrypt** on Windows, **OpenSSL** on Linux — all three branches live in `src/Crypto.cpp`. Same file also exposes `hmacSha256` (used by UDP signing) and `randomBytes`.

### Schema

SQLite tables: `users`, `matches`, `chat_messages`, `user_weapons`. Full column list in `README.md` "Schema" section. All queries are parameterized through `AuthManager` (which is misnamed — it's the whole DB layer, used by the server only).

### Resource paths

Never write to the binary's directory — that breaks under the Mac app sandbox and on read-only-installed Windows builds. Use:
- `cg::resourcePath(name)` — bundled assets (fonts, etc). Resolves to `Contents/Resources/` inside the `.app` on Mac.
- `cg::userDataPath(name)` — read/write data (SQLite, prefs, TLS pins, cert/key). Resolves to `~/Library/Application Support/ClaudeGame/` on Mac, `%APPDATA%\ClaudeGame\` on Windows.

`src/Paths.cpp` is the only place that knows the OS-specific layout.

### UI / i18n

raylib renders everything. UI is split per-screen: `ConnectScreen` → `LoginScreen` → `Menu` → one of `{Game, Leaderboard, ChatRoom, Store, SoloMenu}`. Each is a class with a `run()` method that owns its own raylib window event loop.

**Two fonts only** (both SIL OFL): Inter for English, Noto Sans SC for Chinese. Loaded by `ui::loadFonts()` (`src/UiTheme.cpp`) — reload after `i18n::toggleLanguage()` to pick up the right glyph range. **Do not add proprietary fonts** — the project's license posture depends on this.

### Anti-cheat: counters, not bans

`server/Server.cpp` maintains per-client suspicion **counters** plus thresholds (`kAimSnapKickThreshold=60`, `kBadHmacKickThreshold=5`, `kUdpFloodKickThreshold=5`, `kBadInputKickThreshold=30`). Hitting a threshold = TCP-close that client. Persisted bans are intentionally not implemented.

A 15-bit `suspicionFlags` bitfield is logged on every disconnect for forensic review.

## Common edit patterns to know

- **Adding a new message type:** add the constant in `include/Protocol.h`, add a handler branch in `server/Server.cpp::dispatchTcp` (or `readUdp`), add the client-side handler in `src/NetClient.cpp` or whichever screen consumes it.
- **Adding a new map:** add the AABB list in `server/Match.cpp` (see `buildArena`/`buildDust`/`buildOffice`), then in `src/Maps.cpp` for the client renderer. The pick happens in `Server::pickRandomMap`.
- **Adding a new weapon:** edit `include/Weapon.h` (server is source of truth for stats). Update the README table.

## License

AGPL-3.0. Required to ship in every distribution package (`.app/Contents/Resources/LICENSE` on Mac, root of the `.zip` on Windows, served from the site repo).
