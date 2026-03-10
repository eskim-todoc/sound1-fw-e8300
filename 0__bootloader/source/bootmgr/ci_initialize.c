#include <ci_initialize.h>
#include <rtt_printf.h>
#include <uart_printf.h>

void delay_ms(uint32_t ms)
{
    Sys_Delay((SystemCoreClock / 1000) * ms);
}

void delay_us(uint32_t us)
{
    Sys_Delay((SystemCoreClock / 1000000) * us);
}

void initialize_late(void)
{
    // UART
    uart_init();
}

#if 0
void ci_initialize(void)
{
    // LEVEL SHIFTER
    //Sys_DIO_Config(DIO_LEVEL_SHIFTER, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));
    //Sys_GPIO_Set_High(DIO_LEVEL_SHIFTER);

    // SEGGER RTT
    Sys_DIO_Config(DIO_SWCLK, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));
    Sys_DIO_Config(DIO_SWDIO, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));

    Sys_DIO_CM3JTAGConfig(true, false);

    // UNLOCK DEBUGGING
    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                           // I2C
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                             // UNLOCK
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk; // RTT VIEWER

    // INTERRUPT ENABLE
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    // DOWNLOAD MODE
    //Sys_DIO_Config(DIO_DOWNLOAD_MODE, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN));

    ((int *) DSP_PRAM5_REMAP_BASE)[0] = 0; // CFX_EEPROM_data_is_Loaded
}
#endif
