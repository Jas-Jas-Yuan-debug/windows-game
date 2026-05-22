#include "Server.h"
#include "Crypto.h"
#include "Paths.h"
#include "Weapon.h"
#include "Maps.h"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <vector>
namespace {
constexpr int kServerVersion = 1;
constexpr const char* kHelloVerStr = "1";
constexpr int kMaxTcpMsgsPerSec        = 20;
constexpr int kMaxUdpMsgsPerSec        = 90;
constexpr long long kChatMinIntervalMs = 500;
constexpr int kMaxChatLen              = 280;
constexpr int kMaxConnPerIp            = 8;
constexpr int kMaxUsernameLen          = 24;
constexpr int kMaxPasswordLen          = 64;
constexpr int kLoginFailWindowMs       = 60000;
constexpr int kLoginFailLimit          = 5;
constexpr int kLoginLockMs             = 60000;
constexpr unsigned int kFlagInputNonFinite = 1u << 0;
constexpr unsigned int kFlagInputOldTick   = 1u << 1;
constexpr unsigned int kFlagInputFutureTick= 1u << 2;
constexpr unsigned int kFlagTcpFlood       = 1u << 3;
constexpr unsigned int kFlagUdpFlood       = 1u << 4;
constexpr unsigned int kFlagChatFlood      = 1u << 5;
constexpr unsigned int kFlagChatBad        = 1u << 6;
constexpr unsigned int kFlagBadInputShape  = 1u << 7;
constexpr unsigned int kFlagUdpIpMismatch  = 1u << 8;
constexpr unsigned int kFlagAimSnapBurst   = 1u << 9;
constexpr unsigned int kFlagBadUsername    = 1u << 10;
constexpr unsigned int kFlagRegisterAbuse  = 1u << 11;
constexpr unsigned int kFlagIdleAuth       = 1u << 12;
constexpr unsigned int kFlagInputIdle      = 1u << 13;
constexpr unsigned int kFlagBadHmac        = 1u << 14;
constexpr long long kPreLoginIdleMs        = 30000;
constexpr long long kInMatchInputIdleMs    = 30000;
constexpr long long kRegisterWindowMs      = 3600000;
constexpr int kMaxRegisterPerIpPerHour     = 5;
constexpr int kAimSnapBurstThreshold       = 25;
constexpr float kAimSnapRad                = 2.8f;
constexpr int kAimSnapKickThreshold        = 60;
constexpr int kBadHmacKickThreshold        = 5;
constexpr int kUdpFloodKickThreshold       = 5;
constexpr int kBadInputKickThreshold       = 30;
bool isLoopbackIp(unsigned int ipKey) {
    return (ipKey >> 24) == 127;
}
bool validUsernameChars(const std::string& s) {
    for (char ch : s) {
        unsigned char c = (unsigned char)ch;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
               || (c >= '0' && c <= '9')
               || c == '_' || c == '.' || c == '-';
        if (!ok) return false;
    }
    return true;
}
std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    for (auto& c : out) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return out;
}
std::string trimWhitespace(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= 0x20) ++a;
    while (b > a && (unsigned char)s[b - 1] <= 0x20) --b;
    return s.substr(a, b - a);
}
bool finiteFloat(float f) {
    return f == f && f != std::numeric_limits<float>::infinity()
                  && f != -std::numeric_limits<float>::infinity();
}
std::string makeRandomHex(std::size_t bytes) {
    std::vector<unsigned char> buf(bytes);
    if (!cg::randomBytes(buf.data(), bytes)) return std::string();
    return cg::bytesToHex(buf.data(), bytes);
}
std::string makeUdpToken() {
    unsigned char buf[8];
    if (!cg::randomBytes(buf, 8)) return std::string();
    static const char kHex[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 0; i < 8; ++i) {
        out[2 * i]     = kHex[(buf[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[buf[i] & 0xF];
    }
    return out;
}
void setNonBlock(cg_socket_t fd) {
    cg_set_nonblock(fd);
}
long long nowEpochSec() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
long long nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}
Server::Server(const ServerConfig& cfg) : cfg_(cfg), auth_(cfg.dbPath) {}
Server::~Server() { closeSockets(); }
bool Server::bindSockets() {
    cg_netinit();
    tcpFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tcpFd_ == CG_INVALID_SOCKET) { std::cerr << "socket(tcp) failed err=" << cg_lasterr() << std::endl; return false; }
    int yes = 1;
    setsockopt(tcpFd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg_.port);
    if (cfg_.host == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, cfg_.host.c_str(), &addr.sin_addr) != 1) {
            std::cerr << "invalid host: " << cfg_.host << std::endl;
            return false;
        }
    }
    if (::bind(tcpFd_, (sockaddr*)&addr, sizeof(addr)) == CG_SOCKET_ERROR) {
        std::cerr << "bind(tcp) failed err=" << cg_lasterr() << std::endl; return false;
    }
    if (::listen(tcpFd_, 32) == CG_SOCKET_ERROR) {
        std::cerr << "listen failed err=" << cg_lasterr() << std::endl; return false;
    }
    setNonBlock(tcpFd_);
    udpFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udpFd_ == CG_INVALID_SOCKET) { std::cerr << "socket(udp) failed err=" << cg_lasterr() << std::endl; return false; }
    setsockopt(udpFd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    if (::bind(udpFd_, (sockaddr*)&addr, sizeof(addr)) == CG_SOCKET_ERROR) {
        std::cerr << "bind(udp) failed err=" << cg_lasterr() << std::endl; return false;
    }
    setNonBlock(udpFd_);
    return true;
}
void Server::closeSockets() {
    for (auto& kv : clients_) {
        if (kv.second.fd != CG_INVALID_SOCKET) cg_close(kv.second.fd);
    }
    clients_.clear();
    if (tcpFd_ != CG_INVALID_SOCKET) { cg_close(tcpFd_); tcpFd_ = CG_INVALID_SOCKET; }
    if (udpFd_ != CG_INVALID_SOCKET) { cg_close(udpFd_); udpFd_ = CG_INVALID_SOCKET; }
}
void Server::sendRaw(Client& c, const std::string& bytes) {
    if (c.fd == CG_INVALID_SOCKET || !c.tls) return;
    c.sendBuf += bytes;
    if (!c.tlsReady) return;
    while (!c.sendBuf.empty()) {
        std::size_t wrote = 0;
        TlsConn::Status st = c.tls->write(c.sendBuf.data(), c.sendBuf.size(), wrote);
        if (st == TlsConn::Status::Ok && wrote > 0) {
            c.sendBuf.erase(0, wrote);
        } else if (st == TlsConn::Status::WantRead || st == TlsConn::Status::WantWrite) {
            break;
        } else {
            c.wantClose = true;
            break;
        }
    }
}
void Server::sendToClient(Client& c, const std::string& msg) {
    sendRaw(c, msg + "\n");
}
void Server::sendUdpTo(const sockaddr_in& addr, const std::string& msg) {
    if (udpFd_ == CG_INVALID_SOCKET) return;
    ::sendto(udpFd_, msg.data(), (int)msg.size(), 0, (const sockaddr*)&addr, sizeof(addr));
}
void Server::acceptNewClient() {
    while (true) {
        sockaddr_in addr{};
        socklen_t alen = sizeof(addr);
        cg_socket_t fd = ::accept(tcpFd_, (sockaddr*)&addr, &alen);
        if (fd == CG_INVALID_SOCKET) {
            int e = cg_lasterr();
            if (e == CG_EAGAIN || e == CG_EWOULDBLOCK) return;
            std::cerr << "accept failed err=" << e << std::endl; return;
        }
        setNonBlock(fd);
        unsigned int ipKey = ntohl(addr.sin_addr.s_addr);
        if (ipConnCount_[ipKey] >= kMaxConnPerIp) {
            std::cerr << "[antichea] reject conn from "
                      << inet_ntoa(addr.sin_addr) << " (per-IP cap)" << std::endl;
            cg_close(fd);
            continue;
        }
        ipConnCount_[ipKey]++;
        int yes = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));
        Client c;
        c.id = nextClientId_++;
        c.fd = fd;
        c.addr = addr;
        c.connectedAtMs = nowMillis();
        c.lastInputAtMs = c.connectedAtMs;
        c.tls = std::make_unique<TlsConn>();
        std::string tlsErr;
        if (!c.tls->attach(tlsCtx_, /*isServer=*/true, (std::uintptr_t)fd, tlsErr)) {
            std::cerr << "[tls] attach failed: " << tlsErr << std::endl;
            cg_close(fd);
            continue;
        }
        clients_[c.id] = std::move(c);
        std::cerr << "[srv] accepted client " << clients_[c.id].id << " (TLS handshaking)" << std::endl;
    }
}
void Server::onTlsReady(Client& c) {
    if (c.tlsReady) return;
    c.tlsReady = true;
    std::vector<std::string> hello = {
        proto::kT_Hello, kHelloVerStr,
        std::to_string(proto::kTickRate),
        std::to_string(cfg_.teamSize),
    };
    sendToClient(c, proto::encodeLine(hello));
    std::cerr << "[tls] client " << c.id << " handshake done" << std::endl;
}
void Server::readFromClient(Client& c) {
    if (!c.tls) { c.wantClose = true; return; }
    if (!c.tlsReady) {
        TlsConn::Status st = c.tls->handshake();
        if (st == TlsConn::Status::Ok) onTlsReady(c);
        else if (st == TlsConn::Status::WantRead || st == TlsConn::Status::WantWrite) return;
        else { c.wantClose = true; return; }
        if (!c.tlsReady) return;
    }
    char buf[4096];
    while (true) {
        std::size_t got = 0;
        TlsConn::Status st = c.tls->read(buf, sizeof(buf), got);
        if (st == TlsConn::Status::Ok && got > 0) {
            c.recvBuf.append(buf, buf + got);
        } else if (st == TlsConn::Status::WantRead || st == TlsConn::Status::WantWrite) {
            break;
        } else {
            c.wantClose = true; break;
        }
    }
    if (!c.sendBuf.empty()) sendRaw(c, std::string{});
    while (true) {
        auto pos = c.recvBuf.find('\n');
        if (pos == std::string::npos) break;
        std::string line = c.recvBuf.substr(0, pos);
        c.recvBuf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        dispatchTcp(c, line);
        if (c.wantClose) break;
    }
}
void Server::sendLoginOk(Client& c) {
    User u;
    if (!auth_.loadUserByIdPublic(c.userId, u)) {
        sendToClient(c, proto::encodeLine({proto::kT_Err, "user load failed"}));
        return;
    }
    int creditsVal = auth_.credits(c.userId);
    int sel = auth_.selectedWeapon(c.userId);
    if (!auth_.ownsWeapon(c.userId, sel)) sel = proto::kWeaponPistol;
    c.credits = creditsVal;
    c.selectedWeapon = sel;
    std::vector<std::string> fields = {
        proto::kT_LoginOk,
        std::to_string(u.id),
        u.username,
        u.team,
        std::to_string(u.xp),
        std::to_string(u.level),
        std::to_string(u.totalKills),
        std::to_string(u.totalDeaths),
        std::to_string(u.totalHeadshots),
        std::to_string(u.matchesPlayed),
        std::to_string(u.matchesWon),
        c.udpToken,
        std::to_string(c.credits),
        std::to_string(c.selectedWeapon),
        c.udpHmacKey,
    };
    sendToClient(c, proto::encodeLine(fields));
}
void Server::broadcastChat(const std::string& room, const std::string& user,
                           const std::string& content, long long sentAt) {
    std::vector<std::string> fields = {
        proto::kT_ChatMsg, room, user, content, std::to_string(sentAt),
    };
    std::string line = proto::encodeLine(fields);
    for (auto& kv : clients_) {
        auto& c = kv.second;
        if (!c.loggedIn) continue;
        if (room == "team_blue" && c.team != "BLUE") continue;
        if (room == "team_red"  && c.team != "RED")  continue;
        sendToClient(c, line);
    }
}
void Server::sendQueueStatus() {
    int blue = 0, red = 0;
    for (int cid : queue_) {
        auto it = clients_.find(cid);
        if (it == clients_.end()) continue;
        if (it->second.team == "BLUE") ++blue; else ++red;
    }
    int need = 2 * cfg_.teamSize;
    std::vector<std::string> fields = {
        proto::kT_QueueStatus, "?",
        std::to_string(blue), std::to_string(red), std::to_string(need),
    };
    std::string line = proto::encodeLine(fields);
    for (int cid : queue_) {
        auto it = clients_.find(cid);
        if (it == clients_.end()) continue;
        sendToClient(it->second, line);
    }
}
std::string Server::pickRandomMap() {
    static std::mt19937 rng((std::random_device{})());
    auto maps = allMapNames();
    if (maps.empty()) return "Arena";
    std::uniform_int_distribution<size_t> dist(0, maps.size() - 1);
    return maps[dist(rng)];
}
void Server::launchMatch(std::vector<int> candidateCids) {
    const int teamSize = cfg_.teamSize;
    std::vector<int> blueIds, redIds, requeue;
    for (int cid : candidateCids) {
        auto it = clients_.find(cid);
        if (it == clients_.end() || !it->second.loggedIn) continue;
        if (it->second.team == "BLUE") blueIds.push_back(cid);
        else redIds.push_back(cid);
    }
    while ((int)blueIds.size() > teamSize) {
        requeue.push_back(blueIds.back());
        blueIds.pop_back();
    }
    while ((int)redIds.size() > teamSize) {
        requeue.push_back(redIds.back());
        redIds.pop_back();
    }
    for (int cid : requeue) {
        auto it = clients_.find(cid);
        if (it == clients_.end()) continue;
        it->second.queued = true;
        queue_.push_back(cid);
    }
    if (blueIds.empty() && redIds.empty()) return;
    const std::string map = pickRandomMap();
    int matchId = nextMatchId_++;
    auto match = std::make_unique<Match>(matchId, map, teamSize);
    auto addOne = [&](int cid) {
        auto it = clients_.find(cid);
        if (it == clients_.end()) return;
        auto& c = it->second;
        const std::string& team = c.team;
        int wid = c.selectedWeapon;
        if (!auth_.ownsWeapon(c.userId, wid)) wid = proto::kWeaponPistol;
        int slot = match->addPlayer(c.id, c.userId, c.username, team, wid);
        c.matchId = matchId;
        c.slotInMatch = slot;
        c.queued = false;
        c.hadFirstInput = false;
        c.aimSnapCount = 0;
        c.lastInputAtMs = nowMillis();
    };
    for (int cid : blueIds) addOne(cid);
    for (int cid : redIds) addOne(cid);
    int blueFill = teamSize - (int)blueIds.size();
    int redFill  = teamSize - (int)redIds.size();
    int botNum = 1;
    for (int i = 0; i < blueFill; ++i) {
        char nm[24]; std::snprintf(nm, sizeof(nm), "Bot%d", botNum++);
        match->addBot(nm, "BLUE", (i % proto::kWeaponCount) + 1);
    }
    for (int i = 0; i < redFill; ++i) {
        char nm[24]; std::snprintf(nm, sizeof(nm), "Bot%d", botNum++);
        match->addBot(nm, "RED", (i % proto::kWeaponCount) + 1);
    }
    const auto& players = match->slots();
    for (const auto& p : players) {
        if (p.isBot) continue;
        auto it = clients_.find(p.clientId);
        if (it == clients_.end()) continue;
        auto& c = it->second;
        sendToClient(c, proto::encodeLine({
            proto::kT_MatchStart, map, p.team, std::to_string((int)players.size()),
        }));
        for (size_t i = 0; i < players.size(); ++i) {
            const auto& q2 = players[i];
            sendToClient(c, proto::encodeLine({
                proto::kT_MatchPlayer,
                std::to_string((int)i),
                std::to_string(q2.userId),
                q2.username,
                q2.team,
            }));
        }
    }
    matches_[matchId] = std::move(match);
    std::cerr << "[srv] match " << matchId << " started on " << map
              << " with " << players.size() << " players ("
              << blueIds.size() << " blue, " << redIds.size() << " red, "
              << (blueFill + redFill) << " bots)" << std::endl;
}
void Server::startMatchWithBotFill() {
    if (queue_.empty()) return;
    std::vector<int> chosen(queue_.begin(), queue_.end());
    queue_.clear();
    queueWaitStartMs_ = 0;
    launchMatch(chosen);
}
void Server::enforceIdleDisconnects(long long nowMs) {
    for (auto& kv : clients_) {
        Client& c = kv.second;
        if (c.wantClose) continue;
        if (!c.loggedIn) {
            if (nowMs - c.connectedAtMs > kPreLoginIdleMs) {
                c.suspicionFlags |= kFlagIdleAuth;
                c.wantClose = true;
            }
            continue;
        }
        if (c.matchId >= 0 && nowMs - c.lastInputAtMs > kInMatchInputIdleMs) {
            c.suspicionFlags |= kFlagInputIdle;
            std::cerr << "[antichea] in-match input idle client=" << c.id
                      << " user=" << c.username << std::endl;
            leaveMatchOrQueue(c, true);
        }
    }
}
void Server::maybeBotFillTimeout(long long nowMs) {
    if (queue_.empty()) return;
    int need = 2 * cfg_.teamSize;
    if ((int)queue_.size() >= need) return;
    if (queueWaitStartMs_ == 0) return;
    long long timeoutMs = (long long)cfg_.botFillSecs * 1000;
    if (timeoutMs <= 0) return;
    if (nowMs - queueWaitStartMs_ < timeoutMs) return;
    startMatchWithBotFill();
}
void Server::tryStartMatch() {
    int need = 2 * cfg_.teamSize;
    if ((int)queue_.size() < need) return;
    std::vector<int> chosen(queue_.begin(), queue_.begin() + need);
    queue_.erase(queue_.begin(), queue_.begin() + need);
    launchMatch(chosen);
}
void Server::leaveMatchOrQueue(Client& c, bool announce, bool immediateForfeit) {
    if (c.queued) {
        queue_.erase(std::remove(queue_.begin(), queue_.end(), c.id), queue_.end());
        c.queued = false;
        if (queue_.empty()) queueWaitStartMs_ = 0;
        if (announce) sendQueueStatus();
    }
    if (c.matchId >= 0) {
        auto it = matches_.find(c.matchId);
        if (it != matches_.end()) {
            if (immediateForfeit || c.userId == 0) {
                bool over = it->second->forfeit(c.id);
                if (over) recordAndAnnounceMatchEnd(*it->second);
            } else {
                it->second->zeroInputAt(c.slotInMatch);
                OrphanMatchSlot orphan;
                orphan.matchId = c.matchId;
                orphan.slotIndex = c.slotInMatch;
                orphan.expiresAtMs = nowMillis() + 30000;
                orphanSlots_[c.userId] = orphan;
                std::cerr << "[srv] orphan match slot user=" << c.username
                          << " match=" << c.matchId << " slot=" << c.slotInMatch
                          << " (30s grace)" << std::endl;
            }
        }
        c.matchId = -1;
        c.slotInMatch = -1;
    }
}
bool Server::tryReattachOrphan(Client& c) {
    auto it = orphanSlots_.find(c.userId);
    if (it == orphanSlots_.end()) return false;
    OrphanMatchSlot orphan = it->second;
    orphanSlots_.erase(it);
    auto mit = matches_.find(orphan.matchId);
    if (mit == matches_.end()) return false;
    mit->second->reattachSlot(orphan.slotIndex, c.id);
    c.matchId = orphan.matchId;
    c.slotInMatch = orphan.slotIndex;
    const auto& players = mit->second->slots();
    if (orphan.slotIndex < 0 || orphan.slotIndex >= (int)players.size()) return false;
    sendToClient(c, proto::encodeLine({
        proto::kT_Reconnected,
        std::to_string(orphan.matchId),
        std::to_string(orphan.slotIndex),
    }));
    sendToClient(c, proto::encodeLine({
        proto::kT_MatchStart, mit->second->map(),
        players[orphan.slotIndex].team,
        std::to_string((int)players.size()),
    }));
    for (size_t i = 0; i < players.size(); ++i) {
        sendToClient(c, proto::encodeLine({
            proto::kT_MatchPlayer,
            std::to_string((int)i),
            std::to_string(players[i].userId),
            players[i].username,
            players[i].team,
        }));
    }
    std::cerr << "[srv] reattach user=" << c.username
              << " match=" << orphan.matchId << " slot=" << orphan.slotIndex << std::endl;
    return true;
}
void Server::expireOrphans(long long nowMs) {
    for (auto it = orphanSlots_.begin(); it != orphanSlots_.end(); ) {
        if (it->second.expiresAtMs > nowMs) { ++it; continue; }
        auto mit = matches_.find(it->second.matchId);
        if (mit != matches_.end()) {
            const auto& slots = mit->second->slots();
            if (it->second.slotIndex >= 0 && it->second.slotIndex < (int)slots.size()) {
                bool over = mit->second->forfeit(slots[it->second.slotIndex].clientId);
                std::cerr << "[srv] orphan expired user=" << it->first
                          << " (forfeit match " << it->second.matchId << ")" << std::endl;
                if (over) recordAndAnnounceMatchEnd(*mit->second);
            }
        }
        it = orphanSlots_.erase(it);
    }
}
void Server::disconnectClient(int clientId) {
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    Client& c = it->second;
    leaveMatchOrQueue(c, true, /*immediateForfeit=*/false);
    if (c.suspicionFlags) {
        std::cerr << "[antichea] disconnect client " << clientId
                  << " user=" << c.username
                  << " flags=0x" << std::hex << c.suspicionFlags << std::dec
                  << std::endl;
    }
    unsigned int ipKey = ntohl(c.addr.sin_addr.s_addr);
    auto ipIt = ipConnCount_.find(ipKey);
    if (ipIt != ipConnCount_.end()) {
        if (--ipIt->second <= 0) ipConnCount_.erase(ipIt);
    }
    if (c.fd != CG_INVALID_SOCKET) cg_close(c.fd);
    std::cerr << "[srv] disconnect client " << clientId << std::endl;
    clients_.erase(it);
}
Client* Server::clientByUdpToken(const std::string& tok) {
    if (tok.empty()) return nullptr;
    for (auto& kv : clients_) {
        if (kv.second.loggedIn && kv.second.udpToken == tok) return &kv.second;
    }
    return nullptr;
}
void Server::readUdp() {
    char buf[2048];
    sockaddr_in src{};
    socklen_t slen = sizeof(src);
    while (true) {
        cg_ssize_t n = ::recvfrom(udpFd_, buf, sizeof(buf), 0, (sockaddr*)&src, &slen);
        if (n <= 0) {
            if (n == CG_SOCKET_ERROR) {
                int e = cg_lasterr();
                if (e == CG_EAGAIN || e == CG_EWOULDBLOCK) return;
            }
            return;
        }
        std::string msg(buf, buf + n);
        auto fields = proto::splitDecode(msg);
        if (fields.empty()) continue;
        if (fields[0] == proto::kU_Input && fields.size() >= 9) {
            const std::string& tok = fields[1];
            Client* c = clientByUdpToken(tok);
            if (!c) continue;
            if (c->addr.sin_addr.s_addr != src.sin_addr.s_addr) {
                c->suspicionFlags |= kFlagUdpIpMismatch;
                continue;
            }
            if (c->udpHmacKey.empty()) continue;
            const std::string& givenTag = fields.back();
            std::vector<std::string> body(fields.begin(), fields.end() - 1);
            std::string expectTag = cg::hmacSha256Hex(c->udpHmacKey, proto::encodeLine(body));
            if (givenTag.size() != expectTag.size() ||
                std::memcmp(givenTag.data(), expectTag.data(), givenTag.size()) != 0) {
                c->suspicionFlags |= kFlagBadHmac;
                if (++c->badHmacCount >= kBadHmacKickThreshold) {
                    c->wantClose = true;
                    std::cerr << "[antichea] auto-kick client=" << c->id
                              << " user=" << c->username << " (bad HMAC ×"
                              << c->badHmacCount << ")" << std::endl;
                }
                continue;
            }
            long long nowMs = nowMillis();
            if (nowMs - c->udpWindowStartMs >= 1000) {
                c->udpWindowStartMs = nowMs;
                c->udpMsgCount = 0;
            }
            c->udpMsgCount++;
            if (c->udpMsgCount > kMaxUdpMsgsPerSec) {
                c->suspicionFlags |= kFlagUdpFlood;
                if (c->udpMsgCount == kMaxUdpMsgsPerSec + 1) {
                    if (++c->udpFloodCount >= kUdpFloodKickThreshold) {
                        c->wantClose = true;
                        std::cerr << "[antichea] auto-kick client=" << c->id
                                  << " user=" << c->username << " (UDP flood ×"
                                  << c->udpFloodCount << ")" << std::endl;
                    }
                }
                continue;
            }
            if (!c->udpKnown) {
                c->udpAddr = src;
                c->udpKnown = true;
            } else {
                c->udpAddr = src;
            }
            if (c->matchId < 0) continue;
            auto mit = matches_.find(c->matchId);
            if (mit == matches_.end()) continue;
            PlayerInput in;
            in.tick = std::atoi(fields[2].c_str());
            in.moveX = (float)std::atof(fields[3].c_str());
            in.moveZ = (float)std::atof(fields[4].c_str());
            in.yaw = (float)std::atof(fields[5].c_str());
            in.pitch = (float)std::atof(fields[6].c_str());
            in.fire = std::atoi(fields[7].c_str()) ? 1 : 0;
            if (!finiteFloat(in.moveX) || !finiteFloat(in.moveZ) ||
                !finiteFloat(in.yaw)   || !finiteFloat(in.pitch)) {
                c->suspicionFlags |= kFlagInputNonFinite;
                if (++c->badInputCount >= kBadInputKickThreshold) {
                    c->wantClose = true;
                    std::cerr << "[antichea] auto-kick client=" << c->id
                              << " user=" << c->username << " (bad input ×"
                              << c->badInputCount << ")" << std::endl;
                }
                continue;
            }
            float ml = std::sqrt(in.moveX * in.moveX + in.moveZ * in.moveZ);
            if (ml > 1.5f) {
                float inv = 1.0f / ml;
                in.moveX *= inv;
                in.moveZ *= inv;
            }
            if (in.pitch >  1.55f) in.pitch =  1.55f;
            if (in.pitch < -1.55f) in.pitch = -1.55f;
            int serverTick = mit->second->tickNo();
            if (c->lastInputTick >= 0 && in.tick + 5 < c->lastInputTick) {
                c->suspicionFlags |= kFlagInputOldTick;
                continue;
            }
            if (in.tick > serverTick + 120) {
                c->suspicionFlags |= kFlagInputFutureTick;
                continue;
            }
            c->lastInputTick = in.tick;
            if (c->hadFirstInput) {
                float dYaw = in.yaw - c->lastInputYaw;
                while (dYaw > 3.14159f)  dYaw -= 6.28318f;
                while (dYaw < -3.14159f) dYaw += 6.28318f;
                if (std::fabs(dYaw) > kAimSnapRad) {
                    c->aimSnapCount++;
                    if (c->aimSnapCount == kAimSnapBurstThreshold) {
                        c->suspicionFlags |= kFlagAimSnapBurst;
                        std::cerr << "[antichea] aim-snap burst client=" << c->id
                                  << " user=" << c->username
                                  << " snaps=" << c->aimSnapCount << std::endl;
                    }
                    if (c->aimSnapCount >= kAimSnapKickThreshold) {
                        c->wantClose = true;
                        std::cerr << "[antichea] auto-kick client=" << c->id
                                  << " user=" << c->username << " (aim-snap ×"
                                  << c->aimSnapCount << ")" << std::endl;
                    }
                }
            }
            c->lastInputYaw = in.yaw;
            c->hadFirstInput = true;
            c->lastInputAtMs = nowMs;
            mit->second->setInput(c->id, in);
        }
    }
}
void Server::recordAndAnnounceMatchEnd(Match& m) {
    std::vector<int> clientIds;
    std::vector<Slot> snap = m.slots();
    for (auto& s : snap) {
        if (s.isBot) continue;
        clientIds.push_back(s.clientId);
        MatchResult r = m.resultFor(s);
        User before;
        bool haveBefore = auth_.loadUserByIdPublic(s.userId, before);
        if (r.playerKills >= 10 && r.headshots * 10 >= r.playerKills * 9) {
            std::cerr << "[antichea] suspicious HS ratio user=" << s.username
                      << " kills=" << r.playerKills
                      << " hs=" << r.headshots << std::endl;
        }
        int xpEarned = auth_.recordMatchFor(s.userId, r);
        User after;
        bool haveAfter = auth_.loadUserByIdPublic(s.userId, after);
        int newLevel = haveAfter ? after.level : (haveBefore ? before.level : 1);
        bool leveledUp = (haveBefore && haveAfter) ? (after.level > before.level) : false;
        int creditsEarned = 50 * r.playerKills + 50 * r.headshots + (r.won ? 200 : 0);
        int newCredits = auth_.addCredits(s.userId, creditsEarned);
        auto cit = clients_.find(s.clientId);
        if (cit != clients_.end()) {
            cit->second.credits = newCredits;
        }
        if (cit == clients_.end()) continue;
        std::vector<std::string> fields = {
            proto::kT_MatchEnd,
            std::to_string(r.won ? 1 : 0),
            std::to_string(r.team == "BLUE" ? r.teamScore : r.enemyTeamScore),
            std::to_string(r.team == "BLUE" ? r.enemyTeamScore : r.teamScore),
            std::to_string(r.playerKills),
            std::to_string(r.playerDeaths),
            std::to_string(r.headshots),
            std::to_string(xpEarned),
            std::to_string(newLevel),
            std::to_string(leveledUp ? 1 : 0),
            std::to_string(creditsEarned),
            std::to_string(newCredits),
        };
        sendToClient(cit->second, proto::encodeLine(fields));
        cit->second.matchId = -1;
        cit->second.slotInMatch = -1;
    }
    matches_.erase(m.id());
}
void Server::tickMatches() {
    if (matches_.empty()) return;
    const float dt = 1.0f / (float)proto::kTickRate;
    std::vector<int> toEnd;
    for (auto& kv : matches_) {
        Match& m = *kv.second;
        m.computeBotInputs(dt);
        std::vector<KillEvent> kills;
        m.tick(dt, kills);
        for (const auto& s : m.slots()) {
            if (!s.active) continue;
            if (s.isBot) continue;
            auto cit = clients_.find(s.clientId);
            if (cit == clients_.end()) continue;
            std::string payload = m.buildStateFor(s);
            if (cit->second.udpKnown) {
                sendUdpTo(cit->second.udpAddr, payload);
            }
        }
        for (const auto& ev : kills) {
            std::vector<std::string> ef = {
                proto::kU_Event, "kill",
                std::to_string(ev.killerUid),
                std::to_string(ev.victimUid),
                std::to_string(ev.headshot),
            };
            std::string line = proto::encodeLine(ef);
            for (const auto& s : m.slots()) {
                if (!s.active) continue;
                if (s.isBot) continue;
                auto cit = clients_.find(s.clientId);
                if (cit == clients_.end()) continue;
                if (cit->second.udpKnown) sendUdpTo(cit->second.udpAddr, line);
                sendToClient(cit->second, line);
            }
        }
        if (m.ended()) toEnd.push_back(kv.first);
    }
    for (int id : toEnd) {
        auto it = matches_.find(id);
        if (it != matches_.end()) recordAndAnnounceMatchEnd(*it->second);
    }
}
void Server::dispatchTcp(Client& c, const std::string& line) {
    long long nowMs = nowMillis();
    if (nowMs - c.tcpWindowStartMs >= 1000) {
        c.tcpWindowStartMs = nowMs;
        c.tcpMsgCount = 0;
    }
    c.tcpMsgCount++;
    if (c.tcpMsgCount > kMaxTcpMsgsPerSec) {
        c.suspicionFlags |= kFlagTcpFlood;
        if (c.tcpMsgCount > kMaxTcpMsgsPerSec * 4) {
            c.wantClose = true;
        } else if (c.tcpMsgCount == kMaxTcpMsgsPerSec + 1) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "rate limit"}));
        }
        return;
    }
    if (line.size() > 4096) {
        c.suspicionFlags |= kFlagBadInputShape;
        sendToClient(c, proto::encodeLine({proto::kT_Err, "oversized message"}));
        return;
    }
    auto f = proto::splitDecode(line);
    if (f.empty()) return;
    const std::string& type = f[0];
    if (type == proto::kT_Ping) {
        sendToClient(c, proto::encodeLine({proto::kT_Pong}));
        return;
    }
    if (type == proto::kT_Register) {
        if (f.size() < 3) { sendToClient(c, proto::encodeLine({proto::kT_Err, "bad register"})); return; }
        if (f[1].size() < 3 || (int)f[1].size() > kMaxUsernameLen ||
            f[2].size() < 4 || (int)f[2].size() > kMaxPasswordLen) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "invalid username or password"}));
            return;
        }
        if (!validUsernameChars(f[1])) {
            c.suspicionFlags |= kFlagBadUsername;
            sendToClient(c, proto::encodeLine({proto::kT_Err, "username has invalid characters"}));
            return;
        }
        unsigned int ipKey = ntohl(c.addr.sin_addr.s_addr);
        if (!isLoopbackIp(ipKey)) {
            auto& log = ipRegisterLog_[ipKey];
            long long cutoff = nowMs - kRegisterWindowMs;
            log.erase(std::remove_if(log.begin(), log.end(),
                      [cutoff](long long t){ return t < cutoff; }), log.end());
            if ((int)log.size() >= kMaxRegisterPerIpPerHour) {
                c.suspicionFlags |= kFlagRegisterAbuse;
                std::cerr << "[antichea] register throttle ip="
                          << inet_ntoa(c.addr.sin_addr) << std::endl;
                sendToClient(c, proto::encodeLine({proto::kT_Err, "too many registrations from this address"}));
                return;
            }
            log.push_back(nowMs);
        }
        if (!auth_.registerUser(f[1], f[2])) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "register failed"}));
            return;
        }
        User u;
        if (auth_.loadUserByNamePublic(f[1], u)) {
            sendToClient(c, proto::encodeLine({proto::kT_RegisterOk, u.team}));
        } else {
            sendToClient(c, proto::encodeLine({proto::kT_RegisterOk, "BLUE"}));
        }
        return;
    }
    if (type == proto::kT_Login) {
        if (f.size() < 3) { sendToClient(c, proto::encodeLine({proto::kT_Err, "bad login"})); return; }
        if (f[1].empty() || (int)f[1].size() > kMaxUsernameLen ||
            f[2].empty() || (int)f[2].size() > kMaxPasswordLen) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "invalid credentials"}));
            return;
        }
        std::string key = toLowerCopy(f[1]);
        auto& rec = loginAttempts_[key];
        if (rec.lockUntilMs > nowMs) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "too many attempts; try later"}));
            return;
        }
        User u;
        bool ok = auth_.loadUserByNamePublic(f[1], u) && auth_.login(f[1], f[2]);
        if (!ok) {
            if (nowMs - rec.windowStartMs > kLoginFailWindowMs) {
                rec.windowStartMs = nowMs;
                rec.failCount = 0;
            }
            rec.failCount++;
            if (rec.failCount >= kLoginFailLimit) {
                rec.lockUntilMs = nowMs + kLoginLockMs;
                std::cerr << "[antichea] login lock username=" << key
                          << " for " << (kLoginLockMs / 1000) << "s" << std::endl;
            }
            sendToClient(c, proto::encodeLine({proto::kT_Err, "invalid credentials"}));
            return;
        }
        rec.failCount = 0;
        rec.lockUntilMs = 0;
        auth_.loadUserByNamePublic(f[1], u);
        auth_.logout();
        c.loggedIn = true;
        c.userId = u.id;
        c.username = u.username;
        c.team = u.team;
        c.udpToken = makeUdpToken();
        c.udpHmacKey = makeRandomHex(32);
        if (c.udpToken.empty() || c.udpHmacKey.empty()) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "server rng failed"}));
            return;
        }
        sendLoginOk(c);
        tryReattachOrphan(c);
        return;
    }
    if (type == proto::kT_Logout) {
        leaveMatchOrQueue(c, true);
        c.loggedIn = false; c.userId = 0; c.username.clear(); c.team.clear();
        c.udpToken.clear(); c.udpHmacKey.clear(); c.udpKnown = false;
        sendToClient(c, proto::encodeLine({proto::kT_Ok, "logout"}));
        return;
    }
    if (!c.loggedIn) {
        sendToClient(c, proto::encodeLine({proto::kT_Err, "not logged in"}));
        return;
    }
    if (type == proto::kT_ChatSend) {
        if (f.size() < 3) { sendToClient(c, proto::encodeLine({proto::kT_Err, "bad chat"})); return; }
        const std::string& room = f[1];
        if ((int)f[2].size() > kMaxChatLen) {
            c.suspicionFlags |= kFlagChatBad;
            sendToClient(c, proto::encodeLine({proto::kT_Err, "message too long"}));
            return;
        }
        std::string content = trimWhitespace(f[2]);
        if (content.empty()) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "empty message"}));
            return;
        }
        if (nowMs - c.lastChatMs < kChatMinIntervalMs) {
            c.suspicionFlags |= kFlagChatFlood;
            sendToClient(c, proto::encodeLine({proto::kT_Err, "chat rate limit"}));
            return;
        }
        if (!auth_.sendMessageAs(c.userId, c.team, room, content)) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "message rejected"}));
            return;
        }
        c.lastChatMs = nowMs;
        broadcastChat(room, c.username, content, nowEpochSec());
        return;
    }
    if (type == proto::kT_ChatHistory) {
        if (f.size() < 2) { sendToClient(c, proto::encodeLine({proto::kT_Err, "bad history"})); return; }
        const std::string& room = f[1];
        auto msgs = auth_.recentMessagesFor(c.team, room, 100);
        sendToClient(c, proto::encodeLine({proto::kT_ChatBatchBeg, room, std::to_string((int)msgs.size())}));
        for (const auto& m : msgs) {
            sendToClient(c, proto::encodeLine({
                proto::kT_ChatMsg, m.room, m.username, m.content, std::to_string(m.sentAt),
            }));
        }
        sendToClient(c, proto::encodeLine({proto::kT_ChatBatchEnd, room}));
        return;
    }
    if (type == proto::kT_QueueJoin) {
        if (c.matchId >= 0) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "already in match"})); return;
        }
        if (!c.queued) {
            bool wasEmpty = queue_.empty();
            queue_.push_back(c.id);
            c.queued = true;
            if (wasEmpty) queueWaitStartMs_ = nowMillis();
        }
        sendQueueStatus();
        tryStartMatch();
        if (queue_.empty()) queueWaitStartMs_ = 0;
        return;
    }
    if (type == proto::kT_QueueLeave) {
        leaveMatchOrQueue(c, true);
        sendToClient(c, proto::encodeLine({proto::kT_Ok, "queue_leave"}));
        return;
    }
    if (type == proto::kT_Reload) {
        if (c.matchId < 0) return;
        auto it = matches_.find(c.matchId);
        if (it == matches_.end()) return;
        for (auto& s : it->second->slotsMut()) {
            if (s.clientId != c.id) continue;
            const Weapon* w = weapons::lookup(s.weaponId);
            if (!w) w = weapons::lookup(proto::kWeaponPistol);
            if (s.alive && s.reloadTimer == 0.0f && s.mag < w->magSize && s.reserve > 0) {
                s.reloadTimer = w->reloadSec;
            }
            break;
        }
        return;
    }
    if (type == proto::kT_LeaveMatch) {
        leaveMatchOrQueue(c, true);
        sendToClient(c, proto::encodeLine({proto::kT_Ok, "leave_match"}));
        return;
    }
    if (type == proto::kT_StoreList) {
        int sel = auth_.selectedWeapon(c.userId);
        for (int i = 0; i < weapons::kCount; ++i) {
            const Weapon& w = weapons::kTable[i];
            int owned = auth_.ownsWeapon(c.userId, w.id) ? 1 : 0;
            int selected = (sel == w.id) ? 1 : 0;
            int cdMs = (int)(w.cooldownSec * 1000.0f);
            std::vector<std::string> fields = {
                proto::kT_StoreItem,
                std::to_string(w.id),
                w.name,
                std::to_string(w.price),
                std::to_string(w.damageBody),
                std::to_string(w.damageHs),
                std::to_string(w.magSize),
                std::to_string(w.reserve),
                std::to_string(cdMs),
                std::to_string(owned),
                std::to_string(selected),
            };
            sendToClient(c, proto::encodeLine(fields));
        }
        int creditsVal = auth_.credits(c.userId);
        c.credits = creditsVal;
        sendToClient(c, proto::encodeLine({proto::kT_StoreEnd, std::to_string(creditsVal)}));
        return;
    }
    if (type == proto::kT_StoreBuy) {
        if (f.size() < 2) { sendToClient(c, proto::encodeLine({proto::kT_Err, "bad buy"})); return; }
        int wid = std::atoi(f[1].c_str());
        int rc = auth_.buyWeapon(c.userId, wid);
        if (rc == 0) {
            int newCredits = auth_.credits(c.userId);
            c.credits = newCredits;
            sendToClient(c, proto::encodeLine({
                proto::kT_StoreBought,
                std::to_string(wid),
                std::to_string(newCredits),
            }));
        } else if (rc == -1) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "already owned"}));
        } else if (rc == -2) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "not enough credits"}));
        } else {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "unknown weapon"}));
        }
        return;
    }
    if (type == proto::kT_WeaponSelect) {
        if (f.size() < 2) { sendToClient(c, proto::encodeLine({proto::kT_Err, "bad select"})); return; }
        int wid = std::atoi(f[1].c_str());
        if (!weapons::lookup(wid)) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "unknown weapon"}));
            return;
        }
        if (!auth_.selectWeapon(c.userId, wid)) {
            sendToClient(c, proto::encodeLine({proto::kT_Err, "not owned"}));
            return;
        }
        c.selectedWeapon = wid;
        sendToClient(c, proto::encodeLine({proto::kT_WeaponOk, std::to_string(wid)}));
        return;
    }
    if (type == proto::kT_LeaderXP || type == proto::kT_LeaderKills || type == proto::kT_LeaderWin) {
        std::vector<LeaderEntry> rows;
        std::string kind = type;
        if (type == proto::kT_LeaderXP) rows = auth_.topByXP(10);
        else if (type == proto::kT_LeaderKills) rows = auth_.topByKills(10);
        else rows = auth_.topByWinRate(10, 1);
        int rank = 1;
        for (const auto& e : rows) {
            std::vector<std::string> fields = {
                proto::kT_LeaderRow, kind,
                std::to_string(rank++),
                e.username,
                std::to_string(e.level),
                std::to_string(e.xp),
                std::to_string(e.totalKills),
                std::to_string(e.totalDeaths),
                std::to_string(e.matchesPlayed),
                std::to_string(e.matchesWon),
                std::to_string(e.totalHeadshots),
                e.rank,
            };
            sendToClient(c, proto::encodeLine(fields));
        }
        sendToClient(c, proto::encodeLine({proto::kT_LeaderEnd, kind}));
        return;
    }
    sendToClient(c, proto::encodeLine({proto::kT_Err, std::string("unknown message: ") + type}));
}
int Server::run() {
    ready_.store(false);
    if (!bindSockets()) return 1;
    std::cerr << "claudegame_server v" << kServerVersion
              << " team_size=" << cfg_.teamSize
              << " db=" << cfg_.dbPath << std::endl;
    {
        std::string certDir = cg::userDataDir();
        std::string certPath = certDir + "/server.crt";
        std::string keyPath  = certDir + "/server.key";
        std::string certPem, keyPem;
        auto slurp = [](const std::string& p, std::string& out) {
            std::ifstream f(p, std::ios::binary);
            if (!f.good()) return false;
            out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            return !out.empty();
        };
        if (!(slurp(certPath, certPem) && slurp(keyPath, keyPem))) {
            std::cerr << "[tls] generating new self-signed cert at " << certPath << std::endl;
            std::string err;
            if (!tls::generateSelfSignedCert("claudegame-server", certPem, keyPem, err)) {
                std::cerr << "[tls] cert generation failed: " << err << std::endl;
                return 1;
            }
            std::ofstream(certPath, std::ios::binary).write(certPem.data(), certPem.size());
            std::ofstream(keyPath,  std::ios::binary).write(keyPem.data(),  keyPem.size());
        }
        std::string err;
        if (!tlsCtx_.initServer(certPem, keyPem, err)) {
            std::cerr << "[tls] context init failed: " << err << std::endl;
            return 1;
        }
        std::cerr << "[tls] cert fingerprint sha256=" << tls::certFingerprintHex(certPem) << std::endl;
    }
    std::cerr << "listening on " << cfg_.host << ":" << cfg_.port << std::endl;
    ready_.store(true);
    long long nextTick = nowMillis();
    const long long tickMs = 1000 / proto::kTickRate;
    while (!stop_) {
        long long now = nowMillis();
        long long until = nextTick - now;
        if (until < 0) until = 0;
        if (until > 100) until = 100;
        fd_set rfds;
        FD_ZERO(&rfds);
        cg_socket_t maxFd = 0;
        FD_SET(tcpFd_, &rfds); if (tcpFd_ > maxFd) maxFd = tcpFd_;
        FD_SET(udpFd_, &rfds); if (udpFd_ > maxFd) maxFd = udpFd_;
        for (auto& kv : clients_) {
            cg_socket_t fd = kv.second.fd;
            if (fd != CG_INVALID_SOCKET) {
                FD_SET(fd, &rfds);
                if (fd > maxFd) maxFd = fd;
            }
        }
        timeval tv;
        tv.tv_sec = (long)(until / 1000);
        tv.tv_usec = (long)((until % 1000) * 1000);
        int rc = ::select((int)(maxFd + 1), &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            int e = cg_lasterr();
            if (e == CG_EINTR) continue;
            std::cerr << "select err=" << e << std::endl; break;
        }
        if (rc > 0) {
            if (FD_ISSET(tcpFd_, &rfds)) acceptNewClient();
            if (FD_ISSET(udpFd_, &rfds)) readUdp();
            std::vector<int> ids;
            ids.reserve(clients_.size());
            for (auto& kv : clients_) ids.push_back(kv.first);
            for (int id : ids) {
                auto it = clients_.find(id);
                if (it == clients_.end()) continue;
                if (it->second.fd == CG_INVALID_SOCKET) continue;
                if (FD_ISSET(it->second.fd, &rfds)) {
                    readFromClient(it->second);
                }
            }
        }
        std::vector<int> dead;
        for (auto& kv : clients_) if (kv.second.wantClose) dead.push_back(kv.first);
        for (int id : dead) disconnectClient(id);
        now = nowMillis();
        enforceIdleDisconnects(now);
        maybeBotFillTimeout(now);
        expireOrphans(now);
        if (now >= nextTick) {
            tickMatches();
            nextTick += tickMs;
            if (nextTick < now - 500) nextTick = now + tickMs;
        }
    }
    std::cerr << "[srv] shutting down" << std::endl;
    ready_.store(false);
    closeSockets();
    return 0;
}
