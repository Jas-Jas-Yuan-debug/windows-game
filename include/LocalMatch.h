#pragma once
#include "NetClient.h"
#include "Maps.h"
#include "Bot.h"
#include <raylib.h>
#include <string>
#include <vector>
class LocalMatch {
public:
    enum class Mode { Practice, VsBots };
    LocalMatch(NetClient& net, Mode mode);
    void run();
private:
    struct Actor {
        Vector3 pos;
        float yaw = 0.0f;
        float pitch = 0.0f;
        int hp = 100;
        int mag = 12;
        int reserve = 48;
        bool alive = true;
        float cooldown = 0.0f;
        float reload = 0.0f;
        float respawn = 0.0f;
        BotState bot;
        bool isBot = false;
        int team = 0;
        std::string name;
        int weaponId = 1;
    };
    struct Dummy {
        Vector3 pos;
        int hp = 100;
        float respawn = 0.0f;
        int hitCount = 0;
    };
    NetClient& net_;
    Mode mode_;
    int weaponId_ = 1;
    std::string mapName_;
    std::vector<MapAABB> obstacles_;
    MapInfo mapInfo_{};
    Camera3D camera_{};
    Actor self_{};
    std::vector<Actor> bots_;
    std::vector<Dummy> dummies_;
    int selfKills_ = 0;
    int botKills_ = 0;
    int dummyHits_ = 0;
    float matchTime_ = 0.0f;
    float matchLimit_ = 180.0f;
    bool ended_ = false;
    void initWorld();
    void updateSelf(float dt);
    void updateBots(float dt);
    bool isBlockedAt(float x, float z) const;
    bool rayBlocked(Vector3 from, Vector3 to) const;
    void fireSelf();
    void fireBot(Actor& b);
    void respawn(Actor& a);
    void draw3D();
    void drawHUD();
};
