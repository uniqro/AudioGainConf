#pragma once
/* VxWorks ioLib shim — close() and fioRead() come from sockLib.h */
#include "sockLib.h"

/*
 * Override fioRead with a dispatch-aware wrapper.
 *
 * AudioConfigProcessMessage in AudioConfigTask.c is a TODO stub that logs
 * but never calls the AudioCmdHandle*() handlers.  We intercept at the
 * fioRead level: after the actual recv, we call the command dispatcher with
 * the same client socket fd, then return normally so AudioConfigHandleClient
 * can continue its housekeeping (logging, state transitions).
 *
 * The original fioRead macro (= recv wrapper) must be undefined first.
 */
#undef fioRead

/* Forward declaration — defined in pc_port/src/dispatch_glue.c */
int _pcport_fioRead_dispatch(int fd, char *buf, int n);

#define fioRead(fd, buf, n)  _pcport_fioRead_dispatch((fd), (buf), (n))
