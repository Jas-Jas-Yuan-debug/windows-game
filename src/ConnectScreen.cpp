#include "ConnectScreen.h"
#include "EmbeddedServer.h"
#include "Paths.h"
#include "Protocol.h"
#include "UiTheme.h"
#include "I18n.h"
#include <raylib.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
namespace {
constexpr int kWinW = 800;
constexpr int kWinH = 600;
bool drawButton(const char* text, int x, int y, int w, int h, Color base) {
    Vector2 mp = GetMousePosition();
    bool hover = mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color c = base;
    if (hover) {
        c.r = (unsigned char)std::min(255, c.r + 30);
        c.g = (unsigned char)std::min(255, c.g + 30);
        c.b = (unsigned char)std::min(255, c.b + 30);
    }
    DrawRectangle(x - 2, y - 2, w + 4, h + 4, BLACK);
    DrawRectangle(x, y, w, h, c);
    int tw = ui::measureTextBold(text, 22);
    ui::drawTextBold(text, x + (w - tw) / 2, y + (h - 22) / 2, 22, RAYWHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
bool drawLangButton(int x, int y, int w, int h) {
    Vector2 mp = GetMousePosition();
    bool hover = mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color base = hover ? Color{ 90, 100, 130, 255 } : Color{ 60, 70, 100, 255 };
    DrawRectangle(x, y, w, h, base);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 16;
    const char* lbl = i18n::langToggleLabel();
    int tw = ui::measureTextBold(lbl, fs);
    ui::drawTextBold(lbl, x + (w - tw) / 2, y + (h - fs) / 2, fs, RAYWHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
void drawField(const char* label, const std::string& v, int x, int y, int w, int h, bool focused) {
    ui::drawText(label, x, y - 22, 18, LIGHTGRAY);
    Color border = focused ? Color{ 220, 60, 60, 255 } : Color{ 90, 90, 100, 255 };
    DrawRectangle(x - 2, y - 2, w + 4, h + 4, border);
    DrawRectangle(x, y, w, h, { 25, 25, 32, 255 });
    ui::drawText(v.c_str(), x + 10, y + (h - 22) / 2, 22, RAYWHITE);
    if (focused) {
        int tw = ui::measureText(v.c_str(), 22);
        if (((int)(GetTime() * 2)) % 2 == 0) {
            DrawRectangle(x + 10 + tw + 2, y + 6, 2, h - 12, RAYWHITE);
        }
    }
}
void handleText(std::string& target, int maxLen, bool digitsOnly) {
    int ch = GetCharPressed();
    while (ch > 0) {
        bool ok = ch >= 32 && ch < 127 && (int)target.size() < maxLen;
        if (digitsOnly && !(ch >= '0' && ch <= '9')) ok = false;
        if (ok) target.push_back((char)ch);
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !target.empty()) target.pop_back();
}
}
ConnectScreen::ConnectScreen(NetClient& net, EmbeddedServer* embedded) : net_(net), embedded_(embedded) {}
bool ConnectScreen::run() {
    InitWindow(kWinW, kWinH, "ClaudeGame - Connect");
    SetTargetFPS(60);
    SetExitKey(0);
    i18n::init();
    ui::loadFonts();
    enum class Focus { Host, Port };
    Focus focus = Focus::Host;
    const int fieldW = 400, fieldH = 40;
    const int fieldX = (kWinW - fieldW) / 2;
    const int hostY = 220;
    const int portY = 300;
    const int btnW = 180, btnH = 44;
    const int connectX = kWinW / 2 - btnW - 12;
    const int quitX = kWinW / 2 + 12;
    const int btnY = 400;
    const int hostBtnW = 280, hostBtnH = 40;
    const int hostBtnX = (kWinW - hostBtnW) / 2;
    const int hostBtnY = 360;
    std::string status;
    std::string statusKey;
    Color statusColor = LIGHTGRAY;
    bool connecting = false;
    double connectStarted = 0.0;
    bool connectedAndHello = false;
    bool quit = false;
    while (!WindowShouldClose() && !connectedAndHello && !quit) {
        if (IsKeyPressed(KEY_ESCAPE)) { quit = true; break; }
        if (IsKeyPressed(KEY_TAB)) {
            focus = (focus == Focus::Host) ? Focus::Port : Focus::Host;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            if (m.x >= fieldX && m.x <= fieldX + fieldW) {
                if (m.y >= hostY && m.y <= hostY + fieldH) focus = Focus::Host;
                else if (m.y >= portY && m.y <= portY + fieldH) focus = Focus::Port;
            }
        }
        if (!connecting) {
            if (focus == Focus::Host) handleText(host_, 64, false);
            else handleText(port_, 6, true);
        }
        bool enterPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        BeginDrawing();
        ClearBackground({ 12, 10, 14, 255 });
        if (drawLangButton(kWinW - 90, 18, 72, 28)) {
            i18n::toggleLanguage();
            ui::loadFonts();
        }
        const char* title = i18n::tr("connect.title");
        int tw = ui::measureTextBold(title, 48);
        ui::drawTextBold(title, (kWinW - tw) / 2, 60, 48, { 220, 60, 60, 255 });
        const char* sub = i18n::tr("connect.sub");
        int sw2 = ui::measureText(sub, 18);
        ui::drawText(sub, (kWinW - sw2) / 2, 130, 18, LIGHTGRAY);
        drawField(i18n::tr("connect.host"), host_, fieldX, hostY, fieldW, fieldH, focus == Focus::Host);
        drawField(i18n::tr("connect.port"), port_, fieldX, portY, fieldW, fieldH, focus == Focus::Port);
        bool clickedHost = embedded_ && drawButton(i18n::tr("connect.btn.host"), hostBtnX, hostBtnY, hostBtnW, hostBtnH, { 80, 60, 140, 255 });
        bool clickedConnect = drawButton(i18n::tr("connect.btn.connect"), connectX, btnY, btnW, btnH, { 50, 120, 60, 255 });
        bool clickedQuit = drawButton(i18n::tr("connect.btn.quit"), quitX, btnY, btnW, btnH, { 140, 50, 50, 255 });
        if (clickedQuit) { quit = true; }
        if (clickedHost && embedded_ && !connecting) {
            if (!embedded_->isRunning()) {
                std::string err;
                std::string dbp = cg::userDataPath("server.sqlite");
                if (!embedded_->start(27015, 5, dbp, err)) {
                    statusKey.clear();
                    status = std::string("host failed: ") + err;
                    statusColor = RED;
                } else {
                    host_ = "127.0.0.1";
                    port_ = "27015";
                    statusKey = "connect.status.hosted";
                    statusColor = SKYBLUE;
                }
            } else {
                host_ = "127.0.0.1";
                port_ = "27015";
            }
        }
        if ((clickedConnect || enterPressed) && !connecting) {
            if (host_.empty()) {
                statusKey = "connect.err.host";
                statusColor = RED;
            } else if (port_.empty()) {
                statusKey = "connect.err.port";
                statusColor = RED;
            } else {
                std::string err;
                int p = std::atoi(port_.c_str());
                if (p <= 0 || p > 65535) {
                    statusKey = "connect.err.invalidport";
                    statusColor = RED;
                } else if (!net_.connect(host_, (uint16_t)p, err)) {
                    statusKey = "connect.err.connfail";
                    status = std::string(i18n::tr(statusKey.c_str())) + err;
                    statusKey.clear();
                    statusColor = RED;
                } else {
                    statusKey = "connect.status.connecting";
                    statusColor = SKYBLUE;
                    connecting = true;
                    connectStarted = GetTime();
                }
            }
        }
        if (connecting) {
            net_.poll();
            auto msgs = net_.drainTcp();
            for (auto& m : msgs) {
                if (!m.empty() && m[0] == proto::kT_Hello) {
                    connectedAndHello = true;
                    break;
                }
            }
            if (!connectedAndHello && (GetTime() - connectStarted > 3.0)) {
                net_.disconnect();
                connecting = false;
                statusKey = "connect.status.timeout";
                statusColor = RED;
            }
        }
        const char* shown = nullptr;
        if (!statusKey.empty()) shown = i18n::tr(statusKey.c_str());
        else if (!status.empty()) shown = status.c_str();
        if (shown && *shown) {
            int sw3 = ui::measureText(shown, 20);
            ui::drawText(shown, (kWinW - sw3) / 2, btnY + btnH + 30, 20, statusColor);
        }
        const char* hint = i18n::tr("connect.hint");
        int hw = ui::measureText(hint, 16);
        ui::drawText(hint, (kWinW - hw) / 2, kWinH - 40, 16, GRAY);
        EndDrawing();
    }
    CloseWindow();
    return connectedAndHello;
}
