#ifndef cfx_cm3_sharedMemory_H__
#define cfx_cm3_sharedMemory_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <definitionsForAlgorithm.h>

typedef struct
{
    int PCM_mode;
    int PCM_liveStimulationTemplate[df_MaxNumTransferableChannel];
    int PCM_specificBuffer[df_MaxNumTransferableChannel];
    int PCM_mode_next;
    int Reset_PCM;
    int conneded_ISDCheckPCM_state;
} CFX_CM3_SharedMemory_PCM_interface;

typedef struct
{
    int i2c_State;
    int RW;
    int SlaveAddr;
    int dataSize;
    int CM3_TxBuffer[df_i2c_Tx_buffer_size];
    int CM3_RxBuffer[df_i2c_Rx_buffer_size];
} ST__CFX_CM3_SharedMemory_CFX_i2c_interface;

typedef struct
{
    int mapping_year;
    int mapping_month;
    int mapping_day;
    int mapping_hour;
    int mapping_min;
    int mapping_sec;
} ST__MAPPING_DATE;

typedef enum
{
    en__referenceNA = 0,
    en__monopolr_body,
    en__monopolr_rod,
    en__monopolr_BothRodBody,
    en__bipolar,
    en__commonground,
    en__semi_simultaneously
} EN___STIMULATION_MODE;

typedef struct
{
    ST__MAPPING_DATE mapStamp;
    int              user_usableMapNum;
    int              usableMapIndex[4];  // 1 : 사용가능, 0 : 사용불가
    ST__MAPPING_DATE map_date[4];        // 년도(xx년),월, 일, 시, 분, 초
} ST__CFX_CM3_SharedMemory_connected_ISD_Map_info;

typedef enum
{
    Left_Ear = 1,
    Right_Ear
} EN__LOCATION_OF_ISD;

typedef struct
{
    int isd_year;
    int isd_month_model;
    // int isd_model_num;
    int                 isd_serial;
    EN__LOCATION_OF_ISD isd_location_RL;
    int                 isd_userName[25];
    int                 remocon_passkey[4];
} ST__CFX_CM3_SharedMemory_ISD_info;

typedef struct
{
    int mapNum;
    int stimulVolume;
    int audioVolume;
    int indicatorLED_OnOff;
    int indicatorStimul_OnOff;
    int teleCoil_OnOff;
    int Ble_Onff;
} ST__CFX_CM3_SharedMemory_userSettingValue;

typedef struct
{
    ST__MAPPING_DATE      mappingDate;
    int                   stimulationStrategy;
    int                   firstPulsePhase;
    EN___STIMULATION_MODE stimulationMode;
    int                   stimulationPulsePhaseWidth;
    int                   numFrequencyBand;
    int                   stimulationIndicatorChannelNum;
    int                   stimulationIndicatorAmplitude_uA;
    int                   usableStimulationElectrodIndex[df_MaxNumOfElectrode];
    int                   usableReferenceElectrodIndex[df_MaxNumOfElectrode];
    int                   CIS_FreqBandOrder[df_MaxNumOfElectrode];
    int                   T_level_uA[df_MaxNumOfElectrode];
    int                   C_level_uA[df_MaxNumOfElectrode];
    int                   audio_input_x_mim[df_MaxNumOfElectrode];
    int                   audio_input_x_max[df_MaxNumOfElectrode];
} ST__CFX_CM3_SharedMemory_mapData;

typedef struct
{
    int T_level_255[df_MaxNumOfElectrode];  // CM3에서 계산함
    int C_level_255[df_MaxNumOfElectrode];  // CM3에서 계산함
    int frameNumPerChannel;                 // cm3에서 계산함
    int transferableFrameNum;               // cm3에서 계산함
} ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3;

typedef struct
{
    int indicatorStimulLevel_255;                     // cm3에서 계산함
    int indicatorStimulOutput_OnOff_Coltroled_byCM3;  // cm3에서 변경함.
} ST__CFX_CM3_SharedMemory_calculatedStimulationIndcator_byCM3;

typedef struct
{
    int cm3Command_mapChange;
    int cfx_Reloaded_MapdataFlag;
    int cm3_audioParameterCalculationDone_Flag;
} ST__CFX_CM3_SharedMemory_MapDataChange;

typedef enum
{
    en__normalMode = 0,
    en__mappingMode,
    en__systemReset,
} EN__SYSTEM_OP_MODE;

typedef struct
{
    EN__SYSTEM_OP_MODE system_opMode;  // 현재 사용되는 곳 없음. 지울 것. 지우면  공유메모리 주소도 변경해야 함
    int                powerButton_pushed_CFX_to_CM3;
    int                batteryLevel_CfX_to_CM3;  // NOTE : CM3에서 제어한 이후로 미사용됨
    int                consumptionPowerControl_Command_CM3_to_CFX;
    int                enter_ULP_mode_Command_CM3_to_CFX;
} ST__CFX_CM3_SharedMemory_System;

typedef enum
{
    flash_Command_NA = 0,
    flash_Command_Read,
    flash_Command_Write,
    flash_Command_Erase,
    flash_Command_Recover
} EN__mapping_ReadWriteMap_command;

typedef struct
{
    EN__mapping_ReadWriteMap_command flashCommand;
    int                              isd_index;
    int                              map_index;
} ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash;

typedef struct
{
    ST__CFX_CM3_SharedMemory_ISD_info         ISD_info_mapData;
    ST__CFX_CM3_SharedMemory_userSettingValue userSettingValue_mapData;
    ST__MAPPING_DATE                          mapStamp;
    ST__CFX_CM3_SharedMemory_mapData          readWritemapData;
} ST__CFX_CM3_SharedMemory_RepositoryForReadWriteMapData;

typedef struct
{
    int chargerConnectorPluggedIn;
    int carryingCasePluggedIn;
    int carryingCaseCoverOpen;
} ST__USB_CONNECTOR;

typedef struct
{
    int                                                          CFX_EEPROM_data_is_Loaded;
    int                                                          CFX_ErrorCode;
    ST__USB_CONNECTOR                                            chargerState;
    CFX_CM3_SharedMemory_PCM_interface                           cfx_PCM_interface;
    ST__CFX_CM3_SharedMemory_CFX_i2c_interface                   cfx_i2c_interface;
    int                                                          connected_ISD_num;  // 0연결안됨, 1~4 연결된 ISD 번호
    ST__CFX_CM3_SharedMemory_connected_ISD_Map_info              connected_ISD_Map_info;
    ST__CFX_CM3_SharedMemory_ISD_info                            cfx_ISD_info[MAX_NUM_USER];
    ST__CFX_CM3_SharedMemory_userSettingValue                    userSettingValue;
    int                                                          userSettingValueLoadedFlag;
    ST__CFX_CM3_SharedMemory_mapData                             currentMapData;
    ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3          calculatedStimulPara_byCM3;
    ST__CFX_CM3_SharedMemory_calculatedStimulationIndcator_byCM3 calculatedStimulationIndcator_byCM3;
    int                                                          currentOutputStimulLevel_255[df_MaxNumOfElectrode];
    ST__CFX_CM3_SharedMemory_MapDataChange                       mapChangeFlag;
    ST__CFX_CM3_SharedMemory_System                              systemShare;
    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash           ReadWriteCommand_ForFlash;
    ST__CFX_CM3_SharedMemory_RepositoryForReadWriteMapData       repositoryForReadWriteMapData;
    int                                                          batteryCalibrationValue;
    int                                                          backtelControlRegister;
    int                                                          isMappingProgramConneted;  // 사용안함 지울 것
    int                                                          earpieceDetecion;
    int                                                          maxAudioInput;
    int                                                          CM3_status;
    int                                                          CM3_tempValue1;
    int                                                          CM3_tempValue2;
    int                                                          is_CFX_started;
    int                                                          is_enabled_CFX_iteration;

    /* 일시: 2026-01-20
     * 작성: 김은수
     * 내용: 자극 묵음 기능의 활성화/비활성화 여부를 제어할 수 있도록 구현 */
    int is_enabled_mute_stimulation_under_t_level;
    int mute_stimulation_t_level_offset;

    // 일시: 2026-02-24
    // 작성: 김은수
    // 내용: PCM의 SpecificCommand 읽기를 어디까지 했는지 표시하는 플래그와 인덱스
    int is_pcm_specific_command_reading;
    int pcm_specific_command_read_index;

    /* 일시: 2026-07-20
     * 작성: 김은수
     * 내용: Gain Conversion Table 적용. 앱 0x8C 로 설정되는 게인 인덱스와 I2S 소스 종류.
     *       CM3 의 cfx_cm3_sharedMemory.h 와 필드 순서가 반드시 일치해야 한다. */
    int gain_table_index_a;    // 0~255, 마이크 경로 게인 (128 = 0 dB)
    int gain_table_index_b;    // 0~255, I2S 크래들 마이크 경로 게인 (128 = 0 dB)
    int is_i2s_source_cradle;  // 1 = Mic (Case), 0 = Streaming 또는 미연결
} ST__CFX_CM3_SharedMemory_ALL;

#ifdef CFX_CORE
#if (CFX_CORE == 1)
#define Addr_SharedMem ((volatile ST__CFX_CM3_SharedMemory_ALL chess_storage(IOMEM) *) 0x28000)
#else
#error "CFX_CORE is defined, but value is not 1....."
#endif
#else
#define CFX_CM3_SHARED_MEMORY_BASE_ADDR DSP_PRAM5_REMAP_BASE
#endif

#endif
