#pragma once
/*
 * Flash parameter block API stub.
 * Implementation in pc_port/src/flash_stubs.c
 * On embedded hardware these call into the DA2_9651 NOR flash driver.
 */
#include <stdint.h>

int ReadDA2_9651ParameterBlock(int offset, uint8_t *dest, int size);
int WriteDA2_9651ParameterBlock(int offset, const uint8_t *src, int size);
