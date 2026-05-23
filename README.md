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

Server is the source of truth for these stats (`include/Weapon.h` for guns, `include/Throwable.h` for throwables). Damage at the muzzle; bullets *slow down* in flight (real Newton drag — see "Ballistics" below) so a far hit does less.

### Guns (19 total)

**Originals (IDs 1–5)** — the starter five, unchanged:

| ID | Name    | Caliber | Price | Body | HS  | Mag | Reserve | Cooldown | Reload |
| -- | ------- | ------- | ----- | ---- | --- | --- | ------- | -------- | ------ |
| 1  | Pistol  | 9mm     | free  | 25   | 75  | 12  | 48      | 0.40s    | 1.5s   |
| 2  | SMG     | 9mm     | 400   | 18   | 54  | 30  | 90      | 0.09s    | 2.0s   |
| 3  | Shotgun | 12ga    | 900   | 95   | 140 | 8   | 32      | 0.80s    | 2.6s   |
| 4  | Rifle   | 5.56mm  | 1500  | 34   | 100 | 30  | 90      | 0.12s    | 2.2s   |
| 5  | Sniper  | 7.62mm  | 2400  | 80   | 200 | 5   | 15      | 1.20s    | 3.0s   |

**12.7 mm anti-materiel (IDs 6–8)** — extreme damage, slow fire, massive recoil:

| ID | Name             | Price | Body | HS  | Mag | Reserve | Cooldown | Reload | Muzzle |
| -- | ---------------- | ----- | ---- | --- | --- | ------- | -------- | ------ | ------ |
| 6  | Barrett M82      | 5000  | 180  | 350 | 10  | 30      | 1.50s    | 4.0s   | 855 m/s |
| 7  | McMillan TAC-50  | 7000  | 220  | 400 | 5   | 20      | 1.90s    | 4.5s   | 860 m/s |
| 8  | NTW-20           | 9000  | 280  | 500 | 3   | 12      | 2.40s    | 5.0s   | 720 m/s |

**7.62 mm battle rifle / DMR (IDs 9–11)** — high damage, moderate fire rate:

| ID | Name           | Price | Body | HS  | Mag | Reserve | Cooldown | Reload | Muzzle  |
| -- | -------------- | ----- | ---- | --- | --- | ------- | -------- | ------ | ------- |
| 9  | **AK-47**      | 2500  | 45   | 130 | 30  | 90      | 0.10s    | 2.5s   | 715 m/s |
| 10 | SVD Dragunov   | 3200  | 72   | 175 | 10  | 30      | 0.50s    | 3.0s   | 830 m/s |
| 11 | FN FAL         | 3500  | 55   | 150 | 20  | 60      | 0.13s    | 2.7s   | 840 m/s |

**5.56 mm assault rifle (IDs 12–15)** — medium damage, fast fire:

| ID | Name    | Price | Body | HS | Mag | Reserve | Cooldown | Reload | Muzzle  |
| -- | ------- | ----- | ---- | -- | --- | ------- | -------- | ------ | ------- |
| 12 | M4A1    | 1700  | 32   | 95 | 30  | 90      | 0.09s    | 2.1s   | 910 m/s |
| 13 | M16A4   | 1600  | 35   | 100| 30  | 90      | 0.11s    | 2.2s   | 955 m/s |
| 14 | SCAR-L  | 1900  | 34   | 98 | 30  | 90      | 0.10s    | 2.1s   | 870 m/s |
| 15 | AUG A3  | 1850  | 33   | 96 | 30  | 90      | 0.09s    | 2.3s   | 940 m/s |

**9 mm pistol / SMG (IDs 16–19)** — low damage, very fast or rapid fire:

| ID | Name        | Price | Body | HS | Mag | Reserve | Cooldown | Reload | Muzzle  |
| -- | ----------- | ----- | ---- | -- | --- | ------- | -------- | ------ | ------- |
| 16 | Glock 17    | 200   | 22   | 66 | 17  | 51      | 0.18s    | 1.4s   | 375 m/s |
| 17 | Beretta M9  | 250   | 23   | 68 | 15  | 45      | 0.20s    | 1.5s   | 380 m/s |
| 18 | MP5         | 700   | 20   | 60 | 30  | 90      | 0.075s   | 2.0s   | 400 m/s |
| 19 | P90         | 800   | 19   | 55 | 50  | 100     | 0.07s    | 2.2s   | 715 m/s |

### Throwables (30 total)

Press **G** for grenade, **F** for flashbang, **B** for bomb during a match. Default loadout = M67 / M84 / C4; per-player throwable inventory is in the Store UI but the equip pick currently defaults to those three (full equip wiring is TODO).

**Grenades (IDs 100–109)** — fragmentation + smoke:

| ID  | Name              | Price | Fuse | Radius | Damage | Notes |
| --- | ----------------- | ----- | ---- | ------ | ------ | ----- |
| 100 | M67 Fragmentation | 300   | 4.0s | 8 m    | 120    | US standard frag |
| 101 | RGD-5             | 220   | 3.5s | 7 m    | 100    | Soviet WWII-era |
| 102 | F1                | 250   | 4.0s | 9 m    | 140    | "Limonka" — heavy iron body |
| 103 | Mk2 Pineapple     | 180   | 4.5s | 7.5 m  | 110    | US WWII pineapple |
| 104 | M61               | 280   | 4.0s | 8.5 m  | 130    | Vietnam-era US frag |
| 105 | RGO               | 320   | 3.0s | 10 m   | 160    | Russian impact (timed) |
| 106 | V40 Mini          | 150   | 4.0s | 5 m    | 60     | Tiny Dutch mini |
| 107 | RDG-2 Smoke       | 100   | 2.0s | 15 m   | 8      | **Smoke** — near-zero damage, LOS block |
| 108 | M18 Smoke         | 120   | 1.8s | 14 m   | 5      | **Smoke** — colored variant |
| 109 | Mk3A2 Concussion  | 400   | 4.0s | 12 m   | 200    | Offensive blast + mild flash |

**Flashbangs (IDs 200–209)** — disorient via FLASH packets:

| ID  | Name              | Price | Fuse | Radius | Damage | Flash | Duration |
| --- | ----------------- | ----- | ---- | ------ | ------ | ----- | -------- |
| 200 | **M84 Flashbang** | 150   | 1.5s | 10 m   | 6      | 1.00  | 5.0s     |
| 201 | GBG-001           | 130   | 1.5s | 9 m    | 5      | 0.95  | 4.5s     |
| 202 | BTG-S             | 110   | 1.5s | 8.5 m  | 5      | 0.90  | 4.0s     |
| 203 | AB-EI             | 90    | 1.6s | 8 m    | 4      | 0.80  | 3.5s     |
| 204 | Stingball         | 180   | 1.5s | 10 m   | 18     | 0.70  | 2.5s     |
| 205 | B&T Diversionary  | 200   | 1.4s | 11 m   | 7      | 1.00  | 6.0s     |
| 206 | MK141 Mod 0       | 220   | 1.5s | 12 m   | 6      | 1.00  | 6.5s     |
| 207 | NICO 9-Banger     | 250   | 1.6s | 14 m   | 9      | 1.00  | 8.0s     |
| 208 | ALS Distraction   | 80    | 1.7s | 7 m    | 3      | 0.55  | 2.0s     |
| 209 | FlashShield Pro   | 180   | 1.5s | 20 m   | 4      | 0.85  | 5.0s     |

**Bombs / demolition (IDs 300–309)** — varied fuse types, big damage:

| ID  | Name                | Price | Fuse | Radius | Damage | Notes |
| --- | ------------------- | ----- | ---- | ------ | ------ | ----- |
| 300 | **C4 Block**        | 3000  | 40s  | 18 m   | 800    | Plant-and-defuse, 40s timer |
| 301 | Semtex 1H           | 1200  | 4.0s | 10 m   | 400    | Czech plastic explosive |
| 302 | TNT Charge          | 700   | 5.0s | 8 m    | 300    | 1 kg TNT block |
| 303 | **Claymore M18**    | 1800  | prox | 6 m    | 1200   | **Directional** — 60° forward cone, proximity-triggered |
| 304 | Satchel Charge M37  | 1500  | 6.0s | 12 m   | 600    | M37 demolition satchel |
| 305 | IED 5kg             | 500   | 3.0s | 10 m   | 500    | Improvised 5 kg |
| 306 | IED 10kg            | 900   | 3.5s | 14 m   | 800    | Improvised 10 kg |
| 307 | Sticky Bomb         | 1400  | contact | 7 m | 500    | Sticks to first contact + 3s fuse |
| 308 | Det-Cord Roll       | 600   | 2.5s | 3 m    | 300    | Narrow but intense linear charge |
| 309 | Demo Pack 25kg      | 2500  | 8.0s | 25 m   | 2000   | Shaped pack — area denial |

### Ballistics

Bullets are simulated as actual projectiles, not instant rays. Each shot spawns a `Projectile` on the server with the weapon's caliber-specific muzzle velocity, mass, drag coefficient, cross-section area, and aim direction. Each tick (30 Hz, sub-stepped for fast bullets):

- **Newton drag**: `F_drag = ½ ρ_air · v² · C_d · A` against the velocity vector (`ρ_air ≈ 1.225 kg/m³`).
- **Gravity**: 9.81 m/s² downward (yes, your sniper shot drops over long distance).
- **Hit damage scales by velocity²**: a bullet that's lost half its speed deals 25 % of its muzzle damage.
- **Anti-tunneling**: per-tick step is sub-divided when `v · dt > 0.3 m` so a 940 m/s rifle doesn't skip through a target between frames.

Server broadcasts a `TRACER` UDP packet on spawn and an `IMPACT` on stop (with surface kind: 0=wall, 1=body, 2=head, 3=ground, 4=ricochet). Clients render a coloured streak following the projectile and an impact particle at the landing point.

Cap: 200 in-flight projectiles per match (anyone repeatedly hitting that cap gets auto-disconnected).

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
| **G**        | Throw grenade (default M67)  |
| **F**        | Throw flashbang (default M84)|
| **B**        | Throw bomb (default C4)      |
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
