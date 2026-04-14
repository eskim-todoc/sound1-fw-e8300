/**
 * @file lib_audio_in.c
 */

#include <lib_audio_in.h>

static void lib_init_audio_in_path_common(void)
{
    /* Calculate SFCR based on the sampling frequency and ADCCLK and configure
     * the sampling frequency register accordingly */
    SYS_SET_ADC_SAMPLE_FREQ_CFG(AUDIO, LIB_AUDIO_SFCR_16K);

    /* Before configuring the FIFO controller,
     * the IOC, ADCs and DACs are first disabled to ensure that no data is sent
     * to the FIFO controller while the FIFO controller is being configured and
     * that the ADCs/DACs are not in an undesired configuration when the IOC is
     * configured. */

    /* Disable the ADCs without calibrating them */
    SYS_SET_ADC_CTRL(AUDIO, 0, LIB_ADC_CTRL_0_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 1, LIB_ADC_CTRL_1_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 2, LIB_ADC_CTRL_2_DISABLE_VAL);
    SYS_SET_ADC_CTRL(AUDIO, 3, LIB_ADC_CTRL_3_DISABLE_VAL);

    /* Disable the output drivers */
    SYS_SET_OUTPUT_CTRL(AUDIO, LIB_OUTPUT_CTRL_DISABLE_VAL);

    /* Disable the IOC ADC input */
    SYS_IOC_INPUTCONFIG(D_IOC, LIB_IOC_ADC_CFG_DISABLE_VAL);

    /* Disable the IOC PCM input */
    SYS_IOC_PCMINPUTCONFIG(D_IOC, LIB_IOC_PCM_CFG_DISABLE_VAL);

    /* Disable the IOC output */
    SYS_IOC_OUTPUTCONFIG(D_IOC, LIB_IOC_OUTPUT_CFG_DISABLE_VAL);

    /*
     * Configure the FIFOs and Input/Output Controller (IOC)
     */

    HCT_FIFO_Configure; /* Configure FIFOs */
    clear_FIFO_all();   /* Clear all FIFO values */

    SYS_FIFO_AUTOMUTE(D_FIFO_A0_2, FIFO_A0_AUTO_MUTE_ENABLE);  // DAC0 auto mute
    SYS_FIFO_AUTOMUTE(D_FIFO_A0_3, FIFO_A0_AUTO_MUTE_ENABLE);  // DAC1 auto mute

    /* Configure FIFO interrupts */
    SYS_FIFO_CM3INTCONFIG(5, FIFO_INT_A0_4);  // PCM out : CM3
    SYS_FIFO_CFXINTCONFIG(0, FIFO_INT_A0_0);  // MIC 0   : CFX
    SYS_FIFO_CFXINTCONFIG(1, FIFO_INT_A0_1);  // MIC 1   : CFX
    SYS_FIFO_CFXINTCONFIG(2, FIFO_INT_A0_2);  // DAC 0   : CFX
    SYS_FIFO_CFXINTCONFIG(3, FIFO_INT_A0_3);  // DAC 1   : CFX
    SYS_FIFO_CFXINTCONFIG(4, FIFO_INT_A0_4);  // PCM out : CFX
    SYS_FIFO_CFXINTCONFIG(5, FIFO_INT_A0_5);  // I2S in  : CFX
    SYS_FIFO_CFXINTCONFIG(6, FIFO_INT_A0_6);  // I2S out : CFX

    /* Configure how the IOC handles input data from the ADCs */
    SYS_IOC_INPUTCONFIG(D_IOC, LIB_IOC_ADC_CFG_VAL);

    /* Configure how the IOC handles input data from the PCM input */
    SYS_IOC_PCMINPUTCONFIG(D_IOC, LIB_IOC_PCM_CFG_VAL);

    /* Configure how the IOC interacts with the FIFOs */
    SYS_IOC_FIFOACCESSCONFIG(D_IOC, LIB_IOC_FIFO_ACCESS_VALUE);

    /* Configure how the IOC handles output data */
    SYS_IOC_OUTPUTCONFIG(D_IOC, LIB_IOC_OUTPUT_CFG_VAL);
}

void lib_init_audio_in_path_ADC(void)
{
    lib_init_audio_in_path_common();

    /* Configure audio multiplexing */
    SYS_AUDIOMUX_CONFIG(AUDIO, LIB_ADC_AUDIO_MUX_CFG_VAL);

    /* Configure ADCs (CFG) */
    // SYS_SET_ADC_CFG(AUDIO, 0, LIB_ADC_CFG_VAL);
    SYS_SET_ADC_CFG(AUDIO, 3, LIB_ADC_CFG_VAL);

    /* Configure the decimation filters for ADCs */
    // SYS_SET_ADC_DEC_CTRL(AUDIO, 0, LIB_ADC_DEC_CTRL_VAL);
    SYS_SET_ADC_DEC_CTRL(AUDIO, 3, LIB_ADC_DEC_CTRL_VAL);
}

void enable_AMIC(void)
{
    /* Configure and enable ADCs */
    // SYS_SET_ADC_CTRL(AUDIO, 0, (ADC_SEL_AI0 | LIB_ADC_CTRL_VAL));
    SYS_SET_ADC_CTRL(AUDIO, 3, (ADC_SEL_AI3 | LIB_ADC_CTRL_VAL));
}

void configure_audio_path_all(void)
{
    // 샘플링 주파수 관련 설정, ADC, OD, PCM, IOC 등 비활성화 먼저 진행
    // FIFO 설정, FIFO 클리어, FIFO 인터럽트 설정
    // ADC IN, PCM IN, OD OUT, PCM OUT의 FIFO 번호 설정
    lib_init_audio_in_path_common();

    /* Configure audio multiplexing */
    SYS_AUDIOMUX_CONFIG(AUDIO, LIB_DMIC_AUDIO_MUX_CFG_VAL);

    // U7 (DMIC1) : DMIC_CLK1/CAL (DIO22), DMIC_OUT1 (DIO23) : EZ
    // U9 (DMIC2) : DMIC_CLK2     (DIO10), DMIC_OUT2 (DIO17) : QCC

#if (LIB_AUDIO_IN_DMIC_ENABLE_COUNT == 1)
    /* Configure ADCs */
    SYS_SET_ADC_CFG(AUDIO, 1, LIB_ADC_CFG_VAL);  // 이게 필요한지 테스트 해야한다. (필요한듯)

    /* Configure the decimation filters for DMICs */
    SYS_SET_ADC_DEC_CTRL(AUDIO, 1, LIB_ADC_DEC_CTRL_VAL);  // EZ

    // DMIC 관련 DIO 설정
    Sys_DIO_Config(DIO22, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_CLK1/CAL (EZ)
    Sys_DIO_Config(DIO23, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_OUT1     (EZ)
    Sys_DIO_Config(DIO10, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_DISABLE));     // DMIC_CLK2     (QCC)
    Sys_DIO_Config(DIO17, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_DISABLE));     // DMIC_OUT2     (QCC)

    // DMIC를 1개, 2개, 몇개를 사용하든 CLK 설정은 하나만 가능하다. (DMIC_CLK1/CAL을 기본으로 사용)
    DIO->SRC_DMIC_CLK  = DMIC_CLK_SRC_DIO_22;    // DMIC_CLK1/CAL (EZ)
    DIO->SRC_DMIC_DATA = DMIC1_DATA_SRC_DIO_23;  // DMIC_OUT1     (EZ)
#elif (LIB_AUDIO_IN_DMIC_ENABLE_COUNT == 2)
    /* Configure ADCs */
    SYS_SET_ADC_CFG(AUDIO, 1, LIB_ADC_CFG_VAL);  // 이게 필요한지 테스트 해야한다. (필요한듯)
    SYS_SET_ADC_CFG(AUDIO, 2, LIB_ADC_CFG_VAL);  // 이게 필요한지 테스트 해야한다. (필요한듯)

    /* Configure the decimation filters for DMICs */
    SYS_SET_ADC_DEC_CTRL(AUDIO, 1, LIB_ADC_DEC_CTRL_VAL);  // EZ
    SYS_SET_ADC_DEC_CTRL(AUDIO, 2, LIB_ADC_DEC_CTRL_VAL);  // QCC

    // DMIC 관련 DIO 설정
    Sys_DIO_Config(DIO22, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_CLK1/CAL (EZ)
    Sys_DIO_Config(DIO23, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_OUT1     (EZ)
    Sys_DIO_Config(DIO10, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_CLK2     (QCC)
    Sys_DIO_Config(DIO17, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_OUT2     (QCC)

    // DMIC를 1개, 2개, 몇개를 사용하든 CLK 설정은 하나만 가능하다. (DMIC_CLK1/CAL을 기본으로 사용)
    DIO->SRC_DMIC_CLK  = DMIC_CLK_SRC_DIO_22;                            // DMIC_CLK1/CAL (EZ)
    DIO->SRC_DMIC_DATA = DMIC1_DATA_SRC_DIO_23 | DMIC2_DATA_SRC_DIO_17;  // DMIC_OUT1 (DIO23) : EZ, DMIC_OUT2(DIO17) : QCC
#else
#error "Invalid value : LIB_AUDIO_IN_DMIC_ENABLE_COUNT in 'lib_audio_in.h'"
#endif
}

void enable_DMIC(void)
{
    // U7 (DMIC1) : DMIC_CLK1/CAL (DIO22), DMIC_OUT1 (DIO23) : EZ
    // U9 (DMIC2) : DMIC_CLK2     (DIO10), DMIC_OUT2 (DIO17) : QCC

#if (LIB_AUDIO_IN_DMIC_ENABLE_COUNT == 1)
    Sys_DIO_Config(DIO22, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_ADCCLK));  // DMIC_CLK1/CAL (EZ)
#elif (LIB_AUDIO_IN_DMIC_ENABLE_COUNT == 2)
    Sys_DIO_Config(DIO22, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_ADCCLK));  // DMIC_CLK1/CAL (EZ)
    Sys_DIO_Config(DIO10, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_ADCCLK));  // DMIC_CLK2     (QCC)
#else
#error "Invalid value : LIB_AUDIO_IN_DMIC_ENABLE_COUNT in 'lib_audio_in.h'"
#endif
}

void disable_DMIC(void)
{
    // U7 (DMIC1) : DMIC_CLK1/CAL (DIO22), DMIC_OUT1 (DIO23) : EZ
    // U9 (DMIC2) : DMIC_CLK2     (DIO10), DMIC_OUT2 (DIO17) : QCC

#if (LIB_AUDIO_IN_DMIC_ENABLE_COUNT == 1)
    Sys_DIO_Config(DIO22, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_DISABLE));  // DMIC_CLK1/CAL (EZ)
#elif (LIB_AUDIO_IN_DMIC_ENABLE_COUNT == 2)
    Sys_DIO_Config(DIO22, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_DISABLE));  // DMIC_CLK1/CAL (EZ)
    Sys_DIO_Config(DIO10, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_DISABLE));  // DMIC_CLK2     (QCC)
#else
#error "Invalid value : LIB_AUDIO_IN_DMIC_ENABLE_COUNT in 'lib_audio_in.h'"
#endif
}

void clear_FIFO(volatile int _XMEM *p_fifo, int len)
{
    for (int i = 0; i < len; i++)
        chess_loop_range(1, HEAR_LARGEST_FIFO_SIZE)
        {
            p_fifo[i] = 0;
        }
}

void clear_FIFO_all(void)
{
    clear_FIFO(HCT_A0_0, HCT_FIFO_A0_0_LENGTH);  // FA0_0 : MIC0
    clear_FIFO(HCT_A0_1, HCT_FIFO_A0_1_LENGTH);  // FA0_1 : MIC1
    clear_FIFO(HCT_A0_2, HCT_FIFO_A0_2_LENGTH);  // FA0_2 : DAC0
    clear_FIFO(HCT_A0_3, HCT_FIFO_A0_3_LENGTH);  // FA0_3 : DAC1
    clear_FIFO(HCT_A0_4, HCT_FIFO_A0_4_LENGTH);  // FA0_4 : PCM out
    clear_FIFO(HCT_A0_5, HCT_FIFO_A0_5_LENGTH);  // FA0_5 : I2S in
    clear_FIFO(HCT_A0_6, HCT_FIFO_A0_6_LENGTH);  // FA0_6 : I2S out
    clear_FIFO(HCT_A1_0, HCT_FIFO_A1_0_LENGTH);  // FA1_0 : FFT
}

void disable_FIFO_all(void)
{
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_0, 0, 0, 0, 0, 0);  // FA0_0 : MIC0
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_1, 0, 0, 0, 0, 0);  // FA0_1 : MIC1
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_2, 0, 0, 0, 0, 0);  // FA0_2 : DAC0
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_3, 0, 0, 0, 0, 0);  // FA0_3 : DAC1
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_4, 0, 0, 0, 0, 0);  // FA0_4 : PCM out
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_5, 0, 0, 0, 0, 0);  // FA0_5 : I2S in
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_6, 0, 0, 0, 0, 0);  // FA0_6 : I2S out
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A1_0, 0, 0, 0, 0, 0);  // FA1_0 : FFT
}
