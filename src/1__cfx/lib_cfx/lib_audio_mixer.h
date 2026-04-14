/**
 * @file Lib_audio_mixer.h
 */

#ifndef __lib_audio_mixer_h__
#define __lib_audio_mixer_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <definitionsForAlgorithm.h>

#define LIB_AUDIO_LOOPBACK_LEN 16
#define LIB_AUDIO_BIT_RSHIFT   6

// 자료형 범위 관련
#define INT24_MAX ((long) 0x7FFFFF)   // +8,388,607
#define INT24_MIN (-(long) 0x800000)  // -8,388,608

void lib_audio_loopback(int _XMEM* sink, int _XMEM* src);
void lib_loopback_AGC_out(int _XMEM* sink, int _XMEM* src);

#endif  // __lib_audio_mixer_h__
