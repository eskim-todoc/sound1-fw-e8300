/**
 * @file SEGGER_RTT_Wrapper.c
 */

#include <SEGGER_RTT_Wrapper.h>

void SEGGER_RTT_Wrapper_Init(void)
{
    /* Enable 3.3V Level shifter */
    Sys_DIO_Config(DIO6, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));
    Sys_GPIO_Set_High(DIO6);

    /* Configure SWJ-DP */
    Sys_DIO_Config(DIO30, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_60K_PULL_UP | DIO_MODE_INPUT));
    Sys_DIO_Config(DIO31, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_60K_PULL_UP | DIO_MODE_INPUT));

    Sys_DIO_CM3JTAGConfig(true, false);

    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                           /* I2C */
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                             /* Unlock */
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk; /* RTT Viewer */

    /* Disable exceptions (except NMI and the hard fault exception) and interrupts before configuring interfaces and peripherals
     * by setting a 1 to the 1-bit interrupt mask register PRIMASK */
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);

    /* Clear the enable for all of the external interrupts. */
    Sys_NVIC_DisableAllInt();

    /* Clear the pending status for all of the external interrupts. */
    Sys_NVIC_ClearAllPendingInt();

    /* Un-mask exceptions and interrupts by setting a 0 to the 1-bit interrupt mask register PRIMASK */
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    /* Initialize SEGGER RTT */
    // SEGGER_RTT_Init();
}

void SEGGER_RTT_Wrapper_printf(const char *p_fmt, ...)
{
    static char buf[SEGGER_RTT_PRINTF_BUFFER_SIZE];
    int         len;
    va_list     ap;

    va_start(ap, p_fmt);

    len = vsnprintf(buf, SEGGER_RTT_PRINTF_BUFFER_SIZE, p_fmt, ap);

    va_end(ap);

    SEGGER_RTT_Write(0, buf, len);
}

int SEGGER_RTT_Wrapper_getch(char *p_buf)
{
    return SEGGER_RTT_Read(0, p_buf, 1);
}
