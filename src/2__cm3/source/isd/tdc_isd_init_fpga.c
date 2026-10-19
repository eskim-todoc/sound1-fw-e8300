#include <hw.h>
#include <tdc_isd_pcm.h>
#include <stdbool.h>

#include <board.h>
#include <tdc_isd_init_fpga.h>
#include <internalStimulationChip.h>
#include <tdc_isd.h>

#include <tdc_hal_i2c_isd.h>

#include <tdc_stim_definitions.h>

#include <tdc_isd.h>
#include <tdc_shm.h>
#include <tdc_isd_fpga.h>

#include <tdc_sys_error.h>

// #include "FPGA.h"
#include <tdc_drv_isl9122.h>

#include <tdc_stim_common.h>
#include <tdc_led_output.h>

static int FPGA_version;

#define df_startStabilizationCounter 100

void tdc_isd_init_tx_power_ic(bool isdControlStateChagedFlag)
{
    static int flowControlCounter = 0;
    int        RF_TxPowerValue;
    int        tempValue;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        tdc_isd_clear_control_state_changed_flag();
    }

    switch (flowControlCounter)
    {
        case 0:
            tdc_isd_set_i2c_free();
            tdc_shm_on_off_3_v_pmic_cm3_to_cfx(0);
            break;

        case 3:
            tdc_shm_on_off_3_v_pmic_cm3_to_cfx(1);
            break;

        case 10:  // 전압 제어 범위 중 최소 값으로 시작.
        {
            static int power_reset_err_cnt = 0;

            tdc_isd_set_i2c_busy();

            // Pwoer IC의 전원이 켜져 있어야 한다.
            if (!tdc_drv_isl9122_reset())
            {
                TDC_PRINTF_E("[PMIC] c10: PMIC reset failed\r\n");
                TDC_ISD_DEBOUNCE_FAIL(power_reset_err_cnt, "[PMIC]", "init_txPwr c10", "verify", en__RF_PowerIC_ERROR, en__NON_RESETTABLE);
            }
            else
            {
                power_reset_err_cnt = 0;
            }
        }
        break;

        case 11:  // 전압 제어 범위 중 최소 값으로 시작.
        {
        }
        break;

        case 12:  // 전압 제어 범위 중 최소 값으로 시작.
        {
            tdc_isd_fpga_write_change_tx_power_level(TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE);
        }
        break;

        case 13:  // 전송 파워 증가 시퀀스
        {
            if (tdc_isd_fpga_read_tx_power_level(&RF_TxPowerValue))
            {
                // 증가
                tempValue = RF_TxPowerValue + TDC_DRV_PMIC_VOLTAGE_CONTROL_STEP;

                // 큰 step 으로 전압을 올릴 수 있을 때
                if (tempValue <= TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE)
                {
                    tdc_isd_fpga_write_change_tx_power_level(tempValue);

                    flowControlCounter = 12;  // 함수 종료 전 flowControlCounter++; → 실제로 case 13을 반복
                }
                else  // 큰 step 으로 전압을 올리기 어려울 때
                {
                    tempValue = RF_TxPowerValue + 1;

                    if (tempValue <= TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE)
                    {
                        tdc_isd_fpga_write_change_tx_power_level(tempValue);  // 함수 종료 전 flowControlCounter++; → 실제로 case 13을 반복
                        flowControlCounter = 12;
                    }
                    else
                    {
                        // 더이상 step을 증가 시킬 수 없음 종료
                        flowControlCounter = df_startStabilizationCounter;
                    }
                }
            }
            // I2C 통신 실패에 대한 에러는 tdc_isd_fpga_read_tx_power_level() 함수 내부에서
            // 자체적으로 tdc_isd_change_state(en__isdStatus_PowerIC_Reset); 를 수행하여 해결함
        }
        break;

        case df_startStabilizationCounter + 10:  // 전송 파워 증가 시퀀스
        {
            tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);

            TDC_PRINTF_D("[PMIC] INIT SUCCESS\r\n");
        }
        break;
    }

    flowControlCounter++;
}

void tdc_isd_init_fpga(bool isdControlStateChagedFlag)
{
    static int flowControlCounter = 0;
    int        r_FPGA_registerValue;
    int        comparing;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        tdc_isd_clear_control_state_changed_flag();
    }

    switch (flowControlCounter)  //
    {
        case 0:  // PCM은 0을 출력한다.
        {
            tdc_isd_fpga_reset_fpga_variable();
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_FillZero);
            tdc_isd_set_i2c_free();
        }
        break;

        case 10:  //
        {
            tdc_isd_set_i2c_busy();
        }
        break;

        case 11:  // FPGA를 리셋한다.
        {
            if (tdc_isd_fpga_write_reset())
            {
                if (tdc_isd_fpga_read_version(&FPGA_version))
                {
                    TDC_PRINTF_V("[FPGA] VERSION : %u.%u\r\n", ((FPGA_version >> 4) & 0x0F), (FPGA_version & 0x0F));

                    // FPGA 상태를 읽어 본다.
                    if (tdc_isd_fpga_read_systemregister_1st(&r_FPGA_registerValue))
                    {
                        static int no_reset_value_error_cnt = 0;

                        comparing = r_FPGA_registerValue & FPGA_Status_FlagIndex;
                        if (comparing != FPGA_Status__Reset_value)
                        {
                            // FPGA 초기화
                            tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                            TDC_ISD_DEBOUNCE_FAIL(
                                no_reset_value_error_cnt, "[FPGA]", "tdc_isd_init_fpga c11", "verify", en__FPGA_COMMUNICATION_ERROR, en__FPGA_ResetValueError);

                            TDC_PRINTF_E("[FPGA] c11: not initialized after reset, SYS_STAT1=0x%02X MASKER=0x%02X RESULT=0x%02X\r\n",
                                         r_FPGA_registerValue,
                                         FPGA_Status_FlagIndex,
                                         comparing);
                        }
                        else
                        {
                            // 정상 구간이라 출력문 없음
                            tdc_sys_error_clear_flag(en__FPGA_COMMUNICATION_ERROR);
                            no_reset_value_error_cnt = 0;
                        }
                    }
                    else
                    {
                        TDC_PRINTF_W("[FPGA] c11: systemReg1 read comm fail\r\n");
                    }
                }
                else
                {
                    TDC_PRINTF_W("[FPGA] c11: version read comm fail\r\n");
                }
            }
            else
            {
                TDC_PRINTF_W("[FPGA] c11: reset write comm fail\r\n");
            }
        }
        break;

        case 12:
            // 프리엠블 진행
            // 0을 1ms 출력, 프리엠블 1ms 출력 --> 2ms이 소요됨
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_Preamble);
            break;

        case 20:  // FPGA PCM 상태 확인
        {
            // FPGA 상태를 읽어 본다.
            if (tdc_isd_fpga_read_systemregister_1st(&r_FPGA_registerValue))
            {
                static int no_disabled_rf_error_cnt = 0;

                comparing = r_FPGA_registerValue & FPGA_Status_FlagIndex;
                if (comparing == FPGA_Status__OK_DisabledRF_value)
                {
                    //////////////
                    // FPGA PCM 수신 정상
                    //////////////
                    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);
                    tdc_isd_change_state(en__isdStatus_FPGA_Ok);
                    tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);

                    TDC_PRINTF_D("[FPGA] INIT SUCCESS\r\n");

                    no_disabled_rf_error_cnt = 0;
                }
                else
                {
                    // FPGA 초기화
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                    TDC_ISD_DEBOUNCE_FAIL(
                        no_disabled_rf_error_cnt, "[FPGA]", "tdc_isd_init_fpga c20", "verify", en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM);

                    TDC_PRINTF_E(
                        "[FPGA] c20: INIT FAILED, SYS_STAT1=0x%02X MASKER=0x%02X RESULT=0x%02X\r\n", r_FPGA_registerValue, FPGA_Status_FlagIndex, comparing);
                }
            }
            else
            {
                TDC_PRINTF_E("[FPGA] c20: systemReg1 read comm fail\r\n");
            }
        }
        break;

        default:
            break;
    }

    flowControlCounter++;
}

void tdc_isd_init_device(bool isdControlStateChagedFlag)
{
    static int temporal_registerValue;
    static int flowControlCounter = 0;

    //  int i2c_txBuffer[32];
    //  int i2c_rxBuffer[32];
    int RF_TxPowerValue;
    int r_FPGA_registerValue;
    int w_isd_registerValue;
    int r_isd_registerValue[2];
    int pcm_index;
    int i;

    bool FPGA_FIFO_empty;
    bool FPGA_error;

    if (isdControlStateChagedFlag)
    {
        flowControlCounter = 0;
        tdc_isd_clear_control_state_changed_flag();
        tdc_shm_change_connected_isd_num_cfx(0);
    }

    pcm_index = 0;

    switch (flowControlCounter)
    {
        // FPGA에 에러 발생했는지 확인 하는 단계, 에러 없을 시 10Mhz 캐리어 클럭 끔
        case 0:
        {
            tdc_isd_fpga_check_fpga_pcm_error(&FPGA_error);
            if (FPGA_error)
            {
                // FPGA 에러 발생, FPGA 초기화
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                TDC_PRINTF_E("[FPGA] c0: PCM error detected\r\n");
            }
            else
            {
                tdc_isd_fpga_write_disable_rf_tx();
                tdc_isd_fpga_write_change_tx_power_level(TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE);

                // TDC_PRINTF_V("[FPGA] TRY TO DISABLE XFR(RF) \r\n");
            }

            tdc_isd_set_i2c_free();
        }
        break;

        // 약 200ms 동안 10Mhz 캐리어 클럭을 꺼서 내부기에 전원이 인가되지 않아 꺼져있을 것으로 추정
        // 다시 10Mhz 캐리어 클럭을 켬
        case 200:
        {
            static int outer_rf_tx_error_cnt = 0;
            // PCM 출력 모드 변경
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

            tdc_isd_set_i2c_busy();

            if (!tdc_isd_fpga_is_rf_tx_enable())
            {
                outer_rf_tx_error_cnt = 0;
                // TDC_PRINTF_V("[FPGA] SUCCESS TO DISABLE XFR(RF) THEN, TRY TO ENABLE XFR(RF) \r\n");

                if (tdc_isd_fpga_write_enable_rf_tx())
                {
                    static int inner_rf_tx_error_cnt = 0;

                    if (!tdc_isd_fpga_is_rf_tx_enable())
                    {
                        tdc_isd_change_state(en__isdStatus_PowerIC_OK);  // FPGA 초기화
                        TDC_ISD_DEBOUNCE_FAIL(
                            inner_rf_tx_error_cnt, "[FPGA]", "tdc_isd_init_device c200EN", "verify", en__FPGA_CONFIGUARATION_ERROR, en_RF_Tx_enableError);
                        TDC_PRINTF_E("[FPGA] c200: RF_tx not enabled after write\r\n");
                    }
                    else
                    {
                        // 정상 구간
                        tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                        inner_rf_tx_error_cnt = 0;
                    }
                }
                else
                {
                    TDC_PRINTF_E("[FPGA] c200: RF_tx enable write comm fail\r\n");
                }
            }
            else
            {
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);  // FPGA 초기화
                TDC_ISD_DEBOUNCE_FAIL(
                    outer_rf_tx_error_cnt, "[FPGA]", "tdc_isd_init_device c200DIS", "verify", en__FPGA_CONFIGUARATION_ERROR, en_RF_Tx_enableError);
                TDC_PRINTF_E("[FPGA] c200: RF_tx still enabled (expected off)\r\n");
            }
        }
        break;

        // 약 2ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 펄스 폭을 최소로 설정 (앞으로 PCM 통신으로 이런 저런 설정을 하기 위함)
        case 202:
        {
            tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);  // 펄스 폭 설정

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 여기까지 약 5ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 펄스 폭이 최소로 설정되었는지 검증
        case 205:
        {
            static int pulse_width_error_cnt = 0;
            static int tx_power_err_cnt      = 0;

            if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
            {
                tdc_sys_error_clear_flag(en__FPGA_COMMUNICATION_ERROR);

                if (r_FPGA_registerValue == FPGA_pulsePhaseWidth_minimum)
                {
                    pulse_width_error_cnt = 0;
                    // 펄스 폭 설정이 정상적으로 완료되었기 때문에 펄스폭 값을 업데이트한다.
                    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(r_FPGA_registerValue);

                    tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);

                    if (tdc_isd_fpga_write_change_tx_power_level(TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE))
                    {
                        if (tdc_isd_fpga_read_tx_power_level(&RF_TxPowerValue))
                        {
                            if (RF_TxPowerValue != TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE)
                            {
                                // R_TxPower 설정 실패, powerIC 초기화
                                TDC_ISD_DEBOUNCE_FAIL(
                                    tx_power_err_cnt, "[PMIC]", "tdc_isd_init_device c205pwr", "verify", en__RF_PowerIC_ERROR, en__writtenReadVlaueIsNotSame);
                                tdc_isd_change_state(en__isdStatus_PowerIC_Reset);

                                TDC_PRINTF_E("[PMIC] c205: TxPower != MaxVoltage after write\r\n");
                            }
                            else
                            {
                                // 정상 구간
                                tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                                tx_power_err_cnt = 0;
                            }
                        }
                        else
                        {
                            TDC_PRINTF_E("[PMIC] c205: TxPowerLevel read comm fail\r\n");
                        }
                    }
                    else
                    {
                        TDC_PRINTF_E("[PMIC] c205: TxPowerLevel write comm fail\r\n");
                    }
                }
                else
                {
                    // 펄스폭 설정 실패 , FPGA 초기화
                    TDC_ISD_DEBOUNCE_FAIL(
                        pulse_width_error_cnt, "[FPGA]", "tdc_isd_init_device c205pls", "verify", en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent);
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);

                    TDC_PRINTF_E("[FPGA] c205: pulse width != minimum\r\n");
                }
            }
            else
            {
                TDC_PRINTF_E("[FPGA] c205: pulse width read comm fail\r\n");
            }
        }
        break;

#ifndef DisalbedBackTel
        // 여기까지 약 6ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 백텔 레지스터를 8비트 모드로 설정
        case 206:
        {
            temporal_registerValue = tdc_isd_fpga_reset_8bit_backtel_config(pcm_index++);  // FPGA Backtel - 8bit 레지스터

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 여기까지 약 10ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // FPGA의 백텔 레지스터가 8비트 모드로 설정되었는지 확인 후 백텔 FIFO 클리어 진행
        case 210:
        {
            static int fifo_clear_err_cnt  = 0;
            static int backtel_cfg_err_cnt = 0;

            if (tdc_isd_fpga_read_backtel_config(&r_FPGA_registerValue))
            {
                tdc_sys_error_clear_flag(en__FPGA_COMMUNICATION_ERROR);

                if (temporal_registerValue == r_FPGA_registerValue)
                {
                    backtel_cfg_err_cnt = 0;

                    // 위에서, PCM으로 설정한 FPGA 백텔 레지스터가 원하는 값으로 쓰여졌는지 확인 후 업데이트
                    tdc_isd_fpga_update_fpga_backtel_config_written_value(temporal_registerValue);

                    // FIFO를 지우고 FPGA 상태를 읽어 본다.
                    if (tdc_isd_fpga_write_clear_fifo())
                    {
                        if (tdc_isd_fpga_check_fpga_fifo_empty(&FPGA_FIFO_empty))  // FIFO 지워 졌는지 확인.
                        {
                            if (!FPGA_FIFO_empty)
                            {
                                TDC_ISD_DEBOUNCE_FAIL(
                                    fifo_clear_err_cnt, "[FPGA]", "tdc_isd_init_device c210fifo", "verify", en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared);
                                tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //
                                TDC_PRINTF_E("[FPGA] c210: FIFO not empty after clear\r\n");
                            }
                            else
                            {
                                // 정상 구간
                                tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                                fifo_clear_err_cnt = 0;
                            }
                        }
                        else
                        {
                            TDC_PRINTF_E("[FPGA] c210: FIFO empty check comm fail\r\n");
                        }
                    }
                    else
                    {
                        TDC_PRINTF_E("[FPGA] c210: FIFO clear write comm fail\r\n");
                    }
                }
                else
                {
                    // 백텔 레지스터 설정 오류
                    TDC_ISD_DEBOUNCE_FAIL(
                        backtel_cfg_err_cnt, "[FPGA]", "tdc_isd_init_device c210bt", "verify", en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM);
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);

                    TDC_PRINTF_E("[FPGA] c210: backtel reg mismatch\r\n");
                }
            }
        }
        break;

        // 여기까지 약 50ms 동안 10Mhz 캐리어 클럭이 입력되고 있는 상태에서
        // 내부기 칩의 'h0F 레지스터의 SW_SYS_RESET 비트를 1로 설정하여 내부기 칩의 소프트웨어 리셋을 수행
        // 약 50ms 동안 10Mhz 캐리어 클럭이 입력되었으므로 기초 전원 공급은 충분하다고 판단함
        case 250:
        {
            w_isd_registerValue = ISD_registerAddr_SystemClkReset;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x01;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 약  5ms 동안 내부기 칩이 소프트웨어 리셋이 걸렸으리라 판단
        // 참고:
        // 아래 순서 255 단계에서
        // 'h10 레지스터의 SYSCLK_OE을 1로 설정하는 패킷을 구성하지만 실제로 출력은 하지 않는다.
        case 255:
        {
            /* EEPROM_LSK_Error 대응 LSK/PPSK 레지스터 설정은 제거했다(isd_init.c 와 동일).
             * 이 매크로는 processorDirective.h 에서 #if 0 안에만 있어 정의된 적이 없다. */
            // ISD SYSCLK_OE 설정
            w_isd_registerValue = ISD_registerAddr_IO_Config;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0x08;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는 NopStandby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
            // tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 내부기 칩의 소프트웨어 리셋 이후 약 6ms 뒤에 백텔을 사용해서 내부기 칩의 전원 상태를 읽는다.
        case 256:
        {
            // ISD Power 읽기
            w_isd_registerValue = ISD_registerAddr_PowerCheck;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

            // 나머지 버퍼는  NOP standby
            for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
            {
                tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
            }

            tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
        }
        break;

        // 백텔로 내부기 전원 상태 확인 응답이 왔는지 확인한다.
        // 백텔 데이터 수가 1이면 응답을 받은 것이고 1이 아니면,
        // 응답이 없거나 많아도 문제인 것이므로 내부기 연결 과정 처음부터 다시 진행한다.
        case 260:
        {
            // FIFO 카운터를 읽어 본다.
            if (tdc_isd_fpga_read_fifo_counter(&r_FPGA_registerValue))
            {
                if (r_FPGA_registerValue == 1)
                {
                    tdc_isd_fpga_read_backtel_fifo(r_isd_registerValue, 2);
                    tdc_isd_change_state(en__isdStatus_ISD_Power_Ok);

                    tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                    tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                    tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                    TDC_PRINTF_D("[FPGA] c260: backtel power-level response OK\r\n");
                }
                else
                {
                    tdc_isd_fpga_read_backtel_error_flag(&r_FPGA_registerValue);

                    // 백텔 에러는 발생하지 않았으나 백텔이 들어 오지 않았다. -> 내부기 전송 파워 설정 부터 다시.
                    /* 백텔 카운터 0 에러 시 debounce 실패 매크로만 부르던 디버그 블록은
                         * 제거했다(#if 0 사장). */
                    tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // RF PMIC MAX POWER 설정을 FPGA_OK 상태에서도 진행한다.
                    // tdc_isd_change_state(en__isdStatus_PowerIC_OK);

                    // TDC_PRINTF_V("[FPGA] NO POWER LEVEL BACKTEL RESPONSE FOR INITIAL CONNECTION \r\n");
                }
            }
            else
            {
                TDC_PRINTF_W("[FPGA] c260: FIFO counter read comm fail\r\n");
            }
        }
        break;
        /* DisalbedBackTel 정의 시의 대안 상태전이(case 275 강제)는 제거했다.
   * 이 매크로는 트리 전체에서 정의된 적이 없어 영구 사장이었다. */
#endif

        default:
            break;
    }

    flowControlCounter++;
}
