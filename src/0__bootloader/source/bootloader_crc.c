/**
 * @file bootloader_crc.c
 * @brief Implementation of the bootloader_CRC_calc function.
 *
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

#include <stdint.h>
#include <hw.h>
#include <crc.h>

uint32_t bootloader_CRC_calc(uint32_t *data, uint32_t size)
{
    uint32_t i;

    Sys_Set_CRC_Config(CRC,
                       CRC_LITTLE_ENDIAN        |
                       0                        |
                       CRC_BIT_ORDER_STANDARD   |
                       CRC_FINAL_XOR_STANDARD );

    Sys_CRC_CCITTInitValue(CRC);

    for(i = 0; i < (size>>2); i++)
    {
        Sys_CRC_Add(CRC, data[i], 32);
    }
    for(i = 0; i < (size&03U); i++)
    {
        Sys_CRC_Add(CRC, ((data[size>>2])>>((i&0x03UL)<<3))&0xFFUL, 8);
    }
    return Sys_CRC_GetFinalValue(CRC);
}
