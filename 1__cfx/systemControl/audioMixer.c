/**
 * @file audioMixer.c
 */

#include <audioMixer.h>

void audio_mix_internal_mic_only(void)
{
    int _XMEM *p_mix          = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;
    int _XMEM *p_internal_mic = (int _XMEM *) &HCT_A0_0[0];

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            p_mix[i] = p_internal_mic[i] >> AUDIO_INPUT_RSHIFT;
        }
}

void audio_mix_external_mic_only(void)
{
    int _XMEM *p_mix          = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;
    int _XMEM *p_external_mic = (int _XMEM *) &HCT_A0_1[0];

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            p_mix[i] = p_external_mic[i] >> AUDIO_INPUT_RSHIFT;
            // p_mix[i] = p_external_mic[i];
        }
}


void audio_mix_1_buffer(int _XMEM* p_buf1)
{
    int _XMEM* p_mix = (int _XMEM*) HEAR_ADDR_AUDIO_MIX;

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            p_mix[i] = (p_buf1[i] >> AUDIO_INPUT_RSHIFT);
        }
}

void audio_mix_2_buffers(int _XMEM* p_buf1, int _XMEM* p_buf2)
{
    int _XMEM* p_mix = (int _XMEM*) HEAR_ADDR_AUDIO_MIX;

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            // 두 값을 믹싱 했으므로 반으로 줄여준다.
            p_mix[i] = ((p_buf1[i] >> AUDIO_INPUT_RSHIFT) + (p_buf2[i] >> AUDIO_INPUT_RSHIFT)) >> 1;
        }
}

/* eof */
