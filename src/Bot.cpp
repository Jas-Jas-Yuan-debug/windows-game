#include "Bot.h"
#include <cmath>
#include <cstdlib>
namespace {
float frand() {
    return (float)std::rand() / (float)RAND_MAX;
}
float frandSigned() { return frand() * 2.0f - 1.0f; }
}
BotInput computeBotInput(BotState& bs, const BotView& view, float dt) {
    BotInput out;
    if (view.targetAlive && view.targetVisible) {
        float dx = view.targetPos.x - view.selfPos.x;
        float dz = view.targetPos.z - view.selfPos.z;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > 0.001f) {
            float desiredYaw = std::atan2(dx, dz);
            bs.aimNoise += (frandSigned() * 0.5f - bs.aimNoise) * dt * 1.5f;
            out.yaw = desiredYaw + bs.aimNoise * 0.1f;
            float dy = (view.targetPos.y + 1.5f) - (view.selfPos.y + 1.7f);
            out.pitch = std::atan2(dy, dist);
            float yawDiff = out.yaw - view.selfYaw;
            while (yawDiff > 3.14159f) yawDiff -= 6.28318f;
            while (yawDiff < -3.14159f) yawDiff += 6.28318f;
            if (std::fabs(yawDiff) < 0.18f) {
                out.fire = true;
            }
            if (dist > 8.0f) {
                out.moveX = dx / dist;
                out.moveZ = dz / dist;
            } else if (dist < 3.0f) {
                out.moveX = -dx / dist;
                out.moveZ = -dz / dist;
            } else {
                float strafe = ((bs.roamTimer > 0.5f) ? 1.0f : -1.0f);
                out.moveX = -dz / dist * strafe;
                out.moveZ =  dx / dist * strafe;
            }
        }
    } else {
        bs.roamTimer -= dt;
        if (bs.roamTimer <= 0.0f) {
            bs.roamTimer = 1.5f + frand() * 1.5f;
            float ang = frand() * 6.28318f;
            bs.roamDirX = std::cos(ang);
            bs.roamDirZ = std::sin(ang);
        }
        out.moveX = bs.roamDirX * 0.6f;
        out.moveZ = bs.roamDirZ * 0.6f;
        out.yaw = std::atan2(bs.roamDirX, bs.roamDirZ);
    }
    return out;
}
