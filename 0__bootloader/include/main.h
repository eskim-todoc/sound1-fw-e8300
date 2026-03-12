
#ifndef __esp_main_h__
#define __esp_main_h__

#include <hw.h>
#include <stdbool.h>

#include <bootloader.h>

#define DIO_NUM_LED_R_UART_TX_E8300 DIO8
#define DIO_NUM_LED_G_UART_RX_E8300 DIO15
#define DIO_NUM_LED_B               DIO12
#define DIO_NUM_QCC_CTRL            DIO24
#define DIO_NUM_QCC_ISD_CHECK       DIO13
#define DIO_NUM_DMIC_CLK1_CAL       DIO22

#define DIO_CFG_LED                (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define DIO_CFG_QCC_CTRL           (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define DIO_CFG_QCC_ISD_CHECK      (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define DIO_CFG_CAL_NO_PULL        (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define DIO_CFG_CAL_STRONG_PULL_UP (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_STRONG_PULL_UP | DIO_MODE_GPIO_IN)

#define DIO_ACTIVE_LEVEL_CAL 0

void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif /* __esp_main_h__ */
