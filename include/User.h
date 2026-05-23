#pragma once
#include <set>
#include <string>
#include <vector>
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
    // -------- Throwable inventory (TODO: server inventory schema) --------
    // Client-local until the server agent wires up persistence (a new
    // user_throwables table mirroring user_weapons would be the obvious
    // approach). The Store UI exposes BUY/EQUIP affordances against these
    // fields even when the server schema isn't ready — see Store.cpp.
    std::set<int> ownedThrowables;       // every throwable id the user has purchased
    std::vector<int> selectedGrenades;   // up to kMaxGrenadeSlots equipped (default 4)
    int selectedBomb = 0;                // single bomb-slot id (0 = none)
};
// Inventory caps (server agent: please enforce these in the Match start path).
constexpr int kMaxGrenadeSlots = 4;
