#include "Crypto.h"
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <bcrypt.h>
  #ifndef NT_SUCCESS
    #define NT_SUCCESS(x) ((x) >= 0)
  #endif
#elif defined(__APPLE__)
  #include <CommonCrypto/CommonCryptor.h>
  #include <CommonCrypto/CommonDigest.h>
  #include <CommonCrypto/CommonHMAC.h>
  #include <CommonCrypto/CommonKeyDerivation.h>
  #include <Security/SecRandom.h>
#else
  #include <openssl/evp.h>
  #include <openssl/hmac.h>
  #include <openssl/rand.h>
#endif
namespace cg {
bool sha256SaltPassword(const unsigned char* salt, std::size_t saltLen,
                        const unsigned char* password, std::size_t passwordLen,
                        unsigned char out[kSha256Bytes]) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) return false;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = false;
    if (NT_SUCCESS(BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0))) {
        if (salt && saltLen) BCryptHashData(h, (PUCHAR)salt, (ULONG)saltLen, 0);
        if (password && passwordLen) BCryptHashData(h, (PUCHAR)password, (ULONG)passwordLen, 0);
        if (NT_SUCCESS(BCryptFinishHash(h, out, (ULONG)kSha256Bytes, 0))) ok = true;
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
#elif defined(__APPLE__)
    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);
    if (salt && saltLen)         CC_SHA256_Update(&ctx, salt, (CC_LONG)saltLen);
    if (password && passwordLen) CC_SHA256_Update(&ctx, password, (CC_LONG)passwordLen);
    CC_SHA256_Final(out, &ctx);
    return true;
#else
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    bool ok = false;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1) {
        bool good = true;
        if (salt && saltLen)         good = good && EVP_DigestUpdate(ctx, salt, saltLen) == 1;
        if (password && passwordLen) good = good && EVP_DigestUpdate(ctx, password, passwordLen) == 1;
        unsigned int dlen = 0;
        if (good && EVP_DigestFinal_ex(ctx, out, &dlen) == 1 && dlen == kSha256Bytes) ok = true;
    }
    EVP_MD_CTX_free(ctx);
    return ok;
#endif
}
bool randomBytes(unsigned char* out, std::size_t len) {
#ifdef _WIN32
    return NT_SUCCESS(BCryptGenRandom(nullptr, out, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
#elif defined(__APPLE__)
    return SecRandomCopyBytes(kSecRandomDefault, len, out) == errSecSuccess;
#else
    return RAND_bytes(out, (int)len) == 1;
#endif
}
bool pbkdf2Sha256(const unsigned char* password, std::size_t passwordLen,
                  const unsigned char* salt,     std::size_t saltLen,
                  std::uint32_t iterations,
                  unsigned char* out, std::size_t outLen) {
    if (!password || !salt || !out || iterations == 0 || outLen == 0) return false;
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) return false;
    NTSTATUS st = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)password, (ULONG)passwordLen,
                                        (PUCHAR)salt, (ULONG)saltLen,
                                        (ULONGLONG)iterations,
                                        out, (ULONG)outLen, 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    return NT_SUCCESS(st);
#elif defined(__APPLE__)
    int rc = CCKeyDerivationPBKDF(kCCPBKDF2,
                                  (const char*)password, passwordLen,
                                  salt, saltLen,
                                  kCCPRFHmacAlgSHA256,
                                  iterations,
                                  out, outLen);
    return rc == kCCSuccess;
#else
    return PKCS5_PBKDF2_HMAC((const char*)password, (int)passwordLen,
                             salt, (int)saltLen,
                             (int)iterations,
                             EVP_sha256(),
                             (int)outLen, out) == 1;
#endif
}
bool hmacSha256(const unsigned char* key,  std::size_t keyLen,
                const unsigned char* data, std::size_t dataLen,
                unsigned char out[kSha256Bytes]) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) return false;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = false;
    if (NT_SUCCESS(BCryptCreateHash(alg, &h, nullptr, 0, (PUCHAR)key, (ULONG)keyLen, 0))) {
        if (NT_SUCCESS(BCryptHashData(h, (PUCHAR)data, (ULONG)dataLen, 0)) &&
            NT_SUCCESS(BCryptFinishHash(h, out, (ULONG)kSha256Bytes, 0))) ok = true;
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
#elif defined(__APPLE__)
    CCHmac(kCCHmacAlgSHA256, key, keyLen, data, dataLen, out);
    return true;
#else
    unsigned int outLen = 0;
    return HMAC(EVP_sha256(), key, (int)keyLen, data, dataLen, out, &outLen) != nullptr && outLen == kSha256Bytes;
#endif
}
std::string bytesToHex(const unsigned char* bytes, std::size_t len) {
    static const char k[] = "0123456789abcdef";
    std::string out(len * 2, '0');
    for (std::size_t i = 0; i < len; ++i) {
        out[2 * i]     = k[(bytes[i] >> 4) & 0xF];
        out[2 * i + 1] = k[bytes[i] & 0xF];
    }
    return out;
}
std::string pbkdf2Sha256Hex(const std::string& password, const std::string& salt,
                            std::uint32_t iterations, std::size_t outLen) {
    std::vector<unsigned char> buf(outLen);
    if (!pbkdf2Sha256(reinterpret_cast<const unsigned char*>(password.data()), password.size(),
                      reinterpret_cast<const unsigned char*>(salt.data()), salt.size(),
                      iterations, buf.data(), outLen)) return std::string();
    return bytesToHex(buf.data(), outLen);
}
std::string hmacSha256Hex(const std::string& key, const std::string& data) {
    unsigned char out[kSha256Bytes];
    if (!hmacSha256(reinterpret_cast<const unsigned char*>(key.data()), key.size(),
                    reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                    out)) return std::string();
    return bytesToHex(out, kSha256Bytes);
}
}
