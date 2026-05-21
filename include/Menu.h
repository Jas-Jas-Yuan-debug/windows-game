#pragma once
#include "NetClient.h"
#include <raylib.h>
#include <string>
class Menu {
public:
    enum class Action { Play, Leaderboard, Chat, Store, Solo, Logout, Quit };
    struct Result {
        Action action = Action::Quit;
        std::string selectedMap;
    };
    explicit Menu(NetClient& net);
    Result run();
private:
    NetClient& net_;
    bool drawButton(const char* text, int x, int y, int w, int h, Color base, bool enabled = true);
};
