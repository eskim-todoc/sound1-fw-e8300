/**
 * @file todoc_cfx_uart.h
 */

#ifndef __todoc_cfx_uart_h__
#define __todoc_cfx_uart_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <definitionsForAlgorithm.h>
#include <microcode.h>

typedef struct
{
    int state;
    int flag[32];
    int buffer[512];
} FS_MEM_UART_T;

#define FS_MEM_UART ((volatile FS_MEM_UART_T chess_storage(IOMEM) *) D_DSP_PRAM1_BASE)

#endif  // __todoc_cfx_uart_h__
