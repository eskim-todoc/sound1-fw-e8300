/**
 * @file main.c
 */
#include <agc.h>
#include <driver_PCM_liveStimulation.h>
#include <main.h>

volatile int _XMEM g_break_point = 0;
volatile int _XMEM g_pcm_mode; /* default : PcmBitStream_Mode_FillZero */
volatile int _XMEM g_earpiece_detection;
volatile int _XMEM g_earpiece_adc_enabled;

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

    app_configure_interrupts(CFX_INT_NORMAL);

    while (1)
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

    /* Enable the HEAR function chain, and set the HEAR function chain priority */
    SYS_HEAR_FUNCTIONCHAIN_ENABLE(D_HEAR_FC, HCT_HEAR_FC_ENABLE_VAL);
    SYS_HEAR_FUNCTIONCHAIN_PRIORITY(D_HEAR_FC, HCT_HEAR_FC_PRIORITY_VAL);

    /* Start the HEAR */
    SYS_HEAR_START(D_SYSTEM);

    /* Calculate SFCR based on the sampling frequency and ADCCLK,
     * and configure the sampling frequency register accordingly */
    SYS_SET_ADC_SAMPLE_FREQ_CFG(AUDIO, SFCR_16K);

    configure_audio_path_all();

    /* Configure and enable the PCM */
    lib_init_PCM(DIO_PCM_CLK, DIO_PCM_FRAME, DIO_PCM_SERO);

    /* 처음 초기화 때는 이어피스 연결 상태와 무관하게 DMIC 사용으로 진행 */
    g_earpiece_adc_enabled = 0;
    enable_DMIC();
    disable_AMIC();

    /* Configure the analog output stage */
    SYS_SET_DAC_GF_CTRL(AUDIO, OUTPUT_GAIN_VAL);  // 1.9990234375 gain for audio output
    SYS_SET_OUTPUT_CTRL(AUDIO, OUTPUT_CTRL_VAL);  // Enable OD1

    /* Start PCM output */
    lib_enable_PCM();

    // 공유 메모리의 PCM SpecificCommand 읽기 상태에 대한 플래그를 초기화 한다.
    // 이 플래그는 CM3가 CFX와 SpecificCommand 경쟁 상태를 만드는걸 방지하기 위함이다.
    Addr_SharedMem->is_pcm_specific_command_reading = 0;
    Addr_SharedMem->pcm_specific_command_read_index = 0;
}

void normal_loop(void)
{
    while (Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX != 1)
    {
        // if (g_interrupt_flags.pcm_out == 1 && (g_interrupt_flags.mic0 == 1 || g_interrupt_flags.mic1 == 1))
        if ((g_interrupt_flags.pcm_out == 1) && (g_interrupt_flags.mic0 == 1) && (g_interrupt_flags.mic1 == 1))
        {
            g_interrupt_flags.pcm_out = 0;
            g_interrupt_flags.mic0    = 0;
            g_interrupt_flags.mic1    = 0;

            /* CM3가 설정한 PCM 모드와 이어피스 상태 업데이트 */
            g_pcm_mode           = Addr_SharedMem->cfx_PCM_interface.PCM_mode;
            g_earpiece_detection = Addr_SharedMem->earpieceDetecion;

            /* 설정, 내부기 연결/해제 감지, 신호처리 계수 계산 등을 수행 */
            LB_Normal_PowerMode();

            /* 신호처리 시작 */

            /* PCM LiveStimulation 모드일 때 */
            if (g_pcm_mode == PcmBitStream_Mode_LiveStimulation)
            {
                if (g_earpiece_detection == EARPIECE_DETECT_ACTIVE_LEVEL)
                {
                    /* 이어피스 연결 중인데, 이어피스로 초기화 되어 있지 않다면 이어피스로 변경 */
                    if (g_earpiece_adc_enabled == 0)
                    {
                        reset_FFT_FIFO();
                        enable_AMIC();
                        g_earpiece_adc_enabled = 1;

                        /* attackRelease의 게인 값을 초기화 하여 부드럽게 소리가 들리게 유도한다. */
                        m_prev_agc_10db_gain_Q8_16 = 0;
                    }

                    audio_mix_external_mic_only();
                }
                else
                {
                    /* 이어피스 연결 중이 아닌데, 이어피스로 초기화 되어 있다면 디지털 마이크로 변경 */
                    if (g_earpiece_adc_enabled == 1)
                    {
                        reset_FFT_FIFO();
                        disable_AMIC();
                        g_earpiece_adc_enabled = 0;

                        /* attackRelease의 게인 값을 초기화 하여 부드럽게 소리가 들리게 유도한다. */
                        m_prev_agc_10db_gain_Q8_16 = 0;
                    }

                    audio_mix_internal_mic_only();
                }

                g_interrupt_flags.function_chain0 = 0; /* HEAR_FC_agc_preprocessing */
                CALL_FUNCTION_CHAIN(HEAR_FC_agc_preprocessing);
            }
            /* PCM LiveStimulation 모드 아닐 때 */
            else
            {
                fn_subLoop_common();
            }
        }
        /* End, PCM Handler (PCM, FIFO_A0_4) */
        /* HEAR Function Chain 1 완료 이벤트 (AGC 전처리:"절대값 및 최대값 구하기" 완료된 시점) */
        else if (g_interrupt_flags.function_chain0 == 1)
        {
            audio_agc();

            /* AGC output 데이터를 FFT 계산용 FIFO에 입력한다.
             * FFT 계산용 FIFO를 뒤에서부터 앞으로 복사하는 과정을 수행하지 않는다.
             * HEAR에서 계산해본 결과 입력 버퍼의 내용이 섞이거나 하지 않는다. */
            update_FFT_inputData();

            /* FFT와 vMag를 Function Chain 하나로 동작시키도록 만든 방법 HEAR 처리 시간 216 usec 소요 @ 2025.01.20 */
            g_interrupt_flags.function_chain1 = 0; /* HEAR_FC_fft_vmag */
            CALL_FUNCTION_CHAIN(HEAR_FC_fft_vmag);

            /* FFT와 vMag 계산을 수행할 때 가장 오랜 시간이 소요되므로,
             * 이 계산이 끝나기 전까지 처리할 사항을 처리하면 된다.
             * 예를 들면, 오디오 디버깅을 위해 DAC FIFO로 AGC 출력 결과를 복사하는 등이 있다. */

            // lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], (int _XMEM *) &HCT_A0_1[0]);  // 루프백: 이어피스
            // lib_audio_loopback((int _XMEM *) &HCT_A0_3[0], (int _XMEM *) &HCT_A0_0[0]);  // 루프백: 디지털 마이크
            lib_loopback_AGC_out((int _XMEM *) &HCT_A0_3[0], &m_agc_output_buffer[0]);  // AGC 결과 출력

#if 0
            /**
             * CM3의 Main 함수에서 디버깅 하기 위해 삽입한 코드이다.
             * 오디오 믹스와 AGC 결과를 출력해서 확인하기 위한 코드이다. */
            if (Addr_SharedMem->CM3_tempValue1 == 0)
            {
                for (int i = 0; i < 16; i++)
                {
                    Addr_SharedMem->currentOutputStimulLevel_255[i]      = ((int _XMEM*) HEAR_ADDR_AUDIO_MIX)[i];
                    Addr_SharedMem->currentOutputStimulLevel_255[16 + i] = m_agc_output_buffer[i];
                }

                Addr_SharedMem->CM3_tempValue1 = 1;
            }
#endif

            /* HEAR_FC_fft_vmag 처리 완료 대기 */
            do
            {
                if (g_interrupt_flags.function_chain1 == 1)
                {
                    break;
                }
            } while ((volatile int) 1);

            find_freq_rep_value();  // 113usec @ 2025.01.10

            logarithmMapping();  // 76 usec 소요 @ 2025.01.20

            fn_subLoop_common();  // 4.5 usec @ 2025.01.20

            g_interrupt_flags.function_chain0 = 0;
            g_interrupt_flags.function_chain1 = 0;
        }
        /* End, HEAR Function Chain 1 */

        // Sleep until an interrupt is received.
        // The interrupt controller will wake us up if any enabled interrupt is
        // pending, even though the master interrupt enable is disabled at this
        // point (see the Hardware Reference Manual).
        SYS_WAIT_FOR_INTERRUPT;

    }  // End, while()
}

void standby(void)
{
    /* Disable PCM. */
    PCM0->CTRL = PCM_DISABLE;

    /* 디지털 마이크 비활성화 */
    Sys_DIO_Config(DIO11, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));  // DMIC_CLK
    Sys_DIO_Config(DIO4, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));   // DMIC_OUT

    Sys_GPIO_Set_Low(DIO11);
    Sys_GPIO_Set_Low(DIO4);

    DIO->SRC_DMIC_CLK  = DMIC_CLK_SRC_CONST_HIGH;
    DIO->SRC_DMIC_DATA = (DMIC3_DATA_SRC_CONST_HIGH | DMIC2_DATA_SRC_CONST_HIGH | DMIC1_DATA_SRC_CONST_HIGH | DMIC0_DATA_SRC_CONST_HIGH);

    /* Disable the output drivers. */
    SYS_SET_OUTPUT_CTRL(AUDIO, OUTPUT_CTRL_DISABLE_VAL);

    /* Disable the ADCs without calibrating them */
    SYS_SET_ADC_CTRL(AUDIO, 0, ADC_CTRL_0_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 1, ADC_CTRL_1_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 2, ADC_CTRL_2_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 3, ADC_CTRL_3_DISABLE_VAL);

    /* Disable the IOC ADC input. */
    SYS_IOC_INPUTCONFIG(D_IOC, IOC_ADC_CFG_DISABLE_VAL);

    /* Disable the IOC PCM input. */
    SYS_IOC_PCMINPUTCONFIG(D_IOC, IOC_PCM_CFG_DISABLE_VAL);

    /* Disable the IOC output. */
    SYS_IOC_OUTPUTCONFIG(D_IOC, IOC_OUTPUT_CFG_DISABLE_VAL);

    /* Disable all FIFOs. */
    disable_FIFO_all();

    /* Disable the HEAR. */
    SYS_HEAR_PAUSE(D_SYSTEM);

    /* Reset PCM DIOs. */
    Sys_DIO_Config(DIO_PCM_CLK, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));
    Sys_DIO_Config(DIO_PCM_SERO, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));
    Sys_DIO_Config(DIO_PCM_FRAME, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));

    Sys_GPIO_Set_Low(DIO_PCM_CLK);
    Sys_GPIO_Set_Low(DIO_PCM_SERO);
    Sys_GPIO_Set_Low(DIO_PCM_FRAME);

    /* interrupt settings. */
    app_configure_interrupts(CFX_INT_STANDBY);

    /* Clear 'enter ULP mode Command'. */
    Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX = 0;

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

void fn_subLoop_common(void)
{
    // CM3에서 PCM 리셋 명령어 들어왔는지 확인
    if (Addr_SharedMem->cfx_PCM_interface.Reset_PCM == 1)
    {
        // call fn_reset_PCM
    }

    if (Addr_SharedMem->chargerState.chargerConnectorPluggedIn != df_Connected)
    {
        pcmDataOut(); // PCM
    }
}
