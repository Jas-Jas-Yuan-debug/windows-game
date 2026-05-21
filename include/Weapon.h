#pragma once
#include "Protocol.h"
struct Weapon {
    int id;
    const char* name;
    int price;
    int damageBody;
    int damageHs;
    int magSize;
    int reserve;
    float cooldownSec;
    float reloadSec;
};
namespace weapons {
inline constexpr Weapon kTable[] = {
    { proto::kWeaponPistol,  "Pistol",   0,    25,  75, 12, 48, 0.40f, 1.5f },
    { proto::kWeaponSMG,     "SMG",      400,  18,  54, 30, 90, 0.09f, 2.0f },
    { proto::kWeaponShotgun, "Shotgun",  900,  95, 140,  8, 32, 0.80f, 2.6f },
    { proto::kWeaponRifle,   "Rifle",    1500, 34, 100, 30, 90, 0.12f, 2.2f },
    { proto::kWeaponSniper,  "Sniper",   2400, 80, 200,  5, 15, 1.20f, 3.0f },
};
inline constexpr int kCount = sizeof(kTable) / sizeof(kTable[0]);
inline const Weapon* lookup(int id) {
    for (int i = 0; i < kCount; ++i) {
        if (kTable[i].id == id) return &kTable[i];
    }
    return nullptr;
}
}
