#include "I18n.h"
#include "Paths.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace i18n {
namespace {
Lang g_lang = Lang::EN;
bool g_inited = false;
std::vector<int> g_codepoints;
std::string prefsPath() {
    return cg::userDataPath("client.prefs");
}
struct Entry { const char* key; const char* en; const char* zh; };
const Entry kTable[] = {
    { "connect.title",            "CLAUDEGAME",                                  "克劳德游戏" },
    { "connect.sub",              "Connect to server",                           "连接服务器" },
    { "connect.host",             "Host",                                        "主机" },
    { "connect.port",             "Port",                                        "端口" },
    { "connect.btn.connect",      "CONNECT",                                     "连接" },
    { "connect.btn.host",         "HOST LOCAL GAME",                             "本地开房" },
    { "connect.btn.quit",         "QUIT",                                        "退出" },
    { "connect.status.hosted",    "Local server running; click CONNECT.",        "本地服务器已启动；请点击连接。" },
    { "connect.err.host",         "Host required",                               "请输入主机" },
    { "connect.err.port",         "Port required",                               "请输入端口" },
    { "connect.err.invalidport",  "Invalid port",                                "端口无效" },
    { "connect.err.connfail",     "Connect failed: ",                            "连接失败：" },
    { "connect.status.connecting","Connecting... waiting for HELLO",             "连接中…等待 HELLO" },
    { "connect.status.timeout",   "Timed out waiting for HELLO",                 "等待 HELLO 超时" },
    { "connect.hint",             "TAB switch  -  ENTER connect  -  ESC quit",   "TAB 切换  -  ENTER 连接  -  ESC 退出" },
    { "login.title",              "LOGIN",                                       "登录" },
    { "login.sub",                "Sign in or register",                         "登录或注册" },
    { "login.user",               "Username",                                    "用户名" },
    { "login.pass",               "Password",                                    "密码" },
    { "login.btn.login",          "LOGIN",                                       "登录" },
    { "login.btn.register",       "REGISTER",                                    "注册" },
    { "login.btn.back",           "BACK",                                        "返回" },
    { "login.err.empty",          "Username and password required",              "请输入用户名和密码" },
    { "login.status.wait",        "Waiting for server...",                       "等待服务器…" },
    { "menu.title",               "CLAUDEGAME - CS ARENA",                       "克劳德游戏 - CS 竞技场" },
    { "menu.welcome.fmt",         "Welcome, %s  |  %s  |  Lv %d  |  XP %d  |  ", "欢迎，%s  |  %s  |  等级 %d  |  经验 %d  |  " },
    { "menu.team.fmt",            "Team %s",                                     "队伍 %s" },
    { "menu.credits.fmt",         "Credits: %d",                                 "金币：%d" },
    { "menu.weapon.fmt",          "Weapon: %s",                                  "武器：%s" },
    { "menu.hint.randmap",        "The server picks the map randomly each match.","服务器每场随机选择地图。" },
    { "menu.btn.play",            "PLAY MATCH",                                  "开始对战" },
    { "menu.btn.leaderboard",     "LEADERBOARD",                                 "排行榜" },
    { "menu.btn.chat",            "CHAT",                                        "聊天" },
    { "menu.btn.store",           "STORE",                                       "商店" },
    { "menu.btn.solo",            "SOLO",                                        "单人模式" },
    { "menu.btn.logout",          "LOGOUT",                                      "登出" },
    { "menu.btn.quit",            "QUIT",                                        "退出" },
    { "menu.hint.esc",            "ESC to quit",                                 "按 ESC 退出" },
    { "rank.recruit",             "RECRUIT",                                     "新兵" },
    { "rank.private",             "PRIVATE",                                     "列兵" },
    { "rank.corporal",            "CORPORAL",                                    "下士" },
    { "rank.sergeant",            "SERGEANT",                                    "中士" },
    { "rank.veteran",             "VETERAN",                                     "老兵" },
    { "rank.elite",               "ELITE",                                       "精英" },
    { "solo.title",               "SOLO MODE",                                   "单人模式" },
    { "solo.sub",                 "Choose a solo activity",                      "请选择单人模式" },
    { "solo.btn.practice",        "PRACTICE",                                    "练习" },
    { "solo.btn.bots",            "VS BOTS",                                     "对战机器人" },
    { "solo.btn.back",            "BACK",                                        "返回" },
    { "solo.desc.practice",       "Empty map, target dummies, no enemies.",      "空地图，靶子，无敌人。" },
    { "solo.desc.bots",           "Offline match against AI opponents.",         "离线对战 AI 对手。" },
    { "solo.hud.dummies",         "Hits: %d",                                    "命中：%d" },
    { "solo.hud.exit",            "ESC to leave",                                "ESC 返回" },
    { "store.title",              "STORE",                                       "商店" },
    { "store.credits.fmt",        "Credits: %d",                                 "金币：%d" },
    { "store.btn.buy.fmt",        "BUY $%d",                                     "购买 $%d" },
    { "store.btn.select",         "SELECT",                                      "选择" },
    { "store.btn.selected",       "[SELECTED]",                                  "[已装备]" },
    { "store.btn.owned",          "OWNED",                                       "已拥有" },
    { "weapon.pistol",            "Pistol",                                      "手枪" },
    { "weapon.smg",               "SMG",                                         "冲锋枪" },
    { "weapon.shotgun",           "Shotgun",                                     "霰弹枪" },
    { "weapon.rifle",             "Rifle",                                       "步枪" },
    { "weapon.sniper",            "Sniper",                                      "狙击枪" },
    { "lb.title",                 "LEADERBOARD",                                 "排行榜" },
    { "lb.tab.xp",                "By XP",                                       "按经验" },
    { "lb.tab.kills",             "By Kills",                                    "按击杀" },
    { "lb.tab.winrate",           "By Win Rate",                                 "按胜率" },
    { "lb.empty",                 "No entries yet",                              "暂无记录" },
    { "lb.loading",               "Loading...",                                  "加载中…" },
    { "lb.hint",                  "TAB to switch tabs   ESC to return",          "TAB 切换   ESC 返回" },
    { "chat.title",               "CHAT",                                        "聊天" },
    { "chat.room.public",         "PUBLIC",                                      "公共" },
    { "chat.room.blue",           "TEAM BLUE",                                   "蓝队" },
    { "chat.room.red",            "TEAM RED",                                    "红队" },
    { "chat.placeholder",         "Type a message...",                           "输入消息…" },
    { "common.btn.back",          "BACK",                                        "返回" },
    { "common.team.blue",         "BLUE",                                        "蓝" },
    { "common.team.red",          "RED",                                         "红" },
    { "lang.toggle.from_en",      "中文",                                         "中文" },
    { "lang.toggle.from_zh",      "EN",                                          "EN" },
};
constexpr int kTableSize = sizeof(kTable) / sizeof(kTable[0]);
std::unordered_map<std::string, const Entry*> g_index;
void buildIndex() {
    g_index.clear();
    for (int i = 0; i < kTableSize; ++i) g_index[kTable[i].key] = &kTable[i];
}
int utf8ToCodepoint(const char*& p) {
    unsigned char c = (unsigned char)*p++;
    if (c < 0x80) return c;
    int extra = 0, val = 0;
    if ((c & 0xE0) == 0xC0) { extra = 1; val = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; val = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; val = c & 0x07; }
    else return 0xFFFD;
    for (int i = 0; i < extra; ++i) {
        unsigned char cc = (unsigned char)*p;
        if ((cc & 0xC0) != 0x80) return 0xFFFD;
        val = (val << 6) | (cc & 0x3F);
        ++p;
    }
    return val;
}
void rebuildCodepoints() {
    std::unordered_set<int> set;
    for (int cp = 32; cp < 127; ++cp) set.insert(cp);
    set.insert(0x2026);
    for (int i = 0; i < kTableSize; ++i) {
        const char* s = (g_lang == Lang::ZH) ? kTable[i].zh : kTable[i].en;
        if (!s) continue;
        const char* p = s;
        while (*p) {
            int cp = utf8ToCodepoint(p);
            if (cp > 0) set.insert(cp);
        }
    }
    g_codepoints.assign(set.begin(), set.end());
}
void loadPrefs() {
    std::ifstream f(prefsPath());
    if (!f.good()) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "lang") {
            if (v == "zh") g_lang = Lang::ZH;
            else g_lang = Lang::EN;
        }
    }
}
}
void init() {
    if (g_inited) return;
    buildIndex();
    loadPrefs();
    rebuildCodepoints();
    g_inited = true;
}
void save() {
    std::FILE* f = std::fopen(prefsPath().c_str(), "w");
    if (!f) return;
    std::fprintf(f, "lang=%s\n", g_lang == Lang::ZH ? "zh" : "en");
    std::fclose(f);
}
Lang current() { init(); return g_lang; }
void setLanguage(Lang l) {
    init();
    if (g_lang == l) return;
    g_lang = l;
    rebuildCodepoints();
    save();
}
void toggleLanguage() {
    setLanguage(g_lang == Lang::EN ? Lang::ZH : Lang::EN);
}
const char* tr(const char* key) {
    init();
    auto it = g_index.find(key);
    if (it == g_index.end()) return key;
    const Entry* e = it->second;
    return (g_lang == Lang::ZH && e->zh) ? e->zh : e->en;
}
const std::vector<int>& codepointsForCurrent() {
    init();
    return g_codepoints;
}
const char* langToggleLabel() {
    init();
    return g_lang == Lang::EN ? tr("lang.toggle.from_en") : tr("lang.toggle.from_zh");
}
}
