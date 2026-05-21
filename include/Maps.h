#pragma once
#include <string>
#include <vector>
struct MapAABB {
    float cx, cy, cz;
    float sx, sy, sz;
};
void buildMap(const std::string& name, std::vector<MapAABB>& out);
std::vector<std::string> allMapNames();
struct MapInfo {
    float halfX;
    float halfZ;
    bool  spawnUsesX;
    float blueX, blueZ;
    float redX,  redZ;
};
MapInfo mapInfoFor(const std::string& name);
