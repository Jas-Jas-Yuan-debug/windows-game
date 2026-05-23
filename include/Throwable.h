#pragma once
// Throwable equipment: grenades, flashbangs, demolition charges.
// Stat table is data-only (no raylib/types beyond <string>/<vector>) so this
// header can be included by both client UI code and the headless server.
//
// Convention for the registry — mirrors weapons::lookup() in Weapon.h:
//   throwables::lookup(id)  -> const Throwable* (nullptr if id unknown)
//   throwables::all()       -> const std::vector<Throwable>& (display order)
//
// ID space:
//   100-109 = Grenades (frag / smoke / concussion)
//   200-209 = Flashbangs
//   300-309 = Bombs / demolition charges (C4, Claymore, Semtex, ...)
//
// The "kind" field is the server-relevant discriminator: the ballistics
// agent uses it to dispatch to the right effect (Grenade => radial damage,
// Flashbang => vision impairment via kU_Flash, Bomb => either timed (C4),
// proximity (Claymore — directional), or contact (Sticky)).
#include <string>
#include <vector>

enum class ThrowableKind {
    Grenade   = 0,
    Flashbang = 1,
    Bomb      = 2,
};

struct Throwable {
    int           id;
    ThrowableKind kind;
    std::string   name;
    int           priceCredits;
    float         fuseSec;            // Seconds from throw/release until detonation.
                                      //   0.0   = instant (Claymore — uses proximity instead)
                                      //   <0.0  = contact (Sticky bomb)
                                      //   >5.0  = long-fuse plant-and-defuse (C4)
    float         radiusM;            // Effect radius in metres (damage falloff origin).
    float         maxDamage;          // Peak damage at radius = 0.
    float         flashIntensity;     // 0.0 = no flash, 1.0 = pure white (only meaningful for Flashbang).
    float         flashDurationSec;   // Seconds the flash effect lingers.
    float         falloffPow;         // 1.0 = linear, 2.0 = quadratic (more "explosion-y").
    float         massKg;             // Mass — used by the server's throw-arc physics.
};

namespace throwables {

// Note on directional / contact specials (server agent: please special-case):
//   - Claymore M18 (id 304): fuseSec=0.0 + kind=Bomb means PROXIMITY trigger,
//     and its damage should only apply in a narrow forward cone (~60deg),
//     not a sphere.
//   - Sticky Bomb (id 307): fuseSec<0 means it sticks to whatever it first
//     contacts and then runs a short internal countdown; treat as contact-armed.
//   - C4 Block (id 300): long fuseSec (40s) is the classic plant-and-defuse;
//     defuse interaction lives outside this header.
inline const std::vector<Throwable>& all() {
    static const std::vector<Throwable> kAll = {
        // ============================================================
        // IDs 100-109 — Grenades  ($100-$400)
        // ============================================================
        // Fragmentation grenades + smoke (no/low damage, big radius for LOS).
        //  id, kind,                  name,              price, fuse,  radius, dmg,  flashI, flashDur, falloff, mass
        { 100, ThrowableKind::Grenade, "M67 Fragmentation", 300, 4.0f,   8.0f,  120.0f, 0.0f, 0.0f, 1.5f, 0.40f }, // US M67 — 5s composition B body
        { 101, ThrowableKind::Grenade, "RGD-5",             220, 3.5f,   7.0f,  100.0f, 0.0f, 0.0f, 1.4f, 0.31f }, // Soviet WWII-era frag
        { 102, ThrowableKind::Grenade, "F1",                250, 4.0f,   9.0f,  140.0f, 0.0f, 0.0f, 1.6f, 0.60f }, // F1 "limonka" — heavy iron body
        { 103, ThrowableKind::Grenade, "Mk2 Pineapple",     180, 4.5f,   7.5f,  110.0f, 0.0f, 0.0f, 1.5f, 0.60f }, // US WWII pineapple
        { 104, ThrowableKind::Grenade, "M61",               280, 4.0f,   8.5f,  130.0f, 0.0f, 0.0f, 1.5f, 0.45f }, // Vietnam-era US frag
        { 105, ThrowableKind::Grenade, "RGO",               320, 3.0f,  10.0f,  160.0f, 0.0f, 0.0f, 1.7f, 0.53f }, // Russian impact frag (modeled timed)
        { 106, ThrowableKind::Grenade, "V40 Mini",          150, 4.0f,   5.0f,   60.0f, 0.0f, 0.0f, 1.3f, 0.14f }, // Tiny Dutch mini grenade
        { 107, ThrowableKind::Grenade, "RDG-2 Smoke",       100, 2.0f,  15.0f,    8.0f, 0.0f, 0.0f, 1.0f, 0.50f }, // SMOKE — near-zero damage, big LOS block
        { 108, ThrowableKind::Grenade, "M18 Smoke",         120, 1.8f,  14.0f,    5.0f, 0.0f, 0.0f, 1.0f, 0.54f }, // M18 colored smoke
        { 109, ThrowableKind::Grenade, "Mk3A2 Concussion",  400, 4.0f,  12.0f,  200.0f, 0.2f, 0.5f, 2.0f, 0.45f }, // Offensive concussion — high blast, mild flash

        // ============================================================
        // IDs 200-209 — Flashbangs  ($80-$250)
        // ============================================================
        // Low damage. Flash intensity 0.5-1.0, duration 1.5-8s, radius 5-20m.
        //  id, kind,                    name,                  price, fuse, radius, dmg,  flashI, flashDur, falloff, mass
        { 200, ThrowableKind::Flashbang, "M84 Flashbang",         150, 1.5f,  10.0f,  6.0f, 1.00f, 5.0f, 1.5f, 0.20f }, // US M84 — the genre standard
        { 201, ThrowableKind::Flashbang, "GBG-001",               130, 1.5f,   9.0f,  5.0f, 0.95f, 4.5f, 1.5f, 0.21f }, // generic German bang
        { 202, ThrowableKind::Flashbang, "BTG-S",                 110, 1.5f,   8.5f,  5.0f, 0.90f, 4.0f, 1.5f, 0.20f }, // Eastern Bloc analog
        { 203, ThrowableKind::Flashbang, "AB-EI",                  90, 1.6f,   8.0f,  4.0f, 0.80f, 3.5f, 1.4f, 0.18f }, // budget flash
        { 204, ThrowableKind::Flashbang, "Stingball",             180, 1.5f,  10.0f, 18.0f, 0.70f, 2.5f, 1.5f, 0.25f }, // rubber-pellet sting + flash combo
        { 205, ThrowableKind::Flashbang, "B&T Diversionary",      200, 1.4f,  11.0f,  7.0f, 1.00f, 6.0f, 1.5f, 0.22f }, // Brugger & Thomet pro
        { 206, ThrowableKind::Flashbang, "MK141 Mod 0",           220, 1.5f,  12.0f,  6.0f, 1.00f, 6.5f, 1.5f, 0.24f }, // US MK141 SOF version
        { 207, ThrowableKind::Flashbang, "NICO 9-Banger",         250, 1.6f,  14.0f,  9.0f, 1.00f, 8.0f, 1.4f, 0.30f }, // 9-burst — sustained disorientation
        { 208, ThrowableKind::Flashbang, "ALS Distraction",        80, 1.7f,   7.0f,  3.0f, 0.55f, 2.0f, 1.4f, 0.17f }, // cheap training stun
        { 209, ThrowableKind::Flashbang, "FlashShield Pro",       180, 1.5f,  20.0f,  4.0f, 0.85f, 5.0f, 1.6f, 0.26f }, // wide-radius riot variant

        // ============================================================
        // IDs 300-309 — Bombs / demolition charges  ($500-$3000)
        // ============================================================
        // High damage, varied fuse types. See note above on Claymore/Sticky/C4 specials.
        //  id, kind,                name,             price, fuse,  radius, dmg,    flashI, flashDur, falloff, mass
        { 300, ThrowableKind::Bomb, "C4 Block",         3000, 40.0f, 18.0f,  800.0f, 0.0f, 0.0f, 1.8f, 1.25f }, // plant-and-defuse classic, 40s timer
        { 301, ThrowableKind::Bomb, "Semtex 1H",        1200,  4.0f, 10.0f,  400.0f, 0.0f, 0.0f, 1.7f, 1.00f }, // Czech plastic explosive
        { 302, ThrowableKind::Bomb, "TNT Charge",        700,  5.0f,  8.0f,  300.0f, 0.0f, 0.0f, 1.5f, 1.00f }, // 1 kg TNT block
        { 303, ThrowableKind::Bomb, "Claymore M18",     1800,  0.0f,  6.0f, 1200.0f, 0.0f, 0.0f, 2.5f, 1.58f }, // PROXIMITY + DIRECTIONAL (server: 60° cone forward)
        { 304, ThrowableKind::Bomb, "Satchel Charge M37",1500, 6.0f, 12.0f,  600.0f, 0.0f, 0.0f, 1.6f, 2.30f }, // M37 demolition satchel
        { 305, ThrowableKind::Bomb, "IED 5kg",           500,  3.0f, 10.0f,  500.0f, 0.0f, 0.0f, 1.5f, 5.00f }, // improvised 5kg
        { 306, ThrowableKind::Bomb, "IED 10kg",          900,  3.5f, 14.0f,  800.0f, 0.0f, 0.0f, 1.6f, 10.0f }, // improvised 10kg
        { 307, ThrowableKind::Bomb, "Sticky Bomb",      1400, -1.0f,  7.0f,  500.0f, 0.0f, 0.0f, 1.7f, 0.80f }, // CONTACT — fuseSec<0 marker (3s after stick is server's job)
        { 308, ThrowableKind::Bomb, "Det-Cord Roll",     600,  2.5f,  3.0f,  300.0f, 0.0f, 0.0f, 1.4f, 0.60f }, // narrow but intense linear charge
        { 309, ThrowableKind::Bomb, "Demo Pack 25kg",   2500,  8.0f, 25.0f, 2000.0f, 0.0f, 0.0f, 1.8f, 25.0f }, // shaped demo pack — area denial
    };
    return kAll;
}

inline const Throwable* lookup(int id) {
    const auto& tbl = all();
    for (const auto& t : tbl) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

// Convenience: iterate by category for UI tabs.
inline std::vector<const Throwable*> ofKind(ThrowableKind k) {
    std::vector<const Throwable*> out;
    for (const auto& t : all()) {
        if (t.kind == k) out.push_back(&t);
    }
    return out;
}

} // namespace throwables
