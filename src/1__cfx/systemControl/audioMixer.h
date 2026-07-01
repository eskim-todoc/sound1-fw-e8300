/**
 * @file audioMixer.h
 */

#ifndef __audioMixer_h__
#define __audioMixer_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <agc.h>
#include <definitionsForAlgorithm.h>
#include <microcode.h>
#include <shared_memory.h>
#include <stimulationStrategy.h>

// 빔포밍 delay-and-sum 정규화 시프트: 두 채널을 더한 뒤 >>1(÷2)로 평균.
// 위상 정렬된 동일 음원(상관)의 합 = 2배이므로 ÷2로 단일 마이크와 레벨 정합.
// (AUDIO_INPUT_RSHIFT=5(입력 스케일)와는 별개의 ÷2 정규화)
#define AUDIO_MIX_NORMALIZE_RSHIFT 1

// [MODULE] M1 믹싱 연산 — 상세: 유닛-모듈-테스트맵.md / audioMixer.c
void tdc_audio_mix_1_buffer(int _XMEM *p_buf1);
void tdc_audio_mix_2_buffers(int _XMEM *p_buf1, int _XMEM *p_buf2);
void tdc_audio_mix_2_buffers_for_beamforming(int _XMEM *p_delay_buf0, int _XMEM *p_delay_buf1, int _XMEM *p_no_delay_buf0);

#endif // __audioMixer_h__
