

#ifndef DEFINITION_Preprocessor_H__
#define DEFINITION_Preprocessor_H__

//////////
// 실행 보드

// #define Board_is_TD_DEV_ver_1_4
#define Board_is_OTE_VER_1_5
// #define Board_is_OTE_VER_1_4
// #define Board_is_OTE_VER_1_3

///////////
// FPGA 버전
#define FPGA_Ver_is_270

///////////
// 내부이식장치 버전
// #define     ISD_Ver_is_111
#define ISD_Ver_is_112

// 디버깅 관련

#define DebuggerEnable  // 디버깅 장치의 접근을 허용.. 최종 제품에서는 삭제하는것을 권장

#define RELEASE  // 메모리 부족으로 테스트용 자극 출력(동물실험 및 내부기 시험용 코드 비활성화)

#if 0

#if 0
#define DebuggingMode_3_3V_PMIC_controlled_by_CFX
#endif

#if 0
#define DebuggingMode_Only_CFX_code
#endif

#if 0
#define DebuggingMode_telecoilTestInput
#endif

#if 0
#define DebuggingMode_AGCTestInput
#endif

#if 0
#define DebuggingMode_AGC_TestMaxInput
#endif

#if 0
#define DebuggingMode_FFT_TestInput
#endif

#if 0
#define DebuggingMode_Vmag_TestInput
#endif

#if 0
#define DebuggingMode_FreqAnal_TestInput
#endif

#if 0
#define DebuggingMode_StimulVolumeTest
#endif

#if 0
#define DebuggingMode_FreqAnal_TestInput
#endif

#if 0
#define DebuggingMode_Test_generatingPCM
#endif

#if 0
#define DebuggingMode_Test_PCM_dataOut
#endif

#if 0
#define DebuggingMode_LED_Pins_as_CFX_TestPoint
#endif

#if 0
#define DebuggingMode_Only_CM3_code
#endif

#endif

/* RTT 기반 UI 테스트 커맨드 (개발/검증 전용) */
#define ENABLE_UI_CMD

////////////////////////////////////////////
// 동작 환경 선택
//////////////////

#if 0
#define EEPROM_LSK_Error
#endif

#if 0
#define disable_Tx_PowerControl
#endif

#if 1
#define conneded_ISDCheck_byISDPower
#else
#define conneded_ISDCheck_byForwardPath
#endif

#define CM3_I2C_controls_FPAG

//////////
// ble chip 관련

#define Df_Disconnection_BLE_Time_ms 2000

#define Df_MaxDeliveryChargeLimitationKKK 1

/////////////////
// UART

#if 0

#define AudioInputSignal_Tx_usingUART

#endif

#define CFX_UART_USING_ISR

#define CFX_SYS_Clk        15360000
#define CFX_UART_BAUD_RATE 921600  // 460800//921600//115200//38400//115200//38400
// #define CFX_UART_BAUD_RATE         921600 //460800//921600//115200//38400//115200//38400

// Board_Bingley_devBoard 1.0에서 레벨쉬프트의 스위칭 주파수 제한으로 발생.
// 38400 확인(스위칭 타이밍 정상)
// 57600 확인(스위칭 타이밍 정상)
// 115200확인(스위칭 타이밍 정상)
// 460800확인(하지만...타이밍 문제 발생 우려)
// 921600확인(하지만...타이밍 문제 발생 우려)

#define CFX_UART_RX_ENABLE             // PC에서 데이터 수신을 활성화하여 터미널 명령을 받을 경우에 사용한다
#define UART_bufferLength_forAudio 64  // 16*4

///////////////////////////////////////////////////////////////////////////////////////////
// CFX 관련
/////////////////
// I2C
#if 1
#define CFX_I2c_using_ISR
#endif

#define df_i2c_Tx_buffer_size 64
#define df_i2c_Rx_buffer_size 64

// UART

///////////////////////////////////////////////////////////////////////////////////////////
// CM3 관련

#define CM3_I2c_using_ISR

#define CM3_SPI_USING_DMA

///////////////////
// 신호처리 기본

// 사용자수

#define MaxNumUser 4
// 사용자별 맵 개수
#define MaxNumMap 4

//
#define ManufacturingDefault_ISD_No 1

///////////////////////
// 논리
#define df_True  1
#define df_False 0

#define df_Defalut      0
#define df_Connected    1
#define df_Disconnected 2

// 휴대보관함 커버 열림/닫힘 상태를 구분하기 위해 추가함 (김은수, 2026.02.20)
#define df_Opened 1
#define df_Closed 2

#endif /* DEFINITION_Preprocessor_H__ */



