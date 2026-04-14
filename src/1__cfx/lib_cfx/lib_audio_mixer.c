/**
 * @file Lib_audio_mixer.c
 */

#include <lib_audio_mixer.h>

void lib_audio_loopback(int _XMEM* sink, int _XMEM* src)
{
    for (register int i = 0; i < LIB_AUDIO_LOOPBACK_LEN; i++)
        chess_loop_range(LIB_AUDIO_LOOPBACK_LEN, LIB_AUDIO_LOOPBACK_LEN)
        {
            sink[i] = src[i];  // >> (LIB_AUDIO_BIT_RSHIFT / 2);
        }
}

void lib_loopback_AGC_out(int _XMEM* sink, int _XMEM* src)
{
    long long_acc;

    for (register int i = 0; i < LIB_AUDIO_LOOPBACK_LEN; i++)
        chess_loop_range(LIB_AUDIO_LOOPBACK_LEN, LIB_AUDIO_LOOPBACK_LEN)
        {
            long_acc = src[i];
            long_acc = long_acc << LIB_AUDIO_BIT_RSHIFT;

            if (long_acc > INT24_MAX)
            {
                long_acc = INT24_MAX;
            }
            else if (long_acc < INT24_MIN)
            {
                long_acc = INT24_MIN;
            }

            sink[i] = (int) long_acc;
            //sink[i] = src[i] << LIB_AUDIO_BIT_RSHIFT;
        }
}
