#pragma once
#include "NetClient.h"
#include <string>
class EmbeddedServer;
class ConnectScreen {
public:
    ConnectScreen(NetClient& net, EmbeddedServer* embedded);
    bool run();
private:
    NetClient& net_;
    EmbeddedServer* embedded_;
    std::string host_ = "127.0.0.1";
    std::string port_ = "27015";
};
