/**
 * Copyright Aselsan A.S.
 *
 * The copyright to the computer program(s) herein
 * is the property of Aselsan A.S.
 * The program(s) may be used and/or copied only with
 * the written permission of Aselsan A.S.
 * or in accordance with the terms and
 * conditions stipulated in the agreement/contract
 * under which the program(s) have been supplied.
 */

/**
 * @file AudioConfigTask.h
 * @brief Audio Configuration TCP Task for VxWorks RTOS.
 *
 * Implements a TCP server task that listens on port 5558 for audio
 * configuration commands. Supports max 4 simultaneous client connections.
 *
 * Task constants:
 *   - Priority: 70 (RManager 68-70 compatible)
 *   - Stack: 5000 bytes
 *   - TCP Port: 5558
 *   - Max Connections: 4
 */

#ifndef AUDIOCONFIGTASK_H
#define AUDIOCONFIGTASK_H

#include <vxWorks.h>
#include <sockLib.h>
#include <inetLib.h>
#include <ioLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

/* ========================================================================= */
/* Task Constants                                                             */
/* ========================================================================= */

/**
 * @brief Audio configuration task priority.
 * Compatible with RManager task priority range (68-70).
 */
#define TASK_AUDIO_CONFIG_PRIORITY    70

/**
 * @brief Audio configuration task stack size in bytes.
 */
#define TASK_AUDIO_CONFIG_STACK       5000

/**
 * @brief Audio configuration task name.
 */
#define TASK_AUDIO_CONFIG_NAME        "audio_config_task"

/**
 * @brief TCP port number for audio configuration.
 * Separate from existing RadioConnect port (5518).
 */
#define AUDIO_CONFIG_PORT_NUM         5558

/**
 * @brief Maximum number of simultaneous client connections.
 */
#define AUDIO_CONFIG_MAX_CONNECTIONS  4

/**
 * @brief TCP receive buffer size for configuration messages.
 */
#define AUDIO_CONFIG_MSG_SIZE         1024

/* ========================================================================= */
/* Task State Machine                                                         */
/* ========================================================================= */

/**
 * @brief Audio configuration task states.
 *
 * State transition sequence:
 *   INIT -> LISTENING -> CONNECTED -> RECEIVING -> PROCESSING -> DISCONNECTED
 *                              ^                    |
 *                              |____________________|
 */
typedef enum {
    AC_STATE_INIT = 0,           /**< Task initialization */
    AC_STATE_LISTENING,          /** Socket listen() + accept() */
    AC_STATE_CONNECTED,          /**< Client connected */
    AC_STATE_RECEIVING,          /**< TCP recv() in progress */
    AC_STATE_PROCESSING,         /**< Processing configuration parameters */
    AC_STATE_DISCONNECTED,       /**< Client disconnected */
    AC_STATE_ERROR               /**< Error state */
} AudioConfigTaskState;

/* ========================================================================= */
/* Client Connection Structure                                                */
/* ========================================================================= */

/**
 * @brief Client connection state structure.
 *
 * Tracks individual client connection state and socket file descriptor.
 */
typedef struct {
    int clientFd;                          /**< Client socket file descriptor */
    AudioConfigTaskState state;            /**< Current connection state */
    struct sockaddr_in clientAddr;         /**< Client address information */
    socklen_t clientAddrLen;               /**< Client address length */
} AudioConfigClient;

/* ========================================================================= */
/* Global State                                                               */
/* ========================================================================= */

#ifdef ALLOCATE_AUDIOCONFIGTASK_EXP_H
    #define AUDIOCONFIGTASK_EXT
#else
    #define AUDIOCONFIGTASK_EXT extern
#endif

/**
 * @brief Global audio configuration task state.
 */
AUDIOCONFIGTASK_EXT volatile AudioConfigTaskState AudioConfigState;

/**
 * @brief Server socket file descriptor.
 */
AUDIOCONFIGTASK_EXT int AudioConfigServerSocket;

/**
 * @brief Client connections array (max AUDIO_CONFIG_MAX_CONNECTIONS).
 */
AUDIOCONFIGTASK_EXT AudioConfigClient AudioConfigClients[AUDIO_CONFIG_MAX_CONNECTIONS];

/* ========================================================================= */
/* Function Prototypes                                                        */
/* ========================================================================= */

/**
 * @brief Audio configuration task main function.
 *
 * Implements TCP server loop:
 *   1. Create socket
 *   2. Bind to port 5558
 *   3. Listen for connections
 *   4. Accept client connections (max 4)
 *   5. Receive and process configuration messages
 *   6. Handle client disconnect
 *
 * @returns None (infinite loop RTOS task)
 *
 * @note Task spawned with priority 70, stack 5000 bytes
 * @note Uses existing VxWorks socket API (same as RConnect.c)
 */
void AudioConfigTask(void);

/**
 * @brief Initialize audio configuration task state.
 *
 * Resets global state and client connections array.
 */
void AudioConfigTaskInit(void);

/**
 * @brief Handle client connection.
 *
 * @param clientIdx - Index into AudioConfigClients array
 * @returns OK on success, ERROR on failure
 */
STATUS AudioConfigHandleClient(int clientIdx);

/**
 * @brief Process received configuration message.
 *
 * @param clientIdx - Index into AudioConfigClients array
 * @param buffer - Message buffer
 * @param length - Message length
 * @returns OK on success, ERROR on failure
 */
STATUS AudioConfigProcessMessage(int clientIdx, char* buffer, int length);

#endif /* AUDIOCONFIGTASK_H */
