#pragma once
/* VxWorks compatibility shim -- provides base types and constants for PC build */

#ifdef _MSC_VER
/*
 * On MSVC with /Zp1 (1-byte global struct packing, required so that
 * AudioConfigFileData is exactly 300 bytes), Windows SDK headers must be
 * included under natural 8-byte alignment, otherwise C_ASSERT checks inside
 * winnt.h (lines 2602, 2757, 2810) and ws2ipdef.h fail.
 *
 * vxWorks.h is force-included first (/FI), so processing all Windows SDK
 * headers here -- inside a pack(push,8)/pop pair -- means that by the time
 * any TU (e.g. tasklib_stubs.c) pulls in <windows.h> itself, include guards
 * prevent re-processing and every Windows struct retains its correct size.
 */
#  pragma pack(push, 8)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601
#  endif
#  include <winsock2.h>   /* must precede windows.h to prevent old winsock.h */
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <process.h>    /* _beginthreadex for tasklib_stubs.c */
#  pragma pack(pop)
/* wingdi.h (pulled in via windows.h) redefines ERROR to 0.
 * Undo that before any AudioConf source sees the macro. */
#  ifdef ERROR
#    undef ERROR
#  endif
/* __attribute__((packed)) is a GCC extension; map to a no-op on MSVC.
 * /Zp1 enforces equivalent 1-byte packing project-wide. */
#  define __attribute__(x)
#endif /* _MSC_VER */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* STATUS is a VxWorks type not defined by the Windows SDK. */
typedef int STATUS;

/* BOOL, UINT8/16/32 are defined by windef.h/winnt.h on MSVC; guard to avoid
 * redefinition. On GCC/non-Windows these come only from here. */
#ifndef BOOL
typedef int BOOL;
#endif
#ifndef UINT8
typedef uint8_t  UINT8;
#endif
#ifndef UINT16
typedef uint16_t UINT16;
#endif
#ifndef UINT32
typedef uint32_t UINT32;
#endif

#define OK    0
#define ERROR (-1)
#ifndef TRUE
#  define TRUE  1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

typedef void (*FUNCPTR)(void);

/* Pull in logMsg so any translation unit that does not explicitly include
 * logLib.h still gets the definition (GCC 15 treats implicit declarations
 * as hard errors). */
#include "logLib.h"
