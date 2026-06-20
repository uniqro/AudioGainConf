#pragma once
/* VxWorks taskLib shim — declarations; implementation in src/tasklib_stubs.c */

int  taskSpawn(const char *name, int priority, int options, int stackSize,
               void (*entry)(void),
               int a1, int a2, int a3, int a4, int a5,
               int a6, int a7, int a8, int a9, int a10);
void taskDelay(int ticks);
int  sysClkRateGet(void);
