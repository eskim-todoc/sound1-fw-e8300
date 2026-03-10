/**
 * @file interruptServiceRoutine.h
 */

#ifndef __interrupt_service_routine_h__
#define __interrupt_service_routine_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define CFX_INT_STANDBY 0
#define CFX_INT_NORMAL  1

typedef struct
{
    int read_isd_info;
    int wake_up;
    int mic0;
    int mic1;
    int dac1;
    int pcm_out;
    int function_chain0;
    int function_chain1;
} app_interrupt_flag_t;

void app_configure_interrupts(int mode);
void app_clear_all_interrupt_flags(void);

extern volatile app_interrupt_flag_t chess_storage(XMEM) g_interrupt_flags;

#endif // __interrupt_service_routine_h__
