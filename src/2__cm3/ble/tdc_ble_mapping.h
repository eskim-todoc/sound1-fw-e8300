#ifndef __tdc_ble_mapping_h__
#define __tdc_ble_mapping_h__

#include <stdbool.h>
#include "tdc_stim_definitions.h"
#include "tdc_shm.h"
#include "tdc_isd.h"
#include "tdc_ble_protocol.h"

typedef struct
{
    int iterationNum;
    int channel;
    int stimulationLevel_uA;
    int pulseWidth_start_usec;
    int pulseWidth_end_usec;

} ST__MAPPINGPAYLOAD_IMPEDANCE;

typedef struct
{
    int firstPulsePhase;
    int stimulatonMode;
    int stimulationElectrodeNum;
    int measurementElectrodeNum;
    int bipolarReferenceElectrodeNum;
    int stimulationLevel_uA_masker;
    int stimulationLevel_uA_probe;
    int pulseWidth;
    int maskerProbeInterval_numFrame;  // 1Frame이 41.6666 usec에 해당한다. --1msec에 24채널의 PCM을 생성할 경우.
    int iterationNum;
    int adcPreampGain;
    int adcSamplingFreq;
    int adcMeasurementDelay;
    int measurementSampleNum;
} ST__MAPPINGPAYLOAD_eCAP;

typedef struct
{
    int usableElectrodeNum;
    int firstPulsePhase;
    int stimulatonMode;
    int stimulationElectrodeNum;
    int bipolarReferenceElectrodeNum;
    int stimulationLevel_uA;
    int pulseWidth;
    int stimulationTime_100msec;
} ST__MAPPINGPAYLOAD_SPECIFIC_STIMULATION;

typedef struct
{

    int subCommand;  // 시작, 중지, 자극 볼륨 조절, 마이크 감도 조절, 알림용 자극  설정

    int stimulVolume;
    int audioVolume;
    int stimulationIndicatorChannelNum;
    int stimulationIndicatorAmplitude_uA;

    int                   stimulationStrategy;
    int                   firstPulsePhase;
    EN___STIMULATION_MODE stimulationMode;
    int                   stimulationPulsePhaseWidth;
    int                   numFrequencyBand;
    int                   equlizer_ReadStart_index;
    int                   equlizer_ReadEnd_index;
    int                   usableStimulationElectrodIndex[df_MaxNumOfElectrode];
    int                   usableReferenceElectrodIndex[df_MaxNumOfElectrode];
    int                   CIS_FreqBandOrder[df_MaxNumOfElectrode];
    int                   T_level_uA[df_MaxNumOfElectrode];
    int                   C_level_uA[df_MaxNumOfElectrode];
    int                   audio_input_x_mim[df_MaxNumOfElectrode];
    int                   audio_input_x_max[df_MaxNumOfElectrode];

} ST__MAPPINGPAYLOAD_LIVE_STIMULATION;

typedef struct
{
    int slot_index;
    int map_index;
} ST__MAPPINGPAYLOAD_READWRITEDATA_FLASH;

typedef struct
{
    int  ISD_id;
    int  location;
    int  userName[25];
    bool id_check_is_completed;
    bool id_match;
} ST__MAPPINGPAYLOAD_ORIGINAL_ISD_INFO;

typedef struct
{

    int pulseWidth;
    int firstPulsePhase;
    int stimulatonMode;
    int usableElectrodeNum;
    int stimulationElectrodeNum[df_MaxNumOfElectrode];
    int bipolarReferenceElectrodeNum[df_MaxNumOfElectrode];
    int stimulationDacSlope;
    int stimulationDacOffsetReslution;
    int stimulationDacOffset_255;
    int stimulationLevel_255;
    int stimulationTime_100msec;

} ST__MAPPINGPAYLOAD_TEST_STIMULATION;

typedef struct
{
    EN__MAPPING_COMMAND                     fetched_command;
    EN__MAPPING_COMMAND                     command;
    ST__MAPPINGPAYLOAD_IMPEDANCE            impedanceCheck;
    ST__MAPPINGPAYLOAD_eCAP                 eCapMeasurement;
    ST__MAPPINGPAYLOAD_SPECIFIC_STIMULATION tdc_isd_map_specific_stim_step;
    ST__MAPPINGPAYLOAD_LIVE_STIMULATION     tdc_isd_map_live_step;
    ST__MAPPINGPAYLOAD_TEST_STIMULATION     testStimulation;
    ST__MAPPINGPAYLOAD_READWRITEDATA_FLASH  ReadWriteMapData_Flash;
    ST__MAPPINGPAYLOAD_ORIGINAL_ISD_INFO    rx_orignal_ISD_info;

} ST__MAPPING_PACKET;

// #pragma pack(4)
typedef struct
{
    EN__ISD_CONTROL_STATE isdControlCommand;
    bool                  mappingConnection;
    bool                  StimulationIndicatorTrigger;
    bool                  BLE_Off;

} ST__MAPPING_STATE;

void tdc_ble_mapping_clear_command();

bool                tdc_ble_mapping_is_program_connected(void);
void                tdc_ble_mapping_change_command_ble_disconnected(void);
void                tdc_ble_mapping_change_command_waiting_ble_off(void);
EN__MAPPING_COMMAND tdc_ble_mapping_get_command(void);
// const ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void);
ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void);

void tdc_ble_mapping_fetch_packet(const int *Rx_dataPacket);

ST__MAPPING_STATE tdc_ble_mapping_step(ST__ISD_STATUS ISD_state);




void tdc_ble_mapping_update_program_connection(bool connection);

#endif
