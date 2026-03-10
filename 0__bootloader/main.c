/**
 * @file main.c
 * @brief Calls the main bootloader function.
 *
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

#include <bootloader.h>
#include <hw.h>
#include <stdbool.h>
#include <stddef.h>

#define DIO_NUM_LED_RED   DIO24
#define DIO_NUM_LED_GREEN DIO22
#define DIO_NUM_LED_BLUE  DIO29

#define DIO_NUM_NRF_ON_OFF_COMMAND DIO16
#define DIO_NUM_NRF_SWDIO_NRESET   DIO35
#define DIO_NUM_UART_RX            DIO21

#define DIO_NUM_CALIBRATION DIO19

#define PIN_CFG_UART_RX            (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define PIN_CFG_SWDIO_NRESET       (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define PIN_CFG_LED                (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define PIN_CFG_NRF_ON_OFF_COMMAND (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define PIN_CFG_CALIBATION         (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN)
#define PIN_CFG_CALIBATION_RESET   (DIO_8X_DRIVE | DIO_LPF_DISABLE | DIO_250K_PULL_UP | DIO_MODE_DISABLE)

#define PIN_ACTIVE_LEVEL_SWDIO_NRESET  0
#define PIN_ACTIVE_LEVEL_DEBUGGER_MODE 0

void debugger_main(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

int main(void)
{
    // LED OFF를 가장 먼저 수행
    Sys_GPIO_Set_Low(DIO_NUM_LED_RED);
    Sys_GPIO_Set_Low(DIO_NUM_LED_GREEN);
    Sys_GPIO_Set_Low(DIO_NUM_LED_BLUE);

    Sys_DIO_Config(DIO_NUM_LED_RED, PIN_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_GREEN, PIN_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_BLUE, PIN_CFG_LED);

    // UART RX핀을 디버거 모드 진입 여부를 판단하는데 사용
    Sys_DIO_Config(DIO_NUM_UART_RX, PIN_CFG_UART_RX);

    // 캘리브레이션 핀을 Weak Pullup으로 설정, 디버거 모드 진입 여부를 판단하는데 사용
    Sys_DIO_Config(DIO_NUM_CALIBRATION, PIN_CFG_CALIBATION);

    // SWJ-DP에 대한 DIO 설정
    Sys_DIO_CM3JTAGConfig(true, false);

    // 디버거 접근 제한 해제
    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                            // I2C
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                              // UNLOCK
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk;  // SEGGER RTT VIEWER

    // 인터럽트 상태 초기화
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    // nRF OFF로 설정
    Sys_DIO_Config(DIO_NUM_NRF_ON_OFF_COMMAND, PIN_CFG_NRF_ON_OFF_COMMAND);
    Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);

    delay_us(10);  // 캘리브레이션 핀의 설정을 위한 핀 레벨 안정화 시간

    if ((Sys_GPIO_Read(DIO_NUM_CALIBRATION) == PIN_ACTIVE_LEVEL_DEBUGGER_MODE) || (Sys_GPIO_Read(DIO_NUM_UART_RX) == PIN_ACTIVE_LEVEL_DEBUGGER_MODE))
    {
        debugger_main();
    }

    // CFX와 CM3의 공유 메모리를 LPDSP32 PRAM5 베이스 주소로 설정했으며, 공유 메모리의 첫 멤버가 CFX_EEPROM_data_is_Loaded 이다.
    // CFX와 CM3의 부팅 시퀀스를 위해 CFX_EEPROM_data_is_Loaded를 0으로 초기화 한다.
    ((int *) DSP_PRAM5_REMAP_BASE)[0] = 0;

    Sys_GPIO_Set_High(DIO_NUM_LED_GREEN);
    Sys_GPIO_Set_High(DIO_NUM_LED_BLUE);

    bootloader_main();

    return 0;
}

void debugger_main(void)
{
    int  color = 0;
    bool red   = false;
    bool green = false;
    bool blue  = false;

    // nRF ON으로 설정
    Sys_GPIO_Set_High(DIO_NUM_NRF_ON_OFF_COMMAND);

    // 캘리브레이션 핀 초기화
    Sys_DIO_Config(DIO_NUM_CALIBRATION, PIN_CFG_CALIBATION_RESET);

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        red   = (color >> 0) & 1;
        green = (color >> 1) & 1;
        blue  = (color >> 2) & 1;

        color = (color + 1) % 8;

        Sys_GPIO_Write(DIO_NUM_LED_RED, red);
        Sys_GPIO_Write(DIO_NUM_LED_GREEN, green);
        Sys_GPIO_Write(DIO_NUM_LED_BLUE, blue);

        delay_ms(500);
    }
}

void delay_ms(uint32_t ms)
{
    Sys_Delay((SystemCoreClock / 1000) * ms);
}

void delay_us(uint32_t us)
{
    Sys_Delay((SystemCoreClock / 1000000) * us);
}
