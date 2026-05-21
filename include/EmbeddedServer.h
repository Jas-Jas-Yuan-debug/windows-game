#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
class Server;
class EmbeddedServer {
public:
    EmbeddedServer();
    ~EmbeddedServer();
    EmbeddedServer(const EmbeddedServer&) = delete;
    EmbeddedServer& operator=(const EmbeddedServer&) = delete;
    bool start(std::uint16_t port, int teamSize, const std::string& dbPath, std::string& err);
    void stop();
    bool isRunning() const { return running_.load(); }
    bool isReady() const;
    std::uint16_t port() const { return port_; }
private:
    std::unique_ptr<Server> server_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::uint16_t port_ = 0;
};
