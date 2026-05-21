#include "LoginScreen.h"
#include "Protocol.h"
#include "UiTheme.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace {
constexpr int kWinW = 800;
constexpr int kWinH = 600;
constexpr int kMaxLen = 32;
}
LoginScreen::LoginScreen(NetClient& net) : net_(net) {}
void LoginScreen::setStatus(const std::string& msg, StatusKind kind) {
    status_ = msg;
    statusKind_ = kind;
}
void LoginScreen::handleTextInput(std::string& target) {
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch < 127 && (int)target.size() < kMaxLen) {
            target.push_back((char)ch);
        }
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !target.empty()) {
        target.pop_back();
    }
    static float repeatT = 0.0f;
    if (IsKeyDown(KEY_BACKSPACE)) {
        repeatT += GetFrameTime();
        if (repeatT > 0.45f && !target.empty()) {
            target.pop_back();
            repeatT -= 0.05f;
        }
    } else {
        repeatT = 0.0f;
    }
}
void LoginScreen::drawField(const char* label, const std::string& value, int x, int y,
                            int w, int h, bool focused, bool password) {
    ui::drawText(label, x, y - 22, 18, LIGHTGRAY);
    Color border = focused ? Color{ 220, 60, 60, 255 } : Color{ 90, 90, 100, 255 };
    DrawRectangle(x - 2, y - 2, w + 4, h + 4, border);
    DrawRectangle(x, y, w, h, { 25, 25, 32, 255 });
    std::string shown = password ? std::string(value.size(), '*') : value;
    ui::drawText(shown.c_str(), x + 10, y + (h - 22) / 2, 22, RAYWHITE);
    if (focused) {
        int tw = ui::measureText(shown.c_str(), 22);
        if (std::fmod(caretBlink_, 1.0f) < 0.5f) {
            DrawRectangle(x + 10 + tw + 2, y + 6, 2, h - 12, RAYWHITE);
        }
    }
}
bool LoginScreen::drawButton(const char* text, int x, int y, int w, int h, Color base) {
    Vector2 m = GetMousePosition();
    bool hover = m.x >= x && m.x <= x + w && m.y >= y && m.y <= y + h;
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
bool LoginScreen::run() {
    InitWindow(kWinW, kWinH, "ClaudeGame - Login");
    SetTargetFPS(60);
    SetExitKey(0);
    ui::loadFonts();
    const int fieldW = 400, fieldH = 40;
    const int fieldX = (kWinW - fieldW) / 2;
    const int userY = 200;
    const int passY = 280;
    const int btnW = 180, btnH = 44;
    const int loginX = kWinW / 2 - btnW - 12;
    const int regX   = kWinW / 2 + 12;
    const int btnY   = 360;
    bool loggedIn = false;
    bool quit = false;
    while (!WindowShouldClose() && !loggedIn && !quit) {
        float dt = GetFrameTime();
        caretBlink_ += dt;
        if (!net_.isConnected()) {
            setStatus("Disconnected from server", StatusKind::Error);
        }
        if (IsKeyPressed(KEY_ESCAPE)) { quit = true; break; }
        if (IsKeyPressed(KEY_TAB)) {
            focus_ = (focus_ == Focus::Username) ? Focus::Password : Focus::Username;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            if (m.x >= fieldX && m.x <= fieldX + fieldW) {
                if (m.y >= userY && m.y <= userY + fieldH) focus_ = Focus::Username;
                else if (m.y >= passY && m.y <= passY + fieldH) focus_ = Focus::Password;
            }
        }
        if (pending_ == Pending::None) {
            if (focus_ == Focus::Username) handleTextInput(username_);
            else handleTextInput(password_);
        }
        bool enterPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        net_.poll();
        auto msgs = net_.drainTcp();
        for (auto& m : msgs) {
            if (m.empty()) continue;
            const std::string& t = m[0];
            if (t == proto::kT_LoginOk) {
                loggedIn = true;
                pending_ = Pending::None;
                setStatus("Logged in!", StatusKind::Success);
            } else if (t == proto::kT_RegisterOk) {
                pending_ = Pending::None;
                std::string teamStr = (m.size() >= 2) ? m[1] : std::string();
                setStatus("Registered to team " + teamStr + ". Now log in.", StatusKind::Success);
            } else if (t == proto::kT_Err) {
                pending_ = Pending::None;
                std::string em = (m.size() >= 2) ? m[1] : std::string("error");
                setStatus(em, StatusKind::Error);
            }
        }
        bool loginClicked = false;
        bool regClicked = false;
        BeginDrawing();
        ClearBackground({ 12, 10, 14, 255 });
        const char* title = "CLAUDEGAME - CS ARENA";
        int tw = ui::measureTextBold(title, 40);
        ui::drawTextBold(title, (kWinW - tw) / 2, 70, 40, { 220, 60, 60, 255 });
        const char* sub = "Log in or register to deploy";
        int sw2 = ui::measureText(sub, 18);
        ui::drawText(sub, (kWinW - sw2) / 2, 120, 18, LIGHTGRAY);
        drawField("Username", username_, fieldX, userY, fieldW, fieldH,
                  focus_ == Focus::Username, false);
        drawField("Password", password_, fieldX, passY, fieldW, fieldH,
                  focus_ == Focus::Password, true);
        loginClicked = drawButton("LOGIN", loginX, btnY, btnW, btnH, { 140, 30, 30, 255 });
        regClicked   = drawButton("REGISTER", regX, btnY, btnW, btnH, { 50, 50, 70, 255 });
        if (pending_ == Pending::None && (loginClicked || enterPressed)) {
            if (username_.empty() || password_.empty()) {
                setStatus("Username and password required", StatusKind::Error);
            } else if (!net_.isConnected()) {
                setStatus("Not connected", StatusKind::Error);
            } else {
                net_.sendLogin(username_, password_);
                pending_ = Pending::Login;
                pendingStarted_ = GetTime();
                setStatus("Logging in...", StatusKind::Info);
            }
        } else if (pending_ == Pending::None && regClicked) {
            if (username_.empty() || password_.empty()) {
                setStatus("Username and password required", StatusKind::Error);
            } else if (!net_.isConnected()) {
                setStatus("Not connected", StatusKind::Error);
            } else {
                net_.sendRegister(username_, password_);
                pending_ = Pending::Register;
                pendingStarted_ = GetTime();
                setStatus("Registering...", StatusKind::Info);
            }
        }
        if (pending_ != Pending::None && (GetTime() - pendingStarted_ > 5.0)) {
            pending_ = Pending::None;
            setStatus("No response from server", StatusKind::Error);
        }
        if (!status_.empty()) {
            Color sc = RAYWHITE;
            switch (statusKind_) {
                case StatusKind::Success: sc = GREEN; break;
                case StatusKind::Error:   sc = RED; break;
                case StatusKind::Info:    sc = SKYBLUE; break;
                default: break;
            }
            int sw3 = ui::measureText(status_.c_str(), 20);
            ui::drawText(status_.c_str(), (kWinW - sw3) / 2, btnY + btnH + 30, 20, sc);
        }
        const char* hint = "TAB switch  -  ENTER login  -  ESC back";
        int hw = ui::measureText(hint, 16);
        ui::drawText(hint, (kWinW - hw) / 2, kWinH - 40, 16, GRAY);
        EndDrawing();
        if (!net_.isConnected()) {
            quit = true;
        }
    }
    CloseWindow();
    return loggedIn;
}
