#include "UiTheme.h"
#include "I18n.h"
#include "Paths.h"
#include <raylib.h>
#include <cstdio>
#include <vector>
namespace ui {
namespace {
constexpr int kBaseSize = 64;
Font g_font{};
Font g_fontBold{};
bool g_loaded = false;
i18n::Lang g_loadedLang = i18n::Lang::EN;
bool fontIsValid(const Font& f) {
    return f.texture.id != 0;
}
Font loadWith(const char* relPath, const std::vector<int>& cps) {
    std::string full = cg::resourcePath(relPath);
    Font f;
    if (cps.empty()) {
        f = LoadFontEx(full.c_str(), kBaseSize, nullptr, 0);
    } else {
        std::vector<int> tmp = cps;
        f = LoadFontEx(full.c_str(), kBaseSize, tmp.data(), (int)tmp.size());
    }
    if (!fontIsValid(f)) {
        std::fprintf(stderr, "[ui] WARN: failed to load font '%s' — falling back to default font\n", full.c_str());
        return GetFontDefault();
    }
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return f;
}
void doLoad() {
    i18n::Lang lang = i18n::current();
    const auto& cps = i18n::codepointsForCurrent();
    if (lang == i18n::Lang::ZH) {
        g_font     = loadWith("assets/NotoSansSC-Regular.otf", cps);
        g_fontBold = loadWith("assets/NotoSansSC-Bold.otf", cps);
    } else {
        g_font     = loadWith("assets/Inter-Regular.ttf", cps);
        g_fontBold = loadWith("assets/Inter-Bold.ttf", cps);
    }
    g_loadedLang = lang;
    g_loaded = true;
}
}
void loadFonts() {
    g_font = Font{};
    g_fontBold = Font{};
    g_loaded = false;
    doLoad();
}
void unloadFonts() {
    if (!g_loaded) return;
    if (fontIsValid(g_font))     UnloadFont(g_font);
    if (fontIsValid(g_fontBold)) UnloadFont(g_fontBold);
    g_font = Font{};
    g_fontBold = Font{};
    g_loaded = false;
}
Font font(int) {
    if (!g_loaded || g_loadedLang != i18n::current()) loadFonts();
    return g_font;
}
Font fontBold(int) {
    if (!g_loaded || g_loadedLang != i18n::current()) loadFonts();
    return g_fontBold;
}
void drawText(const char* text, int x, int y, int fontSize, Color color) {
    if (!text) return;
    Font f = font(fontSize);
    DrawTextEx(f, text, Vector2{ (float)x, (float)y }, (float)fontSize, 1.0f, color);
}
void drawTextBold(const char* text, int x, int y, int fontSize, Color color) {
    if (!text) return;
    Font f = fontBold(fontSize);
    DrawTextEx(f, text, Vector2{ (float)x, (float)y }, (float)fontSize, 1.0f, color);
}
int measureText(const char* text, int fontSize) {
    if (!text) return 0;
    Font f = font(fontSize);
    Vector2 v = MeasureTextEx(f, text, (float)fontSize, 1.0f);
    return (int)v.x;
}
int measureTextBold(const char* text, int fontSize) {
    if (!text) return 0;
    Font f = fontBold(fontSize);
    Vector2 v = MeasureTextEx(f, text, (float)fontSize, 1.0f);
    return (int)v.x;
}
}
