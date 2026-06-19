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
 * @file AudioParamDef.h
 * @brief Audio DSP (C5416) gain configuration parameter definitions.
 *
 * Defines parameter enums, gain block struct (15 bytes), and config file
 * data struct (300 bytes) for Audio DSP gain configuration storage
 * and transfer over SPI.
 *
 * Gain value scaling convention:
 *   - x1 (direct index): Discrete/set-point parameters stored as-is in uint8_t
 *   - x2 (half-dB steps): 0.5dB-step parameters scaled by 2 (e.g., 6dB=12, 59.5dB=119)
 */

#ifndef AUDIOPARAMDEF_H
#define AUDIOPARAMDEF_H

#include <stdint.h>

/* ========================================================================= */
/* Constants                                                                  */
/* ========================================================================= */

/* Number of audio mode sets: AM / FM / HQII / 9600 / 9600A / SK2 */
#define AUDIO_SET_COUNT             6U

/* Gain block size: 15 uint8_t values = 15 bytes */
#define AUDIO_GAIN_BLOCK_SIZE       15U

/* Config file struct size: 300 bytes exactly */
#define AUDIO_CFG_FILE_SIZE         300U

/* Header fields */
#define AUDIO_CFG_HEADER_ID_SIZE    40U
#define AUDIO_CFG_VERSION_SIZE      30U  /* "VMM.mm.ff DD.MM.YYYY HH:MM:SS\0" + pad */

/* File header constants (fixed set names, no per-set naming) */
#define AUDIO_CFG_MAGIC             "AUDIO_CFG_V1"

#define AUDIO_CFG_SET_AM            "AM"
#define AUDIO_CFG_SET_FM            "FM"
#define AUDIO_CFG_SET_HQII          "HQII"
#define AUDIO_CFG_SET_9600          "9600"
#define AUDIO_CFG_SET_9600A         "9600A"
#define AUDIO_CFG_SET_SK2           "SK2"

#define AUDIO_CFG_VERSION           "1.0.0"

/* ========================================================================= */
/* Mode set index enum                                                        */
/* ========================================================================= */

/**
 * @brief Audio mode set index enumeration.
 * Used to select which of the 6 predefined mode sets is being accessed.
 */
typedef enum {
    AM_MODE = 0,              /**< AM broadcast mode */
    FM_MODE,                  /**< FM broadcast mode */
    HQII_MODE,                /**< HQII (high fidelity II) mode */
    MODE_9600,                /**< 9600 protocol mode */
    MODE_9600A,               /**< 9600A protocol mode */
    SK2_MODE,                 /**< SK2 (Turkish military) mode */
    AUDIO_MODE_CNT            /**< Count of mode sets */
} AudioModeSetIndex;

/* ========================================================================= */
/* Gain parameter offsets within AudioGainBlock (15 bytes)                    */
/* ========================================================================= */

/**
 * @brief Offsets of gain parameters within the 15-byte AudioGainBlock struct.
 * These offsets are used to locate each field in the packed binary representation.
 */
typedef enum {
    OFFSET_INPUT_LEVEL_CONTROL_GAIN       = 0,    /* 0-8,         x1 (index) */
    OFFSET_ADC_PGA_GAIN                   = 1,    /* 0-119,       x2 (0.5dB steps) */
    OFFSET_SIDETONE_ATTENUATION_GAIN      = 2,    /* 0-120,       x2 (0dB to -60dB) */
    OFFSET_OUTPUT_LEVEL_CONTROL_GAIN      = 3,    /* 0-9,         x1 (index) */
    OFFSET_SPEAKER_ATTENUATION_GAIN       = 4,    /* 0-127,       x2 (0.5dB steps) */
    OFFSET_SOFT_AGC_ACTIVE                = 5,    /* 0-1,         x1 (PASSIVE/ACTIVE) */
    OFFSET_SOFT_AGC_TARGET_MOD_INDEX      = 6,    /* 0-100,       x1 (%1 steps)     */
    OFFSET_SOFT_AGC_MAX_GAIN              = 7,    /* 0-30,        x1 (0-30dB)      */
    OFFSET_HARD_AGC_ACTIVE                = 8,    /* 0-1,         x1 (PASSIVE/ACTIVE) */
    OFFSET_HARD_AGC_TARGET_GAIN           = 9,    /* 0-7,         x1 (8 discrete)   */
    OFFSET_HARD_AGC_ATTACK_TIME           = 10,   /* 0-3,         x1 (4 discrete)   */
    OFFSET_HARD_AGC_DECAY_TIME            = 11,   /* 0-3,         x1 (4 discrete)   */
    OFFSET_HARD_AGC_MAX_GAIN              = 12,   /* 0-119,       x2 (0.5dB steps) */
    OFFSET_HARD_AGC_NOISE_HYSTERESIS_LEVEL= 13,   /* 0-3,         x1 (4 discrete)   */
    OFFSET_HARD_AGC_NOISE_THRESHOLD       = 14,   /* 0-31,        x1 (5-bit)        */
    AUDIO_GAIN_PARAM_CNT                  = 15
} AudioGainOffset;

/* ========================================================================= */
/* Input Level Control Gain (x1, 0-8 = 9 discrete levels)                   */
/* ========================================================================= */

/**
 * @brief Input level control gain levels.
 * Each level represents a 1.5dB step. Input range: 0dB to -12dB.
 */
typedef enum {
    DB0_GAIN         = 0,      /*  0.0 dB  */
    DB_MINUS_1_5_GAIN = 1,     /* -1.5 dB  */
    DB_MINUS_3_GAIN  = 2,      /* -3.0 dB  */
    DB_MINUS_4_5_GAIN = 3,     /* -4.5 dB  */
    DB_MINUS_6_GAIN  = 4,      /* -6.0 dB  */
    DB_MINUS_7_5_GAIN = 5,     /* -7.5 dB  */
    DB_MINUS_9_GAIN  = 6,      /* -9.0 dB  */
    DB_MINUS_10_5_GAIN = 7,    /* -10.5 dB */
    DB_MINUS_12_GAIN = 8       /* -12.0 dB */
} InputLevelControlGain;

/* ========================================================================= */
/* Output Level Control Gain (x1, 0-9 = 10 discrete levels)                 */
/* ========================================================================= */

/**
 * @brief Output level control gain levels.
 * Each level represents 1dB step. Range: 0dB to +9dB.
 */
typedef enum {
    OUT_LEVEL_0DB  = 0,   /*  0 dB */
    OUT_LEVEL_1DB  = 1,   /* +1 dB */
    OUT_LEVEL_2DB  = 2,   /* +2 dB */
    OUT_LEVEL_3DB  = 3,   /* +3 dB */
    OUT_LEVEL_4DB  = 4,   /* +4 dB */
    OUT_LEVEL_5DB  = 5,   /* +5 dB */
    OUT_LEVEL_6DB  = 6,   /* +6 dB */
    OUT_LEVEL_7DB  = 7,   /* +7 dB */
    OUT_LEVEL_8DB  = 8,   /* +8 dB */
    OUT_LEVEL_9DB  = 9    /* +9 dB */
} OutputLevelControlGain;

/* ========================================================================= */
/* Soft AGC Active (x1)                                                       */
/* ========================================================================= */

typedef enum {
    SOFT_AGC_PASSIVE = 0,       /**< Soft AGC disabled (bypassed) */
    SOFT_AGC_ACTIVE  = 1        /**< Soft AGC enabled */
} SoftAgcActive;

/* ========================================================================= */
/* Hard AGC Active (x1)                                                       */
/* ========================================================================= */

typedef enum {
    HARD_AGC_PASSIVE = 0,       /**< Hard AGC disabled (bypassed) */
    HARD_AGC_ACTIVE  = 1        /**< Hard AGC enabled */
} HardAgcActive;

/* ========================================================================= */
/* Hard AGC Target Gain (x1, 8 discrete levels)                              */
/* ========================================================================= */

/**
 * @brief Hard AGC target gain levels.
 * Standard PowerPC radio AGC target settings.
 */
typedef enum {
    HARD_AGC_TARGET_MINUS_5_5DB  = 0x00,    /* -5.5 dB */
    HARD_AGC_TARGET_MINUS_8_DB   = 0x01,    /* -8.0 dB */
    HARD_AGC_TARGET_MINUS_10_DB  = 0x02,    /* -10.0 dB */
    HARD_AGC_TARGET_MINUS_12_DB  = 0x03,    /* -12.0 dB */
    HARD_AGC_TARGET_MINUS_14_DB  = 0x04,    /* -14.0 dB */
    HARD_AGC_TARGET_MINUS_17_DB  = 0x05,    /* -17.0 dB */
    HARD_AGC_TARGET_MINUS_20_DB  = 0x06,    /* -20.0 dB */
    HARD_AGC_TARGET_MINUS_24_DB  = 0x07     /* -24.0 dB */
} HardAgcTargetGain;

/* ========================================================================= */
/* Hard AGC Attack/Decay Time (x1, 4 discrete levels)                        */
/* ========================================================================= */

/**
 * @brief Hard AGC attack time levels.
 * Attack time: how fast AGC responds to signal increase.
 */
typedef enum {
    HARD_AGC_ATTACK_8MS  = 0x00,    /* 8 msec  */
    HARD_AGC_ATTACK_11MS = 0x01,    /* 11 msec */
    HARD_AGC_ATTACK_16MS = 0x02,    /* 16 msec */
    HARD_AGC_ATTACK_20MS = 0x03     /* 20 msec */
} HardAgcAttackTime;

/**
 * @brief Hard AGC decay time levels.
 * Decay time: how fast AGC releases after signal decrease.
 */
typedef enum {
    HARD_AGC_DECAY_100MS = 0x00,    /* 100 msec */
    HARD_AGC_DECAY_200MS = 0x01,    /* 200 msec */
    HARD_AGC_DECAY_400MS = 0x02,    /* 400 msec */
    HARD_AGC_DECAY_500MS = 0x03     /* 500 msec */
} HardAgcDecayTime;

/* ========================================================================= */
/* Hard AGC Noise Hysteresis (x1, 4 discrete levels)                         */
/* ========================================================================= */

/**
 * @brief Hard AGC noise hysteresis levels.
 * Prevents AGC gain fluctuations near the threshold.
 */
typedef enum {
    HARD_AGC_HYSTERESIS_DISABLED = 0x00,    /* Hysteresis disabled */
    HARD_AGC_HYSTERESIS_1DB      = 0x01,    /* 1 dB hysteresis     */
    HARD_AGC_HYSTERESIS_2DB      = 0x02,    /* 2 dB hysteresis     */
    HARD_AGC_HYSTERESIS_3DB      = 0x03     /* 3 dB hysteresis     */
} HardAgcNoiseHysteresis;

/* ========================================================================= */
/* Hard AGC Noise Threshold (x1, 5-bit: 0-31)                                */
/* ========================================================================= */

/**
 * @brief Hard AGC noise threshold levels (5-bit, 0-31).
 * Maps to -30 dB (0x01) through -90 dB (0x1F). 0x00 = disabled.
 */
typedef enum {
    HARD_AGC_NOISE_DISABLED      = 0x00,    /* Disabled */
    HARD_AGC_NOISE_MINUS_30_DB   = 0x01,    /* -30 dB */
    HARD_AGC_NOISE_MINUS_32_DB   = 0x02,    /* -32 dB */
    HARD_AGC_NOISE_MINUS_34_DB   = 0x03,    /* -34 dB */
    HARD_AGC_NOISE_MINUS_36_DB   = 0x04,    /* -36 dB */
    HARD_AGC_NOISE_MINUS_38_DB   = 0x05,    /* -38 dB */
    HARD_AGC_NOISE_MINUS_40_DB   = 0x06,    /* -40 dB */
    HARD_AGC_NOISE_MINUS_42_DB   = 0x07,    /* -42 dB */
    HARD_AGC_NOISE_MINUS_44_DB   = 0x08,    /* -44 dB */
    HARD_AGC_NOISE_MINUS_46_DB   = 0x09,    /* -46 dB */
    HARD_AGC_NOISE_MINUS_48_DB   = 0x0A,    /* -48 dB */
    HARD_AGC_NOISE_MINUS_50_DB   = 0x0B,    /* -50 dB */
    HARD_AGC_NOISE_MINUS_52_DB   = 0x0C,    /* -52 dB */
    HARD_AGC_NOISE_MINUS_54_DB   = 0x0D,    /* -54 dB */
    HARD_AGC_NOISE_MINUS_56_DB   = 0x0E,    /* -56 dB */
    HARD_AGC_NOISE_MINUS_58_DB   = 0x0F,    /* -58 dB */
    HARD_AGC_NOISE_MINUS_60_DB   = 0x10,    /* -60 dB */
    HARD_AGC_NOISE_MINUS_62_DB   = 0x11,    /* -62 dB */
    HARD_AGC_NOISE_MINUS_64_DB   = 0x12,    /* -64 dB */
    HARD_AGC_NOISE_MINUS_66_DB   = 0x13,    /* -66 dB */
    HARD_AGC_NOISE_MINUS_68_DB   = 0x14,    /* -68 dB */
    HARD_AGC_NOISE_MINUS_70_DB   = 0x15,    /* -70 dB */
    HARD_AGC_NOISE_MINUS_72_DB   = 0x16,    /* -72 dB */
    HARD_AGC_NOISE_MINUS_74_DB   = 0x17,    /* -74 dB */
    HARD_AGC_NOISE_MINUS_76_DB   = 0x18,    /* -76 dB */
    HARD_AGC_NOISE_MINUS_78_DB   = 0x19,    /* -78 dB */
    HARD_AGC_NOISE_MINUS_80_DB   = 0x1A,    /* -80 dB */
    HARD_AGC_NOISE_MINUS_82_DB   = 0x1B,    /* -82 dB */
    HARD_AGC_NOISE_MINUS_84_DB   = 0x1C,    /* -84 dB */
    HARD_AGC_NOISE_MINUS_86_DB   = 0x1D,    /* -86 dB */
    HARD_AGC_NOISE_MINUS_88_DB   = 0x1E,    /* -88 dB */
    HARD_AGC_NOISE_MINUS_90_DB   = 0x1F     /* -90 dB */
} HardAgcNoiseThreshold;

/* ========================================================================= */
/* Hard AGC Active                                                            */
/* ========================================================================= */

typedef HardAgcActive HardAgcActiveType;

/* ========================================================================= */
/* Audio Gain Block (15 bytes, packed)                                       */
/* ========================================================================= */

/**
 * @brief Audio DSP gain configuration block for a single mode set.
 *
 * All 15 parameters packed into exactly 15 bytes (one uint8_t per param).
 * Gain value scaling:
 *   - x1 (direct): Discrete/index params stored as the enum value directly
 *   - x2 (scaled): 0.5dB-step params multiplied by 2 (e.g., 6dB = 12)
 *
 * Layout (15 consecutive bytes):
 *   Byte 0:  Input Level Control Gain       (0-8,   x1)
 *   Byte 1:  ADC PGA Gain                   (0-119, x2)
 *   Byte 2:  Sidetone Attenuation Gain      (0-120, x2)
 *   Byte 3:  Output Level Control Gain      (0-9,   x1)
 *   Byte 4:  Speaker Attenuation Gain       (0-127, x2)
 *   Byte 5:  Soft AGC Active                (0-1,   x1)
 *   Byte 6:  Soft AGC Target Mod Index      (0-100, x1)
 *   Byte 7:  Soft AGC Max Gain              (0-30,  x1)
 *   Byte 8:  Hard AGC Active                (0-1,   x1)
 *   Byte 9:  Hard AGC Target Gain           (0-7,   x1)
 *   Byte 10: Hard AGC Attack Time           (0-3,   x1)
 *   Byte 11: Hard AGC Decay Time            (0-3,   x1)
 *   Byte 12: Hard AGC Max Gain              (0-119, x2)
 *   Byte 13: Hard AGC Noise Hysteresis Level(0-3,   x1)
 *   Byte 14: Hard AGC Noise Threshold       (0-31,  x1)
 */
typedef struct {
    uint8_t input_level_control_gain;       /* 0-8 (index, x1)        */
    uint8_t adc_pga_gain;                   /* 0-119 (0.5dB*2, max 59.5dB at 119) */
    uint8_t sidetone_attenuation_gain;      /* 0-120 (0dB to -60dB, *2, max 120)  */
    uint8_t output_level_control_gain;      /* 0-9 (index, x1)     */
    uint8_t speaker_attenuation_gain;       /* 0-127 (0.5dB*2, max 63.5dB at 127) */
    uint8_t soft_agc_active;                /* 0=PASSIVE, 1=ACTIVE */
    uint8_t soft_agc_target_mod_index;      /* 0-100 (%1 steps, x1) */
    uint8_t soft_agc_max_gain;              /* 0-30 (x1 dB, max 30dB) */
    uint8_t hard_agc_active;                /* 0=PASSIVE, 1=ACTIVE */
    uint8_t hard_agc_target_gain;           /* 0-7 (8 discrete levels, x1) */
    uint8_t hard_agc_attack_time;           /* 0-3 (4 discrete, x1) */
    uint8_t hard_agc_decay_time;            /* 0-3 (4 discrete, x1) */
    uint8_t hard_agc_max_gain;              /* 0-119 (0.5dB*2, max 59.5dB at 119) */
    uint8_t hard_agc_noise_hysteresis;      /* 0-3 (4 discrete, x1) */
    uint8_t hard_agc_noise_threshold;       /* 0-31 (5-bit, x1) */
} AudioGainBlock;                          /* Total: 15 bytes       */

/* ========================================================================= */
/* Audio Config File Data (300 bytes, packed)                                */
/* ========================================================================= */

/**
 * @brief Audio configuration file data structure (300 bytes).
 *
 * Used for loading/storing audio config from/to flash (DA2 block).
 * Layout:
 *   Offset 0:   Magic ID (40 bytes)
 *   Offset 40:  Version (10 bytes)
 *   Offset 50:  Set count (= 6, 1 byte)
 *   Offset 51:  Sets (6 x 15 = 90 bytes)
 *   Offset 141: CRC32 (4 bytes, covers header_id..last byte of sets[5])
 *   Offset 145: Unused padding (155 bytes)
 *   Total:     300 bytes exactly
 *
 * @note The entire struct must be packed to avoid any compiler padding.
 */
typedef struct {
    char header_id[AUDIO_CFG_HEADER_ID_SIZE]; /* "AUDIO_CFG_V1" + pad to 40B */
    char version[AUDIO_CFG_VERSION_SIZE];     /* "1.0.0" + pad to 10B    */
    uint8_t set_count;                        /* Must be AUDIO_SET_COUNT  */
    AudioGainBlock sets[AUDIO_SET_COUNT];     /* 6 x 15 = 90 bytes        */
    uint32_t crc32;                           /* CRC32 of data area       */
    uint8_t unused_pad[AUDIO_CFG_FILE_SIZE -  /* Padding to 300 bytes     */
                  (AUDIO_CFG_HEADER_ID_SIZE +
                   AUDIO_CFG_VERSION_SIZE + 1 +
                   (AUDIO_GAIN_BLOCK_SIZE * AUDIO_SET_COUNT) + 4)];
} __attribute__((packed)) AudioConfigFileData; /* Total: 300 bytes, no padding */

/* ========================================================================= */
/* External declarations                                                      */
/* ========================================================================= */

#ifdef  ALLOCATE
    #define AEXT
    #define AINIT(x) = x
#else
    #define AEXT extern
    #define AINIT(x)
#endif

/* Initialize a gain block to default values (all 0 / PASSIVE). */
AEXT void AudioGainBlockDefault(AudioGainBlock *pBlock);

/* Initialize the config file data with defaults. */
AEXT void AudioConfigFileDefault(AudioConfigFileData *pData);

/* Validate that a gain block contains reasonable values. */
AEXT int  AudioGainBlockValidate(const AudioGainBlock *pBlock);

/* Calculate CRC32 over a data buffer (standard CRC32 polynomial 0xEDB88320). */
AEXT uint32_t AudioCfgCrc32Calculate(const uint8_t *pData, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOPARAMDEF_H */
