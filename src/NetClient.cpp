#include "PlatformNet.h"
#include "NetClient.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
NetClient::~NetClient() {
    disconnect();
}
bool NetClient::connect(const std::string& host, uint16_t port, std::string& errOut) {
    disconnect();
    cg_netinit();
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    int rc = ::getaddrinfo(host.c_str(), portStr, &hints, &res);
    if (rc != 0 || !res) {
        errOut = std::string("getaddrinfo: ") + gai_strerror(rc);
        return false;
    }
    cg_socket_t fd = CG_INVALID_SOCKET;
    addrinfo* used = nullptr;
    for (addrinfo* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == CG_INVALID_SOCKET) continue;
        if (::connect(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            used = p;
            break;
        }
        cg_close(fd);
        fd = CG_INVALID_SOCKET;
    }
    if (fd == CG_INVALID_SOCKET || !used) {
        ::freeaddrinfo(res);
        errOut = "could not connect to host";
        return false;
    }
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
    if (!cg_set_nonblock(fd)) {
        cg_close(fd);
        ::freeaddrinfo(res);
        errOut = "set non-blocking failed";
        return false;
    }
    tcpFd_ = fd;
    cg_socket_t ufd = ::socket(used->ai_family, SOCK_DGRAM, 0);
    if (ufd == CG_INVALID_SOCKET) {
        ::freeaddrinfo(res);
        errOut = "udp socket() failed";
        disconnect();
        return false;
    }
    sockaddr_storage peer{};
    int peerLen = (int)used->ai_addrlen;
    std::memcpy(&peer, used->ai_addr, (size_t)peerLen);
    if (::connect(ufd, (sockaddr*)&peer, peerLen) != 0) {
        cg_close(ufd);
        ::freeaddrinfo(res);
        errOut = "udp connect() failed";
        disconnect();
        return false;
    }
    if (!cg_set_nonblock(ufd)) {
        cg_close(ufd);
        ::freeaddrinfo(res);
        errOut = "set non-blocking udp failed";
        disconnect();
        return false;
    }
    udpFd_ = ufd;
    ::freeaddrinfo(res);
    tcpRecvBuf_.clear();
    tcpSendBuf_.clear();
    tcpInbox_.clear();
    udpInbox_.clear();
    return true;
}
void NetClient::disconnect() {
    if (tcpFd_ != CG_INVALID_SOCKET) { cg_close(tcpFd_); tcpFd_ = CG_INVALID_SOCKET; }
    if (udpFd_ != CG_INVALID_SOCKET) { cg_close(udpFd_); udpFd_ = CG_INVALID_SOCKET; }
    tcpRecvBuf_.clear();
    tcpSendBuf_.clear();
    tcpInbox_.clear();
    udpInbox_.clear();
    loggedIn = false;
}
void NetClient::tryFlushTcp() {
    if (tcpFd_ == CG_INVALID_SOCKET || tcpSendBuf_.empty()) return;
    while (!tcpSendBuf_.empty()) {
        cg_ssize_t n = ::send(tcpFd_, tcpSendBuf_.data(), (int)tcpSendBuf_.size(), 0);
        if (n > 0) {
            tcpSendBuf_.erase(0, (size_t)n);
        } else if (n == CG_SOCKET_ERROR) {
            int e = cg_lasterr();
            if (e == CG_EAGAIN || e == CG_EWOULDBLOCK) break;
            disconnect();
            return;
        } else {
            disconnect();
            return;
        }
    }
}
void NetClient::poll() {
    if (tcpFd_ == CG_INVALID_SOCKET) return;
    tryFlushTcp();
    char buf[4096];
    while (true) {
        cg_ssize_t n = ::recv(tcpFd_, buf, sizeof(buf), 0);
        if (n > 0) {
            tcpRecvBuf_.append(buf, (size_t)n);
        } else if (n == 0) {
            disconnect();
            return;
        } else {
            int e = cg_lasterr();
            if (e == CG_EAGAIN || e == CG_EWOULDBLOCK) break;
            if (e == CG_EINTR) continue;
            disconnect();
            return;
        }
    }
    size_t start = 0;
    for (size_t i = 0; i < tcpRecvBuf_.size(); ++i) {
        if (tcpRecvBuf_[i] == '\n') {
            std::string line = tcpRecvBuf_.substr(start, i - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) {
                auto fields = proto::splitDecode(line);
                if (!fields.empty()) {
                    parseHelloIfPresent(fields);
                    parseLoginOkIfPresent(fields);
                    tcpInbox_.push_back(std::move(fields));
                }
            }
            start = i + 1;
        }
    }
    if (start > 0) tcpRecvBuf_.erase(0, start);
    if (udpFd_ != CG_INVALID_SOCKET) {
        while (true) {
            cg_ssize_t n = ::recv(udpFd_, buf, sizeof(buf), 0);
            if (n > 0) {
                std::string dg(buf, (size_t)n);
                while (!dg.empty() && (dg.back() == '\n' || dg.back() == '\r')) dg.pop_back();
                if (!dg.empty()) {
                    auto fields = proto::splitDecode(dg);
                    if (!fields.empty()) udpInbox_.push_back(std::move(fields));
                }
            } else {
                int e = cg_lasterr();
                if (n == 0) break;
                if (e == CG_EAGAIN || e == CG_EWOULDBLOCK) break;
                if (e == CG_EINTR) continue;
                break;
            }
        }
    }
}
void NetClient::sendTcp(const std::vector<std::string>& fields) {
    if (tcpFd_ == CG_INVALID_SOCKET) return;
    std::string line = proto::encodeLine(fields);
    line.push_back('\n');
    tcpSendBuf_.append(line);
    tryFlushTcp();
}
void NetClient::sendUdp(const std::vector<std::string>& fields) {
    if (udpFd_ == CG_INVALID_SOCKET) return;
    std::string dg = proto::encodeLine(fields);
    cg_ssize_t n = ::send(udpFd_, dg.data(), (int)dg.size(), 0);
    (void)n;
}
std::vector<std::vector<std::string>> NetClient::drainTcp() {
    std::vector<std::vector<std::string>> out;
    out.swap(tcpInbox_);
    return out;
}
std::vector<std::vector<std::string>> NetClient::drainUdp() {
    std::vector<std::vector<std::string>> out;
    out.swap(udpInbox_);
    return out;
}
void NetClient::parseHelloIfPresent(const std::vector<std::string>& msg) {
    if (msg.size() < 4) return;
    if (msg[0] != proto::kT_Hello) return;
    serverTickRate = std::atoi(msg[2].c_str());
    if (serverTickRate <= 0) serverTickRate = proto::kTickRate;
    teamSize = std::atoi(msg[3].c_str());
    if (teamSize <= 0) teamSize = proto::kMaxTeamSize;
}
void NetClient::parseLoginOkIfPresent(const std::vector<std::string>& msg) {
    if (msg.size() < 12) return;
    if (msg[0] != proto::kT_LoginOk) return;
    userId         = std::atoi(msg[1].c_str());
    username       = msg[2];
    team           = msg[3];
    xp             = std::atoi(msg[4].c_str());
    level          = std::atoi(msg[5].c_str());
    totalKills     = std::atoi(msg[6].c_str());
    totalDeaths    = std::atoi(msg[7].c_str());
    totalHeadshots = std::atoi(msg[8].c_str());
    matchesPlayed  = std::atoi(msg[9].c_str());
    matchesWon     = std::atoi(msg[10].c_str());
    udpToken       = msg[11];
    credits        = (msg.size() >= 13) ? std::atoi(msg[12].c_str()) : 0;
    selectedWeapon = (msg.size() >= 14) ? std::atoi(msg[13].c_str()) : proto::kWeaponPistol;
    if (selectedWeapon < 1 || selectedWeapon > proto::kWeaponCount) selectedWeapon = proto::kWeaponPistol;
    loggedIn       = true;
}
