#pragma once
#include "NetClient.h"
class Store {
public:
    explicit Store(NetClient& net);
    void run();
private:
    NetClient& net_;
};
