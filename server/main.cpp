#include "Server.h"
#include "Protocol.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
namespace {
volatile std::sig_atomic_t g_stop = 0;
Server* g_server = nullptr;
void onSignal(int) { g_stop = 1; if (g_server) g_server->stop(); }
void printUsage() {
    std::cerr <<
        "Usage: claudegame_server [--port N] [--team-size N] [--db PATH] [--host H]\n";
}
}
int main(int argc, char** argv) {
    ServerConfig cfg;
    cfg.port = proto::kDefaultPort;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port") cfg.port = std::atoi(next("--port"));
        else if (a == "--team-size") {
            cfg.teamSize = std::atoi(next("--team-size"));
            if (cfg.teamSize < 1) cfg.teamSize = 1;
            if (cfg.teamSize > proto::kMaxTeamSize) cfg.teamSize = proto::kMaxTeamSize;
        }
        else if (a == "--db") cfg.dbPath = next("--db");
        else if (a == "--host") cfg.host = next("--host");
        else if (a == "--bot-fill-secs") cfg.botFillSecs = std::atoi(next("--bot-fill-secs"));
        else if (a == "--help" || a == "-h") { printUsage(); return 0; }
        else { std::cerr << "unknown arg: " << a << "\n"; printUsage(); return 2; }
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        Server srv(cfg);
        g_server = &srv;
        int rc = srv.run();
        g_server = nullptr;
        return rc;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
