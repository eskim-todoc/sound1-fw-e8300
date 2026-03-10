/**
 * @file stimulationStrategy.h
 */

#ifndef __stimulationStrategy_h__
#define __stimulationStrategy_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <OTE_1_5_gen_UART.h>
#include <definitionsForAlgorithm.h>
#include <driver_PCM.h>
#include <main.h>
#include <nonlinearMapping.h>
#include <shared_memory.h>
#include <system_control.h>
#include <microcode.h>

void read_transferableChannelNum_fromCM3(void);
void prepare_pcmStimulationPacketHeader(void);
void stimulationStrategy(void);
void stimulationStrategy_nOFm(void);
void stimulationStrategy_nOFm_Phase0(void);
void stimulationStrategy_nOFm_Phase1(void);
void stimulationStrategy_CIS(void);

extern int _XMEM g_nOFm_LastStimulus_BandIndex;
extern int _XMEM g_nOFm_Phase;  // 0 : 상위 0 ~ 7, 1 : 하위 8 ~ 15

extern int chess_storage(XMEM) g_pcm_stimulation_packet_header;
extern int chess_storage(XMEM) g_transferableChannelNum_per_1msec;
extern int chess_storage(XMEM) g_pcmFrameNum_per_channel;
extern int chess_storage(XMEM) addr_transferred_index;
extern int chess_storage(XMEM) addr_stimulationData[df_MaxNumOfElectrode];
extern int chess_storage(XMEM) addr_stimulationTempBuff[df_MaxNumOfElectrode];

#endif  // __stimulationStrategy_h__
