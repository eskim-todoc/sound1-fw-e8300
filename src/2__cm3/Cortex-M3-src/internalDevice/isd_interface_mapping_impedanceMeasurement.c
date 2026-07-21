#include <hw.h>
#include <isdExecution/driver_PCM.h>
#include <stdbool.h>
#include "error.h"
#include "FPGA.h"
#include "isd_interface_FPGA.h"
#include "internalStimulationChip.h"
#include "isd_interface.h"
#if 0
#include "tdc_hal_i2c_cfx.h"
#else
#include "tdc_hal_i2c_isd.h"
#endif
#include "definitionsForAlgorithm.h"

#include "isd_interface.h"
#include "isd_interface_init_FPGA.h"
#include "cfx_cm3_sharedMemory.h"
#include "mappingControl.h"
#include "tdc_hal_spi.h"
#include "isd_interface_mapping_impedanceMeasurement.h"

#include "electrodeMapping.h"

#include <tdc_printf.h>

static char impedanceValue[df_maxIterationNum_impedance][2][2];

void impedanceMeasurement(bool startFlag)
{
    int  w_FPGA_registerValue;
    int  r_FPGA_registerValue;
    int  comparing;
    int  pulseWidth;
    bool FPGA_FIFO_empty;
    bool FPGA_error;
    int  backtelCounter;

    int w_isd_registerValue;
    int r_isd_registerValue;
    int bitReverse;
    int last_fpga_settingValue = 0;
    int w_FPGA_VolatileValue;

    static int  iterationNum                = 0;
    static int  electrodeNum                = 0;
    static int  flowCounter                 = 0;
    static bool monopolarImpedanceCheckDone = false;
    int         pcm_index                   = 0;
    int         i;

    // 계산되는 자극 파라미터
    static int numFramePerChannel_startWidth;
    static int numFramePerChannel_endWidth;
    static int numFramePerChannel;
    int        numFramePerChannel_index;
    int        duration;
    int        tokenTime;
    int        deliveryCharge_pico;

    static int  stimulDAC_slope;
    static int  stimulDAC_offsetResolution;
    static int  stimulDAC_offsetValue;
    static int  stimulDAC_offsetValue_uA;
    static int  stimulLevel_255 = 0;
    int         stimulLevel_uA;
    static bool toggle_start_end_pulseWidth = false;

    int backtelFIFO[2];

    static int ble_transfer_index;
    static int flowCounter_SendingSPI = 1000;
    int        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;

    //////////////////////

    static ST__MAPPING_PACKET *mappingPacket;

    int tempInt;

    pcm_index = 0;

    if (startFlag)
    {
        TDC_PRINTF_V("[MAPPING] CHECK IMPEDANCE, START FLAG IS SET \r\n");

        mappingPacket = getMappingPacket();

        flowCounter  = 0;
        iterationNum = 0;

        if (mappingPacket->impedanceCheck.channel == 255)
        {
            electrodeNum = 0;
        }
        else
        {
            electrodeNum = mappingPacket->impedanceCheck.channel - 1;
        }

        ble_transfer_index = 0;

        monopolarImpedanceCheckDone = false;
        toggle_start_end_pulseWidth = true;
    }

    switch (flowCounter)
    {
        case 0:
        {
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            // 펄스폭에 따른 자극 프레임 갯수 계산

            // 소요시간 계산 - 측정 시작 시 펄스폭

            // 좁은 펄스폭 적용시 프레임 갯수
            // 펄스폭 시간 (펄스 위상 x 2)
            duration = mappingPacket->impedanceCheck.pulseWidth_start_usec;
            duration += duration;

            // 펄스 위상 반전 간격
            duration += FPGA_interphaseGapTokenTime;

            // 자극파라미터 전송 시간
            duration += FPGA_electrodAndStimulLevelTokenTime;

            tokenTime                     = FPGA_oneChannelDataTokenTime;
            numFramePerChannel_startWidth = 1;

            while (true)
            {
                if (duration < tokenTime)
                {
                    break;
                }
                numFramePerChannel_startWidth++;
                tokenTime += FPGA_oneChannelDataTokenTime;
            }

            // 좁은 펄스폭 적용시 프레임 갯수
            // 펄스폭 시간 (펄스 위상 x 2)
            duration = mappingPacket->impedanceCheck.pulseWidth_end_usec;
            duration += duration;

            // 펄스 위상 반전 간격
            duration += FPGA_interphaseGapTokenTime;

            // 자극파라미터 전송 시간
            duration += FPGA_electrodAndStimulLevelTokenTime;

            tokenTime                   = FPGA_oneChannelDataTokenTime;
            numFramePerChannel_endWidth = 1;

            while (true)
            {
                if (duration < tokenTime)
                {
                    break;
                }
                numFramePerChannel_endWidth++;
                tokenTime += FPGA_oneChannelDataTokenTime;
            }

            // 최대 전하량 계산
            deliveryCharge_pico = (mappingPacket->impedanceCheck.pulseWidth_end_usec) * (mappingPacket->impedanceCheck.stimulationLevel_uA);  // usec단위.. (time*10^-6}*{level*10^-6)

            if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)  // 전하량 초과
            {
                TDC_PRINTF_E("[MAPPING] IMPEDANCE CHECK - MAX CHARGE OVER \r\n");
                sendErrorToApp(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__MaxChargeOver, __LINE__);  // 에러 전송
                clear_mappingCommand();                                                                       // 커맨드 리셋;
            }

#if 1
            // 가장 출력이 큰 DAC  사용
            stimulDAC_slope            = Stimulation_DAC_D;
            stimulDAC_offsetResolution = Offset_DAC_A;

            if (mappingPacket->impedanceCheck.stimulationLevel_uA < (stimulDAC_D_only_Saturation_uA + offsetDAC_A_Saturation_uA))
            {
                if (mappingPacket->impedanceCheck.stimulationLevel_uA > stimulDAC_D_only_Saturation_uA)  // 오프셋을 적용하지 않은 상태에서 출력할 수 없으면 오프셋 적용
                {
                    stimulDAC_offsetValue_uA = stimulDAC_A_only_Saturation_uA;  // 510
                }
                else
                {
                    stimulDAC_offsetValue_uA = 0;
                }

                tempInt               = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                stimulDAC_offsetValue = tempInt >> 15;

                stimulLevel_uA  = mappingPacket->impedanceCheck.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_D;
                stimulLevel_255 = tempInt >> 15;  // 기울기 스텝으로 나눈다.

#else
            // 작은 출력으로 임피던스를 측정하기 때문에.. 가장 해상도가 높게 DAC를 설정해서 사용한다.
            stimulDAC_slope            = Stimulation_DAC_A;
            stimulDAC_offsetResolution = Offset_DAC_A;

            if (mappingPacket->impedanceCheck.stimulationLevel_uA < (stimulDAC_A_only_Saturation_uA + offsetDAC_A_Saturation_uA))
            {
                if (mappingPacket->impedanceCheck.stimulationLevel_uA > stimulDAC_A_only_Saturation_uA)  // 오프셋을 적용하지 않은 상태에서 출력할 수 없으면 오프셋 적용
                {
                    stimulDAC_offsetValue_uA = stimulDAC_A_only_Saturation_uA;  // 510
                }
                else
                {
                    stimulDAC_offsetValue_uA = 0;
                }

                tempInt               = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                stimulDAC_offsetValue = tempInt >> 15;

                stimulLevel_uA  = mappingPacket->impedanceCheck.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                stimulLevel_255 = tempInt >> 15;  // 기울기 스텝으로 나눈다.
#endif
            }
            else  // 최소 기울기에 대해서만 일단 구현.. 모든 범위의 출력을 설정하려면 추가 코딩이 필요하나.. 임피던스는 작은 출력으로 측정하기 때문에 필요가 없을 듯하다.
            {
                TDC_PRINTF_E("[MAPPING] IMPEDANCE CHECK - STIMUL LEVEL OVER \r\n");
                sendErrorToApp(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__stimulLevelOver, __LINE__);  // 에러 전송
                clear_mappingCommand();                                                                         // 커맨드 리셋;
            }
        }
        break;

        case 1:
        {
            // 이전에 전송된 자극 파라미터 값이 있을 수 있기 때문에 출력을 내보내기 위하여 설정 파라미터들 전송한다

            // 펄스폭 0으로 설정 - ISD 설정 커맨드를 보낼때는 펄스폭을 0으로 설정해서 보내는 것이 심플함.
            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

            // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
            fill_pcmBuff_lastSimulationOut(&pcm_index);

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // NOP-Standby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // PCM 출력 모드 변경
        }
        break;

        case 2:
        {
            if (write_FPGA_clear_FIFO())
            {
                // FPGA 상태를 읽어 본다.
                if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
                {
                    if (!FPGA_FIFO_empty)
                    {
                        TDC_PRINTF_E("[MAPPING] IMPEDANCE CHECK - FIFO NOT EMPTY \r\n");
                        change_isd_state(en__isdStatus_PowerIC_OK);  //
                    }
                }
            }

            // FPGA backtel 레지스터 설정
            change_12BitBacktel_mode(pcm_index++);

            // 나머지는 Nop으로 채움.
            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);   // NOP-Standby
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);  // PCM 출력 모드 변경
        }
        break;

        case 4://3:
        {
            // 자극 출력 DAC 설정
            // ISD 0x5   - DAC Offset 쓰기
            w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetValue;
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);  //  0x0b00  | 0x50000
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // ISD 0x6    - 자극 파라미터 설정  쓰기
            w_isd_registerValue = ISD_registerAddr_StimulationConfig;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            // 0, 7번비트
            w_isd_registerValue = w_isd_registerValue << 1;
            // offsetResolution 6번 비트
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetResolution;

            // DAC slope    4,5번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | stimulDAC_slope;

            // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
            w_isd_registerValue = w_isd_registerValue << 2;

#if 1
            // monopolr_mp2 :
            w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;  // en__monopolr_rod =2

#else
            // monopolr_mp1 :
            w_isd_registerValue = w_isd_registerValue | en__monopolr_body;  // en__monopolr_body=1

            // monopolr_mp2 :
            w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;  // en__monopolr_rod =2

            //  monopolr_mp3 :
            w_isd_registerValue = w_isd_registerValue | monopolr_mp3;  // en__monopolr_BothRodBody =3
#endif

            // 자극 출력 모드 - 모노폴라
            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | 0;

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x0d04 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // ISD 0x9    - 임피던스 측정 설정
            w_isd_registerValue = ISD_registerAddr_adc_measurement;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | adc_measurementMode_impedance;  // 측정 모드 임피던스

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | adcSamplingRate20kHz;  // adc 샘플링 주파수

            w_isd_registerValue = w_isd_registerValue << 2;

#if 1
            w_isd_registerValue = w_isd_registerValue | mesurementStart_on_configureADCregister;  // adc 측정 시작 시점  :ADC 0x08 레지스터 설정  시점
#else
            w_isd_registerValue = w_isd_registerValue | mesurementStart_on_stiulationOut;  // adc 측정 시작 시점 : 자극 출력 시점
#endif

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x1305 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            // fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);

            // ISD 0x8    - 측정 채널

            w_isd_registerValue = ISD_registerAddr_adc_samplingChannel;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | electrodeMap[electrodeNum];

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x1100 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 나머지 버퍼는  NOP-Backtel
            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopBacktel);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopBacktel);
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);
        }
        break;

        case 6://4:
        {
            changePcmOutputMode(PcmBitStream_Mode_NopBacktel);
        }
        break;

        case 8://5:
        {
            // 펄스 폭 설정 - FPGA에 전송
            if (toggle_start_end_pulseWidth)
            {
                pulseWidth                  = (mappingPacket->impedanceCheck.pulseWidth_start_usec);
                numFramePerChannel          = numFramePerChannel_startWidth;
                toggle_start_end_pulseWidth = false;
            }
            else
            {
                pulseWidth                  = (mappingPacket->impedanceCheck.pulseWidth_end_usec);
                numFramePerChannel          = numFramePerChannel_endWidth;
                toggle_start_end_pulseWidth = true;
            }

            chang_PulseWidth(pcm_index++, pulseWidth);
            upadte_fpga_pulsePhaseWidth_written_Value(pulseWidth);

            // 자극 출력 파라미터

            w_isd_registerValue = positivePulseFirst << firstPulsePhasePositionAtPCM_Mold;                                // Positive Pulse first;
            w_isd_registerValue = w_isd_registerValue | (electrodeMap[electrodeNum] << electrodIndexPositionAtPCM_Mold);  // 자극 전극 번호
            w_isd_registerValue = w_isd_registerValue | (stimulLevel_255 << stimulationPositionAtPCM_Mold);               // 자극 출력 크기

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 펄스 폭을 맞추기 위한  Nop-Token

            for (numFramePerChannel_index = 1; numFramePerChannel_index < numFramePerChannel; numFramePerChannel_index++)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            //

            if (pcm_index >= df_MaxNumTransferableChannel)
            {
                sendErrorToApp(en__mapping_impedanceChekck, en__EN__PCM_GEN_ERROR, en__PCMBufferOwerFlow, __LINE__);  // 에러 전송
                clear_mappingCommand();                                                                               // 커맨드 리셋;
            }
            else
            {
                for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                {
                    fillSepcificCommndBuffer(i, pcm_Mold_NopStandby);
                }
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);
        }
        break;

        case 10://6:
        {
            // 측정용 자극 출력 또는 0x09 설정 파라미터

#if 1  // ADC 설정 파라미터 전송시 측정 시작 방법

            w_isd_registerValue = ISD_registerAddr_adc_measurement;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | adc_measurementMode_impedance;  // 측정 모드 임피던스

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | adcSamplingRate20kHz;  // adc 샘플링 주파수

            w_isd_registerValue = w_isd_registerValue << 2;
            w_isd_registerValue = w_isd_registerValue | mesurementStart_Disable;  // adc 측정 시작 시점  -- 이값을 리셋해준다.

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x1304 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

#else

            w_isd_registerValue = 1 << firstPulsePhasePositionAtPCM_Mold;                                    // Positive Pulse first;
            w_isd_registerValue = w_isd_registerValue | (electrodeNum << electrodIndexPositionAtPCM_Mold);   // 자극 전극 번호
            w_isd_registerValue = w_isd_registerValue | (stimulLevel_255 << stimulationPositionAtPCM_Mold);  // 자극 출력 크기

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

#endif

            // 펄스 폭을 맞추기 위한  Nop-Token
#if 0
                        for(numFramePerChannel_index=1; numFramePerChannel_index<numFramePerChannel; numFramePerChannel_index++)
                            fillSepcificCommndBuffer(pcm_index++,pcm_Mold_NopStandby);

#else
            for (numFramePerChannel_index = 1; numFramePerChannel_index < (numFramePerChannel - 1); numFramePerChannel_index++)  // 펄스폭이 넓어질 경우 NOP과 Backtel NOP의 타이밍 문제로 NOP의 개수를 1개 줄이고 Backtel NOP으로 대체
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }
#endif
            // 나머지 버퍼는  NOP-Backtel

            if (pcm_index >= df_MaxNumTransferableChannel)
            {
                sendErrorToApp(en__mapping_impedanceChekck, en__EN__PCM_GEN_ERROR, en__PCMBufferOwerFlow, __LINE__);  // 에러 전송
                clear_mappingCommand();                                                                               // 커맨드 리셋;
            }
            else
            {
                for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
                {
                    fillSepcificCommndBuffer(i, pcm_Mold_NopBacktel);
                }
            }

            changeNextPcmOutputMode(PcmBitStream_Mode_NopBacktel);
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);
        }
        break;

        case 12://10:
        {
            changePcmOutputMode(PcmBitStream_Mode_NopBacktel);
        }
        break;

        case 14://12:  // 임피던스 측정값 읽음
        {
            // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);

            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            if (check_FPGA_PCM_Error(&FPGA_error))
            {
                if (!FPGA_error)
                {
                    // 백텔 수신 확인
                    if (read_FPGA_FIFO_counter(&backtelCounter))
                    {
                        if (backtelCounter == 2)
                        {
                            // 백텔 데이터
                            // if (tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_BackTel, backtelFIFO, 2))
                            if (read_FPGA_backtel_FIFO(backtelFIFO, 2))
                            {

                                if (toggle_start_end_pulseWidth)  // toggle_start_end_pulseWidth // 긴 폭의 측정이 완료되었다.- 1회 측정 완료.
                                {

                                    // 펄스 폭 넚은 것의 측정값
                                    impedanceValue[iterationNum][en__endPulse_saving_index][FIPGA_FIFO_index_0] = (char) backtelFIFO[FIPGA_FIFO_index_0];
                                    impedanceValue[iterationNum][en__endPulse_saving_index][FIPGA_FIFO_index_1] = (char) backtelFIFO[FIPGA_FIFO_index_1];

                                    iterationNum++;
                                    if (iterationNum < mappingPacket->impedanceCheck.iterationNum)  // 한 채널의 측정이 완료
                                    {
                                        flowCounter = 0;  // 반복횟수가 완료될 때까지 측정을 다시 한다.
                                    }
                                    else
                                    {
                                        flowCounter_SendingSPI = flowCounter;
                                    }
                                }
                                else  //  짧은 폭의 측정이 완료 되었다.
                                {
                                    // 좁은 펄스폭
                                    impedanceValue[iterationNum][en__startPulse_saving_index][FIPGA_FIFO_index_0] = (char) backtelFIFO[FIPGA_FIFO_index_0];
                                    impedanceValue[iterationNum][en__startPulse_saving_index][FIPGA_FIFO_index_1] = (char) backtelFIFO[FIPGA_FIFO_index_1];
                                    flowCounter                                                                   = 0;
                                }
                            }
                        }
                        else
                        {
                            change_isd_state(en__isdStatus_FPGA_Ok);                                                           // 백텔 안들어옴 에러 // 내부기 전송 파워 설정 부터 다시.
                            sendErrorToApp(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);  // 에러 전송
                            clear_mappingCommand();                                                                            // 커맨드 리셋;
                        }
                    }
                }
                else
                {
                    change_isd_state(en__isdStatus_PowerIC_OK);                                                                 // FPGA 에러 발생, FPGA 초기화
                    sendErrorToApp(en__mapping_eCAP_Measurement_masking, en__FPGA_CONFIGUARATION_ERROR, FPGA_error, __LINE__);  // 에러 전송 // FPGA 에러 값을 그대로 전달
                    clear_mappingCommand();                                                                                     // 커맨드 리셋;
                }
            }
        }
        break;

        default:
            break;
    }  // end, switch (flowCounter)

    // if(flowCounter>=flowCounter_SendingSPI)// 한채널의 반복 측정이 완료된 상태이다.
    if (flowCounter > flowCounter_SendingSPI)  // 한채널의 반복 측정이 완료된 상태이다.
    {

        buffer_tx_index = 0;

        if (ble_transfer_index >= mappingPacket->impedanceCheck.iterationNum)  // 측정데이터 전송이 완료 되었으면  다음
        {
            ble_transfer_index = 0;

            // 측정 진행 여부 확인

            if (mappingPacket->impedanceCheck.channel == 255)  // 전 채널 측정일 경우 채널을 증가 시키면서 측정한다.
            {
                electrodeNum++;
#if 1
                if (electrodeNum >= df_MaxNumOfElectrode)
                {
                    clear_mappingCommand();  // 명령 종료 // 커맨드 리셋;
                }
#else
                if (electrodeNum >= 16)  // 현재 실험보드에서 16번 전극 이후는 측정하면 에러 발생
                {
                    clear_mappingCommand();  // 명령 종료 // 커맨드 리셋;
                }
#endif
                iterationNum = 0;
                flowCounter  = 0;
            }
            else  // 단일 채널일 경우 반복 측정이 완료 되었으므로 종료
            {
                clear_mappingCommand();  // 명령 종료 // 커맨드 리셋;
            }
        }
        else
        {
            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = mappingPacket->command;

            // pay-load 준비
            bufferForSPI_tx[buffer_tx_index++] = en__MonoPolar_impedance;

            bufferForSPI_tx[buffer_tx_index++] = electrodeNum + 1;  // 채널 번호 1~32

            for (i = 0; i < Max_ImpedanceReturnDataSize; i++)  // ble 패킷 사이즈로 인하여..1회 전달 시 최대, 4번 측정한 데이터 전달 가능.
            {
                if (ble_transfer_index < mappingPacket->impedanceCheck.iterationNum)
                {
                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[ble_transfer_index][en__startPulse_saving_index][FIPGA_FIFO_index_0];
                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[ble_transfer_index][en__startPulse_saving_index][FIPGA_FIFO_index_1];

                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[ble_transfer_index][en__endPulse_saving_index][FIPGA_FIFO_index_0];
                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[ble_transfer_index][en__endPulse_saving_index][FIPGA_FIFO_index_1];

                    ble_transfer_index++;
                }
                else
                {
                    break;
                }
            }

            // 송신 데이터 SPI TX버퍼에 복사
            tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);
        }
    }

    flowCounter++;
}
