#pragma once
/*
 * netinet/tcp.h shim.
 * Provides TCP_NODELAY and IPPROTO_TCP for all platforms.
 * On Windows, these come from winsock2.h (already included via sockLib.h).
 * On Linux, pass through to the real system header.
 */
#ifdef _WIN32
#  ifndef TCP_NODELAY
#    define TCP_NODELAY 1
#  endif
#  ifndef IPPROTO_TCP
#    define IPPROTO_TCP 6
#  endif
#else
#  ifdef __has_include_next
#    include_next <netinet/tcp.h>
#  endif
#endif
