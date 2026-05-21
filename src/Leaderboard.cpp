#include "Leaderboard.h"
#include "LeaderEntry.h"
#include "Protocol.h"
#include "UiTheme.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
Leaderboard::Leaderboard(NetClient& net) : net_(net) {}
namespace {
bool drawTabButton(const char* text, int x, int y, int w, int h, bool active) {
    Vector2 mp = GetMousePosition();
    bool hover = mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color base = active ? Color{ 80, 110, 170, 255 } : Color{ 40, 50, 70, 255 };
    if (hover && !active) base = { 60, 75, 110, 255 };
    DrawRectangle(x, y, w, h, base);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 18;
    int tw = ui::measureTextBold(text, fs);
    ui::drawTextBold(text, x + (w - tw) / 2, y + (h - fs) / 2, fs, RAYWHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
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
const char* requestType(int tab) {
    switch (tab) {
        case 0: return proto::kT_LeaderXP;
        case 1: return proto::kT_LeaderKills;
        case 2: return proto::kT_LeaderWin;
        default: return proto::kT_LeaderXP;
    }
}
const char* kindName(int tab) { return requestType(tab); }
}
void Leaderboard::run() {
    InitWindow(800, 600, "ClaudeGame - Leaderboard");
    SetTargetFPS(60);
    SetExitKey(0);
    ui::loadFonts();
    int tab = 0;
    std::vector<LeaderEntry> entries;
    bool loading = true;
    auto request = [&](int t) {
        entries.clear();
        loading = true;
        net_.sendTcp({ requestType(t) });
    };
    request(tab);
    std::string myName = net_.username;
    bool done = false;
    while (!WindowShouldClose() && !done) {
        net_.poll();
        auto msgs = net_.drainTcp();
        for (auto& m : msgs) {
            if (m.empty()) continue;
            if (m[0] == proto::kT_LeaderRow) {
                if (m.size() < 12) continue;
                if (m[1] != kindName(tab)) continue;
                LeaderEntry e;
                e.username        = m[3];
                e.level           = std::atoi(m[4].c_str());
                e.xp              = std::atoi(m[5].c_str());
                e.totalKills      = std::atoi(m[6].c_str());
                e.totalDeaths     = std::atoi(m[7].c_str());
                e.matchesPlayed   = std::atoi(m[8].c_str());
                e.matchesWon      = std::atoi(m[9].c_str());
                e.totalHeadshots  = std::atoi(m[10].c_str());
                e.rank            = m[11];
                e.kd = (double)e.totalKills / (double)std::max(1, e.totalDeaths);
                e.winRate = e.matchesPlayed > 0
                    ? (double)e.matchesWon / (double)e.matchesPlayed : 0.0;
                entries.push_back(std::move(e));
            } else if (m[0] == proto::kT_LeaderEnd) {
                if (m.size() >= 2 && m[1] == kindName(tab)) loading = false;
            }
        }
        net_.drainUdp();
        if (!net_.isConnected()) { done = true; break; }
        if (IsKeyPressed(KEY_ESCAPE)) { done = true; break; }
        if (IsKeyPressed(KEY_TAB)) {
            tab = (tab + 1) % 3;
            request(tab);
        }
        BeginDrawing();
        ClearBackground({ 18, 22, 32, 255 });
        const char* title = "LEADERBOARD";
        int titleFs = 36;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (800 - tw) / 2, 18, titleFs, GOLD);
        const char* labels[3] = { "By XP", "By Kills", "By Win Rate" };
        int tabW = 200, tabH = 36;
        int tabsTotal = 3 * tabW + 2 * 10;
        int tabStartX = (800 - tabsTotal) / 2;
        int tabY = 70;
        for (int i = 0; i < 3; ++i) {
            int x = tabStartX + i * (tabW + 10);
            if (drawTabButton(labels[i], x, tabY, tabW, tabH, tab == i)) {
                if (tab != i) {
                    tab = i;
                    request(tab);
                }
            }
        }
        int tableX = 20;
        int tableY = 130;
        int rowH = 30;
        DrawRectangle(tableX, tableY, 760, rowH, { 40, 50, 70, 255 });
        int cx[11] = { 10, 50, 180, 230, 320, 380, 425, 470, 525, 590, 660 };
        const char* hdrs[11] = { "#", "Name", "Lv", "Rank", "XP", "K", "D", "K/D", "Mat", "Win%", "HS" };
        int hdrFs = 16;
        for (int i = 0; i < 11; ++i) {
            ui::drawTextBold(hdrs[i], tableX + cx[i], tableY + (rowH - hdrFs) / 2, hdrFs, RAYWHITE);
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            const LeaderEntry& e = entries[i];
            int y = tableY + rowH + (int)i * rowH;
            bool isMe = !myName.empty() && e.username == myName;
            Color rowCol = isMe ? Color{ 90, 80, 30, 255 }
                                : ((i % 2 == 0) ? Color{ 28, 32, 46, 255 } : Color{ 22, 26, 38, 255 });
            DrawRectangle(tableX, y, 760, rowH, rowCol);
            Color tc = isMe ? YELLOW : RAYWHITE;
            int fs = 15;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%zu", i + 1);
            ui::drawText(buf, tableX + cx[0], y + (rowH - fs)/2, fs, tc);
            ui::drawText(e.username.c_str(), tableX + cx[1], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%d", e.level);
            ui::drawText(buf, tableX + cx[2], y + (rowH - fs)/2, fs, tc);
            ui::drawText(e.rank.c_str(), tableX + cx[3], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%d", e.xp);
            ui::drawText(buf, tableX + cx[4], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%d", e.totalKills);
            ui::drawText(buf, tableX + cx[5], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%d", e.totalDeaths);
            ui::drawText(buf, tableX + cx[6], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%.2f", e.kd);
            ui::drawText(buf, tableX + cx[7], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%d", e.matchesPlayed);
            ui::drawText(buf, tableX + cx[8], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%.0f%%", e.winRate * 100.0);
            ui::drawText(buf, tableX + cx[9], y + (rowH - fs)/2, fs, tc);
            std::snprintf(buf, sizeof(buf), "%d", e.totalHeadshots);
            ui::drawText(buf, tableX + cx[10], y + (rowH - fs)/2, fs, tc);
        }
        if (entries.empty()) {
            const char* none = loading ? "Loading..." : "No entries yet";
            int nw = ui::measureText(none, 20);
            ui::drawText(none, (800 - nw) / 2, 300, 20, LIGHTGRAY);
        }
        if (drawBackButton(20, 540, 120, 40)) {
            done = true;
        }
        ui::drawText("TAB to switch tabs   ESC to return", 160, 552, 16, GRAY);
        EndDrawing();
    }
    CloseWindow();
}
