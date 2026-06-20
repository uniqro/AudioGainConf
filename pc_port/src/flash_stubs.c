/*
 * flash_stubs.c — DA2_9651 flash block emulation via a local binary file.
 *
 * ReadDA2_9651ParameterBlock / WriteDA2_9651ParameterBlock
 *   → fread / fwrite on "audio_flash.bin" in the current working directory.
 *
 * The file is 1280 bytes (DA2_9651_BLOCK_LEN = 0x500) to match the real
 * flash region. It is created with zeros on first write; AudioConf_Init()
 * in main.c initialises it with valid defaults before the TCP task starts.
 *
 * Endianness note
 * ───────────────
 * The embedded target (MPC866, PowerPC) is big-endian.  The uint32_t crc32
 * field in AudioConfigFileData is stored in native byte order — big-endian
 * on the target, little-endian on x86.  Binary files are therefore NOT
 * interchangeable between the embedded target and this PC port without
 * byte-swapping the crc32 field.  For standalone PC testing this is not a
 * problem because the C code writes and reads the file with the same
 * endianness.
 */

#include "ParameterExp.h"
#include "e2map.h"
#include "vxWorks.h"
#include <stdio.h>
#include <string.h>

#ifndef AUDIO_FLASH_FILE
#  define AUDIO_FLASH_FILE "audio_flash.bin"
#endif

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static FILE *_open_flash(const char *mode)
{
    FILE *f = fopen(AUDIO_FLASH_FILE, mode);
    return f;
}

/* Create an all-zero flash file of the correct size if it doesn't exist yet */
static void _ensure_file_exists(void)
{
    FILE *f = _open_flash("rb");
    if (f) {
        long sz;
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fclose(f);
        if (sz >= (long)DA2_9651_BLOCK_LEN)
            return;   /* already the right size */
    }
    /* Create / resize */
    f = _open_flash("wb");
    if (f) {
        uint8_t zeros[DA2_9651_BLOCK_LEN];
        memset(zeros, 0, sizeof(zeros));
        fwrite(zeros, 1, DA2_9651_BLOCK_LEN, f);
        fclose(f);
        printf("[FLASH] Created %s (%d bytes, zeroed)\n",
               AUDIO_FLASH_FILE, DA2_9651_BLOCK_LEN);
    } else {
        fprintf(stderr, "[FLASH] ERROR: cannot create %s\n", AUDIO_FLASH_FILE);
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int ReadDA2_9651ParameterBlock(int offset, uint8_t *dest, int size)
{
    FILE *f;

    if (!dest || size <= 0)
        return ERROR;

    _ensure_file_exists();

    f = _open_flash("rb");
    if (!f) {
        fprintf(stderr, "[FLASH] ReadDA2: cannot open %s\n", AUDIO_FLASH_FILE);
        return ERROR;
    }

    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        fclose(f);
        return ERROR;
    }

    if ((int)fread(dest, 1, (size_t)size, f) != size) {
        fclose(f);
        return ERROR;
    }

    fclose(f);
    return OK;
}

int WriteDA2_9651ParameterBlock(int offset, const uint8_t *src, int size)
{
    FILE *f;

    if (!src || size <= 0)
        return ERROR;

    _ensure_file_exists();

    f = _open_flash("r+b");   /* file must already exist */
    if (!f) {
        fprintf(stderr, "[FLASH] WriteDA2: cannot open %s for writing\n",
                AUDIO_FLASH_FILE);
        return ERROR;
    }

    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        fclose(f);
        return ERROR;
    }

    if ((int)fwrite(src, 1, (size_t)size, f) != size) {
        fclose(f);
        return ERROR;
    }

    fflush(f);
    fclose(f);
    printf("[FLASH] Write %d bytes at offset %d\n", size, offset);
    return OK;
}
