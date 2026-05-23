#pragma once
#include "NetClient.h"
#include <raylib.h>
#include <string>
#include <vector>
class Game {
public:
    Game(NetClient& net, const std::string& mapName);
    void run();
private:
    struct Obstacle {
        Vector3 center;
        Vector3 size;
    };
    struct PlayerEntry {
        int slot = 0;
        float x = 0, y = 0, z = 0;
        float yaw = 0.0f;
        int hp = 100;
        std::string team;
        bool alive = true;
        int uid = 0;
        std::string name;
    };
    struct RosterEntry {
        int slot = 0;
        int uid = 0;
        std::string name;
        std::string team;
    };
    struct TracerVisual {
        int projectileId = 0;
        int shooterUid = 0;
        Vector3 pos;
        Vector3 vel;
        float age = 0.0f;
        float life = 0.18f;
        Color color = RAYWHITE;
    };
    struct ImpactVisual {
        Vector3 pos;
        float age = 0.0f;
        float life = 0.45f;
        int surfaceKind = 0;
        int victimUid = 0;
    };
    NetClient& net_;
    std::string mapName_;
    static constexpr int kScreenW = 1280;
    static constexpr int kScreenH = 720;
    static constexpr float kPlayerEyeY = 1.7f;
    float arenaHalfX_ = 25.0f;
    float arenaHalfZ_ = 25.0f;
    Color floorColor_ = DARKGRAY;
    std::vector<Obstacle> obstacles_;
    enum class Phase { Queue, InMatch, Ended, Aborted };
    Phase phase_ = Phase::Queue;
    int queueBlue_ = 0;
    int queueRed_ = 0;
    int queueNeed_ = 0;
    std::string myTeam_;
    std::vector<RosterEntry> roster_;
    int curTick_ = 0;
    int selfHp_ = 100, selfMag_ = 0, selfReserve_ = 0;
    int selfKills_ = 0, selfDeaths_ = 0, selfHs_ = 0, selfScore_ = 0;
    int blueScore_ = 0, redScore_ = 0;
    int timeLeftMs_ = 0;
    int selfWeaponId_ = 1;
    std::vector<TracerVisual> tracers_;
    std::vector<ImpactVisual> impacts_;
    float hitMarkerTimer_ = 0.0f;
    float missMarkerTimer_ = 0.0f;
    float damageVignetteTimer_ = 0.0f;
    int lastSelfHp_ = 100;
    std::vector<PlayerEntry> players_;
    Camera3D camera_{};
    double lastInputSent_ = 0.0;
    int sendTick_ = 0;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    bool won_ = false;
    int endBlue_ = 0, endRed_ = 0;
    int endKills_ = 0, endDeaths_ = 0, endHs_ = 0;
    int xpEarned_ = 0, newLevel_ = 0;
    bool leveledUp_ = false;
    int creditsEarned_ = 0;
    int newCredits_ = 0;
    void loadMap();
    void handleTcp(const std::vector<std::string>& m);
    void handleUdp(const std::vector<std::string>& m);
    void applyStatePayload(const std::vector<std::string>& m);
    void runQueueLoop();
    void runMatchLoop();
    void runEndLoop();
    void sendInput(bool fire);
    void updateLocalCamera(float dt);
    void updateEffects(float dt);
    PlayerEntry* findSelfPlayer();
    void draw3D();
    void drawHUD();
    void drawEndScreen();
};
