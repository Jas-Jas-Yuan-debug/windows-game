#include "Game.h"
#include "Maps.h"
#include "Protocol.h"
#include "UiTheme.h"
#include "Weapon.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
namespace {
inline std::string toStr(int v) {
    char b[32]; std::snprintf(b, sizeof(b), "%d", v); return b;
}
inline std::string toStrF(float v) {
    char b[32]; std::snprintf(b, sizeof(b), "%.3f", v); return b;
}
const char* weaponNameFor(int id) {
    const Weapon* w = weapons::lookup(id);
    if (w) return w->name;
    static const char* fallback[] = { "Pistol", "SMG", "Shotgun", "Rifle", "Sniper" };
    if (id >= 1 && id <= 5) return fallback[id - 1];
    return "Pistol";
}
}
Game::Game(NetClient& net, const std::string& mapName)
    : net_(net), mapName_(mapName) {}
void Game::loadMap() {
    obstacles_.clear();
    if (mapName_ == "Dust") {
        floorColor_ = { 220, 200, 150, 255 };
    } else if (mapName_ == "Office") {
        floorColor_ = { 55, 55, 60, 255 };
    } else {
        floorColor_ = DARKGRAY;
    }
    MapInfo mi = mapInfoFor(mapName_);
    arenaHalfX_ = mi.halfX;
    arenaHalfZ_ = mi.halfZ;
    std::vector<MapAABB> boxes;
    buildMap(mapName_, boxes);
    obstacles_.reserve(boxes.size());
    for (const auto& b : boxes) {
        Obstacle o;
        o.center = { b.cx, b.cy, b.cz };
        o.size   = { b.sx, b.sy, b.sz };
        obstacles_.push_back(o);
    }
}
void Game::applyStatePayload(const std::vector<std::string>& m) {
    if (m.size() < 15) return;
    curTick_      = std::atoi(m[1].c_str());
    selfHp_       = std::atoi(m[2].c_str());
    selfMag_      = std::atoi(m[3].c_str());
    selfReserve_  = std::atoi(m[4].c_str());
    selfKills_    = std::atoi(m[5].c_str());
    selfDeaths_   = std::atoi(m[6].c_str());
    selfHs_       = std::atoi(m[7].c_str());
    selfScore_    = std::atoi(m[8].c_str());
    blueScore_    = std::atoi(m[9].c_str());
    redScore_     = std::atoi(m[10].c_str());
    timeLeftMs_   = std::atoi(m[11].c_str());
    selfWeaponId_ = std::atoi(m[12].c_str());
    int count     = std::atoi(m[13].c_str());
    if (count < 0) count = 0;
    if (count > proto::kMaxStatePlayers) count = proto::kMaxStatePlayers;
    players_.clear();
    players_.reserve((size_t)count);
    const std::string& list = m[14];
    size_t pos = 0;
    int parsed = 0;
    while (pos <= list.size() && parsed < count) {
        size_t sep = list.find(';', pos);
        std::string token = list.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
        if (sep == std::string::npos) pos = list.size() + 1;
        else pos = sep + 1;
        if (token.empty()) continue;
        std::vector<std::string> parts;
        parts.reserve(10);
        size_t p = 0;
        while (p <= token.size()) {
            size_t c = token.find(',', p);
            std::string part = token.substr(p, c == std::string::npos ? std::string::npos : c - p);
            parts.push_back(part);
            if (c == std::string::npos) break;
            p = c + 1;
        }
        if (parts.size() < 10) continue;
        PlayerEntry pe;
        pe.slot  = std::atoi(parts[0].c_str());
        pe.x     = (float)std::atof(parts[1].c_str());
        pe.y     = (float)std::atof(parts[2].c_str());
        pe.z     = (float)std::atof(parts[3].c_str());
        pe.yaw   = (float)std::atof(parts[4].c_str());
        pe.hp    = std::atoi(parts[5].c_str());
        pe.team  = parts[6];
        pe.alive = parts[7] != "0";
        pe.uid   = std::atoi(parts[8].c_str());
        pe.name  = proto::urlDecode(parts[9]);
        players_.push_back(std::move(pe));
        ++parsed;
    }
    if (selfWeaponId_ < 1 || selfWeaponId_ > proto::kWeaponCount) {
        selfWeaponId_ = proto::kWeaponPistol;
    }
}
void Game::handleTcp(const std::vector<std::string>& m) {
    if (m.empty()) return;
    const std::string& t = m[0];
    if (t == proto::kT_QueueStatus) {
        if (m.size() >= 5) {
            if (!m[1].empty() && m[1] != "?") {
                mapName_ = m[1];
            }
            queueBlue_ = std::atoi(m[2].c_str());
            queueRed_  = std::atoi(m[3].c_str());
            queueNeed_ = std::atoi(m[4].c_str());
        }
    } else if (t == proto::kT_MatchStart) {
        if (m.size() >= 4) {
            mapName_ = m[1];
            myTeam_  = m[2];
            roster_.clear();
            loadMap();
            phase_ = Phase::InMatch;
        }
    } else if (t == proto::kT_MatchPlayer) {
        if (m.size() >= 5) {
            RosterEntry r;
            r.slot = std::atoi(m[1].c_str());
            r.uid  = std::atoi(m[2].c_str());
            r.name = m[3];
            r.team = m[4];
            roster_.push_back(std::move(r));
        }
    } else if (t == proto::kT_MatchEnd) {
        if (m.size() >= 10) {
            won_       = m[1] != "0";
            endBlue_   = std::atoi(m[2].c_str());
            endRed_    = std::atoi(m[3].c_str());
            endKills_  = std::atoi(m[4].c_str());
            endDeaths_ = std::atoi(m[5].c_str());
            endHs_     = std::atoi(m[6].c_str());
            xpEarned_  = std::atoi(m[7].c_str());
            newLevel_  = std::atoi(m[8].c_str());
            leveledUp_ = m[9] != "0";
            creditsEarned_ = (m.size() >= 11) ? std::atoi(m[10].c_str()) : 0;
            newCredits_    = (m.size() >= 12) ? std::atoi(m[11].c_str()) : net_.credits;
            net_.credits = newCredits_;
            net_.level   = newLevel_;
            net_.xp     += xpEarned_;
            phase_ = Phase::Ended;
        }
    } else if (t == proto::kT_StoreBought) {
        if (m.size() >= 3) net_.credits = std::atoi(m[2].c_str());
    } else if (t == proto::kT_WeaponOk) {
        if (m.size() >= 2) net_.selectedWeapon = std::atoi(m[1].c_str());
    }
}
void Game::handleUdp(const std::vector<std::string>& m) {
    if (m.empty()) return;
    if (m[0] == proto::kU_State) {
        applyStatePayload(m);
    }
}
void Game::sendInput(bool fire) {
    float mx = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
    float mz = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
    net_.sendUdp({
        proto::kU_Input,
        net_.udpToken,
        toStr(sendTick_++),
        toStrF(mx),
        toStrF(mz),
        toStrF(yaw_),
        toStrF(pitch_),
        fire ? std::string("1") : std::string("0"),
    });
}
void Game::updateLocalCamera(float dt) {
    (void)dt;
    Vector2 md = GetMouseDelta();
    yaw_   += md.x * 0.0025f;
    pitch_ += md.y * 0.0025f;
    if (pitch_ > 1.4f) pitch_ = 1.4f;
    if (pitch_ < -1.4f) pitch_ = -1.4f;
    PlayerEntry* self = findSelfPlayer();
    Vector3 pos;
    if (self) {
        pos = { self->x, kPlayerEyeY, self->z };
    } else {
        pos = camera_.position;
    }
    camera_.position = pos;
    Vector3 fwd = {
        std::cos(pitch_) * std::sin(yaw_),
        -std::sin(pitch_),
        std::cos(pitch_) * std::cos(yaw_)
    };
    camera_.target = Vector3Add(camera_.position, fwd);
    camera_.up = { 0.0f, 1.0f, 0.0f };
}
Game::PlayerEntry* Game::findSelfPlayer() {
    for (auto& p : players_) {
        if (p.uid == net_.userId) return &p;
    }
    return nullptr;
}
void Game::draw3D() {
    BeginMode3D(camera_);
    DrawPlane({0.0f, 0.0f, 0.0f}, {arenaHalfX_ * 2.0f, arenaHalfZ_ * 2.0f}, floorColor_);
    DrawGrid(50, 1.0f);
    Color wallColor = { 60, 60, 70, 255 };
    float wallH = 3.0f;
    DrawCube({ 0.0f, wallH * 0.5f,  arenaHalfZ_}, arenaHalfX_ * 2.0f, wallH, 0.5f, wallColor);
    DrawCube({ 0.0f, wallH * 0.5f, -arenaHalfZ_}, arenaHalfX_ * 2.0f, wallH, 0.5f, wallColor);
    DrawCube({ arenaHalfX_, wallH * 0.5f, 0.0f}, 0.5f, wallH, arenaHalfZ_ * 2.0f, wallColor);
    DrawCube({-arenaHalfX_, wallH * 0.5f, 0.0f}, 0.5f, wallH, arenaHalfZ_ * 2.0f, wallColor);
    for (const auto& o : obstacles_) {
        DrawCube(o.center, o.size.x, o.size.y, o.size.z, BROWN);
        DrawCubeWires(o.center, o.size.x, o.size.y, o.size.z, BLACK);
    }
    Color blueCol = { 40, 80, 200, 255 };
    Color redCol  = { 200, 50, 50, 255 };
    for (const auto& p : players_) {
        if (!p.alive) continue;
        if (p.uid == net_.userId) continue;
        Color c = (p.team == "BLUE") ? blueCol : redCol;
        Vector3 cpos = { p.x, p.y + 1.0f, p.z };
        DrawCube(cpos, 1.0f, 2.0f, 1.0f, c);
        DrawCubeWires(cpos, 1.0f, 2.0f, 1.0f, BLACK);
    }
    EndMode3D();
}
void Game::drawHUD() {
    int sw = kScreenW;
    int sh = kScreenH;
    for (const auto& p : players_) {
        if (!p.alive) continue;
        if (p.uid == net_.userId) continue;
        Vector3 head = { p.x, p.y + 2.4f, p.z };
        Vector2 sp = GetWorldToScreen(head, camera_);
        if (sp.x < 0 || sp.x > sw || sp.y < 0 || sp.y > sh) continue;
        Color c = (p.team == "BLUE") ? Color{ 40, 120, 240, 255 } : Color{ 240, 80, 80, 255 };
        const char* tag = p.name.c_str();
        int tw = ui::measureText(tag, 14);
        DrawRectangle((int)sp.x - tw/2 - 3, (int)sp.y - 8, tw + 6, 16, { 0,0,0,150 });
        ui::drawText(tag, (int)sp.x - tw/2, (int)sp.y - 6, 14, c);
    }
    char scoreBuf[64];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "BLUE  %d  -  %d  RED", blueScore_, redScore_);
    int scoreFs = 36;
    int swid = ui::measureTextBold(scoreBuf, scoreFs);
    DrawRectangle(sw/2 - swid/2 - 16, 8, swid + 32, scoreFs + 16, { 0,0,0,160 });
    ui::drawTextBold(scoreBuf, sw/2 - swid/2, 16, scoreFs, RAYWHITE);
    int total = std::max(0, timeLeftMs_) / 1000;
    int mm = total / 60, ss = total % 60;
    char tbuf[16];
    std::snprintf(tbuf, sizeof(tbuf), "%d:%02d", mm, ss);
    int tw = ui::measureTextBold(tbuf, 24);
    ui::drawTextBold(tbuf, sw/2 - tw/2, 16 + scoreFs + 4, 24, YELLOW);
    ui::drawTextBold(net_.username.c_str(), 12, 10, 22, RAYWHITE);
    char tlbuf[128];
    std::snprintf(tlbuf, sizeof(tlbuf), "Team %s  Lv %d", myTeam_.c_str(), net_.level);
    ui::drawText(tlbuf, 12, 36, 18, { 200, 200, 220, 255 });
    std::snprintf(tlbuf, sizeof(tlbuf), "Map: %s", mapName_.c_str());
    ui::drawText(tlbuf, 12, 58, 16, GRAY);
    int barW = 240, barH = 24;
    int barX = 12, barY = sh - barH - 16;
    DrawRectangle(barX - 2, barY - 2, barW + 4, barH + 4, BLACK);
    DrawRectangle(barX, barY, barW, barH, { 40, 0, 0, 255 });
    float frac = std::clamp((float)selfHp_ / 100.0f, 0.0f, 1.0f);
    Color hc = frac > 0.5f ? GREEN : (frac > 0.25f ? ORANGE : RED);
    DrawRectangle(barX, barY, (int)(barW * frac), barH, hc);
    char hpbuf[32];
    std::snprintf(hpbuf, sizeof(hpbuf), "HP %d", selfHp_);
    ui::drawTextBold(hpbuf, barX + 8, barY + 4, 18, RAYWHITE);
    const char* wName = weaponNameFor(selfWeaponId_);
    char wTag[64];
    std::snprintf(wTag, sizeof(wTag), "%s #%d", wName, selfWeaponId_);
    int wnFs = 22;
    int wnW = ui::measureTextBold(wTag, wnFs);
    int wnX = sw - wnW - 16;
    int wnY = sh - 76;
    DrawRectangle(wnX - 8, wnY - 4, wnW + 16, wnFs + 8, { 0, 0, 0, 150 });
    ui::drawTextBold(wTag, wnX, wnY, wnFs, GOLD);
    char ammoBuf[32];
    const char* ammoStr;
    if (selfHp_ <= 0) {
        ammoStr = "RESPAWNING";
    } else {
        std::snprintf(ammoBuf, sizeof(ammoBuf), "%d / %d", selfMag_, selfReserve_);
        ammoStr = ammoBuf;
    }
    int aw = ui::measureTextBold(ammoStr, 28);
    ui::drawTextBold(ammoStr, sw - aw - 16, sh - 42, 28, RAYWHITE);
    char st[128];
    std::snprintf(st, sizeof(st), "K %d   D %d   HS %d   Score %d",
                  selfKills_, selfDeaths_, selfHs_, selfScore_);
    int stw = ui::measureText(st, 20);
    int panelX = sw/2 - stw/2 - 12;
    int panelY = sh - 36;
    DrawRectangle(panelX, panelY - 4, stw + 24, 28, { 0,0,0,150 });
    ui::drawText(st, sw/2 - stw/2, panelY, 20, RAYWHITE);
    int cx = sw / 2, cy = sh / 2;
    DrawLine(cx - 8, cy, cx + 8, cy, RAYWHITE);
    DrawLine(cx, cy - 8, cx, cy + 8, RAYWHITE);
}
void Game::drawEndScreen() {
    int sw = kScreenW;
    int sh = kScreenH;
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.75f));
    const char* title = won_ ? "VICTORY!" : "DEFEAT";
    Color tc = won_ ? GOLD : RED;
    int fs = 72;
    int tw = ui::measureTextBold(title, fs);
    ui::drawTextBold(title, sw/2 - tw/2, 80, fs, tc);
    int leftX = sw/2 - 280;
    int rightX = sw/2 + 20;
    int y0 = 200;
    ui::drawTextBold("MATCH STATS", leftX, y0, 26, RAYWHITE);
    char b1[128];
    std::snprintf(b1, sizeof(b1), "Map: %s", mapName_.c_str());
    ui::drawText(b1, leftX, y0 + 40, 20, LIGHTGRAY);
    std::snprintf(b1, sizeof(b1), "Score: BLUE %d - %d RED", endBlue_, endRed_);
    ui::drawText(b1, leftX, y0 + 68, 20, LIGHTGRAY);
    std::snprintf(b1, sizeof(b1), "Your K/D: %d / %d", endKills_, endDeaths_);
    ui::drawText(b1, leftX, y0 + 96, 20, LIGHTGRAY);
    std::snprintf(b1, sizeof(b1), "Headshots: %d", endHs_);
    ui::drawText(b1, leftX, y0 + 124, 20, LIGHTGRAY);
    ui::drawTextBold("PROGRESSION", rightX, y0, 26, RAYWHITE);
    char p1[128];
    std::snprintf(p1, sizeof(p1), "XP earned: +%d", xpEarned_);
    ui::drawText(p1, rightX, y0 + 40, 20, YELLOW);
    std::snprintf(p1, sizeof(p1), "Level: %d", newLevel_);
    ui::drawText(p1, rightX, y0 + 68, 20, LIGHTGRAY);
    std::snprintf(p1, sizeof(p1), "+%d credits  (Total: %d)", creditsEarned_, newCredits_);
    ui::drawText(p1, rightX, y0 + 96, 20, GOLD);
    if (leveledUp_) {
        char lu[64];
        std::snprintf(lu, sizeof(lu), "LEVEL UP! -> Lv %d", newLevel_);
        int luw = ui::measureTextBold(lu, 32);
        ui::drawTextBold(lu, sw/2 - luw/2, y0 + 200, 32, YELLOW);
    }
    const char* sub = "Press ENTER to continue";
    int sw2 = ui::measureText(sub, 22);
    ui::drawText(sub, sw/2 - sw2/2, sh - 60, 22, RAYWHITE);
}
void Game::runQueueLoop() {
    EnableCursor();
    while (!WindowShouldClose() && phase_ == Phase::Queue) {
        net_.poll();
        auto tmsgs = net_.drainTcp();
        for (auto& m : tmsgs) handleTcp(m);
        auto umsgs = net_.drainUdp();
        for (auto& m : umsgs) handleUdp(m);
        if (!net_.isConnected()) { phase_ = Phase::Aborted; break; }
        if (IsKeyPressed(KEY_ESCAPE)) {
            net_.sendTcp({ proto::kT_QueueLeave });
            phase_ = Phase::Aborted;
            break;
        }
        BeginDrawing();
        ClearBackground({ 20, 20, 28, 255 });
        const char* title = "WAITING FOR PLAYERS";
        int titleFs = 40;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (kScreenW - tw) / 2, 180, titleFs, GOLD);
        const char* mapHint = "Map: (server picks at match start)";
        if (!mapName_.empty()) {
            char info[128];
            std::snprintf(info, sizeof(info), "Map: %s", mapName_.c_str());
            int iw = ui::measureText(info, 22);
            ui::drawText(info, (kScreenW - iw) / 2, 250, 22, RAYWHITE);
        } else {
            int iw = ui::measureText(mapHint, 22);
            ui::drawText(mapHint, (kScreenW - iw) / 2, 250, 22, LIGHTGRAY);
        }
        char loadout[96];
        std::snprintf(loadout, sizeof(loadout), "Loadout: %s",
                      weaponNameFor(net_.selectedWeapon));
        int lw = ui::measureTextBold(loadout, 22);
        ui::drawTextBold(loadout, (kScreenW - lw) / 2, 280, 22, { 180, 220, 255, 255 });
        char counts[128];
        std::snprintf(counts, sizeof(counts), "Blue %d   Red %d   (need %d more)",
                      queueBlue_, queueRed_, queueNeed_);
        int cw = ui::measureText(counts, 24);
        ui::drawText(counts, (kScreenW - cw) / 2, 320, 24, SKYBLUE);
        const char* esc = "ESC to leave queue";
        int ew = ui::measureText(esc, 18);
        ui::drawText(esc, (kScreenW - ew) / 2, kScreenH - 60, 18, LIGHTGRAY);
        EndDrawing();
    }
}
void Game::runMatchLoop() {
    DisableCursor();
    camera_.fovy = 70.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    camera_.up = { 0.0f, 1.0f, 0.0f };
    camera_.position = { 0.0f, kPlayerEyeY, 0.0f };
    camera_.target = { 0.0f, kPlayerEyeY, 1.0f };
    lastInputSent_ = GetTime();
    while (!WindowShouldClose() && phase_ == Phase::InMatch) {
        float dt = GetFrameTime();
        net_.poll();
        auto tmsgs = net_.drainTcp();
        for (auto& m : tmsgs) handleTcp(m);
        auto umsgs = net_.drainUdp();
        for (auto& m : umsgs) handleUdp(m);
        if (!net_.isConnected()) { phase_ = Phase::Aborted; break; }
        if (IsKeyPressed(KEY_ESCAPE)) {
            net_.sendTcp({ proto::kT_LeaveMatch });
            phase_ = Phase::Aborted;
            break;
        }
        if (IsKeyPressed(KEY_R)) {
            net_.sendTcp({ proto::kT_Reload });
        }
        updateLocalCamera(dt);
        double now = GetTime();
        double interval = 1.0 / (double)std::max(1, net_.serverTickRate);
        if (now - lastInputSent_ >= interval) {
            lastInputSent_ = now;
            bool fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            sendInput(fire);
        }
        BeginDrawing();
        ClearBackground({ 20, 20, 28, 255 });
        draw3D();
        drawHUD();
        EndDrawing();
    }
}
void Game::runEndLoop() {
    EnableCursor();
    while (!WindowShouldClose() && phase_ == Phase::Ended) {
        net_.poll();
        net_.drainTcp();
        net_.drainUdp();
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            break;
        }
        BeginDrawing();
        ClearBackground({ 20, 20, 28, 255 });
        draw3D();
        drawHUD();
        drawEndScreen();
        EndDrawing();
    }
}
void Game::run() {
    InitWindow(kScreenW, kScreenH, "ClaudeGame - Team Deathmatch");
    SetTargetFPS(60);
    SetExitKey(0);
    ui::loadFonts();
    loadMap();
    net_.sendTcp({ proto::kT_QueueJoin, mapName_ });
    runQueueLoop();
    if (phase_ == Phase::InMatch) {
        runMatchLoop();
    }
    if (phase_ == Phase::Ended) {
        runEndLoop();
    }
    EnableCursor();
    CloseWindow();
}
