#include "Game.h"
#include "Maps.h"
#include "Protocol.h"
#include "Throwable.h"
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
bool parseVec3Csv(const std::string& s, Vector3& out) {
    size_t a = s.find(',');
    if (a == std::string::npos) return false;
    size_t b = s.find(',', a + 1);
    if (b == std::string::npos) return false;
    out.x = (float)std::atof(s.substr(0, a).c_str());
    out.y = (float)std::atof(s.substr(a + 1, b - a - 1).c_str());
    out.z = (float)std::atof(s.substr(b + 1).c_str());
    return true;
}
Color tracerColor(int id) {
    switch (id) {
        case 3: return { 255, 135,  40, 255 };
        case 2: return { 255,  80,  65, 255 };
        case 1: return { 255, 230,  90, 255 };
        default: return { 220, 240, 255, 255 };
    }
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
    if (selfHp_ < lastSelfHp_) damageVignetteTimer_ = 0.35f;
    lastSelfHp_ = selfHp_;
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
    } else if (m[0] == proto::kU_Tracer && m.size() >= 6) {
        TracerVisual tv;
        tv.shooterUid = std::atoi(m[1].c_str());
        parseVec3Csv(m[2], tv.pos);
        parseVec3Csv(m[3], tv.vel);
        tv.color = tracerColor(std::atoi(m[4].c_str()));
        tv.projectileId = std::atoi(m[5].c_str());
        float speed = Vector3Length(tv.vel);
        if (speed > 0.01f) tv.vel = Vector3Scale(tv.vel, 1.0f / speed);
        tracers_.push_back(tv);
        if (tracers_.size() > 96) tracers_.erase(tracers_.begin(), tracers_.begin() + (tracers_.size() - 96));
    } else if (m[0] == proto::kU_Impact && m.size() >= 5) {
        ImpactVisual iv;
        parseVec3Csv(m[2], iv.pos);
        iv.surfaceKind = std::atoi(m[3].c_str());
        iv.victimUid = std::atoi(m[4].c_str());
        impacts_.push_back(iv);
        if (iv.victimUid != 0) hitMarkerTimer_ = 0.18f;
        else missMarkerTimer_ = 0.16f;
        if (impacts_.size() > 96) impacts_.erase(impacts_.begin(), impacts_.begin() + (impacts_.size() - 96));
        // Spawn directional particles: red blood for body/head, yellow sparks for wall,
        // tan dust for ground. ~8 per impact.
        int n = 8;
        Color pc = (iv.surfaceKind == 1 || iv.surfaceKind == 2) ? Color{ 200, 30, 30, 255 }
                 : (iv.surfaceKind == 3)                         ? Color{ 190, 170, 130, 255 }
                 :                                                 Color{ 255, 200, 80, 255 };
        for (int k = 0; k < n; ++k) {
            ParticleVisual pv;
            pv.pos = iv.pos;
            float th = (float)k / (float)n * 6.2832f;
            float ph = ((float)(k * 7 % 9) - 4.0f) * 0.25f;
            float sp = 2.5f + (k % 3) * 0.6f;
            pv.vel = { std::cos(th)*sp, std::sin(ph)*sp + 1.5f, std::sin(th)*sp };
            pv.color = pc;
            pv.life = 0.35f + (k % 3) * 0.05f;
            pv.size = (iv.surfaceKind == 2) ? 0.07f : 0.04f;  // bigger for headshot
            particles_.push_back(pv);
        }
        if ((int)particles_.size() > 400) particles_.erase(particles_.begin(), particles_.begin() + (particles_.size() - 400));
    } else if (m[0] == proto::kU_Flash && m.size() >= 4) {
        int victimUid = std::atoi(m[1].c_str());
        if (victimUid == net_.userId) {
            float intensity = (float)std::atof(m[2].c_str());
            float dur = (float)std::atof(m[3].c_str());
            damageVignetteTimer_ = std::max(damageVignetteTimer_, dur);
            // Burn the screen white. Decays inside updateEffects().
            flashWhiteoutAlpha_ = std::max(flashWhiteoutAlpha_, std::min(1.0f, intensity));
        }
    } else if (m[0] == proto::kU_Explode && m.size() >= 3) {
        int typeId = std::atoi(m[1].c_str());
        Vector3 pos; parseVec3Csv(m[2], pos);
        const Throwable* t = throwables::lookup(typeId);
        if (t) {
            ExplosionVisual e;
            e.pos = pos;
            e.radius = t->radiusM;
            // Smoke grenades (very low damage) get a long-lived gray cloud instead of a fireball.
            bool isSmoke = (t->kind == ThrowableKind::Grenade && t->maxDamage < 20.0f);
            if (isSmoke) {
                SmokeVisual s;
                s.pos = pos; s.radius = t->radiusM; s.life = 6.0f;
                smokes_.push_back(s);
            } else {
                e.life = (t->kind == ThrowableKind::Flashbang) ? 0.45f : 1.4f;
                e.tint = (t->kind == ThrowableKind::Flashbang) ? Color{ 255, 255, 220, 255 }
                       : (t->kind == ThrowableKind::Bomb)      ? Color{ 255, 110, 40, 255 }
                       :                                         Color{ 255, 160, 60, 255 };
                explosions_.push_back(e);
                // Throw 18 sparks/debris radially.
                for (int k = 0; k < 18; ++k) {
                    ParticleVisual pv;
                    pv.pos = pos;
                    float th = (float)k / 18.0f * 6.2832f;
                    float ph = ((float)(k * 37 % 11) - 5.0f) * 0.18f;
                    float sp = 4.0f + (k % 5) * 0.6f;
                    pv.vel = { std::cos(th)*sp, std::sin(ph)*sp + 2.5f, std::sin(th)*sp };
                    pv.color = e.tint;
                    pv.life = 0.7f + (k % 4) * 0.1f;
                    pv.size = 0.06f;
                    particles_.push_back(pv);
                }
            }
        }
        if ((int)explosions_.size() > 32) explosions_.erase(explosions_.begin(), explosions_.begin() + (explosions_.size() - 32));
        if ((int)smokes_.size() > 16)     smokes_.erase(smokes_.begin(),         smokes_.begin()     + (smokes_.size()     - 16));
        if ((int)particles_.size() > 400) particles_.erase(particles_.begin(),   particles_.begin() + (particles_.size()  - 400));
    }
}
void Game::sendInput(bool fire) {
    float mx = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
    float mz = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
    if (fire) {
        muzzleFlashTimer_ = 0.06f;
        // Per-weapon recoil kick — scaled down to a small visual pitch jitter.
        const Weapon* w = weapons::lookup(net_.selectedWeapon);
        float kick = w ? w->recoilKick : 2.0f;
        recoilKick_ += kick * 0.0035f;
        if (recoilKick_ > 0.25f) recoilKick_ = 0.25f;
    }
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
    yaw_   -= md.x * 0.0025f;
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
    // Apply recoil kick as a momentary pitch-up (recoilKick_ added; decayed in updateEffects).
    float effPitch = pitch_ - recoilKick_;
    if (effPitch < -1.45f) effPitch = -1.45f;
    Vector3 fwd = {
        std::cos(effPitch) * std::sin(yaw_),
        -std::sin(effPitch),
        std::cos(effPitch) * std::cos(yaw_)
    };
    camera_.target = Vector3Add(camera_.position, fwd);
    camera_.up = { 0.0f, 1.0f, 0.0f };
}
void Game::updateEffects(float dt) {
    for (auto& t : tracers_) {
        t.age += dt;
        t.pos = Vector3Add(t.pos, Vector3Scale(t.vel, dt * 90.0f));
    }
    tracers_.erase(std::remove_if(tracers_.begin(), tracers_.end(),
                    [](const TracerVisual& t) { return t.age >= t.life; }), tracers_.end());
    for (auto& i : impacts_) i.age += dt;
    impacts_.erase(std::remove_if(impacts_.begin(), impacts_.end(),
                    [](const ImpactVisual& i) { return i.age >= i.life; }), impacts_.end());
    // Particles (blood / sparks / dust / explosion debris): ballistic step + drag.
    for (auto& p : particles_) {
        p.age += dt;
        p.vel.y -= 9.81f * dt;
        p.vel.x *= (1.0f - 1.2f * dt);
        p.vel.z *= (1.0f - 1.2f * dt);
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                    [](const ParticleVisual& p) { return p.age >= p.life || p.pos.y < -0.5f; }), particles_.end());
    for (auto& e : explosions_) e.age += dt;
    explosions_.erase(std::remove_if(explosions_.begin(), explosions_.end(),
                    [](const ExplosionVisual& e) { return e.age >= e.life; }), explosions_.end());
    for (auto& s : smokes_) s.age += dt;
    smokes_.erase(std::remove_if(smokes_.begin(), smokes_.end(),
                    [](const SmokeVisual& s) { return s.age >= s.life; }), smokes_.end());
    hitMarkerTimer_ = std::max(0.0f, hitMarkerTimer_ - dt);
    missMarkerTimer_ = std::max(0.0f, missMarkerTimer_ - dt);
    damageVignetteTimer_ = std::max(0.0f, damageVignetteTimer_ - dt);
    muzzleFlashTimer_ = std::max(0.0f, muzzleFlashTimer_ - dt);
    recoilKick_ = std::max(0.0f, recoilKick_ - dt * 1.2f);  // decay ~0.2s
    flashWhiteoutAlpha_ = std::max(0.0f, flashWhiteoutAlpha_ - dt * 0.55f);
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
        Color cover = o.size.y > 2.5f ? Color{ 95, 88, 78, 255 } : Color{ 115, 95, 65, 255 };
        DrawCube(o.center, o.size.x, o.size.y, o.size.z, cover);
        DrawCubeWires(o.center, o.size.x, o.size.y, o.size.z, BLACK);
        if (o.size.y <= 1.3f) {
            DrawCube({ o.center.x, o.center.y + o.size.y * 0.5f + 0.08f, o.center.z },
                     o.size.x * 0.85f, 0.16f, o.size.z * 0.85f, { 75, 85, 65, 255 });
        }
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
    for (const auto& t : tracers_) {
        float a = 1.0f - (t.age / std::max(0.01f, t.life));
        Vector3 tail = Vector3Subtract(t.pos, Vector3Scale(t.vel, 2.0f));
        DrawCylinderEx(tail, t.pos, 0.025f, 0.01f, 8, Fade(t.color, a));
    }
    for (const auto& i : impacts_) {
        float a = 1.0f - (i.age / std::max(0.01f, i.life));
        Color c = i.victimUid != 0 ? Color{ 255, 45, 45, 255 } :
                  (i.surfaceKind == 3 ? Color{ 190, 170, 130, 255 } : Color{ 255, 210, 80, 255 });
        DrawSphere(i.pos, 0.08f + i.age * 0.45f, Fade(c, a));
    }
    // Particles (blood / sparks / dust / debris) — small cubes facing roughly the camera.
    for (const auto& p : particles_) {
        float a = 1.0f - (p.age / std::max(0.01f, p.life));
        DrawCube(p.pos, p.size, p.size, p.size, Fade(p.color, a));
    }
    // Explosions — bright expanding sphere + dim outer shell.
    for (const auto& e : explosions_) {
        float t = e.age / std::max(0.01f, e.life);
        float r = e.radius * (0.35f + t * 0.85f);
        float a = (1.0f - t) * 0.65f;
        DrawSphere(e.pos, r * 0.30f, Fade(e.tint, a));
        DrawSphereWires(e.pos, r, 10, 12, Fade(e.tint, a * 0.45f));
    }
    // Smoke clouds — multiple stacked translucent spheres that drift up.
    for (const auto& s : smokes_) {
        float t = s.age / std::max(0.01f, s.life);
        float baseA = (t < 0.15f) ? (t / 0.15f) * 0.55f : (1.0f - t) * 0.55f;
        for (int k = 0; k < 5; ++k) {
            Vector3 c = s.pos;
            c.y += 0.4f + k * 0.5f + t * 1.2f;
            c.x += std::sin(s.age * 0.7f + k) * 0.4f;
            c.z += std::cos(s.age * 0.6f + k * 1.3f) * 0.4f;
            float r = s.radius * (0.35f + k * 0.10f) * (0.5f + t * 0.6f);
            DrawSphere(c, r, Fade(Color{ 110, 110, 115, 255 }, baseA));
        }
    }
    // ---- First-person weapon viewmodel ----
    // Per-weapon shape derived from caliber + price. Drawn slightly below + ahead of camera,
    // following a damped right-hand offset so the gun "swings" naturally with look.
    {
        const Weapon* w = weapons::lookup(net_.selectedWeapon);
        int caliber = w ? w->caliberMm : 9;
        Vector3 fwd = Vector3Normalize(Vector3Subtract(camera_.target, camera_.position));
        Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera_.up));
        Vector3 down  = Vector3Negate(camera_.up);
        Vector3 base  = Vector3Add(camera_.position, Vector3Scale(fwd, 0.55f));
        base = Vector3Add(base, Vector3Scale(right, 0.18f));
        base = Vector3Add(base, Vector3Scale(down, 0.30f));
        Color metal = { 40, 42, 48, 255 };
        Color stock = { 65, 50, 35, 255 };
        Color mag   = { 28, 28, 32, 255 };
        if (caliber >= 127) {            // 12.7 mm — very long with big scope + bipod
            DrawCubeV(base, { 0.14f, 0.14f, 1.30f }, metal);
            DrawCubeV(Vector3Add(base, Vector3Scale(fwd, 0.10f)), { 0.10f, 0.18f, 0.45f }, metal);
            DrawSphere(Vector3Add(base, Vector3Add(Vector3Scale(camera_.up, 0.16f), Vector3Scale(fwd, 0.05f))), 0.07f, { 25, 25, 30, 255 });
        } else if (caliber == 762) {     // 7.62 mm — AK-shape: receiver + curved mag
            DrawCubeV(base, { 0.13f, 0.13f, 0.95f }, metal);
            DrawCubeV(Vector3Add(base, Vector3Scale(camera_.up, -0.15f)), { 0.10f, 0.18f, 0.14f }, mag);
            DrawCubeV(Vector3Add(base, Vector3Scale(fwd, -0.30f)), { 0.12f, 0.14f, 0.30f }, stock);
        } else if (caliber == 556) {     // 5.56 mm — AR-15 / M4-ish
            DrawCubeV(base, { 0.11f, 0.11f, 0.85f }, metal);
            DrawCubeV(Vector3Add(base, Vector3Scale(camera_.up, -0.12f)), { 0.08f, 0.16f, 0.10f }, mag);
            DrawCubeV(Vector3Add(base, Vector3Scale(fwd, -0.28f)), { 0.10f, 0.10f, 0.22f }, stock);
        } else if (caliber == 12) {      // shotgun — wide, no mag, longer barrel
            DrawCubeV(base, { 0.16f, 0.13f, 0.95f }, metal);
            DrawCubeV(Vector3Add(base, Vector3Scale(fwd, -0.32f)), { 0.13f, 0.13f, 0.28f }, stock);
        } else {                         // 9 mm — short pistol / SMG
            DrawCubeV(base, { 0.10f, 0.10f, (caliber == 9 && net_.selectedWeapon >= 18) ? 0.55f : 0.30f }, metal);
            DrawCubeV(Vector3Add(base, Vector3Scale(camera_.up, -0.10f)), { 0.08f, 0.14f, 0.10f }, mag);
        }
        // Muzzle flash — quick bright billboard at barrel tip.
        if (muzzleFlashTimer_ > 0.0f) {
            float fl = muzzleFlashTimer_ / 0.06f;
            Vector3 tip = Vector3Add(base, Vector3Scale(fwd, 0.55f));
            DrawSphere(tip, 0.10f + (1.0f - fl) * 0.05f, Fade(Color{ 255, 230, 110, 255 }, fl));
            DrawSphere(tip, 0.20f + (1.0f - fl) * 0.10f, Fade(Color{ 255, 170,  40, 255 }, fl * 0.5f));
        }
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
    if (hitMarkerTimer_ > 0.0f) {
        DrawLine(cx - 18, cy - 18, cx - 8, cy - 8, RED);
        DrawLine(cx + 18, cy - 18, cx + 8, cy - 8, RED);
        DrawLine(cx - 18, cy + 18, cx - 8, cy + 8, RED);
        DrawLine(cx + 18, cy + 18, cx + 8, cy + 8, RED);
    } else if (missMarkerTimer_ > 0.0f) {
        DrawCircleLines(cx, cy, 18.0f, Fade(LIGHTGRAY, 0.8f));
    }
    if (damageVignetteTimer_ > 0.0f) {
        float a = std::min(0.45f, damageVignetteTimer_ * 1.2f);
        DrawRectangle(0, 0, sw, sh, Fade(RED, a));
    }
    // Flashbang whiteout — drawn LAST so it covers everything.
    if (flashWhiteoutAlpha_ > 0.0f) {
        DrawRectangle(0, 0, sw, sh, Fade(WHITE, std::min(1.0f, flashWhiteoutAlpha_)));
    }
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
        if (IsKeyPressed(KEY_G) || IsKeyPressed(KEY_F) || IsKeyPressed(KEY_B)) {
            int typeId = IsKeyPressed(KEY_G) ? 100 : (IsKeyPressed(KEY_F) ? 200 : 300);
            Vector3 fwd = Vector3Subtract(camera_.target, camera_.position);
            float fl = Vector3Length(fwd);
            if (fl > 0.01f) fwd = Vector3Scale(fwd, 1.0f / fl);
            Vector3 origin = Vector3Add(camera_.position, Vector3Scale(fwd, 0.5f));
            auto v3 = [](Vector3 v) {
                char b[64]; std::snprintf(b, sizeof(b), "%.3f,%.3f,%.3f", v.x, v.y, v.z); return std::string(b);
            };
            net_.sendTcp({ proto::kT_Throw, std::to_string(typeId), v3(origin), v3(fwd), "18" });
        }
        updateLocalCamera(dt);
        updateEffects(dt);
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
