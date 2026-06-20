/*
 * tasklib_stubs.c — VxWorks task API implementation for PC
 *
 * taskSpawn  → pthread_create  (Linux) / _beginthreadex (Windows)
 * taskDelay  → sleep scaled to 60 Hz (sysClkRateGet returns 60)
 */

#include "vxWorks.h"
#include "taskLib.h"
#include <stdio.h>

#ifdef _WIN32
#  include <windows.h>
#  include <process.h>

static unsigned __stdcall _task_entry_win(void *arg)
{
    void (*fn)(void) = (void (*)(void))arg;
    fn();
    return 0;
}

int taskSpawn(const char *name, int priority, int options, int stackSize,
              void (*entry)(void),
              int a1, int a2, int a3, int a4, int a5,
              int a6, int a7, int a8, int a9, int a10)
{
    HANDLE h;
    (void)priority; (void)options;
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    (void)a6; (void)a7; (void)a8; (void)a9; (void)a10;

    h = (HANDLE)_beginthreadex(NULL,
                               (unsigned)(stackSize > 0 ? stackSize : 8192),
                               _task_entry_win,
                               (void *)entry,
                               0, NULL);
    if (h == NULL) {
        fprintf(stderr, "[TASK] Failed to create thread for '%s'\n", name);
        return ERROR;
    }
    printf("[TASK] Spawned '%s' (handle=%p)\n", name, (void *)h);
    CloseHandle(h);   /* detach — task runs independently */
    return 1;         /* non-zero = valid task id */
}

void taskDelay(int ticks)
{
    DWORD ms = (ticks > 0) ? (DWORD)((uint64_t)ticks * 1000 / 60) : 0;
    Sleep(ms);
}

#else  /* Linux / macOS */

#include <pthread.h>
#include <unistd.h>

static void *_task_entry_posix(void *arg)
{
    void (*fn)(void) = (void (*)(void))arg;
    fn();
    return NULL;
}

int taskSpawn(const char *name, int priority, int options, int stackSize,
              void (*entry)(void),
              int a1, int a2, int a3, int a4, int a5,
              int a6, int a7, int a8, int a9, int a10)
{
    pthread_t tid;
    pthread_attr_t attr;
    (void)priority; (void)options;
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    (void)a6; (void)a7; (void)a8; (void)a9; (void)a10;

    pthread_attr_init(&attr);
    if (stackSize > 0)
        pthread_attr_setstacksize(&attr, (size_t)stackSize);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, _task_entry_posix, (void *)entry) != 0) {
        fprintf(stderr, "[TASK] pthread_create failed for '%s'\n", name);
        pthread_attr_destroy(&attr);
        return ERROR;
    }
    pthread_attr_destroy(&attr);
    printf("[TASK] Spawned '%s' (tid=%lu)\n", name, (unsigned long)tid);
    return (int)(intptr_t)tid;
}

void taskDelay(int ticks)
{
    if (ticks > 0)
        usleep((useconds_t)((uint64_t)ticks * 1000000UL / 60));
}

#endif  /* _WIN32 */

int sysClkRateGet(void)
{
    return 60;   /* VxWorks default: 60 ticks per second */
}
