#pragma once
#include <string>
struct MatchResult {
    std::string mapName;
    std::string team;
    int teamScore = 0;
    int enemyTeamScore = 0;
    int playerKills = 0;
    int playerDeaths = 0;
    int playerScore = 0;
    int headshots = 0;
    bool won = false;
};
