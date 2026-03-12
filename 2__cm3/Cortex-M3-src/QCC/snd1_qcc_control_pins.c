/**
 * @file snd1_qcc_control_pins.c
 */

#include "snd1_qcc_control_pins.h"

void snd1_qcc_init_control_pins(void)
{
    Sys_DIO_Config(SND1_PIN_QCC_CTRL, SND1_PIN_GPIO_OUT_CFG);
    Sys_DIO_Config(SND1_PIN_ISD_CHECK, SND1_PIN_GPIO_OUT_CFG);

    snd1_qcc_isd_connection(false);
    snd1_qcc_shutdown(true);
}

void snd1_qcc_isd_connection(bool connection)
{
#if (SND1_QCC_ISD_CONN_ACTIVE_LEVEL == 1)
    if (connection)
    {
        Sys_GPIO_Set_High(SND1_PIN_ISD_CHECK);
    }
    else
    {
        Sys_GPIO_Set_Low(SND1_PIN_ISD_CHECK);
    }
#else
    if (connection)
    {
        Sys_GPIO_Set_Low(SND1_PIN_ISD_CHECK);
    }
    else
    {
        Sys_GPIO_Set_High(SND1_PIN_ISD_CHECK);
    }
#endif
}

void snd1_qcc_shutdown(bool shutdown)
{
#if (SND1_QCC_SHUTDOWN_ACTIVE_LEVEL == 1)
    if (shutdown)
    {
        Sys_GPIO_Set_High(SND1_PIN_QCC_CTRL);
    }
    else
    {
        Sys_GPIO_Set_Low(SND1_PIN_QCC_CTRL);
    }
#else
    if (shutdown)
    {
        Sys_GPIO_Set_Low(SND1_PIN_QCC_CTRL);
    }
    else
    {
        Sys_GPIO_Set_High(SND1_PIN_QCC_CTRL);
    }
#endif
}
