
#include <hw.h>
#include <bootloader.h>
#include <stdbool.h>

#include <main.h>
#include <ci_initialize.h>
#include <service.h>
#include <tdc_uart.h>

int main(void)
{
    // EEPROM의 WP을 방지 (쓰기 가능)
    // 그런데 실제로 사용되는 EEPROM 제품의 경우 리셋 디폴트가
    // WP 기능의 핀이 아니고 QSPI 기능으로 동작하는 제품임
    // 그래서 WP 핀 설정이 현재는 사실상 큰 의미 없음
    Sys_DIO_Config(DIO7, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN));

    // 디버깅 인터페이스 설정
    Sys_DIO_CM3JTAGConfig(true, false);                                 // SWJ-DP DIO 설정
    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                            // I2C 접근 제한 해제
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                              // 디버그 접근 해제
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk;  // SEGGER RTT 뷰어 접근 해제 (메모리 접근)

    // 인터럽트 초기화
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    // 필수 DIO 설정
    Sys_DIO_Config(DIO_NUM_QCC_CTRL, DIO_CFG_QCC_CTRL);            // DIO 설정: QCC CTRL
    Sys_DIO_Config(DIO_NUM_QCC_ISD_CHECK, DIO_CFG_QCC_ISD_CHECK);  // DIO 설정: 내부기 연결 상태
    Sys_GPIO_Set_Low(DIO_NUM_QCC_CTRL);                            // QCC 절전모드
    Sys_GPIO_Set_Low(DIO_NUM_QCC_ISD_CHECK);                       // 내부기 연결 안됨

    // DIO 설정: UART 사용 여부
    Sys_DIO_Config(DIO_NUM_UART_ENABLE, DIO_CFG_UART_ENABLE_PULL_UP);  // DIO 설정: UART 사용 여부 체크

    // 서비스 모드 진입 여부
    Sys_DIO_Config(DIO_NUM_DMIC_CLK1_CAL, DIO_CFG_CAL_PULL_UP);  // DIO 설정: 디버거 모드 동작 체크

    tdc_delay_us(10);  // 각종 DIO의 올바른 풀업 설정을 위한 짧은 대기

    // UART 사용 여부 검증
    tdc_uart_set_enable(Sys_GPIO_Read(DIO_NUM_UART_ENABLE) == DIO_ACTIVE_LEVEL_UART_ENABLE);
    tdc_uart_init();
    tdc_uart_printf("\r\n\n\n");

    // DMIC_CLK1_CAL DIO 레벨에 따라 동작 설정됨
    if (Sys_GPIO_Read(DIO_NUM_DMIC_CLK1_CAL) == DIO_ACTIVE_LEVEL_CAL)
    {
        tdc_delay_us(10);  // 확실하게 하기 위해 10 us 딜레이 후 다시 검증

        if (Sys_GPIO_Read(DIO_NUM_DMIC_CLK1_CAL) == DIO_ACTIVE_LEVEL_CAL)
        {
            Sys_DIO_Config(DIO_NUM_DMIC_CLK1_CAL, DIO_CFG_CAL_NO_PULL);  // DIO 설정: 디버거 모드 동작 체크 초기화

            service_main();  // 서비스 모드 메인
        }
    }
    else
    {
        Sys_DIO_Config(DIO_NUM_DMIC_CLK1_CAL, DIO_CFG_CAL_NO_PULL);  // DIO 설정: 디버거 모드 동작 체크 초기화

        // CFX와 CM3의 공유 메모리는 LPDSP32 PRAM5 베이스 주소를 사용함
        // 공유 메모리의 첫 멤버는 CFX_EEPROM_data_is_Loaded 이며,
        // CFX와 CM3의 부팅 시퀀스를 위해 CFX_EEPROM_data_is_Loaded를 0으로 초기화 시킴
        ((int *) DSP_PRAM5_REMAP_BASE)[0] = 0;

        bootloader_main(); // 부트로더 메인
    }

    return 0;
}
