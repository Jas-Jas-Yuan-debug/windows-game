#pragma once
#include <raylib.h>
namespace ui {
void loadFonts();
void unloadFonts();
Font font(int sizePx);
Font fontBold(int sizePx);
void drawText(const char* text, int x, int y, int fontSize, Color color);
void drawTextBold(const char* text, int x, int y, int fontSize, Color color);
int  measureText(const char* text, int fontSize);
int  measureTextBold(const char* text, int fontSize);
}
