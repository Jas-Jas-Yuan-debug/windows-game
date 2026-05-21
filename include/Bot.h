#pragma once
#include <vector>
#include <raylib.h>
struct BotView {
    Vector3 selfPos;
    float selfYaw;
    int selfHp;
    Vector3 targetPos;
    bool targetAlive;
    bool targetVisible;
};
struct BotInput {
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool fire = false;
};
struct BotState {
    float roamTimer = 0.0f;
    float roamDirX = 0.0f;
    float roamDirZ = 1.0f;
    float aimNoise = 0.0f;
};
BotInput computeBotInput(BotState& bs, const BotView& view, float dt);
