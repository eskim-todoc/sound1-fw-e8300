#include <stdbool.h>
#include <hw.h>

#include <tdc_shm.h>
#include <tdc_isd_stim_mode_encode.h>
#include <FPGA.h>
#include <tdc_isd_pcm.h>
#include <tdc_isd_fpga.h>
#include <internalStimulationChip.h>
#include <tdc_isd.h>
#include <tdc_hal_i2c_isd.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd_init_fpga.h>
#include <tdc_isd.h>
#include <tdc_stim_indicator.h>
#include <tdc_stim_para_cal.h>
#include <electrodeMapping.h>
#include <tdc_sys_error.h>

/*
  두 상태 기계가 각자 진행 상태를 갖는다. 이름이 같아 승격할 때 접두어로 갈랐다.
  합치면 모노폴라 설정 중에 바이폴라 카운터가 밟히는 식으로 서로를 침범한다.
*/

// 모노폴라 · 공통접지 경로
static const ST_STIUL_DAC_REGISTER_VALUE      *s_mono_p_stimulDAC_setting;
static const ST__CFX_CM3_SharedMemory_mapData *s_mono_p_mapdata;
static int                                     s_mono_flowControlCounter = 0;
static int                                     s_mono_tempCounter        = 0;
static int                                     s_mono_sent_stimulConfig  = 0;
static int                                     s_mono_writenBacktelRegisterValue;

// 바이폴라 경로
static const ST__CFX_CM3_SharedMemory_mapData *s_bi_p_mapdata;
static const ST_STIUL_DAC_REGISTER_VALUE      *s_bi_p_stimulDAC_setting;
static int                                     s_bi_flowControlCounter = 0;
static int                                     s_bi_tempCounter        = 0;
static int                                     s_bi_sent_stimulConfig  = 0;
static int                                     s_bi_backtelBuff[df_MaxNumOfElectrode];
static int                                     s_bi_bipolarReferenceElectrodeNum[df_MaxNumOfElectrode];

static bool stimulationParameterSettingDone = false;

void tdc_isd_clear_stim_para_setting_done(void)
{
    stimulationParameterSettingDone = false;
}

void done_setting_StimulPara_variable(void)
{
    stimulationParameterSettingDone = true;
}

bool tdc_isd_is_stim_para_setting_done(void)
{
    return stimulationParameterSettingDone;
}

// FPGA 에러 확인 후 펄스폭 최소 설정 · 백텔 8비트 모드
static bool tdc_isd_stim_para_mono_flow00_init_fpga_and_backtel(void)
{
    int  i;
    bool FPGA_error;
    int  pcm_index = 0;

    tdc_isd_fpga_check_fpga_pcm_error(&FPGA_error);

    if (FPGA_error)
    {
        // FPGA 에러 발생, FPGA 초기화
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
    }
    else
    {
        tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);

        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

        s_mono_writenBacktelRegisterValue = tdc_isd_fpga_change_8_bit_backtel_mode(pcm_index++);

        for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
        {
            tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
        }

        tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand
    }

    return false;
}

// 펄스폭·백텔 설정 검증 후 백텔 FIFO 클리어
static bool tdc_isd_stim_para_mono_flow04_verify_and_clear_fifo(void)
{
    int  r_FPGA_registerValue;
    bool FPGA_FIFO_empty;
    bool stimulationConfigError = false;

    if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
    {
        if (r_FPGA_registerValue == FPGA_pulsePhaseWidth_minimum)
        {
            // 펄스 폭 설정이 정상적으로 완료되었기 때문에 펄스폭 값을 업데이트한다.
            tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(r_FPGA_registerValue);
        }
        else
        {
            // 펄스폭 설정 실패, FPGA 초기화
            tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_PulseWidthDifferent, __LINE__);
            tdc_isd_change_state(en__isdStatus_PowerIC_OK);

            stimulationConfigError = true;
        }
    }

    if (tdc_isd_fpga_read_backtel_config(&r_FPGA_registerValue))
    {
        if (s_mono_writenBacktelRegisterValue == r_FPGA_registerValue)
        {
            // PCM 으로 설정한 FPGA 백텔 레지스터가 원하는 값으로 쓰여졌는지확인 후 업데이트
            tdc_isd_fpga_update_fpga_backtel_config_written_value(s_mono_writenBacktelRegisterValue);

            if (tdc_isd_fpga_write_clear_fifo())  // 백텔 FIFO 지우기
            {
                // FPGA 상태를 읽어 본다.
                if (tdc_isd_fpga_check_fpga_fifo_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
                {
                    if (!FPGA_FIFO_empty)
                    {
                        tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_FIFO_NotCleared, __LINE__);
                        tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //

                        stimulationConfigError = true;
                    }
                }
            }
        }
        else
        {
            // 백텔 레지스터 설정 오류
            tdc_sys_error_update(en__FPGA_CONFIGUARATION_ERROR, en_RegisterConfigError_byPCM, __LINE__);
            tdc_isd_change_state(en__isdStatus_PowerIC_OK);

            stimulationConfigError = true;
        }
    }

    return stimulationConfigError;
}

// 자극 파라미터 레지스터 쓰기
static bool tdc_isd_stim_para_mono_flow05_write_stim_para(void)
{
    int i;
    int w_isd_registerValue;
    int pcm_index = 0;

    s_mono_p_stimulDAC_setting = tdc_stim_read_dac_register_value();
    s_mono_p_mapdata           = tdc_shm_get_pointer_current_map_data();

    // ISD - DAC Offset "쓰기" ('h05: slope offset register)
    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
    w_isd_registerValue = w_isd_registerValue << 8;
    w_isd_registerValue = w_isd_registerValue | s_mono_p_stimulDAC_setting->DAC_offsetLevel_register;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    // ISD - 자극 파라미터 설정 "쓰기" ('h06: stimulation configuration register)
    w_isd_registerValue = ISD_registerAddr_StimulationConfig;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

    // 7번 비트 STIM_REF_HW_CTRL_DISABLE (리셋 값: 0b1)
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | 1;

    // 6번 비트 SLOPE_OFFSET_RESOULUTION
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | s_mono_p_stimulDAC_setting->DAC_offsetSlope_register;

    // 5~4번 비트 SLOPE_SELECT
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | s_mono_p_stimulDAC_setting->DAC_Slope_register;

    // 3~2번 비트 MP_CONFIG (모노폴라 출력 모드에서 기준전극)
    w_isd_registerValue = w_isd_registerValue << 2;

    // 리셋값이 3이며, 바이폴라, 공통접지, 동시 자극의 경우는 접지를 끊는다.
    w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_reference_bits(s_mono_p_mapdata->stimulationMode);

    // 1~0번 STIM_MODE (자극 출력 모드: 모노폴라, 바이폴라, 공통접지, 동시모사)
    w_isd_registerValue = w_isd_registerValue << 2;

    w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_output_bits(s_mono_p_mapdata->stimulationMode);

    // 만들어 놓은 패킷을 0xFF로 비트연산 해서 사용하는게 아니고, 나중에 비교하는데 사용한다.
    s_mono_sent_stimulConfig = w_isd_registerValue & 0xFF;
    w_isd_registerValue      = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    /*
      /=================================================================================/
      /      ** PCM protocol for ISD data packet **                                     /
      /=================================================================================/
      /                                                                                 /
      /      +-+ <============================= 3 bits : header                         /
      /      |-|                                                                        /
      /      |-|   +-----+ <=================== 7 bits : address                        /
      /      |-|   |-----|                                                              /
      /      |-|   |-----|   +------+ <======== 8 bits : data                           /
      /      |-|   |-----|   |------|                                                   /
      /      jih g fedcba9 8 76543210                                                   /
      /          |     |                                                                /
      /          |     + <===================== 1 bit  : r/w                            /
      /          |                                                                      /
      /          + <=========================== 1 bit  : packet mode                    /
      /                                                  (parameter/configuration)      /
      /                                                                                 /
      /=================================================================================/
      /                                                                                 /
      /       j: bit 19          i: bit 18          h: bit 18          g: bit 18        /
      /                                                                                 /
      /=================================================================================/
     */

#ifndef DisalbedBackTel
    // ISD - DAC Offset "읽기" ('h05: slope offset register)
    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
    w_isd_registerValue = w_isd_registerValue << 8;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

    // ISD - 자극 파라미터 설정 "읽기" ('h06: stimulation configuration register)
    w_isd_registerValue = ISD_registerAddr_StimulationConfig;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
    w_isd_registerValue = w_isd_registerValue << 8;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
    {
        tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopBacktel);
    }
#else
    for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
    {
        tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
    }
#endif

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// 자극 파라미터가 실제로 반영됐는지 확인
static bool tdc_isd_stim_para_mono_flow09_verify_stim_para(void)
{
    int  backtelBuff[64];
    bool isdSettingError;
    bool stimulationConfigError = false;

#ifndef DisalbedBackTel
    isdSettingError = false;

    if (tdc_isd_fpga_read_backtel_fifo(backtelBuff, 2))
    {
        // DAC offset 값 확인
        if (backtelBuff[0] != s_mono_p_stimulDAC_setting->DAC_offsetLevel_register)
        {
            isdSettingError = true;
        }

        // 자극 설정값 확인
        if (backtelBuff[1] != s_mono_sent_stimulConfig)
        {
            isdSettingError = true;
        }
    }

    // 오프셋 값과 자극 파라미터 설정에 오류가 없으면 정상
    if (isdSettingError)
    {
        tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
        stimulationConfigError = true;
    }
#endif

    return stimulationConfigError;
}

// 펄스폭을 맵 데이터 값으로 설정
static bool tdc_isd_stim_para_mono_flow10_set_pulse_width(void)
{
    int i;
    int pcm_index = 0;

    // 펄스 폭을 맵 데이터에 해당하는 값으로 다시 설정
    tdc_isd_fpga_change_pulse_width(pcm_index++, s_mono_p_mapdata->stimulationPulsePhaseWidth);

    tdc_isd_fpga_disable_backtel(pcm_index++);

    for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
    {
        tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// 펄스폭 설정 검증 및 백텔 FIFO 클리어
static bool tdc_isd_stim_para_mono_flow14_verify_pulse_width(void)
{
    int  r_FPGA_registerValue;
    bool stimulationConfigError = false;

    if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
    {
        if (r_FPGA_registerValue == s_mono_p_mapdata->stimulationPulsePhaseWidth)
        {
            tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(s_mono_p_mapdata->stimulationPulsePhaseWidth);  //
            // 자극 파라미터 설정 완료
            if (tdc_isd_fpga_write_clear_fifo())
            {
                TDC_PRINTF_V("[STIMULATION] DONE, STIMULATION PARAMETER SETTING \r\n");
                done_setting_StimulPara_variable();
            }
            else
            {
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            }
        }
        else
        {
            if (tdc_isd_fpga_read_system_error_flag(&r_FPGA_registerValue))
            {
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            }

            tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            stimulationConfigError = true;
        }
    }

    return stimulationConfigError;
}

// 펄스폭 최소 설정 · 백텔 8비트 모드
static bool tdc_isd_stim_para_bi_flow00_init_fpga_and_backtel(void)
{
    int pcm_index = 0;

    tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);  // 펄스 폭 0으로 설정
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(FPGA_pulsePhaseWidth_minimum);

    tdc_isd_fpga_change_8_bit_backtel_mode(pcm_index++);  // 백텔 레지스터 8 비트 설정 (백텔 하드웨어 회로 ON 포함)

    tdc_isd_fill_pcm_last_stimulation_out(&pcm_index);  // 이전 자극 파라미터를 출력하기 위한 임의의 값 출력

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// 바이폴라 기준전극 버퍼 구성 · 백텔 FIFO 클리어
static bool tdc_isd_stim_para_bi_flow05_build_reference(void)
{
    int  i;
    bool FPGA_FIFO_empty;
    bool stimulationConfigError = false;

    s_bi_p_stimulDAC_setting = tdc_stim_read_dac_register_value();
    s_bi_p_mapdata           = tdc_shm_get_pointer_current_map_data();

    // 바이폴라 기준 전극 버퍼 구성
    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        // 기본 값으로 바이폴라 레퍼런스 전극 번호를 31로 일괄 초기화한다.
        s_bi_bipolarReferenceElectrodeNum[i] = unusedReferenceElectrode_DummyNum;
    }

    TDC_PRINTF_I("[PARA] TOTAL NUM FREQ BAND : %d \r\n", s_bi_p_mapdata->numFrequencyBand);

    /* 루프 상한 이중 가드 (2026-08-06 추가, 위험_4).
     * numFrequencyBand 는 맵 데이터에서 오고 배열은 df_MaxNumOfElectrode 칸이다.
     * 입력 검증이 언젠가 느슨해져도 배열을 넘지 않도록 상한을 함께 건다.
     * 근거: docs/참고/전극 번호/electrodeMap 99 인덱스 범위 이슈 (이식).md §5-7 */
    for (i = 0; (i < s_bi_p_mapdata->numFrequencyBand) && (i < df_MaxNumOfElectrode); i++)
    {
        /* electrodeMap[] 을 적용하지 않고 맵 인덱스를 그대로 쓰던 구버전 바이폴라
         * 기준전극 계산은 제거했다(#if 0 사장). 현재는 아래처럼 electrodeMap[] 으로
         * 논리 전극번호를 PCB 전극번호로 변환해 넣는다. */

        /* 인과는 2026-07-30(이월_D)에 양단이 모두 확정됐다. 남은 것은 재현 확인뿐이다.
         *   앞단 - 쓰레기값의 정체: electrodeMap[98] 은 배열 끝에서 +264B 벗어난
         *          s_tdc_table(ui/tdc_ui_command.c 의 command_entry_t[32]) 안이며,
         *          실기 로그 "REF ELEC NUM : 99 (PCB : 70378)" 의 70378 = 0x112EA 가
         *          .text 범위(0x10000~0x3d244) 안이라 그 포인터 값임이 수치로 맞는다.
         *   뒷단 - 그 값이 왜 백텔 카운트 0 을 부르는가: 아래 :578 주석 참조.
         *          비트 [4:0] 초과 값이 FIFO 클리어를 일으킨다.
         * 즉 "쓰레기값 -> 비트 초과 -> FIFO 클리어 -> 백텔 카운트 0" 사슬이 이어진다.
         * 재현 절차는 docs/참고/ble/재구조화/바이폴라 전극번호 경계 결함.md §5 참조. */

        /* 전극번호 범위 방어 (2026-07-27 추가).
         * 매핑 앱은 사용하지 않는 밴드의 전극번호를 99 로 채워 보낸다.
         * 이를 거르지 않고 electrodeMap[99 - 1] 을 읽으면 32원소 배열의 범위를
         * 벗어나 인접 전역(s_tdc_table)의 값을 집어온다. 실기 로그에서
         * "REF ELEC NUM : 99 (PCB : 70378)" 로 관측된 것이 이 미정의 동작이다.
         * 그 값이 기준전극으로 내부기 레지스터에 기록되어 바이폴라 설정이 깨졌다.
         *
         * 미사용 밴드는 바로 위 루프에서 이미 unusedReferenceElectrode_DummyNum(31)
         * 으로 초기화돼 있으므로, 여기서 건너뛰면 그 더미값이 그대로 유지된다.
         * 구버전(Sullivan1.5)의 #if 0 블록에 있던 99 체크와 같은 취지이며,
         * 범위 검사로 일반화해 0 이나 33 이상도 함께 막는다. */
        if ((s_bi_p_mapdata->usableStimulationElectrodIndex[i] < 1)                        //
            || (df_MaxNumOfElectrode < s_bi_p_mapdata->usableStimulationElectrodIndex[i])  //
            || (s_bi_p_mapdata->usableReferenceElectrodIndex[i] < 1)                       //
            || (df_MaxNumOfElectrode < s_bi_p_mapdata->usableReferenceElectrodIndex[i]))
        {
            TDC_PRINTF_I("[PARA] (1 BASE), BAND : %2d, STIM ELEC NUM : %2d (PCB : XX), REF ELEC NUM : %2d (PCB : XX), UNUSED \r\n",  //
                         i + 1,
                         s_bi_p_mapdata->usableStimulationElectrodIndex[i],
                         s_bi_p_mapdata->usableReferenceElectrodIndex[i]);
            continue;
        }

        s_bi_bipolarReferenceElectrodeNum[electrodeMap[s_bi_p_mapdata->usableStimulationElectrodIndex[i] - 1]] =  // 코드가 길어서 강제 줄 바꿈
            electrodeMap[s_bi_p_mapdata->usableReferenceElectrodIndex[i] - 1];

        TDC_PRINTF_I("[PARA] (1 BASE), BAND : %2d, STIM ELEC NUM : %2d (PCB : %2d), REF ELEC NUM : %2d (PCB : %2d) \r\n",  //
                     i + 1,
                     s_bi_p_mapdata->usableStimulationElectrodIndex[i],
                     electrodeMap[s_bi_p_mapdata->usableStimulationElectrodIndex[i] - 1] + 1,
                     s_bi_p_mapdata->usableReferenceElectrodIndex[i],
                     electrodeMap[s_bi_p_mapdata->usableReferenceElectrodIndex[i] - 1] + 1);
    }

    for (i = s_bi_p_mapdata->numFrequencyBand; i < df_MaxNumOfElectrode; i++)
    {
        TDC_PRINTF_I("[PARA] (1 BASE), BAND : %2d, STIM ELEC NUM : %2d (PCB : XX), REF ELEC NUM : %2d (PCB : XX) \r\n",  //
                     i + 1,
                     s_bi_p_mapdata->usableStimulationElectrodIndex[i],
                     s_bi_p_mapdata->usableReferenceElectrodIndex[i]);
    }

    if (tdc_isd_fpga_write_clear_fifo())  // 백텔 FIFO 클리어
    {
        if (tdc_isd_fpga_check_fpga_fifo_empty(&FPGA_FIFO_empty))  // 지워 졌는지 확인.
        {
            if (!FPGA_FIFO_empty)
            {
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //
                stimulationConfigError = true;
            }
        }
        else
        {
            tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //
            stimulationConfigError = true;
        }
    }
    else
    {
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);  //
        stimulationConfigError = true;
    }

    return stimulationConfigError;
}

// 내부기 칩의 바이폴라 기준전극 FIFO 클리어
static bool tdc_isd_stim_para_bi_flow06_clear_isd_fifo(void)
{
    int w_isd_registerValue;
    int pcm_index = 0;

    // 내부기 칩 레지스터에서 Bipolar 기준전극 FIFO 지우기
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

    return false;
}

// 바이폴라 기준전극 번호를 [first, last) 구간만큼 내부기로 전송한다.
// 두 단계로 나뉜 이유는 한 번에 보낼 수 있는 PCM 채널 수가 32보다 작기 때문이다.
static bool tdc_isd_stim_para_bi_send_reference_range(int first, int last)
{
    int i;
    int w_isd_registerValue;
    int pcm_index = 0;

    for (i = first; i < last; i++)  // 0~23번 자극채널에 대응하는 기준 전극 번호
    {
        w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
        w_isd_registerValue = w_isd_registerValue << 1;
        w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
        w_isd_registerValue = w_isd_registerValue << 8;

        // 바이폴러 레퍼런스 전극 번호는 비트 [4:0] 범위, 범위 초과한 값 입력시 FIFO 클리어 발생 → 이후 백텔 카운트 0 에러 발생할 수 있음
        w_isd_registerValue = w_isd_registerValue | (0x1F & s_bi_bipolarReferenceElectrodeNum[i]);
        w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

        tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
    }

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// 기준전극 FIFO 값을 count 개만큼 읽어 오는 PCM 을 채운다.
// 한 읽기가 PCM 4슬롯(읽기 1 + NopBacktel 3)을 쓰므로 한 번에 6개까지다.
// 6 x 5 + 2 = 32 로 여섯 단계가 이어 붙어 전 전극을 덮는다.
static bool tdc_isd_stim_para_bi_read_reference_burst(int count)
{
    int i;
    int w_isd_registerValue;
    int pcm_index = 0;

    w_isd_registerValue = ISD_registerAddr_en__bipolar_referenceElectroldIndex;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    for (i = 0; i < count; i++)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    }

    for (; pcm_index < df_MaxNumTransferableChannel;)
    {
        tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// I2C 로 기준전극 FIFO 값 읽어 검증
static bool tdc_isd_stim_para_bi_flow29_read_reference_i2c(void)
{
    int  i;
    int  r_FPGA_registerValue;
    int  nop                    = 0;
    bool stimulationConfigError = false;

    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        s_bi_backtelBuff[i] = 0;
    }

    // nop=(tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_FIFO_counter, &r_FPGA_registerValue, 1));
    if (tdc_isd_fpga_read_fifo_counter(&r_FPGA_registerValue))
    {
        if (r_FPGA_registerValue != 0)
        {
            TDC_PRINTF_I("[PARA] BIPOLAR REF READ BACKTEL COUNT : %d \r\n", r_FPGA_registerValue);

            // if (tdc_hal_i2c_cfx_read(df_I2C_ADDR_FPGA_BackTel, s_bi_backtelBuff, df_MaxNumOfElectrode))
            if (tdc_isd_fpga_read_backtel_fifo(s_bi_backtelBuff, r_FPGA_registerValue))
            {
                for (i = 0; i < df_MaxNumOfElectrode - 1; i++)
                {
                    TDC_PRINTF_I("[PARA] CHIP, (0 BASE), INDEX MAP (PCB) [%2d] : (PCB) %d \r\n",  //
                                 i,
                                 ((0x1F) & (s_bi_backtelBuff[i])));

                    // if (electrodeMap[s_bi_bipolarReferenceElectrodeNum[i]] != ((0x1F) & (s_bi_backtelBuff[i])))
                    if (s_bi_bipolarReferenceElectrodeNum[i] != ((0x1F) & (s_bi_backtelBuff[i])))
                    {
                        TDC_PRINTF_E("[PARA] PRE-SETTING BIPOLAR REF INDEX : %d, READ REF INDEX : %d \r\n",  //
                                     s_bi_bipolarReferenceElectrodeNum[i],
                                     (0x1F) & (s_bi_backtelBuff[i]));
                        tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                        stimulationConfigError = true;
                    }
                }

                TDC_PRINTF_I("[PARA] CHIP, (0 BASE), INDEX MAP (PCB) [%2d] : (PCB) %d \r\n",  //
                             df_MaxNumOfElectrode - 1,
                             ((0x1F) & (s_bi_backtelBuff[df_MaxNumOfElectrode - 1])));
            }
        }
        else
        {
            // 백텔이 안들어 왔다.
            TDC_PRINTF_W("[PARA] BIPOLAR REF READ BACKTEL COUNT IS 0 \r\n");
            tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
            stimulationConfigError = true;
        }
    }
    else
    {
        TDC_PRINTF_E("[PARA] I2C FAILED TO READ FIFO COUNT FOR READING BIPOLAR REF \r\n");
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);  // FPGA 리셋
        stimulationConfigError = true;
    }

    return stimulationConfigError;
}

// 자극 파라미터 레지스터 쓰기
static bool tdc_isd_stim_para_bi_flow30_write_stim_para(void)
{
    int i;
    int w_isd_registerValue;
    int pcm_index = 0;

    // 펄스 폭 0으로 설정
    tdc_isd_fpga_change_pulse_width_minimum(pcm_index++);
    tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(FPGA_pulsePhaseWidth_minimum);  // 실제로는 Fpag 값을 읽어보고 업데이트 해야 된다.
    // 8bit backter 설정
    tdc_isd_fpga_change_8_bit_backtel_mode(pcm_index++);

    // ISD  - DAC Offset 쓰기
    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | s_bi_p_stimulDAC_setting->DAC_offsetLevel_register;
    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
    // for(i=0; i<mapData->frameNumPerChannel*2; i++)
    //   tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);
    // tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopStandby);

    // ISD  - 자극 파라미터 설정  쓰기
    w_isd_registerValue = ISD_registerAddr_StimulationConfig;

    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_writeRegister;

    // 1, 7번비트
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | 1;

    // offsetResolution 6번 비트
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | s_bi_p_stimulDAC_setting->DAC_offsetSlope_register;

    // DAC slope    4,5번 비트
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | s_bi_p_stimulDAC_setting->DAC_Slope_register;

    // 모노폴라 출력 모드에서 기준전극 모드 2,3번 비트
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_reference_bits(s_bi_p_mapdata->stimulationMode);

    // 자극 출력 모드 0,1번 비트 (모노폴라, 바이폴라, 공통접지, 동시모사)
    w_isd_registerValue = w_isd_registerValue << 2;
    w_isd_registerValue = w_isd_registerValue | tdc_isd_stim_mode_output_bits(s_bi_p_mapdata->stimulationMode);

    s_bi_sent_stimulConfig = w_isd_registerValue & 0xFF;
    w_isd_registerValue    = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

#ifndef DisalbedBackTel
    // ISD  - DAC Offset 읽기
    w_isd_registerValue = ISD_registerAddr_DAC_offsetValue;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);
    // for(i=0; i<mapData->frameNumPerChannel*2; i++)
    //   tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);
    tdc_shm_fill_specific_command_buffer(pcm_index++, pcm_Mold_NopBacktel);

    // ISD  - 자극 파라미터 설정  읽기
    w_isd_registerValue = ISD_registerAddr_StimulationConfig;
    w_isd_registerValue = w_isd_registerValue << 1;
    w_isd_registerValue = w_isd_registerValue | ISD_readRegister;
    w_isd_registerValue = w_isd_registerValue << 8;

    w_isd_registerValue = w_isd_registerValue | pcm_Mold_configuration;

    tdc_shm_fill_specific_command_buffer(pcm_index++, w_isd_registerValue);

    for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
    {
        tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopBacktel);
    }
#else

    for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
    {
        tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
    }

#endif

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// 자극 파라미터가 실제로 반영됐는지 확인
static bool tdc_isd_stim_para_bi_flow34_verify_stim_para(void)
{
    int  r_FPGA_registerValue;
    bool isdSettingError        = false;
    bool stimulationConfigError = false;

#ifndef DisalbedBackTel
    isdSettingError = false;

    if (tdc_isd_fpga_read_fifo_counter(&r_FPGA_registerValue))
    {
        if (r_FPGA_registerValue == 2)
        {
            if (tdc_isd_fpga_read_backtel_fifo(s_bi_backtelBuff, 2))
            {
                // DAC offset 값 확인
                if (s_bi_backtelBuff[0] != s_bi_p_stimulDAC_setting->DAC_offsetLevel_register)
                {
                    isdSettingError = true;

                    TDC_PRINTF_W("[PARA] SETTING ERROR, SETTING OFFSET DAC LEVEL : %d, READ OFFSET DAC LEVEL : %d \r\n",  //
                                 s_bi_p_stimulDAC_setting->DAC_offsetLevel_register,
                                 s_bi_backtelBuff[0]);
                }

                // 자극 설정값 확인
                if (s_bi_backtelBuff[1] != s_bi_sent_stimulConfig)
                {
                    isdSettingError = true;  //

                    TDC_PRINTF_W("[PARA] SETTING ERROR, SETTING STIMUL CONFIG : %d, READ STIMUL CONFIG : %d \r\n",  //
                                 s_bi_sent_stimulConfig,
                                 s_bi_backtelBuff[1]);
                }
            }

            // 오프셋 값과 자극 파라미터 설정에 오류가 없으면 정상
            if (isdSettingError)
            {
                tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
                stimulationConfigError = true;
            }
        }
        else
        {
            TDC_PRINTF_W("[PARA] OFFSET DAC LEVEL, STIM CONFIG READ BACKTEL COOUNT IS NOT 2, (COUNT: %d) \r\n", r_FPGA_registerValue);
            tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
            stimulationConfigError = true;
        }
    }
    else
    {
        TDC_PRINTF_W("[PARA] OFFSET DAC LEVEL, STIM CONFIG READ BACKTEL COOUNT IS 0 \r\n");
        tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
        stimulationConfigError = true;
    }
#endif

    return stimulationConfigError;
}

// 펄스폭을 맵 데이터 값으로 설정
static bool tdc_isd_stim_para_bi_flow35_set_pulse_width(void)
{
    int i;
    int pcm_index = 0;

    // 펄스 폭 .. 설정 PCM 출력으로  FPGA에 전달
    tdc_isd_fpga_change_pulse_width(pcm_index++, s_bi_p_mapdata->stimulationPulsePhaseWidth);

    for (i = pcm_index; i < df_MaxNumTransferableChannel; i++)
    {
        tdc_shm_fill_specific_command_buffer(i, pcm_Mold_NopStandby);
    }

    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 다음 출력 모드 : NopStandby
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_SepcificCommand);  // 현재 출력 모드 : SepcificCommand

    return false;
}

// 펄스폭 설정 검증
static bool tdc_isd_stim_para_bi_flow39_verify_pulse_width(void)
{
    int  r_FPGA_registerValue;
    bool stimulationConfigError = false;

    if (tdc_isd_fpga_read_pulse_width(&r_FPGA_registerValue))
    {
        if (r_FPGA_registerValue == s_bi_p_mapdata->stimulationPulsePhaseWidth)
        {
            tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(
                s_bi_p_mapdata->stimulationPulsePhaseWidth);  // 실제로는 Fpag 값을 읽어보고 업데이트 해야 된다.
            // 자극 파라미터 설정 완료
            // tdc_isd_update_link_connected();

            if (tdc_isd_fpga_write_clear_fifo())
            {
                TDC_PRINTF_V("[STIMULATION] DONE, STIMULATION PARAMETER SETTING \r\n");
                done_setting_StimulPara_variable();
            }
            else
            {
                TDC_PRINTF_E("[PARA] I2C FAILED TO CLEAR FPGA FIFO \r\n");
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            }
        }
        else
        {
            TDC_PRINTF_E("[PARA] FAILED TO SET FPGA PULSE WIDTH \r\n");

            if (tdc_isd_fpga_read_system_error_flag(&r_FPGA_registerValue))
            {
                tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            }

            tdc_isd_change_state(en__isdStatus_PowerIC_OK);
            stimulationConfigError = true;
        }
    }
    else
    {
        TDC_PRINTF_E("[PARA] I2C FAILED TO READ FPGA PULSE WIDTH \r\n");
        tdc_isd_change_state(en__isdStatus_FPGA_Ok);  // 내부기 전송 파워 설정 부터 다시.
    }

    return stimulationConfigError;
}

bool tdc_isd_set_stim_para_monopolar(bool isdControlStateChagedFlag)
{
    bool stimulationConfigError = false;

    s_mono_tempCounter++;

    if (isdControlStateChagedFlag)
    {
        s_mono_flowControlCounter = 0;
    }

    switch (s_mono_flowControlCounter)
    {
        case 0:
        {
            stimulationConfigError = tdc_isd_stim_para_mono_flow00_init_fpga_and_backtel();
            break;
        }

        case 4:
        {
            stimulationConfigError = tdc_isd_stim_para_mono_flow04_verify_and_clear_fifo();
            break;
        }

        case 5:  // 자극 파라미터 설정 - 쓰기
        {
            stimulationConfigError = tdc_isd_stim_para_mono_flow05_write_stim_para();
            break;
        }

        case 9:  // 자극 파라미터 설정 확인
        {
            stimulationConfigError = tdc_isd_stim_para_mono_flow09_verify_stim_para();
            break;
        }

        case 10:
        {
            stimulationConfigError = tdc_isd_stim_para_mono_flow10_set_pulse_width();
            break;
        }

        case 14:
        {
            stimulationConfigError = tdc_isd_stim_para_mono_flow14_verify_pulse_width();
            break;
        }

        default:
        {
            break;
        }
    }

    s_mono_flowControlCounter++;

    return stimulationConfigError;
}

bool tdc_isd_set_stim_para_bipolar(bool isdControlStateChagedFlag)
{
    bool stimulationConfigError = false;

    s_bi_tempCounter++;

    if (isdControlStateChagedFlag)
    {
        s_bi_flowControlCounter = 0;

        tdc_isd_clear_control_state_changed_flag();
    }

    switch (s_bi_flowControlCounter)
    {
        case 0:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow00_init_fpga_and_backtel();
            break;
        }

        case 5:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow05_build_reference();
            break;
        }

        case 6:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow06_clear_isd_fifo();
            break;
        }

        case 8:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_send_reference_range(0, df_MaxNumTransferableChannel);
            break;
        }

        case 10:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_send_reference_range(df_MaxNumTransferableChannel, df_MaxNumOfElectrode);
            break;
        }

        case 14:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_read_reference_burst(6);
            break;
        }

        case 16:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_read_reference_burst(6);
            break;
        }

        case 18:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_read_reference_burst(6);
            break;
        }

        case 20:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_read_reference_burst(6);
            break;
        }

        case 22:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_read_reference_burst(6);
            break;
        }

        case 24:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_read_reference_burst(2);
            break;
        }

        case 29:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow29_read_reference_i2c();
            break;
        }

        case 30:  // 자극 파라미터 설정 - 쓰기
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow30_write_stim_para();
            break;
        }

        case 34:  // 자극 파라미터 설정 확인
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow34_verify_stim_para();
            break;
        }

        case 35:  // 펄스 폭 설정
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow35_set_pulse_width();
            break;
        }

        case 39:
        {
            stimulationConfigError = tdc_isd_stim_para_bi_flow39_verify_pulse_width();
            break;
        }

        default:
        {
            break;
        }
    }

    s_bi_flowControlCounter++;

    // 라이브 모드 시작 후 설정 과정에서 에러가 발생하면 다시 처음부터 시작해야 하는데
    // 실제 코드는 마지막 flowControlCounter에서 이어서 진행하는 버그가 있음
    // 그래서 에러 발생 시 카운터를 직접 0으로 초기화 하도록 수정하였음 by 김은수 2026.03.05
    if (stimulationConfigError)
    {
        s_bi_flowControlCounter = 0;
    }

    return stimulationConfigError;
}

bool tdc_isd_stim_setting_step(bool startTrigger)
{
    ST__CFX_CM3_SharedMemory_mapData *p_mapdata;
    bool                              error;

    error     = false;
    p_mapdata = tdc_shm_get_pointer_current_map_data();

    if (startTrigger)
    {
        TDC_PRINTF_V("[STIMULATION] TRIGGER, STIMULATION PARAMETER SETTING \r\n");
    }

    switch (p_mapdata->stimulationMode)
    {
        case en__monopolr_body:
        case en__monopolr_rod:
        case en__monopolr_BothRodBody:
        {
            // 내부기 칩 레지스터 0x05, 0x06을 설정하여
            // 오프셋 DAC 값과 오프셋 DAC의 기울기, 슬로프 DAC의 기울기, 자극 모드 및 레퍼런스 전극 설정을 진행한다.
            //TDC_PRINTF_I("[SETTING] START MONOPOLAR MODE SETTING \r\n");
            error = tdc_isd_set_stim_para_monopolar(startTrigger);
        }
        break;

        case en__bipolar:
        {
            //TDC_PRINTF_I("[SETTING] START BIPOLAR MODE SETTING \r\n");
            error = tdc_isd_set_stim_para_bipolar(startTrigger);
        }
        break;

        case en__commonground:
        {
            //TDC_PRINTF_I("[SETTING] START CG MODE SETTING \r\n");
            error = tdc_isd_set_stim_para_monopolar(startTrigger);
        }
        break;

        default:
            break;
    }

    return error;
}
