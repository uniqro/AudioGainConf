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
 * @file AudioConf.h
 * @brief Audio Configuration Module initialization interface.
 *
 * This module provides the initialization function for the Audio Configuration
 * subsystem. It spawns the AudioConfigTask which handles TCP-based audio
 * configuration commands on port 5558.
 */

#ifndef AUDIOCONF_H
#define AUDIOCONF_H

#include "vxWorks.h"

/**
 * @brief Initialize the Audio Configuration module.
 *
 * This function spawns the AudioConfigTask which creates a TCP server
 * listening on port 5558 for audio configuration commands.
 *
 * @returns OK on success, ERROR on failure
 *
 * @note Should be called during system initialization
 * @note Task constants are defined in AudioConfigTask.h:
 *   - Priority: 70 (RManager 68-70 compatible)
 *   - Stack: 5000 bytes
 *   - TCP Port: 5558
 */
STATUS AudioConf_Init(void);

#endif /* AUDIOCONF_H */
