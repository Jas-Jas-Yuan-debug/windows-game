#include "LocalMatch.h"
#include "UiTheme.h"
#include "I18n.h"
#include "Weapon.h"
#include <raylib.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>
#include <algorithm>
namespace {
constexpr int kScreenW = 1280;
constexpr int kScreenH = 720;
constexpr float kEyeY = 1.7f;
constexpr float kMoveSpeed = 6.0f;
constexpr float kMouseSens = 0.0025f;
constexpr float kPlayerHalfX = 0.4f;
constexpr float kPlayerHalfZ = 0.4f;
constexpr float kPlayerHeadY = 2.0f;
constexpr float kPlayerHeadMin = 1.3f;
bool aabbBlocks(float x, float z, const MapAABB& b) {
    float minX = b.cx - b.sx * 0.5f - kPlayerHalfX;
    float maxX = b.cx + b.sx * 0.5f + kPlayerHalfX;
    float minZ = b.cz - b.sz * 0.5f - kPlayerHalfZ;
    float maxZ = b.cz + b.sz * 0.5f + kPlayerHalfZ;
    return x >= minX && x <= maxX && z >= minZ && z <= maxZ;
}
bool raySegHitsAabb(Vector3 from, Vector3 to, const MapAABB& b, float& tHit) {
    Vector3 dir = { to.x - from.x, to.y - from.y, to.z - from.z };
    float minX = b.cx - b.sx * 0.5f, maxX = b.cx + b.sx * 0.5f;
    float minY = b.cy - b.sy * 0.5f, maxY = b.cy + b.sy * 0.5f;
    float minZ = b.cz - b.sz * 0.5f, maxZ = b.cz + b.sz * 0.5f;
    float tmin = 0.0f, tmax = 1.0f;
    for (int i = 0; i < 3; ++i) {
        float o = i == 0 ? from.x : (i == 1 ? from.y : from.z);
        float d = i == 0 ? dir.x  : (i == 1 ? dir.y  : dir.z);
        float lo = i == 0 ? minX : (i == 1 ? minY : minZ);
        float hi = i == 0 ? maxX : (i == 1 ? maxY : maxZ);
        if (std::fabs(d) < 1e-6f) {
            if (o < lo || o > hi) return false;
        } else {
            float t1 = (lo - o) / d;
            float t2 = (hi - o) / d;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return true;
}
}
LocalMatch::LocalMatch(NetClient& net, Mode mode) : net_(net), mode_(mode) {
    weaponId_ = net_.selectedWeapon > 0 ? net_.selectedWeapon : 1;
}
void LocalMatch::initWorld() {
    auto names = allMapNames();
    static std::mt19937 rng((unsigned)std::time(nullptr));
    std::uniform_int_distribution<int> pick(0, (int)names.size() - 1);
    mapName_ = names[pick(rng)];
    obstacles_.clear();
    buildMap(mapName_, obstacles_);
    mapInfo_ = mapInfoFor(mapName_);
    self_.pos = { mapInfo_.blueX, 0.0f, mapInfo_.blueZ };
    self_.yaw = std::atan2(-self_.pos.x, -self_.pos.z);
    self_.pitch = 0.0f;
    self_.hp = 100;
    self_.alive = true;
    self_.weaponId = weaponId_;
    const Weapon* w = weapons::lookup(weaponId_);
    if (w) {
        self_.mag = w->magSize;
        self_.reserve = w->reserve;
    }
    self_.team = 0;
    self_.name = net_.username.empty() ? "you" : net_.username;
    bots_.clear();
    dummies_.clear();
    if (mode_ == Mode::VsBots) {
        int n = 4;
        for (int i = 0; i < n; ++i) {
            Actor b{};
            b.isBot = true;
            b.team = 1;
            float baseX = mapInfo_.redX;
            float baseZ = mapInfo_.redZ;
            float ofs = (float)(i - (n-1) / 2.0f) * 3.0f;
            if (mapInfo_.spawnUsesX) { b.pos = { baseX, 0.0f, baseZ + ofs }; }
            else                     { b.pos = { baseX + ofs, 0.0f, baseZ }; }
            b.yaw = std::atan2(-b.pos.x, -b.pos.z);
            b.hp = 100;
            b.alive = true;
            b.weaponId = ((i % 4) + 1);
            const Weapon* bw = weapons::lookup(b.weaponId);
            if (bw) { b.mag = bw->magSize; b.reserve = bw->reserve; }
            char nm[32]; std::snprintf(nm, sizeof(nm), "Bot%d", i + 1);
            b.name = nm;
            bots_.push_back(b);
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            Dummy d{};
            float ang = (float)i / 8.0f * 6.28318f;
            float r = 8.0f + (float)(i % 3) * 2.5f;
            d.pos = { std::cos(ang) * r, 0.0f, std::sin(ang) * r };
            bool inObstacle = false;
            for (auto& o : obstacles_) {
                if (aabbBlocks(d.pos.x, d.pos.z, o)) { inObstacle = true; break; }
            }
            if (inObstacle) {
                d.pos.x *= 1.5f; d.pos.z *= 1.5f;
            }
            d.hp = 100;
            dummies_.push_back(d);
        }
    }
    camera_.position = { self_.pos.x, kEyeY, self_.pos.z };
    camera_.target = { self_.pos.x + std::sin(self_.yaw), kEyeY, self_.pos.z + std::cos(self_.yaw) };
    camera_.up = { 0.0f, 1.0f, 0.0f };
    camera_.fovy = 70.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    matchTime_ = 0.0f;
    selfKills_ = 0;
    botKills_ = 0;
    dummyHits_ = 0;
    ended_ = false;
}
bool LocalMatch::isBlockedAt(float x, float z) const {
    if (std::fabs(x) > mapInfo_.halfX - kPlayerHalfX) return true;
    if (std::fabs(z) > mapInfo_.halfZ - kPlayerHalfZ) return true;
    for (auto& b : obstacles_) {
        if (aabbBlocks(x, z, b)) return true;
    }
    return false;
}
bool LocalMatch::rayBlocked(Vector3 from, Vector3 to) const {
    for (auto& b : obstacles_) {
        float t;
        if (raySegHitsAabb(from, to, b, t) && t >= 0.0f && t <= 1.0f) return true;
    }
    return false;
}
void LocalMatch::updateSelf(float dt) {
    if (!self_.alive) {
        self_.respawn -= dt;
        if (self_.respawn <= 0.0f) respawn(self_);
        return;
    }
    Vector2 md = GetMouseDelta();
    self_.yaw += md.x * kMouseSens;
    self_.pitch -= md.y * kMouseSens;
    if (self_.pitch > 1.45f) self_.pitch = 1.45f;
    if (self_.pitch < -1.45f) self_.pitch = -1.45f;
    float fx = std::sin(self_.yaw), fz = std::cos(self_.yaw);
    float rx = std::cos(self_.yaw), rz = -std::sin(self_.yaw);
    float mx = 0.0f, mz = 0.0f;
    if (IsKeyDown(KEY_W)) { mx += fx; mz += fz; }
    if (IsKeyDown(KEY_S)) { mx -= fx; mz -= fz; }
    if (IsKeyDown(KEY_D)) { mx += rx; mz += rz; }
    if (IsKeyDown(KEY_A)) { mx -= rx; mz -= rz; }
    float ml = std::sqrt(mx*mx + mz*mz);
    if (ml > 0.001f) { mx /= ml; mz /= ml; }
    float nx = self_.pos.x + mx * kMoveSpeed * dt;
    float nz = self_.pos.z + mz * kMoveSpeed * dt;
    if (!isBlockedAt(nx, self_.pos.z)) self_.pos.x = nx;
    if (!isBlockedAt(self_.pos.x, nz)) self_.pos.z = nz;
    self_.cooldown -= dt;
    if (self_.reload > 0.0f) {
        self_.reload -= dt;
        if (self_.reload <= 0.0f) {
            const Weapon* w = weapons::lookup(self_.weaponId);
            if (w) {
                int need = w->magSize - self_.mag;
                int take = std::min(need, self_.reserve);
                self_.mag += take;
                self_.reserve -= take;
            }
        }
    }
    if (IsKeyPressed(KEY_R) && self_.reload <= 0.0f) {
        const Weapon* w = weapons::lookup(self_.weaponId);
        if (w && self_.mag < w->magSize && self_.reserve > 0) {
            self_.reload = w->reloadSec;
        }
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && self_.cooldown <= 0.0f && self_.reload <= 0.0f && self_.mag > 0) {
        fireSelf();
    }
    camera_.position = { self_.pos.x, kEyeY, self_.pos.z };
    Vector3 fwd = {
        std::cos(self_.pitch) * std::sin(self_.yaw),
        std::sin(self_.pitch),
        std::cos(self_.pitch) * std::cos(self_.yaw)
    };
    camera_.target = { camera_.position.x + fwd.x, camera_.position.y + fwd.y, camera_.position.z + fwd.z };
}
void LocalMatch::fireSelf() {
    const Weapon* w = weapons::lookup(self_.weaponId);
    if (!w) return;
    self_.cooldown = w->cooldownSec;
    self_.mag--;
    Vector3 from = camera_.position;
    Vector3 dir = {
        std::cos(self_.pitch) * std::sin(self_.yaw),
        std::sin(self_.pitch),
        std::cos(self_.pitch) * std::cos(self_.yaw)
    };
    Vector3 to = { from.x + dir.x * 100.0f, from.y + dir.y * 100.0f, from.z + dir.z * 100.0f };
    if (mode_ == Mode::VsBots) {
        int bestIdx = -1;
        float bestT = 1.0f;
        bool bestHs = false;
        for (size_t i = 0; i < bots_.size(); ++i) {
            Actor& b = bots_[i];
            if (!b.alive) continue;
            MapAABB body{ b.pos.x, b.pos.y + 1.0f, b.pos.z, 0.8f, 2.0f, 0.8f };
            float t;
            if (raySegHitsAabb(from, to, body, t) && t >= 0.0f && t < bestT) {
                Vector3 hitPt = { from.x + dir.x * 100.0f * t, from.y + dir.y * 100.0f * t, from.z + dir.z * 100.0f * t };
                bool obstacleCloser = false;
                for (auto& ob : obstacles_) {
                    float ot;
                    if (raySegHitsAabb(from, to, ob, ot) && ot >= 0.0f && ot < t) { obstacleCloser = true; break; }
                }
                if (obstacleCloser) continue;
                bool hs = hitPt.y >= b.pos.y + kPlayerHeadMin && hitPt.y <= b.pos.y + kPlayerHeadY;
                bestT = t;
                bestIdx = (int)i;
                bestHs = hs;
            }
        }
        if (bestIdx >= 0) {
            Actor& b = bots_[bestIdx];
            int dmg = bestHs ? w->damageHs : w->damageBody;
            b.hp -= dmg;
            if (b.hp <= 0) {
                b.alive = false;
                b.respawn = 3.0f;
                selfKills_++;
            }
        }
    } else {
        int bestIdx = -1;
        float bestT = 1.0f;
        bool bestHs = false;
        for (size_t i = 0; i < dummies_.size(); ++i) {
            Dummy& d = dummies_[i];
            if (d.hp <= 0) continue;
            MapAABB body{ d.pos.x, 1.0f, d.pos.z, 0.8f, 2.0f, 0.8f };
            float t;
            if (raySegHitsAabb(from, to, body, t) && t >= 0.0f && t < bestT) {
                bool obstacleCloser = false;
                for (auto& ob : obstacles_) {
                    float ot;
                    if (raySegHitsAabb(from, to, ob, ot) && ot >= 0.0f && ot < t) { obstacleCloser = true; break; }
                }
                if (obstacleCloser) continue;
                Vector3 hitPt = { from.x + dir.x * 100.0f * t, from.y + dir.y * 100.0f * t, from.z + dir.z * 100.0f * t };
                bool hs = hitPt.y >= kPlayerHeadMin && hitPt.y <= kPlayerHeadY;
                bestT = t;
                bestIdx = (int)i;
                bestHs = hs;
            }
        }
        if (bestIdx >= 0) {
            Dummy& d = dummies_[bestIdx];
            int dmg = bestHs ? w->damageHs : w->damageBody;
            d.hp -= dmg;
            d.hitCount++;
            dummyHits_++;
            if (d.hp <= 0) {
                d.respawn = 2.0f;
            }
        }
    }
}
void LocalMatch::respawn(Actor& a) {
    a.hp = 100;
    a.alive = true;
    a.cooldown = 0.0f;
    a.reload = 0.0f;
    const Weapon* w = weapons::lookup(a.weaponId);
    if (w) { a.mag = w->magSize; a.reserve = w->reserve; }
    if (a.team == 0) {
        a.pos = { mapInfo_.blueX, 0.0f, mapInfo_.blueZ };
    } else {
        float baseX = mapInfo_.redX, baseZ = mapInfo_.redZ;
        if (mapInfo_.spawnUsesX) a.pos = { baseX, 0.0f, baseZ + ((float)std::rand() / RAND_MAX - 0.5f) * 6.0f };
        else                     a.pos = { baseX + ((float)std::rand() / RAND_MAX - 0.5f) * 6.0f, 0.0f, baseZ };
    }
}
void LocalMatch::updateBots(float dt) {
    if (mode_ != Mode::VsBots) return;
    for (auto& b : bots_) {
        if (!b.alive) {
            b.respawn -= dt;
            if (b.respawn <= 0.0f) respawn(b);
            continue;
        }
        BotView v{};
        v.selfPos = b.pos;
        v.selfYaw = b.yaw;
        v.selfHp  = b.hp;
        v.targetPos = self_.pos;
        v.targetAlive = self_.alive;
        if (self_.alive) {
            Vector3 fromEye = { b.pos.x, b.pos.y + kEyeY, b.pos.z };
            Vector3 toEye   = { self_.pos.x, kEyeY, self_.pos.z };
            v.targetVisible = !rayBlocked(fromEye, toEye);
        } else {
            v.targetVisible = false;
        }
        BotInput inp = computeBotInput(b.bot, v, dt);
        float nx = b.pos.x + inp.moveX * kMoveSpeed * 0.85f * dt;
        float nz = b.pos.z + inp.moveZ * kMoveSpeed * 0.85f * dt;
        if (!isBlockedAt(nx, b.pos.z)) b.pos.x = nx;
        if (!isBlockedAt(b.pos.x, nz)) b.pos.z = nz;
        b.yaw = inp.yaw;
        b.pitch = inp.pitch;
        b.cooldown -= dt;
        if (b.reload > 0.0f) {
            b.reload -= dt;
            if (b.reload <= 0.0f) {
                const Weapon* w = weapons::lookup(b.weaponId);
                if (w) {
                    int need = w->magSize - b.mag;
                    int take = std::min(need, b.reserve);
                    b.mag += take; b.reserve -= take;
                }
            }
        }
        if (b.mag == 0 && b.reload <= 0.0f && b.reserve > 0) {
            const Weapon* w = weapons::lookup(b.weaponId);
            if (w) b.reload = w->reloadSec;
        }
        if (inp.fire && b.cooldown <= 0.0f && b.reload <= 0.0f && b.mag > 0 && v.targetVisible) {
            fireBot(b);
        }
    }
}
void LocalMatch::fireBot(Actor& b) {
    const Weapon* w = weapons::lookup(b.weaponId);
    if (!w) return;
    b.cooldown = w->cooldownSec;
    b.mag--;
    Vector3 from = { b.pos.x, b.pos.y + kEyeY, b.pos.z };
    Vector3 dir = {
        std::cos(b.pitch) * std::sin(b.yaw),
        std::sin(b.pitch),
        std::cos(b.pitch) * std::cos(b.yaw)
    };
    Vector3 to = { from.x + dir.x * 100.0f, from.y + dir.y * 100.0f, from.z + dir.z * 100.0f };
    MapAABB sbody{ self_.pos.x, self_.pos.y + 1.0f, self_.pos.z, 0.8f, 2.0f, 0.8f };
    float t;
    if (!raySegHitsAabb(from, to, sbody, t)) return;
    for (auto& ob : obstacles_) {
        float ot;
        if (raySegHitsAabb(from, to, ob, ot) && ot >= 0.0f && ot < t) return;
    }
    Vector3 hitPt = { from.x + dir.x * 100.0f * t, from.y + dir.y * 100.0f * t, from.z + dir.z * 100.0f * t };
    bool hs = hitPt.y >= self_.pos.y + kPlayerHeadMin && hitPt.y <= self_.pos.y + kPlayerHeadY;
    int dmg = hs ? w->damageHs : w->damageBody;
    self_.hp -= dmg;
    if (self_.hp <= 0 && self_.alive) {
        self_.alive = false;
        self_.respawn = 3.0f;
        botKills_++;
    }
}
void LocalMatch::draw3D() {
    BeginMode3D(camera_);
    DrawPlane({ 0, 0, 0 }, { mapInfo_.halfX * 2, mapInfo_.halfZ * 2 }, DARKGRAY);
    for (auto& b : obstacles_) {
        DrawCube({ b.cx, b.cy, b.cz }, b.sx, b.sy, b.sz, { 80, 80, 90, 255 });
        DrawCubeWires({ b.cx, b.cy, b.cz }, b.sx, b.sy, b.sz, BLACK);
    }
    if (mode_ == Mode::VsBots) {
        for (auto& b : bots_) {
            if (!b.alive) continue;
            DrawCube({ b.pos.x, 1.0f, b.pos.z }, 0.8f, 2.0f, 0.8f, { 200, 70, 70, 255 });
            DrawCube({ b.pos.x, 1.85f, b.pos.z }, 0.5f, 0.5f, 0.5f, { 220, 100, 100, 255 });
        }
    } else {
        for (auto& d : dummies_) {
            if (d.hp <= 0) continue;
            DrawCube({ d.pos.x, 1.0f, d.pos.z }, 0.8f, 2.0f, 0.8f, { 200, 180, 70, 255 });
            DrawCube({ d.pos.x, 1.85f, d.pos.z }, 0.5f, 0.5f, 0.5f, { 230, 210, 120, 255 });
        }
    }
    EndMode3D();
}
void LocalMatch::drawHUD() {
    DrawRectangle(kScreenW / 2 - 6, kScreenH / 2 - 1, 12, 2, WHITE);
    DrawRectangle(kScreenW / 2 - 1, kScreenH / 2 - 6, 2, 12, WHITE);
    char buf[128];
    int y = 12;
    std::snprintf(buf, sizeof(buf), "HP %d", std::max(0, self_.hp));
    ui::drawTextBold(buf, 16, y, 22, RAYWHITE); y += 26;
    const Weapon* w = weapons::lookup(self_.weaponId);
    if (w) {
        std::snprintf(buf, sizeof(buf), "%s  %d / %d", i18n::tr(
            self_.weaponId == 1 ? "weapon.pistol" :
            self_.weaponId == 2 ? "weapon.smg" :
            self_.weaponId == 3 ? "weapon.shotgun" :
            self_.weaponId == 4 ? "weapon.rifle" : "weapon.sniper"
        ), self_.mag, self_.reserve);
        ui::drawTextBold(buf, 16, y, 20, GOLD); y += 24;
    }
    if (self_.reload > 0.0f) {
        ui::drawText("Reloading...", 16, y, 18, SKYBLUE); y += 22;
    }
    if (!self_.alive) {
        const char* respMsg = "Respawn";
        int rw = ui::measureTextBold(respMsg, 28);
        ui::drawTextBold(respMsg, (kScreenW - rw) / 2, kScreenH / 2 + 30, 28, RED);
    }
    if (mode_ == Mode::VsBots) {
        std::snprintf(buf, sizeof(buf), "You %d - %d Bots", selfKills_, botKills_);
        int sw = ui::measureTextBold(buf, 22);
        ui::drawTextBold(buf, (kScreenW - sw) / 2, 16, 22, RAYWHITE);
        int rem = (int)std::max(0.0f, matchLimit_ - matchTime_);
        std::snprintf(buf, sizeof(buf), "%02d:%02d", rem / 60, rem % 60);
        int tw = ui::measureText(buf, 18);
        ui::drawText(buf, (kScreenW - tw) / 2, 44, 18, LIGHTGRAY);
    } else {
        char dummyFmt[64];
        std::snprintf(dummyFmt, sizeof(dummyFmt), "%s", i18n::tr("solo.hud.dummies"));
        std::snprintf(buf, sizeof(buf), dummyFmt, dummyHits_);
        ui::drawTextBold(buf, 16, y, 20, { 200, 230, 120, 255 }); y += 22;
    }
    const char* exitHint = i18n::tr("solo.hud.exit");
    int eh = ui::measureText(exitHint, 16);
    ui::drawText(exitHint, kScreenW - eh - 16, kScreenH - 28, 16, GRAY);
    if (ended_) {
        const char* msg = selfKills_ > botKills_ ? "VICTORY" : "DEFEAT";
        Color c = selfKills_ > botKills_ ? GOLD : RED;
        int mw = ui::measureTextBold(msg, 64);
        ui::drawTextBold(msg, (kScreenW - mw) / 2, kScreenH / 2 - 80, 64, c);
        const char* prompt = "ESC to return";
        int pw = ui::measureText(prompt, 22);
        ui::drawText(prompt, (kScreenW - pw) / 2, kScreenH / 2 + 10, 22, LIGHTGRAY);
    }
}
void LocalMatch::run() {
    InitWindow(kScreenW, kScreenH, mode_ == Mode::VsBots ? "ClaudeGame - Vs Bots" : "ClaudeGame - Practice");
    SetTargetFPS(60);
    SetExitKey(0);
    i18n::init();
    ui::loadFonts();
    DisableCursor();
    initWorld();
    bool done = false;
    while (!WindowShouldClose() && !done) {
        if (IsKeyPressed(KEY_ESCAPE)) { done = true; break; }
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        if (!ended_) {
            updateSelf(dt);
            updateBots(dt);
            for (auto& d : dummies_) {
                if (d.hp <= 0) {
                    d.respawn -= dt;
                    if (d.respawn <= 0.0f) { d.hp = 100; }
                }
            }
            matchTime_ += dt;
            if (mode_ == Mode::VsBots) {
                if (matchTime_ >= matchLimit_ || selfKills_ >= 15 || botKills_ >= 15) ended_ = true;
            }
        }
        BeginDrawing();
        ClearBackground({ 50, 60, 75, 255 });
        draw3D();
        drawHUD();
        EndDrawing();
    }
    EnableCursor();
    CloseWindow();
}
