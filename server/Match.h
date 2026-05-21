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
    void loadMap();
    Vec3 spawnPoint(const std::string& team) const;
    bool lineBlocked(const Vec3& a, const Vec3& b) const;
    void respawn(Slot& s);
};
