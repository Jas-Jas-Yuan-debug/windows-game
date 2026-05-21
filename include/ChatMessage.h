#pragma once
#include <string>
struct ChatMessage {
    int id = 0;
    std::string room;
    int userId = 0;
    std::string username;
    std::string content;
    long long sentAt = 0;
};
