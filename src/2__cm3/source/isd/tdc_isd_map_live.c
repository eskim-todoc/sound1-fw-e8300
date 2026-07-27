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

bool tdc_isd_map_live_step(ST__ISD_STATUS ISD_state)
{
    static EN__LIVE_STIMULATION_SUB_COMMAND prev_subCommand     = en__HoldOn;

    static int stimulationCounter_ms = 0;

    ST__CFX_CM3_SharedMemory_mapData         *p_mapDataSharedMemory;
    static const ST_STIUL_DAC_REGISTER_VALUE *stimulDAC_setting;

    static ST__MAPPING_PACKET *p_mappingPacket;

    static int  toggle_mapNum                     = 0;
    static int  reConnectionCounter               = 0;
    static int  ISD_connectionCounter_withMapping = 0;
    static bool needResetting                     = false;

    int        i;
    int       *p_cfxStimulLevel_255;
    static int flowCounter;
    int        stimulationLevel_uA;
    int        offset;
    int        value;

    int bufferForSPI_tx[BLE_DataPacketSize];
    int buffer_tx_index;

    static bool calculationParameter = false;

    bool isdControlStateChagedFlag;
    bool sitmulationIndicatorTrigger = false;
    bool error                       = false;

    buffer_tx_index = 0;

    p_mappingPacket = (ST__MAPPING_PACKET *) tdc_ble_mapping_get_packet();

    if (prev_subCommand != p_mappingPacket->tdc_isd_map_live_step.subCommand)
    {
        flowCounter = 0;
    }


    switch (p_mappingPacket->tdc_isd_map_live_step.subCommand)
    {
        case en__allParameter:
        {
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

            p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

            // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
            tdc_shm_change_audio_volume(p_mappingPacket->tdc_isd_map_live_step.audioVolume);
            tdc_shm_change_stimul_volume(p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

            // 매핑에서 받은 데이터를 전달한다.

            p_mapDataSharedMemory->stimulationStrategy              = p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy;
            p_mapDataSharedMemory->firstPulsePhase                  = p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase;
            p_mapDataSharedMemory->stimulationMode                  = p_mappingPacket->tdc_isd_map_live_step.stimulationMode;
            p_mapDataSharedMemory->stimulationPulsePhaseWidth       = p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth;
            p_mapDataSharedMemory->numFrequencyBand                 = p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand;
            p_mapDataSharedMemory->stimulationIndicatorChannelNum   = p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;
            p_mapDataSharedMemory->stimulationIndicatorAmplitude_uA = p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA;

            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i];
                p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i];
                p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i];
                p_mapDataSharedMemory->T_level_uA[i]                     = p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i];
                p_mapDataSharedMemory->C_level_uA[i]                     = p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i];
                p_mapDataSharedMemory->audio_input_x_mim[i]              = p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i];
                p_mapDataSharedMemory->audio_input_x_max[i]              = p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i];
            }

            // 맵번호를 매핑용 번호로 전달하고// start 중에
            if (toggle_mapNum)
            {
                tdc_shm_change_program_map_num(-1);
            }
            else
            {
                tdc_shm_change_program_map_num(-2);
            }

            toggle_mapNum++;
            toggle_mapNum &= 0x1;

            // BLE 응답 전송
            // 패치 단계에서 전송을 완료했다.

            // 모드 변경;
            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__ALL_PARAMERTER, SUB COMMAND : STANDBY \r\n");
        }
        break;

        case en__Start:
        {
            // CFX에서 맵데이터의 로딩이 완료될 때까지 로딩
            if (tdc_shm_is_map_data_loaded_cfx())
            {
                // TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__START, CFX IS MAPDATA LOADED \r\n");

                if (tdc_isd_is_new_map_loaded_flag())  // 맵이 변경되어서  계산이 필요한 경우.
                {
                    TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__START, NEW MAP LOADED FLAG IS TRUE \r\n");

                    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                    calculationParameter = tdc_stim_calc_para_and_cfx_share();
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

                if (calculationParameter)
                {
                    if (!tdc_isd_is_stim_para_setting_done())
                    {
                        // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                        error = tdc_isd_stim_setting_step(isdControlStateChagedFlag);

                        if (error)
                        {
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                        }

                        // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                    }
                    else
                    {
#if 1
                        tdc_isd_change_state(en__isdStatus_stimul_10V_Ok);
                        //////////////////////////////////////

                        tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                        tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                        tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                        TDC_PRINTF_I("[LIVE] LIVE STIMULATION, SUCCESS TO ENABLE STIM 10V BY EN__START \r\n");
#endif

                        // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_live_stimulation;

                        //  sub-command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = en__Start;

                        // 송신 데이터 SPI TX버퍼에 복사
                        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;

                        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
                        // 자극 출력
                    }
                }
                else
                {
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                }
            }

            if (flowCounter > /*50*/ 1000)  // 실시간 자극 파라미터 설정 오류 (13msec)
            {
                // 여기서 에러
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__stimulationParameterUnloaded, __LINE__);

                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
            }
        }
        break;

        case en__StimulationVolumeAdjust:
        {
            if (ISD_state.conneded_ISD)
            {

                // 연결 상태 업데이트
                tdc_isd_update_link_by_backtel_live();

                tdc_shm_change_stimul_volume(p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

                // 송신 데이터 준비
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = en__StimulationVolumeAdjust;

                bufferForSPI_tx[buffer_tx_index++] = tdc_shm_read_stimul_volume();

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 라이브 자극 유지로 변경
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
            else
            {

                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
        }
        break;

        case en__MicSensitivityAdjust:
        {
            if (ISD_state.conneded_ISD)
            {
                // 연결 상태 업데이트
                tdc_isd_update_link_by_backtel_live();

                tdc_shm_change_audio_volume(p_mappingPacket->tdc_isd_map_live_step.audioVolume);

                // 송신 데이터 준비
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = en__MicSensitivityAdjust;

                bufferForSPI_tx[buffer_tx_index++] = tdc_shm_read_audio_volume();

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 라이브 자극 유지로 변경
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
        }
        break;

        case en__mapping_Stimul_indicator:
        {
            if (ISD_state.conneded_ISD)
            {
                // 연결 상태 업데이트
                tdc_isd_update_link_by_backtel_live();

                if (flowCounter == 0)
                {
                    p_mapDataSharedMemory                                   = tdc_shm_get_pointer_current_map_data();
                    p_mapDataSharedMemory->stimulationIndicatorChannelNum   = p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;
                    p_mapDataSharedMemory->stimulationIndicatorAmplitude_uA = p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorAmplitude_uA;

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
                        bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                        // pay-load 준비
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_Stimul_indicator;

                        // 송신 데이터 SPI TX버퍼에 복사
                        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                        // 라이브 자극 유지로 변경
                        p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
                    }
                }

                stimulationCounter_ms++;
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
        }
        break;

        case en__readEqualizer:  //
        {
            if (ISD_state.conneded_ISD)
            {
                // 연결 상태 업데이트
                tdc_isd_update_link_by_backtel_live();

                counterRX++;

                stimulDAC_setting = tdc_stim_read_dac_register_value();

                p_cfxStimulLevel_255 = tdc_shm_read_current_stimul_level_255();

                // 송신 데이터 준비
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = en__readEqualizer;

                // 255레벨의 자극을 uA로 변환
                //  offset 값 계산
                // 255레벨의 자극을 uA로 변환
                //  offset 값 계산
                if (stimulDAC_setting->DAC_offsetSlope_register == 0)  // offsetDAC_A 기울기 ,,, 2uA 기울기 오프셋
                {
                    value  = offsetDAC_A_Slope_QI5F12 * stimulDAC_setting->DAC_offsetLevel_register;
                    offset = value >> 12;  // offsetDAC_A 기울기
                }
                else  // offsetDAC_B 기울기 ,,, 4uA 기울기 오프셋
                {
                    value  = offsetDAC_B_Slope_QI5F12 * stimulDAC_setting->DAC_offsetLevel_register;
                    offset = value >> 12;  // offsetDAC_B 기울기
                }

                for (i = p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadStart_index - 1; i <= p_mappingPacket->tdc_isd_map_live_step.equlizer_ReadEnd_index - 1; i++)
                {

                    if (p_cfxStimulLevel_255[i] != 0)
                    {
                        switch (stimulDAC_setting->DAC_Slope_register)
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
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;

                stimulationCounter_ms++;
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;
            }
        }
        break;

        case en__readDeviceStatus:
        {
            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

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

            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__HoldOn;

            stimulationCounter_ms++;
        }
        break;

        case en__HoldOn:
        {
#if 1
            if (!ISD_state.conneded_ISD)
            {
                needResetting       = true;
                reConnectionCounter = 0;
            }
            else
            {
                if (needResetting)  // 연결이 끊어졌다가 다시 붙은 경우
                {
                    if (reConnectionCounter == 0)
                    {
                        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                        p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

                        // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
                        tdc_shm_change_audio_volume(p_mappingPacket->tdc_isd_map_live_step.audioVolume);
                        tdc_shm_change_stimul_volume(p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

                        // 매핑에서 받은 데이터를 전달한다.

                        p_mapDataSharedMemory->stimulationStrategy            = p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy;
                        p_mapDataSharedMemory->firstPulsePhase                = p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase;
                        p_mapDataSharedMemory->stimulationMode                = p_mappingPacket->tdc_isd_map_live_step.stimulationMode;
                        p_mapDataSharedMemory->stimulationPulsePhaseWidth     = p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth;
                        p_mapDataSharedMemory->numFrequencyBand               = p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand;
                        p_mapDataSharedMemory->stimulationIndicatorChannelNum = p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;

                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i];
                            p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i];
                            p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i];
                            p_mapDataSharedMemory->T_level_uA[i]                     = p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i];
                            p_mapDataSharedMemory->C_level_uA[i]                     = p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i];
                            p_mapDataSharedMemory->audio_input_x_mim[i]              = p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i];
                            p_mapDataSharedMemory->audio_input_x_max[i]              = p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i];
                        }

                        // 맵번호를 매핑용 번호로 전달하고// start 중에
                        if (toggle_mapNum)
                        {
                            tdc_shm_change_program_map_num(-1);
                        }

                        else
                        {
                            tdc_shm_change_program_map_num(-2);
                        }

                        toggle_mapNum++;
                        toggle_mapNum &= 0x1;
                    }

#if 0


                                                if(reConnectionCounter>2)
                                                {
                                                            if(tdc_isd_is_new_map_loaded_flag()) // 맵이 변경되어서  계산이 필요한 경우.
                                                            {

                                                                //Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                                                                calculationParameter=tdc_stim_calc_para_and_cfx_share();
                                                                tdc_stim_indicator_set_level_255(); // 자극 알림 크기 uA -> 255레벨로 변환


                                                                tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그


                                                                tdc_isd_clear_stim_para_setting_done();
                                                                tdc_isd_clear_new_map_loaded_flag();

                                                                isdControlStateChagedFlag=true;
                                                                //Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                                                            }
                                                            else
                                                                isdControlStateChagedFlag=false;


                                                            if(!tdc_isd_is_stim_para_setting_done())
                                                            {
                                                                //Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                                                                tdc_isd_stim_setting_step(isdControlStateChagedFlag);
                                                                //Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                                                            }
                                                            else
                                                            {
                                                                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
                                                                //자극 출력

                                                                needResetting=false;
                                                            }
                                                }
#else
                    if (tdc_shm_is_map_data_loaded_cfx())
                    {
                        if (tdc_isd_is_new_map_loaded_flag())  // 맵이 변경되어서  계산이 필요한 경우.
                        {
                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            calculationParameter = tdc_stim_calc_para_and_cfx_share();
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

                        if (calculationParameter)
                        {
                            if (!tdc_isd_is_stim_para_setting_done())
                            {
                                // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                                error = tdc_isd_stim_setting_step(isdControlStateChagedFlag);

                                if (error)
                                {
                                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                                    p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                                }

                                // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                            }
                            else
                            {
#if 1
                                tdc_isd_change_state(en__isdStatus_stimul_10V_Ok);
                                //////////////////////////////////////

                                tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                                tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                                tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                                TDC_PRINTF_I("[LIVE] LIVE STIMULATION, SUCCESS TO ENABLE STIM 10V BY EN__HOLDON \r\n");
#endif

                                // 자극 출력
                                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);

                                needResetting = false;
                            }
                        }
                        else
                        {
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                            p_mappingPacket->tdc_isd_map_live_step.subCommand = en__Standby;
                        }
                    }

                    reConnectionCounter++;

#endif
                }
                else
                {
                    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
                    // 응답 데이터 생성
                    // 연결 상태 업데이트
                    tdc_isd_update_link_by_backtel_live();

                    stimulationCounter_ms++;

                    // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                }
            }

#else
            reConnectionCounter++;

            if (ISD_state.conneded_ISD)
            {
                // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
                // 응답 데이터 생성
                // 연결 상태 업데이트
                tdc_isd_update_link_by_backtel_live();

                stimulationCounter_ms++;

                reConnectionCounter = 0;

                // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
            }
            else  // 연결이 뜮어 지면 다시 자극 관련 파라미터를 설정한다.
            {

                if (ISD_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)
                {

                    if (reConnectionCounter == 0)
                    {

                        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                        p_mapDataSharedMemory = tdc_shm_get_pointer_current_map_data();

                        // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
                        tdc_shm_change_audio_volume(p_mappingPacket->tdc_isd_map_live_step.audioVolume);
                        tdc_shm_change_stimul_volume(p_mappingPacket->tdc_isd_map_live_step.stimulVolume);

                        // 매핑에서 받은 데이터를 전달한다.

                        p_mapDataSharedMemory->stimulationStrategy            = p_mappingPacket->tdc_isd_map_live_step.stimulationStrategy;
                        p_mapDataSharedMemory->firstPulsePhase                = p_mappingPacket->tdc_isd_map_live_step.firstPulsePhase;
                        p_mapDataSharedMemory->stimulationMode                = p_mappingPacket->tdc_isd_map_live_step.stimulationMode;
                        p_mapDataSharedMemory->stimulationPulsePhaseWidth     = p_mappingPacket->tdc_isd_map_live_step.stimulationPulsePhaseWidth;
                        p_mapDataSharedMemory->numFrequencyBand               = p_mappingPacket->tdc_isd_map_live_step.numFrequencyBand;
                        p_mapDataSharedMemory->stimulationIndicatorChannelNum = p_mappingPacket->tdc_isd_map_live_step.stimulationIndicatorChannelNum;

                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = p_mappingPacket->tdc_isd_map_live_step.usableStimulationElectrodIndex[i];
                            p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = p_mappingPacket->tdc_isd_map_live_step.usableReferenceElectrodIndex[i];
                            p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = p_mappingPacket->tdc_isd_map_live_step.CIS_FreqBandOrder[i];
                            p_mapDataSharedMemory->T_level_uA[i]                     = p_mappingPacket->tdc_isd_map_live_step.T_level_uA[i];
                            p_mapDataSharedMemory->C_level_uA[i]                     = p_mappingPacket->tdc_isd_map_live_step.C_level_uA[i];
                            p_mapDataSharedMemory->audio_input_x_mim[i]              = p_mappingPacket->tdc_isd_map_live_step.audio_input_x_mim[i];
                            p_mapDataSharedMemory->audio_input_x_max[i]              = p_mappingPacket->tdc_isd_map_live_step.audio_input_x_max[i];
                        }

                        // 맵번호를 매핑용 번호로 전달하고// start 중에
                        if (toggle_mapNum)
                            tdc_shm_change_program_map_num(-1);

                        else
                            tdc_shm_change_program_map_num(-2);

                        toggle_mapNum++;
                        toggle_mapNum &= 0x1;
                    }

                    if (reConnectionCounter >= 1)
                    {
                        //
                        if (tdc_isd_is_new_map_loaded_flag())  // 맵이 변경되어서  계산이 필요한 경우.
                        {

                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            calculationParameter = tdc_stim_calc_para_and_cfx_share();
                            tdc_stim_indicator_set_level_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                            tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                            tdc_isd_clear_stim_para_setting_done();
                            tdc_isd_clear_new_map_loaded_flag();

                            isdControlStateChagedFlag = true;
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                        else
                            isdControlStateChagedFlag = false;

                        if (!tdc_isd_is_stim_para_setting_done())
                        {
                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            tdc_isd_stim_setting_step(isdControlStateChagedFlag);
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                        else
                        {

                            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);
                            // 자극 출력
                        }
                    }

                    reConnectionCounter++;
                }
                else
                {
                    reConnectionCounter = 0;
                }
            }
#endif
        }
        break;

        case en__Stop:
        {
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

            // 파라미터 초기화
            stimulationCounter_ms = 0;

            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

            // pay-load 준비
            bufferForSPI_tx[buffer_tx_index++] = en__Stop;

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

            // 커맨드 리셋;
            tdc_ble_mapping_clear_command();
        }
        break;

        case en__Standby:
        {
            if (ISD_state.conneded_ISD)
            {
                //tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

                if (ISD_connectionCounter_withMapping == 0)
                {
                    TDC_PRINTF_V("\r\n[MAPPING] LINK CHECK TRIGGERED (LIVE, STANDBY) \r\n");
                }

                tdc_isd_update_link_by_backtel_mapping(ISD_connectionCounter_withMapping);

                ISD_connectionCounter_withMapping++;

                if (ISD_connectionCounter_withMapping > 300)
                {
                    ISD_connectionCounter_withMapping = 0;
                }
            }
            else
            {
                ISD_connectionCounter_withMapping = 0;
            }
        }

        break;
    }

    flowCounter++;

    prev_subCommand=p_mappingPacket->tdc_isd_map_live_step.subCommand;

    return sitmulationIndicatorTrigger;
}
