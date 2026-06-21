#pragma once
/*
 * VxWorks sockLib shim.
 *
 * On Windows (MinGW): wraps Winsock2 so that the VxWorks code which uses
 *   int socket fd  and  ERROR/NULL checks works unchanged.
 * On Linux: standard POSIX headers — almost nothing to do.
 *
 * Key mappings provided here:
 *   fioRead(fd,buf,n)  →  recv(fd,buf,n,0)      (blocking read)
 *   bzero(p,n)         →  memset(p,0,n)
 *   close(s)           →  closesocket(s)         (Windows only)
 *   socket/bind/listen/accept/send/recv/setsockopt — int-safe wrappers on Windows
 */

#include <string.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601   /* Windows 7+ */
#endif

/* When the project is built with /Zp1 (to honour __attribute__((packed)) on
 * AudioConf structs), Windows SDK headers would otherwise be parsed under
 * 1-byte packing, causing sizeof() mismatches in SDK-internal C_ASSERT checks
 * (e.g. ws2ipdef.h:908 SOCKET_ADDRESS = 12 instead of 16).
 * Push 8-byte packing (MSVC x64 default) before the SDK includes so that all
 * Winsock2/WS2 struct definitions retain their natural layout; pop afterwards
 * so that /Zp1 is in effect again for the AudioConf struct definitions. */
#ifdef _MSC_VER
#  pragma pack(push, 8)
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#  pragma pack(pop)
#endif

/* ── Windows socket wrappers (SOCKET → int, SOCKET_ERROR → -1/ERROR) ──────
 * These are defined BEFORE the #define macros so the winsock2 function name
 * resolves to the real API, not our macro. */

static inline int _pcw_socket(int af, int type, int proto) {
    SOCKET s = socket(af, type, proto);
    return (s == INVALID_SOCKET) ? (-1) : (int)(intptr_t)s;
}
static inline int _pcw_accept(int fd, struct sockaddr *addr, int *len) {
    SOCKET s = accept((SOCKET)(intptr_t)fd, addr, len);
    return (s == INVALID_SOCKET) ? (-1) : (int)(intptr_t)s;
}
static inline int _pcw_bind(int fd, const struct sockaddr *addr, int len) {
    return (bind((SOCKET)(intptr_t)fd, addr, len) == SOCKET_ERROR) ? (-1) : 0;
}
static inline int _pcw_listen(int fd, int backlog) {
    return (listen((SOCKET)(intptr_t)fd, backlog) == SOCKET_ERROR) ? (-1) : 0;
}
static inline int _pcw_close(int fd) {
    return closesocket((SOCKET)(intptr_t)fd);
}
static inline int _pcw_send(int fd, const char *buf, int len, int flags) {
    int r = send((SOCKET)(intptr_t)fd, buf, len, flags);
    return (r == SOCKET_ERROR) ? (-1) : r;
}
static inline int _pcw_recv(int fd, char *buf, int len, int flags) {
    int r = recv((SOCKET)(intptr_t)fd, buf, len, flags);
    return (r == SOCKET_ERROR) ? (-1) : r;
}
static inline int _pcw_setsockopt(int fd, int level, int optname,
                                  const char *optval, int optlen) {
    return (setsockopt((SOCKET)(intptr_t)fd, level, optname,
                       optval, optlen) == SOCKET_ERROR) ? (-1) : 0;
}
/* bind with SO_REUSEADDR automatically set before binding */
static inline int _pcw_bind_reuse(int fd, const struct sockaddr *addr, int len) {
    int yes = 1;
    setsockopt((SOCKET)(intptr_t)fd, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&yes, sizeof(yes));
    return _pcw_bind(fd, addr, len);
}

/* ── Now define the remapping macros ────────────────────────────────────── */
#define socket(af,type,proto)              _pcw_socket(af,type,proto)
#define accept(fd,addr,len)                _pcw_accept(fd,addr,(int*)(len))
#define bind(fd,addr,len)                  _pcw_bind_reuse(fd,addr,(int)(len))
#define listen(fd,bl)                      _pcw_listen(fd,bl)
#define close(s)                           _pcw_close(s)
#define send(fd,buf,len,fl)                _pcw_send(fd,(const char*)(buf),(int)(len),fl)
#define recv(fd,buf,len,fl)                _pcw_recv(fd,(char*)(buf),(int)(len),fl)
#define setsockopt(fd,lv,opt,val,len)      _pcw_setsockopt(fd,lv,opt,(const char*)(val),(int)(len))
#define bzero(p,n)                         memset(p,0,n)

/* socklen_t is in ws2tcpip.h */

/* wingdi.h (included transitively via windows.h) redefines ERROR as 0.
 * Restore the VxWorks convention: ERROR = -1. */
#ifdef ERROR
#  undef ERROR
#endif
#define ERROR (-1)

#else   /* ── Linux / macOS ──────────────────────────────────────────────── */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* SO_REUSEADDR wrapper for bind — prevents "address already in use" */
static inline int _pcl_bind(int fd, const struct sockaddr *addr, socklen_t len) {
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    return bind(fd, addr, len);
}
#undef bind
#define bind(fd,addr,len) _pcl_bind(fd,addr,(socklen_t)(len))

#endif  /* _WIN32 */

/* ── Platform-agnostic shims ─────────────────────────────────────────────── */

/* fioRead: VxWorks blocking read on socket — same as recv with no flags */
#define fioRead(fd, buf, n)  recv((fd), (buf), (n), 0)
