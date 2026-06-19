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
 * @file AudioParamDef.c
 * @brief Audio DSP (C5416) gain parameter definitions implementation.
 *
 * Implements binary-to-label mapping, gain value translation,
 * CRC32 calculation, and default/validate helpers.
 */

#include "AudioParamDef.h"

/* ========================================================================= */
/* Constants for validation ranges                                            */
/* ========================================================================= */

/* Input Level Control Gain: valid 0-8 */
#define INPUT_LEVEL_GAIN_MIN  0U
#define INPUT_LEVEL_GAIN_MAX  8U

/* ADC PGA Gain: valid 0-119 (x2 scaling, max 59.5dB) */
#define ADC_PGA_GAIN_MIN      0U
#define ADC_PGA_GAIN_MAX      119U

/* Sidetone Attenuation Gain: valid 0-120 (x2 scaling, -60dB = 120 after abs) */
#define SIDETONE_ATTEN_GAIN_MIN 0U
#define SIDETONE_ATTEN_GAIN_MAX 120U

/* Output Level Control Gain: valid 0-9 */
#define OUTPUT_LEVEL_GAIN_MIN   0U
#define OUTPUT_LEVEL_GAIN_MAX   9U

/* Speaker Attenuation Gain: valid 0-127 (x2 scaling, max 63.5dB) */
#define SPEAKER_ATTEN_GAIN_MIN  0U
#define SPEAKER_ATTEN_GAIN_MAX  127U

/* Soft AGC Target Mod Index: valid 0-100 */
#define SOFT_AGC_TARGET_MOD_MIN 0U
#define SOFT_AGC_TARGET_MOD_MAX 100U

/* Soft AGC Max Gain: valid 0-30 */
#define SOFT_AGC_MAX_GAIN_MIN   0U
#define SOFT_AGC_MAX_GAIN_MAX   30U

/* Hard AGC Target Gain: valid 0-7 */
#define HARD_AGC_TARGET_MIN     0U
#define HARD_AGC_TARGET_MAX     7U

/* Hard AGC Attack/Decay Time: valid 0-3 */
#define HARD_AGC_ATTACK_MIN     0U
#define HARD_AGC_ATTACK_MAX     3U
#define HARD_AGC_DECAY_MIN      0U
#define HARD_AGC_DECAY_MAX      3U

/* Hard AGC Max Gain: valid 0-119 (x2 scaling) */
#define HARD_AGC_MAX_GAIN_MIN   0U
#define HARD_AGC_MAX_GAIN_MAX   119U

/* Hard AGC Noise Hysteresis: valid 0-3 */
#define HARD_AGC_NOISE_HYST_MIN 0U
#define HARD_AGC_NOISE_HYST_MAX 3U

/* Hard AGC Noise Threshold: valid 0-31 (but 0 = disabled, 1-31 = -30/-32..-90) */
#define HARD_AGC_NOISE_THR_MIN  0U
#define HARD_AGC_NOISE_THR_MAX  31U

/* ========================================================================= */
/* Static helper functions                                                    */
/* ========================================================================= */

/**
 * @brief CRC32 lookup table (generated with polynomial 0xEDB88320).
 */
static const uint32_t crc32_table[256] = {
    0x00000000U, 0x77073096U, 0xEE0E612CU, 0x990951BAU,
    0x076DC419U, 0x706AF48FU, 0xE963A535U, 0x9E6495A3U,
    0x0EDB8832U, 0x79DCB8A4U, 0xE0D5E91EU, 0x97D2D988U,
    0x09B64C2BU, 0x7EB17CBDU, 0xE7B82D07U, 0x90BF1D91U,
    0x136C9856U, 0x646BA8C0U, 0xFD62F97AU, 0x8A65C9ECU,
    0x14015C4FU, 0x63066C59U, 0xFA0F3D63U, 0x8D080DF5U,
    0x3B6E20C8U, 0x4C69105EU, 0xD56041E4U, 0xA2677172U,
    0x3C03E4D1U, 0x4B04D447U, 0xD20D85FDU, 0xA50AB56BU,
    0x35B5A8FAU, 0x42B2986CU, 0xDBBBC9D6U, 0xACBCF940U,
    0x32D86CE3U, 0x45DF5C75U, 0xDCD60DCFU, 0xABD13D59U,
    0x26D9B0BCU, 0x51DE802AU, 0xC8D7D190U, 0xBFD0E106U,
    0x21B474A5U, 0x56B34433U, 0xCFBA5589U, 0xB8BD651FU,
    0x2802788EU, 0x5F054818U, 0xC60C19A2U, 0xB10B2934U,
    0x2F6FBC97U, 0x58688C01U, 0xC161DDFBU, 0xB666ED6DU,
    0x76DC4190U, 0x01DB7106U, 0x98D220BCU, 0xEFD5102AU,
    0x71B18589U, 0x06B6B51FU, 0x9FBFE4A5U, 0xE8B8D433U,
    0x7807C9A2U, 0x0F00F934U, 0x9609A88EU, 0xE10E9818U,
    0x7F6A0DBBU, 0x086D3D2DU, 0x91646C97U, 0xE6635C01U,
    0x6B6B51F4U, 0x1C6C6162U, 0x856530D8U, 0xF262004EU,
    0x6C0695EDU, 0x1B01A57BU, 0x8208F4C1U, 0xF50FC457U,
    0x65B0D9C6U, 0x12B7E950U, 0x8BBEB8EAU, 0xFCB9887CU,
    0x62DD1DDFU, 0x15DA2D49U, 0x8CD37CF3U, 0xFBD44C65U,
    0x4DB26158U, 0x3AB551CEU, 0xA3BC0074U, 0xD4BB30E2U,
    0x4ADFA541U, 0x3DD895D7U, 0xA4D1C46DU, 0xD3D6F4FBU,
    0x4369E96AU, 0x346ED9FCU, 0xAD678846U, 0xDA60B8D0U,
    0x44042D73U, 0x33031DE5U, 0xAA0A4C5FU, 0xDD0D7CC9U,
    0x5005713CU, 0x270241AAU, 0xBE0B1010U, 0xC90C2086U,
    0x5768B525U, 0x206F85B3U, 0xB966D409U, 0xCE61E49FU,
    0x5EDEF90EU, 0x29D9C998U, 0xB0D09822U, 0xC7D7A8B4U,
    0x59B33D17U, 0x2EB40D81U, 0xB7BD5C3BU, 0xC0BA6CADU,
    0xEDB88320U, 0x9ABFB3B6U, 0x03B6E20CU, 0x74B1D29AU,
    0xEAD54739U, 0x9DD277AFU, 0x04DB2615U, 0x73DC1683U,
    0xE3630B12U, 0x94643B84U, 0x0D6D6A3EU, 0x7A6A5AA8U,
    0xE40ECF0BU, 0x9309FF9DU, 0x0A00AE27U, 0x7D079EB1U,
    0xF00F9344U, 0x8708A3D2U, 0x1E01F268U, 0x6906C2FEU,
    0xF762575DU, 0x806567CBU, 0x196C3671U, 0x6E6B06E7U,
    0xFED41B76U, 0x89D32BE0U, 0x10DA7A5AU, 0x67DD4ACCU,
    0xF9B9DF6FU, 0x8EBEEFF9U, 0x17B7BE43U, 0x60B08ED5U,
    0xD6D6A3E8U, 0xA1D1937EU, 0x38D8C2C4U, 0x4FDFF252U,
    0xD1BB67F1U, 0xA6BC5767U, 0x3FB506DDU, 0x48B2364BU,
    0xD80D2BDAU, 0xAF0A1B4CU, 0x36034AF6U, 0x41047A60U,
    0xDF60EFC3U, 0xA867DF55U, 0x316E8EEFU, 0x4669BE79U,
    0xCB61B38CU, 0xBC66831AU, 0x256FD2A0U, 0x5268E236U,
    0xCC0C7795U, 0xBB0B4703U, 0x220216B9U, 0x5505262FU,
    0xC5BA3BBEU, 0xB2BD0B28U, 0x2BB45A92U, 0x5CB36A04U,
    0xC2D7FFA7U, 0xB5D0CF31U, 0x2CD99E8BU, 0x5BDEAE1DU,
    0x9B64C2B0U, 0xEC63F226U, 0x756AA39CU, 0x026D930AU,
    0x9C0906A9U, 0xEB0E363FU, 0x72076785U, 0x05005713U,
    0x95BF4A82U, 0xE2B87A14U, 0x7BB12BAEU, 0x0CB61B38U,
    0x92D28E9BU, 0xE5D5BE0DU, 0x7CDCEFB7U, 0x0BDBDF21U,
    0x86D3D2D4U, 0xF1D4E242U, 0x68DDB3F8U, 0x1FDA836EU,
    0x81BE16CDU, 0xF6B9265BU, 0x6FB077E1U, 0x18B74777U,
    0x88085AE6U, 0xFF0F6A70U, 0x66063BCAU, 0x11010B5CU,
    0x8F659EFFU, 0xF862AE69U, 0x616BFFD3U, 0x166CCF45U,
    0xA00AE278U, 0xD70DD2EEU, 0x4E048354U, 0x3903B3C2U,
    0xA7672661U, 0xD06016F7U, 0x4969474DU, 0x3E6E77DBU,
    0xAED16A4AU, 0xD9D65ADEU, 0x40DF0B64U, 0x37D83BF2U,
    0xA9BCAE51U, 0xDEBB9EC7U, 0x47B2CF7DU, 0x30B5FFEBU,
    0xBDBDF21DU, 0xCABAC28BU, 0x53B39331U, 0x24B4A3A7U,
    0xBAD03604U, 0xCDD70692U, 0x54DE5728U, 0x23D967BEU,
    0xB3667A2FU, 0xC4614AB9U, 0x5D681B03U, 0x2A6F2B95U,
    0xB40BBE36U, 0xC30C8EA0U, 0x5A05DF1AU, 0x2D02EF8CU
};

/**
 * @brief Internal: CRC32 over data using precomputed table.
 *
 * @param pData     Pointer to data buffer.
 * @param length    Number of bytes.
 * @return CRC32 checksum value.
 */
static uint32_t _crc32_internal(const uint8_t *pData, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;

    for (i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ pData[i]) & 0xFF];
    }

    return (crc ^ 0xFFFFFFFFU);
}

/* ========================================================================= */
/* Binary-to-label mapping functions                                          */
/* ========================================================================= */

/**
 * @brief Convert input level control gain raw value to human-readable label.
 *
 * @param raw   Raw gain value (0-8).
 * @param label Output buffer (minimum 12 bytes) for label string.
 * @return 0 on success.
 */
int InputLevelControlGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    static const char *labels[] = {
        "0dB", "-1.5dB", "-3dB", "-4.5dB", "-6dB",
        "-7.5dB", "-9dB", "-10.5dB", "-12dB"
    };

    if (raw > 8) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 6) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%s", labels[raw]);
    return 0;
}

/**
 * @brief Convert ADC PGA gain raw value (x2) to dB value in milli-dB.
 *
 * @param raw   Raw gain value (0-119), scaled by x2.
 * @return Gain in 0.1dB units (e.g., 60 = 6.0dB), or -1 if invalid.
 */
int AdcPgaGainToDbMili(int raw)
{
    if (raw < 0 || raw > 119) {
        return -1;
    }
    /* x2 scaling: value = dB * 2, so dB = raw / 2.0 */
    /* Return in 0.1dB units: db_mili = raw / 2 * 10 */
    return (raw * 5);
}

/**
 * @brief Convert ADC PGA gain raw value to human-readable label.
 *
 * @param raw   Raw gain value (0-119), scaled by x2.
 * @param label Output buffer (minimum 16 bytes) for label string.
 * @return 0 on success, -1 if invalid.
 */
int AdcPgaGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    float db;

    if (raw > 119) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }

    db = (float)raw / 2.0f;
    (void)snprintf(label, labelSize, "%.1fdb", db);
    return 0;
}

/**
 * @brief Convert sidetone attenuation gain raw value (x2) to dB value.
 *
 * @param raw   Raw value (0-120), scaled by x2. 0=0dB, 120=-60dB.
 * @return Attenuation in 0.1dB units (positive = attenuation), or -1 if invalid.
 */
int SidetoneAttenuationGainToDbMili(int raw)
{
    if (raw < 0 || raw > 120) {
        return -1;
    }
    /* 0dB = 0, -60dB = 120 (x2), so dB = -raw/2.0 */
    /* Return as positive attenuation in 0.1dB units */
    return (raw * 5);
}

/**
 * @brief Convert sidetone attenuation gain raw value to human-readable label.
 *
 * @param raw   Raw value (0-120), scaled by x2.
 * @param label Output buffer (minimum 16 bytes).
 * @return 0 on success, -1 if invalid.
 */
int SidetoneAttenuationGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    float db;

    if (raw > 120) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }

    db = -(float)raw / 2.0f;
    (void)snprintf(label, labelSize, "%.1fdb", db);
    return 0;
}

/**
 * @brief Convert output level control gain raw value to dB label.
 *
 * @param raw   Raw value (0-9), x1 scaling.
 * @param label Output buffer (minimum 8 bytes).
 * @return 0 on success, -1 if invalid.
 */
int OutputLevelControlGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    char buf[12];

    if (raw > 9) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 8) {
        return -1;
    }
    (void)snprintf(buf, sizeof(buf), "+%ddb", raw);
    (void)snprintf(label, labelSize, "%s", buf);
    return 0;
}

/**
 * @brief Convert speaker attenuation gain raw value (x2) to dB label.
 *
 * @param raw   Raw value (0-127), scaled by x2.
 * @param label Output buffer (minimum 16 bytes).
 * @return 0 on success, -1 if invalid.
 */
int SpeakerAttenuationGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    float db;

    if (raw > 127) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }

    db = -(float)raw / 2.0f;
    (void)snprintf(label, labelSize, "%.1fdb", db);
    return 0;
}

/**
 * @brief Convert soft AGC target mod index (0-100) to label.
 *
 * @param raw   Raw index value (0-100), x1 scaling.
 * @param label Output buffer (minimum 16 bytes).
 * @return 0 on success, -1 if invalid.
 */
int SoftAgcTargetModIndexToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    if (raw > 100) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%d%%", raw);
    return 0;
}

/**
 * @brief Convert soft AGC max gain raw value (0-30) to label.
 *
 * @param raw   Raw value (0-30), x1 scaling.
 * @param label Output buffer (minimum 8 bytes).
 * @return 0 on success, -1 if invalid.
 */
int SoftAgcMaxGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    if (raw > 30) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 8) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%ddB", raw);
    return 0;
}

/**
 * @brief Convert hard AGC target gain raw value (0-7) to label.
 *
 * @param raw   Raw value (0-7), x1 scaling.
 * @param label Output buffer (minimum 12 bytes).
 * @return 0 on success, -1 if invalid.
 */
int HardAgcTargetGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    static const char *labels[] = {
        "-5.5dB", "-8dB", "-10dB", "-12dB",
        "-14dB", "-17dB", "-20dB", "-24dB"
    };

    if (raw > 7) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 12) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%s", labels[raw]);
    return 0;
}

/**
 * @brief Convert hard AGC attack time raw value (0-3) to label.
 *
 * @param raw   Raw value (0-3), x1 scaling.
 * @param label Output buffer (minimum 12 bytes).
 * @return 0 on success, -1 if invalid.
 */
int HardAgcAttackTimeToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    static const char *labels[] = {
        "8msec", "11msec", "16msec", "20msec"
    };

    if (raw > 3) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 12) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%s", labels[raw]);
    return 0;
}

/**
 * @brief Convert hard AGC decay time raw value (0-3) to label.
 *
 * @param raw   Raw value (0-3), x1 scaling.
 * @param label Output buffer (minimum 12 bytes).
 * @return 0 on success, -1 if invalid.
 */
int HardAgcDecayTimeToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    static const char *labels[] = {
        "100msec", "200msec", "400msec", "500msec"
    };

    if (raw > 3) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 12) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%s", labels[raw]);
    return 0;
}

/**
 * @brief Convert hard AGC max gain raw value (x2) to dB label.
 *
 * @param raw   Raw value (0-119), scaled by x2 (0.5dB steps).
 * @param label Output buffer (minimum 16 bytes).
 * @return 0 on success, -1 if invalid.
 */
int HardAgcMaxGainToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    float db;

    if (raw > 119) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }

    db = (float)raw / 2.0f;
    (void)snprintf(label, labelSize, "%.1fdb", db);
    return 0;
}

/**
 * @brief Convert hard AGC noise hysteresis raw value (0-3) to label.
 *
 * @param raw   Raw value (0-3), x1 scaling.
 * @param label Output buffer (minimum 16 bytes).
 * @return 0 on success, -1 if invalid.
 */
int HardAgcNoiseHysteresisToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    if (raw == 0) {
        if (labelSize > 0) {
            (void)snprintf(label, labelSize, "DISABLED");
        }
        return 0;
    }
    if (raw > 3) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%ddb", raw);
    return 0;
}

/**
 * @brief Convert hard AGC noise threshold raw value (0-31) to dB label.
 *
 * @param raw   Raw value (0-31), x1 scaling. 0=disabled, 1=–30dB..31=–90dB.
 * @param label Output buffer (minimum 16 bytes).
 * @return 0 on success, -1 if invalid.
 */
int HardAgcNoiseThresholdToLabel(uint8_t raw, char *label, uint32_t labelSize)
{
    if (raw == 0) {
        if (labelSize > 0) {
            (void)snprintf(label, labelSize, "DISABLED");
        }
        return 0;
    }
    if (raw > 31) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 16) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%ddb", -30 - (int)((raw - 1) * 2));
    return 0;
}

/**
 * @brief Get human-readable mode set name for a given index.
 *
 * @param mode    Mode set index (AM_MODE .. SK2_MODE).
 * @param label   Output buffer (minimum 8 bytes) for set name.
 * @return 0 on success, -1 if invalid index or buffer too small.
 */
int AudioModeIndexToName(AudioModeSetIndex mode, char *label, uint32_t labelSize)
{
    static const char *names[] = {
        "AM", "FM", "HQII", "9600", "9600A", "SK2"
    };

    if ((int)mode < 0 || (int)mode >= AUDIO_MODE_CNT) {
        if (labelSize > 0) {
            label[0] = '\0';
        }
        return -1;
    }
    if (labelSize < 8) {
        return -1;
    }
    (void)snprintf(label, labelSize, "%s", names[mode]);
    return 0;
}

/* ========================================================================= */
/* Gain value translation helpers (raw <-> dB)                               */
/* ========================================================================= */

/**
 * @brief Convert raw adc_pga_gain value (x2) to dB (float).
 *
 * @param raw   Raw value (0-119).
 * @return dB value (e.g., 6.0 for raw=12), or -1.0 if invalid.
 */
float AdcPgaGainToDb(float raw)
{
    if (raw < 0 || raw > 119) {
        return -1.0f;
    }
    return raw / 2.0f;
}

/**
 * @brief Convert dB value to raw adc_pga_gain (x2 scaling).
 *
 * @param db   Gain in dB (e.g., 6.0 for 6dB).
 * @return Raw value (0-119), or 0 if invalid.
 */
uint8_t DbToAdcPgaGain(float db)
{
    uint8_t raw;

    if (db < 0.0f || db > 59.5f) {
        return 0;
    }
    raw = (uint8_t)(db * 2.0f + 0.5f);  /* round to nearest */
    if (raw > 119) {
        raw = 119;
    }
    return raw;
}

/**
 * @brief Convert raw sidetone_attenuation_gain value (x2) to dB (float).
 *
 * @param raw   Raw value (0-120), where 0=0dB, 120=-60dB.
 * @return dB value (negative = attenuation), or -1.0 if invalid.
 */
float SidetoneAttenuationGainToDb(float raw)
{
    if (raw > 120) {
        return -1.0f;
    }
    return -(raw / 2.0f);
}

/**
 * @brief Convert dB value to raw sidetone_attenuation_gain (x2 scaling).
 *
 * @param db   Gain in dB (must be 0.0 to -60.0).
 * @return Raw value (0-120), or 0 if invalid.
 */
uint8_t DbToSidetoneAttenuationGain(float db)
{
    uint8_t raw;

    if (db > 0.0f || db < -60.0f) {
        return 0;
    }
    raw = (uint8_t)((-db) * 2.0f + 0.5f);
    if (raw > 120) {
        raw = 120;
    }
    return raw;
}

/**
 * @brief Convert raw speaker_attenuation_gain value (x2) to dB (float).
 *
 * @param raw   Raw value (0-127), where 0=0dB, 127=-63.5dB.
 * @return dB value (negative = attenuation), or -1.0 if invalid.
 */
float SpeakerAttenuationGainToDb(float raw)
{
    if (raw > 127) {
        return -1.0f;
    }
    return -(raw / 2.0f);
}

/**
 * @brief Convert dB value to raw speaker_attenuation_gain (x2 scaling).
 *
 * @param db   Gain in dB (must be 0.0 to -63.5).
 * @return Raw value (0-127), or 0 if invalid.
 */
uint8_t DbToSpeakerAttenuationGain(float db)
{
    uint8_t raw;

    if (db > 0.0f || db < -63.5f) {
        return 0;
    }
    raw = (uint8_t)((-db) * 2.0f + 0.5f);
    if (raw > 127) {
        raw = 127;
    }
    return raw;
}

/**
 * @brief Convert raw hard_agc_max_gain value (x2) to dB (float).
 *
 * @param raw   Raw value (0-119), scaled by x2.
 * @return dB value, or -1.0 if invalid.
 */
float HardAgcMaxGainToDb(float raw)
{
    if (raw > 119) {
        return -1.0f;
    }
    return raw / 2.0f;
}

/**
 * @brief Convert dB value to raw hard_agc_max_gain (x2 scaling).
 *
 * @param db   Gain in dB (must be 0.0 to 59.5).
 * @return Raw value (0-119), or 0 if invalid.
 */
uint8_t DbToHardAgcMaxGain(float db)
{
    uint8_t raw;

    if (db < 0.0f || db > 59.5f) {
        return 0;
    }
    raw = (uint8_t)(db * 2.0f + 0.5f);
    if (raw > 119) {
        raw = 119;
    }
    return raw;
}

/* ========================================================================= */
/* Default/validate functions                                                 */
/* ========================================================================= */

/**
 * @brief Set all parameters in AudioGainBlock to default (initial/reset) values.
 *
 * @param pBlock   Pointer to gain block to initialize.
 */
void AudioGainBlockDefault(AudioGainBlock *pBlock)
{
    if (pBlock == NULL) {
        return;
    }

    pBlock->input_level_control_gain      = DB0_GAIN;              /* 0 = 0dB */
    pBlock->adc_pga_gain                  = 0;                     /* 0dB */
    pBlock->sidetone_attenuation_gain     = 0;                     /* 0dB (no attenuation) */
    pBlock->output_level_control_gain     = 0;                     /* 0 = 0dB */
    pBlock->speaker_attenuation_gain      = 0;                     /* 0dB (no attenuation) */
    pBlock->soft_agc_active               = SOFT_AGC_PASSIVE;      /* AGC off */
    pBlock->soft_agc_target_mod_index     = 0;                     /* 0% */
    pBlock->soft_agc_max_gain             = 0;                     /* 0dB */
    pBlock->hard_agc_active               = HARD_AGC_PASSIVE;      /* AGC off */
    pBlock->hard_agc_target_gain          = HARD_AGC_TARGET_MINUS_10_DB;  /* default -10dB */
    pBlock->hard_agc_attack_time          = HARD_AGC_ATTACK_16MS;  /* default 16ms */
    pBlock->hard_agc_decay_time           = HARD_AGC_DECAY_200MS;  /* default 200ms */
    pBlock->hard_agc_max_gain             = 0;                     /* 0dB */
    pBlock->hard_agc_noise_hysteresis     = HARD_AGC_HYSTERESIS_DISABLED;
    pBlock->hard_agc_noise_threshold      = HARD_AGC_NOISE_DISABLED;
}

/**
 * @brief Set all parameters in AudioConfigFileData to default values.
 *
 * @param pData   Pointer to config data to initialize.
 */
void AudioConfigFileDefault(AudioConfigFileData *pData)
{
    uint32_t i;

    if (pData == NULL) {
        return;
    }

    /* Set header */
    (void)memset(pData->header_id, 0, AUDIO_CFG_HEADER_ID_SIZE);
    (void)strncpy(pData->header_id, AUDIO_CFG_MAGIC, AUDIO_CFG_HEADER_ID_SIZE - 1);
    (void)memset(pData->version, 0, AUDIO_CFG_VERSION_SIZE);
    (void)strncpy(pData->version, AUDIO_CFG_VERSION, AUDIO_CFG_VERSION_SIZE - 1);

    pData->set_count = AUDIO_SET_COUNT;

    /* Initialize each mode set to defaults */
    for (i = 0; i < AUDIO_SET_COUNT; i++) {
        AudioGainBlockDefault(&pData->sets[i]);
    }

    /* CRC32 calculated from data area (header + set_count + sets) */
    pData->crc32 = AudioCfgCrc32Calculate(
        (const uint8_t *)pData,
        AUDIO_CFG_HEADER_ID_SIZE +
        AUDIO_CFG_VERSION_SIZE + 1 +
        (AUDIO_GAIN_BLOCK_SIZE * AUDIO_SET_COUNT)
    );

    /* Clear padding */
    (void)memset(pData->unused_pad, 0, sizeof(pData->unused_pad));
}

/**
 * @brief Validate that all gain values in AudioGainBlock are within valid range.
 *
 * @param pBlock   Pointer to gain block to validate.
 * @return 0 if all values are valid, non-zero if any value is out of range.
 */
int AudioGainBlockValidate(const AudioGainBlock *pBlock)
{
    if (pBlock == NULL) {
        return -1;
    }

    if (pBlock->input_level_control_gain < INPUT_LEVEL_GAIN_MIN ||
        pBlock->input_level_control_gain > INPUT_LEVEL_GAIN_MAX) {
        return -1;
    }

    if (pBlock->adc_pga_gain < ADC_PGA_GAIN_MIN ||
        pBlock->adc_pga_gain > ADC_PGA_GAIN_MAX) {
        return -1;
    }

    if (pBlock->sidetone_attenuation_gain < SIDETONE_ATTEN_GAIN_MIN ||
        pBlock->sidetone_attenuation_gain > SIDETONE_ATTEN_GAIN_MAX) {
        return -1;
    }

    if (pBlock->output_level_control_gain < OUTPUT_LEVEL_GAIN_MIN ||
        pBlock->output_level_control_gain > OUTPUT_LEVEL_GAIN_MAX) {
        return -1;
    }

    if (pBlock->speaker_attenuation_gain < SPEAKER_ATTEN_GAIN_MIN ||
        pBlock->speaker_attenuation_gain > SPEAKER_ATTEN_GAIN_MAX) {
        return -1;
    }

    if (pBlock->soft_agc_active > 1) {
        return -1;
    }

    if (pBlock->soft_agc_target_mod_index > SOFT_AGC_TARGET_MOD_MAX) {
        return -1;
    }

    if (pBlock->soft_agc_max_gain > SOFT_AGC_MAX_GAIN_MAX) {
        return -1;
    }

    if (pBlock->hard_agc_active > 1) {
        return -1;
    }

    if (pBlock->hard_agc_target_gain < HARD_AGC_TARGET_MIN ||
        pBlock->hard_agc_target_gain > HARD_AGC_TARGET_MAX) {
        return -1;
    }

    if (pBlock->hard_agc_attack_time < HARD_AGC_ATTACK_MIN ||
        pBlock->hard_agc_attack_time > HARD_AGC_ATTACK_MAX) {
        return -1;
    }

    if (pBlock->hard_agc_decay_time < HARD_AGC_DECAY_MIN ||
        pBlock->hard_agc_decay_time > HARD_AGC_DECAY_MAX) {
        return -1;
    }

    if (pBlock->hard_agc_max_gain < HARD_AGC_MAX_GAIN_MIN ||
        pBlock->hard_agc_max_gain > HARD_AGC_MAX_GAIN_MAX) {
        return -1;
    }

    if (pBlock->hard_agc_noise_hysteresis > HARD_AGC_NOISE_HYST_MAX) {
        return -1;
    }

    if (pBlock->hard_agc_noise_threshold > HARD_AGC_NOISE_THR_MAX) {
        return -1;
    }

    return 0;
}

/**
 * @brief Calculate CRC32 over data buffer.
 *
 * Uses standard CRC32 polynomial 0xEDB88320 (reflected form of CRC-32/MPEG-2).
 *
 * @param pData   Pointer to data buffer.
 * @param length  Number of bytes to calculate CRC32 over.
 * @return CRC32 checksum value.
 */
uint32_t AudioCfgCrc32Calculate(const uint8_t *pData, uint32_t length)
{
    if (pData == NULL || length == 0) {
        return 0;
    }
    return _crc32_internal(pData, length);
}

/* ========================================================================= */
/* Compile-time struct size verification                                        */
/* ========================================================================= */

/* Verify struct sizes match expectations at compile time.
 * These will generate compiler errors if sizes don't match.               */
typedef char _AudioGainBlock_size_check[
    (sizeof(AudioGainBlock) == AUDIO_GAIN_BLOCK_SIZE) ? 1 : -1];
typedef char _AudioConfigFileData_size_check[
    (sizeof(AudioConfigFileData) == AUDIO_CFG_FILE_SIZE) ? 1 : -1];
