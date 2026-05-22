#pragma once
#include <string>
struct User {
    int id = 0;
    std::string username;
    std::string passwordHash;
    std::string salt;
    int xp = 0;
    int level = 1;
    int totalKills = 0;
    int totalDeaths = 0;
    int totalHeadshots = 0;
    int matchesPlayed = 0;
    int matchesWon = 0;
    std::string team;
    int credits = 500;
    int selectedWeapon = 1;
};
