#pragma once
/*
 * sys/socket.h shim.
 * AudioConfigTask.h includes <sys/socket.h> with angle brackets.
 * sockLib.h (included before this) already pulled in the platform socket API,
 * so this file only needs to pass through on Linux.
 */
#ifndef _WIN32
#  ifdef __has_include_next
#    include_next <sys/socket.h>
#  endif
#endif
