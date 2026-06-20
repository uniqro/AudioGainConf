#pragma once
/*
 * Flash memory map constants — PC stub.
 * On embedded hardware these describe the NOR Flash address space.
 * Here we just provide the size constant used for bounds checking.
 */

#ifndef DA2_9651_BLOCK_LEN
#  define DA2_9651_BLOCK_LEN  0x500   /* 1280 bytes */
#endif

#ifndef DA2_9651_BLOCK_LOC
#  define DA2_9651_BLOCK_LOC  0x00000
#endif
