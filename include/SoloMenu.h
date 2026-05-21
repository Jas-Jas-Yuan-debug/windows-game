#pragma once
#include "NetClient.h"
class SoloMenu {
public:
    enum class Result { Back, Practice, VsBots };
    explicit SoloMenu(NetClient& net);
    Result run();
private:
    NetClient& net_;
};
