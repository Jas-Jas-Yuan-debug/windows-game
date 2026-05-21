#include "Crypto.h"
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
  #include <CommonCrypto/CommonDigest.h>
  #include <cstdlib>
#else
  #include <openssl/evp.h>
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
    arc4random_buf(out, len);
    return true;
#else
    return RAND_bytes(out, (int)len) == 1;
#endif
}
}
