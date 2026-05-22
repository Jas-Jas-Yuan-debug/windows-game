#pragma once
#include "Protocol.h"
#include "TlsConn.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
typedef std::uintptr_t cg_socket_t;
#define CG_INVALID_SOCKET (~(cg_socket_t)0)
#else
typedef int cg_socket_t;
#define CG_INVALID_SOCKET (-1)
#endif
class NetClient {
public:
    NetClient() = default;
    ~NetClient();
    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;
    bool connect(const std::string& host, uint16_t port, std::string& errOut);
    void disconnect();
    bool isConnected() const { return tcpFd_ != CG_INVALID_SOCKET; }
    void poll();
    void sendTcp(const std::vector<std::string>& fields);
    void sendUdp(const std::vector<std::string>& fields);
    void sendRegister(const std::string& user, const std::string& pass);
    void sendLogin(const std::string& user, const std::string& pass);
    static std::string derivePassword(const std::string& username, const std::string& password);
    std::vector<std::vector<std::string>> drainTcp();
    std::vector<std::vector<std::string>> drainUdp();
    int serverTickRate = proto::kTickRate;
    int teamSize = proto::kMaxTeamSize;
    int userId = 0;
    std::string username;
    std::string team;
    std::string udpToken;
    int level = 1;
    int xp = 0;
    int totalKills = 0;
    int totalDeaths = 0;
    int totalHeadshots = 0;
    int matchesPlayed = 0;
    int matchesWon = 0;
    int credits = 0;
    int selectedWeapon = 1;
    bool loggedIn = false;
    std::string udpHmacKey;
    std::string tlsFingerprint;
private:
    static constexpr std::size_t kMaxTcpSendBuf = 256 * 1024;
    cg_socket_t tcpFd_ = CG_INVALID_SOCKET;
    cg_socket_t udpFd_ = CG_INVALID_SOCKET;
    std::unique_ptr<TlsContext> tlsCtx_;
    std::unique_ptr<TlsConn> tls_;
    bool tlsReady_ = false;
    std::string tcpRecvBuf_;
    std::string tcpSendBuf_;
    std::vector<std::vector<std::string>> tcpInbox_;
    std::vector<std::vector<std::string>> udpInbox_;
    void tryFlushTcp();
    void parseHelloIfPresent(const std::vector<std::string>& msg);
    void parseLoginOkIfPresent(const std::vector<std::string>& msg);
};
