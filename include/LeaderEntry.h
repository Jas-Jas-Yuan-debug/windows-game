#pragma once
#include <string>
struct LeaderEntry {
    std::string username;
    int level = 1;
    int xp = 0;
    int totalKills = 0;
    int totalDeaths = 0;
    int matchesPlayed = 0;
    int matchesWon = 0;
    int totalHeadshots = 0;
    double winRate = 0.0;
    double kd = 0.0;
    std::string rank;
};
