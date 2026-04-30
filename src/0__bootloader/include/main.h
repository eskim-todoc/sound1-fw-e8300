
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
#define DIO_NUM_UART_ENABLE         DIO32
#define DIO_NUM_ELAPSE_TIME_CHECK   DIO33

#define DIO_CFG_LED                 (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define DIO_CFG_QCC_CTRL            (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define DIO_CFG_QCC_ISD_CHECK       (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define DIO_CFG_CAL_NO_PULL         (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define DIO_CFG_CAL_PULL_UP         (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN)
#define DIO_CFG_UART_ENABLE_NO_PULL (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define DIO_CFG_UART_ENABLE_PULL_UP (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN)
#define DIO_CFG_ELAPSE_TIME_CHECK   (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

#define DIO_ACTIVE_LEVEL_CAL         0
#define DIO_ACTIVE_LEVEL_UART_ENABLE 0

/* 부트로더 단계 LED 조기 점등 (작업: LED/bootloader-power-on-indicator).
 * =1: 활성 — UART 비활성 시 SW PWM SKYBLUE fade-in 점등
 * =0: 비활성 (디폴트, 기존 부트로더 동작 유지)
 * Eclipse 빌드 설정 / Makefile 에서 -DTDC_BOOT_LED_ENABLE=1 로 override. */
#ifndef TDC_BOOT_LED_ENABLE
#define TDC_BOOT_LED_ENABLE 0
#endif

void tdc_delay_ms(uint32_t ms);
void tdc_delay_us(uint32_t us);

#endif /* __esp_main_h__ */
