#include <tdc_sys_error.h>
#include <tdc_shm.h>
#include <tdc_ble_mapping.h>
#include <tdc_isd.h>
#include <tdc_isd_stim_para_setting.h>
#include <tdc_isd_map_specific_stim.h>
#include <tdc_stim_indicator.h>

#include <FPGA.h>
#include <tdc_isd_fpga.h>
#include <tdc_stim_para_cal.h>

#include <tdc_isd_stim_standalone.h>
#include <tdc_isd_map_live.h>
#include <tdc_pwr_battery.h>
// live 실행을 받으면 맵 번호 인덱스를 마이너스 값으로 변경하여  CFX에서 맵데이터를 실행한다.
#include <tdc_sys_error.h>
#include <electrodeMapping.h>
#include <tdc_hal_spi.h>

#include <tdc_printf.h>

// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

int counterRX = 0;


// 하위명령 사이로 넘기는 값들. 분해 전에는 함수-지역 static 이었다.
static EN__LIVE_STIMULATION_SUB_COMMAND s_prev_subCommand = en__HoldOn;
static int  s_stimulationCounter_ms = 0;
static const ST_STIUL_DAC_REGISTER_VALUE *s_stimulDAC_setting;
static ST__MAPPING_PACKET *s_p_mappingPacket;
static int  s_toggle_mapNum = 0;
static int  s_reConnectionCounter = 0;
static int  s_ISD_connectionCounter_withMapping = 0;
static bool s_needResetting = false;
static int  s_flowCounter;
static bool s_calculationParameter = false;

// 전체 자극 파라미터 적재
static bool tdc_isd_map_live_sub_all_parameter(ST__ISD_STATUS ISD_state)
{
    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;
    int  i;

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

    // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
    tdc_shm_change_audio_volume(s_p_mappingPacket->tdc_isd_map_live_step.audioVolume);
    tdc_shm_change_stimul_volume(s_p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

    // 매핑에서 받은 데이터를 전달한다.

    p_mapDataSharedMemory->stimulationStrategy              = s_p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy;
    p_mapDataSharedMemory->firstPulsePhase                  = s_p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase;
    p_mapDataSharedMemory->stimulationMode                  = s_p_mappingPacket->tdc_isd_map_live_step.stimulationMode;
    p_mapDataSharedMemory->stimulationPulsePhaseWidth       = s_p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth;
    p_mapDataSharedMemory->numFrequencyBand                 = s_p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand;
    p_mapDataSharedMemory->stimulationIndicatorChannelNum   = s_p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;
    p_mapDataSharedMemory->stimulationIndicatorAmplitude_uA = s_p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA;

    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = s_p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i];
        p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = s_p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i];
        p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = s_p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i];
        p_mapDataSharedMemory->T_level_uA[i]                     = s_p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i];
        p_mapDataSharedMemory->C_level_uA[i]                     = s_p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i];
        p_mapDataSharedMemory->audio_input_x_mim[i]              = s_p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i];
        p_mapDataSharedMemory->audio_input_x_max[i]              = s_p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i];
    }

    // 맵번호를 매핑용 번호로 전달하고// start 중에
    if (s_toggle_mapNum)
    {
        tdc_shm_change_program_map_num(-1);
    }
    else
    {
        tdc_shm_change_program_map_num(-2);
    }

    s_toggle_mapNum++;
    s_toggle_mapNum &= 0x1;

    // BLE 응답 전송
    // 패치 단계에서 전송을 완료했다.

    // 모드 변경;
    s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
    TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__ALL_PARAMERTER, SUB COMMAND : STANDBY \r\n");

    return false;
}

// 라이브 자극 시작 - 파라미터 설정 후 출력 개시
static bool tdc_isd_map_live_sub_start(ST__ISD_STATUS ISD_state)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;
    bool isdControlStateChagedFlag;
    bool error = false;

    // CFX에서 맵데이터의 로딩이 완료될 때까지 로딩
    if (tdc_shm_is_map_data_loaded_cfx())
    {
        // TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__START, CFX IS MAPDATA LOADED \r\n");

        if (tdc_isd_is_new_map_loaded_flag())  // 맵이 변경되어서  계산이 필요한 경우.
        {
            TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__START, NEW MAP LOADED FLAG IS TRUE \r\n");

            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
            s_calculationParameter = tdc_stim_calc_para_and_cfx_share();
            tdc_stim_indicator_set_level_255();  // 자극 알림 크기 uA -> 255레벨로 변환

            tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

            TDC_PRINTF_I("[LIVE] LIVE STIMULATION, CFX WILL CALCULATE AUDIO PARAMETERS, NOW \r\n");

            tdc_isd_clear_new_map_loaded_flag();  // 새로운 맵 데이터의 적용을 위한 처리가 완료되었다.

            tdc_isd_clear_stim_para_setting_done();  //

            isdControlStateChagedFlag = true;
            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        }
        else
        {
            isdControlStateChagedFlag = false;
        }

        if (s_calculationParameter)
        {
            if (!tdc_isd_is_stim_para_setting_done())
            {
                // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                error = tdc_isd_stim_setting_step(isdControlStateChagedFlag);

                if (error)
                {
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                    s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }

                // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
            }
            else
            {
                tdc_isd_change_state(en__isdStatus_stimul_10V_Ok);
                //////////////////////////////////////

                tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                TDC_PRINTF_I("[LIVE] LIVE STIMULATION, SUCCESS TO ENABLE STIM 10V BY EN__START \r\n");

                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = en__mapping_live_stimulation;

                //  sub-command loop-back
                bufferForSPI_tx[buffer_tx_index++] = en__Start;

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;

                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
                // 자극 출력
            }
        }
        else
        {
            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
            s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
        }
    }

    if (s_flowCounter > /*50*/ 1000)  // 실시간 자극 파라미터 설정 오류 (13msec)
    {
        // 여기서 에러
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__stimulationParameterUnloaded, __LINE__);

        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
    }

    return false;
}

// 자극 볼륨 조정
static bool tdc_isd_map_live_sub_stim_volume_adjust(ST__ISD_STATUS ISD_state)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;

    if (ISD_state.conneded_ISD)
    {

        // 연결 상태 업데이트
        tdc_isd_update_link_by_backtel_live();

        tdc_shm_change_stimul_volume(s_p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

        // 송신 데이터 준비
        // command loop-back
        bufferForSPI_tx[buffer_tx_index++] = s_p_mappingPacket->command;

        // pay-load 준비
        bufferForSPI_tx[buffer_tx_index++] = en__StimulationVolumeAdjust;

        bufferForSPI_tx[buffer_tx_index++] = tdc_shm_read_stimul_volume();

        // 송신 데이터 SPI TX버퍼에 복사
        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
    }
    else
    {

        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
    }

    return false;
}

// 마이크 감도 조정
static bool tdc_isd_map_live_sub_mic_sensitivity(ST__ISD_STATUS ISD_state)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;

    if (ISD_state.conneded_ISD)
    {
        // 연결 상태 업데이트
        tdc_isd_update_link_by_backtel_live();

        tdc_shm_change_audio_volume(s_p_mappingPacket->tdc_isd_map_live_step.audioVolume);

        // 송신 데이터 준비
        // command loop-back
        bufferForSPI_tx[buffer_tx_index++] = s_p_mappingPacket->command;

        // pay-load 준비
        bufferForSPI_tx[buffer_tx_index++] = en__MicSensitivityAdjust;

        bufferForSPI_tx[buffer_tx_index++] = tdc_shm_read_audio_volume();

        // 송신 데이터 SPI TX버퍼에 복사
        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
    }

    return false;
}

// 자극 알림음 - 이 단계만 반환값을 세운다
static bool tdc_isd_map_live_sub_stim_indicator(ST__ISD_STATUS ISD_state)
{
    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;
    bool sitmulationIndicatorTrigger = false;

    if (ISD_state.conneded_ISD)
    {
        // 연결 상태 업데이트
        tdc_isd_update_link_by_backtel_live();

        if (s_flowCounter == 0)
        {
            p_mapDataSharedMemory                                   = tdc_shm_get_pointer_current_map_data();
            p_mapDataSharedMemory->stimulationIndicatorChannelNum   = s_p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;
            p_mapDataSharedMemory->stimulationIndicatorAmplitude_uA = s_p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA;

            tdc_stim_indicator_set_level_255();  // 자극 알림 크기 uA -> 255레벨로 변환

            // tdc_stim_indicator_set_trigger(); // 자극 알림 시작

            sitmulationIndicatorTrigger = true;  // 자극 알림 시작
        }
        else
        {
            if (!tdc_stim_indicator_is_triggered())  // 자극 알림 종료 되었음?
            {
                // 송신 데이터 준비
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = s_p_mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = en__mapping_Stimul_indicator;

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 라이브 자극 유지로 변경
                s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
        }

        s_stimulationCounter_ms++;
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
    }

    return sitmulationIndicatorTrigger;
}

// 이퀄라이저 설정 읽어 응답
static bool tdc_isd_map_live_sub_read_equalizer(ST__ISD_STATUS ISD_state)
{
    int  i;
    int *p_cfxStimulLevel_255;
    int  stimulationLevel_uA;
    int  offset;
    int  value;
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;

    if (ISD_state.conneded_ISD)
    {
        // 연결 상태 업데이트
        tdc_isd_update_link_by_backtel_live();

        counterRX++;

        s_stimulDAC_setting = tdc_stim_read_dac_register_value();

        p_cfxStimulLevel_255 = tdc_shm_read_current_stimul_level_255();

        // 송신 데이터 준비
        // command loop-back
        bufferForSPI_tx[buffer_tx_index++] = s_p_mappingPacket->command;

        // pay-load 준비
        bufferForSPI_tx[buffer_tx_index++] = en__readEqualizer;

        // 255레벨의 자극을 uA로 변환
        //  offset 값 계산
        // 255레벨의 자극을 uA로 변환
        //  offset 값 계산
        if (s_stimulDAC_setting->DAC_offsetSlope_register == 0)  // offsetDAC_A 기울기 ,,, 2uA 기울기 오프셋
        {
            value  = offsetDAC_A_Slope_QI5F12 * s_stimulDAC_setting->DAC_offsetLevel_register;
            offset = value >> 12;  // offsetDAC_A 기울기
        }
        else  // offsetDAC_B 기울기 ,,, 4uA 기울기 오프셋
        {
            value  = offsetDAC_B_Slope_QI5F12 * s_stimulDAC_setting->DAC_offsetLevel_register;
            offset = value >> 12;  // offsetDAC_B 기울기
        }

        for (i = s_p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadStart_index - 1; i <= s_p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadEnd_index - 1; i++)
        {

            if (p_cfxStimulLevel_255[i] != 0)
            {
                switch (s_stimulDAC_setting->DAC_Slope_register)
                {
                    case 0:  // DAC_A_Slope,,,2uA 기울기
                        value               = DAC_A_Slope_QI5F12 * p_cfxStimulLevel_255[i];
                        stimulationLevel_uA = value >> 12;
                        break;
                    case 1:  // DAC_B_Slope,,, 4uA 기울기
                        value               = DAC_B_Slope_QI5F12 * p_cfxStimulLevel_255[i];
                        stimulationLevel_uA = value >> 12;
                        break;
                    case 2:  // DAC_C_Slope,,, 6uA 기울기
                        value               = DAC_C_Slope_QI5F12 * p_cfxStimulLevel_255[i];
                        stimulationLevel_uA = value >> 12;
                        break;
                    case 3:  // DAC_D_Slope,,, 8uA 기울기
                        value               = DAC_D_Slope_QI5F12 * p_cfxStimulLevel_255[i];
                        stimulationLevel_uA = value >> 12;
                        break;
                }

                // uA 단위의 자극은 오프셋 값과 DAC 출력 값을 합친 값
                value = stimulationLevel_uA + offset;
            }
            else
            {
                value = 0;
            }

            bufferForSPI_tx[buffer_tx_index++] = value >> 8;    // 상위 바이트
            bufferForSPI_tx[buffer_tx_index++] = value & 0xFF;  // 하위 바이트

            // BLE 응답
        }

        // 송신 데이터 SPI TX버퍼에 복사
        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;

        s_stimulationCounter_ms++;
    }
    else
    {
        tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

        // 라이브 자극 유지로 변경
        s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
    }

    return false;
}

// 기기 상태 읽어 응답
static bool tdc_isd_map_live_sub_read_device_status(ST__ISD_STATUS ISD_state)
{
    int  value;
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;

    // 송신 데이터 준비
    // command loop-back
    bufferForSPI_tx[buffer_tx_index++] = s_p_mappingPacket->command;

    // pay-load 준비
    bufferForSPI_tx[buffer_tx_index++] = en__readDeviceStatus;

    // 연결 상태, 배터리 값  전송
    if (ISD_state.conneded_ISD)
    {
        value = 1;
    }
    else
    {
        value = 2;
    }

    bufferForSPI_tx[buffer_tx_index++] = value;

    // value=(int)readBatteryLevel();
    value                              = tdc_pwr_battery_read_percentage();
    bufferForSPI_tx[buffer_tx_index++] = value;

    // 송신 데이터 SPI TX버퍼에 복사

    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

    s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;

    s_stimulationCounter_ms++;

    return false;
}

// 자극 유지 - 연결 상태를 보며 재설정 여부를 판단
static bool tdc_isd_map_live_sub_hold_on(ST__ISD_STATUS ISD_state)
{
    ST__CFX_CM3_SharedMemory_mapData *p_mapDataSharedMemory;
    int  i;
    bool isdControlStateChagedFlag;
    bool error = false;

    if (!ISD_state.conneded_ISD)
    {
        s_needResetting       = true;
        s_reConnectionCounter = 0;
    }
    else
    {
        if (s_needResetting)  // 연결이 끊어졌다가 다시 붙은 경우
        {
            if (s_reConnectionCounter == 0)
            {
                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

                // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
                tdc_shm_change_audio_volume(s_p_mappingPacket->tdc_isd_map_live_step.audioVolume);
                tdc_shm_change_stimul_volume(s_p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

                // 매핑에서 받은 데이터를 전달한다.

                p_mapDataSharedMemory->stimulationStrategy            = s_p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy;
                p_mapDataSharedMemory->firstPulsePhase                = s_p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase;
                p_mapDataSharedMemory->stimulationMode                = s_p_mappingPacket->tdc_isd_map_live_step.stimulationMode;
                p_mapDataSharedMemory->stimulationPulsePhaseWidth     = s_p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth;
                p_mapDataSharedMemory->numFrequencyBand               = s_p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand;
                p_mapDataSharedMemory->stimulationIndicatorChannelNum = s_p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;

                for (i = 0; i < df_MaxNumOfElectrode; i++)
                {
                    p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = s_p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i];
                    p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = s_p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i];
                    p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = s_p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i];
                    p_mapDataSharedMemory->T_level_uA[i]                     = s_p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i];
                    p_mapDataSharedMemory->C_level_uA[i]                     = s_p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i];
                    p_mapDataSharedMemory->audio_input_x_mim[i]              = s_p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i];
                    p_mapDataSharedMemory->audio_input_x_max[i]              = s_p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i];
                }

                // 맵번호를 매핑용 번호로 전달하고// start 중에
                if (s_toggle_mapNum)
                {
                    tdc_shm_change_program_map_num(-1);
                }

                else
                {
                    tdc_shm_change_program_map_num(-2);
                }

                s_toggle_mapNum++;
                s_toggle_mapNum &= 0x1;
            }

            if (tdc_shm_is_map_data_loaded_cfx())
            {
                if (tdc_isd_is_new_map_loaded_flag())  // 맵이 변경되어서  계산이 필요한 경우.
                {
                    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                    s_calculationParameter = tdc_stim_calc_para_and_cfx_share();
                    tdc_stim_indicator_set_level_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                    tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                    tdc_isd_clear_new_map_loaded_flag();  // 새로운 맵 데이터의 적용을 위한 처리가 완료되었다.

                    tdc_isd_clear_stim_para_setting_done();  //

                    isdControlStateChagedFlag = true;
                    // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                }
                else
                {
                    isdControlStateChagedFlag = false;
                }

                if (s_calculationParameter)
                {
                    if (!tdc_isd_is_stim_para_setting_done())
                    {
                        // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                        error = tdc_isd_stim_setting_step(isdControlStateChagedFlag);

                        if (error)
                        {
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                            s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                        }

                        // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                    }
                    else
                    {
                        tdc_isd_change_state(en__isdStatus_stimul_10V_Ok);
                        //////////////////////////////////////

                        tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                        tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                        tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                        TDC_PRINTF_I("[LIVE] LIVE STIMULATION, SUCCESS TO ENABLE STIM 10V BY EN__HOLDON \r\n");

                        // 자극 출력
                        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);

                        s_needResetting = false;
                    }
                }
                else
                {
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                    s_p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }
            }

            s_reConnectionCounter++;

        }
        else
        {
            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
            // 응답 데이터 생성
            // 연결 상태 업데이트
            tdc_isd_update_link_by_backtel_live();

            s_stimulationCounter_ms++;

            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        }
    }


    return false;
}

// 라이브 자극 종료
static bool tdc_isd_map_live_sub_stop(ST__ISD_STATUS ISD_state)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int  buffer_tx_index = 0;

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    // 파라미터 초기화
    s_stimulationCounter_ms = 0;

    // 송신 데이터 준비
    // command loop-back
    bufferForSPI_tx[buffer_tx_index++] = s_p_mappingPacket->command;

    // pay-load 준비
    bufferForSPI_tx[buffer_tx_index++] = en__Stop;

    // 송신 데이터 SPI TX버퍼에 복사
    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

    // 커맨드 리셋;
    tdc_ble_mapping_clear_command();

    return false;
}

// 대기
static bool tdc_isd_map_live_sub_standby(ST__ISD_STATUS ISD_state)
{
    if (ISD_state.conneded_ISD)
    {
        //tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

        if (s_ISD_connectionCounter_withMapping == 0)
        {
            TDC_PRINTF_V("\r\n[MAPPING] LINK CHECK TRIGGERED (LIVE, STANDBY) \r\n");
        }

        tdc_isd_update_link_by_backtel_mapping(s_ISD_connectionCounter_withMapping);

        s_ISD_connectionCounter_withMapping++;

        if (s_ISD_connectionCounter_withMapping > 300)
        {
            s_ISD_connectionCounter_withMapping = 0;
        }
    }
    else
    {
        s_ISD_connectionCounter_withMapping = 0;
    }

    return false;
}

bool tdc_isd_map_live_step(ST__ISD_STATUS ISD_state)
{
    bool sitmulationIndicatorTrigger = false;

    s_p_mappingPacket = (ST__MAPPING_PACKET *) tdc_ble_mapping_get_packet();

    if (s_prev_subCommand != s_p_mappingPacket->tdc_isd_map_live_step.subCommand)
    {
        s_flowCounter = 0;
    }

    switch (s_p_mappingPacket->tdc_isd_map_live_step.subCommand)
    {
        case en__allParameter:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_all_parameter(ISD_state);
            break;
        }

        case en__Start:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_start(ISD_state);
            break;
        }

        case en__StimulationVolumeAdjust:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_stim_volume_adjust(ISD_state);
            break;
        }

        case en__MicSensitivityAdjust:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_mic_sensitivity(ISD_state);
            break;
        }

        case en__mapping_Stimul_indicator:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_stim_indicator(ISD_state);
            break;
        }

        case en__readEqualizer:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_read_equalizer(ISD_state);
            break;
        }

        case en__readDeviceStatus:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_read_device_status(ISD_state);
            break;
        }

        case en__HoldOn:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_hold_on(ISD_state);
            break;
        }

        case en__Stop:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_stop(ISD_state);
            break;
        }

        case en__Standby:
        {
            sitmulationIndicatorTrigger = tdc_isd_map_live_sub_standby(ISD_state);
            break;
        }

    }

    s_flowCounter++;

    s_prev_subCommand = s_p_mappingPacket->tdc_isd_map_live_step.subCommand;

    return sitmulationIndicatorTrigger;
}
