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

void audio_mix_internal_mic_only(void);
void audio_mix_external_mic_only(void);

void lib_audio_loopback(int _XMEM* sink, int _XMEM* src);
void lib_loopback_AGC_out(int _XMEM* sink, int _XMEM* src);

#endif // __audioMixer_h__
