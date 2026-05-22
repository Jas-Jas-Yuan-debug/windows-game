# ClaudeGame — Networked CS Arena

A Counter-Strike-style **5v5 team deathmatch** built as a real client/server multiplayer game in C++17.

- **Server** — authoritative simulation, owns the SQLite DB, runs at 30 Hz, accepts TCP+UDP from clients.
- **Client** — thin raylib renderer. Sends inputs (UDP), receives state (UDP), uses TCP for login, chat, lobby, match metadata.
- Two-player and up — no bots. Configurable per-team size from 1v1 to 5v5.

## Download & install (no build required)

If you just want to play, you don't have to compile anything.

1. **Open GitHub** in a browser: <https://github.com/Jas-Jas-Yuan-debug/windows-game>
   (Or go to <https://github.com> and search the top bar for `windows-game` — pick the one owned by `Jas-Jas-Yuan-debug`.)
2. On the right side of the repo page, click **Releases** (or open `https://github.com/Jas-Jas-Yuan-debug/windows-game/releases`). If there is no Releases section, scroll down to the project's `downloads/` folder and click the file directly.
3. Download `ClaudeGame-windows.zip`.
4. Unzip it anywhere (Desktop, Downloads, wherever) and double-click `claudegame.exe`.

### Windows SmartScreen on first launch

Because the binary isn't signed with a paid Microsoft / EV cert, Windows Defender SmartScreen may show a blue dialog: *"Windows protected your PC"*. Click **More info**, then the **Run anyway** button that appears underneath. You only have to do this once — after the first run Windows whitelists the file.

If your antivirus quarantines `claudegame.exe`, add the unzipped folder to its exclusion list. The binary is just a raylib client + statically-linked mbedTLS / SQLite — nothing that should trip a real scanner.

**Don't double-click `claudegame_server.exe`** unless you specifically want to host a dedicated server. That's the headless game server, no GUI — running it just opens a console window that listens for network connections. For normal play, only run `claudegame.exe` and click **HOST LOCAL GAME** inside it; that spawns the server in-process.

## Build

Two supported paths on Windows. **MSVC + vcpkg** is the standard Microsoft toolchain; **MinGW-w64 via MSYS2** is the GCC route (also what the cross-compile script targets). The only external dependency is **raylib** — mbedTLS is vendored under `third_party/mbedtls/` and built automatically, and SQLite is the bundled amalgamation under `third_party/sqlite/`.

### MSVC + vcpkg (recommended)

Requirements: Visual Studio 2022 with "Desktop development with C++", [CMake](https://cmake.org/download/) on PATH, and [vcpkg](https://github.com/microsoft/vcpkg).

From a Developer Command Prompt:

```bat
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\vcpkg
%VCPKG_ROOT%\vcpkg install raylib --triplet x64-windows
cd path\to\claudegamewindows
windows\build-msvc.bat
```

Output: `build-msvc\Release\claudegame.exe` and `build-msvc\Release\claudegame_server.exe`.

### MinGW-w64 via MSYS2

Requirements: [MSYS2](https://www.msys2.org/). From the **MSYS2 MINGW64** shell:

```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-raylib
cd /c/path/to/claudegamewindows
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j
```

Output: `build-mingw\claudegame.exe` and `build-mingw\claudegame_server.exe`.

### Cross-compile from a Mac (advanced)

`windows\cross-build-from-mac.sh` uses a MinGW-w64 toolchain on macOS to produce the Windows binaries. Output lands in `build-cross-win64\`. Needs prebuilt MinGW raylib + the SQLite amalgamation under `third_party/`.

## Running

### Start a server

From `cmd.exe` or PowerShell in the build folder:

```bat
claudegame_server.exe                                  REM defaults: 0.0.0.0:27015, 5v5
claudegame_server.exe --port 27017 --team-size 1       REM 1v1 for testing
claudegame_server.exe --host 0.0.0.0 --port 27015 --team-size 5 --db data\server.sqlite
```

The server binds **both TCP and UDP** on the same port. It prints `listening on host:port` once ready. The SQLite DB and the TLS cert/key land under `%APPDATA%\ClaudeGame\`. Ctrl-C exits cleanly.

### Connect a client

Double-click `claudegame.exe`, or run it from the terminal. You'll see a connect screen — type the server's host (default `127.0.0.1`) and port (default `27015`) and hit CONNECT. Or click **HOST LOCAL GAME** to spawn an in-process server on `127.0.0.1:27015`.

### Hosting on the public internet

The server binds `0.0.0.0` by default, so it accepts external connections — but your network has to actually deliver them. Standard checklist:

1. **Allow the port** in Windows Defender Firewall. The first time `claudegame_server.exe` binds, Windows will prompt for permission — tick both Private and Public networks. To allow it manually:
   ```powershell
   New-NetFirewallRule -DisplayName "ClaudeGame TCP" -Direction Inbound -Protocol TCP -LocalPort 27015 -Action Allow
   New-NetFirewallRule -DisplayName "ClaudeGame UDP" -Direction Inbound -Protocol UDP -LocalPort 27015 -Action Allow
   ```
2. **Forward the port** on your router to your machine's LAN IP if you're behind NAT.
3. **Find your public IP** (e.g. via [ifconfig.me](https://ifconfig.me)) and give that + the port to your friends.
4. Connections are **TLS-encrypted** and passwords are PBKDF2-hashed end to end — but the server's TLS cert is **self-signed**, so out-of-band fingerprint verification is still on you. The server prints the fingerprint to stderr at startup.

## Game flow

```
ConnectScreen ─► LoginScreen ─► Menu ─┬─► Game (queue → server picks map → TDM → end)
                                      ├─► Leaderboard
                                      ├─► Chat
                                      ├─► Store (5 weapons, credits)
                                      ├─► Logout
                                      └─► Quit
```

All UI text is rendered in **Inter** for English (loaded from `assets/Inter-Regular.ttf` + `Inter-Bold.ttf`) and **Noto Sans SC** for Chinese (`assets/NotoSansSC-Regular.otf` + `NotoSansSC-Bold.otf`). Both are SIL OFL 1.1 — see [`assets/NOTICE_FONTS.md`](assets/NOTICE_FONTS.md).

## Match rules

- One shared queue. When `2 × team_size` players are queued, the server **picks a random map** from {Arena, Dust, Office} and starts the match.
- Teams come from each user's permanent BLUE/RED affiliation; uneven splits get rebalanced.
- First team to **30 kills** or whoever leads at 5:00 wins.
- Damage and fire rate depend on your selected weapon (see Store below).
- Obstacles block bullets; teammates don't. Maps have many hiding spots (L-walls, pillar clusters, crate stacks, low cover).
- Server is authoritative for every hit, position, and score.

## Weapons & Store

Every account starts with **500 credits** and the **Pistol**. Earn credits per match:
`50 · kills + 50 · headshots + 200 (if won)`. Spend them in the Store to unlock weapons, then SELECT one as your loadout for future matches.

| ID | Name    | Price | Body | Headshot | Mag | Reserve | Cooldown | Reload |
| -- | ------- | ----- | ---- | -------- | --- | ------- | -------- | ------ |
| 1  | Pistol  | free  | 25   | 75       | 12  | 48      | 0.40s    | 1.5s   |
| 2  | SMG     | 400   | 18   | 54       | 30  | 90      | 0.09s    | 2.0s   |
| 3  | Shotgun | 900   | 95   | 140      | 8   | 32      | 0.80s    | 2.6s   |
| 4  | Rifle   | 1500  | 34   | 100      | 30  | 90      | 0.12s    | 2.2s   |
| 5  | Sniper  | 2400  | 80   | 200      | 5   | 15      | 1.20s    | 3.0s   |

Server is the source of truth for these stats (`include/Weapon.h`).

## Teams & chat

Every account gets a permanent team at registration via deterministic alternation (1st BLUE, 2nd RED, 3rd BLUE, …).

| Room        | Visibility       |
| ----------- | ---------------- |
| Public      | everyone         |
| Team BLUE   | BLUE users only  |
| Team RED    | RED users only   |

Chat is **live and server-relayed**: when one player posts, every other connected client receives the message instantly via TCP. The server enforces ACLs — a RED user trying to post to `team_blue` gets `ERR|message rejected`.

## Progression

| Level  | Rank              | Unlocks         |
| ------ | ----------------- | --------------- |
| 1–2    | Silver I          | Arena           |
| 3–4    | Silver Elite      | + Dust          |
| 5–7    | Gold Nova         | + Office        |
| 8–10   | Master Guardian   |                 |
| 11–14  | Legendary Eagle   |                 |
| 15–19  | Supreme Master    |                 |
| 20+    | Global Elite      |                 |

`xp = 10·kills + 5·headshots + (50 if won) + 25` per match. `level = 1 + floor(sqrt(xp/100))`, capped at 99.

## Controls

| Input        | Action                       |
| ------------ | ---------------------------- |
| WASD         | Move                         |
| Mouse        | Look                         |
| Left click   | Fire                         |
| R            | Reload                       |
| ESC          | Leave match / back / quit    |
| TAB          | Switch chat tab / leaderboard tab |
| ENTER        | Send chat / dismiss results  |

## Wire protocol

Defined in `include/Protocol.h`. Newline-delimited messages on TCP, datagrams on UDP. Each message: pipe-separated, URL-encoded fields. Type is the first field.

Examples:
```
HELLO|1|30|5
LOGIN|alice|<64-char-hex-pbkdf2-proof>
LOGIN_OK|1|alice|BLUE|0|1|0|0|0|0|0|e099b384000a00d0
CHAT_SEND|public|hello%20world
CHAT_MSG|public|alice|hello world|1779013677
INPUT|<udp_token>|142|0.0|1.0|1.57|0.0|1            (UDP)
STATE|142|95|28|90|3|1|1|350|12|9|214000|10|0,1.2,0,3,4.5,1.0,87,BLUE,1,1,alice;...   (UDP)
```

## Schema

```sql
users          (id, username, password_hash, salt, team,
                xp, level, total_kills, total_deaths, total_headshots,
                matches_played, matches_won, credits, selected_weapon,
                created_at, last_login)
matches        (id, user_id, map_name, team, team_score, enemy_team_score,
                player_kills, player_deaths, player_score, headshots,
                xp_earned, won, played_at)
chat_messages  (id, room, user_id, username, content, sent_at)
user_weapons   (user_id, weapon_id)  -- owned inventory
```

Passwords: client sends `PBKDF2-SHA256(password, "claudegame|" + lowercase(username), 50000)` as hex; server stores `PBKDF2-SHA256(that_proof, random_16_byte_salt, 100000)` as hex. Per-user salt from `RAND_bytes` / `SecRandomCopyBytes`. All queries parameterized.

## Project layout

```
claudegame/
├── CMakeLists.txt                    ← two targets: claudegame + claudegame_server
├── include/
│   ├── Protocol.h                    ← shared wire protocol
│   ├── User.h, MatchResult.h, LeaderEntry.h, ChatMessage.h
│   ├── AuthManager.h                 ← DB layer (server uses it)
│   ├── NetClient.h                   ← TCP+UDP client wrapper
│   ├── ConnectScreen.h, LoginScreen.h, Menu.h, Game.h, Leaderboard.h, ChatRoom.h
├── src/                              ← client + shared .cpp
│   ├── main.cpp, NetClient.cpp, Protocol.cpp, AuthManager.cpp
│   └── ConnectScreen.cpp, LoginScreen.cpp, Menu.cpp, Game.cpp, Leaderboard.cpp, ChatRoom.cpp
├── server/
│   ├── main.cpp                      ← CLI entrypoint
│   ├── Server.{h,cpp}                ← select() loop, TCP+UDP, lobby
│   └── Match.{h,cpp}                 ← 30Hz authoritative simulation
└── data/                             ← server.sqlite at runtime
```

## Security & anti-cheat

### Transport
- **TLS 1.2/1.3 on the TCP channel**. mbedTLS 3.6 is vendored under `third_party/mbedtls/` and linked statically — no system OpenSSL dependency. The server auto-generates an RSA-2048 self-signed cert + key on first run, stored in the user data dir (`~/Library/Application Support/ClaudeGame/server.{crt,key}` on macOS) and reused on subsequent starts. SHA-256 cert fingerprint is printed to stderr at startup; you can share it with players for out-of-band verification. Negotiated suite is typically ECDHE-RSA + AES-256-GCM or CHACHA20-POLY1305.
- **Client verification is currently permissive** — accepts any cert. `NetClient::tlsFingerprint` exposes the live SHA-256 fingerprint of whatever the server presented, so a UI layer can do TOFU / pinning (not wired into the connect screen yet).

### Authentication
- **Two-stage PBKDF2-SHA256**. Client derives `PBKDF2(password, "claudegame|" + lowercase(username), 50000)` before sending. Server stores `PBKDF2(derived, random_salt, 100000)`. The plaintext password never leaves the client.
- **Login throttle** — 5 failed logins per username in 60 s → 60 s lockout.

### UDP packet integrity
- **Per-session HMAC-SHA256**. Server issues a fresh 32-byte key on each login (delivered via TCP). Client signs every UDP INPUT; server verifies. Forged or replayed UDP from any source (same IP or otherwise) is rejected. A bad-HMAC streak of 5 auto-disconnects the client.
- **UDP source-IP pin** — INPUT must come from the same IP as the owning TCP socket.

### Match integrity
- **PVS visibility filter** — `STATE` packets omit enemies you can't legitimately see (head *and* chest line-of-sight blocked). The client never receives positions it isn't allowed to render. Anti-wallhack.
- **Server-authoritative simulation** — movement capped by 30 Hz tick × 6 m/s regardless of input rate; fire rate enforced by per-weapon cooldown; hits decided by server raycasts.
- **Input sanity** — NaN/Inf rejected; move clamped to unit circle; pitch clamped to ±1.55 rad; old/future tick numbers dropped (anti-replay).
- **Reconnect into a running match** — if your TCP drops, your match slot is preserved for 30 s; re-login within that window and the server emits `RECONNECTED` + resends `MATCH_START` and reattaches you. After 30 s the slot forfeits.

### Behavioural heuristics (log + auto-disconnect at threshold)
- **Aim-snap detection** — |Δyaw| > 2.8 rad between consecutive inputs counted. Logs at 25/match; auto-kicks at 60.
- **Headshot streak** — logs at 5 and 10 consecutive HS kills.
- **Stat sanity** — at match end, ≥10 kills with ≥90 % HS ratio is logged.

### Hardening
- **Rate limits** — 20 TCP msgs/sec/client, 90 UDP/sec/client. TCP excess > 4× kicks. UDP excess streak of 5 kicks.
- **Per-IP caps** — 8 concurrent connections per IP; 5 account registrations per IP per hour (loopback exempt).
- **Username charset** — `[A-Za-z0-9_.\-]` only.
- **Chat** — 0.5 s min interval, 200-char cap, no whitespace-only.
- **Idle disconnects** — close if no LOGIN within 30 s of connect, or no INPUT within 30 s while in a match.
- **Suspicion bitfield** — 15-bit flag set logged on every disconnect for forensic review.

## Remaining caveats

- **Self-signed cert + no pinning UI yet.** TLS prevents passive sniffing and prevents MITM if the player verifies the fingerprint out of band, but the client UI doesn't yet display the fingerprint or remember it across connects. A first-connect MITM against an unverified server is still possible. Wire `NetClient::tlsFingerprint` into the ConnectScreen to fix.
- **PBKDF2 iteration count is fixed** at 100,000. Reasonable for 2026 hardware but no Argon2 / scrypt option.
- **The anti-cheat is heuristic.** It logs every suspicious event and auto-disconnects beyond hard thresholds. There is no DB-level ban; a determined attacker can reconnect with the same account after a kick.

Don't reuse real passwords. Don't run on a hostile network without putting it behind a firewall.

## License

ClaudeGame is released under the **GNU Affero General Public License v3.0** — see [`LICENSE`](LICENSE) for the full text. AGPL means: you're free to use, modify, and redistribute, but if you run a modified version as a network service, you must make your modified source available to your users. The bundled fonts (Inter, Noto Sans SC) are SIL OFL — see [`assets/NOTICE_FONTS.md`](assets/NOTICE_FONTS.md).
