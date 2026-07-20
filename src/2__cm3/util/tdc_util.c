/**
 * @file tdc_util.c
 */

#include <tdc_util.h>
#include <tdc_hal_uart.h>

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

void tdc_util_delay_ms_long_time(uint32_t ms, uint32_t cnt)
{
    tdc_hal_uart_printf("[DELAY] ");

    for (int i = 0; i < cnt; i++)
    {
        tdc_util_delay_ms(ms);
        SYS_WATCHDOG_REFRESH();

        if (i == (cnt - 1))
        {
            tdc_hal_uart_printf("%u (ms) \r\n", (i + 1) * 100);
        }
        else
        {
            tdc_hal_uart_printf("%u, ", (i + 1) * 100);
        }
    }
    SYS_WATCHDOG_REFRESH();
}

void tdc_util_print_sysvar_manu_table(void)
{
    uint32_t *p;

    p = (uint32_t *) SYSVAR_MANU_TABLE;

    tdc_hal_uart_printf("SYSVAR_MANU_TABLE = \r\n");

    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_hal_uart_printf("0x%08X ", p[i]);

        if (((i + 1) % 4) == 0)
        {
            tdc_hal_uart_printf("\r\n");
        }
    }

    tdc_hal_uart_printf("\r\n");
}
