#pragma once
#include "Protocol.h"
// Weapon stat block. The first nine fields are the original wire-facing
// damage / mag / cooldown stats (these are serialized in kT_StoreItem).
// The remaining six are *ballistic* fields consumed only by the server
// hit-resolution code and the client recoil/spread visuals — they are
// NOT sent over the wire, so adding them is non-breaking for the
// existing STORE_LIST protocol layout.
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
    // -------- ballistic / handling (server + client) --------
    int   caliberMm;           // 127 = 12.7 mm, 762 = 7.62 mm, 556 = 5.56 mm, 9 = 9 mm.
    float muzzleVelocityMps;   // initial bullet speed (rounded to nearest 5 m/s).
    float bulletMassG;         // projectile mass, grams (one decimal place).
    float dragCoefficient;     // form-factor drag coefficient (~0.295 for spitzer, ~0.5 for 9 mm ball).
    float spreadDeg;           // base cone-of-fire half-angle in degrees.
    float recoilKick;          // pixels of upward pitch kick per shot (client applies, server may use too).
};
namespace weapons {
inline constexpr Weapon kTable[] = {
    // -------- Original 5 (IDs 1-5) -------- preserved exactly.
    // id, name,             price, body, hs,   mag, rsv, cd,    rld,   cal, muzzle, mass, drag,  spread, recoil
    { proto::kWeaponPistol,  "Pistol",   0,    25,  75, 12, 48, 0.40f, 1.5f,    9,  360.0f,  8.0f, 0.500f, 0.50f, 1.8f }, // generic 9 mm sidearm
    { proto::kWeaponSMG,     "SMG",      400,  18,  54, 30, 90, 0.09f, 2.0f,    9,  390.0f,  8.0f, 0.500f, 2.00f, 1.4f }, // 9 mm SMG (MP5-class baseline)
    { proto::kWeaponShotgun, "Shotgun",  900,  95, 140,  8, 32, 0.80f, 2.6f,   12,  410.0f, 32.0f, 0.700f, 4.50f, 6.5f }, // 12-ga buck, 9 pellets approximated
    { proto::kWeaponRifle,   "Rifle",    1500, 34, 100, 30, 90, 0.12f, 2.2f,  556,  940.0f,  4.0f, 0.295f, 1.00f, 2.6f }, // 5.56x45 baseline
    { proto::kWeaponSniper,  "Sniper",   2400, 80, 200,  5, 15, 1.20f, 3.0f,  762,  830.0f,  9.5f, 0.295f, 0.05f, 9.0f }, // 7.62 bolt-action approximation

    // ============================================================
    // IDs 6-8 : 12.7 mm anti-materiel rifles  ($5000-$9000)
    // ============================================================
    //  - extreme damage, slow fire, very heavy bullets.
    //  - muzzle velocity ~853 m/s spec'd; rounded to nearest 5.
    //  - tiny spread, huge recoil kick.
    { 6,  "Barrett M82",     5000, 180, 350,  10, 30, 1.50f, 4.0f,  127,  855.0f, 42.0f, 0.295f, 0.10f, 22.0f }, // .50 BMG semi-auto, 660 gr ball
    { 7,  "McMillan TAC-50", 7000, 220, 400,   5, 20, 1.90f, 4.5f,  127,  860.0f, 47.0f, 0.295f, 0.04f, 26.0f }, // .50 BMG bolt, world-record class
    { 8,  "NTW-20",          9000, 280, 500,   3, 12, 2.40f, 5.0f,  127,  720.0f, 50.0f, 0.300f, 0.06f, 34.0f }, // South African 20 mm hybrid (treated as 12.7 family)

    // ============================================================
    // IDs 9-11 : 7.62 mm battle rifles / DMR  ($2500-$4000)
    // ============================================================
    //  - high damage, moderate fire rate.
    { 9,  "AK-47",           2500,  45, 130,  30, 90, 0.10f, 2.5f,  762,  715.0f,  7.9f, 0.295f, 1.40f, 4.5f }, // 7.62x39, classic
    { 10, "SVD Dragunov",    3200,  72, 175,  10, 30, 0.50f, 3.0f,  762,  830.0f,  9.6f, 0.295f, 0.30f, 7.0f }, // 7.62x54R DMR
    { 11, "FN FAL",          3500,  55, 150,  20, 60, 0.13f, 2.7f,  762,  840.0f,  9.5f, 0.295f, 1.20f, 5.0f }, // 7.62x51 NATO battle rifle

    // ============================================================
    // IDs 12-15 : 5.56 mm assault rifles  ($1200-$2000)
    // ============================================================
    //  - medium damage, fast fire.
    { 12, "M4A1",            1700,  32,  95,  30, 90, 0.09f, 2.1f,  556,  910.0f,  4.0f, 0.295f, 0.95f, 2.4f }, // 5.56 carbine
    { 13, "M16A4",           1600,  35, 100,  30, 90, 0.11f, 2.2f,  556,  955.0f,  4.0f, 0.295f, 0.85f, 2.6f }, // 5.56 full-length
    { 14, "SCAR-L",          1900,  34,  98,  30, 90, 0.10f, 2.1f,  556,  870.0f,  4.0f, 0.295f, 0.90f, 2.5f }, // FN 5.56
    { 15, "AUG A3",          1850,  33,  96,  30, 90, 0.09f, 2.3f,  556,  940.0f,  4.0f, 0.295f, 1.00f, 2.3f }, // Steyr bullpup 5.56

    // ============================================================
    // IDs 16-19 : 9 mm pistols / SMGs  ($200-$800)
    // ============================================================
    //  - low damage, very fast or rapid.
    { 16, "Glock 17",         200,  22,  66,  17, 51, 0.18f, 1.4f,    9,  375.0f,  8.0f, 0.500f, 0.55f, 1.6f }, // 9x19 Para sidearm
    { 17, "Beretta M9",       250,  23,  68,  15, 45, 0.20f, 1.5f,    9,  380.0f,  8.0f, 0.500f, 0.50f, 1.7f }, // 9x19 Para sidearm
    { 18, "MP5",              700,  20,  60,  30, 90, 0.075f,2.0f,    9,  400.0f,  8.0f, 0.500f, 1.80f, 1.3f }, // H&K 9 mm SMG
    { 19, "P90",              800,  19,  55,  50,100, 0.07f, 2.2f,    9,  715.0f,  4.6f, 0.350f, 1.60f, 1.2f }, // 5.7x28 actually, but cataloged as 9-family for simplicity
};
inline constexpr int kCount = sizeof(kTable) / sizeof(kTable[0]);
inline const Weapon* lookup(int id) {
    for (int i = 0; i < kCount; ++i) {
        if (kTable[i].id == id) return &kTable[i];
    }
    return nullptr;
}
}
