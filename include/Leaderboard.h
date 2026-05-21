#pragma once
#include "NetClient.h"
#include <raylib.h>
class Leaderboard {
public:
    explicit Leaderboard(NetClient& net);
    void run();
private:
    NetClient& net_;
};
