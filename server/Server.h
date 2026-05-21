#pragma once
#include "AuthManager.h"
#include "PlatformNet.h"
#include "Protocol.h"
#include "Match.h"
#include "TlsConn.h"
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
struct Client {
    int id = -1;
    cg_socket_t fd = CG_INVALID_SOCKET;
    std::unique_ptr<TlsConn> tls;
    bool tlsReady = false;
    std::string recvBuf;
    std::string sendBuf;
    sockaddr_in addr{};
    bool loggedIn = false;
    int userId = 0;
    std::string username;
    std::string team;
    std::string udpToken;
    std::string udpHmacKey;
    bool udpKnown = false;
    sockaddr_in udpAddr{};
    int matchId = -1;
    int slotInMatch = -1;
    bool queued = false;
    int credits = 0;
    int selectedWeapon = 1;
    bool wantClose = false;
    long long tcpWindowStartMs = 0;
    int tcpMsgCount = 0;
    long long udpWindowStartMs = 0;
    int udpMsgCount = 0;
    long long lastChatMs = 0;
    int lastInputTick = -1;
    unsigned int suspicionFlags = 0;
    long long connectedAtMs = 0;
    long long lastInputAtMs = 0;
    float lastInputYaw = 0.0f;
    bool hadFirstInput = false;
    int aimSnapCount = 0;
    int badHmacCount = 0;
    int udpFloodCount = 0;
    int badInputCount = 0;
};
struct LoginAttemptRecord {
    int failCount = 0;
    long long windowStartMs = 0;
    long long lockUntilMs = 0;
};
struct OrphanMatchSlot {
    int matchId = -1;
    int slotIndex = -1;
    long long expiresAtMs = 0;
};
struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 27015;
    int teamSize = 5;
    int botFillSecs = 120;
    std::string dbPath = "data/server.sqlite";
};
class Server {
public:
    Server(const ServerConfig& cfg);
    ~Server();
    int run();
    void stop() { stop_ = true; }
    bool isReady() const { return ready_.load(); }
private:
    ServerConfig cfg_;
    AuthManager auth_;
    TlsContext tlsCtx_;
    cg_socket_t tcpFd_ = CG_INVALID_SOCKET;
    cg_socket_t udpFd_ = CG_INVALID_SOCKET;
    int nextClientId_ = 1;
    int nextMatchId_ = 1;
    bool stop_ = false;
    std::atomic<bool> ready_{false};
    std::unordered_map<int, Client> clients_;
    std::unordered_map<int, std::unique_ptr<Match>> matches_;
    std::vector<int> queue_;
    long long queueWaitStartMs_ = 0;
    std::unordered_map<std::string, LoginAttemptRecord> loginAttempts_;
    std::unordered_map<unsigned int, int> ipConnCount_;
    std::unordered_map<unsigned int, std::vector<long long>> ipRegisterLog_;
    std::unordered_map<int, OrphanMatchSlot> orphanSlots_;
    bool bindSockets();
    void closeSockets();
    void acceptNewClient();
    void readFromClient(Client& c);
    void dispatchTcp(Client& c, const std::string& line);
    void readUdp();
    void tickMatches();
    void sendToClient(Client& c, const std::string& msg);
    void sendRaw(Client& c, const std::string& bytes);
    void broadcastChat(const std::string& room, const std::string& user, const std::string& content, long long sentAt);
    void disconnectClient(int clientId);
    void leaveMatchOrQueue(Client& c, bool announce, bool immediateForfeit = true);
    void expireOrphans(long long nowMs);
    bool tryReattachOrphan(Client& c);
    void tryStartMatch();
    void startMatchWithBotFill();
    void launchMatch(std::vector<int> candidateCids);
    void maybeBotFillTimeout(long long nowMs);
    void enforceIdleDisconnects(long long nowMs);
    void onTlsReady(Client& c);
    Client* clientByUdpToken(const std::string& tok);
    void sendQueueStatus();
    void sendLoginOk(Client& c);
    void recordAndAnnounceMatchEnd(Match& m);
    void sendUdpTo(const sockaddr_in& addr, const std::string& msg);
    std::string pickRandomMap();
};
