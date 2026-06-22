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
 * @file AudioConfigHandler.h
 * @brief Runtime TCP command handlers for Audio DSP configuration.
 *
 * Provides 4 command handlers for runtime Audio DSP configuration over TCP:
 *   - STATUS: Return current parameter values for all 6 mode sets
 *   - SET_PARAM: Set a single parameter value
 *   - SET_GAIN_BLOCK: Set all 15 parameters for a specific mode set
 *   - VERIFY: Verify CRC32 of stored configuration in flash
 *
 * TCP Communication:
 *   - Port: 5558 (separate from code download port 5518)
 *   - Message format: Binary struct with command type and payload
 *   - Response format: Status byte + optional data payload
 *
 * Usage:
 *   AudioConfigTask receives TCP messages on port 5558,
 *   dispatches to appropriate handler via audio_config_handle_command(),
 *   which calls the specific AudioCmdHandle*() function based on cmd_type.
 */

#ifndef AUDIOCONFIGHANDLER_H
#define AUDIOCONFIGHANDLER_H

#include <stdint.h>
#include "AudioConf/AudioParamDef.h"

/* ========================================================================= */
/* Constants                                                                  */
/* ========================================================================= */

/** TCP port for audio configuration (runtime commands only). */
#define AUDIO_CONFIG_TCP_PORT       5558

/** Maximum TCP message payload size (bytes). SET_META needs header_id(40)+version(30)=70. */
#define AUDIO_CMD_MAX_PAYLOAD       70

/** Response data buffer size (bytes). */
/* Response data payload size - must accommodate STATUS response (6 modes × 15 params = 90 bytes) */
#define AUDIO_RSP_DATA_SIZE         128

/* ========================================================================= */
/* Command Type Enumeration                                                   */
/* ========================================================================= */

/**
 * @brief Audio configuration command types.
 *
 * Command types 0x01-0x7F are requests.
 * Response messages set bit 7 (0x80) to distinguish from requests.
 */
typedef enum {
    AUDIO_CMD_STATUS = 0x01,        /**< Request: Get current parameter status */
    AUDIO_CMD_SET_PARAM = 0x02,     /**< Request: Set single parameter */
    AUDIO_CMD_SET_GAIN_BLOCK = 0x03,/**< Request: Set full gain block (all 15 params) */
    AUDIO_CMD_VERIFY = 0x04,        /**< Request: Verify CRC32 of stored config */
    AUDIO_CMD_SET_META = 0x05,      /**< Request: Persist header_id + version to flash */
    AUDIO_CMD_GET_META = 0x06,      /**< Request: Read header_id + version from flash */
    AUDIO_CMD_RESPONSE = 0x80       /**< Response marker (OR'd with request type) */
} AudioCommandType;

/* ========================================================================= */
/* Status/Error Codes                                                         */
/* ========================================================================= */

/**
 * @brief Command execution status codes.
 */
typedef enum {
    AUDIO_STATUS_OK = 0,            /**< Command executed successfully */
    AUDIO_STATUS_ERROR = 1,         /**< General error */
    AUDIO_STATUS_INVALID_CMD = 2,   /**< Invalid command type */
    AUDIO_STATUS_INVALID_MODE = 3,  /**< Invalid mode set index (0-5 expected) */
    AUDIO_STATUS_INVALID_PARAM = 4, /**< Invalid parameter index (0-14 expected) */
    AUDIO_STATUS_INVALID_VALUE = 5, /**< Invalid gain value (out of range) */
    AUDIO_STATUS_FLASH_ERROR = 6,   /**< Flash read/write error */
    AUDIO_STATUS_CRC_ERROR = 7,     /**< CRC32 verification failed */
    AUDIO_STATUS_SPI_ERROR = 8,     /**< SPI communication error */
    AUDIO_STATUS_BUSY = 9           /**< System busy, try again later */
} AudioCommandStatus;

/* ========================================================================= */
/* Message Structures                                                         */
/* ========================================================================= */

/**
 * @brief TCP request message structure.
 *
 * Layout:
 *   [cmd_type:1][payload:variable]
 *
 * Payload format depends on cmd_type:
 *   - STATUS: No payload
 *   - SET_PARAM: [mode_set:1][param_offset:1][gain_value:1]
 *   - SET_GAIN_BLOCK: [mode_set:1][gain_block:15]
 *   - VERIFY: No payload
 */
typedef struct {
    uint8_t cmd_type;             /**< Command type (AudioCommandType) */
    uint8_t payload[AUDIO_CMD_MAX_PAYLOAD];  /**< Command-specific payload */
} AudioRequest;

/**
 * @brief TCP response message structure.
 *
 * Layout:
 *   [cmd_type:1][status:1][error_code:1][data:variable]
 *
 * cmd_type in response = request cmd_type | AUDIO_CMD_RESPONSE (0x80)
 * status: 0 = OK, 1 = ERROR
 * error_code: Specific error code if status = ERROR
 * data: Response data (parameter values, status info, etc.)
 */
typedef struct {
    uint8_t cmd_type;             /**< Original command | 0x80 */
    uint8_t status;               /**< 0=OK, 1=ERROR */
    uint8_t error_code;           /**< Specific error code if status=ERROR */
    uint8_t data[AUDIO_RSP_DATA_SIZE];  /**< Response data payload */
} AudioResponse;

/**
 * @brief SET_PARAM command payload structure.
 */
typedef struct {
    uint8_t mode_set;             /**< Mode set index (0-5) */
    uint8_t param_offset;         /**< Parameter offset (0-14) */
    uint8_t gain_value;           /**< Gain value (range depends on parameter) */
} __attribute__((packed)) SetParamPayload;

/**
 * @brief SET_GAIN_BLOCK command payload structure.
 */
typedef struct {
    uint8_t mode_set;             /**< Mode set index (0-5) */
    uint8_t gain_block[AUDIO_GAIN_BLOCK_SIZE];  /**< 15 gain values */
} __attribute__((packed)) SetGainBlockPayload;

/**
 * @brief SET_META command payload structure.
 * Carries header_id and version strings to persist in flash.
 * Payload total: 40 + 30 = 70 bytes.
 */
typedef struct {
    uint8_t header_id[AUDIO_CFG_HEADER_ID_SIZE]; /**< 40 bytes, null-padded ASCII */
    uint8_t version[AUDIO_CFG_VERSION_SIZE];      /**< 30 bytes, null-padded ASCII */
} __attribute__((packed)) SetMetaPayload;

/* ========================================================================= */
/* Handler Function Prototypes                                                */
/* ========================================================================= */

/**
 * @brief Handle STATUS command - return current parameter values.
 *
 * Reads current configuration from flash and returns all 15 parameters
 * for all 6 mode sets (90 bytes total).
 *
 * @param client_fd TCP client socket descriptor
 * @param msg Received message buffer (AudioRequest)
 * @return OK (0) on success, ERROR (-1) on failure
 *
 * Response data format:
 *   data[0..89]   = 6 * 15 = 90 bytes gain params
 *   data[90..119] = version[30] stored in flash (null if never set via SET_META)
 */
int AudioCmdHandleStatus(int client_fd, uint8_t *msg);

/**
 * @brief Handle SET_PARAM command - set a single parameter value.
 *
 * Sets one parameter value for a specific mode set, updates flash,
 * and dispatches SPI command to DSP.
 *
 * @param client_fd TCP client socket descriptor
 * @param msg Received message buffer (AudioRequest with SetParamPayload)
 * @return OK (0) on success, ERROR (-1) on failure
 *
 * Request payload format:
 *   [mode_set:1][param_offset:1][gain_value:1]
 *
 * Validation:
 *   - mode_set: 0-5 (AUDIO_MODE_CNT - 1)
 *   - param_offset: 0-14 (AUDIO_GAIN_PARAM_CNT - 1)
 *   - gain_value: Range depends on parameter type (see AudioParamDef.h)
 */
int AudioCmdHandleSetParam(int client_fd, uint8_t *msg);

/**
 * @brief Handle SET_GAIN_BLOCK command - set all 15 parameters for a mode.
 *
 * Sets the complete gain block (all 15 parameters) for a specific mode set,
 * updates flash, and dispatches all 15 SPI commands to DSP.
 *
 * @param client_fd TCP client socket descriptor
 * @param msg Received message buffer (AudioRequest with SetGainBlockPayload)
 * @return OK (0) on success, ERROR (-1) on failure
 *
 * Request payload format:
 *   [mode_set:1][gain_block:15]
 *
 * Validation:
 *   - mode_set: 0-5 (AUDIO_MODE_CNT - 1)
 *   - All 15 gain values within valid ranges
 */
int AudioCmdHandleSetGainBlock(int client_fd, uint8_t *msg);

/**
 * @brief Handle VERIFY command - verify CRC32 of stored configuration.
 *
 * Reads configuration from flash, computes CRC32, and compares with
 * stored value. Returns verification result.
 *
 * @param client_fd TCP client socket descriptor
 * @param msg Received message buffer (AudioRequest)
 * @return OK (0) on success, ERROR (-1) on failure
 *
 * Response data format:
 *   [crc32_stored:4][crc32_computed:4][match:1]
 *   match: 1 if CRCs match, 0 if mismatch
 */
int AudioCmdHandleVerify(int client_fd, uint8_t *msg);

/**
 * @brief Handle SET_META command - persist header_id and version to flash.
 *
 * Reads current configuration from flash, replaces header_id and version
 * fields, recalculates CRC32, and writes back.
 *
 * @param client_fd TCP client socket descriptor
 * @param msg Received message buffer (AudioRequest with SetMetaPayload)
 * @return OK (0) on success, ERROR (-1) on failure
 *
 * Request payload format:
 *   [header_id:40][version:30]
 */
int AudioCmdHandleSetMeta(int client_fd, uint8_t *msg);

/**
 * @brief Handle GET_META command - read header_id and version from flash.
 *
 * Reads current configuration from flash and returns header_id and version.
 *
 * @param client_fd TCP client socket descriptor
 * @param msg Received message buffer (AudioRequest, no payload)
 * @return OK (0) on success, ERROR (-1) on failure
 *
 * Response data format:
 *   data[0..39]  = header_id[40] stored in flash
 *   data[40..69] = version[30]  stored in flash
 */
int AudioCmdHandleGetMeta(int client_fd, uint8_t *msg);

/**
 * @brief Apply all stored configuration from flash to DSP via SPI.
 *
 * Reads the full AudioConfigFileData from flash, verifies CRC32, and
 * dispatches all 6 × 15 = 90 SPI commands to restore DSP state.
 * Called once at system startup before accepting TCP connections.
 * Skips silently if flash is empty or CRC does not match.
 */
void AudioApplyAllFromFlash(void);

#endif /* AUDIOCONFIGHANDLER_H */
