/* ----------------------------------------------------------------------------
 * Copyright (c) 2012 Semiconductor Components Industries, LLC
 * (d/b/a ON Semiconductor). All rights reserved.
 *
 * This code is the property of ON Semiconductor and may not be redistributed
 * in any form without prior written permission from ON Semiconductor. The
 * terms of use and warranty for this code are covered by contractual
 * agreements between ON Semiconductor and the licensee.
 * ----------------------------------------------------------------------------
 * initialize.c
 *  - Initialization source file
 * ----------------------------------------------------------------------------
 * $Revision: 1.14 $
 * $Date: 2012/10/01 17:23:54 $
 * ------------------------------------------------------------------------- */

#include <hw.h>
#include <stdbool.h>

#include "board.h"
#include "driver_SPI.h"
#include "driver_cfx_i2c.h"
#include "driver_i2c.h"

#include "batteryNPowerControl.h"

#include "cfx_cm3_sharedMemory.h"

#include "driver_MIS2DH.h"
#include "error.h"
#include "systemControl.h"

#include "isd_interface.h"
#include "mappingControl.h"
#include "remoteControl.h"

#include "LedOutput.h"
#include "indicatorByStimul.h"
#include "stimulationParaCal.h"

#if defined(Board_is_OTE_VER_1_2)
#include "driver_REN_ISL91128.h"
#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#include "driver_REN_ISL9122.h"
#elif defined(Board_is_OTE_VER_1_3)
#include "driver_REN_ISL98608.h"
#else
#error Link PMIC is NOT selected.
#endif

#include "processorDirective.h"

#include <driver_MAX17262.h>

#include <ci_dio.h>
#include <ci_power.h>
#include <ci_uart.h>
#include <ci_util.h>
#include <ci_filesystem.h>
#include <ci_map.h>
#include <ci_fft.h>
#include <ci_stim_mute.h>
#include <ci_event_log.h>
#include <ci_battery.h>
#include <ci_power.h>
#include <ci_printf.h>
#include <ci_boot.h>
#include <ci_timer.h>

#include <tdc_touch.h>

#include <snd_qcc.h>

void ResetNRF(void)
{
    // Sys_GPIO_Set_High(DIO_NUM_NRF_SWDIO_NRESET);

    for (volatile int i = 0; i < 200; i++)
    {
        __NOP();
    }

    // Sys_GPIO_Set_Low(DIO_NUM_NRF_SWDIO_NRESET);

    for (volatile int i = 0; i < 200; i++)
    {
        __NOP();
    }

    // Sys_GPIO_Set_High(DIO_NUM_NRF_SWDIO_NRESET);
}

void reset_interrupt_Disable_PRIMASK(void)
{
    // 인터럽트 리셋

    /* Disable exceptions (except NMI and the hard fault exception) and
     * interrupts before configuring interfaces and peripherals by setting a
     * 1 to the 1-bit interrupt mask register PRIMASK */
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);

    /* Clear the enable for all of the external interrupts. */
    Sys_NVIC_DisableAllInt();

    /* Clear the pending status for all of the external interrupts. */
    Sys_NVIC_ClearAllPendingInt();
}

void reset_DMA_disable(void)
{
    /* Disable all DMAs */
    Sys_DMA_Mode_Enable(DMA0, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA1, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA2, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA3, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA4, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA5, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA6, DMA_DISABLE);
    Sys_DMA_Mode_Enable(DMA7, DMA_DISABLE);

    /* Clear the DMA status */
    Sys_DMA_Clear_Status(DMA0, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA1, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA2, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA3, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA4, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA5, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA6, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
    Sys_DMA_Clear_Status(DMA7, (DMA_COMPLETE_INT_CLEAR | DMA_CNT_INT_CLEAR));
}

void enable_CFX_trigger_for_iteration(void)
{
    // cfx의 인터럽트를 사용하여 타이머 인터럽트처럼 사용한다.
    NVIC_ClearPendingIRQ(CFX_0_IRQn);
    NVIC_SetPriority(CFX_0_IRQn, 3);  // Highest priority
    NVIC_EnableIRQ(CFX_0_IRQn);

    NVIC_ClearPendingIRQ(FIFO_5_IRQn);
    NVIC_SetPriority(FIFO_5_IRQn, 3);
    NVIC_EnableIRQ(FIFO_5_IRQn);

    // NVIC_EnableIRQ(CFX_1_IRQn);
    // NVIC_EnableIRQ(CFX_2_IRQn);
}

void enable_interrupt(void)
{
    /* Un-mask exceptions and interrupts by setting a 0 to the 1-bit interrupt
     * mask register PRIMASK */
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);
}

void cm3MemorySetupCompleted(void)
{
    // CFX에 인터럽트 발생
    SYSCTRL_CFX_CMD->CFX_CMD_0_ALIAS = 1;  // CFX에 메모리 초기화가 완료되었음을 알려준다.
}

void Uninitialize(void)
{
    /* PRIMASK is a 1-bit register. When this is set, it allows NMI and the hard fault exception;
     * all other interrupts and exceptions are masked;
     * default is 0 (0: no masking, 1: masking) */
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);

    /* FAULTMASK is a 1-bit register.
     * When this is set, it allows only the NMI, and all interrupts and fault handling exceptions are disabled;
     * default is 0 (0: no masking, 1: masking) */
    __set_FAULTMASK(FAULTMASK_ENABLE_INTERRUPTS);

    /* Clear PRIMASK (no masking) */
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    /* Disable all existing interrupts. */
    Sys_NVIC_DisableAllInt();

    /* Clear all pending source. */
    Sys_NVIC_ClearAllPendingInt();

    /* LED arbiter ISR 도 같이 비활성 ? turnOffLED() 가 즉시 OFF 분기로 진입 */
    led_isr_active_set(false);

    /* Turn off the LED */
    turnOffLED();

    /* Clear all error flags */
    clearAllErrorFlag();

    /* Disable LSAD */
    LSAD->CFG = LSAD_DISABLE;

    /* Disable SPI */
    Sys_SPI_TransferConfig(SPI1, DRIVER_SPI_CTRL_DISABLE);

    /* Disable DMA */
    reset_DMA_disable();

    /* Disable I2C */
    enableI2cInterface(false);

    /* Disable UART */
    ci_uart_uninit();

    /* Reset DIOs */
    ci_dio_configure_sleep();
}

void error_toggler(int cnt, int msec)
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

void error_blink(void)
{
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_R, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // R
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_G, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // G
    Sys_DIO_Config(DIO_PIN_INDEX_for_LED_color_B, DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP);  // B

    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);  // R
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);  // G
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);  // B

    while (1)
    {
        error_toggler(40, 25);
        error_toggler(4, 150);
    }
}

/* proc_touch(), iqs323_init() → tdc_touch.c 로 이동됨 */

void Initialize(void)
{
    int counter = 0;
    int ret;

    // 8MB 플래시를 하위 4MB, 상위 4MB로 분할하여 사용한다.
    // 하위 4MB에는 Boot Info, Manufacture Data, FAT, Manifest, App0, App1, App2로 구성된다.
    // 상위 4MB에는 MANUF_TABLE, BATT_CAL, FFT Window, FFT Pass Bin 및 내부기 맵 데이터로 구성된다.
    // 상위 4MB에 구성되는 MANUF_TABLE은 보드 캘리브레이션 시 하위 4MB의 Manufacture Data에 저장되는
    // 캘리브레이션 정보를 상위 4MB에 파일로 백업시킨 정보이다.
    // 매번 펌웨어 이미지를 새로 다운로드 받게 되면, 하위 4MB가 초기화 되며, 이로 인해 매번 캘리브레이션을
    // 다시 수행해야 하는 이슈가 있다.
    // 그래서 캘리브레이션을 한 번만 수행하고 이후로는 이 MANUF_TABLE 정보를 활용하도록 구성하였다.

    ci_util_assert(ci_filesystem_nvm_init());  // NVM 인터페이스 초기화
    ci_util_assert(snd_fatfs_init_mem_map());  // FFT 및 맵 관련 공유 메모리 포인터 초기화
    ci_util_assert(snd_fatfs_remount(1));      // 사용자 드라이브(1)로 마운트
    ci_util_assert(ci_power_normal());         // 전원 및 클럭 설정

    ci_printi("[INIT] POWER NORMAL, CLOCK : %u HZ \r\n", SystemCoreClock);

    ci_dio_configure_normal();

    ci_printv("[INIT] INIT : DIO, UART, ETC.. \r\n");

    /* ====================================================================
     * LED 진입 게이트 ? POWER_ON LED 조기 점등 (P3-Early)
     *
     * 이 시점부터 TIMER3 가 1ms 주기로 g_tdc_timer_t3_tick 증가 + LED arbiter 구동.
     * 이후 ParLED 단계 (FS · NRF · DMA · I2C · SPI 등) 를 진행하는 동안에도
     * LED POWER_ON 버스트가 백그라운드로 출력되어 체감 부팅 시간을 단축한다.
     *
     * 상세: docs/tasks/LED/power-on-early-lighting/구현계획.md §3.2
     * ==================================================================== */

    /* L1: TIMER3 ISR 활성 (1ms tick ? LED · 터치 공유 카운터 + LED arbiter) */
    ci_timer_init(OTE_1_5_GEN_TIMER_TICK_1MS_PM_NORMAL);
    ci_printi("[MILESTONE] LED-GATE-ENTER (TIMER3 ON) \r\n");

    /* L2: 공유 메모리 주소 검증 ? 에러 시 LED 켜기 전에 무한루프 진입 (R4) */
    if (sharedMemoryAddresError())
    {
        ci_printe("[INFO] INVALID SHARED MEMORY ADDRESS \r\n");

        while (1)
        {
            LED_Memory_error();
            __WFI();
        }
    }

    /* L3: 잔상 제거 ? led_isr_active_set(false) 상태에서 즉시 OFF 분기 */
    turnOffLED();

    /* L4: LED arbiter ISR 가용 시작 */
    led_isr_active_set(true);

    /* L5: POWER_ON 버스트 요청 ? TIMER3 ISR 가 SKYBLUE fade-in/out × 5 진행 */
    led_request(LED_SRC_POWER, LED_ST_POWER_ON);
    ci_printi("\r\n");
    ci_printi("################################################################\r\n");
    ci_printi("###  [POWER-ON  START]   t3 = %d ms\r\n", tdc_timer_get_t3_tick());
    ci_printi("################################################################\r\n");
    ci_printi("\r\n");

    /* 드라이브 0으로 변경 후 부트 상태 처리 후
     * 드라이브 1로 변경하여 맵 관련 파일을 사용할 수 있게 설정 */

    ci_util_assert(snd_fatfs_remount(0));  // 부트 드라이브(0)으로 마운트
    ci_boot_init_fp(ci_fatfs_get_fp());
    ci_boot_handle_fsm();
    ci_util_assert(snd_fatfs_remount(1));  // 사용자 드라이브(1)로 마운트

    ci_printi("[INFO] INIT : BOOT STATUS \r\n");

    // Check, make and init ISD map files (info, user setting, map stamp, map_data.....)
    ci_map_init_map_data_all(false);

    ci_printi("[INFO] INIT : MAP DATA ALL \r\n");

    // Check, make and init FFT pass bin files (ch1 to ch32.....).
    ci_fft_init_pass_bin_all();

    ci_printi("[INFO] INIT : FFT PASS BIN \r\n");

    ci_fft_init_window_coeff();  // Check, make and init Hanning Window Coeff

    ci_printi("[INFO] INIT : FFT WINDOW COEFF \r\n");

    ci_stim_mute_init();
    ci_printi("[INFO] INIT : STIM MUTE \r\n");

    ci_event_log_init();

    ci_printi("[INFO] INIT : EVENT LOG \r\n");

    // 1세대에서는 CFX가 플래시에서 ISD 정보를 읽어서 공유 메모리에 저장하던 기능을,
    // 1.5세대에서는 CM3가 직접 플래시에서 맵 데이터를 맵 데이터용 메모리에 로드하기 때문에
    // 이 맵 데이터용 메모리에서 공유 메모리로 ISD 정보를 CM3가 로드하도록 구현하였다.
    // 그러므로, CM3가 직접 ISD 정보를 공유 메모리로 로드 한 후 CFX_EEPROM_data_is_Loaded를 1로 설정한다.
    ci_filesystem_copy_isd_info_from_filesystem_to_shared_memory();
    cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded = 1;

    ci_printv("[INFO] COPY ISD INFO FOR ALL MAPS FROM FS_MEM TO SH_MEM \r\n");

    /* turnOffLED · sharedMemoryAddresError 는 LED 진입 게이트 (P3-Early) 로 이관됨 */

    // NRF 리셋
    ResetNRF();
    ci_printv("[BLE] RESET NRF \r\n");

    // NRF 끄기 전달
    NRF_Off_Command();
    // ci_printv("[BLE] NRF OFF ('DIO%d' LEVEL LOW) \r\n", DIO_NUM_NRF_ON_OFF_COMMAND);

    // 인터럽트 초기화 및 비활성화
    // reset_interrupt_Disable_PRIMASK();

    // 더 이상 EZ에서 배터리 측정하지 않음
    // ci_battery_init();  // 배터리 측정을 위한 초기화

    // DAM 초기화 및 비활성화
    reset_DMA_disable();

    // I2C 초기화
    init_I2c();

#if 0  // 오직 TX PMIC 테스트를 위한 코드
    {
        int tx_power;

        // 인터럽트 활성화
        enable_interrupt();

        // TX PMIC 초기화
        if (!Reset_REN_ISL9122())
        {
            turnON_RedLED();
            while (1)
            {
                SYS_WATCHDOG_REFRESH();
            }
        }

        // 리셋 디폴트로 세팅
        write_change_TxPowerLevel(ResetVoltageSetValue);

        while (1)
        {
            // 읽어본다.
            if (!read_txPowerLevel(&tx_power))
            {
                while (1)
                {
                    SYS_WATCHDOG_REFRESH();
                }
            }

            // 최대 값 설정 완료 되면 더 할거 없이 무한루프
            if (tx_power == MaxVoltageControlValue)
            {
                ci_printw("[TEST] PMIC TX POWER SET DONE \r\n");
                turnON_GreenLED();

                while (1)
                {
                    SYS_WATCHDOG_REFRESH();
                }
            }

            // 지금 레벨에서 스탭을 더한다.
            tx_power = tx_power + VoltageControlStep;

            // 새 레벨이 최대 값을 안 넘으면 이대로 설정
            if (tx_power <= MaxVoltageControlValue)
            {
                if (!write_change_TxPowerLevel(tx_power))
                {
                    while (1)
                    {
                        SYS_WATCHDOG_REFRESH();
                    }
                }
            }
            // 새 레벨이 최대 값을 넘으면 최대 값으로 설정
            else
            {
                if (!write_change_TxPowerLevel(MaxVoltageControlValue))
                {
                    while (1)
                    {
                        SYS_WATCHDOG_REFRESH();
                    }
                }
            }
        }
    }
#endif

    /* P11 (Rev.4 patch): 터치 센서 초기화 ? init_cm3_SPI() 직후로 이동.
     * 사유: warm reset (워치독) 후 NRF 의 잔존 SPI 상태가 init_cm3_SPI() 전에
     *       CS RISE 를 만들어 DMA TRANSFER_WORD_CNT_SHORT 미스매치 회귀 발생.
     *       원래 P11 위치 (init_I2c 직후) 는 SPI init 시점을 늦춰 NRF SPI race 가능성.
     *       본 위치는 본 작업 전 시점 (systemControl 분기 → init_cm3_SPI 후) 과 동등.
     * Auto-ATI 대기 (~1.5s) 는 LED 버스트 (~1.8s) 와 병렬 진행 → 체감 시간 0. */
    tdc_touch_init_begin();
    ci_printi("[MILESTONE] TOUCH-INIT-BEGIN t3=%d \r\n", tdc_timer_get_t3_tick());

    /* 종료 배리어: CFX 트리거 → main_tick · iteration 활성. TIMER3 는 stop 안 함. */
    enable_CFX_trigger_for_iteration();  // CFX_0, FIFO_5 인터럽트 활성화
    ci_printi("[MILESTONE] CFX-ITER-ENABLE t3=%d main=%d \r\n", tdc_timer_get_t3_tick(), ci_timer_get_tick());

    // 인터럽트 활성화 (PRIMASK 는 main.c 에서 이미 enable ? 사실상 noop, 안전망)
    enable_interrupt();

    /* 터치 센서 초기화는 P11 (init_I2c 직후) 에서 tdc_touch_init_begin() 으로 시작.
     * led_isr_active_set(true) 는 LED 진입 게이트 (P3-Early L4) 에서 이미 호출.
     * 상세: docs/tasks/LED/power-on-early-lighting/구현계획.md (Rev.4) */

    // 초기화 과정에서 전원 버튼 (가속도 센서, 이제는 터치 센서)의 인터럽트 상태를 초기화 시킨다.
    cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 = 0;

    // 내부기 통신 용 외부전원 끄기
    OnOff_3V_PMIC_CM3_to_CFX(false);

    // 더 이상 가속도 센서 사용하지 않음
#if 0
    // 가속도 센서 설정
    if (!configure_MIS2DH_asClickMode(2))
    {
        ci_printe("[ACC] FAILED TO CONFIGURE AS CLICK MODE \r\n");
        errorCodeUpdate(en__ACCELEROMETER_ERROR, en__I2C_ACCELER_WritingError, __LINE__);
    }
#endif

    clearAllErrorFlag();

    // 더 이상 EZ가 배터리 측정하지 않음
    snd_batt_set_state(EN__SND_BATT_STATE_RESET);
    snd_batt_set_percent(0);
#if 0
    // LSAD의 측정이 최초 한번은 미정확하다고 하여, 넉넉히 4번 측정이 완료된 후 진행되도록 구현하였다.
    while (1)
    {
    	if (CI_LASD_STABLE_CNT < ci_battery_get_count())
    	{
    		break;
    	}

    	__NOP(); // 최적화 방지 목적의 NOP
    }

    ci_battery_update();

    ci_printi("[LSAD] END OF INIT, CURRENTLY BATT SAMPLE COUNT=%d, LSAD VALUE=%d \r\n",
            ci_battery_get_count(),
            cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3);
#endif

    // USB 충전 상태 초기화
    snd_charger_set_state(EN__SND_CHARGER_STATE_RESET);

    // SPI 초기화
    init_cm3_SPI();

    // 초기화 과정을 통해 SPI 인터페이스 설정도 완료 되었고
    // 위에서 CFX 동작까지 실행시켰으므로, 이제 QCC를 깨우고 배터리 정보를 얻을 수 있도록 한다.
    snd_qcc_set_mode(SND_QCC_MODE_NORMAL);
    ci_printi("\r\n");
    ci_printi("################################################################\r\n");
    ci_printi("###  [QCC SET-NORMAL]    t3 = %d ms\r\n", tdc_timer_get_t3_tick());
    ci_printi("################################################################\r\n");
    ci_printi("\r\n");
}
