#include "Match.h"
#include "Protocol.h"
#include "Maps.h"
#include "Throwable.h"
#include "Weapon.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {
inline float frand01() { return (float)std::rand() / (float)RAND_MAX; }
inline float frand(float a, float b) { return a + (b - a) * frand01(); }
inline float frandSigned() { return frand01() * 2.0f - 1.0f; }
Vec3 vadd(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec3 vsub(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec3 vscale(const Vec3& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
float vdot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float vlen(const Vec3& a) { return std::sqrt(vdot(a, a)); }
Vec3 vnorm(const Vec3& a) {
    float l = vlen(a);
    if (l < 1e-6f) return { 0.0f, 0.0f, 1.0f };
    return vscale(a, 1.0f / l);
}
Vec3 applySpread(const Vec3& dir, float spreadDeg) {
    if (spreadDeg <= 0.001f) return vnorm(dir);
    float yaw = frandSigned() * spreadDeg * 0.0174532925f;
    float pitch = frandSigned() * spreadDeg * 0.0174532925f;
    Vec3 right = vnorm({ dir.z, 0.0f, -dir.x });
    Vec3 up = vnorm({ right.y * dir.z - right.z * dir.y,
                      right.z * dir.x - right.x * dir.z,
                      right.x * dir.y - right.y * dir.x });
    return vnorm(vadd(dir, vadd(vscale(right, yaw), vscale(up, pitch))));
}
float bulletAreaM2(int caliberMm) {
    float diameterM = (float)caliberMm / 1000.0f;
    if (caliberMm == 762) diameterM = 0.00762f;
    else if (caliberMm == 556) diameterM = 0.00556f;
    else if (caliberMm == 127) diameterM = 0.0127f;
    else if (caliberMm == 9) diameterM = 0.009f;
    else if (caliberMm == 12) diameterM = 0.0185f;
    float r = diameterM * 0.5f;
    return 3.14159265f * r * r;
}

// ----------------------- Ballistics constants -----------------------
constexpr float kRhoAir = 1.225f;          // kg/m^3 at sea level, ISA.
constexpr float kGravity = 9.81f;          // m/s^2.
constexpr float kProjectileLifeMaxSec = 3.0f;
constexpr int   kProjectileCapPerMatch = 200;
constexpr float kPlayerBodyRadius = 0.5f;  // ~1 m wide cylinder.
constexpr float kPlayerHeadHeightLo = 1.5f; // anything between head-lo..head-hi is a headshot.
constexpr float kPlayerHeadHeightHi = 1.85f;
constexpr float kPlayerBodyHeightHi = 1.7f; // capsule total height ≈ 1.85 m.
constexpr int   kSurfaceWall = 0;
constexpr int   kSurfaceBody = 1;
constexpr int   kSurfaceHead = 2;
constexpr int   kSurfaceGround = 3;
[[maybe_unused]] constexpr int kSurfaceRicochet = 4;

// ----------------------- Throwable catalog (local fallback) -----------------------
// NOTE / TODO(weapon-data-agent): When `include/Throwable.h` lands, replace this
// with `throwables::lookup(typeId)`. The Throwable.h catalog should expose:
//   - int  id
//   - const char* name (we use it to detect "Claymore" / "Smoke")
//   - float fuseSec, float radius, float maxDamage, float falloffPow
//   - kind: enum { Grenade, Flashbang, Bomb, Smoke, Claymore }
// Until then we mirror the contract here so this server compiles standalone.
enum ThrowKind { TK_Grenade = 0, TK_Flashbang = 1, TK_Bomb = 2, TK_Smoke = 3, TK_Claymore = 4 };
struct LocalThrowable {
    int id;
    const char* name;
    ThrowKind kind;
    float fuseSec;
    float radius;
    float maxDamage;
    float falloffPow;
    float flashDurationSec;  // only used for flashbangs
};
constexpr LocalThrowable kThrowables[] = {
    // id, name,         kind,         fuse,  radius, maxDmg, falloffPow, flashDur
    {  1, "Frag Grenade", TK_Grenade,   3.0f,   6.0f,  120.0f, 1.5f,  0.0f },
    {  2, "Flashbang",    TK_Flashbang, 1.8f,   8.0f,    0.0f, 1.0f,  3.5f },
    {  3, "Smoke",        TK_Smoke,     2.0f,   3.0f,    0.0f, 1.0f,  0.0f },
    {  4, "C4",           TK_Bomb,      4.0f,  10.0f,  650.0f, 1.8f,  0.0f },
    {  5, "Claymore",     TK_Claymore,  0.0f,   3.0f,  300.0f, 1.2f,  0.0f },
};
constexpr int kNumThrowables = sizeof(kThrowables) / sizeof(kThrowables[0]);
const Throwable* lookupThrowable(int typeId) {
    return throwables::lookup(typeId);
}
bool isSmokeName(const std::string& name) {
    return name.find("Smoke") != std::string::npos;
}
bool isClaymoreName(const std::string& name) {
    return name.find("Claymore") != std::string::npos;
}

// ----------------------- "Color ID" for tracers -----------------------
// Derived from caliber so the client can pick a tint. Mapping is intentionally
// simple (it's just a u8 hint).
int tracerColorForWeapon(const Weapon* w) {
    if (!w) return 0;
    if (w->caliberMm >= 100) return 3;  // .50/12.7-class, orange
    if (w->caliberMm >= 700) return 2;  // 7.62-class, red
    if (w->caliberMm >= 500) return 1;  // 5.56-class, yellow
    return 0;                            // 9 mm / pistol, white
}
void loadObstaclesFromShared(const std::string& mapName, std::vector<AABB>& out) {
    std::vector<MapAABB> raw;
    buildMap(mapName, raw);
    out.clear();
    out.reserve(raw.size());
    for (const auto& r : raw) {
        AABB a;
        a.center = { r.cx, r.cy, r.cz };
        a.size   = { r.sx, r.sy, r.sz };
        out.push_back(a);
    }
}
[[maybe_unused]] void buildArena(std::vector<AABB>& out) {
    out = {
        { {  6.0f, 1.0f,  4.0f}, {2.0f, 2.0f, 2.0f} },
        { { -6.0f, 1.0f, -4.0f}, {2.0f, 2.0f, 2.0f} },
        { { 10.0f, 1.5f, -8.0f}, {3.0f, 3.0f, 1.5f} },
        { {-10.0f, 1.5f,  8.0f}, {3.0f, 3.0f, 1.5f} },
        { {  0.0f, 1.0f, 12.0f}, {4.0f, 2.0f, 1.0f} },
        { {  0.0f, 1.0f,-12.0f}, {4.0f, 2.0f, 1.0f} },
        { { 14.0f, 1.0f,  2.0f}, {1.0f, 2.0f, 5.0f} },
        { {-14.0f, 1.0f, -2.0f}, {1.0f, 2.0f, 5.0f} },
    };
}
// Segment vs vertical cylinder (radius r, axis on y from yLo..yHi).
// Returns t in [0,1] along seg if it pierces the cylinder body, with hitY = world y.
bool segmentVsCylinder(const Vec3& a, const Vec3& b,
                       float cx, float cz, float yLo, float yHi, float r,
                       float& tOut, Vec3& hitOut) {
    float dx = b.x - a.x;
    float dz = b.z - a.z;
    float ex = a.x - cx;
    float ez = a.z - cz;
    float A = dx * dx + dz * dz;
    float B = 2.0f * (ex * dx + ez * dz);
    float C = ex * ex + ez * ez - r * r;
    if (A < 1e-12f) {
        // Effectively vertical: check if endpoint XZ is inside, and segment Y spans cylinder.
        if (C > 0.0f) return false;
        float yMin = std::min(a.y, b.y);
        float yMax = std::max(a.y, b.y);
        if (yMax < yLo || yMin > yHi) return false;
        tOut = 0.0f;
        hitOut = a;
        return true;
    }
    float disc = B * B - 4.0f * A * C;
    if (disc < 0.0f) return false;
    float sq = std::sqrt(disc);
    float t0 = (-B - sq) / (2.0f * A);
    float t1 = (-B + sq) / (2.0f * A);
    float t = (t0 >= 0.0f) ? t0 : t1;
    if (t < 0.0f || t > 1.0f) return false;
    float y = a.y + (b.y - a.y) * t;
    if (y < yLo || y > yHi) return false;
    tOut = t;
    hitOut = { a.x + dx * t, y, a.z + dz * t };
    return true;
}
// Segment vs sphere centered at (cx,cy,cz) with radius r.
bool segmentVsSphere(const Vec3& a, const Vec3& b,
                     float cx, float cy, float cz, float r,
                     float& tOut, Vec3& hitOut) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    float ex = a.x - cx, ey = a.y - cy, ez = a.z - cz;
    float A = dx*dx + dy*dy + dz*dz;
    float B = 2.0f * (ex*dx + ey*dy + ez*dz);
    float C = ex*ex + ey*ey + ez*ez - r*r;
    if (A < 1e-12f) return false;
    float disc = B*B - 4.0f*A*C;
    if (disc < 0.0f) return false;
    float sq = std::sqrt(disc);
    float t0 = (-B - sq) / (2.0f * A);
    float t1 = (-B + sq) / (2.0f * A);
    float t = (t0 >= 0.0f) ? t0 : t1;
    if (t < 0.0f || t > 1.0f) return false;
    tOut = t;
    hitOut = { a.x + dx*t, a.y + dy*t, a.z + dz*t };
    return true;
}
// Segment vs AABB (returns nearest non-negative t in [0,1]).
bool segmentVsAabb(const Vec3& a, const Vec3& b, const AABB& box, float& tOut) {
    float minx = box.center.x - box.size.x * 0.5f;
    float maxx = box.center.x + box.size.x * 0.5f;
    float miny = box.center.y - box.size.y * 0.5f;
    float maxy = box.center.y + box.size.y * 0.5f;
    float minz = box.center.z - box.size.z * 0.5f;
    float maxz = box.center.z + box.size.z * 0.5f;
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    float tmin = 0.0f, tmax = 1.0f;
    auto axis = [&](float o, float d, float lo, float hi) -> bool {
        if (std::fabs(d) < 1e-8f) {
            return (o >= lo && o <= hi);
        }
        float t1 = (lo - o) / d;
        float t2 = (hi - o) / d;
        if (t1 > t2) std::swap(t1, t2);
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        return tmin <= tmax;
    };
    if (!axis(a.x, dx, minx, maxx)) return false;
    if (!axis(a.y, dy, miny, maxy)) return false;
    if (!axis(a.z, dz, minz, maxz)) return false;
    if (tmin < 0.0f || tmin > 1.0f) return false;
    tOut = tmin;
    return true;
}
bool rayBox(const Vec3& orig, const Vec3& dir, const AABB& box, float& tHit) {
    float minx = box.center.x - box.size.x * 0.5f;
    float maxx = box.center.x + box.size.x * 0.5f;
    float miny = box.center.y - box.size.y * 0.5f;
    float maxy = box.center.y + box.size.y * 0.5f;
    float minz = box.center.z - box.size.z * 0.5f;
    float maxz = box.center.z + box.size.z * 0.5f;
    float tmin = -1e30f, tmax = 1e30f;
    auto axis = [&](float o, float d, float lo, float hi) -> bool {
        if (std::fabs(d) < 1e-8f) {
            if (o < lo || o > hi) return false;
            return true;
        }
        float t1 = (lo - o) / d;
        float t2 = (hi - o) / d;
        if (t1 > t2) std::swap(t1, t2);
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        return tmin <= tmax;
    };
    if (!axis(orig.x, dir.x, minx, maxx)) return false;
    if (!axis(orig.y, dir.y, miny, maxy)) return false;
    if (!axis(orig.z, dir.z, minz, maxz)) return false;
    if (tmax < 0.0f) return false;
    tHit = (tmin >= 0.0f) ? tmin : tmax;
    return true;
}
}
Match::Match(int id, const std::string& map, int teamSize)
    : id_(id), map_(map), teamSize_(teamSize) {
    loadMap();
}
void Match::loadMap() {
    MapInfo info = mapInfoFor(map_);
    arenaHalfX_     = info.halfX;
    arenaHalfZ_     = info.halfZ;
    blueSpawnUsesX_ = info.spawnUsesX;
    blueSpawnX_     = info.blueX;
    blueSpawnZ_     = info.blueZ;
    redSpawnX_      = info.redX;
    redSpawnZ_      = info.redZ;
    loadObstaclesFromShared(map_, obstacles_);
}
Vec3 Match::spawnPoint(const std::string& team) const {
    if (blueSpawnUsesX_) {
        float x = (team == "BLUE") ? blueSpawnX_ : redSpawnX_;
        float z = frand(-arenaHalfZ_ + 2.0f, arenaHalfZ_ - 2.0f);
        return { x, 0.0f, z };
    } else {
        float z = (team == "BLUE") ? blueSpawnZ_ : redSpawnZ_;
        float x = frand(-arenaHalfX_ + 2.0f, arenaHalfX_ - 2.0f);
        return { x, 0.0f, z };
    }
}
int Match::addPlayer(int clientId, int userId, const std::string& username, const std::string& team,
                     int weaponId) {
    Slot s;
    s.clientId = clientId;
    s.userId = userId;
    s.username = username;
    s.team = team;
    s.pos = spawnPoint(team);
    s.yaw = (team == "BLUE") ? 0.0f : 3.14159265f;
    s.pitch = 0.0f;
    s.hp = 100;
    const Weapon* w = weapons::lookup(weaponId);
    if (!w) w = weapons::lookup(proto::kWeaponPistol);
    s.weaponId = w->id;
    s.mag = w->magSize;
    s.reserve = w->reserve;
    s.alive = true;
    slots_.push_back(std::move(s));
    return (int)slots_.size() - 1;
}
int Match::slotIndexByUserId(int userId) const {
    for (size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].isBot && slots_[i].userId == userId) return (int)i;
    }
    return -1;
}
void Match::reattachSlot(int slotIndex, int newClientId) {
    if (slotIndex < 0 || slotIndex >= (int)slots_.size()) return;
    slots_[slotIndex].clientId = newClientId;
    slots_[slotIndex].active = true;
}
void Match::zeroInputAt(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= (int)slots_.size()) return;
    slots_[slotIndex].input = PlayerInput{};
}
int Match::addBot(const std::string& username, const std::string& team, int weaponId) {
    Slot s;
    s.clientId = -1;
    s.userId = 0;
    s.username = username;
    s.team = team;
    s.pos = spawnPoint(team);
    s.yaw = (team == "BLUE") ? 0.0f : 3.14159265f;
    s.pitch = 0.0f;
    s.hp = 100;
    const Weapon* w = weapons::lookup(weaponId);
    if (!w) w = weapons::lookup(proto::kWeaponPistol);
    s.weaponId = w->id;
    s.mag = w->magSize;
    s.reserve = w->reserve;
    s.alive = true;
    s.isBot = true;
    slots_.push_back(std::move(s));
    return (int)slots_.size() - 1;
}
void Match::computeBotInputs(float dt) {
    for (auto& b : slots_) {
        if (!b.isBot || !b.active) continue;
        if (!b.alive) {
            b.input = PlayerInput{};
            continue;
        }
        int bestIdx = -1;
        float bestDist = 1e9f;
        for (size_t j = 0; j < slots_.size(); ++j) {
            auto& v = slots_[j];
            if (&v == &b) continue;
            if (!v.active || !v.alive) continue;
            if (v.team == b.team) continue;
            float dx = v.pos.x - b.pos.x, dz = v.pos.z - b.pos.z;
            float d2 = dx * dx + dz * dz;
            if (d2 < bestDist) { bestDist = d2; bestIdx = (int)j; }
        }
        PlayerInput in;
        bool seeTarget = false;
        if (bestIdx >= 0) {
            const Slot& tgt = slots_[bestIdx];
            Vec3 eyeA = { b.pos.x, b.pos.y + 1.6f, b.pos.z };
            Vec3 eyeB = { tgt.pos.x, tgt.pos.y + 1.0f, tgt.pos.z };
            seeTarget = !lineBlocked(eyeA, eyeB);
            if (seeTarget) {
                float dx = tgt.pos.x - b.pos.x;
                float dz = tgt.pos.z - b.pos.z;
                float dist = std::sqrt(dx*dx + dz*dz);
                b.bot.aimNoise += ((frand01() - 0.5f) - b.bot.aimNoise) * dt * 1.2f;
                float desired = std::atan2(-dx, -dz);
                in.yaw = desired + b.bot.aimNoise * 0.08f;
                float dy = (tgt.pos.y + 1.4f) - (b.pos.y + 1.6f);
                in.pitch = std::atan2(dy, std::max(0.1f, dist));
                float yawDiff = in.yaw - b.yaw;
                while (yawDiff > 3.14159f) yawDiff -= 6.28318f;
                while (yawDiff < -3.14159f) yawDiff += 6.28318f;
                if (std::fabs(yawDiff) < 0.18f) in.fire = 1;
                if (dist > 8.0f) { in.moveZ = 1.0f; in.moveX = 0.0f; }
                else if (dist < 3.0f) { in.moveZ = -1.0f; in.moveX = 0.0f; }
                else { in.moveX = (b.bot.roamTimer > 0.5f ? 1.0f : -1.0f); in.moveZ = 0.0f; }
            }
        }
        if (!seeTarget) {
            b.bot.roamTimer -= dt;
            if (b.bot.roamTimer <= 0.0f) {
                b.bot.roamTimer = 1.5f + frand01() * 1.5f;
                float ang = frand01() * 6.28318f;
                b.bot.roamDirX = std::cos(ang);
                b.bot.roamDirZ = std::sin(ang);
            }
            in.yaw = std::atan2(-b.bot.roamDirX, -b.bot.roamDirZ);
            in.pitch = 0.0f;
            in.moveX = 0.0f;
            in.moveZ = 0.6f;
            in.fire = 0;
        }
        in.tick = tick_;
        b.input = in;
        if (b.mag == 0 && b.reloadTimer <= 0.0f && b.reserve > 0) {
            const Weapon* w = weapons::lookup(b.weaponId);
            if (w) b.reloadTimer = w->reloadSec;
        }
    }
}
void Match::respawn(Slot& s) {
    s.pos = spawnPoint(s.team);
    s.hp = 100;
    const Weapon* w = weapons::lookup(s.weaponId);
    if (!w) w = weapons::lookup(proto::kWeaponPistol);
    s.mag = w->magSize;
    s.reserve = w->reserve;
    s.alive = true;
    s.respawnTimer = 0.0f;
    s.reloadTimer = 0.0f;
    s.fireCooldown = 0.0f;
}
bool Match::lineBlocked(const Vec3& a, const Vec3& b) const {
    Vec3 d = { b.x - a.x, b.y - a.y, b.z - a.z };
    float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (dist < 0.001f) return false;
    Vec3 dir = { d.x / dist, d.y / dist, d.z / dist };
    for (const auto& o : obstacles_) {
        float t;
        if (rayBox(a, dir, o, t) && t > 0.0f && t < dist) return true;
    }
    return false;
}
void Match::setInput(int clientId, const PlayerInput& in) {
    for (auto& s : slots_) {
        if (s.clientId == clientId) { s.input = in; return; }
    }
}
bool Match::forfeit(int clientId) {
    for (auto& s : slots_) {
        if (s.clientId == clientId) {
            s.active = false;
            s.alive = false;
            break;
        }
    }
    int blueAlive = 0, redAlive = 0;
    for (const auto& s : slots_) {
        if (!s.active) continue;
        if (s.team == "BLUE") ++blueAlive;
        else ++redAlive;
    }
    if (blueAlive == 0 || redAlive == 0) {
        ended_ = true;
        return true;
    }
    return false;
}
void Match::tick(float dt, std::vector<KillEvent>& killEvents) {
    std::vector<TracerEvent> tracers;
    std::vector<ImpactEvent> impacts;
    std::vector<ExplodeEvent> explodes;
    std::vector<FlashEvent> flashes;
    tick(dt, killEvents, tracers, impacts, explodes, flashes);
}
void Match::tick(float dt,
                 std::vector<KillEvent>& killEvents,
                 std::vector<TracerEvent>& tracers,
                 std::vector<ImpactEvent>& impacts,
                 std::vector<ExplodeEvent>& explodes,
                 std::vector<FlashEvent>& flashes) {
    if (ended_) return;
    ++tick_;
    const float speed = 6.0f;
    for (auto& s : slots_) {
        if (!s.active || !s.alive) continue;
        float mx = s.input.moveX, mz = s.input.moveZ;
        float len = std::sqrt(mx * mx + mz * mz);
        if (len > 1.0f) { mx /= len; mz /= len; }
        float fwdX = -std::sin(s.yaw);
        float fwdZ = -std::cos(s.yaw);
        float rgtX = -std::cos(s.yaw);
        float rgtZ =  std::sin(s.yaw);
        float vx = (fwdX * mz + rgtX * mx) * speed;
        float vz = (fwdZ * mz + rgtZ * mx) * speed;
        s.pos.x += vx * dt;
        s.pos.z += vz * dt;
        float bx = arenaHalfX_ - 0.5f;
        float bz = arenaHalfZ_ - 0.5f;
        if (s.pos.x < -bx) s.pos.x = -bx;
        if (s.pos.x >  bx) s.pos.x =  bx;
        if (s.pos.z < -bz) s.pos.z = -bz;
        if (s.pos.z >  bz) s.pos.z =  bz;
        s.yaw = s.input.yaw;
        s.pitch = s.input.pitch;
    }
    for (auto& s : slots_) {
        if (!s.active) continue;
        if (s.fireCooldown > 0.0f) {
            s.fireCooldown -= dt;
            if (s.fireCooldown < 0.0f) s.fireCooldown = 0.0f;
        }
        if (s.reloadTimer > 0.0f) {
            s.reloadTimer -= dt;
            if (s.reloadTimer <= 0.0f) {
                s.reloadTimer = 0.0f;
                const Weapon* w = weapons::lookup(s.weaponId);
                if (!w) w = weapons::lookup(proto::kWeaponPistol);
                int need = w->magSize - s.mag;
                int take = std::min(need, s.reserve);
                s.mag += take;
                s.reserve -= take;
            }
        }
        if (!s.alive) {
            s.respawnTimer -= dt;
            if (s.respawnTimer <= 0.0f) respawn(s);
        }
    }
    for (size_t i = 0; i < slots_.size(); ++i) {
        auto& sh = slots_[i];
        if (!sh.active || !sh.alive) continue;
        if (sh.input.fire == 0) continue;
        if (sh.fireCooldown > 0.0f) continue;
        if (sh.reloadTimer > 0.0f) continue;
        if (sh.mag <= 0) continue;
        const Weapon* wh = weapons::lookup(sh.weaponId);
        if (!wh) wh = weapons::lookup(proto::kWeaponPistol);
        sh.mag -= 1;
        sh.fireCooldown = wh->cooldownSec;
        Vec3 eye = { sh.pos.x, sh.pos.y + 1.6f, sh.pos.z };
        Vec3 dir = {
            -std::sin(sh.yaw) * std::cos(sh.pitch),
             std::sin(sh.pitch),
            -std::cos(sh.yaw) * std::cos(sh.pitch),
        };
        spawnProjectile(sh, eye, dir, tracers);
    }
    stepProjectiles(dt, killEvents, impacts);
    stepThrowables(dt, killEvents, explodes, flashes);
    timeLeft_ -= dt;
    if (timeLeft_ <= 0.0f || blueScore_ >= 30 || redScore_ >= 30) {
        timeLeft_ = std::max(0.0f, timeLeft_);
        ended_ = true;
    }
}
std::string Match::buildStateFor(const Slot& self) const {
    int timeMs = (int)(timeLeft_ * 1000.0f);
    Vec3 selfEye = { self.pos.x, self.pos.y + 1.6f, self.pos.z };
    std::string plist;
    int visibleCount = 0;
    for (size_t i = 0; i < slots_.size(); ++i) {
        const auto& s = slots_[i];
        bool isSelf = (&s == &self);
        bool include = true;
        if (!isSelf && s.alive && s.team != self.team) {
            Vec3 head = { s.pos.x, s.pos.y + 1.7f, s.pos.z };
            Vec3 chest = { s.pos.x, s.pos.y + 1.0f, s.pos.z };
            bool seeHead  = !lineBlocked(selfEye, head);
            bool seeChest = !lineBlocked(selfEye, chest);
            if (!seeHead && !seeChest) include = false;
        }
        if (!include) continue;
        if (visibleCount) plist.push_back(';');
        ++visibleCount;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%zu,%.2f,%.2f,%.2f,%.3f,%d,%s,%d,%d,",
                      i, s.pos.x, s.pos.y, s.pos.z, s.yaw, s.hp,
                      s.team.c_str(), s.alive ? 1 : 0, s.userId);
        plist += buf;
        plist += proto::urlEncode(s.username);
    }
    int count = visibleCount;
    std::vector<std::string> fields = {
        proto::kU_State,
        std::to_string(tick_),
        std::to_string(self.hp),
        std::to_string(self.mag),
        std::to_string(self.reserve),
        std::to_string(self.kills),
        std::to_string(self.deaths),
        std::to_string(self.headshots),
        std::to_string(self.score),
        std::to_string(blueScore_),
        std::to_string(redScore_),
        std::to_string(timeMs),
        std::to_string(self.weaponId),
        std::to_string(count),
        plist,
    };
    return proto::encodeLine(fields);
}
void Match::spawnProjectile(Slot& shooter, const Vec3& eye, const Vec3& aimDir,
                            std::vector<TracerEvent>& tracers) {
    const Weapon* w = weapons::lookup(shooter.weaponId);
    if (!w) w = weapons::lookup(proto::kWeaponPistol);
    if (!w) return;
    if ((int)projectiles_.size() >= kProjectileCapPerMatch) {
        projectiles_.erase(projectiles_.begin());
    }
    Vec3 dir = applySpread(vnorm(aimDir), w->spreadDeg);
    Projectile p;
    p.id = nextProjectileId_++;
    p.shooterCid = shooter.clientId;
    p.shooterUid = shooter.userId;
    p.weaponId = w->id;
    p.pos = eye;
    p.vel = vscale(dir, w->muzzleVelocityMps);
    p.muzzleSpeed = w->muzzleVelocityMps;
    p.mass_kg = std::max(0.001f, w->bulletMassG * 0.001f);
    p.dragCoef = w->dragCoefficient;
    p.crossArea_m2 = bulletAreaM2(w->caliberMm);
    p.colorId = tracerColorForWeapon(w);
    projectiles_.push_back(p);
    TracerEvent tv;
    tv.projectileId = p.id;
    tv.shooterUid = p.shooterUid;
    tv.origin = p.pos;
    tv.velocity = p.vel;
    tv.colorId = p.colorId;
    tracers.push_back(tv);
}
void Match::stepProjectiles(float dt,
                            std::vector<KillEvent>& killEvents,
                            std::vector<ImpactEvent>& impacts) {
    for (size_t i = 0; i < projectiles_.size();) {
        Projectile& p = projectiles_[i];
        Vec3 oldPos = p.pos;
        float speed = vlen(p.vel);
        if (speed > 0.01f) {
            float dragK = 0.5f * kRhoAir * p.dragCoef * p.crossArea_m2 / p.mass_kg;
            Vec3 drag = vscale(p.vel, -dragK * speed);
            p.vel.x += drag.x * dt;
            p.vel.y += (drag.y - kGravity) * dt;
            p.vel.z += drag.z * dt;
        } else {
            p.vel.y -= kGravity * dt;
        }
        p.pos = vadd(p.pos, vscale(p.vel, dt));
        p.lifeSec += dt;

        float bestT = 2.0f;
        int surface = -1;
        int victimIdx = -1;
        int victimUid = 0;
        Vec3 hitPos{};
        for (const auto& o : obstacles_) {
            float t;
            if (segmentVsAabb(oldPos, p.pos, o, t) && t < bestT) {
                bestT = t;
                surface = kSurfaceWall;
                hitPos = vadd(oldPos, vscale(vsub(p.pos, oldPos), t));
            }
        }
        if (p.pos.y <= 0.0f && oldPos.y > 0.0f) {
            float t = oldPos.y / std::max(0.001f, oldPos.y - p.pos.y);
            if (t < bestT) {
                bestT = t;
                surface = kSurfaceGround;
                hitPos = vadd(oldPos, vscale(vsub(p.pos, oldPos), t));
            }
        }
        for (size_t j = 0; j < slots_.size(); ++j) {
            Slot& v = slots_[j];
            if (!v.active || !v.alive || v.userId == p.shooterUid) continue;
            Slot* shooter = nullptr;
            for (auto& s : slots_) if (s.userId == p.shooterUid && s.clientId == p.shooterCid) { shooter = &s; break; }
            if (shooter && v.team == shooter->team) continue;
            float t;
            Vec3 hp;
            if (segmentVsSphere(oldPos, p.pos, v.pos.x, v.pos.y + 1.65f, v.pos.z, 0.32f, t, hp) && t < bestT) {
                bestT = t;
                surface = kSurfaceHead;
                victimIdx = (int)j;
                victimUid = v.userId;
                hitPos = hp;
            }
            if (segmentVsCylinder(oldPos, p.pos, v.pos.x, v.pos.z, v.pos.y, v.pos.y + kPlayerBodyHeightHi,
                                  kPlayerBodyRadius, t, hp) && t < bestT) {
                bestT = t;
                surface = kSurfaceBody;
                victimIdx = (int)j;
                victimUid = v.userId;
                hitPos = hp;
            }
        }
        bool remove = false;
        if (surface >= 0) {
            ImpactEvent iv;
            iv.projectileId = p.id;
            iv.pos = hitPos;
            iv.surfaceKind = surface;
            iv.victimUid = victimUid;
            impacts.push_back(iv);
            if (victimIdx >= 0) {
                Slot& vic = slots_[(size_t)victimIdx];
                Slot* shooter = nullptr;
                for (auto& s : slots_) if (s.userId == p.shooterUid && s.clientId == p.shooterCid) { shooter = &s; break; }
                const Weapon* w = weapons::lookup(p.weaponId);
                if (!w) w = weapons::lookup(proto::kWeaponPistol);
                float retained = std::clamp((speed * speed) / std::max(1.0f, p.muzzleSpeed * p.muzzleSpeed), 0.45f, 1.0f);
                bool hs = surface == kSurfaceHead;
                int dmg = (int)std::round((hs ? w->damageHs : w->damageBody) * retained);
                vic.hp -= std::max(1, dmg);
                if (vic.hp <= 0 && shooter) {
                    vic.hp = 0;
                    vic.alive = false;
                    vic.deaths += 1;
                    vic.respawnTimer = 3.0f;
                    shooter->kills += 1;
                    shooter->score += 100 + (hs ? 50 : 0);
                    if (hs) {
                        shooter->headshots += 1;
                        shooter->consecutiveHs += 1;
                        if (shooter->consecutiveHs == 5 || shooter->consecutiveHs == 10) {
                            std::fprintf(stderr, "[antichea] HS streak user=%s streak=%d kills=%d\n",
                                         shooter->username.c_str(), shooter->consecutiveHs, shooter->kills);
                        }
                    } else {
                        shooter->consecutiveHs = 0;
                    }
                    if (shooter->team == "BLUE") blueScore_ += 1; else redScore_ += 1;
                    killEvents.push_back({ shooter->userId, vic.userId, hs ? 1 : 0 });
                }
            }
            remove = true;
        }
        if (p.lifeSec >= kProjectileLifeMaxSec || std::fabs(p.pos.x) > arenaHalfX_ + 10.0f ||
            std::fabs(p.pos.z) > arenaHalfZ_ + 10.0f || p.pos.y < -2.0f) {
            if (surface < 0) {
                impacts.push_back({ p.id, p.pos, kSurfaceGround, 0 });
            }
            remove = true;
        }
        if (remove) projectiles_.erase(projectiles_.begin() + (long)i);
        else ++i;
    }
}
bool Match::spawnThrowable(int clientId, int typeId, const Vec3& origin,
                           const Vec3& dir, float powerMps) {
    const Throwable* t = lookupThrowable(typeId);
    if (!t) return false;
    ActiveThrowable a;
    a.id = nextThrowableId_++;
    a.typeId = typeId;
    a.throwerCid = clientId;
    for (const auto& s : slots_) {
        if (s.clientId == clientId) {
            a.throwerUid = s.userId;
            a.throwerTeam = s.team;
            break;
        }
    }
    a.pos = origin;
    a.vel = vscale(vnorm(dir), std::clamp(powerMps, 5.0f, 30.0f));
    a.fuseRemaining = t->fuseSec;
    a.forward = vnorm(dir);
    a.isProximity = isClaymoreName(t->name);
    a.armDelaySec = a.isProximity ? 0.8f : 0.0f;
    throwables_.push_back(a);
    return true;
}
void Match::stepThrowables(float dt,
                           std::vector<KillEvent>& killEvents,
                           std::vector<ExplodeEvent>& explodes,
                           std::vector<FlashEvent>& flashes) {
    for (size_t i = 0; i < throwables_.size();) {
        ActiveThrowable& a = throwables_[i];
        const Throwable* t = lookupThrowable(a.typeId);
        if (!t) { throwables_.erase(throwables_.begin() + (long)i); continue; }
        a.vel.y -= kGravity * dt;
        a.vel = vscale(a.vel, std::max(0.0f, 1.0f - dt * 0.35f));
        a.pos = vadd(a.pos, vscale(a.vel, dt));
        if (a.pos.y < 0.05f) { a.pos.y = 0.05f; a.vel.y = -a.vel.y * 0.25f; }
        if (a.armDelaySec > 0.0f) {
            a.armDelaySec -= dt;
            if (a.armDelaySec <= 0.0f) a.armed = true;
        }
        bool detonate = false;
        if (a.isProximity && a.armed) {
            for (const auto& s : slots_) {
                if (!s.active || !s.alive || s.team == a.throwerTeam) continue;
                if (vlen(vsub(s.pos, a.pos)) <= t->radiusM) { detonate = true; break; }
            }
        } else {
            a.fuseRemaining -= dt;
            detonate = a.fuseRemaining <= 0.0f;
        }
        if (!detonate) { ++i; continue; }
        explodes.push_back({ a.typeId, a.pos, a.throwerUid });
        for (auto& s : slots_) {
            if (!s.active || !s.alive || s.team == a.throwerTeam) continue;
            float d = vlen(vsub(s.pos, a.pos));
            if (d > t->radiusM) continue;
            float falloff = std::pow(std::max(0.0f, 1.0f - d / t->radiusM), t->falloffPow);
            if (t->kind == ThrowableKind::Flashbang) {
                flashes.push_back({ s.userId, std::clamp(falloff, 0.0f, 1.0f), t->flashDurationSec * falloff });
            }
            int dmg = (int)std::round(t->maxDamage * falloff);
            if (dmg <= 0) continue;
            s.hp -= dmg;
            if (s.hp <= 0) {
                s.hp = 0;
                s.alive = false;
                s.deaths += 1;
                s.respawnTimer = 3.0f;
                for (auto& shooter : slots_) {
                    if (shooter.userId != a.throwerUid) continue;
                    shooter.kills += 1;
                    shooter.score += 100;
                    if (shooter.team == "BLUE") blueScore_ += 1; else redScore_ += 1;
                    killEvents.push_back({ shooter.userId, s.userId, 0 });
                    break;
                }
            }
        }
        throwables_.erase(throwables_.begin() + (long)i);
    }
}
MatchResult Match::resultFor(const Slot& s) const {
    MatchResult r;
    r.mapName = map_;
    r.team = s.team;
    if (s.team == "BLUE") {
        r.teamScore = blueScore_;
        r.enemyTeamScore = redScore_;
    } else {
        r.teamScore = redScore_;
        r.enemyTeamScore = blueScore_;
    }
    r.playerKills = s.kills;
    r.playerDeaths = s.deaths;
    r.playerScore = s.score;
    r.headshots = s.headshots;
    r.won = (r.teamScore > r.enemyTeamScore);
    return r;
}
