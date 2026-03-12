
#ifndef CFX_CM3_SharedMemory_ADDR_H___
#define CFX_CM3_SharedMemory_ADDR_H___

#include <hw.h>

#include "99_eeprom_address.h"       //ok
#include "definitionsForAlgorithm.h" //ok
#include "processorDirective.h"      //ok

// CM3 DRAM에 cfx-cm3 공유메모리를 할당해서 사용한다.

// CM3의 데이터 메모리 주소를 확인한 값이다.
// CM3 데이터 메모리 시작 주소는 0x2000,0000 이며, 동일한 위치가 CFX에서는 0x4000의 주소로 접근된다.

#define CM3_DataMemoryBaseAddr      0x20000000
#define CFX_AccessAddrForCM3DataMem 0x4000

// CM3_ELF 파일을 복고 시작 주소를 확인한다.

// #define StartAddressCM3_sharedVarialbe 0x20001578 // ELF 파일에서 cfx_cm3_sharedMemoryAll의 주소값을 확인하여 기록한다.
#define StartAddressCM3_sharedVarialbe DSP_PRAM5_REMAP_BASE // __KIM: LPDSP32_PRAM5를 사용한다. Cortex-M3 Remapping 주소는 0x70000 값이다. sections.ld 파일도 수정하였음 유념할 것.

#define BaseAddr_CM3CFX_SharedVariable (StartAddressCM3_sharedVarialbe - CM3_DataMemoryBaseAddr) // 0x1b8// 0x1a4//0x190 //0x3c //0x30// 0x59c // 0x5a4// 0x14//  0x5a4//  0x1c
// #define BaseAddr_CM3CFX_SharedVariable              0x1b0//0x1b8// 0x1a4//0x190 //0x3c //0x30// 0x59c // 0x5a4// 0x14//  0x5a4//  0x1c

#define Addr_SharedMem_CFX_EEPROM_data_is_Loaded (CFX_AccessAddrForCM3DataMem + (BaseAddr_CM3CFX_SharedVariable / 4))

#define Addr_SharedMem_CFX_ERRORCODE (Addr_SharedMem_CFX_EEPROM_data_is_Loaded + 1)

#define Addr_SharedMem_ST__USB_CONNECTOR_STATE   (Addr_SharedMem_CFX_ERRORCODE + 1)
#define Addr_SharedMem_ChargerConnectorPluggedIn (Addr_SharedMem_ST__USB_CONNECTOR_STATE)
#define Addr_SharedMem_CarryingCasePluggedIn     (Addr_SharedMem_ChargerConnectorPluggedIn + 1)
#define Addr_SharedMem_CarryingCaseCoverOpen     (Addr_SharedMem_CarryingCasePluggedIn + 1)

#define Addr_SharedMem_PCM_Mode                    (Addr_SharedMem_CFX_ERRORCODE + 4)
#define Addr_SharedMem_PCM_liveStimulationTemplate (Addr_SharedMem_PCM_Mode + 1)
#define Addr_SharedMem_PCM_SpecificBuffer          (Addr_SharedMem_PCM_liveStimulationTemplate + df_MaxNumTransferableChannel)
#define Addr_SharedMem_PCM_Mode_next               (Addr_SharedMem_PCM_SpecificBuffer + df_MaxNumTransferableChannel)
#define Addr_SharedMem_PCM_ResetCommand            (Addr_SharedMem_PCM_Mode_next + 1)
#define Addr_SharedMem_conneded_ISDCheckPCM_state  (Addr_SharedMem_PCM_ResetCommand + 1)

// OK
#define Addr_SharedMem_CM3_i2c_State            (Addr_SharedMem_conneded_ISDCheckPCM_state + 1)
#define Addr_SharedMem_CM3_i2C_command_RW       (Addr_SharedMem_CM3_i2c_State + 1)
#define Addr_SharedMem_CM3_i2C_slaveAddr        (Addr_SharedMem_CM3_i2C_command_RW + 1)
#define Addr_SharedMem_CM3_i2C_command_dataSize (Addr_SharedMem_CM3_i2C_slaveAddr + 1)
#define Addr_SharedMem_CM3_i2C_txBuffer         (Addr_SharedMem_CM3_i2C_command_dataSize + 1)
#define Addr_SharedMem_CM3_i2C_rxBuffer         (Addr_SharedMem_CM3_i2C_txBuffer + df_i2c_Tx_buffer_size)

#define Addr_SharedMem_Connected_ISD_Num (Addr_SharedMem_CM3_i2C_rxBuffer + df_i2c_Rx_buffer_size)
// OK

#define Addr_SharedMem_Connected_ISD_mapStamp                (Addr_SharedMem_Connected_ISD_Num + 1)
#define Addr_SharedMem_Connected_ISD_mapStamp_nmuOfMap       (Addr_SharedMem_Connected_ISD_mapStamp + 6)
#define Addr_SharedMem_Connected_ISD_mapStamp_usableMapIndex (Addr_SharedMem_Connected_ISD_mapStamp_nmuOfMap + 1)
#define Addr_SharedMem_Connected_ISD_mapStamp_mapDate_1stMap (Addr_SharedMem_Connected_ISD_mapStamp_usableMapIndex + 4)
#define Addr_SharedMem_Connected_ISD_mapStamp_mapDate_2ndMap (Addr_SharedMem_Connected_ISD_mapStamp_mapDate_1stMap + 6)
#define Addr_SharedMem_Connected_ISD_mapStamp_mapDate_3rdMap (Addr_SharedMem_Connected_ISD_mapStamp_mapDate_2ndMap + 6)
#define Addr_SharedMem_Connected_ISD_mapStamp_mapDate_4thMap (Addr_SharedMem_Connected_ISD_mapStamp_mapDate_3rdMap + 6)

#define Addr_SharedMem_ISD_info (Addr_SharedMem_Connected_ISD_mapStamp + 35)

#define Addr_SharedMem_UserSettingValue                  (Addr_SharedMem_ISD_info + (df_24bitWordLength_ISD_info * MaxNumUser))
#define Addr_SharedMem_UserSetting_MapNumber             Addr_SharedMem_UserSettingValue
#define Addr_SharedMem_UserSetting_StimulationVolume     (Addr_SharedMem_UserSettingValue + 1)
#define Addr_SharedMem_UserSetting_AudioVolume           (Addr_SharedMem_UserSettingValue + 2)
#define Addr_SharedMem_UserSetting_Indicator_OnOff       (Addr_SharedMem_UserSettingValue + 3)
#define Addr_SharedMem_UserSetting_IndicatorStimul_OnOff (Addr_SharedMem_UserSettingValue + 4)
#define Addr_SharedMem_UserSetting_TeleCoil_OnOff        (Addr_SharedMem_UserSettingValue + 5)
#define Addr_SharedMem_UserSetting_BLE_OnOff             (Addr_SharedMem_UserSettingValue + 6)

#define Addr_SharedMem_UserSettingValueRoadedFlag (Addr_SharedMem_UserSettingValue + 7)

#define Addr_SharedMem_cuurentMapData                       (Addr_SharedMem_UserSettingValueRoadedFlag + 1)
#define addr_SharedMem_mappingDate                          Addr_SharedMem_cuurentMapData
#define Addr_SharedMem_stimulationStrategy                  addr_SharedMem_mappingDate + 6
#define Addr_SharedMem_firstPulsePhase                      (Addr_SharedMem_cuurentMapData + 7)
#define Addr_SharedMem_stimulationMode                      (Addr_SharedMem_cuurentMapData + 8)
#define Addr_SharedMem_stimulationPulsePhaseWidth           (Addr_SharedMem_cuurentMapData + 9)
#define Addr_SharedMem_numFrequencyBand                     (Addr_SharedMem_cuurentMapData + 10)
#define Addr_SharedMem_stimulationIndicatorChannelNum       (Addr_SharedMem_cuurentMapData + 11)
#define Addr_SharedMem_stimulationIndicatorAmplitude_255    (Addr_SharedMem_cuurentMapData + 12)
#define Addr_SharedMem_usableStimulationElectrodIndex_array (Addr_SharedMem_cuurentMapData + 13)
#define Addr_SharedMem_usableReferenceElectrodIndex_array   (Addr_SharedMem_usableStimulationElectrodIndex_array + 1 * df_MaxNumOfElectrode)
#define Addr_SharedMem_CIS_FreqBandOrder_array              (Addr_SharedMem_usableStimulationElectrodIndex_array + 2 * df_MaxNumOfElectrode)
#define Addr_SharedMem_T_level_uA_array                     (Addr_SharedMem_usableStimulationElectrodIndex_array + 3 * df_MaxNumOfElectrode)
#define Addr_SharedMem_C_level_uA_array                     (Addr_SharedMem_usableStimulationElectrodIndex_array + 4 * df_MaxNumOfElectrode)
#define Addr_SharedMem_audio_input_x_mim_array              (Addr_SharedMem_usableStimulationElectrodIndex_array + 5 * df_MaxNumOfElectrode)
#define Addr_SharedMem_audio_input_x_max_array              (Addr_SharedMem_usableStimulationElectrodIndex_array + 6 * df_MaxNumOfElectrode)

#define Addr_SharedMem_T_level_255_array    (Addr_SharedMem_cuurentMapData + (13 + 7 * df_MaxNumOfElectrode))
#define Addr_SharedMem_C_level_255_array    (Addr_SharedMem_cuurentMapData + (13 + 8 * df_MaxNumOfElectrode))
#define Addr_SharedMem_frameNumPerChannel   (Addr_SharedMem_cuurentMapData + (13 + 9 * df_MaxNumOfElectrode))
#define Addr_SharedMem_transferableFrameNum (Addr_SharedMem_cuurentMapData + (13 + 9 * df_MaxNumOfElectrode) + 1)

#define Addr_SharedMem_indcatorStimulationLevel_255 (Addr_SharedMem_cuurentMapData + (13 + 9 * df_MaxNumOfElectrode) + 2)
#define Addr_SharedMem_indicatorStimulOutput_OnOff  (Addr_SharedMem_cuurentMapData + (13 + 9 * df_MaxNumOfElectrode) + 3)

#define Addr_SharedMem_currentOutputStimulLevel_255_array (Addr_SharedMem_cuurentMapData + (13 + 9 * df_MaxNumOfElectrode) + 4)

#define Addr_SharedMem_Cm3_Command_mapChange      (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 4)
#define Addr_SharedMem_cfx_Reloaded_MapdataFlage  (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 5)
#define Addr_SharedMem_cm3_calculatedParameteFlag (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 6)

// system share
#define Addr_SharedMem_System_opMode                              (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 7)
#define Addr_SharedMem_powerButton_pushed_CFX_to_CM3              (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 8)
#define Addr_SharedMem_batteryLevel_CfX_to_CM3                    (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 9)
#define Addr_SharedMem_consumptionPowerControl_Command_CM3_to_CFX (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 10)
#define Addr_SharedMem_enter_ULP_mode_Command_CM3_to_CFX          (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 11)

#define Addr_SharedMem_ReadWriteCommand_command_ForFlash         (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 12)
#define Addr_SharedMem_ReadWriteCommand_isd_index_ForFlash       (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 13)
#define Addr_SharedMem_ReadWriteCommand_map_index_ForFlash       (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 14)
#define Addr_SharedMem_RepositoryForReadWriteMapData_ISD_info    (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 15)
#define Addr_SharedMem_RepositoryForReadWriteMapData_usersetting (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 15 + df_24bitWordLength_ISD_info)
#define Addr_SharedMem_RepositoryForReadWriteMapData_mapData     (Addr_SharedMem_cuurentMapData + (13 + 10 * df_MaxNumOfElectrode) + 15 + df_24bitWordLength_ISD_info + df_24bitWordLength_UserSettingValues)

#define Addr_SharedMem_batteryCalibarationValue (Addr_SharedMem_RepositoryForReadWriteMapData_mapData + df_24bitWordLength_ProgramData)
#define Addr_SharedMem_backtelControlRegister   (Addr_SharedMem_batteryCalibarationValue + 1)
#define Addr_SharedMem_isMappingProgramConneted (Addr_SharedMem_backtelControlRegister + 1)
#define Addr_SharedMem_earpieceDetection        (Addr_SharedMem_isMappingProgramConneted + 1)
#define Addr_sharedMem_maxAudioInput            (Addr_SharedMem_earpieceDetection + 1)
#define Addr_sharedMem_CM3_Status               (Addr_sharedMem_maxAudioInput + 1)
#define Addr_sharedMem_CM3_tempValue1           (Addr_sharedMem_CM3_Status + 1)
#define Addr_sharedMem_CM3_tempValue2           (Addr_sharedMem_CM3_Status + 2)

#endif
