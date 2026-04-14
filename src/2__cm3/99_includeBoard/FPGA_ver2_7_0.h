
#ifndef FPGA_ver1_2_4_H___
#define FPGA_ver1_2_4_H___

/// 레지스터 주소
#define df_I2C_Addr_ATMEGA 0x33

typedef enum
{
    i2cAddr_FPGA_version = 0x20,
    i2cAddr_FPGA_systemResgister_1st, // writable
    i2cAddr_FPGA_systemResgister_2nd, // writable
    i2cAddr_FPGA_error_Flag,
    i2cAddr_FPGA_backtel_ErrorFlag,
    i2cAddr_FPGA_pulsePhaseWidth, // writable
    i2cAddr_FPGA_FIFO_counter,
    i2cAddr_FGPA_IO_MUX,
    i2cAddr_FPGA_backtel_Config, // writable
    i2cAddr_FPGA_optional_Config,
    i2cAddr_FPGA_Backtel_FIFO = 0x30

} I2C_ADDR_FPGA;

typedef enum
{
    systemResgister_1st_WRITABLE_BIT  = 0x1E,
    systemResgister_2nd_WRITABLE_BIT  = 0x03,
    backterConfiguration_WRITABLE_BIT = 0x03

#if 0
            fpga_IO_MUX_Configuration_WRITABLE_BIT=0x3F
#endif
} WRITTEN_REGISTER_RW_BIT;

/// 레지스터  비트 위치
typedef enum
{
    FPGA_BitPosition_SystemError          = 7,
    FPGA_BitPosition_CarringCaseOpen      = 6,
    FPGA_BitPosition_CarringCaseConnecton = 5,
    FPGA_BitPosition_BatteryCharging      = 4,
    FPGA_BitPosition_EarPieceDetection    = 3,
    FPGA_BitPosition_BaktelDataDectection = 2,
    FPGA_BitPosition_TxEnable             = 1,
    FPGA_BitPosition_ResetFPGA            = 0
} FPGA_SYSTEM_REG_1_BIT_POSITION;

typedef enum
{
    FPGA_BitPosition_Clear_FIFO    = 7,
    FPGA_BitPosition_FIFO_is_FULL  = 6,
    FPGA_BitPosition_FIFO_is_empty = 5,

    FPGA_BitPosition_PCM_CAL = 0
} FPGA_SYSTEM_REG_2_BIT_POSITION;

typedef enum
{
    FPGA_BitPosition_error_clkRate  = 2,
    FPGA_BitPosition_error_SyncLost = 0
} FPGA_ERROR_FLAG_REG_BIT_POSITION;

typedef enum
{
    // PCM data bit position
    pcm_BitPosition_FPGA_backtel_calibration = 2,
    pcm_BitPosition_FPGA_backtel_bitLength   = 1,
    pcm_BitPosition_FPGA_backtel_OnOff       = 0
} FPGA_BACKTEL_BIT_POSITION;

typedef enum
{
    BitLenght_FPGA_backtel_calibration = 5,

} FPGA_BACKTEL_BIT_Length;

/// 레지스터  상태 값

typedef enum
{
    FPGA_Status__Reset_value         = 0X80,
    FPGA_Status__OK_DisabledRF_value = 0,
    FPGA_Status__NoError             = 0
} FPGA_SYSTEM_REG_1_SATATUS;

typedef enum
{
    FPGA_Status_FlagIndex = 0xB7,
} FPGA_SYSTEM_REG_1_Reset_Flag;

typedef enum
{
    FPGA_BitPosition_BACKTEL_NOP_ERROR   = 4,
    FPGA_BitPosition_CRC_CHECK_ERROR     = 3,
    FPGA_BitPosition_DECODING_ERROR      = 2,
    FPGA_BitPosition_PROTOCOL_ERROR      = 1,
    FPGA_BitPosition_BACKTEL_ABORT_ERROR = 0

} FPGA_BACKTEL_ERROR_FLAG_BIT_POSISTION;

typedef struct
{
    int systemResgister_1st_value;
    int systemResgister_2nd_value;
    int stimulation_PhaseDuration_value;
    // int fpga_IO_MUX_Configuration_value;
    int backterConfiguration_value;
    // int fpga_optional_configuration_value;

} LAST_WRITTEN_REGISTER;

typedef enum
{
    systemResgister_1st_resetValue       = 0x00,
    systemResgister_2nd_resetValue       = 0x3E,
    stimulation_PhaseDuration_resetValue = 0,
#if 0
            backterConfiguration_resetValue=0x31,           // 백텔 안들어옴 (32, 39, 59, 119, 120, 121), 백텔 값오류(190) 4월 4주차
#else

    // backterConfiguration_resetValue=0x39, //14
    // backterConfiguration_resetValue=0x35, //13
    // backterConfiguration_resetValue=0x31, //12
    backterConfiguration_resetValue = 0x2D, // 11
    // backterConfiguration_resetValue=0x29, //10        //  백텔 안들어옴 (32, 39, 59, 119, 120, 121), 백텔 값오류(190)
    // backterConfiguration_resetValue=0x25, //9
    // backterConfiguration_resetValue=0x21, //8
    // backterConfiguration_resetValue=0x1D, //7
    // backterConfiguration_resetValue=0x19, //6

#endif
    fpga_IO_MUX_Configuration_resetValue   = 0,
    fpga_optional_configuration_resetValue = 0

} FPGA_WRITABLE_REGISTER_RESET_VALUE;

//
#define FPGA_TxPower_Max       7
#define FPGA_TxPower_Min       0
#define FPGA_TxPower_initValue FPGA_TxPower_Max // 0~7

#define FPGA_BacktelCalibrationVal_Max 0x10
#define FPGA_BacktelCalibrationVal_Min 0x08

// 데이터 전송 관련
#define FPGA_oneChannelDataTokenTime         41 //  하나의 채널 데이터를  전송가능한 최대로 할당된 시간 단위  41.66usec ... 1ms 동안 24채널을
#define FPGA_electrodAndStimulLevelTokenTime 10 // 펄스폭을 제외한 자극파라미터(전극번호+자극크기)를 에러가 발생하지 않고 전송가능한 시간 단위 usec
#define FPGA_interphaseGapTokenTime          4  // 펄스폭을 최소로 했을 때 에러가 발생하지 않고 전송가능한 시간 단위 usec
#define FPGA_pulsePhaseWidth_minimum         13 // 최소 펄스폭 크기.

// PCM 통신으로 설정하는 값들

// 기타
#define df_FPGA_FIFO_buffSize 100
#define FIPGA_FIFO_index_0    0
#define FIPGA_FIFO_index_1    1



#endif
