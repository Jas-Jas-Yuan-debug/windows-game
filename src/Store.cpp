#include "Store.h"
#include "Protocol.h"
#include "Throwable.h"
#include "UiTheme.h"
#include "User.h"
#include "Weapon.h"
#include <raylib.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
namespace {
constexpr int kWinW = 800;
constexpr int kWinH = 600;

// ---- UI rows ---------------------------------------------------------------
struct GunRow {
    int id = 0;
    std::string name;
    int price = 0;
    int dmgBody = 0;
    int dmgHs = 0;
    int mag = 0;
    int reserve = 0;
    int cooldownMs = 0;
    bool owned = false;
    bool selected = false;
    int caliberMm = 0;  // pulled from local Weapon.h catalog for the type icon/badge
};

struct ThrowableRow {
    int id = 0;
    ThrowableKind kind = ThrowableKind::Grenade;
    std::string name;
    int price = 0;
    float fuseSec = 0.0f;
    float radiusM = 0.0f;
    float maxDamage = 0.0f;
    bool owned = false;
    bool equipped = false;
};

// ---- helpers ---------------------------------------------------------------
bool drawButton(const char* text, int x, int y, int w, int h, Color base, bool enabled) {
    Vector2 mp = GetMousePosition();
    bool hover = enabled && mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color fill = base;
    if (!enabled) fill = { 70, 70, 70, 255 };
    else if (hover) fill = { (unsigned char)std::min(255, base.r + 35),
                              (unsigned char)std::min(255, base.g + 35),
                              (unsigned char)std::min(255, base.b + 35), 255 };
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 16;
    int tw = ui::measureTextBold(text, fs);
    ui::drawTextBold(text, x + (w - tw) / 2, y + (h - fs) / 2, fs, enabled ? RAYWHITE : LIGHTGRAY);
    return enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool drawBackButton(int x, int y, int w, int h) {
    Vector2 mp = GetMousePosition();
    bool hover = mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color base = hover ? Color{ 160, 70, 70, 255 } : Color{ 130, 50, 50, 255 };
    DrawRectangle(x, y, w, h, base);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 18;
    int tw = ui::measureTextBold("BACK", fs);
    ui::drawTextBold("BACK", x + (w - tw) / 2, y + (h - fs) / 2, fs, RAYWHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Caliber pill — small colored badge on the left of gun rows so users can
// distinguish 12.7/7.62/5.56/9mm at a glance.
Color caliberColor(int caliberMm) {
    switch (caliberMm) {
        case 127: return { 220,  80,  80, 255 }; // red for AM rifles
        case 762: return { 230, 160,  60, 255 }; // orange for battle rifles
        case 556: return {  90, 170, 230, 255 }; // blue for ARs
        case   9: return { 150, 200, 110, 255 }; // green for 9mm
        case  12: return { 200, 200, 200, 255 }; // gray for shotgun (12-ga)
        default:  return { 180, 180, 180, 255 };
    }
}
const char* caliberLabel(int caliberMm) {
    switch (caliberMm) {
        case 127: return "12.7";
        case 762: return "7.62";
        case 556: return "5.56";
        case   9: return "9mm";
        case  12: return "12ga";
        default:  return "?";
    }
}

const char* throwableTypeBadge(ThrowableKind k) {
    switch (k) {
        case ThrowableKind::Grenade:   return "FRAG";
        case ThrowableKind::Flashbang: return "FLASH";
        case ThrowableKind::Bomb:      return "BOMB";
    }
    return "?";
}
Color throwableTypeColor(ThrowableKind k) {
    switch (k) {
        case ThrowableKind::Grenade:   return { 230, 130,  60, 255 };
        case ThrowableKind::Flashbang: return { 240, 230, 130, 255 };
        case ThrowableKind::Bomb:      return { 220,  80,  80, 255 };
    }
    return GRAY;
}

// One tabbed view of throwables (Grenades / Flashbangs / Bombs).
std::vector<ThrowableRow> buildThrowableRows(ThrowableKind kind,
                                             const std::set<int>& owned,
                                             const std::vector<int>& equippedGrenades,
                                             int equippedBombId) {
    std::vector<ThrowableRow> rows;
    for (const auto* t : throwables::ofKind(kind)) {
        ThrowableRow r;
        r.id = t->id;
        r.kind = t->kind;
        r.name = t->name;
        r.price = t->priceCredits;
        r.fuseSec = t->fuseSec;
        r.radiusM = t->radiusM;
        r.maxDamage = t->maxDamage;
        r.owned = owned.count(t->id) > 0;
        if (kind == ThrowableKind::Bomb) {
            r.equipped = (equippedBombId == t->id);
        } else {
            r.equipped = std::find(equippedGrenades.begin(), equippedGrenades.end(), t->id)
                         != equippedGrenades.end();
        }
        rows.push_back(std::move(r));
    }
    std::sort(rows.begin(), rows.end(),
              [](const ThrowableRow& a, const ThrowableRow& b) { return a.id < b.id; });
    return rows;
}

} // namespace

Store::Store(NetClient& net) : net_(net) {}

void Store::run() {
    InitWindow(kWinW, kWinH, "ClaudeGame - Store");
    SetTargetFPS(60);
    SetExitKey(0);
    ui::loadFonts();

    std::vector<GunRow> guns;
    int credits = net_.credits;
    std::string toast;
    double toastUntil = 0.0;
    Color toastColor = RED;

    // Client-local throwable inventory until the server agent wires up
    // server-side persistence. Once the server protocol supports
    // throwables we'll pull these from a STORE_LIST-style batch.
    // TODO: server inventory schema — replace these with NetClient fields.
    std::set<int> ownedThrowables;
    std::vector<int> equippedGrenades;
    int equippedBombId = 0;

    enum Tab { TAB_GUNS = 0, TAB_GRENADES = 1, TAB_FLASHBANGS = 2, TAB_BOMBS = 3 };
    int activeTab = TAB_GUNS;
    float scrollOffset[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool draggingScroll = false;
    float dragLastY = 0.0f;

    net_.sendTcp({ proto::kT_StoreList });
    bool done = false;

    while (!WindowShouldClose() && !done) {
        net_.poll();
        auto msgs = net_.drainTcp();
        for (auto& m : msgs) {
            if (m.empty()) continue;
            const std::string& t = m[0];
            if (t == proto::kT_StoreItem) {
                if (m.size() < 11) continue;
                GunRow r;
                r.id         = std::atoi(m[1].c_str());
                r.name       = m[2];
                r.price      = std::atoi(m[3].c_str());
                r.dmgBody    = std::atoi(m[4].c_str());
                r.dmgHs      = std::atoi(m[5].c_str());
                r.mag        = std::atoi(m[6].c_str());
                r.reserve    = std::atoi(m[7].c_str());
                r.cooldownMs = std::atoi(m[8].c_str());
                r.owned      = m[9] != "0";
                r.selected   = m[10] != "0";
                if (const Weapon* w = weapons::lookup(r.id)) r.caliberMm = w->caliberMm;
                bool replaced = false;
                for (auto& existing : guns) {
                    if (existing.id == r.id) { existing = r; replaced = true; break; }
                }
                if (!replaced) guns.push_back(std::move(r));
                if (guns.back().selected) net_.selectedWeapon = guns.back().id;
            } else if (t == proto::kT_StoreEnd) {
                if (m.size() >= 2) credits = std::atoi(m[1].c_str());
                net_.credits = credits;
            } else if (t == proto::kT_StoreBought) {
                if (m.size() < 3) continue;
                int boughtId = std::atoi(m[1].c_str());
                int nc       = std::atoi(m[2].c_str());
                credits = nc;
                net_.credits = nc;
                // Same wire format for guns AND throwables — match by ID space.
                for (auto& it : guns) if (it.id == boughtId) it.owned = true;
                if (throwables::lookup(boughtId)) ownedThrowables.insert(boughtId);
                toast = "Purchased!";
                toastColor = GREEN;
                toastUntil = GetTime() + 2.0;
            } else if (t == proto::kT_WeaponOk) {
                if (m.size() < 2) continue;
                int wid = std::atoi(m[1].c_str());
                net_.selectedWeapon = wid;
                for (auto& it : guns) it.selected = (it.id == wid);
                toast = "Loadout updated";
                toastColor = SKYBLUE;
                toastUntil = GetTime() + 2.0;
            } else if (t == proto::kT_Err) {
                std::string em = (m.size() >= 2) ? m[1] : std::string("error");
                toast = em;
                toastColor = RED;
                toastUntil = GetTime() + 2.0;
            }
        }
        net_.drainUdp();
        if (!net_.isConnected()) { done = true; break; }
        if (IsKeyPressed(KEY_ESCAPE)) { done = true; break; }

        BeginDrawing();
        ClearBackground({ 18, 22, 32, 255 });

        // -------- Title + credits --------
        const char* title = "STORE";
        int titleFs = 36;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (kWinW - tw) / 2, 10, titleFs, GOLD);
        char creditStr[64];
        std::snprintf(creditStr, sizeof(creditStr), "Credits: %d", credits);
        int cFs = 18;
        int cW = ui::measureTextBold(creditStr, cFs);
        ui::drawTextBold(creditStr, (kWinW - cW) / 2, 52, cFs, GOLD);

        // -------- Tabs --------
        const char* tabNames[4] = { "Guns", "Grenades", "Flashbangs", "Bombs" };
        const int tabY = 78;
        const int tabH = 32;
        const int tabW = 170;
        const int tabGap = 8;
        const int tabsTotalW = 4 * tabW + 3 * tabGap;
        const int tabX0 = (kWinW - tabsTotalW) / 2;
        for (int i = 0; i < 4; ++i) {
            int x = tabX0 + i * (tabW + tabGap);
            Vector2 mp = GetMousePosition();
            bool hover = mp.x >= x && mp.x <= x + tabW && mp.y >= tabY && mp.y <= tabY + tabH;
            bool isActive = (activeTab == i);
            Color base = isActive
                ? Color{ 110, 130, 200, 255 }
                : (hover ? Color{ 60, 70, 100, 255 } : Color{ 40, 48, 70, 255 });
            DrawRectangle(x, tabY, tabW, tabH, base);
            DrawRectangleLines(x, tabY, tabW, tabH, BLACK);
            int fs = 17;
            int lw = ui::measureTextBold(tabNames[i], fs);
            ui::drawTextBold(tabNames[i], x + (tabW - lw) / 2, tabY + (tabH - fs) / 2, fs,
                             isActive ? RAYWHITE : Color{ 200, 210, 230, 255 });
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                activeTab = i;
            }
        }

        // -------- List region (clipped, scrollable) --------
        const int listX = 20;
        const int listY = 122;
        const int listW = kWinW - 40;
        const int listH = kWinH - listY - 70; // leave room for back button
        DrawRectangle(listX, listY, listW, listH, { 14, 17, 26, 255 });
        DrawRectangleLines(listX, listY, listW, listH, { 50, 60, 88, 255 });

        const int rowH = 76;
        const int rowSpacing = 6;
        const int rowFullH = rowH + rowSpacing;
        int totalRows = 0;
        if (activeTab == TAB_GUNS) {
            totalRows = (int)guns.size();
        } else {
            ThrowableKind k = (activeTab == TAB_GRENADES)   ? ThrowableKind::Grenade
                            : (activeTab == TAB_FLASHBANGS) ? ThrowableKind::Flashbang
                                                            : ThrowableKind::Bomb;
            totalRows = (int)throwables::ofKind(k).size();
        }
        int contentH = totalRows * rowFullH;
        int maxScroll = std::max(0, contentH - listH + 12);

        Vector2 mp = GetMousePosition();
        bool mouseInList = mp.x >= listX && mp.x <= listX + listW
                           && mp.y >= listY && mp.y <= listY + listH;
        // Wheel
        float wheel = GetMouseWheelMove();
        if (mouseInList && wheel != 0.0f) {
            scrollOffset[activeTab] -= wheel * (float)rowFullH * 1.5f;
        }
        // Drag-to-scroll
        if (mouseInList && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Only start dragging if click landed in empty (non-button) space —
            // we approximate that by ignoring the rightmost button column.
            int btnColW = 180;
            if (mp.x < listX + listW - btnColW) {
                draggingScroll = true;
                dragLastY = mp.y;
            }
        }
        if (draggingScroll) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                float dy = mp.y - dragLastY;
                scrollOffset[activeTab] -= dy;
                dragLastY = mp.y;
            } else {
                draggingScroll = false;
            }
        }
        if (scrollOffset[activeTab] < 0.0f) scrollOffset[activeTab] = 0.0f;
        if (scrollOffset[activeTab] > (float)maxScroll) scrollOffset[activeTab] = (float)maxScroll;

        // BeginScissorMode clips drawing to the list region.
        BeginScissorMode(listX, listY, listW, listH);

        int scrollI = (int)scrollOffset[activeTab];

        if (activeTab == TAB_GUNS) {
            std::sort(guns.begin(), guns.end(),
                      [](const GunRow& a, const GunRow& b) { return a.id < b.id; });
            for (size_t i = 0; i < guns.size(); ++i) {
                const GunRow& it = guns[i];
                int rowX = listX + 6;
                int rowW = listW - 12;
                int y = listY + 6 + (int)i * rowFullH - scrollI;
                if (y + rowH < listY || y > listY + listH) continue;
                Color bg = it.selected ? Color{ 60, 60, 30, 255 }
                                       : (i % 2 == 0 ? Color{ 30, 35, 50, 255 } : Color{ 26, 30, 44, 255 });
                DrawRectangle(rowX, y, rowW, rowH, bg);
                DrawRectangleLines(rowX, y, rowW, rowH, { 70, 80, 110, 255 });

                // Caliber pill (left)
                int pillW = 48, pillH = 22;
                int pillX = rowX + 10;
                int pillY = y + 10;
                Color pillCol = caliberColor(it.caliberMm);
                DrawRectangle(pillX, pillY, pillW, pillH, pillCol);
                DrawRectangleLines(pillX, pillY, pillW, pillH, BLACK);
                const char* clab = caliberLabel(it.caliberMm);
                int lFs = 13;
                int lw = ui::measureTextBold(clab, lFs);
                ui::drawTextBold(clab, pillX + (pillW - lw) / 2, pillY + (pillH - lFs) / 2, lFs, BLACK);

                ui::drawTextBold(it.name.c_str(), rowX + 68, y + 8, 21, RAYWHITE);
                char priceBuf[64];
                if (it.owned) {
                    std::snprintf(priceBuf, sizeof(priceBuf), "OWNED");
                    ui::drawText(priceBuf, rowX + 68, y + 38, 14, GREEN);
                } else {
                    std::snprintf(priceBuf, sizeof(priceBuf), "Price: $%d", it.price);
                    Color pc = (credits >= it.price) ? GOLD : LIGHTGRAY;
                    ui::drawText(priceBuf, rowX + 68, y + 38, 14, pc);
                }
                char stats[160];
                float cooldownSec = it.cooldownMs / 1000.0f;
                std::snprintf(stats, sizeof(stats),
                              "Body %d  HS %d  Mag %d  Rsv %d  CD %.2fs",
                              it.dmgBody, it.dmgHs, it.mag, it.reserve, cooldownSec);
                ui::drawText(stats, rowX + 68, y + 56, 13, { 200, 210, 230, 255 });

                int btnW = 160, btnH = 30;
                int btnX = rowX + rowW - btnW - 10;
                int btnY = y + (rowH - btnH) / 2;
                if (it.selected) {
                    drawButton("[ EQUIPPED ]", btnX, btnY, btnW, btnH, { 110, 100, 30, 255 }, false);
                    int fs = 16;
                    const char* lab = "[ EQUIPPED ]";
                    int lwx = ui::measureTextBold(lab, fs);
                    ui::drawTextBold(lab, btnX + (btnW - lwx) / 2, btnY + (btnH - fs) / 2, fs, YELLOW);
                } else if (it.owned) {
                    if (drawButton("EQUIP", btnX, btnY, btnW, btnH, { 50, 90, 160, 255 }, true)) {
                        net_.sendTcp({ proto::kT_WeaponSelect, std::to_string(it.id) });
                    }
                } else if (credits >= it.price) {
                    char buyLab[64];
                    std::snprintf(buyLab, sizeof(buyLab), "BUY $%d", it.price);
                    if (drawButton(buyLab, btnX, btnY, btnW, btnH, { 50, 130, 60, 255 }, true)) {
                        net_.sendTcp({ proto::kT_StoreBuy, std::to_string(it.id) });
                    }
                } else {
                    char poorLab[64];
                    std::snprintf(poorLab, sizeof(poorLab), "$%d (need more)", it.price);
                    drawButton(poorLab, btnX, btnY, btnW, btnH, { 70, 70, 70, 255 }, false);
                }
            }
            if (guns.empty()) {
                const char* none = "Loading store...";
                int nw = ui::measureText(none, 18);
                EndScissorMode();
                ui::drawText(none, listX + (listW - nw) / 2, listY + listH / 2, 18, LIGHTGRAY);
                BeginScissorMode(listX, listY, listW, listH);
            }
        } else {
            ThrowableKind kind = (activeTab == TAB_GRENADES)   ? ThrowableKind::Grenade
                               : (activeTab == TAB_FLASHBANGS) ? ThrowableKind::Flashbang
                                                               : ThrowableKind::Bomb;
            auto rows = buildThrowableRows(kind, ownedThrowables, equippedGrenades, equippedBombId);
            for (size_t i = 0; i < rows.size(); ++i) {
                const ThrowableRow& it = rows[i];
                int rowX = listX + 6;
                int rowW = listW - 12;
                int y = listY + 6 + (int)i * rowFullH - scrollI;
                if (y + rowH < listY || y > listY + listH) continue;
                Color bg = it.equipped ? Color{ 60, 60, 30, 255 }
                                       : (i % 2 == 0 ? Color{ 30, 35, 50, 255 } : Color{ 26, 30, 44, 255 });
                DrawRectangle(rowX, y, rowW, rowH, bg);
                DrawRectangleLines(rowX, y, rowW, rowH, { 70, 80, 110, 255 });

                // Type badge
                int pillW = 56, pillH = 22;
                int pillX = rowX + 10;
                int pillY = y + 10;
                Color pillCol = throwableTypeColor(it.kind);
                DrawRectangle(pillX, pillY, pillW, pillH, pillCol);
                DrawRectangleLines(pillX, pillY, pillW, pillH, BLACK);
                const char* tlab = throwableTypeBadge(it.kind);
                int lFs = 13;
                int lw = ui::measureTextBold(tlab, lFs);
                ui::drawTextBold(tlab, pillX + (pillW - lw) / 2, pillY + (pillH - lFs) / 2, lFs, BLACK);

                ui::drawTextBold(it.name.c_str(), rowX + 76, y + 8, 20, RAYWHITE);
                char priceBuf[64];
                if (it.owned) {
                    std::snprintf(priceBuf, sizeof(priceBuf), "OWNED");
                    ui::drawText(priceBuf, rowX + 76, y + 38, 14, GREEN);
                } else {
                    std::snprintf(priceBuf, sizeof(priceBuf), "Price: $%d", it.price);
                    Color pc = (credits >= it.price) ? GOLD : LIGHTGRAY;
                    ui::drawText(priceBuf, rowX + 76, y + 38, 14, pc);
                }
                char stats[200];
                const char* fuseDesc = "fuse";
                char fuseStr[24];
                if (it.fuseSec < 0.0f) {
                    std::snprintf(fuseStr, sizeof(fuseStr), "contact");
                    fuseDesc = "trigger";
                } else if (it.fuseSec == 0.0f && it.kind == ThrowableKind::Bomb) {
                    std::snprintf(fuseStr, sizeof(fuseStr), "proximity");
                    fuseDesc = "trigger";
                } else {
                    std::snprintf(fuseStr, sizeof(fuseStr), "%.1fs", it.fuseSec);
                }
                std::snprintf(stats, sizeof(stats),
                              "Dmg %.0f  Radius %.1fm  %s %s",
                              it.maxDamage, it.radiusM, fuseDesc, fuseStr);
                ui::drawText(stats, rowX + 76, y + 56, 13, { 200, 210, 230, 255 });

                int btnW = 160, btnH = 30;
                int btnX = rowX + rowW - btnW - 10;
                int btnY = y + (rowH - btnH) / 2;
                if (it.equipped) {
                    drawButton("[ EQUIPPED ]", btnX, btnY, btnW, btnH, { 110, 100, 30, 255 }, false);
                    int fs = 16;
                    const char* lab = "[ EQUIPPED ]";
                    int lwx = ui::measureTextBold(lab, fs);
                    ui::drawTextBold(lab, btnX + (btnW - lwx) / 2, btnY + (btnH - fs) / 2, fs, YELLOW);
                } else if (it.owned) {
                    // Equip locally until the server inventory schema lands.
                    // TODO: server inventory schema — wire to a real
                    //       kT_ThrowableSelect message once defined.
                    if (drawButton("EQUIP", btnX, btnY, btnW, btnH, { 50, 90, 160, 255 }, true)) {
                        if (it.kind == ThrowableKind::Bomb) {
                            equippedBombId = it.id;
                        } else {
                            // Grenade/Flashbang share the kMaxGrenadeSlots slot pool.
                            auto found = std::find(equippedGrenades.begin(), equippedGrenades.end(), it.id);
                            if (found != equippedGrenades.end()) {
                                equippedGrenades.erase(found);
                            } else {
                                if ((int)equippedGrenades.size() >= kMaxGrenadeSlots) {
                                    equippedGrenades.erase(equippedGrenades.begin());
                                }
                                equippedGrenades.push_back(it.id);
                            }
                        }
                        toast = "Equipped (client-side)";
                        toastColor = SKYBLUE;
                        toastUntil = GetTime() + 2.0;
                    }
                } else if (credits >= it.price) {
                    char buyLab[64];
                    std::snprintf(buyLab, sizeof(buyLab), "BUY $%d", it.price);
                    if (drawButton(buyLab, btnX, btnY, btnW, btnH, { 50, 130, 60, 255 }, true)) {
                        credits -= it.price;
                        net_.credits = credits;
                        ownedThrowables.insert(it.id);
                        toast = "Purchased throwable";
                        toastColor = GREEN;
                        toastUntil = GetTime() + 2.0;
                    }
                } else {
                    char poorLab[64];
                    std::snprintf(poorLab, sizeof(poorLab), "$%d (need more)", it.price);
                    drawButton(poorLab, btnX, btnY, btnW, btnH, { 70, 70, 70, 255 }, false);
                }
            }
        }

        EndScissorMode();

        // -------- Scrollbar (visual hint) --------
        if (maxScroll > 0) {
            int sbX = listX + listW - 6;
            int sbY = listY + 4;
            int sbH = listH - 8;
            DrawRectangle(sbX, sbY, 4, sbH, { 40, 48, 70, 255 });
            float ratio = (float)listH / (float)contentH;
            if (ratio > 1.0f) ratio = 1.0f;
            int thumbH = std::max(20, (int)(sbH * ratio));
            float pos = scrollOffset[activeTab] / (float)maxScroll;
            int thumbY = sbY + (int)(pos * (sbH - thumbH));
            DrawRectangle(sbX, thumbY, 4, thumbH, { 120, 140, 200, 255 });
        }

        // -------- Slot summary for throwable tabs --------
        if (activeTab != TAB_GUNS) {
            char summary[160];
            if (activeTab == TAB_BOMBS) {
                if (equippedBombId == 0) {
                    std::snprintf(summary, sizeof(summary), "Bomb slot: empty");
                } else if (const Throwable* tb = throwables::lookup(equippedBombId)) {
                    std::snprintf(summary, sizeof(summary), "Bomb slot: %s", tb->name.c_str());
                }
            } else {
                std::snprintf(summary, sizeof(summary),
                              "Grenade slots: %d / %d  (carry up to %d throwables)",
                              (int)equippedGrenades.size(), kMaxGrenadeSlots, kMaxGrenadeSlots);
            }
            ui::drawText(summary, listX + 6, listY + listH + 6, 13, { 200, 210, 230, 255 });
        }

        // -------- Toast --------
        if (GetTime() < toastUntil) {
            int fs = 18;
            int mw = ui::measureTextBold(toast.c_str(), fs);
            int bx = (kWinW - mw - 24) / 2;
            int by = kWinH - 90;
            DrawRectangle(bx, by, mw + 24, 30, { 0, 0, 0, 200 });
            DrawRectangleLines(bx, by, mw + 24, 30, toastColor);
            ui::drawTextBold(toast.c_str(), bx + 12, by + 6, fs, toastColor);
        }

        if (drawBackButton(20, kWinH - 50, 120, 36)) {
            done = true;
        }
        ui::drawText("ESC to return  •  wheel/drag to scroll", 160, kWinH - 42, 13, GRAY);

        EndDrawing();
    }
    CloseWindow();
}
