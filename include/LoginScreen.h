#pragma once
#include "NetClient.h"
#include <raylib.h>
#include <string>
class LoginScreen {
public:
    explicit LoginScreen(NetClient& net);
    bool run();
private:
    enum class Focus { Username, Password };
    enum class StatusKind { None, Info, Error, Success };
    NetClient& net_;
    std::string username_;
    std::string password_;
    Focus focus_ = Focus::Username;
    std::string status_;
    StatusKind statusKind_ = StatusKind::None;
    float caretBlink_ = 0.0f;
    enum class Pending { None, Login, Register };
    Pending pending_ = Pending::None;
    double pendingStarted_ = 0.0;
    void handleTextInput(std::string& target);
    void setStatus(const std::string& msg, StatusKind kind);
    void drawField(const char* label, const std::string& value, int x, int y,
                   int w, int h, bool focused, bool password);
    bool drawButton(const char* text, int x, int y, int w, int h, Color base);
};
