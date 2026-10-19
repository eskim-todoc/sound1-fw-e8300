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

/* FPGA2.7.0/Sullivan 백텔 패킷: Forward register3 쓰기, 데이터0xFF.
 * Duration 최소값은 CM3 board/FPGA_ver2_7_0.h와 함께 검토한다. */
#define TDC_LINK_BACKTEL_WRITE_FORWARD 0x507FF
#define TDC_LINK_BACKTEL_MIN_PULSE_WIDTH_US 13
#define TDC_LINK_BACKTEL_ENABLE_SLOT 19
#define TDC_LINK_BACKTEL_DISABLE_SLOT 1

/* CM3 작성 100ms 단위1..255. 미게시/범위 오류는 기존300ms로 복귀한다. */
int tdc_link_backtel_period_ms(void);
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
