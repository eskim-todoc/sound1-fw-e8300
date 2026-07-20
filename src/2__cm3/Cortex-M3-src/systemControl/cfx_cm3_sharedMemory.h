/* ============================================================================
 * [공유 ABI - 단독 rename/제거 금지] (2026-07-20, cm3 전면 리팩토링 G0)
 *
 * 이 헤더의 타입 19종과 구조체 필드 105종은 CFX(1__cfx/environment/
 * shared_memory.h)와 calibration(5__calibration/include/cfx_cm3_sharedMemory.h)
 * 이 같은 레이아웃을 복제해 사용하는 프로세서 간 공유 인터페이스다.
 *
 *  - 필드/타입의 제거·순서 변경 = 레이아웃(ABI) 파괴 -> CFX 오동작
 *  - 2__cm3 단독 rename = 세 헤더의 소스 불일치 -> 유지보수 파괴
 *  - 미사용으로 보이는 필드도 존치한다 (은수님 지시, 2026-07-20)
 *
 * 변경이 필요하면 세 프로젝트 헤더를 동시에 바꾸는 별도 작업으로 진행할 것.
 * 목록: docs/tasks/cm3/20260720_cm3-full-refactor/분석-데이터/06_공유-인터페이스.md
 * ========================================================================== */

#ifndef cfx_cm3_sharedMemory_H__
#define cfx_cm3_sharedMemory_H__

#include <stdbool.h>

#include <hw.h>
#include <driver_PCM.h>  //ok

#include "ble_commonProtocol.h"       //ok
#include "board.h"                    //ok
#include "definitionsForAlgorithm.h"  //ok
#include "internalStimulationChip.h"  //ok

#include "tdc_printf.h"

// #include <OTE_1_5_gen_CFX_EEPROM_erase.h>
// #include <OTE_1_5_gen_CFX_EEPROM_read.h>
// #include <OTE_1_5_gen_CFX_EEPROM_write.h>

// #define CFX_CM3_SHARED_MEMORY_BASE_ADDR DSP_PRAM5_REMAP_BASE

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
    en__systemReset
} EN__SYSTEM_OP_MODE;

typedef struct
{
    EN__SYSTEM_OP_MODE system_opMode;  // 현재 사용되는 곳 없음. 지울 것. 지우면  공유메모리 주소도 변경해야 함
    int                powerButton_pushed_CFX_to_CM3;
    int                batteryLevel_CfX_to_CM3;
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
    int CFX_EEPROM_data_is_Loaded;
    int CFX_ErrorCode;

    ST__USB_CONNECTOR chargerState;

    CFX_CM3_SharedMemory_PCM_interface cfx_PCM_interface;

    ST__CFX_CM3_SharedMemory_CFX_i2c_interface cfx_i2c_interface;

    int connected_ISD_num;  // 0연결안됨, 1~4 연결된 ISD 번호

    ST__CFX_CM3_SharedMemory_connected_ISD_Map_info connected_ISD_Map_info;

    ST__CFX_CM3_SharedMemory_ISD_info cfx_ISD_info[MaxNumUser];

    ST__CFX_CM3_SharedMemory_userSettingValue userSettingValue;

    int userSettingValueLoadedFlag;

    ST__CFX_CM3_SharedMemory_mapData currentMapData;

    ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3          calculatedStimulPara_byCM3;
    ST__CFX_CM3_SharedMemory_calculatedStimulationIndcator_byCM3 calculatedStimulationIndcator_byCM3;
    int                                                          currentOutputStimulLevel_255[df_MaxNumOfElectrode];

    ST__CFX_CM3_SharedMemory_MapDataChange                 mapChangeFlag;
    ST__CFX_CM3_SharedMemory_System                        systemShare;
    ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash     ReadWriteCommand_ForFlash;
    ST__CFX_CM3_SharedMemory_RepositoryForReadWriteMapData repositoryForReadWriteMapData;
    int                                                    batteryCalibrationValue;
    int                                                    backtelControlRegister;
    int                                                    isMappingProgramConneted;  // 사용안함 지울 것
    int                                                    earpieceDetecion;
    int                                                    maxAudioInput;
    int                                                    CM3_status;
    int                                                    CM3_tempValue1;
    int                                                    CM3_tempValue2;

    int is_CFX_started;            // NOTE: 추가함
    int is_enabled_CFX_iteration;  // NOTE: 추가함

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
} ST__CFX_CM3_SharedMemory_ALL;

/* CM3 생존 신호 게시 (구 update_CM3Status_toCFX).
 * 읽는 코드는 없고 디버거 관측용이다 - 상세는 정의부 주석 참조.
 * 구 update_CM3tempValue1/2_toCFX 래퍼는 호출처 0 으로 제거됨(2026-07-20).
 * 해당 '필드'는 CFX AGC 가 사용하므로 구조체에 그대로 있다. */
void tdc_shared_publish_cm3_heartbeat(int beat);

bool isCFX_EEPROM_data_Loaded(void);

////
// 공유 메모리 주속 확인

bool sharedMemoryAddresError(void);

// 충전기 상태

/* readUsbConnectorState() 제거(2026-07-15): Sullivan 유산.
 * 충전 상태는 QCC 0x34 기반 snd_charger_get_state() 로 단일화됐다. */

////////////////////////////////////
// PCM 관련

void changePcmOutputMode(int currentPcmOutputMode);
int  readCurrentPcmOutputMode(void);
void changeNextPcmOutputMode(int nextPcmOutputMode);
void fillSepcificCommndBuffer(int Index, int data);
int  readConnectionCheckPcmState(void);
void clearConnectionCheckPcmFiredFlag(void);

///////////////////////////////
// 내부기 제조 정보 읽기

int read_ISD_manufacture_ID(int index);

///////////////////////////////
// 내부기 수술 위치 읽기

///////////////////////////////
// 내부기 사용자 이름 읽기

int *read_recomcon_passkey_connected_ISD(int connected_ISD_Num);

// 맵 스템프
int *readConnected_ISD_MapStamp(void);

///////////////////
// 내부기 정보
void changeConnected_isd_num_CFX(int num);
int  read_connected_ISD_Num(void);

// 연렫된 내부기  환자 이름
int *readConnected_ISD_userName(int connected_ISD_Num);

// 연렫된 내부기  수술 위치
EN__LOCATION_OF_ISD readConnected_ISD_Location(int connected_ISD_Num);

// 열결된 내부기의  맵 요약 정보

// 열결된 내부기의  사용 가능한 맵 갯수
int readConnected_ISD_usableMapNum(void);

// 열결된 내부기의  사용 가능한 맵 인덱스
int *readConnected_ISD_usableMapIndex(void);

// 열결된 내부기의  스템프
int *readConnected_ISD_MapStamp(void);
// 열결된 내부기의  맵 생성일자 들
int *readConnected_ISD_MapDate(int mapNum);

//////////////////////////
// 사용자 설정값

void changeProgramMapNum(int mapNum);
int  readProgramMapNum(void);
void changeStimulVolume(int volume);
int  readStimulVolume(void);
void changeAudioVolume(int volume);
int  readAudioVolume(void);

void changeLED_indicatorOnOff(EN__PAYLOAD_ON_OFF OnOff);
int  readLED_indicatorOnOff(void);
void changeTeleCoil_OnOff(EN__PAYLOAD_ON_OFF OnOff);
int  readTeleCoil_OnOff(void);
void changeStimulIndicator_OnOff(EN__PAYLOAD_ON_OFF OnOff);
int  readStimulIndicator_OnOff(void);

// 사용자 설정값 읽어 들여짐 확인

bool isUserSettingValueLoaded_CFX(void);

/////////////////////
// 맵데이터

const ST__CFX_CM3_SharedMemory_mapData *readCurrentMapData(void);
ST__CFX_CM3_SharedMemory_mapData       *getPointerCurrentMapData(void);
void                                    setReadWriteMapDataFlashCommand(ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash command_ForFlash);
bool                                    isReadWriteMapDataFlashCommandDone(void);
int                                    *getPointerRepositoryForReadWriteMapData_isd_info(void);
int                                    *getPointerRepositoryForReadWriteMapData_userSetting(void);
int                                    *getPointerRepositoryForReadWriteMapData_stimulPara(void);

///////////////
// 자극에 필요한 추가 파라미터(CM3에서 계산되어 CFX에 전달되어야 한다.)

ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3 *getPointerCalculatedStimulPara_byCM3(void);

/////
/// CFX와의 통신
void setFlag_AudioParametersCalculationDone_Cm3ToCfx(void);  // 파라미터 계산이 완료되었을을 CFX에 알려주는 플레그 (CFX에서 확인 후 자동을 클리어)

/////////////
// 자극 알림

/*
int *getCalculatedStimulationIndcatorLevel_byCM3(void);
int *getPointer_stimulationIndcatorOnOff_byCM3(void);
*/

void setCalculatedStimulationIndcatorLevel_byCM3(int stimulationIndicatorLevel_255);
void setStimulationIndcatorOnOff_byCM3(bool On_Off);

/////////
// 현재 출력되고 있는 자극 레벨

int *readCurrentStimulLevel_255(void);
int  readAudioSignalMax(void);

/////
// 맵데이터 변경관련  CFX와의 통신

bool isMapdateLoaded_CFX(void);

// 맵 번호 변경 명령을 CFX에 전달
void setCommandMapChange_Cm3ToCfx(void);

// 매핑 프로그램 연결 상태 CFX에 전달.

void shareMappingProgramConnection(bool connection);

/////
// 동작 모드, 가속도 센서, 전원 off명령
void changeSystemModeFlag(EN__SYSTEM_OP_MODE flag);
bool isPowerButtonPushed(void);
void enterLowPowerMode_CM3_to_CFX();
void OnOff_3V_PMIC_CM3_to_CFX(bool OnOff);

void upadateSystemOpModeToCFX(EN__SYSTEM_OP_MODE mode);

int readBatteryLevel_FromCFX(void);

int readBatteryCalibrationValue(void);

void updagteBacktelControlValue_toCFX(int value);

void updateEarpieceDetectionValue_toCFX(bool detection);
///// CFX의 에러
int readCfxErrorCode(void);


// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

#endif
