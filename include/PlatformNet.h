#pragma once
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  typedef SOCKET cg_socket_t;
  typedef int cg_ssize_t;
  #define CG_INVALID_SOCKET INVALID_SOCKET
  #define CG_SOCKET_ERROR   SOCKET_ERROR
  inline int cg_close(cg_socket_t s) { return ::closesocket(s); }
  inline int cg_lasterr() { return ::WSAGetLastError(); }
  #define CG_EAGAIN      WSAEWOULDBLOCK
  #define CG_EWOULDBLOCK WSAEWOULDBLOCK
  #define CG_EINTR       WSAEINTR
  inline bool cg_set_nonblock(cg_socket_t s) {
      u_long mode = 1;
      return ::ioctlsocket(s, FIONBIO, &mode) == 0;
  }
  inline void cg_netinit() {
      static bool done = false;
      if (!done) { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); done = true; }
  }
  inline void cg_netshutdown() { ::WSACleanup(); }
#else
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <cerrno>
  typedef int     cg_socket_t;
  typedef ssize_t cg_ssize_t;
  #define CG_INVALID_SOCKET (-1)
  #define CG_SOCKET_ERROR   (-1)
  inline int cg_close(cg_socket_t s) { return ::close(s); }
  inline int cg_lasterr() { return errno; }
  #define CG_EAGAIN      EAGAIN
  #define CG_EWOULDBLOCK EWOULDBLOCK
  #define CG_EINTR       EINTR
  inline bool cg_set_nonblock(cg_socket_t s) {
      int f = ::fcntl(s, F_GETFL, 0);
      if (f < 0) return false;
      return ::fcntl(s, F_SETFL, f | O_NONBLOCK) == 0;
  }
  inline void cg_netinit() {}
  inline void cg_netshutdown() {}
#endif
