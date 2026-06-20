/*
 * spi_stubs.c — SPI link layer stub for PC build.
 *
 * On the embedded target (MPC866) addCommand() queues a byte into the SPI
 * DMA transmit ring buffer for transmission to the C5416 DSP.
 *
 * On PC we just log the complete 4-byte packet to stderr when all bytes of a
 * dispatch have been received:
 *   [SPI] CMD=0xC1  MODE=0x02  GAIN=0x28  CKSUM=0x0F
 *
 * The AudioSpiGateway dispatch sequence is:
 *   1. addingCommands = TRUE
 *   2. save globalModemTxBuf.head to globalModemTxBuf_head_shadow
 *   3. addCommand(CMD), addCommand(MODE), addCommand(GAIN), addCommand(CKSUM)
 *   4. restore globalModemTxBuf.head = globalModemTxBuf_head_shadow
 *   5. addingCommands = FALSE
 *
 * We detect the 4-byte boundary and log on the 4th byte.
 */

#include "96SK/DevreAnahtarlama/spi/spi_lnk.h"
#include "vxWorks.h"
#include <stdio.h>

/* ── Global variable definitions (extern in spi_lnk.h) ───────────────────── */

int            addingCommands               = FALSE;
uint16_t       globalModemTxBuf_head_shadow = 0;
ModemTxBufType globalModemTxBuf             = {{0}, 0, 0};

/* ── addCommand implementation ────────────────────────────────────────────── */

void addCommand(uint8_t command)
{
    static uint8_t  pkt[4];
    static int      pkt_len = 0;

    /* Store byte at current head position */
    globalModemTxBuf.data[globalModemTxBuf.head & 0xFF] = command;
    globalModemTxBuf.head++;

    /* Accumulate bytes for logging */
    if (pkt_len < 4) {
        pkt[pkt_len] = command;
    }
    pkt_len++;

    /* Complete 4-byte SPI packet: CMD + MODE + GAIN + CKSUM */
    if (pkt_len >= 4) {
        fprintf(stderr,
                "[SPI] CMD=0x%02X  MODE=0x%02X  GAIN=0x%02X  CKSUM=0x%02X\n",
                pkt[0], pkt[1], pkt[2], pkt[3]);
        pkt_len = 0;
    }
}
