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
 * @file AudioConfigHandler.c
 * @brief Implementation of runtime TCP command handlers for Audio DSP configuration.
 *
 * Implements 4 command handlers:
 *   1. AudioCmdHandleStatus() - Return current parameter values
 *   2. AudioCmdHandleSetParam() - Set single parameter value
 *   3. AudioCmdHandleSetGainBlock() - Set full gain block (all 15 params)
 *   4. AudioCmdHandleVerify() - Verify CRC32 of stored config
 *
 * Each handler: parse → validate → process → respond
 * Uses non-blocking SPI dispatch via AudioSpiGateway functions.
 */

#include "AudioConf/AudioConfigHandler.h"
#include "AudioConf/AudioSpiGateway.h"
#include "AudioConf/AudioFlashMgr.h"
#include "AudioConf/AudioParamDef.h"

/* Compile-time guard: STATUS response (gains + version) must fit in data[] */
typedef char _status_rsp_fits[
    (AUDIO_MODE_CNT * AUDIO_GAIN_BLOCK_SIZE + AUDIO_CFG_VERSION_SIZE <= AUDIO_RSP_DATA_SIZE) ? 1 : -1];
#include "vxWorks.h"
#include "logLib.h"
#include "string.h"
#include "socket.h"

/* ========================================================================= */
/* Local Helper Functions                                                     */
/* ========================================================================= */

/**
 * @brief Send response message to TCP client.
 *
 * @param client_fd TCP client socket descriptor
 * @param response Response message to send
 * @return OK (0) on success, ERROR (-1) on send failure
 */
static int send_response(int client_fd, AudioResponse *response)
{
    int result;
    
    result = send(client_fd, (char *)response, sizeof(AudioResponse), 0);
    if (result < 0) {
        logMsg("AudioConfigHandler: send() failed, error=%d\n", result, 0, 0, 0, 0, 0);
        return ERROR;
    }
    
    return OK;
}

/**
 * @brief Validate gain value for a specific parameter.
 *
 * @param param_offset Parameter offset (0-14)
 * @param gain_value Gain value to validate
 * @return TRUE if valid, FALSE if invalid
 */
static int validate_gain_value(uint8_t param_offset, uint8_t gain_value)
{
    switch (param_offset) {
        case OFFSET_INPUT_LEVEL_CONTROL_GAIN:
            /* Range: 0-8 (9 discrete levels) */
            return (gain_value <= DB_MINUS_12_GAIN) ? TRUE : FALSE;
            
        case OFFSET_ADC_PGA_GAIN:
            /* Range: 0-119 (x2 scaled, 0-59.5dB) */
            return (gain_value <= 119) ? TRUE : FALSE;
            
        case OFFSET_SIDETONE_ATTENUATION_GAIN:
            /* Range: 0-120 (x2 scaled, 0 to -60dB) */
            return (gain_value <= 120) ? TRUE : FALSE;
            
        case OFFSET_OUTPUT_LEVEL_CONTROL_GAIN:
            /* Range: 0-9 (10 discrete levels) */
            return (gain_value <= OUT_LEVEL_9DB) ? TRUE : FALSE;
            
        case OFFSET_SPEAKER_ATTENUATION_GAIN:
            /* Range: 0-127 (x2 scaled, 0 to -63.5dB) */
            return (gain_value <= 127) ? TRUE : FALSE;
            
        case OFFSET_SOFT_AGC_ACTIVE:
            /* Range: 0-1 (PASSIVE/ACTIVE) */
            return (gain_value <= SOFT_AGC_ACTIVE) ? TRUE : FALSE;
            
        case OFFSET_SOFT_AGC_TARGET_MOD_INDEX:
            /* Range: 0-100 (%) */
            return (gain_value <= 100) ? TRUE : FALSE;
            
        case OFFSET_SOFT_AGC_MAX_GAIN:
            /* Range: 0-30 (dB) */
            return (gain_value <= 30) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_ACTIVE:
            /* Range: 0-1 (PASSIVE/ACTIVE) */
            return (gain_value <= 1) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_TARGET_GAIN:
            /* Range: 0-7 (8 discrete levels) */
            return (gain_value <= 7) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_ATTACK_TIME:
            /* Range: 0-3 (4 discrete levels) */
            return (gain_value <= 3) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_DECAY_TIME:
            /* Range: 0-3 (4 discrete levels) */
            return (gain_value <= 3) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_MAX_GAIN:
            /* Range: 0-119 (x2 scaled, 0-59.5dB) */
            return (gain_value <= 119) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_NOISE_HYSTERESIS_LEVEL:
            /* Range: 0-3 (4 discrete levels) */
            return (gain_value <= 3) ? TRUE : FALSE;
            
        case OFFSET_HARD_AGC_NOISE_THRESHOLD:
            /* Range: 0-31 (5-bit, 32 levels) */
            return (gain_value <= 31) ? TRUE : FALSE;
            
        default:
            return FALSE;
    }
}

/**
 * @brief Dispatch SPI command for a single parameter.
 *
 * @param param_offset Parameter offset (0-14)
 * @param mode_set Mode set index (0-5)
 * @param gain_value Gain value
 * @return OK (0) on success, ERROR (-1) on SPI error
 */
static int dispatch_spi_command(uint8_t param_offset, uint8_t mode_set, uint8_t gain_value)
{
    int result = ERROR;
    
    switch (param_offset) {
        case OFFSET_INPUT_LEVEL_CONTROL_GAIN:
            result = AudioSpiSetInputLevel((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_ADC_PGA_GAIN:
            result = AudioSpiSetAdcPga((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_SIDETONE_ATTENUATION_GAIN:
            result = AudioSpiSetSidetoneAttenuation((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_OUTPUT_LEVEL_CONTROL_GAIN:
            result = AudioSpiSetOutputLevel((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_SPEAKER_ATTENUATION_GAIN:
            result = AudioSpiSetSpeakerAttenuation((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_SOFT_AGC_ACTIVE:
            result = AudioSpiSetSoftAgcActive((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_SOFT_AGC_TARGET_MOD_INDEX:
            result = AudioSpiSetSoftAgcTargetModIndex((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_SOFT_AGC_MAX_GAIN:
            result = AudioSpiSetSoftAgcMaxGain((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_ACTIVE:
            result = AudioSpiSetHardAgcActive((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_TARGET_GAIN:
            result = AudioSpiSetHardAgcTargetGain((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_ATTACK_TIME:
            result = AudioSpiSetHardAgcAttackTime((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_DECAY_TIME:
            result = AudioSpiSetHardAgcDecayTime((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_MAX_GAIN:
            result = AudioSpiSetHardAgcMaxGain((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_NOISE_HYSTERESIS_LEVEL:
            result = AudioSpiSetHardAgcNoiseHysteresis((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        case OFFSET_HARD_AGC_NOISE_THRESHOLD:
            result = AudioSpiSetHardAgcNoiseThreshold((AudioModeSetIndex)mode_set, gain_value);
            break;
            
        default:
            logMsg("AudioConfigHandler: Invalid param_offset=%d\n", param_offset, 0, 0, 0, 0, 0);
            return ERROR;
    }
    
    if (result != OK) {
        logMsg("AudioConfigHandler: SPI command failed, offset=%d, mode=%d, value=%d\n", 
               param_offset, mode_set, gain_value, 0, 0);
    }
    
    return result;
}

/* ========================================================================= */
/* Public Handler Functions                                                   */
/* ========================================================================= */

/**
 * @brief Handle STATUS command - return current parameter values.
 *
 * Reads current configuration from flash and returns all 15 parameters
 * for all 6 mode sets (90 bytes total).
 */
int AudioCmdHandleStatus(int client_fd, uint8_t *msg)
{
    AudioResponse response;
    AudioConfigFileData config_data;
    int result;
    int i;
    
    memset(&response, 0, sizeof(AudioResponse));
    response.cmd_type = AUDIO_CMD_STATUS | AUDIO_CMD_RESPONSE;
    
    /* Read configuration from flash */
    result = audio_flash_read_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: STATUS - flash read failed, result=%d\n", result, 0, 0, 0, 0);
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }
    
    /* Copy gain blocks for all 6 mode sets to response data */
    /* data[0..89]: [mode0_param0..mode0_param14]...[mode5_param14] */
    for (i = 0; i < AUDIO_MODE_CNT; i++) {
        memcpy(&response.data[i * AUDIO_GAIN_BLOCK_SIZE],
               (const uint8_t *)&config_data.sets[i],
               AUDIO_GAIN_BLOCK_SIZE);
    }

    /* data[90..119]: version string stored in flash (null-padded, 30 bytes) */
    memcpy(&response.data[AUDIO_MODE_CNT * AUDIO_GAIN_BLOCK_SIZE],
           config_data.version,
           AUDIO_CFG_VERSION_SIZE);

    response.status = AUDIO_STATUS_OK;
    response.error_code = 0;

    logMsg("AudioConfigHandler: STATUS - returned %d bytes for %d mode sets\n",
           AUDIO_MODE_CNT * AUDIO_GAIN_BLOCK_SIZE, AUDIO_MODE_CNT, 0, 0, 0);
    
    return send_response(client_fd, &response);
}

/**
 * @brief Handle SET_PARAM command - set a single parameter value.
 *
 * Sets one parameter value for a specific mode set, updates flash,
 * and dispatches SPI command to DSP.
 */
int AudioCmdHandleSetParam(int client_fd, uint8_t *msg)
{
    AudioRequest *request = (AudioRequest *)msg;
    AudioResponse response;
    SetParamPayload *payload;
    AudioConfigFileData config_data;
    int result;
    
    memset(&response, 0, sizeof(AudioResponse));
    response.cmd_type = AUDIO_CMD_SET_PARAM | AUDIO_CMD_RESPONSE;
    
    /* Parse payload */
    payload = (SetParamPayload *)request->payload;
    
    /* Validate mode set index (0-5) */
    if (payload->mode_set >= AUDIO_MODE_CNT) {
        logMsg("AudioConfigHandler: SET_PARAM - invalid mode_set=%d\n", payload->mode_set, 0, 0, 0, 0);
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_INVALID_MODE;
        return send_response(client_fd, &response);
    }
    
    /* Validate parameter offset (0-14) */
    if (payload->param_offset >= AUDIO_GAIN_PARAM_CNT) {
        logMsg("AudioConfigHandler: SET_PARAM - invalid param_offset=%d\n", payload->param_offset, 0, 0, 0, 0);
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_INVALID_PARAM;
        return send_response(client_fd, &response);
    }
    
    /* Validate gain value range */
    if (!validate_gain_value(payload->param_offset, payload->gain_value)) {
        logMsg("AudioConfigHandler: SET_PARAM - invalid gain_value=%d for param=%d\n", 
               payload->gain_value, payload->param_offset, 0, 0, 0);
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_INVALID_VALUE;
        return send_response(client_fd, &response);
    }
    
    /* Read current configuration from flash */
    result = audio_flash_read_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_PARAM - flash read failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }
    
    /* Update the specific parameter in the gain block */
    ((uint8_t *)&config_data.sets[payload->mode_set])[payload->param_offset] = payload->gain_value;

    /* Recalculate CRC32 over the covered region before writing */
    config_data.crc32 = AudioCfgCrc32Calculate(
        (const uint8_t *)&config_data, AUDIO_CFG_CRC32_COVERED_SIZE);

    /* Write updated configuration back to flash */
    result = audio_flash_write_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_PARAM - flash write failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }
    
    /* Dispatch SPI command to DSP (non-blocking) */
    result = dispatch_spi_command(payload->param_offset, payload->mode_set, payload->gain_value);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_PARAM - SPI dispatch failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_SPI_ERROR;
        return send_response(client_fd, &response);
    }
    
    response.status = AUDIO_STATUS_OK;
    response.error_code = 0;
    
    logMsg("AudioConfigHandler: SET_PARAM - mode=%d, param=%d, value=%d OK\n", 
           payload->mode_set, payload->param_offset, payload->gain_value, 0, 0);
    
    return send_response(client_fd, &response);
}

/**
 * @brief Handle SET_GAIN_BLOCK command - set all 15 parameters for a mode.
 *
 * Sets the complete gain block (all 15 parameters) for a specific mode set,
 * updates flash, and dispatches all 15 SPI commands to DSP.
 */
int AudioCmdHandleSetGainBlock(int client_fd, uint8_t *msg)
{
    AudioRequest *request = (AudioRequest *)msg;
    AudioResponse response;
    SetGainBlockPayload *payload;
    AudioConfigFileData config_data;
    int result;
    int i;
    
    memset(&response, 0, sizeof(AudioResponse));
    response.cmd_type = AUDIO_CMD_SET_GAIN_BLOCK | AUDIO_CMD_RESPONSE;
    
    /* Parse payload */
    payload = (SetGainBlockPayload *)request->payload;
    
    /* Validate mode set index (0-5) */
    if (payload->mode_set >= AUDIO_MODE_CNT) {
        logMsg("AudioConfigHandler: SET_GAIN_BLOCK - invalid mode_set=%d\n", payload->mode_set, 0, 0, 0, 0);
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_INVALID_MODE;
        return send_response(client_fd, &response);
    }
    
    /* Validate all 15 gain values */
    for (i = 0; i < AUDIO_GAIN_BLOCK_SIZE; i++) {
        if (!validate_gain_value(i, payload->gain_block[i])) {
            logMsg("AudioConfigHandler: SET_GAIN_BLOCK - invalid value at param=%d, value=%d\n", 
                   i, payload->gain_block[i], 0, 0, 0);
            response.status = AUDIO_STATUS_ERROR;
            response.error_code = AUDIO_STATUS_INVALID_VALUE;
            return send_response(client_fd, &response);
        }
    }
    
    /* Read current configuration from flash */
    result = audio_flash_read_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_GAIN_BLOCK - flash read failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }
    
    /* Update the entire gain block for the specified mode set */
    memcpy((uint8_t *)&config_data.sets[payload->mode_set],
           payload->gain_block,
           AUDIO_GAIN_BLOCK_SIZE);

    /* Recalculate CRC32 over the covered region before writing */
    config_data.crc32 = AudioCfgCrc32Calculate(
        (const uint8_t *)&config_data, AUDIO_CFG_CRC32_COVERED_SIZE);

    /* Write updated configuration back to flash */
    result = audio_flash_write_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_GAIN_BLOCK - flash write failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }
    
    /* Dispatch all 15 SPI commands to DSP (non-blocking) */
    for (i = 0; i < AUDIO_GAIN_BLOCK_SIZE; i++) {
        result = dispatch_spi_command(i, payload->mode_set, payload->gain_block[i]);
        if (result != OK) {
            logMsg("AudioConfigHandler: SET_GAIN_BLOCK - SPI dispatch failed at param=%d\n", i, 0, 0, 0, 0);
            response.status = AUDIO_STATUS_ERROR;
            response.error_code = AUDIO_STATUS_SPI_ERROR;
            return send_response(client_fd, &response);
        }
    }
    
    response.status = AUDIO_STATUS_OK;
    response.error_code = 0;
    
    logMsg("AudioConfigHandler: SET_GAIN_BLOCK - mode=%d, 15 params OK\n", payload->mode_set, 0, 0, 0, 0);
    
    return send_response(client_fd, &response);
}

/**
 * @brief Handle SET_META command - persist header_id and version to flash.
 */
int AudioCmdHandleSetMeta(int client_fd, uint8_t *msg)
{
    AudioRequest *request = (AudioRequest *)msg;
    AudioResponse response;
    SetMetaPayload *payload;
    AudioConfigFileData config_data;
    int result;

    memset(&response, 0, sizeof(AudioResponse));
    response.cmd_type = AUDIO_CMD_SET_META | AUDIO_CMD_RESPONSE;

    payload = (SetMetaPayload *)request->payload;

    /* Read current configuration from flash */
    result = audio_flash_read_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_META - flash read failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }

    /* Update metadata fields */
    memcpy(config_data.header_id, payload->header_id, AUDIO_CFG_HEADER_ID_SIZE);
    memcpy(config_data.version,   payload->version,   AUDIO_CFG_VERSION_SIZE);

    /* Recalculate CRC32 over the covered region */
    config_data.crc32 = AudioCfgCrc32Calculate(
        (const uint8_t *)&config_data, AUDIO_CFG_CRC32_COVERED_SIZE);

    /* Write updated configuration back to flash */
    result = audio_flash_write_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: SET_META - flash write failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }

    response.status = AUDIO_STATUS_OK;
    response.error_code = 0;

    logMsg("AudioConfigHandler: SET_META - header_id and version saved OK\n");

    return send_response(client_fd, &response);
}

/**
 * @brief Handle VERIFY command - verify CRC32 of stored configuration.
 *
 * Reads configuration from flash, computes CRC32, and compares with
 * stored value. Returns verification result.
 */
int AudioCmdHandleVerify(int client_fd, uint8_t *msg)
{
    AudioResponse response;
    AudioConfigFileData config_data;
    uint32_t crc_stored;
    uint32_t crc_computed;
    uint8_t crc_match;
    int result;
    
    memset(&response, 0, sizeof(AudioResponse));
    response.cmd_type = AUDIO_CMD_VERIFY | AUDIO_CMD_RESPONSE;
    
    /* Read configuration from flash */
    result = audio_flash_read_config((uint8_t *)&config_data, AUDIO_CFG_FILE_SIZE);
    if (result != OK) {
        logMsg("AudioConfigHandler: VERIFY - flash read failed\n");
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_FLASH_ERROR;
        return send_response(client_fd, &response);
    }
    
    /* Get stored CRC32 */
    crc_stored = config_data.crc32;
    
    /* Compute CRC32 over covered region (141 bytes: header + gain sets) */
    crc_computed = AudioCfgCrc32Calculate((uint8_t *)&config_data, AUDIO_CFG_CRC32_COVERED_SIZE);
    
    /* Compare CRCs */
    crc_match = (crc_stored == crc_computed) ? 1 : 0;
    
    /* Pack response data: [crc_stored:4][crc_computed:4][match:1] */
    memcpy(&response.data[0], &crc_stored, 4);
    memcpy(&response.data[4], &crc_computed, 4);
    response.data[8] = crc_match;
    
    if (crc_match) {
        response.status = AUDIO_STATUS_OK;
        response.error_code = 0;
        logMsg("AudioConfigHandler: VERIFY - CRC32 OK (0x%08X)\n", crc_computed, 0, 0, 0, 0);
    } else {
        response.status = AUDIO_STATUS_ERROR;
        response.error_code = AUDIO_STATUS_CRC_ERROR;
        logMsg("AudioConfigHandler: VERIFY - CRC32 MISMATCH (stored=0x%08X, computed=0x%08X)\n", 
               crc_stored, crc_computed, 0, 0, 0);
    }
    
    return send_response(client_fd, &response);
}
