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

void ResetNRF(void)
{
    Sys_GPIO_Set_High(DIO_NUM_NRF_SWDIO_NRESET);

    for (volatile int i = 0; i < 200; i++)
    {
        __NOP();
    }

    Sys_GPIO_Set_Low(DIO_NUM_NRF_SWDIO_NRESET);

    for (volatile int i = 0; i < 200; i++)
    {
        __NOP();
    }

    Sys_GPIO_Set_High(DIO_NUM_NRF_SWDIO_NRESET);
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

    ci_printv("[INFO] BUILD DATE : %s \r\n", __DATE__);
    ci_printv("[INFO] BUILD TIME : %s \r\n", __TIME__);

    ci_util_assert(ci_filesystem_nvm_init());
    ci_util_assert(ci_filesystem_mount());
    ci_util_assert(ci_power_normal());

    ci_printi("[INFO] POWER NORMAL, CLOCK : %u HZ \r\n", SystemCoreClock);

    ci_util_assert(ci_filesystem_remount());
    ci_printv("[INFO] REMOUNT FILESYSTEM DRIVE ('%s') \r\n", CI_FILESYSTEM_LOGICAL_DRIVE_NUM);

    ci_dio_configure_normal();
    ci_uart_init();

    ci_printv("[INFO] INIT : DIO, UART, ETC.. \r\n");

    SYS_WATCHDOG_REFRESH();

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

    // LED 출력 끄기
    turnOffLED();

    // CFX와 CM3와의 공유 메모리 주소 확인 (컴파일 오류)
    if (sharedMemoryAddresError())
    {
        ci_printe("[INFO] INVALID SHARED MEMORY ADDRESS \r\n");

        while (1)
        {
            LED_Memory_error();
            __WFI();
        }
    }

    // NRF 리셋
    ResetNRF();
    ci_printv("[BLE] RESET NRF \r\n");

    // NRF 끄기 전달
    NRF_Off_Command();
    ci_printv("[BLE] NRF OFF ('DIO%d' LEVEL LOW) \r\n", DIO_NUM_NRF_ON_OFF_COMMAND);

    // 인터럽트 초기화 및 비활성화
    // reset_interrupt_Disable_PRIMASK();

    ci_battery_init();  // 배터리 측정을 위한 초기화

    // DAM 초기화 및 비활성화
    reset_DMA_disable();

    // I2C 초기화
    init_I2c();

    // SPI 초기화
    init_cm3_SPI();

    // CFX 트리거를 받은 인터럽트 활성화
    enable_CFX_trigger_for_iteration();

    // 인터럽트 활성화
    enable_interrupt();

    // 내부기 통신 용 외부전원 끄기
    OnOff_3V_PMIC_CM3_to_CFX(false);

    // 가속도 센서 설정
    if (!configure_MIS2DH_asClickMode(2))
    {
        ci_printe("[ACC] FAILED TO CONFIGURE AS CLICK MODE \r\n");
        errorCodeUpdate(en__ACCELEROMETER_ERROR, en__I2C_ACCELER_WritingError, __LINE__);
    }

    clearAllErrorFlag();

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
}
