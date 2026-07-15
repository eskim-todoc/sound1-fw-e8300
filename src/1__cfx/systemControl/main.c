/**
 * @file main.c
 */
#include <agc.h>
#include <driver_PCM_liveStimulation.h>
#include <main.h>

volatile int _XMEM g_break_point = 0;
volatile int _XMEM g_pcm_mode; /* default : PcmBitStream_Mode_FillZero */

volatile int _XMEM g_standby_checker = 0;  // 절전모드에 들어갔는지 표시하기 위한 플래그

int main(void)
{
    /* The breakPoint variable below must be placed at the first line of the
     * main function code to debug without break point error. */
    g_break_point = 1;

    /* Set the system priorities */
    D_SYSTEM->CTRL = SYSTEM_PRIORITY_VAL;

    /* Configure the memory arbitrator */
    SYSCTRL->MEM_ARBITER_CFG = SYSTEM_MEM_ARBITER;

    /* Initialize the loop stack.
     * This clears the stack of register triplets that describe the
     * currently-executing loops. If data has been somehow left in the loop
     * stack during a previous execution of the code, problems may arise when
     * attempting to execute new loops. Refer to the documentation for more
     * information regarding hardware loops and the loop stack. */
    SYS_CFX_INIT_LOOP_STACK();

    // CFX 상태 레지스터의 s 즉, Saturation 비트를 saturation 모드로 설정
    // 0: Wrapping, 1: Saturation
    set_saturation_mode(1);

    app_configure_interrupts(CFX_INT_NORMAL);

    g_standby_checker = 0;

    while (1)
        chess_keep_sw_loop
        {
            // CM3는 is_CFX_started가 1이면 CFX가 동작 중인 상태로 판단한다.
            // CFX의 동작 상태를 확인한 후 CM3의 초기화 과정이 시작된다.
            // CM3의 초기화 과정이 끝난 뒤 CFX의 iteration을 1로 설정하게 되면, CFX가 normal 모드로 진입할 수 있게 된다.
            Addr_SharedMem->is_CFX_started = 1;

            if (Addr_SharedMem->is_enabled_CFX_iteration == 1)
            {
                normal();
                standby();
            }

            SYS_CFX_NOP();  // 최적화 방지 및 메모리 접근 경쟁 상태 방지 목적
        }

    return 0;
}

void normal(void)
{
    normal_init();
    normal_loop();
}

void normal_init(void)
{
    /* Disable and reset the HEAR */
    SYS_HEAR_PAUSE(D_SYSTEM);
    SYS_HEAR_RESET(D_SYSTEM);

    SYS_CFX_NOP();
    SYS_CFX_NOP();

    app_configure_interrupts(CFX_INT_NORMAL);

    /* Enable the HEAR function chain, and set the HEAR function chain priority */
    SYS_HEAR_FUNCTIONCHAIN_ENABLE(D_HEAR_FC, HCT_HEAR_FC_ENABLE_VAL);
    SYS_HEAR_FUNCTIONCHAIN_PRIORITY(D_HEAR_FC, HCT_HEAR_FC_PRIORITY_VAL);

    /* Start the HEAR */
    SYS_HEAR_START(D_SYSTEM);

    // 오디오 IN 0, ADC 0, DMIC 0(ch0)   :   FIFO_A0_0   :   DMIC_CLK1/CAL (DIO22) + DMIC_OUT1 (DIO23)   :   EZ
    // 오디오 IN 2, ADC 2, DMIC 2(ch2)   :   FIFO_A0_1   :   DMIC_CLK2     (DIO10) + DMIC_OUT2 (DIO17)   :   QCC
    tdc_configure_audio_path_all();

    lib_init_PCM(DIO_PCM_CLK, DIO_PCM_FRAME, DIO_PCM_MOSI);                // PCM 설정
    lib_init_I2S(DIO_I2S_CLK, DIO_I2S_FRAME, DIO_I2S_MISO, DIO_I2S_FLAG);  // I2S 설정

    // 순서 1. 마이크 입력 켜기 (빔포밍: 2-DMIC로 시작)
    tdc_enable_2_DMICs();

    // 순서 2. I2S 입력 켜기
    lib_enable_I2S();

    // 순서 3. PCM 출력 켜기
    lib_enable_PCM();

    // 순서 4. DAC 출력 켜기
    SYS_SET_DAC_GF_CTRL(AUDIO, OUTPUT_GAIN_VAL);  // DAC 출력을 약 2배로 게인 증가 시킴 (1.9990234375)
    SYS_SET_OUTPUT_CTRL(AUDIO, OUTPUT_CTRL_VAL);  // OD_1 활성화 (OD_0 NC 상태)

    fn_reset_PCM();  // 프리앰블 시작은 어보트부터 하도록 초기화

    // 공유 메모리의 PCM SpecificCommand 읽기 상태에 대한 플래그를 초기화 한다.
    // 이 플래그는 CM3가 CFX와 SpecificCommand 경쟁 상태를 만드는걸 방지하기 위함이다.
    Addr_SharedMem->is_pcm_specific_command_reading = 0;
    Addr_SharedMem->pcm_specific_command_read_index = 0;
}

inline void fifo_fill_zero(int *p_fifo)
{
    int *restrict px = p_fifo;
    int *restrict py = p_fifo + 8;

    for (register int i = 0; i < 8; i++)
        chess_loop_range(8, 8) chess_unroll_loop(*)
        {
            px[i] = 0;
            py[i] = 0;
        }
}

void normal_loop(void)
{
    while (Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX != 1)
    {
#if 1
        // I2S 플래그 핀이 액티브 상태여야 진짜 입력이다.
        if (Sys_GPIO_Read(I2S_FLAG_DIO_NUM) == I2S_FLAG_ACTIVE_LEVEL)
        {
            // I2S 입력 인터럽트가 발생했다면
            if (lib_g_i2s_interrupt_flag == 1)
            {
                int *restrict p_dst;
                int *restrict p_src;

                // 인터럽트 플래그 초기화
                lib_g_i2s_interrupt_flag = 0;

                // 입력 데이터 복사해야함
                p_dst = (int *) &lib_g_i2s_buffers[lib_g_i2s_buffer_in_pos][0];
                p_src = (int *) (int _XMEM *) &HCT_A0_5[0];

                for (register int i = 0; i < 16; i++)
                    chess_loop_range(16, 16) chess_unroll_loop(*)
                    {
                        p_dst[i] = p_src[i];
                    }

                // 입력 버퍼의 포인터 변경
                if (lib_g_i2s_buffer_in_pos == 0)
                {
                    lib_g_i2s_buffer_in_pos = 1;
                }
                else
                {
                    lib_g_i2s_buffer_in_pos = 0;
                }

                tdc_i2s_set_streaming_state(LIB_I2S_STATE_ENABLED);
            }

            /** I2S 스트리밍 상태인데 DMIC가 2개 사용 중인 상태 **/
            if (I2S_isStreaming() && (tdc_get_enabled_DMIC_count() == LIB_AUDIO_DMIC_COUNT_DUAL))
            {
                tdc_enable_1_DMIC();
            }
        }
        else
        {
            // 인터럽트 플래그 초기화
            lib_g_i2s_interrupt_flag = 0;

            // I2S 스트리밍 상태를 활성화로 변경
            tdc_i2s_set_streaming_state(LIB_I2S_STATE_DISABLED);

            // 입력/출력 버퍼 인덱스 초기화
            lib_g_i2s_buffer_in_pos  = 0;
            lib_g_i2s_buffer_out_pos = 0;

            /** I2S 스트리밍 상태가 아닌데 DMIC가 1개 사용 중인 상태 **/
            if ((!I2S_isStreaming()) && (tdc_get_enabled_DMIC_count() == LIB_AUDIO_DMIC_COUNT_SINGLE))
            {
                tdc_enable_2_DMICs();
            }
        }
#endif

        // I2S 입력이 언제부터 들어올지 알 수 없다. 그러므로 PCM 출력과 EZ 마이크만 사용하도록 한다.
        // mic0 = DMIC1 = EZ, mic1 = DMIC2 = QCC
        if ((g_interrupt_flags.pcm_out == 1) && (g_interrupt_flags.mic0 == 1) && (g_interrupt_flags.mic1 == 1) && (g_interrupt_flags.dac1 == 1))
        {
            g_interrupt_flags.pcm_out = 0;
            g_interrupt_flags.mic0    = 0;  // e8300
            g_interrupt_flags.mic1    = 0;  // qcc
            g_interrupt_flags.dac1    = 0;  // dac1

            /* CM3가 설정한 PCM 모드와 이어피스 상태 업데이트 */
            g_pcm_mode = Addr_SharedMem->cfx_PCM_interface.PCM_mode;

            /* copy DMIC buffers */
            tdc_copy_DMIC_buffers();

            /* 설정, 내부기 연결/해제 감지, 신호처리 계수 계산 등을 수행 */
            LB_Normal_PowerMode();

            /* 신호처리 시작 */

            /* PCM LiveStimulation 모드일 때 */
            if (g_pcm_mode == PcmBitStream_Mode_LiveStimulation)
            {
                PCM_LiveStimulation_Mode();
            }
            else /* PCM LiveStimulation 모드 아닐 때 */
            {
                if (Addr_SharedMem->chargerState.chargerConnectorPluggedIn != df_Connected)
                {
                    pcmDataOut();  // PCM
                }
            }
        }
        // End, PCM Handler (PCM, FIFO_A0_4)
        // HEAR Function Chain 1 완료 이벤트 (AGC 전처리, '절대값 및 최대값 구하기' 완료된 시점)
        else if (g_interrupt_flags.function_chain0 == 1)
        {
            HEAR_LiveStimulation_Mode();
        }

        // Sleep until an interrupt is received.
        // The interrupt controller will wake us up if any enabled interrupt is
        // pending, even though the master interrupt enable is disabled at this
        // point (see the Hardware Reference Manual).
        SYS_WAIT_FOR_INTERRUPT;

    }  // End, while()
}

void standby(void)
{
    lib_disable_PCM();  // PCM
    g_pcm_mode = PCM_PREAMBLE_STATE_FILL_ZERO;

    /* Disable the output drivers. */
    SYS_SET_OUTPUT_CTRL(AUDIO, OUTPUT_CTRL_DISABLE_VAL);

    lib_i2s_disable();  // I2S

    /* Disable the ADCs without calibrating them */
    SYS_SET_ADC_CTRL(AUDIO, 0, ADC_CTRL_0_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 1, ADC_CTRL_1_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 2, ADC_CTRL_2_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 3, ADC_CTRL_3_DISABLE_VAL);

    /* Disable DMIC */
    disable_DMIC();

    /* Disable the IOC ADC input. */
    SYS_IOC_INPUTCONFIG(D_IOC, IOC_ADC_CFG_DISABLE_VAL);

    /* Disable the IOC PCM input. */
    SYS_IOC_PCMINPUTCONFIG(D_IOC, IOC_PCM_CFG_DISABLE_VAL);

    /* Disable the IOC output. */
    SYS_IOC_OUTPUTCONFIG(D_IOC, IOC_OUTPUT_CFG_DISABLE_VAL);

    /* Configure how the IOC1 handles I2S data */
    SYS_IOC_INPUTCONFIG(LIB_I2S_IOC, I2S_IOC_INPUT_CFG_NONE);
    SYS_IOC_OUTPUTCONFIG(LIB_I2S_IOC, I2S_IOC_OUTPUT_CFG_NONE);
    SYS_IOC_PCMINPUTCONFIG(LIB_I2S_IOC, I2S_IOC_PCM_CFG_NONE);

    /* Disable all FIFOs. */
    disable_FIFO_all();

    /* Disable the HEAR. */
    SYS_HEAR_PAUSE(D_SYSTEM);

    /* Reset PCM DIOs. */
    Sys_DIO_Config(DIO_PCM_CLK, OTE1_5GEN_PCM_DIO_RESET);
    // Sys_DIO_Config(PCM_MISO, OTE1_5GEN_PCM_DIO_RESET);
    Sys_DIO_Config(DIO_PCM_MOSI, OTE1_5GEN_PCM_DIO_RESET);
    Sys_DIO_Config(DIO_PCM_FRAME, OTE1_5GEN_PCM_DIO_RESET);

#if 1
    /* Reset I2S DIOs */
    Sys_DIO_Config(DIO_I2S_CLK, OTE1_5GEN_PCM_DIO_RESET);
    Sys_DIO_Config(DIO_I2S_MISO, OTE1_5GEN_PCM_DIO_RESET);
    // Sys_DIO_Config(I2S_MOSI, OTE1_5GEN_PCM_DIO_RESET);
    Sys_DIO_Config(DIO_I2S_FRAME, OTE1_5GEN_PCM_DIO_RESET);
#endif

    DIO->SRC_PCM[0] = (PCM_SERI_SRC_CONST_HIGH | PCM_FRAME_SRC_CONST_HIGH | PCM_CLK_SRC_CONST_HIGH);
    DIO->SRC_PCM[1] = (PCM_SERI_SRC_CONST_HIGH | PCM_FRAME_SRC_CONST_HIGH | PCM_CLK_SRC_CONST_HIGH);

    /* interrupt settings. */
    app_configure_interrupts(CFX_INT_STANDBY);

    /* Clear 'enter ULP mode Command'. */
    Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX = 0;

    g_standby_checker = 1;

    while ((volatile int) 1)
    {
        if (g_interrupt_flags.wake_up == 1)
        {
            break;
        }

        SYS_CFX_INT_CLEARALLPENDING(D_INT);
        SYS_WAIT_FOR_INTERRUPT;
    }
}

/**
 * tdc_copy_DMIC_buffers - DMIC FIFO를 입력 링버퍼로 shift-복사.
 *
 * 매 인터럽트(블록 16샘플)마다 두 마이크에 대해:
 *   1) 기존 블록을 한 칸 뒤로 민다: block[0](직전 최신) → block[1](직전).
 *   2) 새 FIFO 데이터를 block[0](현재 최신)에 채운다.
 * 결과적으로 block[0]=최신, block[1]=직전 프레임이 유지된다(빔포밍 블록 경계용).
 * (BUF_MAX_CNT=2이면 내부 j 루프는 1회: block[1] = block[0].)
 */
/* [MODULE] M4 DMIC 버퍼 수집 / [UNIT] U9 DMIC 링버퍼 시프트.
 *   검증=integration-test / 전제(의존)=M3 HW 오디오 경로.   상세: 유닛-모듈-테스트맵.md */
void tdc_copy_DMIC_buffers(void)
{
    // DMIC1 (EZ, FIFO MIC0 = HCT_A0_0)
    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            for (register int j = 0; j < LIB_AUDIO_IN_BUF_MAX_CNT - 1; j++)
                chess_loop_range(LIB_AUDIO_IN_BUF_MAX_CNT - 1, LIB_AUDIO_IN_BUF_MAX_CNT - 1)
                {
                    g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][(LIB_AUDIO_IN_BUF_MAX_CNT - 1) - j][i] = g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][(LIB_AUDIO_IN_BUF_MAX_CNT - 2) - j][i];
                }

            g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][0][i] = ((int _XMEM *) &HCT_A0_0)[i];
        }

    // DMIC2 (QCC, FIFO MIC1 = HCT_A0_1)
    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            for (register int j = 0; j < LIB_AUDIO_IN_BUF_MAX_CNT - 1; j++)
                chess_loop_range(LIB_AUDIO_IN_BUF_MAX_CNT - 1, LIB_AUDIO_IN_BUF_MAX_CNT - 1)
                {
                    g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC2][(LIB_AUDIO_IN_BUF_MAX_CNT - 1) - j][i] = g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC2][(LIB_AUDIO_IN_BUF_MAX_CNT - 2) - j][i];
                }

            g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC2][0][i] = ((int _XMEM *) &HCT_A0_1)[i];
        }
}

/* [DEAD] 미사용 - 호출부 없음(아래 PCM_LiveStimulation_Mode 내 주석). I2S 처리는
 *        PCM_LiveStimulation_Mode 인라인으로 수행. 물리 삭제는 후속 일괄 - git 이력 보존. */
void I2S_handle(void)
{
    int _XMEM *p_buffer;

    lib_i2s_copy_data_from_fifo((int _XMEM *) &HCT_A0_5[0]);

    if (lib_g_i2s_buffer_state == I2S_BUFFER_STATE_READY)
    {
        // I2S 처리 시점 디버깅을 위해 삽입했던 것으로 보임
        // Sys_GPIO_Set_Low(DIO11);  // V0.3에서 Net Name: DMIC_CLK_QCC/CAL

        Addr_SharedMem->currentOutputStimulLevel_255[30] = lib_g_i2s_buffer_sub_state;

        p_buffer = I2S_get_buffer_with_fade_in_process();

        for (register int i = 0; i < LIB_I2S_DATA_BUF_LEN; i++)
            chess_loop_range(LIB_I2S_DATA_BUF_LEN, LIB_I2S_DATA_BUF_LEN)
            {
                Addr_SharedMem->currentOutputStimulLevel_255[i] = p_buffer[15 - i];
            }
        Addr_SharedMem->currentOutputStimulLevel_255[31] = 1;

        tdc_audio_mix_2_buffers((int _XMEM *) &HCT_A0_0[0], p_buffer);
        lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], p_buffer);  // DAC1로 I2S 출력
        // lib_audio_loopback((int _XMEM *) &HCT_A0_2[0], p_buffer);  // DAC0로 I2S 출력

        lib_g_i2s_buffer_copy_cnt--;

        if (lib_g_i2s_buffer_copy_cnt == I2S_BUFFER_UNDERRUN_CNT)
        {
            lib_g_i2s_buffer_state     = I2S_BUFFER_STATE_UNDERRUN;
            lib_g_i2s_buffer_sub_state = I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0;

            // I2S 처리 시점 디버깅을 위해 삽입했던 것으로 보임
            // Sys_GPIO_Set_High(DIO11); // V0.3에서 Net Name: DMIC_CLK_QCC/CAL
        }
    }
    else if (lib_g_i2s_buffer_state == I2S_BUFFER_STATE_UNDERRUN)
    {
        Addr_SharedMem->currentOutputStimulLevel_255[30] = lib_g_i2s_buffer_sub_state;

        p_buffer = I2S_get_buffer_with_fade_out_process();

        for (register int i = 0; i < LIB_I2S_DATA_BUF_LEN; i++)
            chess_loop_range(LIB_I2S_DATA_BUF_LEN, LIB_I2S_DATA_BUF_LEN)
            {
                Addr_SharedMem->currentOutputStimulLevel_255[i] = p_buffer[15 - i];
            }
        Addr_SharedMem->currentOutputStimulLevel_255[31] = 1;

        tdc_audio_mix_2_buffers((int _XMEM *) &HCT_A0_0[0], p_buffer);
        lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], p_buffer);  // DAC1로 I2S 출력

        if (lib_g_i2s_buffer_sub_state != (I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0 + I2S_BUFFER_UNDERRUN_CNT))
        {
            lib_g_i2s_buffer_copy_cnt--;
        }
    }

    lib_i2s_copy_data_from_fifo((int _XMEM *) &HCT_A0_5[0]);
}

/* ============================================================================
 * [MODULE] M6 신호 디스패치 - 상태(I2S/DMIC수/front mic)에 따라 믹싱 경로 선택.
 *   I2S 스트리밍: tdc_audio_mix_2_buffers(DMIC1 + I2S).
 *   비스트리밍 2-DMIC: tdc_audio_mix_2_buffers_for_beamforming(L/R로 front 채널 지연).
 *   비스트리밍 1-DMIC/NONE: tdc_audio_mix_1_buffer(DMIC1).
 *   검증=integration-test / 전제(의존)=M1 믹싱·M2 라우팅·M4 DMIC수집·M5 I2S 통과.
 *   상세: 유닛-모듈-테스트맵.md
 * ========================================================================== */
void PCM_LiveStimulation_Mode(void)
{
    // I2S_update_state();  // I2S 스트리밍 체크

    if (I2S_isStreaming())  // I2S 스트리밍 상태면 I2S 처리
    {
        // I2S_handle();
        int *p_buffer = (int *) &lib_g_i2s_buffers[lib_g_i2s_buffer_out_pos][0];

        // I2S + 마이크 둘 다 믹싱 버퍼에 복사 (합친 후 나누기 2 하는 것 더이상 안함)
        // tdc_audio_mix_2_buffers((int _XMEM *) &HCT_A0_0[0], p_buffer);
        tdc_audio_mix_2_buffers((int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][0][0], p_buffer);  // DMIC1 현재블록 sample0

        // I2S만 믹싱 버퍼에 복사
        // tdc_audio_mix_1_buffer(p_buffer);

        // 출력 버퍼의 포인터 변경
        if (lib_g_i2s_buffer_out_pos == 0)
        {
            lib_g_i2s_buffer_out_pos = 1;
        }
        else
        {
            lib_g_i2s_buffer_out_pos = 0;
        }
    }
    else  // I2S 스트리밍 상태가 아니면 Mic만 처리
    {
        if (tdc_get_enabled_DMIC_count() == LIB_AUDIO_DMIC_COUNT_SINGLE)
        {
            tdc_audio_mix_1_buffer((int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][0][0]);  // DMIC1 현재블록 sample0
        }
        else if (tdc_get_enabled_DMIC_count() == LIB_AUDIO_DMIC_COUNT_DUAL)
        {
            // 빔포밍 인수 = (지연 mic 현재블록, 지연 mic 직전블록, 기준 mic 현재블록).
            //   블록 인덱스: [BUF_MAX_CNT-2]=0(현재/최신), [BUF_MAX_CNT-1]=1(직전).
            //   front mic(음원에 먼저 닿는 쪽)을 지연 채널로 넘겨 forward 빔을 정렬한다.
            if (tdc_audio_get_front_mic() == LIB_AUDIO_FRONT_MIC_LEFT)
            {
                // Left 귀: front = DMIC2(QCC) → DMIC2 지연, DMIC1 기준
                tdc_audio_mix_2_buffers_for_beamforming((int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC2][LIB_AUDIO_IN_BUF_MAX_CNT - 2][0], (int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC2][LIB_AUDIO_IN_BUF_MAX_CNT - 1][0], (int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][0][0]);
            }
            else if (tdc_audio_get_front_mic() == LIB_AUDIO_FRONT_MIC_RIGHT)
            {
                // Right 귀: front = DMIC1(EZ) → DMIC1 지연, DMIC2 기준
                tdc_audio_mix_2_buffers_for_beamforming((int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][LIB_AUDIO_IN_BUF_MAX_CNT - 2][0], (int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][LIB_AUDIO_IN_BUF_MAX_CNT - 1][0], (int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC2][0][0]);
            }
            else
            {
                // front mic 미지정: 기본 DMIC1(EZ) 단일 사용
                tdc_audio_mix_1_buffer((int _XMEM *) &g_lib_dmic_in_buffers[LIB_DMIC_IDX_DMIC1][0][0]);  // DMIC1 현재블록 sample0
            }
            // lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], (int _XMEM *) &HCT_A0_0[0]);  // DAC1로 MIC1 출력
        }
        // lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], (int _XMEM *) &HCT_A0_0[0]);  // DAC1로 MIC0 출력
    }

    CALL_FUNCTION_CHAIN(HEAR_FC_agc_preprocessing);
}

void HEAR_LiveStimulation_Mode(void)
{
    if (g_interrupt_flags.function_chain0 == 1)
    {
        g_interrupt_flags.function_chain0 = 0;

        audio_agc();

        /* AGC output 데이터를 FFT 계산용 FIFO에 입력한다.
         * FFT 계산용 FIFO를 뒤에서부터 앞으로 복사하는 과정을 수행하지 않는다.
         * HEAR에서 계산해본 결과 입력 버퍼의 내용이 섞이거나 하지 않는다. */
        update_FFT_inputData();

        /* FFT와 vMag를 Function Chain 하나로 동작시키도록 만든 방법 HEAR 처리 시간 216 usec 소요 @ 2025.01.20 */
        CALL_FUNCTION_CHAIN(HEAR_FC_fft_vmag);

        /* FFT와 vMag 계산을 수행할 때 가장 오랜 시간이 소요되므로,
         * 이 계산이 끝나기 전까지 처리할 사항을 처리하면 된다.
         * 예를 들면, 오디오 디버깅을 위해 DAC FIFO로 AGC 출력 결과를 복사하는 등이 있다. */

        // (디버그 마더 보드의 실크 오타로 DAC0이라고 헤더에 적혀있다. 하지만, 실제로 DAC1을 사용함)
        lib_loopback_AGC_out((int _XMEM *) &HCT_A0_3[0], &m_agc_output_buffer[0]);  // DAC1으로 AGC 출력

        lib_i2s_clear_data();  // I2S 디버깅 출력까지 완료되면 버퍼 클리어

        do
        {
            if (g_interrupt_flags.function_chain1 == 1)
            {
                g_interrupt_flags.function_chain1 = 0;
                break;
            }
        } while ((volatile int) 1);

        /* pcmDataOut() -> PcmBitStream_Mode_LiveStimulation: 세부 설정으로 이동 */
#if 0
        find_freq_rep_value(); // 113usec @ 2025.01.10
        logarithmMapping(); // 76 usec 소요 @ 2025.01.20
#endif

        pcmDataOut(); // PCM
    }
}
