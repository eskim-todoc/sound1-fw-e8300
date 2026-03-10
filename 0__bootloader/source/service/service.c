
#include <service.h>
#include <main.h>

void service_main(void)
{
    esp_debugger_led_t debugger_led;

    Sys_DIO_Config(DIO_NUM_LED_R_UART_TX_E8300, DIO_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_G_UART_RX_E8300, DIO_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_B, DIO_CFG_LED);

    debugger_led.raw = 0;  // start with black color

    while (1)
    {
        for (int i = 0; i < 8; i++)
        {
            debugger_led.raw = i;

            Sys_GPIO_Write(DIO_NUM_LED_R_UART_TX_E8300, debugger_led.element.r);
            Sys_GPIO_Write(DIO_NUM_LED_G_UART_RX_E8300, debugger_led.element.g);
            Sys_GPIO_Write(DIO_NUM_LED_B, debugger_led.element.b);

            SYS_WATCHDOG_REFRESH();
            delay_ms(380);
        }
    }
}
