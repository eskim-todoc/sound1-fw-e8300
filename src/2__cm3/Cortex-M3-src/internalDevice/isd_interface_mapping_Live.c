#include "tdc_sys_error.h"
#include "cfx_cm3_sharedMemory.h"
#include "mappingControl.h"
#include "isd_interface.h"
#include "isd_interface_stimulationParaSetting.h"
#include "isd_interface_mapping_SepcificStimulation.h"
#include "indicatorByStimul.h"

#include "FPGA.h"
#include "isd_interface_FPGA.h"
#include "stimulationParaCal.h"

#include "isd_interface_stimulationStandAlone.h"
#include "isd_interface_mapping_Live.h"
#include "tdc_pwr_battery.h"
// live 실행을 받으면 맵 번호 인덱스를 마이너스 값으로 변경하여  CFX에서 맵데이터를 실행한다.
#include "tdc_sys_error.h"
#include "electrodeMapping.h"
#include "tdc_hal_spi.h"

#include <tdc_printf.h>

// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

int counterRX = 0;

bool liveStimulation(ST__ISD_STATUS ISD_state)
{
    static EN__LIVE_STIMULATION_SUB_COMMAND prev_subCommand     = en__HoldOn;
    static int                              programNum_original = 99;  // 매핑 프로그램과 연결되기 전에 단독으로 사용되고 있을 때 설정된 매핑프로그램 번호 저장 용.

    static int stimulationTime_ms    = 0;
    static int stimulationCounter_ms = 0;

    ST__CFX_CM3_SharedMemory_mapData         *p_mapDataSharedMemory;
    static const ST_STIUL_DAC_REGISTER_VALUE *stimulDAC_setting;

    static ST__MAPPING_PACKET *p_mappingPacket;

    static int  toggle_mapNum                     = 0;
    static int  evenOdd                           = 0;
    static int  reConnectionCounter               = 0;
    static int  ISD_connectionCounter_withMapping = 0;
    static bool needResetting                     = false;

    int        i;
    int       *p_cfxStimulLevel_255;
    static int flowCounter;
    int        stimulationLevel_uA;
    int        stimul_255;
    int        offset;
    int        tempA, tempB;
    int        value;

    int bufferForSPI_tx[BLE_DataPacketSize];
    int buffer_tx_index;

    static bool ISD_is_connected     = true;
    static bool calculationParameter = false;

    bool isdControlStateChagedFlag;
    bool sitmulationIndicatorTrigger = false;
    bool error                       = false;

    buffer_tx_index = 0;

    p_mappingPacket = (ST__MAPPING_PACKET *) getMappingPacket();

    if (prev_subCommand != p_mappingPacket->liveStimulation.subCommand)
    {
        flowCounter = 0;
    }

    if (!ISD_state.conneded_ISD)
    {
        tempA = 234;
    }

    switch (p_mappingPacket->liveStimulation.subCommand)
    {
        case en__allParameter:
        {
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            p_mapDataSharedMemory = getPointerCurrentMapData();

            // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
            changeAudioVolume(p_mappingPacket->liveStimulation.audioVolume);
            changeStimulVolume(p_mappingPacket->liveStimulation.stimulVolume);

            // 매핑에서 받은 데이터를 전달한다.

            p_mapDataSharedMemory->stimulationStrategy              = p_mappingPacket->liveStimulation.stimulationStrategy;
            p_mapDataSharedMemory->firstPulsePhase                  = p_mappingPacket->liveStimulation.firstPulsePhase;
            p_mapDataSharedMemory->stimulationMode                  = p_mappingPacket->liveStimulation.stimulationMode;
            p_mapDataSharedMemory->stimulationPulsePhaseWidth       = p_mappingPacket->liveStimulation.stimulationPulsePhaseWidth;
            p_mapDataSharedMemory->numFrequencyBand                 = p_mappingPacket->liveStimulation.numFrequencyBand;
            p_mapDataSharedMemory->stimulationIndicatorChannelNum   = p_mappingPacket->liveStimulation.stimulationIndicatorChannelNum;
            p_mapDataSharedMemory->stimulationIndicatorAmplitude_uA = p_mappingPacket->liveStimulation.stimulationIndicatorAmplitude_uA;

            for (i = 0; i < df_MaxNumOfElectrode; i++)
            {
                p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = p_mappingPacket->liveStimulation.usableStimulationElectrodIndex[i];
                p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = p_mappingPacket->liveStimulation.usableReferenceElectrodIndex[i];
                p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = p_mappingPacket->liveStimulation.CIS_FreqBandOrder[i];
                p_mapDataSharedMemory->T_level_uA[i]                     = p_mappingPacket->liveStimulation.T_level_uA[i];
                p_mapDataSharedMemory->C_level_uA[i]                     = p_mappingPacket->liveStimulation.C_level_uA[i];
                p_mapDataSharedMemory->audio_input_x_mim[i]              = p_mappingPacket->liveStimulation.audio_input_x_mim[i];
                p_mapDataSharedMemory->audio_input_x_max[i]              = p_mappingPacket->liveStimulation.audio_input_x_max[i];
            }

            // 맵번호를 매핑용 번호로 전달하고// start 중에
            if (toggle_mapNum)
            {
                changeProgramMapNum(-1);
            }
            else
            {
                changeProgramMapNum(-2);
            }

            toggle_mapNum++;
            toggle_mapNum &= 0x1;

            // BLE 응답 전송
            // 패치 단계에서 전송을 완료했다.

            // 모드 변경;
            p_mappingPacket->liveStimulation.subCommand = en__Standby;
            TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__ALL_PARAMERTER, SUB COMMAND : STANDBY \r\n");
        }
        break;

        case en__Start:
        {
            // CFX에서 맵데이터의 로딩이 완료될 때까지 로딩
            if (isMapdateLoaded_CFX())
            {
                // TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__START, CFX IS MAPDATA LOADED \r\n");

                if (isNewMapLoadeFlag())  // 맵이 변경되어서  계산이 필요한 경우.
                {
                    TDC_PRINTF_I("[LIVE] LIVE STIMULATION, EN__START, NEW MAP LOADED FLAG IS TRUE \r\n");

                    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                    calculationParameter = calculationStimulParaN_cfxShare();
                    setIndicatoStimlulLevel_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                    setFlag_AudioParametersCalculationDone_Cm3ToCfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                    TDC_PRINTF_I("[LIVE] LIVE STIMULATION, CFX WILL CALCULATE AUDIO PARAMETERS, NOW \r\n");

                    clear_newMapLoadeFlag();  // 새로운 맵 데이터의 적용을 위한 처리가 완료되었다.

                    clear_sitmulationParaSettingDone();  //

                    isdControlStateChagedFlag = true;
                    // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                }
                else
                {
                    isdControlStateChagedFlag = false;
                }

                if (calculationParameter)
                {
                    if (!isStimulParaIsSettingDone())
                    {
                        // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                        error = stimulationSetting(isdControlStateChagedFlag);

                        if (error)
                        {
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                            p_mappingPacket->liveStimulation.subCommand = en__Standby;
                        }

                        // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                    }
                    else
                    {
#if 1
                        change_isd_state(en__isdStatus_stimul_10V_Ok);
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

                        p_mappingPacket->liveStimulation.subCommand = en__HoldOn;

                        changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);
                        // 자극 출력
                    }
                }
                else
                {
                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                    p_mappingPacket->liveStimulation.subCommand = en__Standby;
                }
            }

            if (flowCounter > /*50*/ 1000)  // 실시간 자극 파라미터 설정 오류 (13msec)
            {
                // 여기서 에러
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__stimulationParameterUnloaded, __LINE__);

                p_mappingPacket->liveStimulation.subCommand = en__Standby;
            }
        }
        break;

        case en__StimulationVolumeAdjust:
        {
            if (ISD_state.conneded_ISD)
            {

                // 연결 상태 업데이트
                update_isd_LinkConnection_byBacktel_withLiveStimulation();

                changeStimulVolume(p_mappingPacket->liveStimulation.stimulVolume);

                // 송신 데이터 준비
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = en__StimulationVolumeAdjust;

                bufferForSPI_tx[buffer_tx_index++] = readStimulVolume();

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 라이브 자극 유지로 변경
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
            }
            else
            {

                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
            }
        }
        break;

        case en__MicSensitivityAdjust:
        {
            if (ISD_state.conneded_ISD)
            {
                // 연결 상태 업데이트
                update_isd_LinkConnection_byBacktel_withLiveStimulation();

                changeAudioVolume(p_mappingPacket->liveStimulation.audioVolume);

                // 송신 데이터 준비
                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = en__MicSensitivityAdjust;

                bufferForSPI_tx[buffer_tx_index++] = readAudioVolume();

                // 송신 데이터 SPI TX버퍼에 복사
                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                // 라이브 자극 유지로 변경
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
            }
        }
        break;

        case en__mapping_Stimul_indicator:
        {
            if (ISD_state.conneded_ISD)
            {
                // 연결 상태 업데이트
                update_isd_LinkConnection_byBacktel_withLiveStimulation();

                if (flowCounter == 0)
                {
                    p_mapDataSharedMemory                                   = getPointerCurrentMapData();
                    p_mapDataSharedMemory->stimulationIndicatorChannelNum   = p_mappingPacket->liveStimulation.stimulationIndicatorChannelNum;
                    p_mapDataSharedMemory->stimulationIndicatorAmplitude_uA = p_mappingPacket->liveStimulation.stimulationIndicatorAmplitude_uA;

                    setIndicatoStimlulLevel_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                    // set_stimulationIndicatorTrigger(); // 자극 알림 시작

                    sitmulationIndicatorTrigger = true;  // 자극 알림 시작
                }
                else
                {
                    if (!is_Triggered_stimulationIndicator())  // 자극 알림 종료 되었음?
                    {
                        // 송신 데이터 준비
                        // command loop-back
                        bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;

                        // pay-load 준비
                        bufferForSPI_tx[buffer_tx_index++] = en__mapping_Stimul_indicator;

                        // 송신 데이터 SPI TX버퍼에 복사
                        tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                        // 라이브 자극 유지로 변경
                        p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
                    }
                }

                stimulationCounter_ms++;
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
            }
        }
        break;

        case en__readEqualizer:  //
        {
            if (ISD_state.conneded_ISD)
            {
                // 연결 상태 업데이트
                update_isd_LinkConnection_byBacktel_withLiveStimulation();

                counterRX++;

                stimulDAC_setting = readStimulDAC_RegisterValue();

                p_cfxStimulLevel_255 = readCurrentStimulLevel_255();

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

                for (i = p_mappingPacket->liveStimulation.equlizer_ReadStart_index - 1; i <= p_mappingPacket->liveStimulation.equlizer_ReadEnd_index - 1; i++)
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
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;

                stimulationCounter_ms++;
            }
            else
            {
                tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__EN__ISD_ERROR, en__ISD_notConnected, __LINE__);

                // 라이브 자극 유지로 변경
                p_mappingPacket->liveStimulation.subCommand = en__HoldOn;
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

            p_mappingPacket->liveStimulation.subCommand = en__HoldOn;

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
                        changePcmOutputMode(PcmBitStream_Mode_NopStandby);

                        p_mapDataSharedMemory = getPointerCurrentMapData();

                        // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
                        changeAudioVolume(p_mappingPacket->liveStimulation.audioVolume);
                        changeStimulVolume(p_mappingPacket->liveStimulation.stimulVolume);

                        // 매핑에서 받은 데이터를 전달한다.

                        p_mapDataSharedMemory->stimulationStrategy            = p_mappingPacket->liveStimulation.stimulationStrategy;
                        p_mapDataSharedMemory->firstPulsePhase                = p_mappingPacket->liveStimulation.firstPulsePhase;
                        p_mapDataSharedMemory->stimulationMode                = p_mappingPacket->liveStimulation.stimulationMode;
                        p_mapDataSharedMemory->stimulationPulsePhaseWidth     = p_mappingPacket->liveStimulation.stimulationPulsePhaseWidth;
                        p_mapDataSharedMemory->numFrequencyBand               = p_mappingPacket->liveStimulation.numFrequencyBand;
                        p_mapDataSharedMemory->stimulationIndicatorChannelNum = p_mappingPacket->liveStimulation.stimulationIndicatorChannelNum;

                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = p_mappingPacket->liveStimulation.usableStimulationElectrodIndex[i];
                            p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = p_mappingPacket->liveStimulation.usableReferenceElectrodIndex[i];
                            p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = p_mappingPacket->liveStimulation.CIS_FreqBandOrder[i];
                            p_mapDataSharedMemory->T_level_uA[i]                     = p_mappingPacket->liveStimulation.T_level_uA[i];
                            p_mapDataSharedMemory->C_level_uA[i]                     = p_mappingPacket->liveStimulation.C_level_uA[i];
                            p_mapDataSharedMemory->audio_input_x_mim[i]              = p_mappingPacket->liveStimulation.audio_input_x_mim[i];
                            p_mapDataSharedMemory->audio_input_x_max[i]              = p_mappingPacket->liveStimulation.audio_input_x_max[i];
                        }

                        // 맵번호를 매핑용 번호로 전달하고// start 중에
                        if (toggle_mapNum)
                        {
                            changeProgramMapNum(-1);
                        }

                        else
                        {
                            changeProgramMapNum(-2);
                        }

                        toggle_mapNum++;
                        toggle_mapNum &= 0x1;
                    }

#if 0


                                                if(reConnectionCounter>2)
                                                {
                                                            if(isNewMapLoadeFlag()) // 맵이 변경되어서  계산이 필요한 경우.
                                                            {

                                                                //Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                                                                calculationParameter=calculationStimulParaN_cfxShare();
                                                                setIndicatoStimlulLevel_255(); // 자극 알림 크기 uA -> 255레벨로 변환


                                                                setFlag_AudioParametersCalculationDone_Cm3ToCfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그


                                                                clear_sitmulationParaSettingDone();
                                                                clear_newMapLoadeFlag();

                                                                isdControlStateChagedFlag=true;
                                                                //Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                                                            }
                                                            else
                                                                isdControlStateChagedFlag=false;


                                                            if(!isStimulParaIsSettingDone())
                                                            {
                                                                //Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                                                                stimulationSetting(isdControlStateChagedFlag);
                                                                //Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                                                            }
                                                            else
                                                            {
                                                                changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);
                                                                //자극 출력

                                                                needResetting=false;
                                                            }
                                                }
#else
                    if (isMapdateLoaded_CFX())
                    {
                        if (isNewMapLoadeFlag())  // 맵이 변경되어서  계산이 필요한 경우.
                        {
                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            calculationParameter = calculationStimulParaN_cfxShare();
                            setIndicatoStimlulLevel_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                            setFlag_AudioParametersCalculationDone_Cm3ToCfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                            clear_newMapLoadeFlag();  // 새로운 맵 데이터의 적용을 위한 처리가 완료되었다.

                            clear_sitmulationParaSettingDone();  //

                            isdControlStateChagedFlag = true;
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                        else
                        {
                            isdControlStateChagedFlag = false;
                        }

                        if (calculationParameter)
                        {
                            if (!isStimulParaIsSettingDone())
                            {
                                // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                                error = stimulationSetting(isdControlStateChagedFlag);

                                if (error)
                                {
                                    tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
                                    p_mappingPacket->liveStimulation.subCommand = en__Standby;
                                }

                                // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                            }
                            else
                            {
#if 1
                                change_isd_state(en__isdStatus_stimul_10V_Ok);
                                //////////////////////////////////////

                                tdc_sys_error_clear_flag(en__RF_PowerIC_ERROR);
                                tdc_sys_error_clear_flag(en__FPGA_CONFIGUARATION_ERROR);
                                tdc_sys_error_clear_flag(en__EN__ISD_ERROR);

                                TDC_PRINTF_I("[LIVE] LIVE STIMULATION, SUCCESS TO ENABLE STIM 10V BY EN__HOLDON \r\n");
#endif

                                // 자극 출력
                                changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);

                                needResetting = false;
                            }
                        }
                        else
                        {
                            tdc_sys_error_send_to_app(en__mapping_live_stimulation, en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
                            p_mappingPacket->liveStimulation.subCommand = en__Standby;
                        }
                    }

                    reConnectionCounter++;

#endif
                }
                else
                {
                    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                    changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);
                    // 응답 데이터 생성
                    // 연결 상태 업데이트
                    update_isd_LinkConnection_byBacktel_withLiveStimulation();

                    stimulationCounter_ms++;

                    // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                }
            }

#else
            reConnectionCounter++;

            if (ISD_state.conneded_ISD)
            {
                // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);
                // 응답 데이터 생성
                // 연결 상태 업데이트
                update_isd_LinkConnection_byBacktel_withLiveStimulation();

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

                        changePcmOutputMode(PcmBitStream_Mode_NopStandby);

                        p_mapDataSharedMemory = getPointerCurrentMapData();

                        // 오디오 볼륨(마이크 감도), 자극 볼륨을 변경하고
                        changeAudioVolume(p_mappingPacket->liveStimulation.audioVolume);
                        changeStimulVolume(p_mappingPacket->liveStimulation.stimulVolume);

                        // 매핑에서 받은 데이터를 전달한다.

                        p_mapDataSharedMemory->stimulationStrategy            = p_mappingPacket->liveStimulation.stimulationStrategy;
                        p_mapDataSharedMemory->firstPulsePhase                = p_mappingPacket->liveStimulation.firstPulsePhase;
                        p_mapDataSharedMemory->stimulationMode                = p_mappingPacket->liveStimulation.stimulationMode;
                        p_mapDataSharedMemory->stimulationPulsePhaseWidth     = p_mappingPacket->liveStimulation.stimulationPulsePhaseWidth;
                        p_mapDataSharedMemory->numFrequencyBand               = p_mappingPacket->liveStimulation.numFrequencyBand;
                        p_mapDataSharedMemory->stimulationIndicatorChannelNum = p_mappingPacket->liveStimulation.stimulationIndicatorChannelNum;

                        for (i = 0; i < df_MaxNumOfElectrode; i++)
                        {
                            p_mapDataSharedMemory->usableStimulationElectrodIndex[i] = p_mappingPacket->liveStimulation.usableStimulationElectrodIndex[i];
                            p_mapDataSharedMemory->usableReferenceElectrodIndex[i]   = p_mappingPacket->liveStimulation.usableReferenceElectrodIndex[i];
                            p_mapDataSharedMemory->CIS_FreqBandOrder[i]              = p_mappingPacket->liveStimulation.CIS_FreqBandOrder[i];
                            p_mapDataSharedMemory->T_level_uA[i]                     = p_mappingPacket->liveStimulation.T_level_uA[i];
                            p_mapDataSharedMemory->C_level_uA[i]                     = p_mappingPacket->liveStimulation.C_level_uA[i];
                            p_mapDataSharedMemory->audio_input_x_mim[i]              = p_mappingPacket->liveStimulation.audio_input_x_mim[i];
                            p_mapDataSharedMemory->audio_input_x_max[i]              = p_mappingPacket->liveStimulation.audio_input_x_max[i];
                        }

                        // 맵번호를 매핑용 번호로 전달하고// start 중에
                        if (toggle_mapNum)
                            changeProgramMapNum(-1);

                        else
                            changeProgramMapNum(-2);

                        toggle_mapNum++;
                        toggle_mapNum &= 0x1;
                    }

                    if (reConnectionCounter >= 1)
                    {
                        //
                        if (isNewMapLoadeFlag())  // 맵이 변경되어서  계산이 필요한 경우.
                        {

                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            calculationParameter = calculationStimulParaN_cfxShare();
                            setIndicatoStimlulLevel_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                            setFlag_AudioParametersCalculationDone_Cm3ToCfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                            clear_sitmulationParaSettingDone();
                            clear_newMapLoadeFlag();

                            isdControlStateChagedFlag = true;
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                        else
                            isdControlStateChagedFlag = false;

                        if (!isStimulParaIsSettingDone())
                        {
                            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                            stimulationSetting(isdControlStateChagedFlag);
                            // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                        }
                        else
                        {

                            changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);
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
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

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
            clear_mappingCommand();
        }
        break;

        case en__Standby:
        {
            if (ISD_state.conneded_ISD)
            {
                //changePcmOutputMode(PcmBitStream_Mode_NopStandby);

                if (ISD_connectionCounter_withMapping == 0)
                {
                    TDC_PRINTF_V("\r\n[MAPPING] LINK CHECK TRIGGERED (LIVE, STANDBY) \r\n");
                }

                update_isd_LinkConnection_byBacktel_withMapping(ISD_connectionCounter_withMapping);

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

    prev_subCommand=p_mappingPacket->liveStimulation.subCommand;

    return sitmulationIndicatorTrigger;
}
