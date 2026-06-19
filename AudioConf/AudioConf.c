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
 * @file AudioConf.c
 * @brief Audio Configuration Module initialization implementation.
 *
 * Initializes the Audio Configuration subsystem by spawning AudioConfigTask.
 * Follows the same pattern as RConnect module (Common\RSupport\RConnect.c).
 */

#include "vxWorks.h"
#include "logLib.h"
#include "taskLib.h"
#include "AudioConf.h"
#include "AudioConfigTask.h"

/**
 * @brief Initialize the Audio Configuration module.
 *
 * Spawns AudioConfigTask with the following parameters:
 *   - Name: "audio_config_task"
 *   - Priority: 70 (compatible with RManager 68-70 range)
 *   - Stack: 5000 bytes
 *   - Entry: AudioConfigTask function
 *
 * @returns OK on success, ERROR if taskSpawn fails
 *
 * @note Called from usrAppInit() during system initialization
 */
STATUS AudioConf_Init(void)
{
    int taskId;
    
    taskId = taskSpawn(
        TASK_AUDIO_CONFIG_NAME,
        TASK_AUDIO_CONFIG_PRIORITY,
        0,  /* Options: none */
        TASK_AUDIO_CONFIG_STACK,
        (FUNCPTR)AudioConfigTask,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    );
    
    if (taskId == ERROR)
    {
        logMsg("AUDIO_CONF ERROR: Failed to spawn AudioConfigTask\n", 0, 0, 0, 0, 0, 0);
        return ERROR;
    }
    
    logMsg("AUDIO_CONF: AudioConfigTask spawned (PID: %d)\n", taskId, 0, 0, 0, 0, 0);
    return OK;
}
