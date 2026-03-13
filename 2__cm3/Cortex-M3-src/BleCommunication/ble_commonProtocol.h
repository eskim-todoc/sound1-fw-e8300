
#ifndef BLE_REMOTECONTROL_POROTOCOL_H__
#define BLE_REMOTECONTROL_POROTOCOL_H__

#define BLE_DataPacketSize 20
#define todoc_PayloadSize  19  // 20바이트 중, 1개는 헤더, 19개는 데이터

typedef enum
{
    en__PAYLOAD_ON = 1,
    en__PAYLOAD_OFF
} EN__PAYLOAD_ON_OFF;

typedef enum
{
    en__PAYLOAD_INCREASE = 1,
    en__PAYLOAD_DECREASE
} EN__PAYLOAD_INCREASE_DECREASE;

typedef enum
{
    en__DONE_OK        = 1,
    en__ERROR_Response = 0xF0
} EN__PAYLOAD_RESULT;

typedef enum
{
    en__bleSetting_IDLE                   = 0,
    en__bleSetting_ReadConnected_ISD_info = 0x30,
} ReadCommandForBleSetting;

typedef enum
{

    en__BLE_COMM_COMMAND_connecteLogDate = 0x31,
    en__BLE_Disconnected_Flag            = 0x32,
    en__BLE_COMM_COMMAND_REPlY_ERROR_BLE = 0xF0,
} BLE_CMMUNICATION_COMMAND;

typedef enum
{
    EN__SND_BT_CMD_SYSTEM_INFO_BATTERY = 0x33,
    EN__SND_BT_CMD_SYSTEM_INFO_POWER   = 0x34,
} EN__SND_BT_CMD_SYSTEM_INFO;

typedef enum
{
    en__remoteControl_IDLE              = 0,
    en__remoteControl_check_isd_passKey = 0x40,              // 40 (리모콘 사용)
    en__remoteControl_readInfoOfExtenalDevice,               // 41 (리모콘 사용) // 확인 완료: nRF 자체 처리
    en__remoteControl_readInfoOfMap,                         // 42
    en__remoteControl_readStatusOfExtenalDevice,             // 43 (리모콘 사용) // 확인 완료: 헤더 only 패킷
    en__remoteControl_changeMapNum,                          // 44 (리모콘 사용) // 확인 완료: 1~2 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_adjustStimulationVolume,               // 45 (리모콘 사용) // 확인 완료: 1~2 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_adjustMicVolume,                       // 46 (리모콘 사용) // 확인 완료: 1~2 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_OnOffTelecoil,                         // 47 (리모콘 사용) // 확인 완료: 1~2 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_OnOffStimulationIndicator,             // 48 (리모콘 사용) // 확인 완료: 1~2 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_OnOffLED,                              // 49 (리모콘 사용) // 확인 완료: 1~2 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_read_SlotData_ISD_N_USER,              // 4A (리모콘 사용) // 확인 완료: 1~4 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__remoteControl_read_Mapdata_STIMUL_PARA,              // 4B
    en__remoteControl_write_SlotData_ISD_N_USER,             // 4C
    en__remoteControl_write_Mapdata_STIMUL_PARA,             // 4D
    en__remoteControl_erase_SlotData,                        // 4E
    en__remoteControl_erase_mapData_STIMUL_PARA,             // 4F
    en__remoteControl_erase_mppingData_exceptSlot_1,         // 50
    en__remoteControl_read_OwnerNameOfExternalDevice,        // 51
    en__remoteControl_readSoundSignal,                       // 52
    en__remoteControl_read_NRF_FirmwareInfo,                 // 53
    en__remoteControl_read_Ezairo_FirmwareInfo,              // 54
    en__remoteControl_write_SerialNumOfExternalDevice,       // 55 (리모콘 사용) // 확인 완료: nRF 자체 처리
    en__remoteControl_write_ParingKey,                       // 56 (리모콘 사용) // 확인 완료: nRF 자체 처리
    en__remoteControl_recover_ALL_SlotData_ManufactureData,  // 57
    en__remoteControl_readSystemErrorCode,                   // 58 (리모콘 사용) // NOTE: 실제 사용되고 있나? 이거 보내면 묵묵 부답으로 시간 초과 타임아웃 발생함

    /* 일시: 2026-01-20
     * 작성: 김은수
     * 내용: 자극 묵음 기능의 활성화/비활성화 여부를 제어할 수 있도록 구현 */
    en__remoteControl_specificSystemOperationSetting,  // 59 (리모콘 사용)
    en__remoteControl_waiting_for_BleOff
} EN__REMOTE_CONTROL_COMMAND;

typedef enum
{
    en__mapping_IDLE            = 0,
    en__mapping_ble_disconneted = 0x10,
    en__mapping_connect         = 0x60,                // 60 (리모콘 사용) // 확인 완료: 헤더 only 패킷
    en__mapping_disconnect,                            // 61 // 확인 완료: 헤더 only 패킷
    en__mapping_impedanceChekck,                       // 62 // 확인 완료: 각 멤버의 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__mapping_eCAP_Measurement_masking,              // 63 // 현재 버전 매핑 앱에서 사용 X
    en__mapping_eCAP_Measurement_alternative,          // 64 // 현재 버전 매핑 앱에서 사용 X
    en__mapping_specific_stimulation,                  // 65 // 확인 완료: 각 멤버의 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__mapping_live_stimulation,                      // 66 // 확인 완료: 각 하위 명령 및 데이터 인덱스의 멤버의 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__mapping_deviceStatus,                          // 67 // 환인 완료: 헤더 only 패킷
    en__mapping_write_original_ISD_N_USER,             // 68 // 확인 완료: 수술 위치 1~2 범위 확인 하는 것으로 처리, 다른 멤버는 범위 없음
    en__mapping_read_original_ISD_N_USER,              // 69 // 확인 완료: 헤더 only 패킷
    en__mapping_read_SlotData_ISD_N_USER,              // 6A // 확인 완료: 슬롯 번호 1~4 범위 확인하는 것으로 처리
    en__mapping_read_Mapdata_STIMUL_PARA,              // 6B // 확인 완료: 각 멤버의 범위 외에는 en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__mapping_write_SlotData_ISD_N_USER,             // 6C (리모콘 사용) // 내부기 및 사용자 성보, 설정값 // 확인 완료: 각 데이터 인덱스의 각 멤버의 범위 외에는
                                                       // en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__mapping_write_Mapdata_STIMUL_PARA,             // 6D (리모콘 사용) // 맵데이터 쓰기 // 확인 완료: 각 데이터 인덱스의 각 멤버의 범위 외에는
                                                       // en__EN__BLE_PROTOCOL_ERROR, en__OutOfDataRange 처리
    en__mapping_erase_SlotData_manufacture,            // 6E (리모콘 사용) // 맵 삭제 // 확인 완료: 슬롯 번호 1~4, 이 외에는 en__EN__BLE_PROTOCOL_ERROR,
                                                       // en__OutOfDataRange 처리
    en__mapping_erase_mapData_STIMUL_PARA,             // 6F (리모콘 사용) // 확인 완료: 슬롯 번호 1~4, 맵 번호 1~4, 이 외에는 en__EN__BLE_PROTOCOL_ERROR,
                                                       // en__OutOfDataRange 처리
    en__mapping_recover_mppingData_exceptSlot_1,       // 70 // 확인 완료: 헤더 only 패킷
    en__mapping_recover_ALL_SlotData_ManufactureData,  // 71 (리모콘 사용) // 공장 초기화 // 확인 완료: 헤더 only 패킷
    en__mapping_waiting_for_BleOff,
    en__mapping_testStimulation = 0x90,
    en__mapping_read_Connected_ISD_id
} EN__MAPPING_COMMAND;

#define Max_ImpedanceReturnDataSize              4  // b
#define PayloadSize_ExternalDeviceInfo_Size_byte 8  // 8byte
#endif
