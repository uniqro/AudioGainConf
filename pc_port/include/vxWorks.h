#pragma once
/* VxWorks compatibility shim — provides base types and constants for PC build */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

typedef int      STATUS;
typedef int      BOOL;
typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;

#define OK    0
#define ERROR (-1)
#ifndef TRUE
#  define TRUE  1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

typedef void (*FUNCPTR)(void);

/* MSVC doesn't support __attribute__((packed)).
 * Defining it as a no-op lets the original headers parse correctly.
 * Actual 1-byte packing is enforced project-wide via /Zp1 in CMakeLists.txt,
 * which is safe because Windows SDK headers use explicit #pragma pack(push/pop)
 * to protect their own structs from the global packing flag. */
#ifdef _MSC_VER
#  define __attribute__(x)
#endif

/* Pull in logMsg so any translation unit that doesn't explicitly include
 * logLib.h still gets the definition (GCC 15 treats implicit declarations
 * as hard errors). */
#include "logLib.h"
