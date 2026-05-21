#pragma once
#include <cstddef>
#include <cstdint>
namespace cg {
constexpr std::size_t kSha256Bytes = 32;
bool sha256SaltPassword(const unsigned char* salt, std::size_t saltLen,
                        const unsigned char* password, std::size_t passwordLen,
                        unsigned char out[kSha256Bytes]);
bool randomBytes(unsigned char* out, std::size_t len);
}
