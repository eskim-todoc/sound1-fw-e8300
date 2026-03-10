
#include <hw.h>
#include <bootloader.h>
#include <stdbool.h>

#include <main.h>
#include <ci_initialize.h>
#include <uart_printf.h>
#include <service.h>

int main(void)
{
    // Configure DIOs for SWJ-DP
    Sys_DIO_CM3JTAGConfig(true, false);

    // Unlock debugging
    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                            // I2C
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                              // Unlock
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk;  // SEGGER RTT-viewer

    // initialize interrupt
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);

    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();

    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    Sys_DIO_Config(DIO_NUM_DMIC_CLK1_CAL, DIO_CFG_CAL_STRONG_PULL_UP);  // DIO 설정: 디버거 모드 동작 체크
    Sys_DIO_Config(DIO_NUM_QCC_CTRL, DIO_CFG_QCC_CTRL);                 // DIO 설정: QCC CTRL
    Sys_DIO_Config(DIO_NUM_QCC_ISD_CHECK, DIO_CFG_QCC_ISD_CHECK);       // DIO 설정: 내부기 연결 상태

    Sys_GPIO_Set_Low(DIO_NUM_QCC_CTRL);       // QCC 절전모드
    Sys_GPIO_Set_Low(DIO_NUM_QCC_ISD_CHECK);  // 내부기 연결 안됨

    delay_us(10);  // CAL DIO의 올바른 풀업 설정을 위한 짧은 대기

    // DMIC_CLK1_CAL DIO 레벨에 따라 동작 설정됨
    if (Sys_GPIO_Read(DIO_NUM_DMIC_CLK1_CAL) == DIO_ACTIVE_LEVEL_CAL)
    {
        Sys_DIO_Config(DIO_NUM_DMIC_CLK1_CAL, DIO_CFG_CAL_NO_PULL);

#if 0
        // QCC의 USB 및 펌웨어 기본 동작 테스트를 위해서 CTRL과 ISD_CHECK를 강제로 1로 설정한 코드 (2026.03.04)
        Sys_GPIO_Set_High(DIO_NUM_QCC_CTRL);       // QCC 절전모드 해제
        Sys_GPIO_Set_High(DIO_NUM_QCC_ISD_CHECK);  // 내부기 연결 상태
#endif

        service_main();  // 서비스 모드 메인
    }
    else
    {
        Sys_DIO_Config(DIO_NUM_DMIC_CLK1_CAL, DIO_CFG_CAL_NO_PULL);  // DIO 설정: 디버거 모드 동작 체크 초기화

        // CFX와 CM3의 공유 메모리를 LPDSP32 PRAM5 베이스 주소로 설정했으며, 공유 메모리의 첫 멤버가 CFX_EEPROM_data_is_Loaded 이다.
        // CFX와 CM3의 부팅 시퀀스를 위해 CFX_EEPROM_data_is_Loaded를 0으로 초기화 한다.
        ((int *) DSP_PRAM5_REMAP_BASE)[0] = 0;

        bootloader_main(); // 부트로더 메인
    }

    return 0;
}
