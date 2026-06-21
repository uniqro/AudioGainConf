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
 *   3. Returns the recv byte count (positive).
 *
 * Connection lifetime is managed by the VxWorks state machine:
 *   - AudioConfigTask.c:AC_STATE_CONNECTED loop calls AudioConfigHandleClient
 *     repeatedly as long as clientFd != NULL.  Each fioRead call here blocks
 *     until the Python client sends the next command, so multiple commands can
 *     be served over one TCP session without reconnecting.
 *   - When Python closes the TCP connection, recv returns 0.  This function
 *     returns 0, AudioConfigHandleClient detects the error and calls
 *     AudioConfigCloseClient (closes fd, nulls slot), and the state machine
 *     transitions to AC_STATE_LISTENING naturally.
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
    uint8_t cmd;

    /* Real blocking recv — same semantics as original fioRead */
    r = recv(fd, buf, n, 0);
    if (r <= 0)
        return r;   /* error or graceful close — let caller handle */

    /* Dispatch the received command to the appropriate handler */
    cmd = (uint8_t)buf[0];

    /* Minimum frame sizes (cmd byte + payload):
     *   STATUS / VERIFY : 1 byte  (cmd only)
     *   SET_PARAM       : 4 bytes (cmd + mode + offset + value)
     *   SET_GAIN_BLOCK  : 17 bytes (cmd + mode + 15 gain values)
     *   SET_META        : 71 bytes (cmd + header_id[40] + version[30]) */
    {
        static const int min_len[6] = { 0, 1, 4, 17, 1, 71 };
        if (cmd >= 1 && cmd <= 5 && r < min_len[cmd]) {
            logMsg("AudioConfig: short frame cmd=0x%02X len=%d (min=%d)\n",
                   cmd, r, min_len[cmd], 0, 0, 0);
            return r;
        }
    }

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
        case AUDIO_CMD_SET_META:
            AudioCmdHandleSetMeta(fd, (uint8_t *)buf);
            break;
        default:
            logMsg("AudioConfig: Unknown cmd=0x%02X on fd=%d\n",
                   cmd, fd, 0, 0, 0, 0);
            break;
    }

    return r;
}
