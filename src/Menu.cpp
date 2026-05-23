#include "Menu.h"
#include "Protocol.h"
#include "UiTheme.h"
#include "I18n.h"
#include "Weapon.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
Menu::Menu(NetClient& net) : net_(net) {}
namespace {
const char* rankKeyForLevel(int level) {
    if (level >= 20) return "rank.elite";
    if (level >= 15) return "rank.veteran";
    if (level >= 10) return "rank.sergeant";
    if (level >= 5)  return "rank.corporal";
    if (level >= 3)  return "rank.private";
    return "rank.recruit";
}
const char* weaponI18nKey(int id) {
    switch (id) {
        case 1: return "weapon.pistol";
        case 2: return "weapon.smg";
        case 3: return "weapon.shotgun";
        case 4: return "weapon.rifle";
        case 5: return "weapon.sniper";
        default: return nullptr;
    }
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
}
bool Menu::drawButton(const char* text, int x, int y, int w, int h, Color base, bool enabled) {
    Vector2 mp = GetMousePosition();
    bool hover = enabled && mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color fill = base;
    if (!enabled) fill = { 60, 60, 60, 255 };
    else if (hover) fill = { (unsigned char)std::min(255, base.r + 35),
                              (unsigned char)std::min(255, base.g + 35),
                              (unsigned char)std::min(255, base.b + 35), 255 };
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 22;
    int tw = ui::measureTextBold(text, fs);
    ui::drawTextBold(text, x + (w - tw) / 2, y + (h - fs) / 2, fs, enabled ? RAYWHITE : LIGHTGRAY);
    return enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
Menu::Result Menu::run() {
    InitWindow(800, 600, "ClaudeGame - Menu");
    SetTargetFPS(60);
    SetExitKey(0);
    i18n::init();
    ui::loadFonts();
    Result result;
    result.action = Action::Quit;
    result.selectedMap = "";
    std::string username = net_.username.empty() ? std::string("guest") : net_.username;
    bool done = false;
    while (!WindowShouldClose() && !done) {
        net_.poll();
        net_.drainTcp();
        net_.drainUdp();
        if (!net_.isConnected()) {
            result.action = Action::Quit;
            done = true;
            break;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            result.action = Action::Quit;
            done = true;
            break;
        }
        int level = net_.level;
        int xp = net_.xp;
        std::string rank = i18n::tr(rankKeyForLevel(level));
        std::string team = net_.team;
        int credits = net_.credits;
        const char* weaponKey = weaponI18nKey(net_.selectedWeapon);
        const Weapon* selectedWeapon = weapons::lookup(net_.selectedWeapon);
        const char* weaponName = weaponKey ? i18n::tr(weaponKey)
                                           : (selectedWeapon ? selectedWeapon->name : "Pistol");
        BeginDrawing();
        ClearBackground({ 18, 22, 32, 255 });
        if (drawLangButton(800 - 90, 18, 72, 28)) {
            i18n::toggleLanguage();
            ui::loadFonts();
        }
        const char* title = i18n::tr("menu.title");
        int titleFs = 40;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (800 - tw) / 2, 30, titleFs, GOLD);
        int cardX = 40, cardY = 90, cardW = 720, cardH = 110;
        DrawRectangle(cardX, cardY, cardW, cardH, { 30, 35, 50, 255 });
        DrawRectangleLines(cardX, cardY, cardW, cardH, { 80, 90, 120, 255 });
        char line1[256];
        char welcomeFmt[128];
        std::snprintf(welcomeFmt, sizeof(welcomeFmt), "%s", i18n::tr("menu.welcome.fmt"));
        std::snprintf(line1, sizeof(line1), welcomeFmt, username.c_str(), rank.c_str(), level, xp);
        char teamPart[64];
        if (!team.empty()) {
            const char* teamLoc = team == "BLUE" ? i18n::tr("common.team.blue")
                                : team == "RED"  ? i18n::tr("common.team.red")
                                : team.c_str();
            char teamFmt[64];
            std::snprintf(teamFmt, sizeof(teamFmt), "%s", i18n::tr("menu.team.fmt"));
            std::snprintf(teamPart, sizeof(teamPart), teamFmt, teamLoc);
        } else {
            teamPart[0] = '\0';
        }
        int cardFs = 18;
        int baseW = ui::measureText(line1, cardFs);
        int teamW = ui::measureTextBold(teamPart, cardFs);
        int totalCardW = baseW + teamW;
        int cx0 = cardX + (cardW - totalCardW) / 2;
        int y1 = cardY + 18;
        ui::drawText(line1, cx0, y1, cardFs, RAYWHITE);
        Color teamColor = RAYWHITE;
        if (team == "BLUE") teamColor = { 80, 130, 240, 255 };
        else if (team == "RED") teamColor = { 230, 80, 80, 255 };
        ui::drawTextBold(teamPart, cx0 + baseW, y1, cardFs, teamColor);
        char line2[160];
        char creditsFmt[64];
        std::snprintf(creditsFmt, sizeof(creditsFmt), "%s", i18n::tr("menu.credits.fmt"));
        std::snprintf(line2, sizeof(line2), creditsFmt, credits);
        char line3[160];
        char weaponFmt[64];
        std::snprintf(weaponFmt, sizeof(weaponFmt), "%s", i18n::tr("menu.weapon.fmt"));
        std::snprintf(line3, sizeof(line3), weaponFmt, weaponName);
        int row2Fs = 18;
        int sep = 24;
        int line2W = ui::measureTextBold(line2, row2Fs);
        int line3W = ui::measureTextBold(line3, row2Fs);
        int totalRow2 = line2W + sep + line3W;
        int rx0 = cardX + (cardW - totalRow2) / 2;
        int y2 = cardY + 54;
        ui::drawTextBold(line2, rx0, y2, row2Fs, GOLD);
        ui::drawTextBold(line3, rx0 + line2W + sep, y2, row2Fs, { 180, 220, 255, 255 });
        const char* hint = i18n::tr("menu.hint.randmap");
        int hFs = 14;
        int hW = ui::measureText(hint, hFs);
        ui::drawText(hint, cardX + (cardW - hW) / 2, cardY + 84, hFs, { 150, 160, 180, 255 });
        int btnW = 320, btnH = 40;
        int btnX = (800 - btnW) / 2;
        int btnY0 = 215;
        int gap = 8;
        if (drawButton(i18n::tr("menu.btn.play"), btnX, btnY0, btnW, btnH, { 40, 120, 50, 255 })) {
            result.action = Action::Play;
            done = true;
        }
        if (drawButton(i18n::tr("menu.btn.solo"), btnX, btnY0 + (btnH + gap), btnW, btnH, { 130, 70, 140, 255 })) {
            result.action = Action::Solo;
            done = true;
        }
        if (drawButton(i18n::tr("menu.btn.leaderboard"), btnX, btnY0 + 2 * (btnH + gap), btnW, btnH, { 50, 80, 140, 255 })) {
            result.action = Action::Leaderboard;
            done = true;
        }
        if (drawButton(i18n::tr("menu.btn.chat"), btnX, btnY0 + 3 * (btnH + gap), btnW, btnH, { 70, 110, 90, 255 })) {
            result.action = Action::Chat;
            done = true;
        }
        if (drawButton(i18n::tr("menu.btn.store"), btnX, btnY0 + 4 * (btnH + gap), btnW, btnH, { 150, 110, 30, 255 })) {
            result.action = Action::Store;
            done = true;
        }
        if (drawButton(i18n::tr("menu.btn.logout"), btnX, btnY0 + 5 * (btnH + gap), btnW, btnH, { 140, 90, 40, 255 })) {
            result.action = Action::Logout;
            done = true;
        }
        if (drawButton(i18n::tr("menu.btn.quit"), btnX, btnY0 + 6 * (btnH + gap), btnW, btnH, { 140, 50, 50, 255 })) {
            result.action = Action::Quit;
            done = true;
        }
        ui::drawText(i18n::tr("menu.hint.esc"), 12, 580, 14, GRAY);
        EndDrawing();
    }
    CloseWindow();
    return result;
}
