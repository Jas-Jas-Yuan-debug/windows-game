#pragma once
#include "NetClient.h"
class ChatRoom {
public:
    explicit ChatRoom(NetClient& net);
    void run();
private:
    NetClient& net_;
};
