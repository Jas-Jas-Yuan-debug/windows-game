#include "Store.h"
#include "Protocol.h"
#include "UiTheme.h"
#include "Weapon.h"
#include <raylib.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
namespace {
constexpr int kWinW = 800;
constexpr int kWinH = 600;
struct ItemRow {
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
};
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
    int fs = 18;
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
}
Store::Store(NetClient& net) : net_(net) {}
void Store::run() {
    InitWindow(kWinW, kWinH, "ClaudeGame - Store");
    SetTargetFPS(60);
    SetExitKey(0);
    ui::loadFonts();
    std::vector<ItemRow> items;
    int credits = net_.credits;
    int selectedWeapon = net_.selectedWeapon;
    std::string toast;
    double toastUntil = 0.0;
    Color toastColor = RED;
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
                ItemRow r;
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
                bool replaced = false;
                for (auto& existing : items) {
                    if (existing.id == r.id) { existing = r; replaced = true; break; }
                }
                if (!replaced) items.push_back(std::move(r));
                if (items.back().selected) selectedWeapon = items.back().id;
            } else if (t == proto::kT_StoreEnd) {
                if (m.size() >= 2) credits = std::atoi(m[1].c_str());
                net_.credits = credits;
            } else if (t == proto::kT_StoreBought) {
                if (m.size() < 3) continue;
                int wid = std::atoi(m[1].c_str());
                int nc  = std::atoi(m[2].c_str());
                credits = nc;
                net_.credits = nc;
                for (auto& it : items) if (it.id == wid) it.owned = true;
                toast = "Purchased!";
                toastColor = GREEN;
                toastUntil = GetTime() + 2.0;
            } else if (t == proto::kT_WeaponOk) {
                if (m.size() < 2) continue;
                int wid = std::atoi(m[1].c_str());
                selectedWeapon = wid;
                net_.selectedWeapon = wid;
                for (auto& it : items) it.selected = (it.id == wid);
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
        const char* title = "STORE";
        int titleFs = 44;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (kWinW - tw) / 2, 16, titleFs, GOLD);
        char creditStr[64];
        std::snprintf(creditStr, sizeof(creditStr), "Credits: %d", credits);
        int cFs = 22;
        int cW = ui::measureTextBold(creditStr, cFs);
        ui::drawTextBold(creditStr, (kWinW - cW) / 2, 66, cFs, GOLD);
        std::sort(items.begin(), items.end(),
                  [](const ItemRow& a, const ItemRow& b) { return a.id < b.id; });
        int rowY0 = 110;
        int rowH = 78;
        int rowSpacing = 6;
        int rowX = 20;
        int rowW = kWinW - 40;
        for (size_t i = 0; i < items.size(); ++i) {
            const ItemRow& it = items[i];
            int y = rowY0 + (int)i * (rowH + rowSpacing);
            Color bg = it.selected ? Color{ 60, 60, 30, 255 }
                                   : (i % 2 == 0 ? Color{ 30, 35, 50, 255 } : Color{ 26, 30, 44, 255 });
            DrawRectangle(rowX, y, rowW, rowH, bg);
            DrawRectangleLines(rowX, y, rowW, rowH, { 70, 80, 110, 255 });
            ui::drawTextBold(it.name.c_str(), rowX + 14, y + 8, 24, RAYWHITE);
            char priceBuf[64];
            if (it.owned) {
                std::snprintf(priceBuf, sizeof(priceBuf), "OWNED");
                ui::drawText(priceBuf, rowX + 14, y + 40, 16, GREEN);
            } else {
                std::snprintf(priceBuf, sizeof(priceBuf), "Price: $%d", it.price);
                Color pc = (credits >= it.price) ? GOLD : LIGHTGRAY;
                ui::drawText(priceBuf, rowX + 14, y + 40, 16, pc);
            }
            char stats[160];
            float cooldownSec = it.cooldownMs / 1000.0f;
            std::snprintf(stats, sizeof(stats),
                          "Body %d  HS %d  Mag %d  Reserve %d  Cooldown %.2fs",
                          it.dmgBody, it.dmgHs, it.mag, it.reserve, cooldownSec);
            ui::drawText(stats, rowX + 160, y + 44, 15, { 200, 210, 230, 255 });
            int btnW = 170, btnH = 36;
            int btnX = rowX + rowW - btnW - 14;
            int btnY = y + (rowH - btnH) / 2;
            if (it.selected) {
                drawButton("[ SELECTED ]", btnX, btnY, btnW, btnH, { 110, 100, 30, 255 }, false);
                int fs = 18;
                const char* lab = "[ SELECTED ]";
                int lw = ui::measureTextBold(lab, fs);
                ui::drawTextBold(lab, btnX + (btnW - lw) / 2, btnY + (btnH - fs) / 2, fs, YELLOW);
            } else if (it.owned) {
                if (drawButton("SELECT", btnX, btnY, btnW, btnH, { 50, 90, 160, 255 }, true)) {
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
                std::snprintf(poorLab, sizeof(poorLab), "$%d", it.price);
                drawButton(poorLab, btnX, btnY, btnW, btnH, { 70, 70, 70, 255 }, false);
            }
        }
        if (items.empty()) {
            const char* none = "Loading store...";
            int nw = ui::measureText(none, 22);
            ui::drawText(none, (kWinW - nw) / 2, kWinH / 2, 22, LIGHTGRAY);
        }
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
        ui::drawText("ESC to return", 160, kWinH - 42, 14, GRAY);
        EndDrawing();
    }
    CloseWindow();
}
