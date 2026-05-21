#include "Maps.h"
namespace {
void buildArena(std::vector<MapAABB>& out) {
    out = {
        { 0.0f, 0.6f,  3.0f,  6.0f, 1.2f, 0.6f },
        { 0.0f, 0.6f, -3.0f,  6.0f, 1.2f, 0.6f },
        { 3.0f, 0.6f,  0.0f,  0.6f, 1.2f, 6.0f },
        {-3.0f, 0.6f,  0.0f,  0.6f, 1.2f, 6.0f },
        {  6.0f, 1.0f,  6.0f,  2.0f, 2.0f, 2.0f },
        { -6.0f, 1.0f, -6.0f,  2.0f, 2.0f, 2.0f },
        { -6.0f, 1.0f,  6.0f,  2.0f, 2.0f, 2.0f },
        {  6.0f, 1.0f, -6.0f,  2.0f, 2.0f, 2.0f },
        {  18.0f, 1.5f,  18.0f, 4.0f, 3.0f, 1.0f },
        {  18.0f, 1.5f,  16.0f, 1.0f, 3.0f, 3.0f },
        { -18.0f, 1.5f, -18.0f, 4.0f, 3.0f, 1.0f },
        { -18.0f, 1.5f, -16.0f, 1.0f, 3.0f, 3.0f },
        {  18.0f, 1.5f, -18.0f, 4.0f, 3.0f, 1.0f },
        {  18.0f, 1.5f, -16.0f, 1.0f, 3.0f, 3.0f },
        { -18.0f, 1.5f,  18.0f, 4.0f, 3.0f, 1.0f },
        { -18.0f, 1.5f,  16.0f, 1.0f, 3.0f, 3.0f },
        {  0.0f, 0.6f,  18.0f, 4.0f, 1.2f, 0.6f },
        {  0.0f, 0.6f, -18.0f, 4.0f, 1.2f, 0.6f },
    };
}
void buildDust(std::vector<MapAABB>& out) {
    out = {
        { -8.0f, 1.5f, -10.0f, 1.0f, 3.0f, 8.0f },
        { -8.0f, 1.5f,  10.0f, 1.0f, 3.0f, 8.0f },
        {  8.0f, 1.5f, -10.0f, 1.0f, 3.0f, 8.0f },
        {  8.0f, 1.5f,  10.0f, 1.0f, 3.0f, 8.0f },
        {  0.0f, 1.5f,  0.0f, 1.0f, 3.0f, 1.0f },
        {  1.5f, 1.5f,  1.5f, 1.0f, 3.0f, 1.0f },
        { -1.5f, 1.5f, -1.5f, 1.0f, 3.0f, 1.0f },
        {  0.0f, 0.6f,  6.0f, 4.0f, 1.2f, 0.6f },
        {  0.0f, 0.6f, -6.0f, 4.0f, 1.2f, 0.6f },
        {  4.0f, 1.0f,  3.0f, 2.0f, 2.0f, 2.0f },
        { -4.0f, 1.0f, -3.0f, 2.0f, 2.0f, 2.0f },
        {  4.0f, 1.0f, -8.0f, 2.0f, 2.0f, 2.0f },
        { -4.0f, 1.0f,  8.0f, 2.0f, 2.0f, 2.0f },
        { -15.0f, 1.0f, -18.0f, 2.0f, 2.0f, 1.0f },
        {  15.0f, 1.0f,  18.0f, 2.0f, 2.0f, 1.0f },
        {   8.0f, 1.0f, -16.0f, 2.0f, 2.0f, 1.0f },
        {  -8.0f, 1.0f,  16.0f, 2.0f, 2.0f, 1.0f },
        {  12.0f, 1.5f,  0.0f, 4.0f, 3.0f, 1.0f },
        {  14.0f, 1.5f,  2.0f, 1.0f, 3.0f, 3.0f },
        { -12.0f, 1.5f,  0.0f, 4.0f, 3.0f, 1.0f },
        { -14.0f, 1.5f, -2.0f, 1.0f, 3.0f, 3.0f },
    };
}
void buildOffice(std::vector<MapAABB>& out) {
    out = {
        {  0.0f, 1.25f,  4.0f, 1.0f, 2.5f, 4.0f },
        {  0.0f, 1.25f, -4.0f, 1.0f, 2.5f, 4.0f },
        { -4.0f, 1.0f,  2.0f, 1.0f, 2.0f, 1.0f },
        { -4.0f, 1.0f, -2.0f, 1.0f, 2.0f, 1.0f },
        {  4.0f, 1.0f,  2.0f, 1.0f, 2.0f, 1.0f },
        {  4.0f, 1.0f, -2.0f, 1.0f, 2.0f, 1.0f },
        { -4.0f, 0.6f,  0.0f, 0.6f, 1.2f, 4.0f },
        {  4.0f, 0.6f,  0.0f, 0.6f, 1.2f, 4.0f },
        { -13.0f, 1.0f, -6.0f, 3.0f, 2.0f, 1.0f },
        { -13.0f, 1.0f, -2.0f, 3.0f, 2.0f, 1.0f },
        { -15.0f, 1.0f, -4.0f, 1.0f, 2.0f, 3.0f },
        {  13.0f, 1.0f,  6.0f, 3.0f, 2.0f, 1.0f },
        {  13.0f, 1.0f,  2.0f, 3.0f, 2.0f, 1.0f },
        {  15.0f, 1.0f,  4.0f, 1.0f, 2.0f, 3.0f },
        { -15.0f, 1.0f,  12.0f, 2.0f, 2.0f, 2.0f },
        {  15.0f, 1.0f, -12.0f, 2.0f, 2.0f, 2.0f },
        { -16.0f, 0.6f,  0.0f, 0.6f, 1.2f, 4.0f },
        {  16.0f, 0.6f,  0.0f, 0.6f, 1.2f, 4.0f },
        {  0.0f, 1.0f,  15.0f, 3.0f, 2.0f, 1.0f },
        {  0.0f, 1.0f, -15.0f, 3.0f, 2.0f, 1.0f },
    };
}
}
void buildMap(const std::string& name, std::vector<MapAABB>& out) {
    if (name == "Dust") {
        buildDust(out);
    } else if (name == "Office") {
        buildOffice(out);
    } else {
        buildArena(out);
    }
}
std::vector<std::string> allMapNames() {
    return {"Arena", "Dust", "Office"};
}
MapInfo mapInfoFor(const std::string& name) {
    MapInfo m{};
    if (name == "Office") {
        m.halfX = 20.0f; m.halfZ = 20.0f;
        m.spawnUsesX = true;
        m.blueX = -18.0f; m.blueZ = 0.0f;
        m.redX  =  18.0f; m.redZ  = 0.0f;
    } else if (name == "Dust") {
        m.halfX = 25.0f; m.halfZ = 25.0f;
        m.spawnUsesX = false;
        m.blueX = 0.0f; m.blueZ = -22.0f;
        m.redX  = 0.0f; m.redZ  =  22.0f;
    } else {
        m.halfX = 25.0f; m.halfZ = 25.0f;
        m.spawnUsesX = false;
        m.blueX = 0.0f; m.blueZ = -22.0f;
        m.redX  = 0.0f; m.redZ  =  22.0f;
    }
    return m;
}
