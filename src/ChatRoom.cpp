#include "ChatRoom.h"
#include "ChatMessage.h"
#include "Protocol.h"
#include "UiTheme.h"
#include <raylib.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
ChatRoom::ChatRoom(NetClient& net) : net_(net) {}
namespace {
constexpr int kScreenW = 800;
constexpr int kScreenH = 600;
constexpr int kMaxInput = 280;
struct TabSpec {
    const char* label;
    const char* room;
    Color accent;
};
bool drawTabButton(const char* text, int x, int y, int w, int h, bool active, bool enabled) {
    Vector2 mp = GetMousePosition();
    bool hover = enabled && mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color base;
    if (!enabled)      base = { 35, 35, 40, 255 };
    else if (active)   base = { 80, 110, 170, 255 };
    else if (hover)    base = { 60, 75, 110, 255 };
    else               base = { 40, 50, 70, 255 };
    DrawRectangle(x, y, w, h, base);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 18;
    int tw = ui::measureTextBold(text, fs);
    Color tc = enabled ? RAYWHITE : Color{ 110, 110, 110, 255 };
    ui::drawTextBold(text, x + (w - tw) / 2, y + (h - fs) / 2, fs, tc);
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
std::string formatTime(long long unixSeconds) {
    std::time_t t = static_cast<std::time_t>(unixSeconds);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    return std::string(buf);
}
}
void ChatRoom::run() {
    InitWindow(kScreenW, kScreenH, "ClaudeGame - Chat");
    SetTargetFPS(60);
    SetExitKey(0);
    ui::loadFonts();
    std::array<TabSpec, 3> tabs = {{
        { "PUBLIC",    "public",    Color{ 110, 110, 120, 255 } },
        { "TEAM BLUE", "team_blue", Color{ 40,  80,  200, 255 } },
        { "TEAM RED",  "team_red",  Color{ 200, 50,  50,  255 } },
    }};
    auto isAccessible = [&](int i) {
        if (i == 0) return true;
        const std::string& team = net_.team;
        if (i == 1) return team == "BLUE";
        if (i == 2) return team == "RED";
        return false;
    };
    std::array<std::vector<ChatMessage>, 3> roomMsgs;
    bool inBatch = false;
    std::string batchRoom;
    std::vector<ChatMessage> batchBuffer;
    auto roomIndexFor = [&](const std::string& roomName) -> int {
        for (int i = 0; i < 3; ++i) {
            if (roomName == tabs[(size_t)i].room) return i;
        }
        return -1;
    };
    int activeTab = 0;
    for (int i = 1; i < 3; ++i) {
        if (isAccessible(i)) { activeTab = i; break; }
    }
    auto requestHistory = [&](int t) {
        roomMsgs[(size_t)t].clear();
        net_.sendTcp({ proto::kT_ChatHistory, tabs[(size_t)t].room });
    };
    for (int i = 0; i < 3; ++i) {
        if (isAccessible(i)) requestHistory(i);
    }
    float scrollOffset = 0.0f;
    bool followBottom = true;
    std::string input;
    double rejectedAt = -10.0;
    auto switchTab = [&](int newTab) {
        if (newTab == activeTab) return;
        if (!isAccessible(newTab)) return;
        activeTab = newTab;
        scrollOffset = 0.0f;
        followBottom = true;
    };
    const int panelX = 20;
    const int panelY = 130;
    const int panelW = kScreenW - 40;
    const int panelH = 350;
    const int lineH = 20;
    const int lineFs = 16;
    const int inputY = panelY + panelH + 15;
    const int inputH = 40;
    const int inputX = panelX;
    const int inputW = panelW - 80;
    bool done = false;
    while (!WindowShouldClose() && !done) {
        net_.poll();
        auto msgs = net_.drainTcp();
        for (auto& m : msgs) {
            if (m.empty()) continue;
            const std::string& t = m[0];
            if (t == proto::kT_ChatBatchBeg) {
                if (m.size() >= 2) {
                    inBatch = true;
                    batchRoom = m[1];
                    batchBuffer.clear();
                }
            } else if (t == proto::kT_ChatBatchEnd) {
                if (inBatch && m.size() >= 2 && m[1] == batchRoom) {
                    int idx = roomIndexFor(batchRoom);
                    if (idx >= 0) roomMsgs[(size_t)idx] = std::move(batchBuffer);
                    batchBuffer.clear();
                    inBatch = false;
                    if (idx == activeTab) { followBottom = true; scrollOffset = 0.0f; }
                }
            } else if (t == proto::kT_ChatMsg) {
                if (m.size() < 5) continue;
                ChatMessage cm;
                cm.room = m[1];
                cm.username = m[2];
                cm.content = m[3];
                cm.sentAt = std::atoll(m[4].c_str());
                if (inBatch && cm.room == batchRoom) {
                    batchBuffer.push_back(std::move(cm));
                } else {
                    int idx = roomIndexFor(cm.room);
                    if (idx >= 0) {
                        roomMsgs[(size_t)idx].push_back(std::move(cm));
                        if (idx == activeTab && followBottom) {
                        }
                    }
                }
            }
        }
        net_.drainUdp();
        if (!net_.isConnected()) { done = true; break; }
        if (IsKeyPressed(KEY_ESCAPE)) { done = true; break; }
        if (IsKeyPressed(KEY_TAB)) {
            for (int step = 1; step < 3; ++step) {
                int cand = (activeTab + step) % 3;
                if (isAccessible(cand)) { switchTab(cand); break; }
            }
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            scrollOffset -= wheel * lineH * 3.0f;
            followBottom = false;
        }
        int ch = GetCharPressed();
        while (ch > 0) {
            if (ch >= 32 && ch < 127 && (int)input.size() < kMaxInput) {
                input.push_back(static_cast<char>(ch));
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !input.empty()) {
            input.pop_back();
        }
        static double lastBack = 0.0;
        if (IsKeyDown(KEY_BACKSPACE) && !input.empty()) {
            double now = GetTime();
            if (now - lastBack > 0.05 && !IsKeyPressed(KEY_BACKSPACE)) {
                if (now - lastBack > 0.4) {
                    input.pop_back();
                    lastBack = now - 0.35;
                }
            }
            if (IsKeyPressed(KEY_BACKSPACE)) lastBack = now;
        }
        if (IsKeyPressed(KEY_ENTER) && !input.empty()) {
            if (isAccessible(activeTab)) {
                net_.sendTcp({ proto::kT_ChatSend, tabs[(size_t)activeTab].room, input });
                input.clear();
                followBottom = true;
                scrollOffset = 0.0f;
            } else {
                rejectedAt = GetTime();
            }
        }
        auto& active = roomMsgs[(size_t)activeTab];
        int totalContentH = static_cast<int>(active.size()) * lineH;
        int viewportH = panelH - 16;
        int maxScroll = std::max(0, totalContentH - viewportH);
        if (followBottom) scrollOffset = static_cast<float>(maxScroll);
        if (scrollOffset < 0.0f) scrollOffset = 0.0f;
        if (scrollOffset > maxScroll) scrollOffset = static_cast<float>(maxScroll);
        if (static_cast<int>(scrollOffset) >= maxScroll) followBottom = true;
        BeginDrawing();
        ClearBackground({ 18, 22, 32, 255 });
        Color accent = tabs[(size_t)activeTab].accent;
        DrawRectangle(0, 0, kScreenW, 60, accent);
        const char* title = "CHAT";
        int titleFs = 32;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (kScreenW - tw) / 2, 14, titleFs, RAYWHITE);
        const std::string& teamStr = net_.team;
        if (!teamStr.empty()) {
            char tb[48];
            std::snprintf(tb, sizeof(tb), "You: Team %s", teamStr.c_str());
            int tbw = ui::measureText(tb, 14);
            ui::drawText(tb, kScreenW - tbw - 12, 8, 14, RAYWHITE);
        }
        int tabW = 200, tabH = 40;
        int tabsTotal = 3 * tabW + 2 * 10;
        int tabStartX = (kScreenW - tabsTotal) / 2;
        int tabY = 75;
        for (int i = 0; i < 3; ++i) {
            int x = tabStartX + i * (tabW + 10);
            bool acc = isAccessible(i);
            if (drawTabButton(tabs[(size_t)i].label, x, tabY, tabW, tabH, activeTab == i, acc)) {
                switchTab(i);
            }
        }
        DrawRectangle(panelX, panelY, panelW, panelH, { 24, 28, 40, 255 });
        DrawRectangleLines(panelX, panelY, panelW, panelH, { 80, 90, 120, 255 });
        BeginScissorMode(panelX + 1, panelY + 1, panelW - 2, panelH - 2);
        int yStart = panelY + 8 - static_cast<int>(scrollOffset);
        for (size_t i = 0; i < active.size(); ++i) {
            const ChatMessage& mm = active[i];
            int y = yStart + static_cast<int>(i) * lineH;
            if (y + lineH < panelY || y > panelY + panelH) continue;
            std::string ts = formatTime(mm.sentAt);
            std::string head = "[" + ts + "] " + mm.username + ": ";
            int headW = ui::measureText(head.c_str(), lineFs);
            ui::drawText(head.c_str(), panelX + 10, y, lineFs, { 170, 180, 210, 255 });
            ui::drawText(mm.content.c_str(), panelX + 10 + headW, y, lineFs, RAYWHITE);
        }
        EndScissorMode();
        if (active.empty()) {
            const char* none = "No messages yet. Say hi!";
            int nw = ui::measureText(none, 18);
            ui::drawText(none, panelX + (panelW - nw) / 2, panelY + panelH / 2 - 10, 18, LIGHTGRAY);
        }
        DrawRectangle(inputX, inputY, inputW, inputH, { 30, 35, 50, 255 });
        DrawRectangleLines(inputX, inputY, inputW, inputH, { 90, 100, 130, 255 });
        if (input.empty()) {
            ui::drawText("Type a message...", inputX + 8, inputY + (inputH - 16) / 2, 16, { 110, 115, 130, 255 });
        } else {
            ui::drawText(input.c_str(), inputX + 8, inputY + (inputH - 16) / 2, 16, RAYWHITE);
        }
        int cursorX = inputX + 8 + ui::measureText(input.c_str(), 16);
        if (((int)(GetTime() * 2)) % 2 == 0) {
            DrawRectangle(cursorX + 1, inputY + 8, 2, inputH - 16, RAYWHITE);
        }
        char cnt[32];
        std::snprintf(cnt, sizeof(cnt), "%d/%d", (int)input.size(), kMaxInput);
        int cntW = ui::measureText(cnt, 16);
        (void)cntW;
        Color cntCol = ((int)input.size() >= kMaxInput) ? Color{ 230, 80, 80, 255 } : LIGHTGRAY;
        ui::drawText(cnt, inputX + inputW + 10, inputY + (inputH - 16) / 2, 16, cntCol);
        if (GetTime() - rejectedAt < 2.0) {
            const char* msg = "Cannot send to this room";
            int mw = ui::measureText(msg, 18);
            int boxW = mw + 24, boxH = 32;
            int bx = (kScreenW - boxW) / 2;
            int by = inputY - boxH - 8;
            DrawRectangle(bx, by, boxW, boxH, { 160, 40, 40, 230 });
            DrawRectangleLines(bx, by, boxW, boxH, BLACK);
            ui::drawText(msg, bx + 12, by + (boxH - 18) / 2, 18, RAYWHITE);
        }
        if (drawBackButton(20, kScreenH - 50, 120, 36)) {
            done = true;
        }
        ui::drawText("TAB switch room   ENTER send   ESC return",
                 160, kScreenH - 42, 14, GRAY);
        EndDrawing();
    }
    CloseWindow();
}
