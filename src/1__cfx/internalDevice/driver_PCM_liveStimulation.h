/**
 * @file driver_PCM_liveStimulation.h
 */

#ifndef __driver_PCM_liveStimulation_h__
#define __driver_PCM_liveStimulation_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <definitionsForAlgorithm.h>

#include <OTE_1_5_gen_UART.h>
#include <driver_PCM.h>
#include <microcode.h>
#include <shared_memory.h>
#include <stimulationStrategy.h>

void check_link_connection_state(void);

void Enable_backtelCircuit(void);                   // driver_PCM_liveStimulation.c 에서만 사용됨
void backtelPcmOut(void);                           // driver_PCM_liveStimulation.c 에서만 사용됨
void frame_1_perChannel(void);                      // driver_PCM_liveStimulation.c 에서만 사용됨
void frame_2_perChannel(void);                      // driver_PCM_liveStimulation.c 에서만 사용됨
void frame_3_perChannel(void);                      // driver_PCM_liveStimulation.c 에서만 사용됨
void frame_4_perChannel(void);                      // driver_PCM_liveStimulation.c 에서만 사용됨
void Disable_backtelCircuit(void);                  // driver_PCM_liveStimulation.c 에서만 사용됨
void shareConnectionCheckFired_DiableBacktel(void); // driver_PCM_liveStimulation.c 에서만 사용됨
void LB_increaseCounter(void);                      // driver_PCM_liveStimulation.c 에서만 사용됨

extern int chess_storage(XMEM) m_link_connection_check_counter;

#endif // __driver_PCM_liveStimulation_h__
