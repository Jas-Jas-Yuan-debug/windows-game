#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace cg {
constexpr std::size_t kSha256Bytes = 32;
bool sha256SaltPassword(const unsigned char* salt, std::size_t saltLen,
                        const unsigned char* password, std::size_t passwordLen,
                        unsigned char out[kSha256Bytes]);
bool pbkdf2Sha256(const unsigned char* password, std::size_t passwordLen,
                  const unsigned char* salt,     std::size_t saltLen,
                  std::uint32_t iterations,
                  unsigned char* out, std::size_t outLen);
bool hmacSha256(const unsigned char* key,  std::size_t keyLen,
                const unsigned char* data, std::size_t dataLen,
                unsigned char out[kSha256Bytes]);
bool randomBytes(unsigned char* out, std::size_t len);
std::string bytesToHex(const unsigned char* bytes, std::size_t len);
std::string pbkdf2Sha256Hex(const std::string& password, const std::string& salt,
                            std::uint32_t iterations, std::size_t outLen = 32);
std::string hmacSha256Hex(const std::string& key, const std::string& data);
}
