/**
 * @file system_control.h
 */

#ifndef __system_control_h__
#define __system_control_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <OTE_1_5_gen_FS_MEM.h>
#include <agc.h>
#include <definitionsForAlgorithm.h>
#include <interrupt_service_routine.h>
#include <shared_memory.h>
#include <stimulationStrategy.h>

#define df_24bitWordLength_mappingDate 6

/* 함수 헤더 */
void LB_Normal_PowerMode(void);
void Normal_PowerMode_event_mapChange(void);
void Normal_PowerMode_event_audioParameterCalculation(void);
void Normal_PowerMode_event_stimulationVolumeChange(void);
void Normal_PowerMode_event_both_stimulation_audio_volumeChange(void);
void Normal_PowerMode_isdConnected(void);
void Normal_PowerMode_isdDisonnected(void);
void copy_MappingData(void);
void copy_MappingData_without_mappingDate(void);

/* 전역 변수 */
// 매핑 프로그램에서 받은 값.
extern int chess_storage(XMEM) addr_MapProgramData_mappingDate[6];
extern int chess_storage(XMEM) addr_MapProgramData_stimulationStrategy;
extern int chess_storage(XMEM) addr_MapProgramData_firstPulsePhase;
extern int chess_storage(XMEM) addr_MapProgramData_StimulationMode;
extern int chess_storage(XMEM) addr_MapProgramData_StimulationPulsePhaseWidth;
extern int chess_storage(XMEM) addr_MapProgramData_FrequencyAnalysisBandNumbers;
extern int chess_storage(XMEM) addr_MapProgramData_indicatorStimulCannel_index;
extern int chess_storage(XMEM) addr_MapProgramData_indicatorStimulLevel_uA;

extern int chess_storage(XMEM) addr_MapProgramData_StimulusChannelAssignedElectrodIndex[32];
extern int chess_storage(XMEM) addr_MapProgramData_ReferenceChannelAssignedElectrodIndex[32];
extern int chess_storage(XMEM) addr_MapProgramData_CIS_FreqBandOrder[32];

extern int chess_storage(XMEM) addr_MapProgramData_T_Level_uA[32];
extern int chess_storage(XMEM) addr_MapProgramData_C_Level_uA[32];

extern int chess_storage(XMEM) addr_MapProgramData_xMinLevel[32];
extern int chess_storage(XMEM) addr_MapProgramData_xMaxLevel[32];

// 펌웨어에서 계산된 값.
extern int chess_storage(XMEM) g_mapping_stimulus_amplitude_T_level[32];
extern int chess_storage(XMEM) g_mapping_stimulus_amplitude_C_level[32];

extern int chess_storage(XMEM) m_previous_audio_volume;
extern int chess_storage(XMEM) m_previous_stimulation_volume;

#endif  // __system_control_h__
