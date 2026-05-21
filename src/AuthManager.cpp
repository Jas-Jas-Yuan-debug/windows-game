#include "AuthManager.h"
#include "Crypto.h"
#include "Weapon.h"
#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>
namespace {
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kMaxUsernameLen = 24;
std::int64_t nowEpoch() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
struct Stmt {
    sqlite3_stmt* s = nullptr;
    Stmt() = default;
    explicit Stmt(sqlite3_stmt* p) : s(p) {}
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    ~Stmt() { if (s) sqlite3_finalize(s); }
};
bool execSimple(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "sqlite3_exec failed";
        if (err) sqlite3_free(err);
        throw std::runtime_error("sqlite exec: " + msg);
    }
    return true;
}
}
AuthManager::AuthManager(const std::string& dbPath) {
    std::filesystem::path p(dbPath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown";
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        throw std::runtime_error("sqlite3_open failed: " + msg);
    }
    execSimple(db_, "PRAGMA foreign_keys = ON;");
    execSimple(db_, "PRAGMA journal_mode = WAL;");
    initSchema();
}
AuthManager::~AuthManager() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
void AuthManager::initSchema() {
    const char* kSchema =
        "CREATE TABLE IF NOT EXISTS users ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    username TEXT UNIQUE NOT NULL,"
        "    password_hash TEXT NOT NULL,"
        "    salt TEXT NOT NULL,"
        "    xp INTEGER NOT NULL DEFAULT 0,"
        "    level INTEGER NOT NULL DEFAULT 1,"
        "    total_kills INTEGER NOT NULL DEFAULT 0,"
        "    total_deaths INTEGER NOT NULL DEFAULT 0,"
        "    total_headshots INTEGER NOT NULL DEFAULT 0,"
        "    matches_played INTEGER NOT NULL DEFAULT 0,"
        "    matches_won INTEGER NOT NULL DEFAULT 0,"
        "    created_at INTEGER NOT NULL,"
        "    last_login INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS matches ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    user_id INTEGER NOT NULL,"
        "    map_name TEXT NOT NULL,"
        "    team TEXT NOT NULL,"
        "    team_score INTEGER NOT NULL,"
        "    enemy_team_score INTEGER NOT NULL,"
        "    player_kills INTEGER NOT NULL,"
        "    player_deaths INTEGER NOT NULL,"
        "    player_score INTEGER NOT NULL,"
        "    headshots INTEGER NOT NULL,"
        "    xp_earned INTEGER NOT NULL,"
        "    won INTEGER NOT NULL,"
        "    played_at INTEGER NOT NULL,"
        "    FOREIGN KEY (user_id) REFERENCES users(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_matches_user ON matches(user_id);"
        "CREATE INDEX IF NOT EXISTS idx_users_xp ON users(xp DESC);"
        "CREATE TABLE IF NOT EXISTS chat_messages ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    room TEXT NOT NULL,"
        "    user_id INTEGER NOT NULL,"
        "    username TEXT NOT NULL,"
        "    content TEXT NOT NULL,"
        "    sent_at INTEGER NOT NULL,"
        "    FOREIGN KEY (user_id) REFERENCES users(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_chat_room_time ON chat_messages(room, sent_at);"
        "CREATE TABLE IF NOT EXISTS user_weapons ("
        "    user_id INTEGER NOT NULL,"
        "    weapon_id INTEGER NOT NULL,"
        "    PRIMARY KEY (user_id, weapon_id),"
        "    FOREIGN KEY (user_id) REFERENCES users(id)"
        ");";
    execSimple(db_, kSchema);
    bool hasTeam = false, hasCredits = false, hasSelectedWeapon = false;
    sqlite3_stmt* probe = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA table_info(users)", -1, &probe, nullptr) == SQLITE_OK) {
        while (sqlite3_step(probe) == SQLITE_ROW) {
            const unsigned char* col = sqlite3_column_text(probe, 1);
            if (!col) continue;
            const char* name = reinterpret_cast<const char*>(col);
            if (std::strcmp(name, "team") == 0) hasTeam = true;
            else if (std::strcmp(name, "credits") == 0) hasCredits = true;
            else if (std::strcmp(name, "selected_weapon") == 0) hasSelectedWeapon = true;
        }
        sqlite3_finalize(probe);
    }
    if (!hasTeam) {
        execSimple(db_, "ALTER TABLE users ADD COLUMN team TEXT NOT NULL DEFAULT 'BLUE'");
    }
    if (!hasCredits) {
        execSimple(db_, "ALTER TABLE users ADD COLUMN credits INTEGER NOT NULL DEFAULT 500");
    }
    if (!hasSelectedWeapon) {
        execSimple(db_, "ALTER TABLE users ADD COLUMN selected_weapon INTEGER NOT NULL DEFAULT 1");
    }
    execSimple(db_,
        "INSERT OR IGNORE INTO user_weapons (user_id, weapon_id) "
        "SELECT id, 1 FROM users");
}
bool AuthManager::isValidUsername(const std::string& username) {
    if (username.empty() || username.size() > kMaxUsernameLen) return false;
    for (char ch : username) {
        unsigned char c = static_cast<unsigned char>(ch);
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
               || (c >= '0' && c <= '9')
               || c == '_' || c == '.' || c == '-';
        if (!ok) return false;
    }
    return true;
}
int AuthManager::computeLevel(int xp) {
    if (xp < 0) xp = 0;
    int lvl = 1 + static_cast<int>(std::floor(std::sqrt(static_cast<double>(xp) / 100.0)));
    if (lvl < 1) lvl = 1;
    if (lvl > 99) lvl = 99;
    return lvl;
}
std::string AuthManager::rankForLevel(int level) {
    if (level <= 2) return "Silver I";
    if (level <= 4) return "Silver Elite";
    if (level <= 7) return "Gold Nova";
    if (level <= 10) return "Master Guardian";
    if (level <= 14) return "Legendary Eagle";
    if (level <= 19) return "Supreme Master";
    return "Global Elite";
}
std::vector<std::string> AuthManager::allMaps() {
    return {"Arena", "Dust", "Office"};
}
int AuthManager::mapUnlockLevel(const std::string& map) {
    if (map == "Arena") return 1;
    if (map == "Dust") return 3;
    if (map == "Office") return 5;
    return 9999;
}
std::vector<std::string> AuthManager::availableMaps() const {
    std::vector<std::string> result;
    int level = currentUser_ ? currentUser_->level : 1;
    for (const auto& m : allMaps()) {
        if (level >= mapUnlockLevel(m)) result.push_back(m);
    }
    return result;
}
std::string AuthManager::currentRank() const {
    if (!currentUser_) return "";
    return rankForLevel(currentUser_->level);
}
bool AuthManager::loadUserByName(const std::string& username, User& out) {
    const char* sql =
        "SELECT id, username, password_hash, salt, xp, level, total_kills, total_deaths, "
        "total_headshots, matches_played, matches_won, team, credits, selected_weapon "
        "FROM users WHERE username = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_text(st.s, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st.s);
    if (rc != SQLITE_ROW) return false;
    out.id = sqlite3_column_int(st.s, 0);
    out.username = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 1));
    out.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 2));
    out.salt = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 3));
    out.xp = sqlite3_column_int(st.s, 4);
    out.level = sqlite3_column_int(st.s, 5);
    out.totalKills = sqlite3_column_int(st.s, 6);
    out.totalDeaths = sqlite3_column_int(st.s, 7);
    out.totalHeadshots = sqlite3_column_int(st.s, 8);
    out.matchesPlayed = sqlite3_column_int(st.s, 9);
    out.matchesWon = sqlite3_column_int(st.s, 10);
    const unsigned char* tm = sqlite3_column_text(st.s, 11);
    out.team = tm ? reinterpret_cast<const char*>(tm) : "";
    out.credits = sqlite3_column_int(st.s, 12);
    out.selectedWeapon = sqlite3_column_int(st.s, 13);
    return true;
}
bool AuthManager::loadUserById(int id, User& out) {
    const char* sql =
        "SELECT id, username, password_hash, salt, xp, level, total_kills, total_deaths, "
        "total_headshots, matches_played, matches_won, team, credits, selected_weapon "
        "FROM users WHERE id = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, id);
    int rc = sqlite3_step(st.s);
    if (rc != SQLITE_ROW) return false;
    out.id = sqlite3_column_int(st.s, 0);
    out.username = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 1));
    out.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 2));
    out.salt = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 3));
    out.xp = sqlite3_column_int(st.s, 4);
    out.level = sqlite3_column_int(st.s, 5);
    out.totalKills = sqlite3_column_int(st.s, 6);
    out.totalDeaths = sqlite3_column_int(st.s, 7);
    out.totalHeadshots = sqlite3_column_int(st.s, 8);
    out.matchesPlayed = sqlite3_column_int(st.s, 9);
    out.matchesWon = sqlite3_column_int(st.s, 10);
    const unsigned char* tm = sqlite3_column_text(st.s, 11);
    out.team = tm ? reinterpret_cast<const char*>(tm) : "";
    out.credits = sqlite3_column_int(st.s, 12);
    out.selectedWeapon = sqlite3_column_int(st.s, 13);
    return true;
}
bool AuthManager::registerUser(const std::string& username,
                               const std::string& password) {
    if (!isValidUsername(username)) return false;
    if (password.empty()) return false;
    User existing;
    if (loadUserByName(username, existing)) return false;
    std::string salt = generateSaltHex();
    std::string hash = hashPassword(salt, password);
    int userCount = 0;
    {
        sqlite3_stmt* craw = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM users", -1, &craw, nullptr) == SQLITE_OK) {
            Stmt cst(craw);
            if (sqlite3_step(cst.s) == SQLITE_ROW) {
                userCount = sqlite3_column_int(cst.s, 0);
            }
        }
    }
    std::string team = (userCount % 2 == 0) ? std::string("BLUE") : std::string("RED");
    const char* sql =
        "INSERT INTO users (username, password_hash, salt, xp, level, total_kills, "
        "total_deaths, total_headshots, matches_played, matches_won, created_at, last_login, team, "
        "credits, selected_weapon) "
        "VALUES (?, ?, ?, 0, 1, 0, 0, 0, 0, 0, ?, NULL, ?, 500, 1)";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_text(st.s, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st.s, 4, nowEpoch());
    sqlite3_bind_text(st.s, 5, team.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st.s);
    if (rc != SQLITE_DONE) return false;
    sqlite3_int64 newId = sqlite3_last_insert_rowid(db_);
    sqlite3_stmt* wraw = nullptr;
    if (sqlite3_prepare_v2(db_,
            "INSERT OR IGNORE INTO user_weapons (user_id, weapon_id) VALUES (?, 1)",
            -1, &wraw, nullptr) == SQLITE_OK) {
        Stmt wst(wraw);
        sqlite3_bind_int64(wst.s, 1, newId);
        sqlite3_step(wst.s);
    }
    return true;
}
bool AuthManager::login(const std::string& username, const std::string& password) {
    User u;
    if (!loadUserByName(username, u)) return false;
    std::string candidate = hashPassword(u.salt, password);
    if (candidate.size() != u.passwordHash.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < candidate.size(); ++i) {
        diff |= static_cast<unsigned char>(candidate[i] ^ u.passwordHash[i]);
    }
    if (diff != 0) return false;
    const char* sql = "UPDATE users SET last_login = ? WHERE id = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) == SQLITE_OK) {
        Stmt st(raw);
        sqlite3_bind_int64(st.s, 1, nowEpoch());
        sqlite3_bind_int(st.s, 2, u.id);
        sqlite3_step(st.s);
    }
    currentUser_ = u;
    return true;
}
bool AuthManager::isLoggedIn() const {
    return currentUser_.has_value();
}
const User* AuthManager::currentUser() const {
    if (!currentUser_) return nullptr;
    return &(*currentUser_);
}
void AuthManager::logout() {
    currentUser_.reset();
}
int AuthManager::recordMatch(const MatchResult& result) {
    if (!currentUser_) return 0;
    int xpEarned = 10 * result.playerKills + 5 * result.headshots + 25;
    if (result.won) xpEarned += 50;
    User u;
    if (!loadUserById(currentUser_->id, u)) return 0;
    int newXp = u.xp + xpEarned;
    int newLevel = computeLevel(newXp);
    int newKills = u.totalKills + result.playerKills;
    int newDeaths = u.totalDeaths + result.playerDeaths;
    int newHeadshots = u.totalHeadshots + result.headshots;
    int newMatchesPlayed = u.matchesPlayed + 1;
    int newMatchesWon = u.matchesWon + (result.won ? 1 : 0);
    execSimple(db_, "BEGIN;");
    bool ok = true;
    {
        const char* sql =
            "UPDATE users SET xp = ?, level = ?, total_kills = ?, total_deaths = ?, "
            "total_headshots = ?, matches_played = ?, matches_won = ? WHERE id = ?";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, newXp);
            sqlite3_bind_int(st.s, 2, newLevel);
            sqlite3_bind_int(st.s, 3, newKills);
            sqlite3_bind_int(st.s, 4, newDeaths);
            sqlite3_bind_int(st.s, 5, newHeadshots);
            sqlite3_bind_int(st.s, 6, newMatchesPlayed);
            sqlite3_bind_int(st.s, 7, newMatchesWon);
            sqlite3_bind_int(st.s, 8, u.id);
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
        }
    }
    if (ok) {
        const char* sql =
            "INSERT INTO matches (user_id, map_name, team, team_score, enemy_team_score, "
            "player_kills, player_deaths, player_score, headshots, xp_earned, won, played_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, u.id);
            sqlite3_bind_text(st.s, 2, result.mapName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st.s, 3, result.team.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st.s, 4, result.teamScore);
            sqlite3_bind_int(st.s, 5, result.enemyTeamScore);
            sqlite3_bind_int(st.s, 6, result.playerKills);
            sqlite3_bind_int(st.s, 7, result.playerDeaths);
            sqlite3_bind_int(st.s, 8, result.playerScore);
            sqlite3_bind_int(st.s, 9, result.headshots);
            sqlite3_bind_int(st.s, 10, xpEarned);
            sqlite3_bind_int(st.s, 11, result.won ? 1 : 0);
            sqlite3_bind_int64(st.s, 12, nowEpoch());
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
        }
    }
    if (ok) {
        execSimple(db_, "COMMIT;");
        User refreshed;
        if (loadUserById(u.id, refreshed)) currentUser_ = refreshed;
        return xpEarned;
    } else {
        execSimple(db_, "ROLLBACK;");
        return 0;
    }
}
namespace {
LeaderEntry rowToEntry(sqlite3_stmt* s) {
    LeaderEntry e;
    e.username = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
    e.level = sqlite3_column_int(s, 1);
    e.xp = sqlite3_column_int(s, 2);
    e.totalKills = sqlite3_column_int(s, 3);
    e.totalDeaths = sqlite3_column_int(s, 4);
    e.matchesPlayed = sqlite3_column_int(s, 5);
    e.matchesWon = sqlite3_column_int(s, 6);
    e.totalHeadshots = sqlite3_column_int(s, 7);
    e.winRate = (e.matchesPlayed > 0)
        ? static_cast<double>(e.matchesWon) / static_cast<double>(e.matchesPlayed)
        : 0.0;
    int deaths = e.totalDeaths > 0 ? e.totalDeaths : 1;
    e.kd = static_cast<double>(e.totalKills) / static_cast<double>(deaths);
    return e;
}
constexpr const char* kLeaderSelect =
    "SELECT username, level, xp, total_kills, total_deaths, matches_played, matches_won, "
    "total_headshots FROM users ";
}
std::vector<LeaderEntry> AuthManager::topByXP(int limit) {
    std::vector<LeaderEntry> result;
    std::string sql = std::string(kLeaderSelect) + "ORDER BY xp DESC, total_kills DESC LIMIT ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) return result;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, limit);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        LeaderEntry e = rowToEntry(st.s);
        e.rank = rankForLevel(e.level);
        result.push_back(std::move(e));
    }
    return result;
}
std::vector<LeaderEntry> AuthManager::topByKills(int limit) {
    std::vector<LeaderEntry> result;
    std::string sql = std::string(kLeaderSelect) + "ORDER BY total_kills DESC, xp DESC LIMIT ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) return result;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, limit);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        LeaderEntry e = rowToEntry(st.s);
        e.rank = rankForLevel(e.level);
        result.push_back(std::move(e));
    }
    return result;
}
std::vector<LeaderEntry> AuthManager::topByWinRate(int limit, int minMatches) {
    std::vector<LeaderEntry> result;
    std::string sql = std::string(kLeaderSelect) +
        "WHERE matches_played >= ? "
        "ORDER BY (CAST(matches_won AS REAL) / CAST(matches_played AS REAL)) DESC, "
        "matches_won DESC LIMIT ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw, nullptr) != SQLITE_OK) return result;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, minMatches);
    sqlite3_bind_int(st.s, 2, limit);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        LeaderEntry e = rowToEntry(st.s);
        e.rank = rankForLevel(e.level);
        result.push_back(std::move(e));
    }
    return result;
}
std::string AuthManager::bytesToHex(const unsigned char* bytes, std::size_t len) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out[2 * i]     = kHex[(bytes[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[bytes[i] & 0xF];
    }
    return out;
}
std::string AuthManager::hexToBytes(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("hexToBytes: odd-length hex string");
    }
    auto nybble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        throw std::runtime_error("hexToBytes: non-hex character");
    };
    std::string out;
    out.resize(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        int hi = nybble(hex[2 * i]);
        int lo = nybble(hex[2 * i + 1]);
        out[i] = static_cast<char>((hi << 4) | lo);
    }
    return out;
}
std::string AuthManager::generateSaltHex() {
    unsigned char buf[kSaltBytes];
    if (!cg::randomBytes(buf, kSaltBytes)) {
        throw std::runtime_error("randomBytes failed");
    }
    return bytesToHex(buf, kSaltBytes);
}
std::string AuthManager::userTeam() const {
    if (!currentUser_) return "";
    return currentUser_->team;
}
std::vector<std::string> AuthManager::roomsForTeam(const std::string& team) {
    if (team == "BLUE") return { "public", "team_blue" };
    if (team == "RED")  return { "public", "team_red" };
    return { "public" };
}
bool AuthManager::canAccessRoom(const std::string& room) const {
    if (!currentUser_) return false;
    if (room == "public") return true;
    if (room == "team_blue") return currentUser_->team == "BLUE";
    if (room == "team_red")  return currentUser_->team == "RED";
    return false;
}
bool AuthManager::sendMessage(const std::string& room, const std::string& content) {
    if (!currentUser_) return false;
    if (!canAccessRoom(room)) return false;
    std::size_t a = 0, b = content.size();
    while (a < b && std::isspace(static_cast<unsigned char>(content[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(content[b - 1]))) --b;
    if (a == b) return false;
    std::string trimmed = content.substr(a, b - a);
    if (trimmed.size() > 280) return false;
    const char* sql =
        "INSERT INTO chat_messages (room, user_id, username, content, sent_at) "
        "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_text(st.s, 1, room.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st.s, 2, currentUser_->id);
    sqlite3_bind_text(st.s, 3, currentUser_->username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 4, trimmed.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st.s, 5, nowEpoch());
    return sqlite3_step(st.s) == SQLITE_DONE;
}
std::vector<ChatMessage> AuthManager::recentMessages(const std::string& room, int limit) {
    std::vector<ChatMessage> result;
    if (limit <= 0) return result;
    const char* sql =
        "SELECT id, room, user_id, username, content, sent_at FROM chat_messages "
        "WHERE room = ? ORDER BY sent_at DESC, id DESC LIMIT ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return result;
    Stmt st(raw);
    sqlite3_bind_text(st.s, 1, room.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st.s, 2, limit);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        ChatMessage m;
        m.id = sqlite3_column_int(st.s, 0);
        m.room = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 1));
        m.userId = sqlite3_column_int(st.s, 2);
        m.username = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 3));
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 4));
        m.sentAt = sqlite3_column_int64(st.s, 5);
        result.push_back(std::move(m));
    }
    std::reverse(result.begin(), result.end());
    return result;
}
namespace {
bool teamCanAccessRoom(const std::string& team, const std::string& room) {
    if (room == "public") return true;
    if (room == "team_blue") return team == "BLUE";
    if (room == "team_red")  return team == "RED";
    return false;
}
}
bool AuthManager::sendMessageAs(int userId, const std::string& team,
                                const std::string& room, const std::string& content) {
    if (!teamCanAccessRoom(team, room)) return false;
    User u;
    if (!loadUserById(userId, u)) return false;
    std::size_t a = 0, b = content.size();
    while (a < b && std::isspace(static_cast<unsigned char>(content[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(content[b - 1]))) --b;
    if (a == b) return false;
    std::string trimmed = content.substr(a, b - a);
    if (trimmed.size() > 280) return false;
    const char* sql =
        "INSERT INTO chat_messages (room, user_id, username, content, sent_at) "
        "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_text(st.s, 1, room.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st.s, 2, u.id);
    sqlite3_bind_text(st.s, 3, u.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 4, trimmed.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st.s, 5, nowEpoch());
    return sqlite3_step(st.s) == SQLITE_DONE;
}
std::vector<ChatMessage> AuthManager::recentMessagesFor(const std::string& team,
                                                        const std::string& room, int limit) {
    std::vector<ChatMessage> result;
    if (!teamCanAccessRoom(team, room)) return result;
    if (limit <= 0) return result;
    const char* sql =
        "SELECT id, room, user_id, username, content, sent_at FROM chat_messages "
        "WHERE room = ? ORDER BY sent_at DESC, id DESC LIMIT ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return result;
    Stmt st(raw);
    sqlite3_bind_text(st.s, 1, room.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st.s, 2, limit);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        ChatMessage m;
        m.id = sqlite3_column_int(st.s, 0);
        m.room = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 1));
        m.userId = sqlite3_column_int(st.s, 2);
        m.username = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 3));
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 4));
        m.sentAt = sqlite3_column_int64(st.s, 5);
        result.push_back(std::move(m));
    }
    std::reverse(result.begin(), result.end());
    return result;
}
int AuthManager::recordMatchFor(int userId, const MatchResult& result) {
    int xpEarned = 10 * result.playerKills + 5 * result.headshots + 25;
    if (result.won) xpEarned += 50;
    User u;
    if (!loadUserById(userId, u)) return 0;
    int newXp = u.xp + xpEarned;
    int newLevel = computeLevel(newXp);
    int newKills = u.totalKills + result.playerKills;
    int newDeaths = u.totalDeaths + result.playerDeaths;
    int newHeadshots = u.totalHeadshots + result.headshots;
    int newMatchesPlayed = u.matchesPlayed + 1;
    int newMatchesWon = u.matchesWon + (result.won ? 1 : 0);
    execSimple(db_, "BEGIN;");
    bool ok = true;
    {
        const char* sql =
            "UPDATE users SET xp = ?, level = ?, total_kills = ?, total_deaths = ?, "
            "total_headshots = ?, matches_played = ?, matches_won = ? WHERE id = ?";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, newXp);
            sqlite3_bind_int(st.s, 2, newLevel);
            sqlite3_bind_int(st.s, 3, newKills);
            sqlite3_bind_int(st.s, 4, newDeaths);
            sqlite3_bind_int(st.s, 5, newHeadshots);
            sqlite3_bind_int(st.s, 6, newMatchesPlayed);
            sqlite3_bind_int(st.s, 7, newMatchesWon);
            sqlite3_bind_int(st.s, 8, u.id);
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
        }
    }
    if (ok) {
        const char* sql =
            "INSERT INTO matches (user_id, map_name, team, team_score, enemy_team_score, "
            "player_kills, player_deaths, player_score, headshots, xp_earned, won, played_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, u.id);
            sqlite3_bind_text(st.s, 2, result.mapName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st.s, 3, result.team.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(st.s, 4, result.teamScore);
            sqlite3_bind_int(st.s, 5, result.enemyTeamScore);
            sqlite3_bind_int(st.s, 6, result.playerKills);
            sqlite3_bind_int(st.s, 7, result.playerDeaths);
            sqlite3_bind_int(st.s, 8, result.playerScore);
            sqlite3_bind_int(st.s, 9, result.headshots);
            sqlite3_bind_int(st.s, 10, xpEarned);
            sqlite3_bind_int(st.s, 11, result.won ? 1 : 0);
            sqlite3_bind_int64(st.s, 12, nowEpoch());
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
        }
    }
    if (ok) {
        execSimple(db_, "COMMIT;");
        return xpEarned;
    } else {
        execSimple(db_, "ROLLBACK;");
        return 0;
    }
}
int AuthManager::credits(int userId) {
    const char* sql = "SELECT credits FROM users WHERE id = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return 0;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, userId);
    if (sqlite3_step(st.s) != SQLITE_ROW) return 0;
    return sqlite3_column_int(st.s, 0);
}
bool AuthManager::ownsWeapon(int userId, int weaponId) {
    const char* sql = "SELECT 1 FROM user_weapons WHERE user_id = ? AND weapon_id = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, userId);
    sqlite3_bind_int(st.s, 2, weaponId);
    return sqlite3_step(st.s) == SQLITE_ROW;
}
std::vector<int> AuthManager::ownedWeapons(int userId) {
    std::vector<int> out;
    const char* sql = "SELECT weapon_id FROM user_weapons WHERE user_id = ? ORDER BY weapon_id ASC";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return out;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, userId);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
        out.push_back(sqlite3_column_int(st.s, 0));
    }
    return out;
}
int AuthManager::selectedWeapon(int userId) {
    const char* sql = "SELECT selected_weapon FROM users WHERE id = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return 1;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, userId);
    if (sqlite3_step(st.s) != SQLITE_ROW) return 1;
    int v = sqlite3_column_int(st.s, 0);
    if (v <= 0) v = 1;
    return v;
}
bool AuthManager::selectWeapon(int userId, int weaponId) {
    if (!weapons::lookup(weaponId)) return false;
    if (!ownsWeapon(userId, weaponId)) return false;
    const char* sql = "UPDATE users SET selected_weapon = ? WHERE id = ?";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) return false;
    Stmt st(raw);
    sqlite3_bind_int(st.s, 1, weaponId);
    sqlite3_bind_int(st.s, 2, userId);
    return sqlite3_step(st.s) == SQLITE_DONE;
}
int AuthManager::buyWeapon(int userId, int weaponId) {
    const Weapon* w = weapons::lookup(weaponId);
    if (!w) return -3;
    if (ownsWeapon(userId, weaponId)) return -1;
    int have = credits(userId);
    if (have < w->price) return -2;
    execSimple(db_, "BEGIN;");
    bool ok = true;
    {
        const char* sql = "UPDATE users SET credits = credits - ? WHERE id = ? AND credits >= ?";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, w->price);
            sqlite3_bind_int(st.s, 2, userId);
            sqlite3_bind_int(st.s, 3, w->price);
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
            if (ok && sqlite3_changes(db_) == 0) ok = false;
        }
    }
    if (ok) {
        const char* sql = "INSERT INTO user_weapons (user_id, weapon_id) VALUES (?, ?)";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, userId);
            sqlite3_bind_int(st.s, 2, weaponId);
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
        }
    }
    if (ok) {
        execSimple(db_, "COMMIT;");
        return 0;
    }
    execSimple(db_, "ROLLBACK;");
    if (credits(userId) < w->price) return -2;
    if (ownsWeapon(userId, weaponId)) return -1;
    return -2;
}
int AuthManager::addCredits(int userId, int delta) {
    execSimple(db_, "BEGIN;");
    bool ok = true;
    {
        const char* sql =
            "UPDATE users SET credits = MAX(0, credits + ?) WHERE id = ?";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, delta);
            sqlite3_bind_int(st.s, 2, userId);
            if (sqlite3_step(st.s) != SQLITE_DONE) ok = false;
        }
    }
    int newTotal = 0;
    if (ok) {
        const char* sql = "SELECT credits FROM users WHERE id = ?";
        sqlite3_stmt* raw = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &raw, nullptr) == SQLITE_OK) {
            Stmt st(raw);
            sqlite3_bind_int(st.s, 1, userId);
            if (sqlite3_step(st.s) == SQLITE_ROW) {
                newTotal = sqlite3_column_int(st.s, 0);
            }
        }
        execSimple(db_, "COMMIT;");
    } else {
        execSimple(db_, "ROLLBACK;");
    }
    return newTotal;
}
std::string AuthManager::hashPassword(const std::string& saltHex,
                                      const std::string& password) {
    constexpr std::uint32_t kPbkdf2Iters = 100000;
    std::string saltBytes = hexToBytes(saltHex);
    unsigned char digest[cg::kSha256Bytes];
    if (!cg::pbkdf2Sha256(
            reinterpret_cast<const unsigned char*>(password.data()), password.size(),
            reinterpret_cast<const unsigned char*>(saltBytes.data()), saltBytes.size(),
            kPbkdf2Iters,
            digest, cg::kSha256Bytes)) {
        throw std::runtime_error("pbkdf2Sha256 failed");
    }
    return bytesToHex(digest, cg::kSha256Bytes);
}
