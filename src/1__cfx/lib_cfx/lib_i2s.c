/**
 * @file lib_i2s.c
 */

#include <lib_i2s.h>

static volatile int _XMEM lib_g_i2s_state = 0;

volatile int _XMEM lib_g_i2s_interrupt_flag   = 0;
volatile int _XMEM lib_g_i2s_interrupt_cnt    = 0;
volatile int _XMEM lib_g_i2s_buffer_copy_done = 0;

int _XMEM lib_g_i2s_buffer[LIB_I2S_DATA_BUF_LEN];
int _XMEM lib_g_i2s_buffer_prev1[LIB_I2S_DATA_BUF_LEN];
int _XMEM lib_g_i2s_buffer_prev2[LIB_I2S_DATA_BUF_LEN];
int _XMEM lib_g_i2s_buffer_output[LIB_I2S_DATA_BUF_LEN];

int _XMEM lib_g_i2s_zero_buffer[16] = {0};
int _XMEM lib_g_i2s_buffers[I2S_BUFFER_FULL_READY_CNT][16];
int _XMEM lib_g_i2s_buffer_in_pos    = 0;
int _XMEM lib_g_i2s_buffer_out_pos   = 0;
int _XMEM lib_g_i2s_buffer_state     = 0;
int _XMEM lib_g_i2s_buffer_sub_state = 0;
int _XMEM lib_g_i2s_buffer_copy_cnt  = 0;

volatile int _XMEM lib_g_i2s_buffer_ready = 0;

volatile int _XMEM lib_g_i2s_prev1_offset = 0;

volatile int _XMEM lib_g_i2s_click_occurred = 0;

static volatile int _XMEM lib_g_i2s_interrupt_cnt_cmp = 0;
static volatile int _XMEM lib_g_i2s_disable_check_cnt = 0;
static volatile int _XMEM lib_g_i2s_enable_check_cnt  = 0;
static volatile int _XMEM lib_g_i2s_is_receiving_data = 0;

volatile int _XMEM lib_g_i2s_last_output_val = 0;

#if (I2S_BUFFER_UNDERRUN_CNT == 3)
int _XMEM I2S_coeff_fade_in_step[3][16] = {
    {0, 1369, 2738, 4107, 5476, 6845, 8214, 9583, 10952, 12321, 13690, 15059, 16428, 17797, 19166, 20535},
    {21904, 23273, 24642, 26011, 27380, 28749, 30118, 31487, 32856, 34225, 35594, 36963, 38332, 39701, 41070, 42439},
    {43808, 45177, 46546, 47915, 49284, 50653, 52022, 53391, 54760, 56129, 57498, 58867, 60236, 61605, 62974, 64343},
};

int _XMEM I2S_coeff_fade_out_step[3][16] = {
    {65536, 64167, 62898, 61629, 60360, 59091, 57822, 56553, 55284, 54015, 52746, 51477, 50208, 48939, 47670, 46401},
    {45132, 43863, 42594, 41325, 40056, 38787, 37518, 36249, 34980, 33711, 32442, 31173, 29904, 28635, 27366, 26097},
    {24828, 23559, 22290, 21021, 19752, 18483, 17214, 15945, 14676, 13407, 12138, 10869, 9600, 8331, 7062, 0},
};

#elif (I2S_BUFFER_UNDERRUN_CNT == 4)
int _XMEM I2S_coeff_fade_in_step[4][16] = {
    {0, 1023, 2047, 3071, 4095, 5119, 6143, 7167, 8191, 9215, 10239, 11263, 12287, 13311, 14335, 15359},
    {16383, 17407, 18431, 19455, 20479, 21503, 22527, 23551, 24575, 25599, 26623, 27647, 28671, 29695, 30719, 31743},
    {32767, 33791, 34815, 35839, 36863, 37887, 38911, 39935, 40959, 41983, 43007, 44031, 45055, 46079, 47103, 48127},
    {49151, 50175, 51199, 52223, 53247, 54271, 55295, 56319, 57343, 58367, 59391, 60415, 61439, 62463, 63487, 65536},
};

int _XMEM I2S_coeff_fade_out_step[4][16] = {
    {65536, 63487, 62463, 61439, 60415, 59391, 58367, 57343, 56319, 55295, 54271, 53247, 52223, 51199, 50175, 49151},
    {48127, 47103, 46079, 45055, 44031, 43007, 41983, 40959, 39935, 38911, 37887, 36863, 35839, 34815, 33791, 32767},
    {31743, 30719, 29695, 28671, 27647, 26623, 25599, 24575, 23551, 22527, 21503, 20479, 19455, 18431, 17407, 16383},
    {15359, 14335, 13311, 12287, 11263, 10239, 9215, 8191, 7167, 6143, 5119, 4095, 3071, 2047, 1023, 0},
};
#elif (I2S_BUFFER_UNDERRUN_CNT == 5)
int _XMEM I2S_coeff_fade_in_step[5][16] = {
    {0, 830, 1660, 2490, 3320, 4150, 4980, 5810, 6640, 7470, 8300, 9130, 9960, 10790, 11620, 12450},
    {13280, 14110, 14940, 15770, 16600, 17430, 18260, 19090, 19920, 20750, 21580, 22410, 23240, 24070, 24900, 25730},
    {26560, 27390, 28220, 29050, 29880, 30710, 31540, 32370, 33200, 34030, 34860, 35690, 36520, 37350, 38180, 39010},
    {39840, 40670, 41500, 42330, 43160, 43990, 44820, 45650, 46480, 47310, 48140, 48970, 49800, 50630, 51460, 52290},
    {53120, 53950, 54780, 55610, 56440, 57270, 58100, 58930, 59760, 60590, 61420, 62250, 63080, 63910, 64740, 65536},
};

int _XMEM I2S_coeff_fade_out_step[5][16] = {
    {65536, 64740, 63910, 63080, 62250, 61420, 60590, 59760, 58930, 58100, 57270, 56440, 55610, 54780, 53950, 53120},
    {52290, 51460, 50630, 49800, 48970, 48140, 47310, 46480, 45650, 44820, 43990, 43160, 42330, 41500, 40670, 39840},
    {39010, 38180, 37350, 36520, 35690, 34860, 34030, 33200, 32370, 31540, 30710, 29880, 29050, 28220, 27390, 26560},
    {25730, 24900, 24070, 23240, 22410, 21580, 20750, 19920, 19090, 18260, 17430, 16600, 15770, 14940, 14110, 13280},
    {12450, 11620, 10790, 9960, 9130, 8300, 7470, 6640, 5810, 4980, 4150, 3320, 2490, 1660, 830, 0},
};
else
#error "Invalid I2S_BUFFER_UNDERRUN_CNT"
#endif

void lib_init_I2S(uint32_t clk_dio, uint32_t frame_dio, uint32_t seri_dio, uint32_t flag_dio)
{
    /* Configure how the IOC1 handles I2S data */
    SYS_IOC_INPUTCONFIG(LIB_I2S_IOC, I2S_IOC_INPUT_CFG_NONE);
    SYS_IOC_OUTPUTCONFIG(LIB_I2S_IOC, I2S_IOC_OUTPUT_CFG_NONE);
    SYS_IOC_PCMINPUTCONFIG(LIB_I2S_IOC, I2S_IOC_PCM_CFG_NONE);

    LIB_I2S->CTRL   = PCM_RESET;
    LIB_I2S->STATUS = 3;  // 클리어 오버런, 언더런

    // I2S FLAG DIO 설정
    Sys_DIO_Config(flag_dio, I2S_FLAG_DIO_CFG);

    // DIO 설정 1차 (I2S 슬레이브 설정 적용을 위한 USRCLK)
    LIB_I2S_DIOConfig(LIB_I2S, PCM_SELECT_SLAVE, LIB_I2S_DIO_CFG, clk_dio, frame_dio, seri_dio, DIO_MODE_USRCLK);

    // I2S 설정
    Sys_PCM_Config(LIB_I2S, LIB_I2S_CFG);

    // DIO 설정 2차 (마스터로부터 입력 시작)
    LIB_I2S_DIOConfig(LIB_I2S, PCM_SELECT_SLAVE, LIB_I2S_DIO_CFG, clk_dio, frame_dio, seri_dio, DIO_MODE_INPUT);

    SYS_IOC_OUTPUTCONFIG(LIB_I2S_IOC, I2S_IOC_OUTPUT_CFG_VAL);
    SYS_IOC_PCMINPUTCONFIG(LIB_I2S_IOC, I2S_IOC_PCM_CFG_VAL);

    lib_g_i2s_enable_check_cnt  = 0;
    lib_g_i2s_disable_check_cnt = 0;
    lib_g_i2s_interrupt_cnt_cmp = 0;
    lib_g_i2s_buffer_ready      = 0;
    lib_g_i2s_buffer_copy_cnt   = 0;
    lib_g_i2s_is_receiving_data = 0;  // I2S 입력이 없는 상태로 설정

    lib_g_i2s_state = LIB_I2S_STATE_DISABLED;
}

void LIB_I2S_DIOConfig(const PCM_Type *pcm,  //
                       uint32_t        slave,
                       uint32_t        cfg,
                       uint32_t        clk,
                       uint32_t        frame,
                       uint32_t        seri,
                       uint32_t        clksrc)
{
    SYS_ASSERT(PCM_REF_VALID(pcm));
    unsigned int diff = (pcm - PCM);

    /* Configure PCM CLK pad configuration*/
    SYS_DIO_CONFIG(clk, (cfg | clksrc));

    /* Configure PCM SERI pad as input */
    SYS_DIO_CONFIG(seri, (cfg | DIO_MODE_INPUT));

    if (slave == PCM_SELECT_MASTER)
    {
        /* Configure PCM FRAME pad as output */
        SYS_DIO_CONFIG(frame, (cfg | (DIO_MODE_PCM0_FRAME + (diff * PCM_PADS_NUM))));
    }
    else
    {
        /* Configure PCM FRAME pad as input */
        SYS_DIO_CONFIG(frame, (cfg | DIO_MODE_INPUT));
    }

    DIO->SRC_PCM[diff] = (((seri << DIO_SRC_PCM_SERI_Pos) & DIO_SRC_PCM_SERI_Mask)      /**/
                          | ((frame << DIO_SRC_PCM_FRAME_Pos) & DIO_SRC_PCM_FRAME_Mask) /**/
                          | ((clk << DIO_SRC_PCM_CLK_Pos) & DIO_SRC_PCM_CLK_Mask));
}

void lib_enable_I2S(void)
{
    LIB_I2S->CTRL = PCM_ENABLE;
}

void lib_i2s_disable(void)
{
    LIB_I2S->CTRL = PCM_DISABLE;
}

int lib_i2s_is_buffer_copy_done(void)
{
    return lib_g_i2s_buffer_copy_done;
}

void lib_i2s_clear_buffer_copy_done(void)
{
    lib_g_i2s_buffer_copy_done = 0;
}

void tdc_i2s_set_streaming_state(int state)
{
    lib_g_i2s_state = state;
}

int I2S_isStreaming(void)
{
    return lib_g_i2s_state == LIB_I2S_STATE_ENABLED;
}

void I2S_update_state(void)
{
    // 상태가 Disabled 상태일 때는 Enabled가 되는지 확인하고,
    // 상태가 Enabled 상태일 때는 반대로 Disabled가 되는지 확인한다.

    switch (lib_g_i2s_state)
    {
        case LIB_I2S_STATE_DISABLED:

            if ((Sys_GPIO_Read(I2S_FLAG_DIO_NUM) == I2S_FLAG_ACTIVE_LEVEL)    // I2S 플래그 액티브 상태이고
                && (lib_g_i2s_interrupt_cnt_cmp != lib_g_i2s_interrupt_cnt))  // I2S 인터럽트도 발생해야 한다.
            {
                lib_g_i2s_enable_check_cnt++;

                // ENABLE CHECK CNT 만큼 I2S 이벤트가 발생했다면 I2S 입력이 있는 상태를 의미한다.
                if (lib_g_i2s_enable_check_cnt == LIB_I2S_ENABLE_CHECK_CNT)
                {
                    lib_g_i2s_disable_check_cnt = 0;
                    lib_g_i2s_buffer_ready      = 0;
                    lib_g_i2s_buffer_copy_cnt   = 0;

                    lib_g_i2s_buffer_in_pos    = 0;
                    lib_g_i2s_buffer_out_pos   = 0;
                    lib_g_i2s_buffer_state     = I2S_BUFFER_STATE_INIT;
                    lib_g_i2s_buffer_sub_state = I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0;

                    lib_g_i2s_state = LIB_I2S_STATE_ENABLED;
                }
            }
            else  // I2S 플래그가 액티브 상태가 아니거나, 인터럽트가 발생되지 않았다면 카운트 초기화
            {
                lib_g_i2s_enable_check_cnt = 0;
            }

            break;

        case LIB_I2S_STATE_ENABLED:

            // I2S 플래그가 액티브 상태가 아닐 때는 즉시 Disabled 상태로 변경
            if (Sys_GPIO_Read(I2S_FLAG_DIO_NUM) != I2S_FLAG_ACTIVE_LEVEL)
            {
                lib_g_i2s_enable_check_cnt = 0;
                lib_g_i2s_state            = LIB_I2S_STATE_DISABLED;
            }
            else
            {
                // 인터럽트가 발생하지 않았어야 한다.
                if (lib_g_i2s_interrupt_cnt_cmp == lib_g_i2s_interrupt_cnt)
                {
                    lib_g_i2s_disable_check_cnt++;

                    // DISBLE CHECK CNT 만큼 I2S 이벤트가 발생하지 않으면 I2S 입력이 없는 상태를 의미한다.
                    if (lib_g_i2s_disable_check_cnt == LIB_I2S_DISABLE_CHECK_CNT)
                    {
                        lib_g_i2s_enable_check_cnt = 0;
                        lib_g_i2s_buffer_ready     = 0;
                        lib_g_i2s_buffer_copy_cnt  = 0;
                        lib_g_i2s_click_occurred   = 0;

                        lib_g_i2s_buffer_in_pos    = 0;
                        lib_g_i2s_buffer_out_pos   = 0;
                        lib_g_i2s_buffer_state     = I2S_BUFFER_STATE_INIT;
                        lib_g_i2s_buffer_sub_state = I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0;

                        lib_g_i2s_state = LIB_I2S_STATE_DISABLED;
                    }
                }
                else  // 인터럽트 발생 시 비활성화 카운트 초기화
                {
                    lib_g_i2s_disable_check_cnt = 0;
                }
            }

            break;
    }

    lib_g_i2s_interrupt_cnt_cmp = lib_g_i2s_interrupt_cnt;
}

void lib_i2s_copy_data_from_fifo(int _XMEM *p_fifo)
{
    int *restrict p_dst;
    int *restrict p_src;

    // Interrupt-safe code 패턴
    // Race Condition, Interrupt Harzard, Shared Variable Conflict라고 불리는
    // 인터럽트 사용시 값이 꼬이는 버그 상황을 고려한 방식

    disable_interrupts();

    if (!lib_g_i2s_interrupt_flag)
    {
        // I2S 인터럽트가 발생되지 않았다면 바로 리턴
        enable_interrupts();
        return;
    }

    lib_g_i2s_interrupt_flag = 0;

    enable_interrupts();

    p_dst = (int *) &lib_g_i2s_buffers[lib_g_i2s_buffer_in_pos][0];
    p_src = (int *) p_fifo;

    for (register int i = 0; i < 16; i++)
        chess_loop_range(16, 16) chess_unroll_loop(*)
        {
            p_dst[i] = p_src[i];
        }

    lib_g_i2s_buffer_in_pos++;

    if (lib_g_i2s_buffer_in_pos == I2S_BUFFER_FULL_READY_CNT)
    {
        lib_g_i2s_buffer_in_pos = 0;
    }

    lib_g_i2s_buffer_copy_cnt++;

    if ((lib_g_i2s_buffer_state == I2S_BUFFER_STATE_INIT)  //
        || (lib_g_i2s_buffer_state == I2S_BUFFER_STATE_UNDERRUN))
    {
        if (lib_g_i2s_buffer_copy_cnt == I2S_BUFFER_FULL_READY_CNT)
        {
            lib_g_i2s_buffer_state     = I2S_BUFFER_STATE_READY;
            lib_g_i2s_buffer_sub_state = I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0;
        }
    }
}

void lib_i2s_clear_data(void)
{
    // 데이터를 삭제한다.
    for (register int i = 0; i < LIB_I2S_DATA_BUF_LEN; i++)
        chess_loop_range(LIB_I2S_DATA_BUF_LEN, LIB_I2S_DATA_BUF_LEN)
        {
            // lib_g_i2s_buffer[i] = 0;
            lib_g_i2s_buffer_output[i] = 0;
        }
}

int _XMEM *I2S_get_buffer_with_fade_in_process(void)
{
    int *p_src   = 0;
    int *p_coeff = 0;

    p_src = &lib_g_i2s_buffers[lib_g_i2s_buffer_out_pos][0];

    lib_g_i2s_buffer_out_pos++;
    if (lib_g_i2s_buffer_out_pos == I2S_BUFFER_FULL_READY_CNT)
    {
        lib_g_i2s_buffer_out_pos = 0;
    }

    if (I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0 <= lib_g_i2s_buffer_sub_state && lib_g_i2s_buffer_sub_state < (I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0 + I2S_BUFFER_UNDERRUN_CNT))
    {
        p_coeff = &I2S_coeff_fade_in_step[lib_g_i2s_buffer_sub_state - I2S_BUFFER_SUB_STATE_FADE_IN_STEP_0][0];
        lib_g_i2s_buffer_sub_state++;
    }

#if 1
    if (p_coeff != 0)
    {
        for (register int i = 0; i < LIB_I2S_DATA_BUF_LEN; i++)
            chess_loop_range(LIB_I2S_DATA_BUF_LEN, LIB_I2S_DATA_BUF_LEN)
            {
#if 1
                long t = (((long) p_src[15 - i]) * ((long) p_coeff[i])) >> 16;

                if (t > Q24_MAX)
                {
                    t = Q24_MAX;
                }
                else if (t < Q24_MIN)
                {
                    t = Q24_MIN;
                }

                p_src[15 - i] = (int) t;
#else
                p_src[15 - i] = p_coeff[i];
#endif
            }
    }
#endif
    return p_src;
}

int _XMEM *I2S_get_buffer_with_fade_out_process(void)
{
    int *p_src   = 0;
    int *p_coeff = 0;

    if (lib_g_i2s_buffer_sub_state == (I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0 + I2S_BUFFER_UNDERRUN_CNT))
    {
        p_src = &lib_g_i2s_zero_buffer[0];
    }
    else
    {
        p_src = &lib_g_i2s_buffers[lib_g_i2s_buffer_out_pos][0];

        lib_g_i2s_buffer_out_pos++;
        if (lib_g_i2s_buffer_out_pos == 4)
        {
            lib_g_i2s_buffer_out_pos = 0;
        }

        if (I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0 <= lib_g_i2s_buffer_sub_state && lib_g_i2s_buffer_sub_state < (I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0 + I2S_BUFFER_UNDERRUN_CNT))
        {
            p_coeff = &I2S_coeff_fade_out_step[lib_g_i2s_buffer_sub_state - I2S_BUFFER_SUB_STATE_FADE_OUT_STEP_0][0];
            lib_g_i2s_buffer_sub_state++;
        }
#if 1
        if (p_coeff != 0)
        {
            for (register int i = 0; i < LIB_I2S_DATA_BUF_LEN; i++)
                chess_loop_range(LIB_I2S_DATA_BUF_LEN, LIB_I2S_DATA_BUF_LEN)
                {
#if 1
                    long t = (((long) p_src[15 - i]) * ((long) p_coeff[i])) >> 16;

                    if (t > Q24_MAX)
                    {
                        t = Q24_MAX;
                    }
                    else if (t < Q24_MIN)
                    {
                        t = Q24_MIN;
                    }

                    p_src[15 - i] = (int) t;
#else
                    p_src[15 - i] = p_coeff[i];
#endif
                }
        }
#endif
    }

    return p_src;
}

/* EOF */

