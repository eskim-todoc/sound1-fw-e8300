/**
 * @file lib_audio_in.c
 */

#include <lib_audio_in.h>

void disable_audio_path_all(void)
{
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
}

void configure_audio_IOC(void)
{
    /* Configure how the IOC handles input data from the ADCs */
    SYS_IOC_INPUTCONFIG(D_IOC, LIB_IOC_ADC_CFG_VAL);

    /* Configure how the IOC handles input data from the PCM input */
    SYS_IOC_PCMINPUTCONFIG(D_IOC, LIB_IOC_PCM_CFG_VAL);

    /* Configure how the IOC interacts with the FIFOs */
    SYS_IOC_FIFOACCESSCONFIG(D_IOC, LIB_IOC_FIFO_ACCESS_VALUE);

    /* Configure how the IOC handles output data */
    SYS_IOC_OUTPUTCONFIG(D_IOC, LIB_IOC_OUTPUT_CFG_VAL);
}

void configure_audio_path_all(void)
{
    /* Before configuring the FIFO controller, the IOC, ADCs and DACs
     * are first disabled to ensure that no data is sent to the FIFO controller while
     * the FIFO controller is being configured and that the ADCs/DACs are
     * not in an undesired configuration when the IOC is configured. */
    disable_audio_path_all();

    /* Configure the FIFOs. */
    configure_FIFO_all();

    /* Configure the Input/Output Controller (IOC). */
    configure_audio_IOC();

    /* Configure audio multiplexing */
    SYS_AUDIOMUX_CONFIG(AUDIO, LIB_DMIC_AUDIO_MUX_CFG_VAL);

    /* Configure ADCs */
    SYS_SET_ADC_CFG(AUDIO, 0, LIB_ADC_CFG_VAL);
    SYS_SET_ADC_CFG(AUDIO, 1, LIB_ADC_CFG_VAL);

    /* Configure the decimation filters for DMICs */
    SYS_SET_ADC_DEC_CTRL(AUDIO, 0, LIB_ADC_DEC_CTRL_VAL);
    SYS_SET_ADC_DEC_CTRL(AUDIO, 1, LIB_ADC_DEC_CTRL_VAL);

    /* DMIC CLK:  DIO11, DMIC OUT:  DIO4
     * DMIC 활성화 전까지, DMIC 라인을 풀업 상태로 유지. */
    Sys_DIO_Config(DIO11, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));  // DMIC_CLK
    Sys_DIO_Config(DIO4, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_INPUT));   // DMIC_OUT

    DIO->SRC_DMIC_CLK  = DMIC_CLK_SRC_DIO_11;
    DIO->SRC_DMIC_DATA = DMIC0_DATA_SRC_DIO_4;
}

void enable_AMIC(void)
{
    /* 실제 ADC는 켜져있고, AI1에 대한 입력만 AI1로 설정한다.
     * KTL 전자파 시험을 위한 설정이다. */
    SYS_SET_ADC_CTRL(AUDIO, 1, (ADC_SEL_AI1 | LIB_ADC_CTRL_VAL));
}

void disable_AMIC(void)
{
    /* 실제 ADC는 켜져있고, AI1에 대한 입력만 VSSA로 단락 시킨다.
     * KTL 전자파 시험을 위한 설정이다. */
    SYS_SET_ADC_CTRL(AUDIO, 1, (ADC_SEL_VSSA | LIB_ADC_CTRL_VAL));
}

void enable_DMIC(void)
{
    // DMIC_CLK에 해당하는 DIO를 INPUT PULLUP에서 ADCCLK로 변경하여, DMIC로 클럭 게이팅
    Sys_DIO_Config(DIO11, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_ADCCLK));  // DMIC_CLK
}

void disable_DMIC(void)
{
    // DMIC_CLK에 해당하는 DIO를 DISABLE PULLUP으로 변경하여, DMIC로 가는 클럭 게이팅 차단
    Sys_DIO_Config(DIO11, (DIO_1X_DRIVE | DIO_LPF_DISABLE | DIO_WEAK_PULL_UP | DIO_MODE_DISABLE));
}

void reset_FFT_FIFO(void)
{
    clear_FIFO(HCT_A1_0, HCT_FIFO_A1_0_LENGTH);      // FA1_0 : FFT
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A1_0,  //
                       HCT_FIFO_A1_0_START,
                       HCT_FIFO_A1_0_LENGTH,
                       HCT_FIFO_A1_0_BLOCK_SIZE,
                       HCT_FIFO_A1_0_BASE_PTR,
                       HCT_FIFO_A1_0_IOBLOCK_PTR);
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
    clear_FIFO(HCT_A0_0, HCT_FIFO_A0_0_LENGTH);  // FA0_0 : MIC0 (DMIC)
    clear_FIFO(HCT_A0_1, HCT_FIFO_A0_1_LENGTH);  // FA0_1 : MIC1 (Earpiece)
    clear_FIFO(HCT_A0_3, HCT_FIFO_A0_3_LENGTH);  // FA0_3 : DAC1
    clear_FIFO(HCT_A0_4, HCT_FIFO_A0_4_LENGTH);  // FA0_4 : PCM out
    clear_FIFO(HCT_A1_0, HCT_FIFO_A1_0_LENGTH);  // FA1_0 : FFT
}

void disable_FIFO_all(void)
{
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_0, 0, 0, 0, 0, 0);  // FA0_0 : MIC0 (DMIC)
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_1, 0, 0, 0, 0, 0);  // FA0_1 : MIC1 (Earpiece)
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_3, 0, 0, 0, 0, 0);  // FA0_3 : DAC1
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A0_4, 0, 0, 0, 0, 0);  // FA0_4 : PCM out
    Sys_FIFO_Configure((D_FIFO_Type *) D_FIFO_A1_0, 0, 0, 0, 0, 0);  // FA1_0 : FFT
}

void configure_FIFO_all(void)
{
    HCT_FIFO_Configure; /* Configure FIFOs */
    clear_FIFO_all();   /* Clear all FIFO values */

    SYS_FIFO_AUTOMUTE(D_FIFO_A0_3, FIFO_A0_AUTO_MUTE_ENABLE);  // DAC1 auto mute
    SYS_FIFO_AUTOMUTE(D_FIFO_A0_4, FIFO_A0_AUTO_MUTE_ENABLE);  // PCM0 auto mute

    /* Configure FIFO interrupts */
    SYS_FIFO_CM3INTCONFIG(5, FIFO_INT_A0_4);  // PCM out : CM3
    SYS_FIFO_CFXINTCONFIG(0, FIFO_INT_A0_0);  // MIC 0   : CFX (DMIC)
    SYS_FIFO_CFXINTCONFIG(1, FIFO_INT_A0_1);  // MIC 1   : CFX (AMIC, Earpiece)
    SYS_FIFO_CFXINTCONFIG(3, FIFO_INT_A0_3);  // DAC 1   : CFX (Debugging)
    SYS_FIFO_CFXINTCONFIG(4, FIFO_INT_A0_4);  // PCM out : CFX
}
