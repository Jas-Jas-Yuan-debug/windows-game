#include "SoloMenu.h"
#include "UiTheme.h"
#include "I18n.h"
#include <algorithm>
#include <raylib.h>
SoloMenu::SoloMenu(NetClient& /*net*/) {}
namespace {
bool drawBigButton(const char* text, int x, int y, int w, int h, Color base) {
    Vector2 mp = GetMousePosition();
    bool hover = mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h;
    Color fill = base;
    if (hover) fill = { (unsigned char)std::min(255, base.r + 35),
                        (unsigned char)std::min(255, base.g + 35),
                        (unsigned char)std::min(255, base.b + 35), 255 };
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, BLACK);
    int fs = 24;
    int tw = ui::measureTextBold(text, fs);
    ui::drawTextBold(text, x + (w - tw) / 2, y + (h - fs) / 2, fs, RAYWHITE);
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
}
SoloMenu::Result SoloMenu::run() {
    InitWindow(800, 600, "ClaudeGame - Solo");
    SetTargetFPS(60);
    SetExitKey(0);
    i18n::init();
    ui::loadFonts();
    Result result = Result::Back;
    bool done = false;
    while (!WindowShouldClose() && !done) {
        if (IsKeyPressed(KEY_ESCAPE)) { result = Result::Back; done = true; break; }
        BeginDrawing();
        ClearBackground({ 18, 22, 32, 255 });
        if (drawLangButton(800 - 90, 18, 72, 28)) {
            i18n::toggleLanguage();
            ui::loadFonts();
        }
        const char* title = i18n::tr("solo.title");
        int titleFs = 40;
        int tw = ui::measureTextBold(title, titleFs);
        ui::drawTextBold(title, (800 - tw) / 2, 50, titleFs, GOLD);
        const char* sub = i18n::tr("solo.sub");
        int sFs = 18;
        int swid = ui::measureText(sub, sFs);
        ui::drawText(sub, (800 - swid) / 2, 110, sFs, LIGHTGRAY);
        int cardW = 340, cardH = 200;
        int gapX = 20;
        int totalW = cardW * 2 + gapX;
        int startX = (800 - totalW) / 2;
        int cardY = 180;
        DrawRectangle(startX, cardY, cardW, cardH, { 30, 35, 50, 255 });
        DrawRectangleLines(startX, cardY, cardW, cardH, { 80, 90, 120, 255 });
        const char* practiceDesc = i18n::tr("solo.desc.practice");
        int pdW = ui::measureText(practiceDesc, 15);
        ui::drawText(practiceDesc, startX + (cardW - pdW) / 2, cardY + 20, 15, { 200, 210, 225, 255 });
        if (drawBigButton(i18n::tr("solo.btn.practice"), startX + 30, cardY + 70, cardW - 60, 56, { 70, 130, 90, 255 })) {
            result = Result::Practice; done = true;
        }
        int rcardX = startX + cardW + gapX;
        DrawRectangle(rcardX, cardY, cardW, cardH, { 30, 35, 50, 255 });
        DrawRectangleLines(rcardX, cardY, cardW, cardH, { 80, 90, 120, 255 });
        const char* botDesc = i18n::tr("solo.desc.bots");
        int bdW = ui::measureText(botDesc, 15);
        ui::drawText(botDesc, rcardX + (cardW - bdW) / 2, cardY + 20, 15, { 200, 210, 225, 255 });
        if (drawBigButton(i18n::tr("solo.btn.bots"), rcardX + 30, cardY + 70, cardW - 60, 56, { 150, 60, 60, 255 })) {
            result = Result::VsBots; done = true;
        }
        if (drawBigButton(i18n::tr("solo.btn.back"), (800 - 220) / 2, 470, 220, 50, { 80, 80, 100, 255 })) {
            result = Result::Back; done = true;
        }
        EndDrawing();
    }
    CloseWindow();
    return result;
}
