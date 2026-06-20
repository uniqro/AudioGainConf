#pragma once
/* VxWorks logLib shim — variadic printf wrapper with timestamp */
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static inline void logMsg(const char *fmt, ...)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    va_list args;

    if (lt) {
        printf("[%02d:%02d:%02d] ", lt->tm_hour, lt->tm_min, lt->tm_sec);
    }
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}
