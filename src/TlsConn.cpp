#include "TlsConn.h"
#include "Crypto.h"
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_csr.h>
#include <psa/crypto.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <errno.h>
  #include <unistd.h>
#endif
namespace {
bool g_psa_inited = false;
void ensurePsa() {
    if (g_psa_inited) return;
    psa_crypto_init();
    g_psa_inited = true;
}
int errStr(int code, char* buf, size_t len) {
    mbedtls_strerror(code, buf, len);
    return code;
}
int netSend(void* ctx, const unsigned char* buf, size_t len) {
    int fd = (int)(intptr_t)ctx;
#ifdef _WIN32
    int n = ::send((SOCKET)fd, (const char*)buf, (int)len, 0);
    if (n == SOCKET_ERROR) {
        int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return n;
#else
    ssize_t n = ::send(fd, buf, len, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_WRITE;
        if (errno == EINTR) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return (int)n;
#endif
}
int netRecv(void* ctx, unsigned char* buf, size_t len) {
    int fd = (int)(intptr_t)ctx;
#ifdef _WIN32
    int n = ::recv((SOCKET)fd, (char*)buf, (int)len, 0);
    if (n == SOCKET_ERROR) {
        int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    if (n == 0) return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    return n;
#else
    ssize_t n = ::recv(fd, buf, len, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_READ;
        if (errno == EINTR) return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    if (n == 0) return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    return (int)n;
#endif
}
int certVerifyAcceptAll(void* /*data*/, mbedtls_x509_crt* /*crt*/, int /*depth*/, uint32_t* flags) {
    *flags = 0;
    return 0;
}
}
struct TlsContext::Impl {
    bool isServer = false;
    mbedtls_ssl_config conf{};
    mbedtls_entropy_context entropy{};
    mbedtls_ctr_drbg_context drbg{};
    mbedtls_x509_crt cert{};
    mbedtls_pk_context key{};
    bool confInited = false;
    bool entropyInited = false;
    bool drbgInited = false;
    bool certInited = false;
    bool keyInited = false;
    Impl() {}
    ~Impl() {
        if (confInited)    mbedtls_ssl_config_free(&conf);
        if (certInited)    mbedtls_x509_crt_free(&cert);
        if (keyInited)     mbedtls_pk_free(&key);
        if (drbgInited)    mbedtls_ctr_drbg_free(&drbg);
        if (entropyInited) mbedtls_entropy_free(&entropy);
    }
};
TlsContext::TlsContext() : impl_(std::make_unique<Impl>()) {}
TlsContext::~TlsContext() = default;
bool TlsContext::initServer(const std::string& certPem, const std::string& keyPem, std::string& err) {
    ensurePsa();
    auto& I = *impl_;
    I.isServer = true;
    mbedtls_entropy_init(&I.entropy); I.entropyInited = true;
    mbedtls_ctr_drbg_init(&I.drbg);   I.drbgInited = true;
    mbedtls_x509_crt_init(&I.cert);   I.certInited = true;
    mbedtls_pk_init(&I.key);          I.keyInited = true;
    mbedtls_ssl_config_init(&I.conf); I.confInited = true;
    const char* pers = "claudegame-server";
    char ebuf[160];
    int rc = mbedtls_ctr_drbg_seed(&I.drbg, mbedtls_entropy_func, &I.entropy,
                                   (const unsigned char*)pers, std::strlen(pers));
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("ctr_drbg_seed: ") + ebuf; return false; }
    rc = mbedtls_x509_crt_parse(&I.cert,
                                (const unsigned char*)certPem.data(), certPem.size() + 1);
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("x509_parse_cert: ") + ebuf; return false; }
    rc = mbedtls_pk_parse_key(&I.key,
                              (const unsigned char*)keyPem.data(), keyPem.size() + 1,
                              nullptr, 0, mbedtls_ctr_drbg_random, &I.drbg);
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("pk_parse_key: ") + ebuf; return false; }
    rc = mbedtls_ssl_config_defaults(&I.conf, MBEDTLS_SSL_IS_SERVER,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("ssl_config_defaults: ") + ebuf; return false; }
    mbedtls_ssl_conf_rng(&I.conf, mbedtls_ctr_drbg_random, &I.drbg);
    mbedtls_ssl_conf_authmode(&I.conf, MBEDTLS_SSL_VERIFY_NONE);
    rc = mbedtls_ssl_conf_own_cert(&I.conf, &I.cert, &I.key);
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("ssl_conf_own_cert: ") + ebuf; return false; }
    return true;
}
bool TlsContext::initClient(std::string& err) {
    ensurePsa();
    auto& I = *impl_;
    I.isServer = false;
    mbedtls_entropy_init(&I.entropy); I.entropyInited = true;
    mbedtls_ctr_drbg_init(&I.drbg);   I.drbgInited = true;
    mbedtls_ssl_config_init(&I.conf); I.confInited = true;
    const char* pers = "claudegame-client";
    char ebuf[160];
    int rc = mbedtls_ctr_drbg_seed(&I.drbg, mbedtls_entropy_func, &I.entropy,
                                   (const unsigned char*)pers, std::strlen(pers));
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("ctr_drbg_seed: ") + ebuf; return false; }
    rc = mbedtls_ssl_config_defaults(&I.conf, MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("ssl_config_defaults: ") + ebuf; return false; }
    mbedtls_ssl_conf_rng(&I.conf, mbedtls_ctr_drbg_random, &I.drbg);
    mbedtls_ssl_conf_authmode(&I.conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_verify(&I.conf, certVerifyAcceptAll, nullptr);
    return true;
}
struct TlsImpl {
    mbedtls_ssl_context ssl{};
    bool sslInited = false;
    bool handshakeDone = false;
    int  fd = -1;
    std::string fingerprint;
    TlsImpl() {}
    ~TlsImpl() { if (sslInited) mbedtls_ssl_free(&ssl); }
};
TlsConn::TlsConn() : p_(std::make_unique<TlsImpl>()) {}
TlsConn::~TlsConn() = default;
TlsConn::TlsConn(TlsConn&& o) noexcept : p_(std::move(o.p_)) {}
TlsConn& TlsConn::operator=(TlsConn&& o) noexcept { p_ = std::move(o.p_); return *this; }
bool TlsConn::attach(TlsContext& ctx, bool /*isServer*/, std::uintptr_t fd, std::string& err) {
    auto* impl = ctx.impl();
    if (!impl) { err = "TlsContext not initialised"; return false; }
    mbedtls_ssl_init(&p_->ssl);
    p_->sslInited = true;
    p_->fd = (int)fd;
    char ebuf[160];
    int rc = mbedtls_ssl_setup(&p_->ssl, &impl->conf);
    if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("ssl_setup: ") + ebuf; return false; }
    mbedtls_ssl_set_bio(&p_->ssl, (void*)(intptr_t)p_->fd, netSend, netRecv, nullptr);
    return true;
}
static TlsConn::Status mapRc(int rc) {
    if (rc == 0) return TlsConn::Status::Ok;
    if (rc == MBEDTLS_ERR_SSL_WANT_READ) return TlsConn::Status::WantRead;
    if (rc == MBEDTLS_ERR_SSL_WANT_WRITE) return TlsConn::Status::WantWrite;
    if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return TlsConn::Status::Closed;
    return TlsConn::Status::Error;
}
TlsConn::Status TlsConn::handshake() {
    if (!p_->sslInited) return Status::Error;
    int rc = mbedtls_ssl_handshake(&p_->ssl);
    if (rc == 0) {
        p_->handshakeDone = true;
        const mbedtls_x509_crt* peer = mbedtls_ssl_get_peer_cert(&p_->ssl);
        if (peer) {
            unsigned char digest[32];
            if (mbedtls_sha256(peer->raw.p, peer->raw.len, digest, 0) == 0) {
                p_->fingerprint = cg::bytesToHex(digest, 32);
            }
        }
        return Status::Ok;
    }
    return mapRc(rc);
}
TlsConn::Status TlsConn::read(void* buf, std::size_t cap, std::size_t& outBytes) {
    outBytes = 0;
    if (!p_->sslInited) return Status::Error;
    int rc = mbedtls_ssl_read(&p_->ssl, (unsigned char*)buf, cap);
    if (rc > 0) { outBytes = (std::size_t)rc; return Status::Ok; }
    return mapRc(rc);
}
TlsConn::Status TlsConn::write(const void* buf, std::size_t len, std::size_t& outBytes) {
    outBytes = 0;
    if (!p_->sslInited) return Status::Error;
    int rc = mbedtls_ssl_write(&p_->ssl, (const unsigned char*)buf, len);
    if (rc > 0) { outBytes = (std::size_t)rc; return Status::Ok; }
    return mapRc(rc);
}
void TlsConn::close() {
    if (p_->sslInited && p_->handshakeDone) {
        mbedtls_ssl_close_notify(&p_->ssl);
    }
}
bool TlsConn::isHandshakeDone() const { return p_->handshakeDone; }
std::string TlsConn::peerCertFingerprintHex() const { return p_->fingerprint; }
namespace tls {
bool generateSelfSignedCert(const std::string& commonName,
                            std::string& certPemOut, std::string& keyPemOut,
                            std::string& err) {
    ensurePsa();
    char ebuf[160];
    mbedtls_pk_context key;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_pk_init(&key);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    bool ok = false;
    do {
        const char* pers = "claudegame-cert-gen";
        int rc = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                       (const unsigned char*)pers, std::strlen(pers));
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("drbg_seed: ") + ebuf; break; }
        rc = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("pk_setup: ") + ebuf; break; }
        rc = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &drbg, 2048, 65537);
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("rsa_gen_key: ") + ebuf; break; }
        std::vector<unsigned char> keyBuf(8192, 0);
        rc = mbedtls_pk_write_key_pem(&key, keyBuf.data(), keyBuf.size());
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("pk_write_pem: ") + ebuf; break; }
        keyPemOut.assign((const char*)keyBuf.data());
        mbedtls_x509write_cert crt;
        mbedtls_x509write_crt_init(&crt);
        mbedtls_x509write_crt_set_subject_key(&crt, &key);
        mbedtls_x509write_crt_set_issuer_key(&crt, &key);
        std::string subj = "CN=" + commonName;
        rc = mbedtls_x509write_crt_set_subject_name(&crt, subj.c_str());
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("set_subject: ") + ebuf; mbedtls_x509write_crt_free(&crt); break; }
        rc = mbedtls_x509write_crt_set_issuer_name(&crt, subj.c_str());
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("set_issuer: ") + ebuf; mbedtls_x509write_crt_free(&crt); break; }
        mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
        mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
        mbedtls_mpi serial; mbedtls_mpi_init(&serial);
        unsigned char serialBytes[16];
        cg::randomBytes(serialBytes, sizeof(serialBytes));
        serialBytes[0] &= 0x7F;
        mbedtls_mpi_read_binary(&serial, serialBytes, sizeof(serialBytes));
        mbedtls_x509write_crt_set_serial(&crt, &serial);
        mbedtls_mpi_free(&serial);
        std::time_t t = std::time(nullptr);
        std::tm* now = std::gmtime(&t);
        char nb[32], na[32];
        std::strftime(nb, sizeof(nb), "%Y%m%d000000", now);
        std::tm na_tm = *now; na_tm.tm_year += 10;
        std::strftime(na, sizeof(na), "%Y%m%d000000", &na_tm);
        rc = mbedtls_x509write_crt_set_validity(&crt, nb, na);
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("set_validity: ") + ebuf; mbedtls_x509write_crt_free(&crt); break; }
        mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
        mbedtls_x509write_crt_set_subject_key_identifier(&crt);
        mbedtls_x509write_crt_set_authority_key_identifier(&crt);
        std::vector<unsigned char> crtBuf(8192, 0);
        rc = mbedtls_x509write_crt_pem(&crt, crtBuf.data(), crtBuf.size(),
                                       mbedtls_ctr_drbg_random, &drbg);
        mbedtls_x509write_crt_free(&crt);
        if (rc != 0) { errStr(rc, ebuf, sizeof(ebuf)); err = std::string("write_crt_pem: ") + ebuf; break; }
        certPemOut.assign((const char*)crtBuf.data());
        ok = true;
    } while (0);
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    return ok;
}
std::string certFingerprintHex(const std::string& certPem) {
    mbedtls_x509_crt c; mbedtls_x509_crt_init(&c);
    int rc = mbedtls_x509_crt_parse(&c, (const unsigned char*)certPem.data(), certPem.size() + 1);
    std::string out;
    if (rc == 0) {
        unsigned char d[32];
        if (mbedtls_sha256(c.raw.p, c.raw.len, d, 0) == 0) out = cg::bytesToHex(d, 32);
    }
    mbedtls_x509_crt_free(&c);
    return out;
}
}
