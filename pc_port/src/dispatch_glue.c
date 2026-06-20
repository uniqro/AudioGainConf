/*
 * dispatch_glue.c — Hooks fioRead to dispatch audio command handlers.
 *
 * AudioConfigProcessMessage in AudioConfigTask.c is a TODO stub that logs
 * but never calls the AudioCmdHandle*() handlers.  The GCC --wrap linker
 * trick doesn't intercept same-TU calls, so we hook at a lower level:
 *
 * ioLib.h overrides the fioRead macro → _pcport_fioRead_dispatch().
 *
 * _pcport_fioRead_dispatch():
 *   1. Calls real recv (blocking, same semantics as original fioRead).
 *   2. If bytes received, dispatches to AudioCmdHandle*() → sends response.
 *   3. Cleans up the client slot (close + NULL fd) so the server can accept
 *      the next connection.  Without this cleanup the VxWorks state machine's
 *      AudioConfigFindFreeClient() quickly exhausts all MAX_CONNECTIONS slots.
 *   4. Returns the recv byte count so AudioConfigHandleClient's housekeeping
 *      (error detection, state transitions) still works correctly.
 */

#include "sockLib.h"
#include "AudioConf/AudioConfigTask.h"
#include "AudioConf/AudioConfigHandler.h"
#include "logLib.h"
#include <stdio.h>

/*
 * _pcport_fioRead_dispatch — recv wrapper with inline command dispatch.
 *
 * Called via the fioRead macro defined in ioLib.h.
 * fd  : client socket file descriptor
 * buf : receive buffer (owned by AudioConfigHandleClient in AudioConfigTask.c)
 * n   : requested byte count
 */
int _pcport_fioRead_dispatch(int fd, char *buf, int n)
{
    int     r;
    int     i;
    uint8_t cmd;

    /* Real blocking recv — same semantics as original fioRead */
    r = recv(fd, buf, n, 0);
    if (r <= 0)
        return r;   /* error or graceful close — let caller handle */

    /* Dispatch the received command to the appropriate handler */
    cmd = (uint8_t)buf[0];
    printf("[DISPATCH] fd=%d  cmd=0x%02X  len=%d\n", fd, cmd, r);
    fflush(stdout);

    switch (cmd) {
        case AUDIO_CMD_STATUS:
            AudioCmdHandleStatus(fd, (uint8_t *)buf);
            break;
        case AUDIO_CMD_SET_PARAM:
            AudioCmdHandleSetParam(fd, (uint8_t *)buf);
            break;
        case AUDIO_CMD_SET_GAIN_BLOCK:
            AudioCmdHandleSetGainBlock(fd, (uint8_t *)buf);
            break;
        case AUDIO_CMD_VERIFY:
            AudioCmdHandleVerify(fd, (uint8_t *)buf);
            break;
        default:
            logMsg("AudioConfig: Unknown cmd=0x%02X on fd=%d\n",
                   cmd, fd, 0, 0, 0, 0);
            break;
    }

    /*
     * Close the client connection and free the slot.
     *
     * The VxWorks state machine in AudioConfigTask.c only frees client slots
     * on ERROR (when fioRead returns 0/-1 after peer close).  But after we
     * dispatch and send a response, AudioConfigHandleClient returns OK and
     * the slot stays occupied with a stale fd.  AudioConfigFindFreeClient()
     * would then fail after MAX_CONNECTIONS requests.
     *
     * Fix: close the fd and NULL the slot here, right after the response is
     * sent.  AudioConfigHandleClient then sees clientFd == NULL and the state
     * machine transitions correctly to AC_STATE_LISTENING.
     */
    close(fd);   /* _pcw_close() via sockLib.h macro on Windows */

    for (i = 0; i < AUDIO_CONFIG_MAX_CONNECTIONS; i++) {
        if (AudioConfigClients[i].clientFd == fd) {
            AudioConfigClients[i].clientFd = 0;  /* NULL — slot is free */
            AudioConfigClients[i].state    = AC_STATE_DISCONNECTED;
            break;
        }
    }

    return r;   /* return byte count to AudioConfigHandleClient */
}
