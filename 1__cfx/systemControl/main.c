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

    // 오디오 IN 0, ADC 0, DMIC 0   :   FIFO_A0_0   :   DMIC_CLK1/CAL (DIO22) + DMIC_OUT1 (DIO23)   :   EZ
    // 오디오 IN 1, ADC 1, DMIC 1   :   FIFO_A0_1   :   DMIC_CLK2     (DIO10) + DMIC_OUT2 (DIO17)   :   QCC
    configure_audio_path_all();

    lib_init_PCM(DIO_PCM_CLK, DIO_PCM_FRAME, DIO_PCM_MOSI);                // PCM 설정
    lib_init_I2S(DIO_I2S_CLK, DIO_I2S_FRAME, DIO_I2S_MISO, DIO_I2S_FLAG);  // I2S 설정

    // 순서 1. 마이크 입력 켜기
    enable_DMIC();

    // 순서 2. I2S 입력 켜기
    lib_enable_I2S();

    // 순서 3. PCM 출력 켜기
    lib_enable_PCM();

    // 순서 4. DAC 출력 켜기
    SYS_SET_DAC_GF_CTRL(AUDIO, OUTPUT_GAIN_VAL);  // DAC 출력을 약 2배로 게인 증가 시킴 (1.9990234375)
    SYS_SET_OUTPUT_CTRL(AUDIO, OUTPUT_CTRL_VAL);  // OD1 활성화 (OD2는 NC 상태)

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
        // I2S 입력이 언제부터 들어올지 알 수 없다. 그러므로 PCM 출력과 EZ 마이크만 사용하도록 한다.
        // mic0 = DMIC0 = EZ, mic1 = DIMC2 = QCC
        if ((g_interrupt_flags.pcm_out == 1) && (g_interrupt_flags.mic0 == 1))  // && (g_interrupt_flags.mic1 == 1))
        {
            g_interrupt_flags.pcm_out = 0;
            g_interrupt_flags.mic0    = 0;
            // g_interrupt_flags.mic1    = 0;

            /* CM3가 설정한 PCM 모드와 이어피스 상태 업데이트 */
            g_pcm_mode = Addr_SharedMem->cfx_PCM_interface.PCM_mode;

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

        audio_mix_2_buffers((int _XMEM *) &HCT_A0_0[0], p_buffer);
        lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], p_buffer);  // DAC1로 I2S 출력

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

        audio_mix_2_buffers((int _XMEM *) &HCT_A0_0[0], p_buffer);
        lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], p_buffer);  // DAC1로 I2S 출력

        if (lib_g_i2s_buffer_sub_state != (I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0 + I2S_BUFFER_UNDERRUN_CNT))
        {
            lib_g_i2s_buffer_copy_cnt--;
        }
    }

    lib_i2s_copy_data_from_fifo((int _XMEM *) &HCT_A0_5[0]);
}

void PCM_LiveStimulation_Mode(void)
{
    I2S_update_state();  // I2S 스트리밍 체크

    if (I2S_isStreaming())  // I2S 스트리밍 상태면 I2S 처리
    {
        I2S_handle();
    }
    else  // I2S 스트리밍 상태가 아니면 Mic만 처리
    {
        audio_mix_internal_mic_only();
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

        // lib_audio_loopback((int _XMEM *) &HCT_A0_2[0], (int _XMEM *) &HCT_A0_0[0]); // DAC0로 MIC0 복사
        // lib_audio_loopback((int _XMEM*) &HCT_A0_3[0], (int _XMEM*) &HCT_A0_1[0]);  // DAC1로 MIC1 복사
        lib_loopback_AGC_out((int _XMEM *) &HCT_A0_3[0], &m_agc_output_buffer[0]);  // DAC1로 AGC 출력
        // lib_audio_loopback((int _XMEM*) &HCT_A0_2[0], (int _XMEM*) &lib_g_i2s_buffer[0]);  // DAC0로 I2S 출력

        lib_i2s_clear_data();  // I2S 디버깅 출력까지 완료되면 버퍼 클리어

        do
        {
            if (g_interrupt_flags.function_chain1 == 1)
            {
                g_interrupt_flags.function_chain1 = 0;
                break;
            }
        } while ((volatile int) 1);

        find_freq_rep_value(); // 113usec @ 2025.01.10

        logarithmMapping(); // 76 usec 소요 @ 2025.01.20

        pcmDataOut(); // PCM
    }
}
