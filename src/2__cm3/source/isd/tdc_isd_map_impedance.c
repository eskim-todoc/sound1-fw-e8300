#include <hw.h>
#include <tdc_isd_pcm.h>
#include <stdbool.h>
#include <tdc_sys_error.h>
#include <FPGA.h>
#include <tdc_isd_fpga.h>
#include <internalStimulationChip.h>
#include <tdc_isd.h>
#include <tdc_hal_i2c_isd.h>
#include <tdc_stim_definitions.h>

#include <tdc_isd.h>
#include <tdc_isd_init_fpga.h>
#include <tdc_shm.h>
#include <tdc_ble_mapping.h>
#include <tdc_hal_spi.h>
#include <tdc_isd_map_impedance.h>

#include <electrodeMapping.h>

#include <tdc_printf.h>

static char impedanceValue[df_maxIterationNum_impedance][2][2];

// 상태 기계가 단계 사이로 넘기는 값들. 분해 전에는 함수-지역 static 이었다.
static int                 s_iterationNum = 0;
static int                 s_electrodeNum = 0;
static int                 s_flowCounter  = 0;
static int                 s_numFramePerChannel_startWidth;
static int                 s_numFramePerChannel_endWidth;
static int                 s_numFramePerChannel;
static int                 s_stimulDAC_slope;
static int                 s_stimulDAC_offsetResolution;
static int                 s_stimulDAC_offsetValue;
static int                 s_stimulDAC_offsetValue_uA;
static int                 s_stimulLevel_255             = 0;
static bool                s_toggle_start_end_pulseWidth = false;
static int                 s_ble_transfer_index;
static int                 s_flowCounter_SendingSPI = 1000;
static ST__MAPPING_PACKET *s_mappingPacket;

// 새 측정 명령이 들어왔다. 상태를 처음으로 되돌린다.
static void tdc_isd_map_impedance_reset(void)
{
    TDC_PRINTF_V("[MAPPING] CHECK IMPEDANCE, START FLAG IS SET \r\n");

    s_mappingPacket = tdc_ble_mapping_get_packet();

    s_flowCounter  = 0;
    s_iterationNum = 0;

    if (s_mappingPacket->impedanceCheck.channel == 255)
    {
        s_electrodeNum = 0;
    }
    else
    {
        s_electrodeNum = s_mappingPacket->impedanceCheck.channel - 1;
    }

    s_ble_transfer_index = 0;

    s_toggle_start_end_pulseWidth = true;
}

// 펄스폭에 따른 프레임 수 · 전하량 · DAC 설정 계산
static void tdc_isd_map_impedance_flow00_calc_frame_and_dac(void)
{
    int duration;
    int tokenTime;
    int deliveryCharge_pico;
    int stimulLevel_uA;
    int tempInt;

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    // 펄스폭에 따른 자극 프레임 갯수 계산

    // 소요시간 계산 - 측정 시작 시 펄스폭

    // 좁은 펄스폭 적용시 프레임 갯수
    // 펄스폭 시간 (펄스 위상 x 2)
    duration = s_mappingPacket->impedanceCheck.pulseWidth_start_usec;
    duration += duration;

    // 펄스 위상 반전 간격
    duration += FPGA_interphaseGapTokenTime;

    // 자극파라미터 전송 시간
    duration += FPGA_electrodAndStimulLevelTokenTime;

    tokenTime                       = FPGA_oneChannelDataTokenTime;
    s_numFramePerChannel_startWidth = 1;

    while (true)
    {
        if (duration < tokenTime)
        {
            break;
        }
        s_numFramePerChannel_startWidth++;
        tokenTime += FPGA_oneChannelDataTokenTime;
    }

    // 좁은 펄스폭 적용시 프레임 갯수
    // 펄스폭 시간 (펄스 위상 x 2)
    duration = s_mappingPacket->impedanceCheck.pulseWidth_end_usec;
    duration += duration;

    // 펄스 위상 반전 간격
    duration += FPGA_interphaseGapTokenTime;

    // 자극파라미터 전송 시간
    duration += FPGA_electrodAndStimulLevelTokenTime;

    tokenTime                     = FPGA_oneChannelDataTokenTime;
    s_numFramePerChannel_endWidth = 1;

    while (true)
    {
        if (duration < tokenTime)
        {
            break;
        }
        s_numFramePerChannel_endWidth++;
        tokenTime += FPGA_oneChannelDataTokenTime;
    }

    // 최대 전하량 계산
    deliveryCharge_pico =
        (s_mappingPacket->impedanceCheck.pulseWidth_end_usec) * (s_mappingPacket->impedanceCheck.stimulationLevel_uA);  // usec단위.. (time*10^-6}*{level*10^-6)

    if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)  // 전하량 초과
    {
        TDC_PRINTF_E("[MAPPING] IMPEDANCE CHECK - MAX CHARGE OVER \r\n");
        tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__MaxChargeOver, __LINE__);  // 에러 전송
        tdc_ble_mapping_clear_command();                                                                         // 커맨드 리셋;
    }

    // 가장 출력이 큰 DAC  사용
    s_stimulDAC_slope            = Stimulation_DAC_D;
    s_stimulDAC_offsetResolution = Offset_DAC_A;

    if (s_mappingPacket->impedanceCheck.stimulationLevel_uA < (stimulDAC_D_only_Saturation_uA + offsetDAC_A_Saturation_uA))
    {
        if (s_mappingPacket->impedanceCheck.stimulationLevel_uA
            > stimulDAC_D_only_Saturation_uA)  // 오프셋을 적용하지 않은 상태에서 출력할 수 없으면 오프셋 적용
        {
            s_stimulDAC_offsetValue_uA = stimulDAC_A_only_Saturation_uA;  // 510
        }
        else
        {
            s_stimulDAC_offsetValue_uA = 0;
        }

        tempInt                 = s_stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
        s_stimulDAC_offsetValue = tempInt >> 15;

        stimulLevel_uA    = s_mappingPacket->impedanceCheck.stimulationLevel_uA - s_stimulDAC_offsetValue_uA;
        tempInt           = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_D;
        s_stimulLevel_255 = tempInt >> 15;  // 기울기 스텝으로 나눈다.
    }
    else  // 최소 기울기에 대해서만 일단 구현.. 모든 범위의 출력을 설정하려면 추가 코딩이 필요하나.. 임피던스는 작은 출력으로 측정하기 때문에 필요가 없을 듯하다.
    {
        TDC_PRINTF_E("[MAPPING] IMPEDANCE CHECK - STIMUL LEVEL OVER \r\n");
        tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__stimulLevelOver, __LINE__);  // 에러 전송
        tdc_ble_mapping_clear_command();                                                                           // 커맨드 리셋;
    }
}

// 펄스폭 최소화 후 이전 자극 파라미터를 흘려보낸다
static void tdc_isd_map_impedance_flow01_flush_previous(void)
{
    int pcm_index = 0;

    // 이전에 전송된 자극 파라미터 값이 있을 수 있기 때문에 출력을 내보내기 위하여 설정 파라미터들 전송한다

    // 펄스폭 0으로 설정 - ISD 설정 커맨드를 보낼때는 펄스폭을 0으로 설정해서 보내는 것이 심플함.
    tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(FPGA_pulsePhaseWidth_minimum);

    // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
    tdc_isd_fill_pcm_last_stimulation_out(&pcm_index);

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // NOP-Standby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // PCM 출력 모드 변경
}

// 백텔 FIFO 를 지우고 비었는지 확인
static void tdc_isd_map_impedance_flow02_clear_fifo(void)
{
    bool FPGA_FIFO_empty;
    int  pcm_index = 0;

    if (tdc_isd_fpga_write_clear_fifo())
    {
        // FPGA 상태를 읽어 본다.
        if (tdc_isd_fpga_check_fpga_fifo_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
        {
            if (!FPGA_FIFO_empty)
            {
                TDC_PRINTF_E("[MAPPING] IMPEDANCE CHECK - FIFO NOT EMPTY \r\n");
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //
            }
        }
    }

    // FPGA backtel 레지스터 설정
    tdc_isd_fpga_change_12_bit_backtel_mode(pcm_index++);

    // 나머지는 Nop으로 채움.
    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // NOP-Standby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // PCM 출력 모드 변경
}

// 자극 파라미터 레지스터 전송 (DAC 오프셋 · 자극 설정)
static void tdc_isd_map_impedance_flow04_write_stim_para(void)
{
    int w_isd_registerValue;
    int pcm_index = 0;

    // 자극 출력 DAC 설정
    // ISD 0x5   - DAC Offset 쓰기
    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | s_stimulDAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);  //  0x0b00  | 0x50000
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

    // ISD 0x6    - 자극 파라미터 설정  쓰기
    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

    // 0, 7번비트
    w_isd_registerValue = w_isd_registerValue << 1;
    // offsetResolution 6번 비트
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | s_stimulDAC_offsetResolution;

    // DAC slope    4,5번 비트
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | s_stimulDAC_slope;

    // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
    w_isd_registerValue = w_isd_registerValue << 2;

    // monopolr_mp2 :
    w_isd_registerValue = w_isd_registerValue | en__monopolr_rod;  // en__monopolr_rod =2

    // 자극 출력 모드 - 모노폴라
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | 0;

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x0d04 | 0x50000

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

    // ISD 0x9    - 임피던스 측정 설정
    w_isd_registerValue = ISD_registerAddr_adc_measurement;

    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

    w_isd_registerValue = w_isd_registerValue << 3;
    w_isd_registerValue = w_isd_registerValue | adc_measurementMode_impedance;  // 측정 모드 임피던스

    w_isd_registerValue = w_isd_registerValue << 3;
    w_isd_registerValue = w_isd_registerValue | adcSamplingRate20kHz;  // adc 샘플링 주파수

    w_isd_registerValue = w_isd_registerValue << 2;

    w_isd_registerValue = w_isd_registerValue | mesurementStart_on_configureADCregister;  // adc 측정 시작 시점  :ADC 0x08 레지스터 설정  시점

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x1305 | 0x50000

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

    // ISD 0x8    - 측정 채널

    w_isd_registerValue = ISD_registerAddr_adc_samplingChannel;

    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

    w_isd_registerValue = w_isd_registerValue << 8;
    // 전극 번호는 비트 [4:0] 범위, 범위 초과한 값 입력시 FIFO 클리어 발생 -> 이후 백텔 카운트 0 에러 발생할 수 있음
    w_isd_registerValue = w_isd_registerValue | (0x1F & electrodeMap[s_electrodeNum]);

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  // 0x1100 | 0x50000

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    // 나머지 버퍼는  NOP-Backtel
    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    }

    // NOP-Standby
    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopBacktel);
    // PCM 출력 모드 변경
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);
}

// PCM 출력을 백텔 수신 모드로 전환
static void tdc_isd_map_impedance_flow06_pcm_backtel_mode(void)
{
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopBacktel);
}

// 측정 펄스폭 설정 - 시작폭과 끝폭을 번갈아 쓴다
static void tdc_isd_map_impedance_flow08_set_pulse_width(void)
{
    int pulseWidth;
    int w_isd_registerValue;
    int i;
    int numFramePerChannel_index;
    int pcm_index = 0;

    // 펄스 폭 설정 - FPGA에 전송
    if (s_toggle_start_end_pulseWidth)
    {
        pulseWidth                    = (s_mappingPacket->impedanceCheck.pulseWidth_start_usec);
        s_numFramePerChannel          = s_numFramePerChannel_startWidth;
        s_toggle_start_end_pulseWidth = false;
    }
    else
    {
        pulseWidth                    = (s_mappingPacket->impedanceCheck.pulseWidth_end_usec);
        s_numFramePerChannel          = s_numFramePerChannel_endWidth;
        s_toggle_start_end_pulseWidth = true;
    }

    tdc_isd_fpga_change_pulse_width(pcm_index++, pulseWidth);
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(pulseWidth);

    // 자극 출력 파라미터

    w_isd_registerValue = positivePulseFirst << firstPulsePhasePositionAtPCM_Mold;  // Positive Pulse first;
    // 전극 번호는 비트 [4:0] 범위 (라이브·0x65 경로와 동일한 마스크)
    w_isd_registerValue = w_isd_registerValue | ((0x1F & electrodeMap[s_electrodeNum]) << electrodIndexPositionAtPCM_Mold);  // 자극 전극 번호
    w_isd_registerValue = w_isd_registerValue | (s_stimulLevel_255 << stimulationPositionAtPCM_Mold);                        // 자극 출력 크기

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    // 펄스 폭을 맞추기 위한  Nop-Token

    for (numFramePerChannel_index = 1; numFramePerChannel_index < s_numFramePerChannel; numFramePerChannel_index++)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    //

    if (pcm_index >= df_MaxNumTransferableChannel)
    {
        tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__PCM_GEN_ERROR, en__PCMBufferOwerFlow, __LINE__);  // 에러 전송
        tdc_ble_mapping_clear_command();                                                                                 // 커맨드 리셋;
    }
    else
    {
        for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
        {
            tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
        }
    }

    // NOP-Standby
    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);
    // PCM 출력 모드 변경
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);
}

// ADC 측정 시작 - 측정용 자극 출력
static void tdc_isd_map_impedance_flow10_start_measure(void)
{
    int w_isd_registerValue;
    int i;
    int numFramePerChannel_index;
    int pcm_index = 0;

    // 측정용 자극 출력 또는 0x09 설정 파라미터

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

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    // 펄스 폭을 맞추기 위한  Nop-Token
    for (numFramePerChannel_index = 1; numFramePerChannel_index < (s_numFramePerChannel - 1);
         numFramePerChannel_index++)  // 펄스폭이 넓어질 경우 NOP과 Backtel NOP의 타이밍 문제로 NOP의 개수를 1개 줄이고 Backtel NOP으로 대체
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }
    // 나머지 버퍼는  NOP-Backtel

    if (pcm_index >= df_MaxNumTransferableChannel)
    {
        tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__PCM_GEN_ERROR, en__PCMBufferOwerFlow, __LINE__);  // 에러 전송
        tdc_ble_mapping_clear_command();                                                                                 // 커맨드 리셋;
    }
    else
    {
        for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
        {
            tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopBacktel);
        }
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopBacktel);
    // PCM 출력 모드 변경
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);
}

// PCM 출력을 백텔 수신 모드로 전환
static void tdc_isd_map_impedance_flow12_pcm_backtel_mode(void)
{
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopBacktel);
}

// 임피던스 측정값 읽어 저장
static void tdc_isd_map_impedance_flow14_read_measured(void)
{
    bool FPGA_error;
    int  backtelCounter;
    int  backtelFIFO[2];

    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    if (tdc_isd_fpga_check_fpga_pcm_error(&FPGA_error))
    {
        if (!FPGA_error)
        {
            // 백텔 수신 확인
            if (tdc_isd_fpga_read_fifo_counter(&backtelCounter))
            {
                if (backtelCounter == 2)
                {
                    // 백텔 데이터
                    // if (tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_BackTel, backtelFIFO, 2))
                    if (tdc_isd_fpga_read_backtel_fifo(backtelFIFO, 2))
                    {
                        if (s_toggle_start_end_pulseWidth)  // s_toggle_start_end_pulseWidth // 긴 폭의 측정이 완료되었다.- 1회 측정 완료.
                        {
                            // 펄스 폭 넚은 것의 측정값
                            impedanceValue[s_iterationNum][en__endPulse_saving_index][FIPGA_FIFO_index_0] = (char) backtelFIFO[FIPGA_FIFO_index_0];
                            impedanceValue[s_iterationNum][en__endPulse_saving_index][FIPGA_FIFO_index_1] = (char) backtelFIFO[FIPGA_FIFO_index_1];

                            s_iterationNum++;
                            if (s_iterationNum < s_mappingPacket->impedanceCheck.iterationNum)  // 한 채널의 측정이 완료
                            {
                                s_flowCounter = 0;  // 반복횟수가 완료될 때까지 측정을 다시 한다.
                            }
                            else
                            {
                                s_flowCounter_SendingSPI = s_flowCounter;
                            }
                        }
                        else  //  짧은 폭의 측정이 완료 되었다.
                        {
                            // 좁은 펄스폭
                            impedanceValue[s_iterationNum][en__startPulse_saving_index][FIPGA_FIFO_index_0] = (char) backtelFIFO[FIPGA_FIFO_index_0];
                            impedanceValue[s_iterationNum][en__startPulse_saving_index][FIPGA_FIFO_index_1] = (char) backtelFIFO[FIPGA_FIFO_index_1];
                            s_flowCounter                                                                   = 0;
                        }
                    }
                }
                else
                {
                    tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 백텔 안들어옴 에러 // 내부기 전송 파워 설정 부터 다시.
                    tdc_sys_error_send_to_app(en__mapping_impedanceChekck, en__EN__ISD_ERROR, en__BackTelCounterZero, __LINE__);  // 에러 전송
                    tdc_ble_mapping_clear_command();                                                                              // 커맨드 리셋;
                }
            }
        }
        else
        {
            tdc_isd_change_state(en__isdStatus_PowerIC_OK);  // FPGA 에러 발생, FPGA 초기화
            tdc_sys_error_send_to_app(
                en__mapping_eCAP_Measurement_masking, en__FPGA_CONFIGUARATION_ERROR, FPGA_error, __LINE__);  // 에러 전송 // FPGA 에러 값을 그대로 전달
            tdc_ble_mapping_clear_command();                                                                 // 커맨드 리셋;
        }
    }
}

// 한 채널의 반복 측정이 끝났다. 측정값을 앱으로 보내거나 다음 채널로 넘어간다.
static void tdc_isd_map_impedance_send_or_advance(void)
{
    int     i;
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index;

    {
        buffer_tx_index = 0;

        if (s_ble_transfer_index >= s_mappingPacket->impedanceCheck.iterationNum)  // 측정데이터 전송이 완료 되었으면  다음
        {
            s_ble_transfer_index = 0;

            // 측정 진행 여부 확인

            if (s_mappingPacket->impedanceCheck.channel == 255)  // 전 채널 측정일 경우 채널을 증가 시키면서 측정한다.
            {
                s_electrodeNum++;
                if (s_electrodeNum >= df_MaxNumOfElectrode)
                {
                    tdc_ble_mapping_clear_command();  // 명령 종료 // 커맨드 리셋;
                }
                s_iterationNum = 0;
                s_flowCounter  = 0;
            }
            else  // 단일 채널일 경우 반복 측정이 완료 되었으므로 종료
            {
                tdc_ble_mapping_clear_command();  // 명령 종료 // 커맨드 리셋;
            }
        }
        else
        {
            // 송신 데이터 준비
            // command loop-back
            bufferForSPI_tx[buffer_tx_index++] = s_mappingPacket->command;

            // pay-load 준비
            bufferForSPI_tx[buffer_tx_index++] = en__MonoPolar_impedance;

            bufferForSPI_tx[buffer_tx_index++] = s_electrodeNum + 1;  // 채널 번호 1~32

            /* impedanceValue 는 char 배열이라(-fsigned-char) 0x80 이상 값이 음수로 담긴다.
         * 아래 (int) 캐스팅에서 부호 확장이 일어나지만 하위 8비트는 보존되고,
         * bufferForSPI_tx 가 uint8_t 라 대입 시 그 하위 8비트만 남는다.
         * 전환 전에는 int 버퍼에 담겼다가 SPI 송신 단계에서 잘렸으므로
         * 결과 바이트는 동일하다 (2026-07-28 B3b 전환 시 확인). */
            for (i = 0; i < Max_ImpedanceReturnDataSize; i++)  // ble 패킷 사이즈로 인하여..1회 전달 시 최대, 4번 측정한 데이터 전달 가능.
            {
                if (s_ble_transfer_index < s_mappingPacket->impedanceCheck.iterationNum)
                {
                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[s_ble_transfer_index][en__startPulse_saving_index][FIPGA_FIFO_index_0];
                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[s_ble_transfer_index][en__startPulse_saving_index][FIPGA_FIFO_index_1];

                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[s_ble_transfer_index][en__endPulse_saving_index][FIPGA_FIFO_index_0];
                    bufferForSPI_tx[buffer_tx_index++] = (int) impedanceValue[s_ble_transfer_index][en__endPulse_saving_index][FIPGA_FIFO_index_1];

                    s_ble_transfer_index++;
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
}

void tdc_isd_map_impedance_step(bool startFlag)
{
    if (startFlag)
    {
        tdc_isd_map_impedance_reset();
    }

    switch (s_flowCounter)
    {
        case 0:
        {
            tdc_isd_map_impedance_flow00_calc_frame_and_dac();
            break;
        }

        case 1:
        {
            tdc_isd_map_impedance_flow01_flush_previous();
            break;
        }

        case 2:
        {
            tdc_isd_map_impedance_flow02_clear_fifo();
            break;
        }

        case 4:  //3:
        {
            tdc_isd_map_impedance_flow04_write_stim_para();
            break;
        }

        case 6:  //4:
        {
            tdc_isd_map_impedance_flow06_pcm_backtel_mode();
            break;
        }

        case 8:  //5:
        {
            tdc_isd_map_impedance_flow08_set_pulse_width();
            break;
        }

        case 10:  //6:
        {
            tdc_isd_map_impedance_flow10_start_measure();
            break;
        }

        case 12:  //10:
        {
            tdc_isd_map_impedance_flow12_pcm_backtel_mode();
            break;
        }

        case 14:  //12:  // 임피던스 측정값 읽음
        {
            tdc_isd_map_impedance_flow14_read_measured();
            break;
        }

        default:
        {
            break;
        }
    }  // end, switch (s_flowCounter)

    if (s_flowCounter > s_flowCounter_SendingSPI)  // 한채널의 반복 측정이 완료된 상태이다.
    {
        tdc_isd_map_impedance_send_or_advance();
    }

    s_flowCounter++;
}
