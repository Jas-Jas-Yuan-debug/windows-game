#include "Match.h"
#include "Protocol.h"
#include "Maps.h"
#include "Weapon.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace {
inline float frand01() { return (float)std::rand() / (float)RAND_MAX; }
inline float frand(float a, float b) { return a + (b - a) * frand01(); }
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
void buildDust(std::vector<AABB>& out) {
    out = {
        { {-10.0f, 1.5f, -10.0f}, {1.0f, 3.0f, 8.0f} },
        { {-10.0f, 1.5f,  10.0f}, {1.0f, 3.0f, 8.0f} },
        { { 10.0f, 1.5f, -10.0f}, {1.0f, 3.0f, 8.0f} },
        { { 10.0f, 1.5f,  10.0f}, {1.0f, 3.0f, 8.0f} },
        { {  0.0f, 1.5f,   0.0f}, {3.0f, 3.0f, 3.0f} },
        { {  5.0f, 1.0f,   3.0f}, {1.5f, 2.0f, 1.5f} },
        { { -5.0f, 1.0f,  -3.0f}, {1.5f, 2.0f, 1.5f} },
        { {  0.0f, 1.0f,   8.0f}, {2.5f, 2.0f, 1.0f} },
        { {  0.0f, 1.0f,  -8.0f}, {2.5f, 2.0f, 1.0f} },
        { {-15.0f, 1.0f, -18.0f}, {2.0f, 2.0f, 1.0f} },
        { { 15.0f, 1.0f,  18.0f}, {2.0f, 2.0f, 1.0f} },
        { {  8.0f, 1.0f, -16.0f}, {2.0f, 2.0f, 1.0f} },
    };
}
void buildOffice(std::vector<AABB>& out) {
    out = {
        { {  0.0f, 1.25f, 0.0f}, {1.0f, 2.5f, 10.0f} },
        { {-10.0f, 1.0f, -8.0f}, {3.0f, 2.0f, 1.0f} },
        { {-10.0f, 1.0f, -4.0f}, {3.0f, 2.0f, 1.0f} },
        { {-13.0f, 1.0f, -6.0f}, {1.0f, 2.0f, 3.0f} },
        { { 10.0f, 1.0f,  8.0f}, {3.0f, 2.0f, 1.0f} },
        { { 10.0f, 1.0f,  4.0f}, {3.0f, 2.0f, 1.0f} },
        { { 13.0f, 1.0f,  6.0f}, {1.0f, 2.0f, 3.0f} },
        { { -6.0f, 1.0f,  8.0f}, {1.0f, 2.0f, 4.0f} },
        { { -2.0f, 1.0f,  8.0f}, {1.0f, 2.0f, 4.0f} },
        { {  6.0f, 1.0f, -8.0f}, {1.0f, 2.0f, 4.0f} },
        { {  2.0f, 1.0f, -8.0f}, {1.0f, 2.0f, 4.0f} },
        { { -5.0f, 1.0f,  0.0f}, {1.5f, 2.0f, 1.5f} },
        { {  5.0f, 1.0f,  0.0f}, {1.5f, 2.0f, 1.5f} },
        { {-15.0f, 1.0f, 12.0f}, {2.0f, 2.0f, 2.0f} },
        { { 15.0f, 1.0f,-12.0f}, {2.0f, 2.0f, 2.0f} },
        { {  0.0f, 1.0f, 15.0f}, {3.0f, 2.0f, 1.0f} },
    };
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
        float rgtX =  std::cos(s.yaw);
        float rgtZ = -std::sin(s.yaw);
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
        float bestT = 1e9f;
        int bestIdx = -1;
        bool bestHs = false;
        for (size_t j = 0; j < slots_.size(); ++j) {
            if (j == i) continue;
            auto& v = slots_[j];
            if (!v.active || !v.alive) continue;
            if (v.team == sh.team) continue;
            AABB hb;
            hb.center = { v.pos.x, v.pos.y + 1.0f, v.pos.z };
            hb.size = { 1.0f, 2.0f, 1.0f };
            float t;
            if (!rayBox(eye, dir, hb, t)) continue;
            if (t <= 0.0f || t >= bestT) continue;
            Vec3 hitPt = { eye.x + dir.x * t, eye.y + dir.y * t, eye.z + dir.z * t };
            if (lineBlocked(eye, hitPt)) continue;
            bool hs = (hitPt.y >= v.pos.y + 1.3f && hitPt.y <= v.pos.y + 2.0f);
            bestT = t; bestIdx = (int)j; bestHs = hs;
        }
        if (bestIdx < 0) continue;
        auto& vic = slots_[bestIdx];
        int dmg = bestHs ? wh->damageHs : wh->damageBody;
        vic.hp -= dmg;
        if (vic.hp <= 0) {
            vic.hp = 0;
            vic.alive = false;
            vic.deaths += 1;
            vic.respawnTimer = 3.0f;
            sh.kills += 1;
            sh.score += 100 + (bestHs ? 50 : 0);
            if (bestHs) {
                sh.headshots += 1;
                sh.consecutiveHs += 1;
                if (sh.consecutiveHs == 5 || sh.consecutiveHs == 10) {
                    std::fprintf(stderr, "[antichea] HS streak user=%s streak=%d kills=%d\n",
                                 sh.username.c_str(), sh.consecutiveHs, sh.kills);
                }
            } else {
                sh.consecutiveHs = 0;
            }
            if (sh.team == "BLUE") blueScore_ += 1; else redScore_ += 1;
            KillEvent ev;
            ev.killerUid = sh.userId;
            ev.victimUid = vic.userId;
            ev.headshot = bestHs ? 1 : 0;
            killEvents.push_back(ev);
        }
    }
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
        plist += s.username;
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
