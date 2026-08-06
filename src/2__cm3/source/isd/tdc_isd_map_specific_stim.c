

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
#include <tdc_isd_stim_mode_encode.h>
#include <tdc_ble_mapping.h>
#include <tdc_hal_spi.h>
#include <tdc_stim_para_cal.h>

#include <electrodeMapping.h>

// int tempBuff[24];

/*
  0x65 특정자극 실행 - flowCounter 상태 기계.

  tdc_ble_mapping.c 가 1ms 주기로 tdc_isd_map_specific_stim_step() 을 부르고,
  이 파일이 flowCounter 를 하나씩 올리며 단계를 진행한다.

    0   자극 파라미터 계산 (프레임 수 · 전하량 검사 · DAC 설정 · 유지시간)
    1   펄스폭 최소화 · 8비트 백텔 · 이전 자극 출력
        (바이폴라가 아니면 여기서 flowCounter 를 +6 해 3·5·7 을 건너뛴다)
    3   바이폴라 기준전극 버퍼 구성 + ISD FIFO 클리어
    5   기준전극 번호 전송 - 앞 24개
    7   기준전극 번호 전송 - 나머지 8개
    9   자극 파라미터 레지스터 전송 (DAC 오프셋 · 자극 설정 · 펄스폭)
    14  펄스폭이 실제로 반영됐는지 검증. 통과하면 cofigureDoneCounter 를 세운다
    20  설정값 요약 로그

  cofigureDoneCounter 를 넘어서면 매 호출마다 자극 PCM 을 채운다(_output_fill).
  유지시간이 지나면 마지막 출력을 내보내고 명령을 종료한다(_output_finish).

  2026-08-06 분해 전에는 이 전부가 한 함수 605줄이었다.
  상태 사이로 값을 넘겨야 해서 함수-지역 static 이 많았고, 분해하면서
  파일 범위로 승격했다. 단일 인스턴스라는 점은 그대로다.
*/

// 상태 기계가 단계 사이로 넘기는 값들. 분해 전에는 함수-지역 static 이었다.
static int flowCounter            = 0;
static int stimulationTime_msec   = 0;
static int stimulationHoldTime_msec;
static int cofigureDoneCounter    = 100;

// case 0 이 계산해 case 9 · 20 과 출력 단계가 쓴다.
static int numFramePerChannel;
static int TransferabelChannelNum = df_MaxNumTransferableChannel;
static int stimulDAC_slope;
static int stimulDAC_offsetResolution;
static int stimulDAC_offsetValue;
static int stimulLevel_255;

// case 3 이 채우고 case 5 · 7 이 전송한다.
static int bipolarReferenceElectrodeNum[df_MaxNumOfElectrode];

// 출력 단계가 쓴다.
static int channel_index         = 0;
static int pulseDurationNop_idex = 1;
static int bipolarFIFO_index     = 0;  // 임시 디버깅용

static ST__MAPPING_PACKET const *mappingPacket;

// 자극 크기에서 DAC 기울기 · 오프셋 · 출력값을 정한다.
//
// 자극 DAC 는 2uA 기울기(A)와 6uA 기울기(C)를 갖고, 오프셋 DAC 는 A·B 두 해상도를 갖는다.
// 요청 자극 크기를 표현할 수 있는 조합을 고르고, 오프셋을 뺀 나머지를 255 단위로 환산한다.
static void tdc_isd_map_specific_stim_calc_dac_setting(void)
{
    int tempInt;
    int stimulDAC_offsetValue_uA;
    int stimulLevel_uA;

    // 자극 슬로프 및 자극 데이터. 계산 stimulLevel_uA stimulLevel_uA stimulDAC_offsetValue_uA
    // 마스커에 대한 것
    if (mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA < (stimulDAC_A_only_Saturation_uA + offsetDAC_B_Saturation_uA))  // 2uA 기울기로 전달 가능한 범위내. // 1530 보다 작은 경우
    {
        // 자극 DAC 기울기 2uA
        stimulDAC_slope = Stimulation_DAC_A;

        if (mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA > (stimulDAC_A_only_Saturation_uA + offsetDAC_A_Saturation_uA))  // 1020~1530  - > 1020 오프셋을 적용해야 되는 경우
        {
            stimulDAC_offsetResolution = Offset_DAC_B;
            stimulDAC_offsetValue_uA   = offsetDAC_B_Saturation_uA;
            tempInt                    = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_B;
            stimulDAC_offsetValue      = tempInt >> 15;

            stimulLevel_uA  = mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA - stimulDAC_offsetValue_uA;
            tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
            stimulLevel_255 = tempInt >> 15;
        }
        else  // 1020보다 작은 경우
        {
            //
            if (mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA > stimulDAC_A_only_Saturation_uA)  // 510 ~ 1020 -> 510 오프셋을 적용한다.
            {
                stimulDAC_offsetResolution = Offset_DAC_A;
                stimulDAC_offsetValue_uA   = offsetDAC_A_Saturation_uA;  //
                tempInt                    = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
                stimulDAC_offsetValue      = tempInt >> 15;

                stimulLevel_uA  = mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                stimulLevel_255 = tempInt >> 15;
            }
            else  // 오프셋 적용을 안해도 되는 경우.
            {
                stimulDAC_offsetResolution = Offset_DAC_A;
                stimulDAC_offsetValue_uA   = 0;  //
                stimulDAC_offsetValue      = 0;

                stimulLevel_uA  = mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA - stimulDAC_offsetValue_uA;
                tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
                stimulLevel_255 = tempInt >> 15;
            }
        }
    }
    else  // 6uA로  자극
    {
        // 자극 DAC 기울기 6uA
        stimulDAC_slope = Stimulation_DAC_C;

        stimulDAC_offsetResolution = Offset_DAC_A;
        stimulDAC_offsetValue_uA   = offsetDAC_A_Saturation_uA;
        tempInt                    = stimulDAC_offsetValue_uA * reciprocal_dividing_QI1F15_Offset_DAC_A;
        stimulDAC_offsetValue      = tempInt >> 15;

        stimulLevel_uA  = mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA - stimulDAC_offsetValue_uA;
        tempInt         = stimulLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_C;
        stimulLevel_255 = tempInt >> 15;
    }

    if (stimulLevel_255 >= 255)
    {
        stimulLevel_255 = 255;
    }
}

// flow 0 - 자극 파라미터를 계산한다. 실제 전송은 뒤 단계들이 한다.
static void tdc_isd_map_specific_stim_flow00_calc_para(void)
{
    int deliveryCharge_pico;

    // 이전에 전송된 자극 파라미터 값이 있을 수 있기 때문에 출력을 내보내기 위하여 설정 파라미터들 전송한다.
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    // 펄스폭에 따른 자극 프레임 갯수 계산
    mappingPacket      = tdc_ble_mapping_get_packet();
    numFramePerChannel = tdc_stim_calc_frame_per_channel(mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth);

    // 사용가능한 채널 수는 24채널 보다 적고 펄스폭은 길게 설정할 경우... 가능하다면 1msec에 1회의 출력이 나올 수 있도록...
    TransferabelChannelNum = tdc_stim_calc_transferable_channel_num(numFramePerChannel, mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum);

    // 최대 전하량 계산

    // usec단위... (time*10^-6}*{level*10^-6) //마스커 자극값을 기준으로 계산한다.
    deliveryCharge_pico = (mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth) * mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA;

    if (deliveryCharge_pico > df_MaxDeliveryCharge_pC)  // 전하량 초과
    {
        tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__EN__ISD_ERROR, en__MaxChargeOver, __LINE__);  // 에러 전송
        tdc_ble_mapping_clear_command();                                                                            // 커맨드 리셋;
    }

    tdc_isd_map_specific_stim_calc_dac_setting();

    stimulationHoldTime_msec = mappingPacket->tdc_isd_map_specific_stim_step.stimulationTime_100msec * 100;
}

// flow 1 - 펄스폭을 최소로 되돌리고 백텔을 8비트로 맞춘 뒤 이전 자극 파라미터를 흘려보낸다.
// 바이폴라가 아니면 기준전극 설정 단계(3·5·7)를 건너뛴다.
static void tdc_isd_map_specific_stim_flow01_reset_pulse(void)
{
    int pcm_index = 0;

    // 펄스 폭 0으로 설정
    tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(FPGA_pulsePhaseWidth_minimum);

    tdc_isd_fpga_change_8_bit_backtel_mode(pcm_index++);  // 8비트 백텔 설정 (2025.12.17 바이폴라 기능)
    tdc_isd_fill_pcm_last_stimulation_out(&pcm_index);    // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    if (mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode != en__bipolar)
    {
        flowCounter = flowCounter + 6;  // 바이폴라 기준전극 설정 값 전송에 6msec 필요, 해당 루틴 생략 (2025.12.17 바이폴라 기능)

        // 1 + 6 = 7; case 7로 강제 설정한 뒤,
        // 마지막에 flowCounter++; 수행하여 다음 진입 시 flowCounter가 8인 상태로 만든다.
        // 즉, 바이폴라 설정 과정을 생략할 때 적용되는 것이다.
    }
}

// flow 3 - 바이폴라 기준전극 버퍼를 구성하고 ISD 쪽 FIFO 를 지운다. (2025.12.17 바이폴라 기능)
static void tdc_isd_map_specific_stim_flow03_build_reference(void)
{
    int w_isd_registerValue;
    int pcm_index = 0;
    int i;

    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        bipolarReferenceElectrodeNum[i] = unusedReferenceElectrode_DummyNum;
    }

    /* 전극번호 범위 방어 (2026-07-30 추가).
     * 라이브 경로 tdc_isd_stim_para_setting.c:486 의 방어를 이식한 것이다.
     * 그쪽은 2026-07-27 에 막혔는데 이 경로는 남아 있었다.
     *
     * 매핑 앱은 모노폴라 기준전극을 99 로 보낸다. 0x65 파싱
     * (tdc_ble_cmd_0x65_specific.c:86)이 이를 stimulationMode 와 무관하게
     * 통과시키므로, 바이폴라 + 99 조합이 여기까지 도달한다.
     * 거르지 않고 electrodeMap[99 - 1] 을 읽으면 32원소 배열의 범위를
     * 264바이트 벗어나 인접 전역(s_tdc_table, ui/tdc_ui_command.c:120)의
     * 포인터 값을 집어온다. 실기 로그의 "REF ELEC NUM : 99 (PCB : 70378)"
     * 이 그 미정의 동작이며 70378 = 0x112EA 는 .text 범위 안 주소다.
     *
     * 그 값이 기준전극으로 레지스터에 실리면 비트 [4:0] 을 넘어 ISD 내부
     * FIFO 클리어를 유발하고 백텔 카운트 0 에러로 이어진다
     * (flow 5 - flow 7 의 마스크 주석 참조).
     *
     * 바로 위 루프에서 전 원소가 unusedReferenceElectrode_DummyNum(31) 로
     * 초기화되므로, 여기서 건너뛰면 그 더미값이 그대로 유지된다.
     * 자극 전극번호도 함께 검사한다 - 좌변 첨자라 범위 밖이면 쓰기가 된다. */
    if ((mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum < 1)                             //
        || (df_MaxNumOfElectrode < mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum)       //
        || (mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum < 1)                     //
        || (df_MaxNumOfElectrode < mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum))
    {
        TDC_PRINTF_I("[SPEC] STIM ELEC NUM : %d, REF ELEC NUM : %d, OUT OF RANGE - USE DUMMY \r\n",  //
                  mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum,
                  mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum);
    }
    else
    {
        bipolarReferenceElectrodeNum[electrodeMap[mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum - 1]] =  // 코드 길어서 line wrapping
            electrodeMap[mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum - 1];
    }

    // Bipolar 기준전극 FIFO 지우기
    w_isd_registerValue = ISD_registerAddr_cipherDataStatus;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | 0x80;  // CHIP_ID_FIFO_RDDATA_INDEX에 아무값이나 쓰면 FIFO가 지워진다.
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
}

// flow 5 · 7 공통 - 기준전극 번호를 [first, last) 구간만큼 ISD 로 전송한다.
// 둘로 나뉜 이유는 한 번에 보낼 수 있는 PCM 채널 수(df_MaxNumTransferableChannel)가 32보다 작기 때문이다.
static void tdc_isd_map_specific_stim_send_reference_range(int first, int last)
{
    int w_isd_registerValue;
    int pcm_index = 0;
    int i;

    for (i = first; i < last; i++)
    {
        w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
        w_isd_registerValue = w_isd_registerValue << 1;
        w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
        w_isd_registerValue = w_isd_registerValue << 8;

        // 바이폴러 레퍼런스 전극 번호는 비트 [4:0] 범위, 범위 초과한 값 입력시 FIFO 클리어 발생 -> 이후 백텔 카운트 0 에러 발생할 수 있음
        w_isd_registerValue = w_isd_registerValue | (0x1F & bipolarReferenceElectrodeNum[i]);
        w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

        tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
    }

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
}

// flow 9 - 자극 파라미터 레지스터를 전송한다 (DAC 오프셋 · 자극 설정 · 펄스폭).
static void tdc_isd_map_specific_stim_flow09_send_stim_para(void)
{
    int w_isd_registerValue;
    int pcm_index = 0;

    // 자극 출력 DAC 설정
    // ISD 0x5 - DAC Offset "쓰기"
    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetValue;  // 마스커 인덱스 값
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);  //      | 0x50000

    // ISD 0x6 - 자극 파라미터 설정 "쓰기"
    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

    // 0, 7번비트
    w_isd_registerValue = w_isd_registerValue << 1;

    // offsetResolution 6번 비트
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | stimulDAC_offsetResolution;

    // DAC slope 4,5번 비트
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | stimulDAC_slope;

    // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_reference_bits(mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode);

    // 자극 출력 모드 0,1번 비트
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_output_bits(mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode);

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;  //  | 0x50000

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    // 펄스 폭 조정
    tdc_isd_fpga_change_pulse_width(pcm_index++, mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth);
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(FPGA_pulsePhaseWidth_minimum);

    // 나머지 버퍼는 NOP-Standby
    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    // 임시 디버깅 용
    bipolarFIFO_index = 1;
}

// flow 14 - 펄스폭이 실제로 반영됐는지 확인한다. 통과하면 이 시점을 설정 완료로 기록한다.
static void tdc_isd_map_specific_stim_flow14_verify_pulse_width(void)
{
    int r_FPGA_registerValue;

    if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
    {
        if (r_FPGA_registerValue == mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth)
        {
            cofigureDoneCounter = flowCounter;
        }
        else
        {
            tdc_sys_error_send_to_app(en__mapping_specific_stimulation, en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, __LINE__);
            tdc_ble_mapping_clear_command();  // 커맨드 리셋;

            if (tdc_isd_fpga_read_system_error_flag(&r_FPGA_registerValue))
            {
                if ((0x0F & r_FPGA_registerValue) != 0)
                {
                    tdc_isd_change_state(en__isdStatus_PowerIC_OK);
                }
            }
        }
    }
}

// flow 20 - 어떻게 처리되는 건지 디버그 메시지 출력을 위한 별도의 단계
static void tdc_isd_map_specific_stim_flow20_log_summary(void)
{
    TDC_PRINTF_I("[SPEC] USABLE ELEC NUM : %d \r\n", mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum);
    TDC_PRINTF_I("[SPEC] PULSE WIDTH : %d \r\n", mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth);
    TDC_PRINTF_I("[SPEC] FRAME NUM PER 1 CH : %d \r\n", numFramePerChannel);
    TDC_PRINTF_I("[SPEC] TRANSFERABLE CH NUM PER 1 MS : %d \r\n", TransferabelChannelNum);
    TDC_PRINTF_I("[SPEC] STIM LEVEL (UA) : %d \r\n", mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA);
    TDC_PRINTF_I("[SPEC] DELIVERY CHARGE PICO : %d \r\n",  //
              (mappingPacket->tdc_isd_map_specific_stim_step.pulseWidth) * mappingPacket->tdc_isd_map_specific_stim_step.stimulationLevel_uA);
    TDC_PRINTF_I("[SPEC] STIM DAC SLOPE : %d \r\n", stimulDAC_slope);
    TDC_PRINTF_I("[SPEC] STIM DAC VALUE : %d \r\n", stimulLevel_255);
    TDC_PRINTF_I("[SPEC] OFFSET DAC RESOLUTION : %d \r\n", stimulDAC_offsetResolution);
    TDC_PRINTF_I("[SPEC] OFFSET DAC VALUE : %d \r\n", stimulDAC_offsetValue);
    TDC_PRINTF_I("[SPEC] STIM HOLD TIME (MS)  : %d \r\n", stimulationHoldTime_msec);
    TDC_PRINTF_I("[SPEC] REF CH MODE : %s \r\n",  //
              mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__monopolr_body          ? "MP-B"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__monopolr_rod         ? "MP-R"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__monopolr_BothRodBody ? "MP-R&B"
                                                                                              : "BP, CG, ETC...");
    TDC_PRINTF_I("[SPEC] STIM MODE : %s \r\n",  //
              mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__monopolr_body          ? "MP-B"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__monopolr_rod         ? "MP-R"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__monopolr_BothRodBody ? "MP-R&B"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__bipolar              ? "BP"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__commonground         ? "CG"
              : mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__semi_simultaneously  ? "SIMULTANEOUSLY"
                                                                                              : "INVALID");

    if (mappingPacket->tdc_isd_map_specific_stim_step.stimulationMode == en__bipolar)
    {
        TDC_PRINTF_I("[SPEC] BIPOLAR REF CH NUM : %d (PCB : %d) \r\n",  //
                  mappingPacket->tdc_isd_map_specific_stim_step.bipolarReferenceElectrodeNum,
                  bipolarReferenceElectrodeNum[electrodeMap[(mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum - 1)]]);
    }

    TDC_PRINTF_I("[SPEC] STIM CH NUM : %d (PCB : %d ) \r\n",  //
              mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum,
              electrodeMap[(mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum - 1)]);
}

// 유지시간이 지났다. 마지막 자극을 내보내고 명령을 종료한다.
static void tdc_isd_map_specific_stim_output_finish(int pcm_index)
{
    uint8_t bufferForSPI_tx[BLE_DataPacketSize];
    int     buffer_tx_index = 0;

    // 마지막 자극을 위하여 자극 출력
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);

    tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(FPGA_pulsePhaseWidth_minimum);

    // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력
    tdc_isd_fill_pcm_last_stimulation_out(&pcm_index);

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    // 출력 완료.

    // 송신 데이터 준비
    // command loop-back
    bufferForSPI_tx[buffer_tx_index++] = mappingPacket->command;

    // pay-load 준비
    bufferForSPI_tx[buffer_tx_index++] = 1;  // 자극 출력 성공

    // 송신 데이터 SPI TX버퍼에 복사

    tdc_hal_spi_write_tx_buffer(bufferForSPI_tx, buffer_tx_index);

    // 커맨드 리셋;
    tdc_ble_mapping_clear_command();

    TDC_PRINTF_I("[MAPPING] STIMULATION HOLD TIME FINISHED \r\n");
}

// CFX 가 SpecificCommand 버퍼를 읽는 중이면 끝날 때까지 기다린다.
// 읽는 도중에 덮어쓰면 반쪽짜리 명령이 나간다.
static void tdc_isd_map_specific_stim_wait_cfx_idle(void)
{
    int specific_command_idle_loop_i;         // by 김은수
    int specific_command_idle_check_counter;  // by 김은수

    // SpecificCommand 체크 카운터가 1 이상이면, CFX가 SpecificCommand를 읽는 중임
    for (specific_command_idle_loop_i = 0; specific_command_idle_loop_i < 50; specific_command_idle_loop_i++)
    {
        specific_command_idle_check_counter = 0;
        Sys_Delay(SystemCoreClock / 1000000);  // 1usec
        specific_command_idle_check_counter += cfx_cm3_sharedMemoryAll.is_pcm_specific_command_reading;
        Sys_Delay(SystemCoreClock / 1000000);  // 1usec
        specific_command_idle_check_counter += cfx_cm3_sharedMemoryAll.is_pcm_specific_command_reading;

        if (specific_command_idle_check_counter == 0)
        {
            // CFX가 SpecificCommand를 읽고 있지 않음
            break;
        }

        TDC_PRINTF_I("[MAPPING] NOW, CFX IS READING SPECIFIC COMMAND (IDLE LOOP CNT: %d) \r\n", specific_command_idle_loop_i);
    }

    if (50 <= specific_command_idle_loop_i)
    {
        TDC_PRINTF_E("[MAPPING] SPECIFIC COMMAND TIMING ISSUE OCCURRED \r\n");
    }
}

// 자극 유지 중 - 이번 1ms 분의 자극 PCM 을 채운다.
// 채널을 돌아가며 하나씩 자극하고 나머지 시점은 NOP 으로 메운다.
static void tdc_isd_map_specific_stim_output_fill(int pcm_index)
{
    int w_isd_registerValue;
    int i;

    tdc_isd_map_specific_stim_wait_cfx_idle();

    for (i = 0; i < TransferabelChannelNum; i++)  //
    {
        if (channel_index == 0)  // 해당자극 출력
        {
            w_isd_registerValue = mappingPacket->tdc_isd_map_specific_stim_step.firstPulsePhase << firstPulsePhasePositionAtPCM_Mold;                                                    // 선행 펄스
            w_isd_registerValue = w_isd_registerValue | (electrodeMap[(mappingPacket->tdc_isd_map_specific_stim_step.stimulationElectrodeNum - 1)] << electrodIndexPositionAtPCM_Mold);  // 자극 전극 번호
            w_isd_registerValue = w_isd_registerValue | (stimulLevel_255 << stimulationPositionAtPCM_Mold);                                                                   // 자극 출력 크기
            w_isd_registerValue = w_isd_registerValue | pcm_Mold_Stimulation;

            // tempBuff[pcm_index]=w_isd_registerValue;
            tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

            bipolarFIFO_index++;  // 임시 디버깅용
        }
        else  //
        {
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);  // 다른 채널 자극 시점..
        }

        // 펄스 폭에 맞추어 NOP
        for (pulseDurationNop_idex = 1; pulseDurationNop_idex < numFramePerChannel; pulseDurationNop_idex++)
        {
            tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
        }

        channel_index++;

        if (mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum > TransferabelChannelNum)
        {
            if (channel_index >= mappingPacket->tdc_isd_map_specific_stim_step.usableElectrodeNum)
            {
                channel_index = 0;
            }
        }
        else
        {
            if (channel_index >= TransferabelChannelNum)
            {
                channel_index = 0;
            }
        }
    }

    //
    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
}

void tdc_isd_map_specific_stim_step(bool startFlag)
{
    int pcm_index = 0;

    if (startFlag)
    {
        flowCounter          = 0;
        stimulationTime_msec = 0;
        channel_index        = 0;
    }

    switch (flowCounter)
    {
        case 0:
        {
            tdc_isd_map_specific_stim_flow00_calc_para();
            break;
        }

        case 1:
        {
            tdc_isd_map_specific_stim_flow01_reset_pulse();
            break;
        }

        case 3:  // (2025.12.17 바이폴라 기능)
        {
            tdc_isd_map_specific_stim_flow03_build_reference();
            break;
        }

        case 5:  // (2025.12.17 바이폴라 기능)
        {
            // 1~24번 자극 전극에 대응하는 기준 전극 번호
            tdc_isd_map_specific_stim_send_reference_range(0, df_MaxNumTransferableChannel);
            break;
        }

        case 7:  // (2025.12.17 바이폴라 기능)
        {
            // 24~32번 자극 전극에 대응하는 기준 전극 번호.
            // 시작값이 df_MaxNumTransferableChannel 인 것은 flow 5 가 0 ~ (그 값 - 1) 을 이미 채웠기 때문이다.
            // 즉 flow 5 와 이 구간이 이어 붙어 df_MaxNumOfElectrode 전체를 빈틈·겹침 없이 덮는다.
            tdc_isd_map_specific_stim_send_reference_range(df_MaxNumTransferableChannel, df_MaxNumOfElectrode);
            break;
        }

        case 9:  // 자극 파라미터.
        {
            tdc_isd_map_specific_stim_flow09_send_stim_para();
            break;
        }

        case 14:
        {
            tdc_isd_map_specific_stim_flow14_verify_pulse_width();
            break;
        }

        case 20:
        {
            tdc_isd_map_specific_stim_flow20_log_summary();
            break;
        }

        default:
        {
            break;
        }
    }

    if (flowCounter > cofigureDoneCounter)  // 자극 설정이 완료되었으며 자극출력을 시작한다.
    {
        if (stimulationTime_msec >= stimulationHoldTime_msec)  // 자극 유지시간 도래.
        {
            tdc_isd_map_specific_stim_output_finish(pcm_index);
        }
        else
        {
            tdc_isd_map_specific_stim_output_fill(pcm_index);
        }

        stimulationTime_msec++;
    }

    flowCounter++;
}
