#pragma once
#include "User.h"
#include "MatchResult.h"
#include "LeaderEntry.h"
#include "ChatMessage.h"
#include <string>
#include <vector>
#include <optional>
struct sqlite3;
class AuthManager {
public:
    explicit AuthManager(const std::string& dbPath);
    ~AuthManager();
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;
    bool registerUser(const std::string& username, const std::string& password);
    bool login(const std::string& username, const std::string& password);
    bool isLoggedIn() const;
    const User* currentUser() const;
    void logout();
    int recordMatch(const MatchResult& result);
    std::vector<LeaderEntry> topByXP(int limit = 10);
    std::vector<LeaderEntry> topByKills(int limit = 10);
    std::vector<LeaderEntry> topByWinRate(int limit = 10, int minMatches = 3);
    std::vector<std::string> availableMaps() const;
    static std::vector<std::string> allMaps();
    static int mapUnlockLevel(const std::string& map);
    std::string currentRank() const;
    static std::string rankForLevel(int level);
    bool sendMessage(const std::string& room, const std::string& content);
    std::vector<ChatMessage> recentMessages(const std::string& room, int limit = 100);
    std::string userTeam() const;
    static std::vector<std::string> roomsForTeam(const std::string& team);
    bool sendMessageAs(int userId, const std::string& team,
                       const std::string& room, const std::string& content);
    std::vector<ChatMessage> recentMessagesFor(const std::string& team,
                                               const std::string& room, int limit = 100);
    int recordMatchFor(int userId, const MatchResult& result);
    bool loadUserByIdPublic(int id, User& out) { return loadUserById(id, out); }
    bool loadUserByNamePublic(const std::string& name, User& out) { return loadUserByName(name, out); }
    int  credits(int userId);
    bool ownsWeapon(int userId, int weaponId);
    std::vector<int> ownedWeapons(int userId);
    int  selectedWeapon(int userId);
    bool selectWeapon(int userId, int weaponId);
    int  buyWeapon(int userId, int weaponId);
    int  addCredits(int userId, int delta);
    // DB-level ban: 0 = not banned; otherwise epoch-seconds until which login is rejected.
    long long bannedUntil(int userId);
    long long bannedUntilByName(const std::string& username);
    // Set ban expiry. Pass nowSeconds + 86400 for 24h, or 0 to clear.
    bool banUserUntil(int userId, long long epochSecondsUntil);
private:
    sqlite3* db_ = nullptr;
    std::optional<User> currentUser_;
    void initSchema();
    static bool isValidUsername(const std::string& username);
    static int computeLevel(int xp);
    bool loadUserByName(const std::string& username, User& out);
    bool loadUserById(int id, User& out);
    bool canAccessRoom(const std::string& room) const;
    static std::string generateSaltHex();
    static std::string hashPassword(const std::string& saltHex,
                                    const std::string& password);
    static std::string bytesToHex(const unsigned char* bytes, std::size_t len);
    static std::string hexToBytes(const std::string& hex);
};
