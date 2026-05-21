#include "Protocol.h"
#include <cctype>
#include <cstdio>
#include <sstream>
namespace proto {
namespace {
inline bool isUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}
inline int fromHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}
}
std::string urlEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    char buf[4];
    for (unsigned char c : s) {
        if (isUnreserved(c)) {
            out.push_back(static_cast<char>(c));
        } else {
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out.append(buf);
        }
    }
    return out;
}
std::string urlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '%' && i + 2 < s.size()) {
            int hi = fromHex(s[i + 1]);
            int lo = fromHex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}
std::vector<std::string> splitDecode(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '|') {
            out.push_back(urlDecode(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(urlDecode(cur));
    return out;
}
std::string encodeLine(const std::vector<std::string>& fields) {
    std::string out;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i) out.push_back('|');
        out.append(urlEncode(fields[i]));
    }
    return out;
}
}
