

#ifndef DEFINITION_Preprocessor_H__
#define DEFINITION_Preprocessor_H__

//////////
// 실행 보드 (지원: Board_is_OTE_VER_1_5)
#define Board_is_OTE_VER_1_5

///////////
// FPGA 버전 (지원: FPGA_Ver_is_270)
#define FPGA_Ver_is_270

///////////
// 내부이식장치 버전 (지원: ISD_Ver_is_112)
#define ISD_Ver_is_112

// 디버깅 관련

#define DebuggerEnable  // 디버깅 장치의 접근을 허용.. 최종 제품에서는 삭제하는것을 권장

#define RELEASE  // 메모리 부족으로 테스트용 자극 출력(동물실험 및 내부기 시험용 코드 비활성화)

/* DebuggingMode_* 13종(3_3V_PMIC_controlled_by_CFX · Only_CFX_code · telecoilTestInput ·
 * AGCTestInput · AGC_TestMaxInput · FFT_TestInput · Vmag_TestInput · FreqAnal_TestInput ·
 * StimulVolumeTest · Test_generatingPCM · Test_PCM_dataOut · LED_Pins_as_CFX_TestPoint ·
 * Only_CM3_code)은 2차 리팩토링에서 제거했다. 전부 중첩 #if 0 안에 있어 정의 자체가
 * 컴파일되지 않았고, #ifdef 소비처도 CM3 전역에 0 이었다. */

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
// CFX

/* CFX UART 설정(CFX_UART_USING_ISR · CFX_UART_BAUD_RATE · CFX_UART_RX_ENABLE ·
 * UART_bufferLength_forAudio · AudioInputSignal_Tx_usingUART)은 2차 리팩토링에서 제거했다.
 * CM3 는 UART 를 사용하지 않으며(2026-07-23 확정), 이 매크로들도 CM3 안에서 참조가 0 이었다.
 * 부트로더(0__bootloader)의 UART 는 별개로 계속 사용 중이다. */

#define CFX_SYS_Clk 15360000

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

#define TDC_HAL_I2C_USING_ISR

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



