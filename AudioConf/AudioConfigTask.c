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
 * @file AudioConfigTask.c
 * @brief Audio Configuration TCP Task implementation for VxWorks RTOS.
 *
 * Implements TCP server on port 5558 for audio configuration commands.
 * Follows the same pattern as RConnect.c (Common\RSupport\RConnect.c).
 */

#define ALLOCATE_AUDIOCONFIGTASK_EXP_H
#include "AudioConfigTask.h"
#include "AudioParamDef.h"

/* Global state definitions */
volatile AudioConfigTaskState AudioConfigState = AC_STATE_INIT;
int AudioConfigServerSocket = 0;
AudioConfigClient AudioConfigClients[AUDIO_CONFIG_MAX_CONNECTIONS];

/* ========================================================================= */
/* Static Function Prototypes                                                 */
/* ========================================================================= */

static STATUS AudioConfigInitClients(void);
static int AudioConfigFindFreeClient(void);
static void AudioConfigCloseClient(int clientIdx);

/* ========================================================================= */
/* Implementation                                                             */
/* ========================================================================= */

/**
 * @brief Initialize client connections array.
 *
 * Resets all client connection states and file descriptors.
 *
 * @returns OK always
 */
static STATUS AudioConfigInitClients(void)
{
    int i;
    for (i = 0; i < AUDIO_CONFIG_MAX_CONNECTIONS; i++)
    {
        AudioConfigClients[i].clientFd = 0;
        AudioConfigClients[i].state = AC_STATE_INIT;
        AudioConfigClients[i].clientAddrLen = 0;
    }
    return OK;
}

/**
 * @brief Find a free client connection slot.
 *
 * @returns Index of free slot, -1 if no free slots available
 */
static int AudioConfigFindFreeClient(void)
{
    int i;
    for (i = 0; i < AUDIO_CONFIG_MAX_CONNECTIONS; i++)
    {
        if (AudioConfigClients[i].clientFd == 0)
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Close a client connection and reset state.
 *
 * @param clientIdx - Index into AudioConfigClients array
 */
static void AudioConfigCloseClient(int clientIdx)
{
    if ((clientIdx >= 0) && (clientIdx < AUDIO_CONFIG_MAX_CONNECTIONS))
    {
        if (AudioConfigClients[clientIdx].clientFd != 0)
        {
            logMsg("AudioConfig: Closing client %d, fd: %d\n", 
                   clientIdx, AudioConfigClients[clientIdx].clientFd, 0, 0, 0, 0);
            close(AudioConfigClients[clientIdx].clientFd);
            AudioConfigClients[clientIdx].clientFd = 0;
        }
        AudioConfigClients[clientIdx].state = AC_STATE_DISCONNECTED;
    }
}

/**
 * @brief Initialize audio configuration task state.
 *
 * Resets global state and client connections array.
 */
void AudioConfigTaskInit(void)
{
    AudioConfigState = AC_STATE_INIT;
    AudioConfigServerSocket = 0;
    AudioConfigInitClients();
    logMsg("AudioConfig: Task initialized\n", 0, 0, 0, 0, 0, 0);
}

/**
 * @brief Process received configuration message.
 *
 * @param clientIdx - Index into AudioConfigClients array
 * @param buffer - Message buffer
 * @param length - Message length
 * @returns OK on success, ERROR on failure
 *
 * @note TODO: Implement message parsing and dispatch to AudioConfigHandler
 */
STATUS AudioConfigProcessMessage(int clientIdx, char* buffer, int length)
{
    STATUS status = OK;
    
    if ((buffer == NULL) || (length <= 0))
    {
        logMsg("AudioConfig: Invalid message from client %d\n", 
               clientIdx, 0, 0, 0, 0, 0);
        return ERROR;
    }
    
    AudioConfigState = AC_STATE_PROCESSING;
    
    /* TODO: Parse message and dispatch to AudioConfigHandler */
    /* For now, just log the received message */
    logMsg("AudioConfig: Received %d bytes from client %d\n", 
           length, clientIdx, 0, 0, 0, 0);
    
    /* TODO: After processing, send response back to client */
    /* TODO: Integrate with AudioSpiGateway to send config to DSP */
    
    AudioConfigState = AC_STATE_CONNECTED;
    
    return status;
}

/**
 * @brief Handle client connection.
 *
 * @param clientIdx - Index into AudioConfigClients array
 * @returns OK on success, ERROR on failure
 *
 * Receives configuration messages from client and processes them.
 * Implements receive timeout and error handling.
 */
STATUS AudioConfigHandleClient(int clientIdx)
{
    STATUS status = OK;
    int nRead;
    char recvBuffer[AUDIO_CONFIG_MSG_SIZE];
    
    if ((clientIdx < 0) || (clientIdx >= AUDIO_CONFIG_MAX_CONNECTIONS))
    {
        return ERROR;
    }
    
    AudioConfigClients[clientIdx].state = AC_STATE_RECEIVING;
    
    /* Receive message from client */
    nRead = fioRead(AudioConfigClients[clientIdx].clientFd, 
                    recvBuffer, 
                    sizeof(recvBuffer));
    
    if ((nRead == ERROR) || (nRead <= 0))
    {
        logMsg("AudioConfig: Read error from client %d, nRead: %d\n", 
               clientIdx, nRead, 0, 0, 0, 0);
        AudioConfigCloseClient(clientIdx);
        return ERROR;
    }
    
    /* Process the received message */
    status = AudioConfigProcessMessage(clientIdx, recvBuffer, nRead);

    if (status == ERROR)
    {
        logMsg("AudioConfig: Process error from client %d\n",
               clientIdx, 0, 0, 0, 0, 0);
        AudioConfigCloseClient(clientIdx);
    }
    else
    {
        /* Restore slot state so the AC_STATE_CONNECTED loop finds this
         * client again on the next iteration and waits for the next command.
         * Without this reset the loop sees AC_STATE_RECEIVING, falls through
         * to the "no active clients" check, and transitions back to
         * AC_STATE_LISTENING — making every command after the first time out. */
        AudioConfigClients[clientIdx].state = AC_STATE_CONNECTED;
    }

    return status;
}

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
 * State machine:
 *   INIT -> LISTENING -> CONNECTED -> RECEIVING -> PROCESSING -> CONNECTED
 *                                      ^                    |
 *                                      |____________________|
 *
 * @returns None (infinite loop RTOS task)
 *
 * @note Task spawned with priority 70, stack 5000 bytes
 * @note Uses existing VxWorks socket API (same as RConnect.c)
 */
void AudioConfigTask(void)
{
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    int clientSocket;
    int optval;
    int clientIdx;
    int sockAddrSize;
    
    /* Initialize state */
    AudioConfigTaskInit();
    
    /* Main task loop (infinite) */
    while (1)
    {
        switch (AudioConfigState)
        {
            case AC_STATE_INIT:
                /* Initialize server socket */
                AudioConfigServerSocket = 0;
                sockAddrSize = sizeof(struct sockaddr_in);
                
                /* Zero out server address structure */
                bzero((char*)&serverAddr, sockAddrSize);
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(AUDIO_CONFIG_PORT_NUM);
                serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                
                /* Create socket */
                AudioConfigServerSocket = socket(AF_INET, SOCK_STREAM, 0);
                if (AudioConfigServerSocket == ERROR)
                {
                    logMsg("AudioConfig: Socket creation failed\n", 0, 0, 0, 0, 0, 0);
                    AudioConfigState = AC_STATE_ERROR;
                    break;
                }
                
                /* Bind socket to port 5558 */
                if (bind(AudioConfigServerSocket, 
                         (struct sockaddr*)&serverAddr, 
                         sockAddrSize) == ERROR)
                {
                    logMsg("AudioConfig: Bind failed, port: %d\n", 
                           AUDIO_CONFIG_PORT_NUM, 0, 0, 0, 0, 0);
                    close(AudioConfigServerSocket);
                    AudioConfigState = AC_STATE_ERROR;
                    break;
                }
                
                /* Start listening */
                if (listen(AudioConfigServerSocket, 
                           AUDIO_CONFIG_MAX_CONNECTIONS) == ERROR)
                {
                    logMsg("AudioConfig: Listen failed\n", 0, 0, 0, 0, 0, 0);
                    close(AudioConfigServerSocket);
                    AudioConfigState = AC_STATE_ERROR;
                    break;
                }
                
                logMsg("AudioConfig: Listening on port %ld\n", 
                       AUDIO_CONFIG_PORT_NUM, 0, 0, 0, 0, 0);
                AudioConfigState = AC_STATE_LISTENING;
                break;
            
            case AC_STATE_LISTENING:
                /* Accept new client connection */
                clientAddrLen = sizeof(struct sockaddr_in);
                clientSocket = accept(AudioConfigServerSocket, 
                                      (struct sockaddr*)&clientAddr, 
                                      &clientAddrLen);
                
                if (clientSocket == ERROR)
                {
                    logMsg("AudioConfig: Accept failed\n", 0, 0, 0, 0, 0, 0);
                    AudioConfigState = AC_STATE_ERROR;
                    break;
                }
                
                /* Find free client slot */
                clientIdx = AudioConfigFindFreeClient();
                if (clientIdx == -1)
                {
                    logMsg("AudioConfig: Max connections reached (%d)\n", 
                           AUDIO_CONFIG_MAX_CONNECTIONS, 0, 0, 0, 0, 0);
                    close(clientSocket);
                    break;
                }
                
                /* Set socket options for performance */
                optval = 1; /* TCP_NODELAY on */
                setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, 
                           (char*)&optval, sizeof(optval));
                
                optval = 0x10000; /* RX buffer size */
                setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, 
                           (char*)&optval, sizeof(optval));
                
                optval = 0x10000; /* TX buffer size */
                setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, 
                           (char*)&optval, sizeof(optval));
                
                optval = 1; /* SO_KEEPALIVE on */
                setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, 
                           (char*)&optval, sizeof(optval));
                
                /* Store client connection info */
                AudioConfigClients[clientIdx].clientFd = clientSocket;
                AudioConfigClients[clientIdx].clientAddr = clientAddr;
                AudioConfigClients[clientIdx].clientAddrLen = clientAddrLen;
                AudioConfigClients[clientIdx].state = AC_STATE_CONNECTED;
                
                logMsg("AudioConfig: Client %d connected, fd: %d\n", 
                       clientIdx, clientSocket, 0, 0, 0, 0);
                
                AudioConfigState = AC_STATE_CONNECTED;
                break;
            
            case AC_STATE_CONNECTED:
                /* Find active client and handle it */
                for (clientIdx = 0; clientIdx < AUDIO_CONFIG_MAX_CONNECTIONS; clientIdx++)
                {
                    if ((AudioConfigClients[clientIdx].clientFd != 0) &&
                        (AudioConfigClients[clientIdx].state == AC_STATE_CONNECTED))
                    {
                        /* Handle this client (blocking receive) */
                        AudioConfigHandleClient(clientIdx);
                        
                        /* After handling, return to listening for more connections */
                        if (AudioConfigClients[clientIdx].clientFd == 0)
                        {
                            /* Client disconnected, continue listening */
                            AudioConfigState = AC_STATE_LISTENING;
                        }
                        break;
                    }
                }
                
                /* If no active clients, go back to listening */
                if (clientIdx >= AUDIO_CONFIG_MAX_CONNECTIONS)
                {
                    AudioConfigState = AC_STATE_LISTENING;
                }
                break;
            
            case AC_STATE_RECEIVING:
                /* Handled in AudioConfigHandleClient() */
                break;
            
            case AC_STATE_PROCESSING:
                /* Handled in AudioConfigProcessMessage() */
                break;
            
            case AC_STATE_DISCONNECTED:
                /* Client disconnected, return to listening */
                AudioConfigState = AC_STATE_LISTENING;
                break;
            
            case AC_STATE_ERROR:
                /* Error state - attempt recovery */
                logMsg("AudioConfig: Error state, attempting recovery\n", 
                       0, 0, 0, 0, 0, 0);
                
                /* Close all client connections */
                for (clientIdx = 0; clientIdx < AUDIO_CONFIG_MAX_CONNECTIONS; clientIdx++)
                {
                    AudioConfigCloseClient(clientIdx);
                }
                
                /* Close server socket if open */
                if (AudioConfigServerSocket != 0)
                {
                    close(AudioConfigServerSocket);
                    AudioConfigServerSocket = 0;
                }
                
                /* Delay before retry */
                taskDelay(sysClkRateGet() * 5);
                
                /* Reset to INIT state */
                AudioConfigTaskInit();
                break;
            
            default:
                logMsg("AudioConfig: Unknown state: %d\n", 
                       AudioConfigState, 0, 0, 0, 0, 0);
                AudioConfigState = AC_STATE_ERROR;
                break;
        }
    }
}
