#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
struct TlsImpl;
class TlsContext;
class TlsConn {
public:
    enum class Status { Ok, WantRead, WantWrite, Closed, Error };
    TlsConn();
    ~TlsConn();
    TlsConn(const TlsConn&) = delete;
    TlsConn& operator=(const TlsConn&) = delete;
    TlsConn(TlsConn&&) noexcept;
    TlsConn& operator=(TlsConn&&) noexcept;
    bool attach(TlsContext& ctx, bool isServer, std::uintptr_t fd, std::string& err);
    Status handshake();
    Status read(void* buf, std::size_t cap, std::size_t& outBytes);
    Status write(const void* buf, std::size_t len, std::size_t& outBytes);
    void close();
    bool isHandshakeDone() const;
    std::string peerCertFingerprintHex() const;
private:
    std::unique_ptr<TlsImpl> p_;
};
class TlsContext {
public:
    TlsContext();
    ~TlsContext();
    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;
    bool initServer(const std::string& certPem, const std::string& keyPem, std::string& err);
    bool initClient(std::string& err);
    struct Impl;
    Impl* impl() { return impl_.get(); }
private:
    std::unique_ptr<Impl> impl_;
};
namespace tls {
bool generateSelfSignedCert(const std::string& commonName,
                            std::string& certPemOut, std::string& keyPemOut,
                            std::string& err);
std::string certFingerprintHex(const std::string& certPem);
}
