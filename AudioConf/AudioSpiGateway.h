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
 * @file AudioSpiGateway.h
 * @brief Audio DSP (C5416) SPI command definitions and function prototypes.
 *
 * Provides 15 SPI command functions to configure audio gain parameters
 * on the C5416 DSP via the existing SPI handset interface.
 *
 * SPI Command Format (3 data bytes + checksum):
 *   [CMD_BYTE] [MODE_BYTE] [GAIN_BYTE] [CHECKSUM]
 *   CMD_BYTE  : SPI_CMD_* constant (0xC0-0xCE)
 *   MODE_BYTE : AudioModeSetIndex value (AM_MODE=0 .. SK2_MODE=5)
 *   GAIN_BYTE : Parameter gain value (range depends on parameter type)
 *   CHECKSUM  : (256 - (CMD + MODE + GAIN)) & 0xFF  (two's complement)
 *
 * All functions use the existing SPI driver infrastructure:
 *   addCommand()        - Queue a single byte to the transmit buffer
 *   globalModemTxBuf    - Shared transmit buffer managed by spi_lnk.c
 *   addingCommands      - Flag to batch command queueing (non-blocking)
 *   globalModemTxBuf_head_shadow - Head position backup for atomic batch ops
 *
 * SPI command range 0xC0-0xCE is reserved for Audio DSP commands only.
 * This range does NOT conflict with existing HS_COMMAND_* values (0x10-0x86).
 */

#ifndef AUDIOSPIGATEWAY_H
#define AUDIOSPIGATEWAY_H

#include <stdint.h>
#include "AudioConf/AudioParamDef.h"

/* ========================================================================= */
/* SPI Command Byte Definitions (0xC0-0xCE) - Audio DSP (C5416) only        */
/* ========================================================================= */

/* Input/Output Level Controls */
#define SPI_CMD_INPUT_LEVEL  0xC0  /* Input level control gain: 0-8 (x1)          */
#define SPI_CMD_ADC_PGA      0xC1  /* ADC PGA gain: 0-119 (x2, 0-59.5dB)         */
#define SPI_CMD_SIDETONE     0xC2  /* Sidetone attenuation: 0-120 (x2, 0--60dB)  */
#define SPI_CMD_OUTPUT_LEVEL 0xC3  /* Output level control gain: 0-9 (x1)         */
#define SPI_CMD_SPEAKER_ATT  0xC4  /* Speaker attenuation: 0-127 (x2, 0-63.5dB)   */

/* Soft AGC Controls */
#define SPI_CMD_SOFT_AGC_ACT 0xC5  /* Soft AGC active: 0=PASSIVE, 1=ACTIVE       */
#define SPI_CMD_SOFT_AGC_MOD 0xC6  /* Soft AGC target mod index: 0-100 (%)       */
#define SPI_CMD_SOFT_AGC_MAX 0xC7  /* Soft AGC max gain: 0-30 (dB)               */

/* Hard AGC Controls */
#define SPI_CMD_HARD_AGC_ACT 0xC8  /* Hard AGC active: 0=PASSIVE, 1=ACTIVE       */
#define SPI_CMD_HARD_AGC_TGT 0xC9  /* Hard AGC target gain: 0-7 (8 discrete)      */
#define SPI_CMD_HARD_AGC_ATK 0xCA  /* Hard AGC attack time: 0-3 (4 levels)        */
#define SPI_CMD_HARD_AGC_DEC 0xCB  /* Hard AGC decay time: 0-3 (4 levels)         */
#define SPI_CMD_HARD_AGC_MAX 0xCC  /* Hard AGC max gain: 0-119 (x2, 0-59.5dB)    */
#define SPI_CMD_HARD_AGC_HYS 0xCD  /* Hard AGC noise hysteresis: 0-3 (4 levels)   */
#define SPI_CMD_HARD_AGC_NT  0xCE  /* Hard AGC noise threshold: 0-31 (5-bit)      */

/* Command count for validation */
#define SPI_AUDIO_CMD_COUNT  15U

/* ========================================================================= */
/* Function prototypes                                                      */
/* ========================================================================= */

/**
 * @brief Set Input Level Control Gain for a specific mode set.
 *
 * Sends SPI command: [0xC0][mode_set][gain_value][checksum]
 *
 * @param modeSet    Audio mode set index (AM_MODE, FM_MODE, etc.)
 * @param gainValue  Gain level index (x1, range 0-8)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetInputLevel(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set ADC PGA Gain for a specific mode set.
 *
 * Sends SPI command: [0xC1][mode_set][gain_value][checksum]
 * Range: 0-119 (x2 scaled = 0dB to 59.5dB in 0.5dB steps).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  PGA gain value (x2 scaled)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetAdcPga(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Sidetone Attenuation Gain for a specific mode set.
 *
 * Sends SPI command: [0xC2][mode_set][gain_value][checksum]
 * Range: 0-120 (x2 scaled = 0dB to -60dB in 0.5dB steps).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Attenuation value (x2 scaled)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetSidetoneAttenuation(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Output Level Control Gain for a specific mode set.
 *
 * Sends SPI command: [0xC3][mode_set][gain_value][checksum]
 * Range: 0-9 (x1).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Output level index (0-9, x1)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetOutputLevel(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Speaker Attenuation Gain for a specific mode set.
 *
 * Sends SPI command: [0xC4][mode_set][gain_value][checksum]
 * Range: 0-127 (x2 scaled = 0dB to 63.5dB in 0.5dB steps).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Speaker attenuation value (x2 scaled)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetSpeakerAttenuation(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Enable/Disable Soft AGC for a specific mode set.
 *
 * Sends SPI command: [0xC5][mode_set][active][checksum]
 * gainValue: 0 = PASSIVE (bypassed), 1 = ACTIVE (enabled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  0=PASSIVE, 1=ACTIVE
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetSoftAgcActive(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Soft AGC Target Modulation Index for a specific mode set.
 *
 * Sends SPI command: [0xC6][mode_set][index][checksum]
 * Range: 0-100 (%).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Target modulation index (0-100)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetSoftAgcTargetModIndex(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Soft AGC Max Gain for a specific mode set.
 *
 * Sends SPI command: [0xC7][mode_set][gain][checksum]
 * Range: 0-30 (dB).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Max gain (0-30 dB)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetSoftAgcMaxGain(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Enable/Disable Hard AGC for a specific mode set.
 *
 * Sends SPI command: [0xC8][mode_set][active][checksum]
 * gainValue: 0 = PASSIVE (bypassed), 1 = ACTIVE (enabled).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  0=PASSIVE, 1=ACTIVE
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcActive(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Hard AGC Target Gain for a specific mode set.
 *
 * Sends SPI command: [0xC9][mode_set][target][checksum]
 * Range: 0-7 (8 discrete levels: -5.5dB to -24dB).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Target gain index (0-7)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcTargetGain(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Hard AGC Attack Time for a specific mode set.
 *
 * Sends SPI command: [0xCA][mode_set][attack][checksum]
 * Range: 0-3 (4 discrete levels: 8ms, 11ms, 16ms, 20ms).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Attack time index (0-3)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcAttackTime(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Hard AGC Decay Time for a specific mode set.
 *
 * Sends SPI command: [0xCB][mode_set][decay][checksum]
 * Range: 0-3 (4 discrete levels: 100ms, 200ms, 400ms, 500ms).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Decay time index (0-3)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcDecayTime(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Hard AGC Max Gain for a specific mode set.
 *
 * Sends SPI command: [0xCC][mode_set][gain][checksum]
 * Range: 0-119 (x2 scaled = 0dB to 59.5dB in 0.5dB steps).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Max gain value (x2 scaled)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcMaxGain(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Hard AGC Noise Hysteresis Level for a specific mode set.
 *
 * Sends SPI command: [0xCD][mode_set][hysteresis][checksum]
 * Range: 0-3 (0=disabled, 1=1dB, 2=2dB, 3=3dB).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Hysteresis index (0-3)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcNoiseHysteresis(AudioModeSetIndex modeSet, uint8_t gainValue);

/**
 * @brief Set Hard AGC Noise Threshold for a specific mode set.
 *
 * Sends SPI command: [0xCE][mode_set][threshold][checksum]
 * Range: 0-31 (5-bit, -30dB to -90dB in 2dB steps).
 *
 * @param modeSet    Audio mode set index
 * @param gainValue  Noise threshold index (0-31)
 * @return 0 on success, non-zero on error
 */
int AudioSpiSetHardAgcNoiseThreshold(AudioModeSetIndex modeSet, uint8_t gainValue);

#endif /* AUDIOSPIGATEWAY_H */
