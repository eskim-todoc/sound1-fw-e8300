#ifndef __tdc_sys_error_h__
#define __tdc_sys_error_h__

#include <stdbool.h>

#include "mappingControl.h"

// 에러 코드 분류 및 배열 재 정립 필요

typedef enum
{
    /* CFX 측 에러 코드 값과 정합 (구 99_errorCode.h - 포함처 0 으로 삭제(2026-07-20).
     * 원 내용: NoError=0, clock_init_Error=1) */
    en__ClockError = 1,
} tdc_sys_error_cfx_error_t;

typedef enum
{
    en__unusableMapData              = 1,
    en__stimulationParameterUnloaded = 2,
    en__MaxChargeOver_mapdata,
    en_sourceCodeError
} tdc_sys_error_dataprocessing_error_t;

typedef enum
{

    en__ACCELER_ResetValueError = 1,
    en__I2C_ACCELER_WritingError,
    en__I2C_ACCELER_ReadingError,

} tdc_sys_error_accelerometer_error_t;

typedef enum
{

    en__NON_RESETTABLE = 1,
    en__TxPower_ResetValue_Error,
    en__writtenReadVlaueIsNotSame,
    en__I2C_RFPOW_WritingError,
    en__I2C_RFPOW_ReadingError

} tdc_sys_error_rf_poweric_error_t;

typedef enum
{

    en__FPGA_ResetValueError = 1,
    en__I2C_FPGA_WritingError,
    en__I2C_FPGA_ReadingError,

} tdc_sys_error_fpga_communication_error_t;

typedef enum
{

    en_RegisterConfigError_byPCM = 1,
    en_RF_Tx_enableError,
    en_BackTelDecodingCalibation_error,
    en_FIFO_NotCleared,
    en_PulseWidthDifferent

} tdc_sys_error_fpga_configuaration_error_t;

typedef enum
{
    en__ISD_notConnected = 1,
    en__ISD_EEPROM_ValueZero,
    en__BackTelCounterZero,
    en__BackterDataLengthError,
    en__ISD_Power_Low,
    en__ISD_Power_Unstable,
    en__No_Matched_ISD_ID,
    en__VTG_Lock_Error,
    en__MaxChargeOver,
    en__MaskerProbe_InterVal_tooShort,
    en__MaskerProbe_InterVal_tooLong,
    en__stimulLevelOver,
    en__SettingError_StimulatonPara,
    en__SettingError_OffsetDAC_Level,
    en__SettingError_BipolarElectrodeNum

} tdc_sys_error_isd_error_t;

typedef enum
{

    en__PreviouCommnadIsNotCompleted = 1,
    en__NO_SECURITY,
    en__Command_Order,
    en__DATA_Order,
    en__UndefinedCommand,
    en__OutOfDataRange
} tdc_sys_error_ble_protocol_error_t;

typedef enum
{
    en__PCMBufferOwerFlow = 1,
    en__PCM_TempleteBuff_OverFlow

} tdc_sys_error_pcm_gen_error_t;

typedef enum
{
    en__data_logging_error_open = 1,
    en__data_logging_error_write,
    en__data_logging_error_not_inited,
    en__data_logging_error_currupted
} tdc_sys_error_data_logging_error_t;

typedef enum
{
    en__NA = 0,                             // 0
    en__CFX_ERROR,                          // 1
    en__dataProcessing_ERROR,               // 2
    en__ACCELEROMETER_ERROR,                // 3
    en__RF_PowerIC_ERROR,                   // 4
    en__FPGA_COMMUNICATION_ERROR,           // 5
    en__FPGA_CONFIGUARATION_ERROR,          // 6
    en__EN__ISD_ERROR,                      // 7
    en__EN__BLE_PROTOCOL_ERROR,             // 8
    en__EN__PCM_GEN_ERROR,                  // 9
    en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR  // 10

} tdc_sys_error_major_t;

typedef struct
{
    // tdc_sys_error_cfx_error_t CFX_ErrorFlag;
    tdc_sys_error_dataprocessing_error_t      dataProcessingErrorFlag;
    tdc_sys_error_accelerometer_error_t       accelerometerErrorFlag;
    tdc_sys_error_rf_poweric_error_t          PowerIcErrorFlag;
    tdc_sys_error_fpga_communication_error_t  FPGA_CommunicationErrorFlag;
    tdc_sys_error_fpga_configuaration_error_t FPGA_ConfiguraionErrorFlag;
    tdc_sys_error_isd_error_t                 ISD_ErrorFlag;
    int                           FPGA_OR_ISD_ErrorFlag;
    int                           FPGA_systemError;    // FPGA의 레지스터 값과 동일하게 유지
    int                           FPGA_backtelError;   // FPGA의 레지스터 값과 동일하게 유지
    tdc_sys_error_data_logging_error_t        data_logging_error;  // NOTE: 사이버 보안을 고려하여, 검사 기록 생성 실패에 대한 오류 정보 추가

} tdc_sys_error_code_t;

void tdc_sys_error_update_fpga_system(int value);

void tdc_sys_error_update_fpga_backtel(int value);

void tdc_sys_error_clear_flag(tdc_sys_error_major_t majorError);

void tdc_sys_error_clear_all(void);

void tdc_sys_error_update(tdc_sys_error_major_t majorError, int detailError, int lineNumber);

tdc_sys_error_code_t tdc_sys_error_read(void);

void tdc_sys_error_send_to_app(EN__MAPPING_COMMAND command, tdc_sys_error_major_t majorError, int minorError, int lineNumber);

#endif
