#pragma once
#include "Protocol.h"
#include "MatchResult.h"
#include <string>
#include <vector>
#include <cstdint>
struct Vec3 { float x = 0, y = 0, z = 0; };
struct AABB { Vec3 center; Vec3 size; };
struct PlayerInput {
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    int fire = 0;
    int tick = 0;
};
struct SlotBotState {
    float roamTimer = 0.0f;
    float roamDirX = 0.0f;
    float roamDirZ = 1.0f;
    float aimNoise = 0.0f;
};
struct Slot {
    int clientId = -1;
    int userId = 0;
    std::string username;
    std::string team;
    Vec3 pos;
    float yaw = 0.0f;
    float pitch = 0.0f;
    int hp = 100;
    int weaponId = 1;
    int mag = 30;
    int reserve = 90;
    int kills = 0;
    int deaths = 0;
    int headshots = 0;
    int score = 0;
    float reloadTimer = 0.0f;
    float fireCooldown = 0.0f;
    float respawnTimer = 0.0f;
    bool alive = true;
    PlayerInput input;
    bool active = true;
    bool isBot = false;
    SlotBotState bot;
    int consecutiveHs = 0;
};
struct KillEvent {
    int killerUid = 0;
    int victimUid = 0;
    int headshot = 0;
};
// In-flight bullet, simulated with Newton drag + gravity each tick.
struct Projectile {
    int id = 0;             // monotonic, used by TRACER/IMPACT to correlate on the client
    int shooterCid = -1;
    int shooterUid = 0;
    int weaponId = 0;
    Vec3 pos;               // current position (m)
    Vec3 vel;               // current velocity (m/s)
    float muzzleSpeed = 0.0f;
    float mass_kg = 0.008f;
    float dragCoef = 0.30f;
    float crossArea_m2 = 0.0f;  // pi * (caliber_mm/2000)^2
    float lifeSec = 0.0f;
    int colorId = 0;
};
// In-flight throwable (grenade / flashbang / bomb / smoke / claymore).
struct ActiveThrowable {
    int id = 0;
    int typeId = 0;
    int throwerCid = -1;
    int throwerUid = 0;
    std::string throwerTeam;
    Vec3 pos;
    Vec3 vel;
    float fuseRemaining = 0.0f;
    // For Claymore-class proximity detonators: forward facing & "armed" flag.
    Vec3 forward;
    bool isProximity = false;
    float armDelaySec = 0.0f;  // refuse to detonate until armed
    bool armed = false;
};
// Effect broadcasts emitted by Match::tick — Server fans them out.
struct TracerEvent {
    int projectileId;
    int shooterUid;
    Vec3 origin;
    Vec3 velocity;
    int colorId;
};
struct ImpactEvent {
    int projectileId;
    Vec3 pos;
    int surfaceKind;   // 0=wall, 1=body, 2=head, 3=ground, 4=ricochet
    int victimUid;
};
struct ExplodeEvent {
    int throwableTypeId;
    Vec3 pos;
    int throwerUid;
};
struct FlashEvent {
    int victimUid;     // affected player (server addresses only this player)
    float intensity;   // [0..1]
    float durationSec;
};
class Match {
public:
    Match(int id, const std::string& map, int teamSize);
    int id() const { return id_; }
    const std::string& map() const { return map_; }
    int teamSize() const { return teamSize_; }
    bool ended() const { return ended_; }
    int addPlayer(int clientId, int userId, const std::string& username, const std::string& team,
                  int weaponId = 1);
    int addBot(const std::string& username, const std::string& team, int weaponId);
    int slotIndexByUserId(int userId) const;
    void reattachSlot(int slotIndex, int newClientId);
    void zeroInputAt(int slotIndex);
    void computeBotInputs(float dt);
    void tick(float dt, std::vector<KillEvent>& killEvents);
    // Extended tick that also emits projectile / throwable effect events.
    void tick(float dt,
              std::vector<KillEvent>& killEvents,
              std::vector<TracerEvent>& tracers,
              std::vector<ImpactEvent>& impacts,
              std::vector<ExplodeEvent>& explodes,
              std::vector<FlashEvent>& flashes);
    // Spawn a throwable on behalf of a client. Returns true if accepted.
    bool spawnThrowable(int clientId, int typeId, const Vec3& origin,
                        const Vec3& dir, float powerMps);
    int projectilesInFlight() const { return (int)projectiles_.size(); }
    bool forfeit(int clientId);
    void setInput(int clientId, const PlayerInput& in);
    const std::vector<Slot>& slots() const { return slots_; }
    std::vector<Slot>& slotsMut() { return slots_; }
    int blueScore() const { return blueScore_; }
    int redScore() const { return redScore_; }
    float timeLeft() const { return timeLeft_; }
    int tickNo() const { return tick_; }
    std::string buildStateFor(const Slot& self) const;
    MatchResult resultFor(const Slot& s) const;
    void end() { ended_ = true; }
private:
    int id_;
    std::string map_;
    int teamSize_;
    std::vector<Slot> slots_;
    std::vector<AABB> obstacles_;
    float arenaHalfX_ = 25.0f;
    float arenaHalfZ_ = 25.0f;
    bool blueSpawnUsesX_ = false;
    float blueSpawnX_ = 0.0f, blueSpawnZ_ = -22.0f;
    float redSpawnX_ = 0.0f, redSpawnZ_ = 22.0f;
    int blueScore_ = 0;
    int redScore_ = 0;
    float timeLeft_ = 300.0f;
    int tick_ = 0;
    bool ended_ = false;
    std::vector<Projectile> projectiles_;
    std::vector<ActiveThrowable> throwables_;
    int nextProjectileId_ = 1;
    int nextThrowableId_ = 1;
    void loadMap();
    Vec3 spawnPoint(const std::string& team) const;
    bool lineBlocked(const Vec3& a, const Vec3& b) const;
    void respawn(Slot& s);
    void spawnProjectile(Slot& shooter, const Vec3& eye, const Vec3& aimDir,
                         std::vector<TracerEvent>& tracers);
    void stepProjectiles(float dt,
                         std::vector<KillEvent>& killEvents,
                         std::vector<ImpactEvent>& impacts);
    void stepThrowables(float dt,
                        std::vector<KillEvent>& killEvents,
                        std::vector<ExplodeEvent>& explodes,
                        std::vector<FlashEvent>& flashes);
};
