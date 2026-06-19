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
 * @file AudioFlashMgr.c
 * @brief Flash read/write/verify implementation for Audio DSP configuration.
 *
 * Provides audio-config-specific wrappers around the existing
 * DA2_9651ParameterBlock read/write APIs with bounds checking,
 * buffer validation, and CRC32 verification.
 *
 * Reference: Uses same patterns as ReadDA2_9651ParameterBlock() and
 *            WriteDA2_9651ParameterBlock() in Common/Param/Parameter.c
 */

#include "AudioConf/AudioFlashMgr.h"
#include "AudioConf/AudioParamDef.h"
#include "ParameterExp.h"
#include "e2map.h"
#include <string.h>

/* ========================================================================= */
/* Internal helper: Compute CRC32 over the covered region of config data     */
/* ========================================================================= */

/**
 * @brief Compute the expected CRC32 for a loaded audio config buffer.
 *
 * CRC32 covers bytes from header_id through all config sets
 * (AUDIO_CFG_CRC32_COVERED_SIZE = 141 bytes).
 *
 * @param buffer  Pointer to audio config data in memory.
 * @return Computed CRC32 value.
 */
static uint32_t _audio_flash_compute_crc32(const uint8_t *buffer)
{
    return AudioCfgCrc32Calculate(buffer, AUDIO_CFG_CRC32_COVERED_SIZE);
}

/* ========================================================================= */
/* Public API: Read config from flash                                        */
/* ========================================================================= */

int audio_flash_read_config(uint8_t *dest, uint16_t size)
{
    uint16_t read_size = (size == 0) ? AUDIO_CFG_FILE_SIZE : size;

    /* Bounds check: offset + size must not exceed DA2 block length */
    if ((AUDIO_CFG_FLASH_OFFSET + read_size) > DA2_9651_BLOCK_LEN) {
        logMsg("AUDIO_FLASH ERROR: read request exceeds DA2 block boundary (off=%u, size=%u)!\n",
               AUDIO_CFG_FLASH_OFFSET, read_size, 0, 0, 0, 0);
        return ERROR;
    }

    /* Null pointer check */
    if (dest == NULL) {
        logMsg("AUDIO_FLASH ERROR: read_config called with null pointer!\n",
               0, 0, 0, 0, 0, 0);
        return ERROR;
    }

    /* Call existing DA2 parameter block read */
    if (ReadDA2_9651ParameterBlock(AUDIO_CFG_FLASH_OFFSET, dest, read_size) != OK) {
        logMsg("AUDIO_FLASH ERROR: failed to read audio config from DA2 flash!\n",
               0, 0, 0, 0, 0, 0);
        return ERROR;
    }

    return OK;
}

/* ========================================================================= */
/* Public API: Write config to flash                                         */
/* ========================================================================= */

int audio_flash_write_config(const uint8_t *src, uint16_t size)
{
    uint16_t write_size = (size == 0) ? AUDIO_CFG_FILE_SIZE : size;

    /* Bounds check: offset + size must not exceed DA2 block length */
    if ((AUDIO_CFG_FLASH_OFFSET + write_size) > DA2_9651_BLOCK_LEN) {
        logMsg("AUDIO_FLASH ERROR: write request exceeds DA2 block boundary (off=%u, size=%u)!\n",
               AUDIO_CFG_FLASH_OFFSET, write_size, 0, 0, 0, 0);
        return ERROR;
    }

    /* Null pointer check */
    if (src == NULL) {
        logMsg("AUDIO_FLASH ERROR: write_config called with null pointer!\n",
               0, 0, 0, 0, 0, 0);
        return ERROR;
    }

    /* Call existing DA2 parameter block write */
    if (WriteDA2_9651ParameterBlock(AUDIO_CFG_FLASH_OFFSET, (uint8_t *)src, write_size) != OK) {
        logMsg("AUDIO_FLASH ERROR: failed to write audio config to DA2 flash!\n",
               0, 0, 0, 0, 0, 0);
        return ERROR;
    }

    return OK;
}

/* ========================================================================= */
/* Public API: Verify CRC32 consistency                                      */
/* ========================================================================= */

int audio_flash_verify_config(void)
{
    uint8_t buffer[AUDIO_CFG_FILE_SIZE];
    uint32_t stored_crc32;
    uint32_t computed_crc32;

    /* Read the full config from flash */
    if (audio_flash_read_config(buffer, AUDIO_CFG_FILE_SIZE) != OK) {
        logMsg("AUDIO_FLASH ERROR: verify_config failed to read from flash!\n",
               0, 0, 0, 0, 0, 0);
        return ERROR;
    }

    /* Extract stored CRC32 (at offset 141, big-endian on PowerPC) */
    stored_crc32 = ((uint32_t)buffer[141] << 24)
                 | ((uint32_t)buffer[142] << 16)
                 | ((uint32_t)buffer[143] << 8)
                 | (uint32_t)buffer[144];

    /* Compute expected CRC32 over the covered region */
    computed_crc32 = _audio_flash_compute_crc32(buffer);

    /* Compare */
    if (stored_crc32 != computed_crc32) {
        logMsg("AUDIO_FLASH ERROR: CRC32 mismatch! stored=0x%08X, computed=0x%08X!\n",
               stored_crc32, computed_crc32, 0, 0, 0, 0);
        return ERROR;
    }

    return OK;
}
