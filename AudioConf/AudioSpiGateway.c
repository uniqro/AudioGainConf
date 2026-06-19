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
 * @file AudioSpiGateway.c
 * @brief Audio DSP (C5416) SPI command implementation.
 *
 * Implements 15 SPI command functions for audio gain configuration
 * of the C5416 DSP. Each function sends a 4-byte SPI packet:
 *   [CMD_BYTE] [MODE_BYTE] [GAIN_BYTE] [CHECKSUM]
 *
 * SPI Packet Format:
 *   Byte 0: Command ID (0xC0-0xCE, defined in AudioSpiGateway.h)
 *   Byte 1: Mode set index (AudioModeSetIndex: 0-5)
 *   Byte 2: Gain value (parameter-specific range)
 *   Byte 3: Checksum = (256 - sum_of_bytes_0_to_2) & 0xFF
 *           Implementation uses two's complement: ~(sum & 0xFF) + 1
 *
 * Dispatch Mechanism:
 *   All functions use the existing SPI driver infrastructure from spi.c:
 *   - addCommand(UINT8)       - Queue a byte to the transmit buffer
 *   - addingCommands          - Batch flag to prevent trigger send
 *   - globalModemTxBuf        - Transmit buffer (defined in spi.c)
 *   - globalModemTxBuf_head   - Current write position in buffer
 *   - globalModemTxBuf_head_shadow - Saved head for atomic batch ops
 *
 * The dispatch is non-blocking: commands are queued for later transmission
 * by the existing SPI link layer. No new locks, queues, or mutexes needed.
 *
 * SPI Command Range: 0xC0-0xCE (15 commands)
 *   Does NOT conflict with HS_COMMAND_* range (0x10-0x86)
 */

#include "AudioConf/AudioSpiGateway.h"
#include "96SK/DevreAnahtarlama/spi/spi_lnk.h"

/* ========================================================================= */
/* VxWorks type definitions (fallback if not provided by BSP)              */
/* ========================================================================= */

#ifndef UINT8
typedef unsigned char   UINT8;
#endif
#ifndef UINT16
typedef unsigned short  UINT16;
#endif
#ifndef UINT32
typedef unsigned int    UINT32;
#endif
#ifndef BOOL
typedef int             BOOL;
#endif

/* ========================================================================= */
/* External declarations (defined in spi_lnk.c, accessed via spi.c)         */
/* ========================================================================= */

/** @brief Queue a single byte to the SPI transmit buffer. */
extern void addCommand(UINT8 command);

/** @brief Flag: TRUE while batching commands in the transmit buffer. */
extern BOOL addingCommands;

/** @brief Saved transmit buffer head position for batch operations. */
extern UINT16 globalModemTxBuf_head_shadow;

/** @brief Transmit buffer (defined in spi_lnk.c, structured in spi_lnk.h). */
extern ModemTxBufType globalModemTxBuf;

/* ========================================================================= */
/* Helper: Dispatch a 3-byte command + checksum to SPI                      */
/* ========================================================================= */

/**
 * @brief Dispatch a 3-byte SPI command with checksum to the DSP.
 *
 * Follows the exact pattern used in spi.c for multi-byte commands
 * (e.g., d_SetComMode, d_SetMode, h_SetEarpieceAtt, SetBackLight).
 *
 * Packet format: [CMD][MODE][GAIN][CHECKSUM]
 * Checksum: (256 - (CMD + MODE + GAIN)) & 0xFF
 *           Equivalent to: (~((CMD + MODE + GAIN) & 0xFF) + 1) & 0xFF
 *           (two's complement negation of the byte-wise sum)
 *
 * Batch dispatch via addCommand() with head shadow save/restore:
 *   1. Save current head position
 *   2. Queue CMD, MODE, GAIN bytes via addCommand()
 *   3. Compute and queue checksum byte
 *   4. Restore head position (transmit trigger handles the batch)
 *
 * @param cmdByte    SPI command byte (0xC0-0xCE)
 * @param modeSet    Audio mode set index (0-5, cast to uint8_t)
 * @param gainValue  Gain/value byte (parameter-specific)
 */
static void audioSpiDispatch(uint8_t cmdByte, uint8_t modeSet, uint8_t gainValue)
{
    /*
     * Checksum calculation (matches spi.c: h_SetEarpieceAtt, d_SetComMode, etc.):
     *   cksum = CMD+MODE+GAIN, then checksum = (256 - cksum) & 0xFF
     * This is the standard two's negation used in the existing SPI driver.
     */
    uint8_t cksum   = (uint8_t)(cmdByte + modeSet + gainValue);
    uint8_t checksum = (uint8_t)(256 - cksum);

    /*
     * Batch-dispatch pattern (mirrors spi.c: d_SetComMode, SetBackLight, etc.).
     * addingCommands=TRUE prevents early transmission; head shadow ensures
     * the buffer position resets after all 4 bytes are queued.
     */
    addingCommands = TRUE;
    globalModemTxBuf_head_shadow = globalModemTxBuf.head;
    addCommand(cmdByte);
    addCommand(modeSet);
    addCommand(gainValue);
    addCommand(checksum);
    globalModemTxBuf.head = globalModemTxBuf_head_shadow;
    addingCommands = FALSE;
}

/* ========================================================================= */
/* 1. Input Level Control Gain (0xC0, x1, 0-8)                              */
/* ========================================================================= */

/**
 * @brief Set Input Level Control Gain for a mode set.
 *
 * Command: [0xC0][mode_set][gain_index][checksum]
 * Gain index range: 0 (0dB) to 8 (-12dB) in 1.5dB steps.
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Gain index (db0_gain=0 .. db_minus_12_gain=8)
 * @return 0 on success
 */
int AudioSpiSetInputLevel(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_INPUT_LEVEL, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 2. ADC PGA Gain (0xC1, x2, 0-119 = 0-59.5dB)                             */
/* ========================================================================= */

/**
 * @brief Set ADC PGA Gain for a mode set.
 *
 * Command: [0xC1][mode_set][gain_x2][checksum]
 * Gain range: 0 (0dB) to 119 (59.5dB) in 0.5dB steps (x2 scaled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  PGA gain (x2 scaled: 0-119)
 * @return 0 on success
 */
int AudioSpiSetAdcPga(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_ADC_PGA, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 3. Sidetone Attenuation Gain (0xC2, x2, 0-120 = 0--60dB)                 */
/* ========================================================================= */

/**
 * @brief Set Sidetone Attenuation Gain for a mode set.
 *
 * Command: [0xC2][mode_set][att_x2][checksum]
 * Attenuation range: 0 (0dB) to 120 (-60dB) in 0.5dB steps (x2 scaled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Attenuation value (x2 scaled: 0-120)
 * @return 0 on success
 */
int AudioSpiSetSidetoneAttenuation(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_SIDETONE, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 4. Output Level Control Gain (0xC3, x1, 0-9)                              */
/* ========================================================================= */

/**
 * @brief Set Output Level Control Gain for a mode set.
 *
 * Command: [0xC3][mode_set][gain_index][checksum]
 * Gain index range: 0 (0dB) to 9 (9dB) in 1dB steps.
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Gain index (0-9)
 * @return 0 on success
 */
int AudioSpiSetOutputLevel(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_OUTPUT_LEVEL, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 5. Speaker Attenuation Gain (0xC4, x2, 0-127 = 0-63.5dB)                 */
/* ========================================================================= */

/**
 * @brief Set Speaker Attenuation Gain for a mode set.
 *
 * Command: [0xC4][mode_set][att_x2][checksum]
 * Attenuation range: 0 (0dB) to 127 (63.5dB) in 0.5dB steps (x2 scaled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Attenuation value (x2 scaled: 0-127)
 * @return 0 on success
 */
int AudioSpiSetSpeakerAttenuation(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_SPEAKER_ATT, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 6. Soft AGC Active (0xC5, x1, 0-1)                                        */
/* ========================================================================= */

/**
 * @brief Enable/Disable Soft AGC for a mode set.
 *
 * Command: [0xC5][mode_set][active][checksum]
 * gainValue: 0 = PASSIVE (AGC bypassed), 1 = ACTIVE (AGC enabled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  0=PASSIVE, 1=ACTIVE
 * @return 0 on success
 */
int AudioSpiSetSoftAgcActive(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_SOFT_AGC_ACT, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 7. Soft AGC Target Modulation Index (0xC6, x1, 0-100)                     */
/* ========================================================================= */

/**
 * @brief Set Soft AGC Target Modulation Index for a mode set.
 *
 * Command: [0xC6][mode_set][mod_index][checksum]
 * Range: 0% to 100% in 1% steps.
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Target modulation index (0-100)
 * @return 0 on success
 */
int AudioSpiSetSoftAgcTargetModIndex(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_SOFT_AGC_MOD, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 8. Soft AGC Max Gain (0xC7, x1, 0-30)                                     */
/* ========================================================================= */

/**
 * @brief Set Soft AGC Max Gain for a mode set.
 *
 * Command: [0xC7][mode_set][max_gain][checksum]
 * Range: 0dB to 30dB in 1dB steps.
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Max gain (0-30 dB)
 * @return 0 on success
 */
int AudioSpiSetSoftAgcMaxGain(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_SOFT_AGC_MAX, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 9. Hard AGC Active (0xC8, x1, 0-1)                                        */
/* ========================================================================= */

/**
 * @brief Enable/Disable Hard AGC for a mode set.
 *
 * Command: [0xC8][mode_set][active][checksum]
 * gainValue: 0 = PASSIVE (AGC bypassed), 1 = ACTIVE (AGC enabled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  0=PASSIVE, 1=ACTIVE
 * @return 0 on success
 */
int AudioSpiSetHardAgcActive(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_ACT, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 10. Hard AGC Target Gain (0xC9, x1, 0-7)                                  */
/* ========================================================================= */

/**
 * @brief Set Hard AGC Target Gain for a mode set.
 *
 * Command: [0xC9][mode_set][target][checksum]
 * Gain index range (8 discrete levels):
 *   0=-5.5dB, 1=-8dB, 2=-10dB, 3=-12dB,
 *   4=-14dB, 5=-17dB, 6=-20dB, 7=-24dB
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Target gain index (0-7)
 * @return 0 on success
 */
int AudioSpiSetHardAgcTargetGain(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_TGT, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 11. Hard AGC Attack Time (0xCA, x1, 0-3)                                  */
/* ========================================================================= */

/**
 * @brief Set Hard AGC Attack Time for a mode set.
 *
 * Command: [0xCA][mode_set][attack][checksum]
 * Attack time levels (4 discrete):
 *   0=8ms, 1=11ms, 2=16ms, 3=20ms
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Attack time index (0-3)
 * @return 0 on success
 */
int AudioSpiSetHardAgcAttackTime(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_ATK, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 12. Hard AGC Decay Time (0xCB, x1, 0-3)                                   */
/* ========================================================================= */

/**
 * @brief Set Hard AGC Decay Time for a mode set.
 *
 * Command: [0xCB][mode_set][decay][checksum]
 * Decay time levels (4 discrete):
 *   0=100ms, 1=200ms, 2=400ms, 3=500ms
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Decay time index (0-3)
 * @return 0 on success
 */
int AudioSpiSetHardAgcDecayTime(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_DEC, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 13. Hard AGC Max Gain (0xCC, x2, 0-119 = 0-59.5dB)                        */
/* ========================================================================= */

/**
 * @brief Set Hard AGC Max Gain for a mode set.
 *
 * Command: [0xCC][mode_set][max_x2][checksum]
 * Range: 0 (0dB) to 119 (59.5dB) in 0.5dB steps (x2 scaled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Max gain (x2 scaled: 0-119)
 * @return 0 on success
 */
int AudioSpiSetHardAgcMaxGain(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_MAX, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 14. Hard AGC Noise Hysteresis (0xCD, x1, 0-3)                             */
/* ========================================================================= */

/**
 * @brief Set Hard AGC Noise Hysteresis Level for a mode set.
 *
 * Command: [0xCD][mode_set][hysteresis][checksum]
 * Hysteresis levels (4 discrete):
 *   0=disabled, 1=1dB, 2=2dB, 3=3dB
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Hysteresis index (0-3)
 * @return 0 on success
 */
int AudioSpiSetHardAgcNoiseHysteresis(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_HYS, (uint8_t)modeSet, gainValue);
    return 0;
}

/* ========================================================================= */
/* 15. Hard AGC Noise Threshold (0xCE, x1, 0-31)                             */
/* ========================================================================= */

/**
 * @brief Set Hard AGC Noise Threshold for a mode set.
 *
 * Command: [0xCE][mode_set][threshold][checksum]
 * Threshold levels (32 values, 5-bit):
 *   0=disabled, 1=-30dB, 2=-32dB, ..., 31=-90dB (2dB/step)
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Noise threshold index (0-31)
 * @return 0 on success
 */
int AudioSpiSetHardAgcNoiseThreshold(AudioModeSetIndex modeSet, uint8_t gainValue)
{
    audioSpiDispatch(SPI_CMD_HARD_AGC_NT, (uint8_t)modeSet, gainValue);
    return 0;
}
