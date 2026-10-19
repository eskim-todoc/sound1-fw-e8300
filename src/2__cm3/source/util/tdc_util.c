/**
 * @file tdc_util.c
 */

#include <tdc_util.h>

static void _critical_error_led_toggler(uint32_t cnt, uint32_t msec)
{
    uint32_t msec_1 = (SystemCoreClock / 1000);

    for (volatile int i = 0; i < cnt; i++)
    {
        Sys_GPIO_Toggle(DIO_PIN_INDEX_for_LED_color_R);
        Sys_Delay(msec_1 * msec);
        SYS_WATCHDOG_REFRESH();

        Sys_GPIO_Toggle(DIO_PIN_INDEX_for_LED_color_R);
        Sys_Delay(msec_1 * msec);
        SYS_WATCHDOG_REFRESH();
    }
}

void tdc_util_indicate_critical_error(void)
{
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_R, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // R
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_G, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // G
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_B, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // B

    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);  // R
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);  // G
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);  // B

    while (1)
    {
        _critical_error_led_toggler(40, 25);
        _critical_error_led_toggler(4, 150);
    }
}

void tdc_util_assert(int a)
{
    if (a != df_True)
    {
        tdc_util_indicate_critical_error();
    }
}

void tdc_util_delay_ms(uint32_t ms)
{
    Sys_Delay((SystemCoreClock / 1000) * ms);
}
