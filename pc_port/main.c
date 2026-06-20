/*
 * main.c — PC entry point for the AudioConf TCP server.
 *
 * Responsibilities:
 *   1. Winsock2 initialisation on Windows
 *   2. Flash file initialisation (create audio_flash.bin with defaults if absent)
 *   3. Call AudioConf_Init() which spawns AudioConfigTask in a background thread
 *   4. Keep the main thread alive so the process doesn't exit
 *
 * The flash file path is "audio_flash.bin" in the current working directory.
 * The TCP server listens on port 5558 (same as embedded target).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Force-include our vxWorks types and logLib before anything else */
#include "vxWorks.h"
#include "taskLib.h"

/* AudioConf module */
#include "AudioConf/AudioConf.h"
#include "AudioConf/AudioParamDef.h"
#include "AudioConf/AudioFlashMgr.h"
#include "e2map.h"          /* DA2_9651_BLOCK_LEN (1280) */

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <signal.h>
#else
#  include <signal.h>
#endif

#ifndef AUDIO_FLASH_FILE
#  define AUDIO_FLASH_FILE "audio_flash.bin"
#endif

/* ── Flash initialisation ─────────────────────────────────────────────────── */

/*
 * If audio_flash.bin doesn't exist or is smaller than DA2_9651_BLOCK_LEN
 * (1280 bytes), write a fresh full-size file: AudioConfigFileData defaults
 * in the first 300 bytes, zeros for the remainder.
 *
 * Must match the size check in flash_stubs.c:_ensure_file_exists() which
 * also tests against DA2_9651_BLOCK_LEN — if main.c writes fewer bytes,
 * _ensure_file_exists() overwrites the defaults with all zeros on the first
 * ReadDA2_9651ParameterBlock call, corrupting the CRC and every parameter.
 */
static void init_flash_if_needed(void)
{
    FILE *f = fopen(AUDIO_FLASH_FILE, "rb");
    int   need_init = 1;

    if (f) {
        long sz;
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fclose(f);
        if (sz >= (long)DA2_9651_BLOCK_LEN)
            need_init = 0;
    }

    if (!need_init) {
        printf("[INIT] Flash file exists (%s, %d bytes)\n",
               AUDIO_FLASH_FILE, DA2_9651_BLOCK_LEN);
        return;
    }

    {
        /* Build a full 1280-byte flash image: struct (300 B) + zero padding */
        uint8_t flash_image[DA2_9651_BLOCK_LEN];
        AudioConfigFileData data;
        size_t nw;

        memset(flash_image, 0, sizeof(flash_image));
        AudioConfigFileDefault(&data);
        memcpy(flash_image, &data, sizeof(data));   /* 300 bytes at offset 0 */

        f = fopen(AUDIO_FLASH_FILE, "wb");
        if (!f) {
            fprintf(stderr, "[INIT] ERROR: cannot create %s\n", AUDIO_FLASH_FILE);
            return;
        }
        nw = fwrite(flash_image, 1, sizeof(flash_image), f);
        fclose(f);

        if (nw == sizeof(flash_image)) {
            printf("[INIT] Initialised %s with default parameters (%zu bytes)\n",
                   AUDIO_FLASH_FILE, nw);
        } else {
            fprintf(stderr, "[INIT] WARNING: short write to %s (%zu/%zu)\n",
                    AUDIO_FLASH_FILE, nw, sizeof(flash_image));
        }
    }
}

/* ── Signal handler for clean shutdown ────────────────────────────────────── */

static volatile int g_running = 1;

static void on_sigint(int sig)
{
    (void)sig;
    printf("\n[MAIN] Shutting down...\n");
    g_running = 0;
}

/* ── Entry point ──────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=============================================================\n");
    printf("  MPC866 Audio DSP Configuration Server — PC Port\n");
    printf("  Listening on TCP port 5558\n");
    printf("  Flash file: %s\n", AUDIO_FLASH_FILE);
    printf("  Press Ctrl+C to stop.\n");
    printf("=============================================================\n");

#ifdef _WIN32
    {
        WSADATA wsa;
        int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rc != 0) {
            fprintf(stderr, "[MAIN] WSAStartup failed: %d\n", rc);
            return 1;
        }
        printf("[INIT] Winsock2 %d.%d initialised\n",
               LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));
    }
#endif

    signal(SIGINT,  on_sigint);
#ifdef SIGTERM
    signal(SIGTERM, on_sigint);
#endif

    init_flash_if_needed();

    if (AudioConf_Init() != OK) {
        fprintf(stderr, "[MAIN] AudioConf_Init() failed — exiting\n");
        return 1;
    }

    printf("[MAIN] AudioConfigTask running.  Waiting for PC UI connection...\n");

    /* Keep main thread alive while AudioConfigTask handles requests */
    while (g_running) {
        taskDelay(60);   /* sleep 1 s (60 ticks at 60 Hz) */
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
