#ifndef ERRROR_H__
#define ERRROR_H__

#include <stdbool.h>

#include "mappingControl.h"

// 에러 코드 분류 및 배열 재 정립 필요

typedef enum
{
    /* CFX 측 에러 코드 값과 정합 (구 99_errorCode.h - 포함처 0 으로 삭제(2026-07-20).
     * 원 내용: NoError=0, clock_init_Error=1) */
    en__ClockError = 1,
} EN__CFX_ERROR;

typedef enum
{
    en__unusableMapData              = 1,
    en__stimulationParameterUnloaded = 2,
    en__MaxChargeOver_mapdata,
    en_sourceCodeError
} EN__DATAPROCESSING_ERROR;

typedef enum
{

    en__ACCELER_ResetValueError = 1,
    en__I2C_ACCELER_WritingError,
    en__I2C_ACCELER_ReadingError,

} EN__ACCELEROMETER_ERROR;

typedef enum
{

    en__NON_RESETTABLE = 1,
    en__TxPower_ResetValue_Error,
    en__writtenReadVlaueIsNotSame,
    en__I2C_RFPOW_WritingError,
    en__I2C_RFPOW_ReadingError

} EN__RF_PowerIC_ERROR;

typedef enum
{

    en__FPGA_ResetValueError = 1,
    en__I2C_FPGA_WritingError,
    en__I2C_FPGA_ReadingError,

} EN__FPGA_COMMUNICATION_ERROR;

typedef enum
{

    en_RegisterConfigError_byPCM = 1,
    en_RF_Tx_enableError,
    en_BackTelDecodingCalibation_error,
    en_FIFO_NotCleared,
    en_PulseWidthDifferent

} EN__FPGA_CONFIGUARATION_ERROR;

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

} EN__ISD_ERROR;

typedef enum
{

    en__PreviouCommnadIsNotCompleted = 1,
    en__NO_SECURITY,
    en__Command_Order,
    en__DATA_Order,
    en__UndefinedCommand,
    en__OutOfDataRange
} EN__BLE_PROTOCOL_ERROR;

typedef enum
{
    en__PCMBufferOwerFlow = 1,
    en__PCM_TempleteBuff_OverFlow

} EN__PCM_GEN_ERROR;

typedef enum
{
    en__data_logging_error_open = 1,
    en__data_logging_error_write,
    en__data_logging_error_not_inited,
    en__data_logging_error_currupted
} EN__DATA_LOGGING_ERROR;

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

} EN__MAJOR_ERRORCODE;

typedef struct
{
    // EN__CFX_ERROR CFX_ErrorFlag;
    EN__DATAPROCESSING_ERROR      dataProcessingErrorFlag;
    EN__ACCELEROMETER_ERROR       accelerometerErrorFlag;
    EN__RF_PowerIC_ERROR          PowerIcErrorFlag;
    EN__FPGA_COMMUNICATION_ERROR  FPGA_CommunicationErrorFlag;
    EN__FPGA_CONFIGUARATION_ERROR FPGA_ConfiguraionErrorFlag;
    EN__ISD_ERROR                 ISD_ErrorFlag;
    int                           FPGA_OR_ISD_ErrorFlag;
    int                           FPGA_systemError;    // FPGA의 레지스터 값과 동일하게 유지
    int                           FPGA_backtelError;   // FPGA의 레지스터 값과 동일하게 유지
    EN__DATA_LOGGING_ERROR        data_logging_error;  // NOTE: 사이버 보안을 고려하여, 검사 기록 생성 실패에 대한 오류 정보 추가

} ST__ERROR_CODE;

void update_FPGA_systemError(int value);

void update_FPGA_backtelError(int value);

void clearErrorFlag(EN__MAJOR_ERRORCODE majorError);

void clearAllErrorFlag(void);

void errorCodeUpdate(EN__MAJOR_ERRORCODE majorError, int detailError, int lineNumber);

ST__ERROR_CODE readErrorCode(void);

void sendErrorToApp(EN__MAPPING_COMMAND command, EN__MAJOR_ERRORCODE majorError, int minorError, int lineNumber);

#endif
