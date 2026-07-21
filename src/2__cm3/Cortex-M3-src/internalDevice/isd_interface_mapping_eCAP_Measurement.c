#include <hw.h>
#include <isdExecution/driver_PCM.h>
#include <stdbool.h>
#include "tdc_sys_error.h"
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
#include "isd_interface_mapping_eCAP_Measurement.h"

#include "electrodeMapping.h"

// static int eCAP_value[4][64]={0,};

static int bufferForReading_eCAP[4][64] = {
    0,
};

static int PCM_templete_eCAP[2][df_MaxNumTransferableChannel]; // [ {프루브 자극 출력 파라미미터 - pulse폭 에 맞춘 NOP} : {마스커 프로브 인터벌 (펄스폭 0으로
                                                               // 설정,0x05,0x06 설정, 펄스폭 조정  ,, 인터벌 맞춤용 NOP)} : {프로브 자극 출력용 0x08 설정-펄스
                                                               // 폭에 맞춘 NOP})

#define masker_index 0
#define probe_index  1

#define MinNumFrameForConfigProbe 3 // 마스커와 프로브의 자극 크기가 달라서 DAC 기울기 및 오프셋 값이 동일하지 않을 경우, 4개의 데이터가 필요하고,

void file_PCM_templete_eCAP(EN__eCAP_Templete dataMode, int data)
{
    static int frameIndex  = 0;
    static int bufferIndex = 0;

    switch (dataMode)
    {

        case en__clearIndex:
        {
            frameIndex  = 0;
            bufferIndex = 0;
        }
        break;

        case en__fillForwardData:
        {
            if (frameIndex < 2)
            {

                PCM_templete_eCAP[frameIndex][bufferIndex++] = data;

                if (bufferIndex >= df_MaxNumTransferableChannel)
                {
                    frameIndex++;
                    bufferIndex = 0;
                }
            }
            else
            {
                // 버퍼 크기 초과
                tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__PCM_GEN_ERROR, en__PCM_TempleteBuff_OverFlow, __LINE__);

                // 커맨드 리셋;
                clear_mappingCommand();
            }
        }
        break;

        case en__fillBackwardData:
        {

            while (1)
            {
                if (frameIndex < 2)
                {

                    PCM_templete_eCAP[frameIndex][bufferIndex++] = data;

                    if (bufferIndex >= df_MaxNumTransferableChannel)
                    {
                        frameIndex++;
                        bufferIndex = 0;
                    }
                }
                else
                {
                    break;
                }
            }
        }
        break;
    }
}
void eCapMeasurement_masking(bool startFlag)
{

    int  w_FPGA_registerValue;
    int  r_FPGA_registerValue;
    int  comparing;
    int  tempValue;
    bool FPGA_FIFO_empty;
    bool FPGA_error;

    int w_isd_registerValue;
    int r_isd_registerValue;

    int last_fpga_settingValue = 0;
    int w_FPGA_VolatileValue;

    static int iterationNum = 0;

    static int flowCounter = 0;
    static int stimulPattern_index;

    int pcm_index;
    int templeteBuff_index;
    int i, k;
    int sendingPatternIndex;

    // 계산되는 자극 파라미터
    static int numFramePerChannel;
    int        duration;
    int        tokenTime;
    int        deliveryCharge_pico;

    static int stimulDAC_slope[2];
    static int stimulDAC_offsetResolution[2];
    static int stimulDAC_offsetValue[2];
    static int stimulLevel_255[2];

    int stimulLevel_uA, stimulDAC_offsetValue_uA;

    static bool maskerProbeDAC_diff;
    static int  backtelStart_flowCounter;
    int         maskerProbeIntervalCouter;
    static int  bipolarFIFO_index;

    static int backtelReceiveTime_ms;

    static int eCAP_mesureStartTime = 0;

    static int ble_transfer_index;
    int        bufferForSPI_tx[BLE_DataPacketSize];
    int        buffer_tx_index;

    int tempInt;
    //////////////////////

    static ST__MAPPING_PACKET const *mappingPacket;

    if (startFlag)
    {

        flowCounter         = 0;
        iterationNum        = 0;
        stimulPattern_index = en__probeAlone;
        // stimulPattern_index=en__maskerNprobe;
        backtelStart_flowCounter = 1000;

        ble_transfer_index = 0;

        file_PCM_templete_eCAP(en__clearIndex, 0);
    }

    pcm_index = 0;

    switch (flowCounter)
    {

        case 0:
        {
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);

            // 펄스폭에 따른 자극 프레임 갯수 계산

            mappingPacket = getMappingPacket();
            // 소요시간 계산 - 측정 시작 시 펄스폭
            //  펄스폭 시간 (펄스 위상 x 2)
            duration = mappingPacket->eCapMeasurement.pulseWidth;
            duration += duration;
            // 펄스 위상 반전 간격
            duration += FPGA_interphaseGapTokenTime;
            // 자극파라미터 전송 시간
            duration += FPGA_electrodAndStimulLevelTokenTime;

            tokenTime          = FPGA_oneChannelDataTokenTime;
            numFramePerChannel = 1;
            while (true)
            {
                if (duration < tokenTime)
                {
                    break;
                }
                numFramePerChannel++;
                tokenTime += FPGA_oneChannelDataTokenTime;
            }

            // 최대 전하량 계산

            if (mappingPacket->eCapMeasurement.stimulationLevel_uA_masker > mappingPacket->eCapMeasurement.stimulationLevel_uA_probe)
            {
                stimulLevel_uA = mappingPacket->eCapMeasurement.stimulationLevel_uA_masker;
            }
            else
            {
                stimulLevel_uA = mappingPacket->eCapMeasurement.stimulationLevel_uA_probe;
            }

            deliveryCharge_pico =
                (mappingPacket->eCapMeasurement.pulseWidth) * stimulLevel_uA; // usec단위.. (time*10^-6}*{level*10^-6) //마스커 자극값을 기준으로 계산한다.
            if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)                // 전하량 초과
            {
                // 에러 전송

                tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__MaxChargeOver, __LINE__);

                // 커맨드 리셋;
                clear_mappingCommand();
            }

            // 자극 슬로프 및 자극 데이터. 계산  stimulLevel_uA stimulLevel_uA stimulDAC_offsetValue_uA
            // 마스커에 대한 것
            if (mappingPacket->eCapMeasurement.stimulationLevel_uA_masker <
                (stimulDAC_A_only_Saturation_uA + offsetDAC_B_Saturation_uA)) // 2uA 기울기로 전달 가능한 범위내. // 1530 보다 작은 경우
            {

                // 자극 DAC 기울기 2uA
                stimulDAC_slope[masker_index] = Stimulation_DAC_A;

                if (mappingPacket->eCapMeasurement.stimulationLevel_uA_masker >
                    (stimulDAC_A_only_Saturation_uA + offsetDAC_A_Saturation_uA)) // 1020~1530  - > 1020 오프셋을 적용해야 되는 경우
                {

                    stimulDAC_offsetResolution[masker_index] = Offset_DAC_B;

                    stimulDAC_offsetValue_uA            = offsetDAC_B_Saturation_uA;
                    tempInt                             = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
                    stimulDAC_offsetValue[masker_index] = tempInt >> 15;

                    stimulLevel_uA                = mappingPacket->eCapMeasurement.stimulationLevel_uA_masker - stimulDAC_offsetValue_uA;
                    tempInt                       = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                    stimulLevel_255[masker_index] = tempInt >> 15;
                }
                else // 1020보다 작은 경우
                {
                    //
                    if (mappingPacket->eCapMeasurement.stimulationLevel_uA_masker > stimulDAC_A_only_Saturation_uA) // 510 ~ 1020 -> 510 오프셋을 적용한다.
                    {

                        stimulDAC_offsetResolution[masker_index] = Offset_DAC_A;
                        stimulDAC_offsetValue_uA                 = offsetDAC_A_Saturation_uA;

                        tempInt                             = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                        stimulDAC_offsetValue[masker_index] = tempInt >> 15;

                        stimulLevel_uA                = mappingPacket->eCapMeasurement.stimulationLevel_uA_masker - stimulDAC_offsetValue_uA;
                        tempInt                       = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                        stimulLevel_255[masker_index] = tempInt >> 15;
                    }
                    else // 오프셋 적용을 안해도 되는 경우.
                    {
                        stimulDAC_offsetResolution[masker_index] = Offset_DAC_A;
                        stimulDAC_offsetValue_uA                 = 0; //
                        stimulDAC_offsetValue[masker_index]      = 0;

                        stimulLevel_uA                = mappingPacket->eCapMeasurement.stimulationLevel_uA_masker - stimulDAC_offsetValue_uA;
                        tempInt                       = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                        stimulLevel_255[masker_index] = tempInt >> 15;
                    }
                }
            }
            else // 4uA로  자극
            {
                // 자극 DAC 기울기 4uA
                stimulDAC_slope[masker_index] = Stimulation_DAC_B;

                stimulDAC_offsetResolution[masker_index] = Offset_DAC_B;
                stimulDAC_offsetValue_uA                 = offsetDAC_B_Saturation_uA;
                tempInt                                  = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
                stimulDAC_offsetValue[masker_index]      = tempInt >> 15;

                stimulLevel_uA                = mappingPacket->eCapMeasurement.stimulationLevel_uA_masker - stimulDAC_offsetValue_uA;
                tempInt                       = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_B;
                stimulLevel_255[masker_index] = tempInt >> 15;
            }

            // 프루브에 대한 것
            if (mappingPacket->eCapMeasurement.stimulationLevel_uA_probe <
                (stimulDAC_A_only_Saturation_uA + offsetDAC_B_Saturation_uA)) // 2uA 기울기로 전달 가능한 범위내. // 1530 보다 작은 경우
            {

                // 자극 DAC 기울기 2uA
                stimulDAC_slope[probe_index] = Stimulation_DAC_A;

                if (mappingPacket->eCapMeasurement.stimulationLevel_uA_probe >
                    (stimulDAC_A_only_Saturation_uA + offsetDAC_A_Saturation_uA)) // 1020~1530  - > 1020 오프셋을 적용해야 되는 경우
                {

                    stimulDAC_offsetResolution[probe_index] = Offset_DAC_B;
                    stimulDAC_offsetValue_uA                = offsetDAC_B_Saturation_uA;
                    tempInt                                 = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
                    stimulDAC_offsetValue[probe_index]      = tempInt >> 15;

                    stimulLevel_uA               = mappingPacket->eCapMeasurement.stimulationLevel_uA_probe - stimulDAC_offsetValue_uA;
                    tempInt                      = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                    stimulLevel_255[probe_index] = tempInt >> 15;
                }
                else // 1020보다 작은 경우
                {
                    //
                    if (mappingPacket->eCapMeasurement.stimulationLevel_uA_probe > stimulDAC_A_only_Saturation_uA) // 510 ~ 1020 -> 510 오프셋을 적용한다.
                    {

                        stimulDAC_offsetResolution[probe_index] = Offset_DAC_A;
                        stimulDAC_offsetValue_uA                = offsetDAC_A_Saturation_uA; //
                        tempInt                                 = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                        stimulDAC_offsetValue[probe_index]      = tempInt >> 15;

                        stimulLevel_uA               = mappingPacket->eCapMeasurement.stimulationLevel_uA_probe - stimulDAC_offsetValue_uA;
                        tempInt                      = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                        stimulLevel_255[probe_index] = tempInt >> 15;
                    }
                    else // 오프셋 적용을 안해도 되는 경우.
                    {
                        stimulDAC_offsetResolution[probe_index] = Offset_DAC_A;
                        stimulDAC_offsetValue_uA                = 0; //
                        stimulDAC_offsetValue[probe_index]      = 0;

                        stimulLevel_uA               = mappingPacket->eCapMeasurement.stimulationLevel_uA_probe - stimulDAC_offsetValue_uA;
                        tempInt                      = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                        stimulLevel_255[probe_index] = tempInt >> 15;
                    }
                }
            }
            else // 4uA로  자극
            {
                // 자극 DAC 기울기 4uA
                stimulDAC_slope[probe_index] = Stimulation_DAC_B;

                stimulDAC_offsetResolution[probe_index] = Offset_DAC_B;
                stimulDAC_offsetValue_uA                = offsetDAC_B_Saturation_uA;
                tempInt                                 = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
                stimulDAC_offsetValue[probe_index]      = tempInt >> 15;

                stimulLevel_uA               = mappingPacket->eCapMeasurement.stimulationLevel_uA_probe - stimulDAC_offsetValue_uA;
                tempInt                      = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_B;
                stimulLevel_255[probe_index] = tempInt >> 15;
            }

            // 마스커와 프로브의 자극 크기가 다를경우,,, DAC 기울기 및 슬로프  설정을 달리 해줘야 되는 경우 발생.( 마스커 또는 포로브 단독으로 출력이 생성되는
            // 경우가 있기 때문에 항상 발생하기 때문에.. DAC offset을 꺼야 하는 경우 무조 건 발생

            if (mappingPacket->eCapMeasurement.maskerProbeInterval_numFrame < MinNumFrameForConfigProbe)
            {
                // 마스커 출력과 프로브출력 간격을 넓혀야 한다.

                // 에러 전송
                tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__MaskerProbe_InterVal_tooShort, __LINE__);

                // 커맨드 리셋;
                clear_mappingCommand();
            }

            if ((numFramePerChannel << 1) + mappingPacket->eCapMeasurement.maskerProbeInterval_numFrame >
                48) // 마스커와 프로브 자극 출력과 인터벌의 합이 2msec, 48개를 넘지 않아야 된다.
            {
                // 마스커 출력과 프로브출력 간격을 좁히거나, 펄스 폭을 좁혀야 한다. 2msec 템플릿에 다 들어 가지 못한다.

                // 에러 전송
                tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__MaskerProbe_InterVal_tooLong, __LINE__);

                // 커맨드 리셋;
                clear_mappingCommand();
            }

            // 백텔을 수신하는데 필요한 시간.

            backtelReceiveTime_ms = mappingPacket->eCapMeasurement.measurementSampleNum >>
                                    4; // 1msec 동안 수신할 수 있는 백텍 데이터 샘플수는 약 16개이다.( 4bit shift == 16으로 나누기)
        }
        break;

        case 1:
        {

            // 이전에 전송된 자극 파라미터 값이 있을 수 있기 때문에 출력을 내보내기 위하여 설정 파라미터들 전송한다.

            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // 펄스 폭 0으로 설정

#if 0
                w_FPGA_registerValue=0;  // 펄스폭 0
                w_FPGA_registerValue=(w_FPGA_registerValue|pcm_Mold_PulsePhaseWidth);
                fillSepcificCommndBuffer(pcm_index++,w_FPGA_registerValue);
#else

            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

#endif

#if 0
                // ISD path 확인용 임의의 값
                w_isd_registerValue = ISD_registerAddr_forwardPath_check;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                w_isd_registerValue = w_isd_registerValue | df_forwardPathCheck_arbitraryValue;
                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;
                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
#else
            // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
            fill_pcmBuff_lastSimulationOut(&pcm_index);

#endif
            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);

            if (mappingPacket->eCapMeasurement.stimulatonMode != en__bipolar)
            {
                flowCounter = flowCounter + 2; // 바이폴라 기준전극 설정 값 전송에 2msec 필요, 해당 루틴 생략
            }
        }
        break;

        case 2:
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // Bipolar 기준전극  FIFO 지우기
            w_isd_registerValue = ISD_registerAddr_cipherDataStatus;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x80; // CHIP_ID_FIFO_RDDATA_INDEXdp 아무값이나 쓰면 FIFO가 지워진다.
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 기준 전극 번호.
            bipolarFIFO_index = 0;
            for (i = 0; i < df_MaxNumTransferableChannel; i++, bipolarFIFO_index++) // 0~22번 자극 전극에 대응하는 기준 전극 번호
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                if (bipolarFIFO_index == mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum -
                                             1) //  //1~24번 채널 중,매핑에서 받은 기준전극 번호가 있으면 해당 전극번호를 설정하고 나머지는 32번 전극에 설정

                {
                    w_isd_registerValue = (w_isd_registerValue) | (electrodeMap[bipolarFIFO_index]);
                }
                else
                {
                    w_isd_registerValue = w_isd_registerValue | electrodeMap[31];
                }

                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 3:
        {
            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            for (i = 0; i < 8; i++, bipolarFIFO_index++) // 23~31번 자극전극에 대응하는 기준전극
            {
                w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
                w_isd_registerValue = w_isd_registerValue << 1;
                w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                w_isd_registerValue = w_isd_registerValue << 8;

                if (bipolarFIFO_index == mappingPacket->eCapMeasurement.bipolarReferenceElectrodeNum -
                                             1) // 25~32번 자극전극 mapping에서 수신된 기준전극 번호에 해당하는 번호만 설정하고 다른 것은 32번 채널로 설정.
                {
                    w_isd_registerValue = (w_isd_registerValue) | (electrodeMap[bipolarFIFO_index]);
                }
                else
                {
                    w_isd_registerValue = w_isd_registerValue | electrodeMap[31];
                }

                w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);
            }

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 4:
        {

            eCAP_mesureStartTime = flowCounter;

            // PCM 출력 모드 변경

            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

#if 0

            // FPGA 피포 지움

                last_fpga_settingValue=get_last_fpga_written_Value(i2cAddr_FPGA_systemResgister_2nd);

                w_FPGA_VolatileValue = (1 << FPGA_BitPosition_Clear_FIFO); // 피포를 지운다.

                w_FPGA_VolatileValue = w_FPGA_VolatileValue | last_fpga_settingValue;

                //if (tdc_hal_i2c_cfx_write(df_I2C_ADDR_FPGA_System_State, &w_FPGA_VolatileValue, 1))
                if (tdc_hal_i2c_isd_write(i2cAddr_FPGA_systemResgister_2nd, &w_FPGA_VolatileValue, 1))
                {

                    //if (tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_System_State, &r_FPGA_registerValue, 1))
                    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_systemResgister_2nd, &r_FPGA_registerValue, 1))
                    {



                        comparing = last_fpga_settingValue | (1<<FPGA_BitPosition_FIFO_is_empty); // 에러 플레그 및 설정값을 비교용 값으로 사용한다.

                        if (r_FPGA_registerValue != comparing) // 설정값 확인
                        {
                            comparing=0;

#if 0
                            // I2C 읽기   실패, FPGA 초기화
                            change_isd_state(en__isdStatus_PowerIC_OK);

                            // 에러 전송
                            tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking,en__FPGA_ERROR, en_FIFO_NotCleared, __LINE__);
                            // 커맨드 리셋;
                            clear_mappingCommand();

#endif
                        }

                    }
                    else
                    {
                        // I2C 읽기   실패, FPGA 초기화
                        change_isd_state(en__isdStatus_PowerIC_OK);

                        // 에러 전송
                        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking,en__I2C_ERROR,en__ReadError, __LINE__);
                        // 커맨드 리셋;
                        clear_mappingCommand();
                    }
                }
                else
                {
                    // I2C 쓰기   실패, FPGA 초기화
                    change_isd_state(en__isdStatus_PowerIC_OK);

                    // 에러 전송
                    tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking,en__I2C_ERROR,en__WriteError, __LINE__);
                    // 커맨드 리셋;
                    clear_mappingCommand();
                }

#else

            if (write_FPGA_clear_FIFO())
            {

                // FPGA 상태를 읽어 본다.
                if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty)) // 지워 졌는지 확인.
                {
                    if (!FPGA_FIFO_empty)
                    {
                        change_isd_state(en__isdStatus_PowerIC_OK); //
                    }
                }
            }

#endif

            // 펄스 폭 0으로 변경

#if 0
                w_FPGA_registerValue=0;  // 펄스폭 0
                w_FPGA_registerValue=(w_FPGA_registerValue|pcm_Mold_PulsePhaseWidth);
                fillSepcificCommndBuffer(pcm_index++,w_FPGA_registerValue);
#else

            chang_PulseWidth_minimum(pcm_index++);
            upadte_fpga_pulsePhaseWidth_written_Value(FPGA_pulsePhaseWidth_minimum);

#endif

#if 0
                // FPGA backtel 레지스터 설정

                    w_FPGA_registerValue=get_last_fpga_written_Value(i2cAddr_FPGA_backtel_Config);

                    // FPGA Backtel- 12bit 레지스터( BitPosition_FPGA_backtel_bitLength 1로 변경)
                    w_FPGA_registerValue=w_FPGA_registerValue|(1<<pcm_BitPosition_FPGA_backtel_bitLength);
                    // backtel 활성화
                    w_FPGA_registerValue=w_FPGA_registerValue|(1<<pcm_BitPosition_FPGA_backtel_OnOff);

                    // PCM 몰드에 결합
                    w_FPGA_registerValue=(w_FPGA_registerValue| pcm_Mold_BacktelConfiguration);
                    // PCM 출력
                    fillSepcificCommndBuffer(pcm_index++, w_FPGA_registerValue);
#else
            change_12BitBacktel_mode(pcm_index++);
#endif

            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 5:
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            // ISD 0x7    - eCAP 측정 갯수

            w_isd_registerValue = ISD_registerAddr_eCAP_smaplingSize;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | mappingPacket->eCapMeasurement.measurementSampleNum; // 측정 갯수

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x0f20  | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // ISD 0xA    - probe 출력 후 측정 시점 딜레이.

            w_isd_registerValue = ISD_registerAddr_adc_measurementDealy;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | 1; // NRT_HP_SW_ON

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | 0; // NRT_RST_WAIT_TIME

            w_isd_registerValue = w_isd_registerValue << 4;
            w_isd_registerValue = w_isd_registerValue | mappingPacket->eCapMeasurement.adcMeasurementDelay; // 프로프 출력 후 딜레이 [3:0]

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x1581 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // ISD 0xB or 0xC   - pre-amp Gain
            if (electrodeMap[(mappingPacket->eCapMeasurement.measurementElectrodeNum - 1)] < 16)
            {
                w_isd_registerValue = ISD_registerAddr_adc_preAmpGain_lowerChannel;
            }
            else
            {
                w_isd_registerValue = ISD_registerAddr_adc_preAmpGain_higherChannel;
            }

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | mappingPacket->eCapMeasurement.adcPreampGain; // 측정 갯수 [3:0]

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x1701 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

#if 1
            // ISD 0xF
            w_isd_registerValue = ISD_registerAddr_SystemClkReset;
            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
            w_isd_registerValue = w_isd_registerValue << 8;

            w_isd_registerValue = w_isd_registerValue | 0x2; // NRT_SWON_TIME 5usec
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

#endif

            // ISD 0x9    - eCAP 설정 초기화
            w_isd_registerValue = ISD_registerAddr_adc_measurement;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | adc_measurementMode_impedance; // 임피던스 측정 모드로 세팅

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | mappingPacket->eCapMeasurement.adcSamplingFreq; // adc 샘플링 주파수

            w_isd_registerValue = w_isd_registerValue << 2;

            w_isd_registerValue = w_isd_registerValue | mesurementStart_Disable; // adc 측정 비활성화.

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x1300 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // ISD 0x8    - 측정 채널

            w_isd_registerValue = ISD_registerAddr_adc_samplingChannel;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = (w_isd_registerValue) | (electrodeMap[(mappingPacket->eCapMeasurement.measurementElectrodeNum - 1)]);

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x1102 | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // ISD 0x9    - eCAP 측정 설정
            w_isd_registerValue = ISD_registerAddr_adc_measurement;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | adc_measurementMode_eCAP; // 측정 모드 eCAP

            w_isd_registerValue = w_isd_registerValue << 3;
            w_isd_registerValue = w_isd_registerValue | mappingPacket->eCapMeasurement.adcSamplingFreq; // adc 샘플링 주파수

            w_isd_registerValue = w_isd_registerValue << 2;

#if 1
            w_isd_registerValue = w_isd_registerValue | mesurementStart_on_configureADCregister; // adc 측정 시작 시점  :ADC 0x08 레지스터 설정  시점
#else
            w_isd_registerValue = w_isd_registerValue | mesurementStart_on_stiulationOut; // adc 측정 시작 시점 : 자극 출력 시점
#endif

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x1321 | 0x50000   -40khz

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            //////////////////
            // 마스커 위치 출력룡 DAC 설정
            //////////////////
            switch (stimulPattern_index)
            {

                case en__probeAlone:
                case en__switchingArtifact:
                {
                    // 마스커 위치에 출력이 없다.
                    // 자극 출력 DAC 설정
                    // ISD 0x5   - DAC Offset 쓰기
                    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                    w_isd_registerValue = w_isd_registerValue << 8;

                    w_isd_registerValue = w_isd_registerValue | 0; // 마스커 위치에 출력이 없으므로 오프셋 값 0
                    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                    fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue); //  0x0b00  | 0x50000

                    // ISD 0x6    - 자극 파라미터 설정  쓰기
                    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

                    // 0, 7번비트
                    w_isd_registerValue = w_isd_registerValue << 1;

                    // offsetResolution 6번 비트
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | Offset_DAC_A;

                    // DAC slope    4,5번 비트
                    w_isd_registerValue = w_isd_registerValue << 2;
                    w_isd_registerValue = w_isd_registerValue | Stimulation_DAC_A;
                }
                break;

                case en__maskerNprobe:
                case en__maskerAlone:
                {
                    // 마스커 위치에 출력이 없다.
                    // 자극 출력 DAC 설정
                    // ISD 0x5   - DAC Offset 쓰기
                    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                    w_isd_registerValue = w_isd_registerValue << 8;

                    w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetValue[masker_index]; // 마스커 인덱스 값
                    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                    fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue); //  0x0b00  | 0x50000

                    // ISD 0x6    - 자극 파라미터 설정  쓰기
                    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

                    // 0, 7번비트
                    w_isd_registerValue = w_isd_registerValue << 1;

                    // offsetResolution 6번 비트
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetResolution[masker_index];

                    // DAC slope    4,5번 비트
                    w_isd_registerValue = w_isd_registerValue << 2;
                    w_isd_registerValue = w_isd_registerValue | stimulDAC_slope[masker_index];
                }
                break;
            }
            // ISD 0x6    - 자극 파라미터 설정  쓰기
            // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->eCapMeasurement.stimulatonMode)
            {
                case en__monopolr_body:

                    w_isd_registerValue = w_isd_registerValue | en__monopolr_body;
                    break;
                case en__monopolr_rod:

                    w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;
                    break;
                case en__monopolr_BothRodBody:

                    w_isd_registerValue = w_isd_registerValue | en__monopolr_BothRodBody;
                    break;
                default:
                    w_isd_registerValue = w_isd_registerValue | en__referenceNA; // // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
                    break;
            }

            // 자극 출력 모드 0,1번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->eCapMeasurement.stimulatonMode)
            {
                case en__monopolr_body:
                case en__monopolr_rod:
                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | 0;
                    break;
                case en__bipolar:
                    w_isd_registerValue = w_isd_registerValue | 1;
                    break;
                case en__commonground:
                    w_isd_registerValue = w_isd_registerValue | 2;
                    break;
                case en__semi_simultaneously:
                    w_isd_registerValue = w_isd_registerValue | 3;
                    break;
            }

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x0d04  | 0x50000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 마스커 자극 출력 (선행펄스, 전극, 자극 크기)

            // 자극 출력 파라미터

            w_isd_registerValue = mappingPacket->eCapMeasurement.firstPulsePhase << firstPulsePhasePositionAtPCM_Mold; // 선행 펄스 ;
            w_isd_registerValue = w_isd_registerValue | ((electrodeMap[(mappingPacket->eCapMeasurement.stimulationElectrodeNum - 1)])
                                                         << electrodIndexPositionAtPCM_Mold); // 자극 전극 번호

            switch (stimulPattern_index)
            {

                case en__probeAlone:
                case en__switchingArtifact:
                {

                    w_isd_registerValue = w_isd_registerValue | (0 << stimulationPositionAtPCM_Mold); // 자극 출력 크기 0
                }
                break;
                case en__maskerNprobe:
                case en__maskerAlone:
                {

                    w_isd_registerValue = w_isd_registerValue | (stimulLevel_255[masker_index] << stimulationPositionAtPCM_Mold); // 자극 출력 크기
                }
                default:
                    break;
            }

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation; // 0x8400  | 0x40000

            fillSepcificCommndBuffer(pcm_index++, w_isd_registerValue);

            // 마스커 출력용  펄스폭 조정
#if 0
                    w_FPGA_registerValue=(mappingPacket->eCapMeasurement.pulseWidth-FPGA_pulsePhaseWidth_minimum);
                    w_FPGA_registerValue=(w_FPGA_registerValue|pcm_Mold_PulsePhaseWidth);

                    fillSepcificCommndBuffer(pcm_index++,w_FPGA_registerValue);
#else
            chang_PulseWidth(pcm_index++, mappingPacket->eCapMeasurement.pulseWidth);
            upadte_fpga_pulsePhaseWidth_written_Value(mappingPacket->eCapMeasurement.pulseWidth);
#endif
            // 나머지 버퍼는  NOP-Standby
            for (; pcm_index < df_MaxNumTransferableChannel;)
            {
                fillSepcificCommndBuffer(pcm_index++, pcm_Mold_NopStandby);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;

        case 6:
        {

            //  인터벌 적용을 위한 템플릿.

            // [ {프루브용 자극 출력 용 0x05설정,펄스폭 맞춤 NOP},{마스커 프로브 인터벌 (펄스폭 0, 프로브 출력 자극 파라미터, 0x06설정, 펄스폭 설정  , 인터벌
            // 맞춤용 NOP..)} , {프로브 자극 출력용 0x08 설정-펄스 폭에 맞춘 NOP})

            templeteBuff_index = 0;
            k                  = 0;

            // PCM_templete_eCAP[k][templeteBuff_index++]=pcm_Mold_NopStandby;

            ////////////////
            // 프로브 위치에 출력  DAC 오프셋  -- 여기서 마스터 출력이 실제 생성됨.
            ////////////////
            switch (stimulPattern_index)
            {

                case en__maskerAlone:
                case en__switchingArtifact:
                {
                    // 자극 출력 DAC 설정
                    // ISD 0x5   - DAC Offset 쓰기
                    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                    w_isd_registerValue = w_isd_registerValue << 8;

                    w_isd_registerValue = w_isd_registerValue | 0; // 오프셋 값 0

                    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                    file_PCM_templete_eCAP(en__fillForwardData, w_isd_registerValue);
                    // PCM_templete_eCAP[k][templeteBuff_index++]=w_isd_registerValue;
                }
                break;

                case en__probeAlone:
                case en__maskerNprobe:
                {
                    // 자극 출력 DAC 설정
                    // ISD 0x5   - DAC Offset 쓰기
                    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
                    w_isd_registerValue = w_isd_registerValue << 8;

                    w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetValue[probe_index]; // 프로브 인덱스 값

                    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

                    file_PCM_templete_eCAP(en__fillForwardData, w_isd_registerValue);
                    // PCM_templete_eCAP[k][templeteBuff_index++]=w_isd_registerValue;
                }
                break;
            }

            for (i = 1; i < numFramePerChannel; i++) // DAC 0x05  파라미터가 1프레임 출력되었기 때문에 i=1에서 시작한다.
            {

                file_PCM_templete_eCAP(en__fillForwardData, pcm_Mold_NopStandby);
                // PCM_templete_eCAP[k][templeteBuff_index++]=pcm_Mold_NopStandby;
            }

            // 마스커 자극 파형 출력 후, 프로브 파형 출력 사이에 넣을 데이터..
            maskerProbeIntervalCouter = 1;

            // 펄스 폭  0
            w_FPGA_registerValue = 0; // 펄스폭 0

            file_PCM_templete_eCAP(en__fillForwardData, (w_FPGA_registerValue | pcm_Mold_PulsePhaseWidth));
            // PCM_templete_eCAP[k][templeteBuff_index++]=(w_FPGA_registerValue|pcm_Mold_PulsePhaseWidth);
            maskerProbeIntervalCouter++;

            // 프로브 출력 파형에 적용될 DAC 슬로프, 해싱도

            switch (stimulPattern_index)
            {

                case en__maskerAlone:
                case en__switchingArtifact:
                {

                    // ISD 0x6    - 자극 파라미터 설정  쓰기
                    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

                    // 0, 7번비트
                    w_isd_registerValue = w_isd_registerValue << 1;

                    // offsetResolution 6번 비트
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | Offset_DAC_A;

                    // DAC slope    4,5번 비트
                    w_isd_registerValue = w_isd_registerValue << 2;
                    w_isd_registerValue = w_isd_registerValue | Stimulation_DAC_A;
                }
                break;

                case en__probeAlone:
                case en__maskerNprobe:
                {

                    // ISD 0x6    - 자극 파라미터 설정  쓰기
                    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

                    // 0, 7번비트
                    w_isd_registerValue = w_isd_registerValue << 1;

                    // offsetResolution 6번 비트
                    w_isd_registerValue = w_isd_registerValue << 1;
                    w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetResolution[probe_index];

                    // DAC slope    4,5번 비트
                    w_isd_registerValue = w_isd_registerValue << 2;
                    w_isd_registerValue = w_isd_registerValue | stimulDAC_slope[probe_index];
                }
                break;
            }
            // ISD 0x6    - 자극 파라미터 설정  쓰기
            // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
            w_isd_registerValue = w_isd_registerValue << 2;
            switch (mappingPacket->eCapMeasurement.stimulatonMode)
            {
                case en__monopolr_body:

                    w_isd_registerValue = w_isd_registerValue | en__monopolr_body;
                    break;
                case en__monopolr_rod:

                    w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;
                    break;
                case en__monopolr_BothRodBody:

                    w_isd_registerValue = w_isd_registerValue | en__monopolr_BothRodBody;
                    break;
                default:
                    w_isd_registerValue = w_isd_registerValue | en__referenceNA; // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
                    break;
            }

            // 자극 출력 모드 0,1번 비트
            w_isd_registerValue = w_isd_registerValue << 2;

            switch (mappingPacket->eCapMeasurement.stimulatonMode)
            {
                case en__monopolr_body:
                case en__monopolr_rod:
                case en__monopolr_BothRodBody:
                    w_isd_registerValue = w_isd_registerValue | 0;
                    break;
                case en__bipolar:
                    w_isd_registerValue = w_isd_registerValue | 1;
                    break;
                case en__commonground:
                    w_isd_registerValue = w_isd_registerValue | 2;
                    break;
                case en__semi_simultaneously:
                    w_isd_registerValue = w_isd_registerValue | 3;
                    break;
            }

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

            file_PCM_templete_eCAP(en__fillForwardData, w_isd_registerValue);
            // PCM_templete_eCAP[k][templeteBuff_index++]=w_isd_registerValue;
            maskerProbeIntervalCouter++;

            // 프로브 자극 출력 파라미터 :

            w_isd_registerValue = mappingPacket->eCapMeasurement.firstPulsePhase << firstPulsePhasePositionAtPCM_Mold; // 선행 펄스 ;
            w_isd_registerValue = w_isd_registerValue | ((electrodeMap[(mappingPacket->eCapMeasurement.stimulationElectrodeNum - 1)])
                                                         << electrodIndexPositionAtPCM_Mold); // 자극 전극 번호

            switch (stimulPattern_index)
            {

                case en__maskerAlone:
                case en__switchingArtifact:
                {

                    w_isd_registerValue = w_isd_registerValue | (0 << stimulationPositionAtPCM_Mold); // 자극 출력 크기 0
                }
                break;
                case en__maskerNprobe:
                case en__probeAlone:
                {
                    w_isd_registerValue = w_isd_registerValue | (stimulLevel_255[probe_index] << stimulationPositionAtPCM_Mold); // 자극 출력 크기
                }
                default:
                    break;
            }

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;

            file_PCM_templete_eCAP(en__fillForwardData, w_isd_registerValue);
            // PCM_templete_eCAP[k][templeteBuff_index++]=w_isd_registerValue;
            maskerProbeIntervalCouter++;

            // 펄스폭 조정 (프로브 출력이 나갈 때 적용될)

            w_FPGA_registerValue = (mappingPacket->eCapMeasurement.pulseWidth - FPGA_pulsePhaseWidth_minimum); //
            file_PCM_templete_eCAP(en__fillForwardData, (w_FPGA_registerValue | pcm_Mold_PulsePhaseWidth));
            // PCM_templete_eCAP[k][templeteBuff_index++]=(w_FPGA_registerValue | pcm_Mold_PulsePhaseWidth);
            maskerProbeIntervalCouter++;

            // 마스터와 프로브 간격 채우기..

            for (; maskerProbeIntervalCouter < mappingPacket->eCapMeasurement.maskerProbeInterval_numFrame; maskerProbeIntervalCouter++)
            {
                file_PCM_templete_eCAP(en__fillForwardData, pcm_Mold_NopStandby);
                // PCM_templete_eCAP[k][templeteBuff_index++]=pcm_Mold_NopStandby;
            }

            // 프로브 파형 출력용 0x08 설정 (실제 적용할 값이 아니며, 프로브 출력을 나오게 하기위한 트리거로 사용된다.)

            //

            w_isd_registerValue = ISD_registerAddr_adc_samplingChannel;

            w_isd_registerValue = w_isd_registerValue << 1;
            w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

            w_isd_registerValue = w_isd_registerValue << 8;
            w_isd_registerValue = w_isd_registerValue | 0; //

            w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration; // 0x1100 | 0x50000

            file_PCM_templete_eCAP(en__fillForwardData, w_isd_registerValue);
            // PCM_templete_eCAP[k][templeteBuff_index++]=w_isd_registerValue;

            // 자극 펄스  NOP
            for (i = 1; i < numFramePerChannel; i++)
            {
                file_PCM_templete_eCAP(en__fillForwardData, pcm_Mold_NopStandby);
                // PCM_templete_eCAP[k][templeteBuff_index++]=pcm_Mold_NopStandby;
            }

            // 나머지 버퍼는  backtel 데이터를 수신할 준비.
            file_PCM_templete_eCAP(en__fillBackwardData, pcm_Mold_NopBacktel);
        }

        break;

        case 7:
        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            for (i = 0; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, PCM_templete_eCAP[0][i]);
            }

            // NOP-Standby
            changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        }
        break;
        case 8:

        {

            // PCM 출력 모드 변경
            changePcmOutputMode(PcmBitStream_Mode_SepcificCommand);

            for (i = 0; i < df_MaxNumTransferableChannel; i++)
            {
                fillSepcificCommndBuffer(i, PCM_templete_eCAP[1][i]);
            }

            // NOP-Backtel
            changeNextPcmOutputMode(PcmBitStream_Mode_NopBacktel);

            backtelStart_flowCounter = flowCounter + backtelReceiveTime_ms + 4; // 현재 PCM FIFO에서 실제 출력이 나가서 적용되는 시점의 flowCouter :  현재
                                                                                // 플로우 카운터 + 측정 샘플 수에 해당하는 백텔 수신 시간 + FIFO 출력 delay
        }
        break;

        default:
            break;
    }

    if (flowCounter == backtelStart_flowCounter)
    {

        changePcmOutputMode(PcmBitStream_Mode_NopStandby);

#if 0

                    // FGPA 상태 확인
                    //if(tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_System_State, &r_FPGA_registerValue, 1))
                    if(tdc_hal_i2c_isd_read(i2cAddr_FPGA_systemResgister_1st, &r_FPGA_registerValue, 1))
                    {

                        comparing=r_FPGA_registerValue&(1<<FPGA_BitPosition_SystemError);   // 에러 비트
                        if(comparing==0x20)
                        {

                            //tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_Error_Flag, &r_FPGA_registerValue, 1);
                            tdc_hal_i2c_isd_read(i2cAddr_FPGA_error_Flag, &r_FPGA_registerValue, 1);



                            // FPGA 에러 발생, FPGA 초기화
                            change_isd_state(en__isdStatus_PowerIC_OK);

                            // 에러 전송
                            tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking,en__FPGA_ERROR,r_FPGA_registerValue, __LINE__); //FPGA 에러 값을 그대로 전달
                            // 커맨드 리셋;
                            clear_mappingCommand();

                        }

                    }
                    else
                    {
                        // I2C 읽기   실패, FPGA 초기화
                        change_isd_state(en__isdStatus_PowerIC_OK);

                        // 에러 전송
                        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking,en__I2C_ERROR,en__ReadError, __LINE__);
                        // 커맨드 리셋;
                        clear_mappingCommand();
                    }

#else
        if (check_FPGA_PCM_Error(&FPGA_error))
        {
            if (!FPGA_error)
            {
                // 백텔 수신 확인

                if (check_FPGA_FIFO_empty(&FPGA_FIFO_empty))
                {
                    if (!FPGA_FIFO_empty)
                    {

                        if (read_FPGA_backtel_FIFO(&bufferForReading_eCAP[stimulPattern_index - 1][0],
                                                   mappingPacket->eCapMeasurement.measurementSampleNum << 1))
                        {
                            // ecap 측정값을 읽어 옴.
                        }
                    }
                    else
                    {
                        // 백텔 안들어옴 에러
                        change_isd_state(en__isdStatus_FPGA_Ok); // 내부기 전송 파워 설정 부터 다시.

                        // 에러 전송
                        tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking, en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);
                        // 커맨드 리셋;
                        clear_mappingCommand();
                    }
                }
            }
            else
            {

                // FPGA 에러 발생, FPGA 초기화
                change_isd_state(en__isdStatus_PowerIC_OK);

                // 에러 전송
                tdc_sys_error_send_to_app(en__mapping_eCAP_Measurement_masking,
                               en__FPGA_CONFIGUARATION_ERROR,
                               r_FPGA_registerValue,
                               __LINE__); // FPGA 에러 값을 그대로 전달
                // 커맨드 리셋;
                clear_mappingCommand();
            }
        }

#endif
    }

#if 0
    if(flowCounter>backtelStart_flowCounter)
    {

                buffer_tx_index=0;



             // command loop-back
                bufferForSPI_tx[buffer_tx_index++]=mappingPacket->command;

             // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++]=iterationNum+1;      // 측정회차

                bufferForSPI_tx[buffer_tx_index++]=stimulPattern_index; // 측정 패턴


                for(i=0; i<Max_eCAP_ReturnDataSize; i++)// ble 패킷 사이즈로 인하여..1회 전달 시 최대, 4번 측정한 데이터 전달 가능.
                {
                    if(ble_transfer_index<mappingPacket->eCapMeasurement.measurementSampleNum)
                    {
                        bufferForSPI_tx[buffer_tx_index++]=(int)bufferForReading_eCAP[(ble_transfer_index<<1)]; // 상위 바이트
                        bufferForSPI_tx[buffer_tx_index++]=(int)bufferForReading_eCAP[(ble_transfer_index<<1)+1]; // 하위 바이트

                        ble_transfer_index++;
                    }
                    else
                    {
                        break;
                    }


                }



                // 송신 데이터 SPI TX버퍼에 복사

                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx,buffer_tx_index);



                if(ble_transfer_index>=mappingPacket->eCapMeasurement.measurementSampleNum) // 측정데이터 전송이 완료 되었으면  다음
                {
                    ble_transfer_index=0;

                    file_PCM_templete_eCAP(en__clearIndex, 0);

                    // 다음 패턴 인덱스로 변경
                    stimulPattern_index++;

                    while(1)
                    {
                        if(tdc_hal_spi_is_tx_buffer_empty())
                        {

                            ble_transfer_index=0;

                            break;
                        }
                        else
                        {
                            __WFE();
                        }
                    }

                    // 측정 카운터 리셋하여 다시 측정
                    flowCounter=eCAP_mesureStartTime-1;


                    if(stimulPattern_index>en__switchingArtifact)
                    {
                        stimulPattern_index=en__probeAlone;
                        iterationNum++;


                        if(iterationNum>=mappingPacket->eCapMeasurement.iterationNum)
                        {
                            // 측정 완료.


                            // 커맨드 리셋;
                            clear_mappingCommand();


                        }

                    }
                }


        // 매핑 프로그램으로 데이터 전달.







    }

#endif

    if (flowCounter > backtelStart_flowCounter)
    {

        stimulPattern_index++;
        file_PCM_templete_eCAP(en__clearIndex, 0);

        if (stimulPattern_index > en__switchingArtifact)
        {

#if 0
                                for(k=0;k <256; k++)
                                    bufferForReading_eCAP[0][k]=0;

                                for(k=0;k <128; k++)
                                {
                                    bufferForReading_eCAP[0][k*2+1]=k;
                                }
#endif
            sendingPatternIndex = 0;
            ble_transfer_index  = 0;

            // // 4개 패턴  측정 값 전송
            while (1)
            {

                buffer_tx_index = 0;

                // command loop-back
                bufferForSPI_tx[buffer_tx_index++] = mappingPacket->command;

                // pay-load 준비
                bufferForSPI_tx[buffer_tx_index++] = iterationNum + 1; // 측정회차

                bufferForSPI_tx[buffer_tx_index++] = sendingPatternIndex + 1; // 측정 패턴

                for (i = 0; i < Max_eCAP_ReturnDataSize; i++) // ble 패킷 사이즈로 인하여..1회 전달 시 최대, 4번 측정한 데이터 전달 가능.
                {
                    if (ble_transfer_index < mappingPacket->eCapMeasurement.measurementSampleNum)
                    {
                        bufferForSPI_tx[buffer_tx_index++] = (int) bufferForReading_eCAP[sendingPatternIndex][(ble_transfer_index << 1)];     // 상위 바이트
                        bufferForSPI_tx[buffer_tx_index++] = (int) bufferForReading_eCAP[sendingPatternIndex][(ble_transfer_index << 1) + 1]; // 하위 바이트

                        ble_transfer_index++;
                    }

                    if (ble_transfer_index == mappingPacket->eCapMeasurement.measurementSampleNum)
                    {
                        ble_transfer_index = 0;
                        sendingPatternIndex++;
                        break;
                    }
                }

                // 송신 데이터 SPI TX버퍼에 복사

                tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

                if (sendingPatternIndex == en__switchingArtifact)
                {
                    break;
                }
            }

            /////////////////////////////////

            stimulPattern_index = en__probeAlone;
            iterationNum++;
        }

        // 측정 카운터 리셋하여 다시 측정
        flowCounter = eCAP_mesureStartTime - 1;

        if (iterationNum >= mappingPacket->eCapMeasurement.iterationNum)
        {
            // 측정 완료.

            // 커맨드 리셋;
            clear_mappingCommand();
        }
    }

    flowCounter++;
}
