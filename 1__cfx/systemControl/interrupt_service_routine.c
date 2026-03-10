/**
 * @file interrupt_service_routine.c
 */

#include <interrupt_service_routine.h>

volatile app_interrupt_flag_t chess_storage(XMEM) g_interrupt_flags = {
    .read_isd_info   = 0,
    .wake_up         = 0,
    .mic0            = 0,
    .mic1            = 0,
    .dac1            = 0,
    .pcm_out         = 0,
    .function_chain0 = 0,
    .function_chain1 = 0,
};

void app_configure_interrupts(int mode)
{
    disable_interrupts(); /* disable interrupts on the cfx by clearing 'sr.ie' */

    SYS_CFX_INT_MASTER_DISABLE(D_INT); /* disable master interrupt */
    SYS_CFX_INT_DISABLEALL(D_INT);     /* disable all interrupts prior to configuring and re-enabling them */

    SYS_CFX_INT_CLEARALLPENDING(D_INT); /* clear any pending interrupts */

    SYS_CFX_INT_PRIORITY(D_INT, 0, 0); /* configure the interrupt priority 0 */
    SYS_CFX_INT_PRIORITY(D_INT, 1, 0); /* configure the interrupt priority 1 */
    SYS_CFX_INT_PRIORITY(D_INT, 2, 0); /* configure the interrupt priority 2 */

    if (mode == CFX_INT_NORMAL)
    {
        D_INT->EBL_0  = INT0_EBL_WATCHDOG;
        D_INT->EBL_7  = (INT7_EBL_FIFO_4 | INT7_EBL_FIFO_3 | INT7_EBL_FIFO_1 | INT7_EBL_FIFO_0);
        D_INT->EBL_8  = (INT8_EBL_HEAR_1 | INT8_EBL_HEAR_0);
        D_INT->EBL_10 = INT10_EBL_CM3_0;
    }
    else /* OTE1_5GEN_INT_STANDBY */
    {
        D_INT->EBL_0  = INT0_EBL_WATCHDOG;
        D_INT->EBL_10 = INT10_EBL_CM3_1; /* This is to wake up! */
    }

    app_clear_all_interrupt_flags(); /* clear all interrupt flags */

    SYS_CFX_INT_MASTER_ENABLE(D_INT); /* enable master interrupt */

    enable_interrupts(); /* enable interrupts on the CFX by setting 'sr.ie' */
}

void app_clear_all_interrupt_flags(void)
{
    g_interrupt_flags.read_isd_info   = 0;
    g_interrupt_flags.wake_up         = 0;
    g_interrupt_flags.mic0            = 0;
    g_interrupt_flags.mic1            = 0;
    g_interrupt_flags.dac1            = 0;
    g_interrupt_flags.pcm_out         = 0;
    g_interrupt_flags.function_chain0 = 0;
    g_interrupt_flags.function_chain1 = 0;
}

extern "C" void HEAR_0_ISR() property(isr)
{
    g_interrupt_flags.function_chain0 = 1;
}

extern "C" void HEAR_1_ISR() property(isr)
{
    g_interrupt_flags.function_chain1 = 1;
}

///////////////////////////////////////////////////////////////////////////////////////

extern "C" void FIFO_0_ISR() property(isr)
{
    g_interrupt_flags.mic0 = 1;
}

extern "C" void FIFO_1_ISR() property(isr)
{
    g_interrupt_flags.mic1 = 1;
}

extern "C" void FIFO_3_ISR() property(isr)
{
    g_interrupt_flags.dac1 = 1;
}

extern "C" void FIFO_4_ISR() property(isr)
{
    g_interrupt_flags.pcm_out = 1;
}

///////////////////////////////////////////////////////////////////////////////////////

extern "C" void CM3_0_ISR() property(isr)
{
    g_interrupt_flags.read_isd_info = 1;
}

extern "C" void CM3_1_ISR() property(isr)
{
    g_interrupt_flags.wake_up = 1;
}
