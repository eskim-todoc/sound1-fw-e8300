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

void lib_audio_loopback(int _XMEM* sink, int _XMEM* src)
{
    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            sink[i] = src[i];
        }
}

void lib_loopback_AGC_out(int _XMEM* sink, int _XMEM* src)
{
    long long_acc;

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            long_acc = src[i];
            long_acc = long_acc << AUDIO_INPUT_RSHIFT;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            sink[i] = (int) long_acc;
        }
}

/* eof */
