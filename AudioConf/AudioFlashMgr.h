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
 * @file AudioFlashMgr.h
 * @brief Flash read/write/verify functions for Audio DSP configuration storage.
 *
 * Wraps existing DA2_9651ParameterBlock APIs to provide config-area-specific
 * read, write, and CRC32 verification for the 300-byte audio configuration
 * data stored at the start of the DA2_9651_BLOCK.
 *
 * Flash Layout within DA2_9651_BLOCK (2048 bytes):
 *   Offset 0:   Audio Config (300 bytes)
 *   Offset 300: Available for future audio-related data (~1748 bytes)
 */

#ifndef AUDIOFLASHEMGR_H
#define AUDIOFLASHEMGR_H

#include <stdint.h>
#include "e2map.h"

/* ========================================================================= */
/* DA2_9651_BLOCK offset definitions (from e2map.h, duplicated for safety)    */
/* ========================================================================= */

/** DA2_9651_BLOCK size in bytes (0x500 = 1280). NOTE: verify against e2map.h hardware definition. */
#ifndef DA2_9651_BLOCK_LEN
#define DA2_9651_BLOCK_LEN    0x500
#endif

/** Absolute flash location of DA2_9651_BLOCK start. */
#ifndef DA2_9651_BLOCK_LOC
#define DA2_9651_BLOCK_LOC    0x00000
#endif

/* ========================================================================= */
/* Audio config constants                                                     */
/* ========================================================================= */

/** Size of audio configuration file (must match AudioParamDef.h). */
#define AUDIO_CFG_FILE_SIZE     300U

/** Audio config offset from start of DA2_9651_BLOCK (bytes). */
#define AUDIO_CFG_FLASH_OFFSET  0U

/** CRC32 covers bytes 0-160: header_id(40) + version(30) + set_count(1) + sets(90) = 161 bytes. */
#define AUDIO_CFG_CRC32_COVERED_SIZE  161U

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

/**
 * @brief Read audio configuration from DA2 flash.
 *
 * Uses ReadDA2_9651ParameterBlock() internally. Reads exactly AUDIO_CFG_FILE_SIZE
 * bytes from DA2_9651_BLOCK starting at AUDIO_CFG_FLASH_OFFSET into the
 * destination buffer.
 *
 * @param[out] dest  Destination buffer (must be >= AUDIO_CFG_FILE_SIZE bytes).
 * @param[in]  size   Number of bytes to read (0 = full AUDIO_CFG_FILE_SIZE).
 * @return OK (0) on success, ERROR (-1) on failure.
 */
int audio_flash_read_config(uint8_t *dest, uint16_t size);

/**
 * @brief Write audio configuration to DA2 flash.
 *
 * Uses WriteDA2_9651ParameterBlock() internally. Writes exactly the specified
 * number of bytes from the source buffer into DA2_9651_BLOCK starting at
 * AUDIO_CFG_FLASH_OFFSET.
 *
 * @param[in]  src   Source buffer (>= size bytes, ideally AUDIO_CFG_FILE_SIZE).
 * @param[in]  size   Number of bytes to write (0 = full AUDIO_CFG_FILE_SIZE).
 * @return OK (0) on success, ERROR (-1) on failure.
 */
int audio_flash_write_config(const uint8_t *src, uint16_t size);

/**
 * @brief Verify audio configuration CRC32 consistency.
 *
 * Reads AUDIO_CFG_FILE_SIZE bytes from flash, validates the stored CRC32
 * against a recomputed value over the covered region (bytes 0 through 140),
 * and returns the result.
 *
 * @return OK (0) if CRC32 matches, ERROR (-1) if mismatch or read failure.
 */
int audio_flash_verify_config(void);

#endif /* AUDIOFLASHEMGR_H */
