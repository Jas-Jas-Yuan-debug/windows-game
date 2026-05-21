#include "EmbeddedServer.h"
#include "Server.h"
#include <iostream>
EmbeddedServer::EmbeddedServer() = default;
EmbeddedServer::~EmbeddedServer() {
    stop();
}
bool EmbeddedServer::start(std::uint16_t port, int teamSize, const std::string& dbPath, std::string& err) {
    if (running_.load()) {
        err = "embedded server already running";
        return false;
    }
    ServerConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = port;
    cfg.teamSize = teamSize;
    cfg.dbPath = dbPath;
    try {
        server_ = std::make_unique<Server>(cfg);
    } catch (const std::exception& e) {
        err = std::string("server construct: ") + e.what();
        return false;
    }
    running_.store(true);
    port_ = port;
    Server* raw = server_.get();
    thread_ = std::thread([this, raw]() {
        try {
            raw->run();
        } catch (const std::exception& e) {
            std::cerr << "[embedded] server thread fatal: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[embedded] server thread fatal: unknown" << std::endl;
        }
        running_.store(false);
    });
    return true;
}
void EmbeddedServer::stop() {
    if (server_) server_->stop();
    if (thread_.joinable()) thread_.join();
    server_.reset();
    running_.store(false);
}
