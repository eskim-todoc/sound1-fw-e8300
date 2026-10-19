

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

/* 시스템 시험용 DIO35 사용 여부: 0=사용 안 함, 1=GPIO 출력 Low 초기화.
 * SYS-012는 CFX test/tdc_sys_test_config.h의 같은 상수도 1로 빌드한다.
 * 측정 중 CFX가 이 핀을 사용하면 CM3에서 동시에 토글하지 않는다.
 * 실제 보드에서 미사용 핀임을 확인한다. EXTCLK 입력과 함께 쓸 수 없다. */
#ifndef TDC_SYSTEM_TEST_DIO35_ENABLE
#define TDC_SYSTEM_TEST_DIO35_ENABLE 0
#endif
#if (TDC_SYSTEM_TEST_DIO35_ENABLE != 0) && (TDC_SYSTEM_TEST_DIO35_ENABLE != 1)
#error "TDC_SYSTEM_TEST_DIO35_ENABLE must be 0 or 1"
#endif

/* SW-FW-SYS-Test-ID-7 전용 RTT 명령. 1인 시험 빌드에서만 --sys7을 제공한다.
 * 제품 전하량 보호 설정/판정은 바꾸지 않는다. 시험 후 정상 복귀는 재부팅한다. */
#ifndef TDC_SYS_TEST_7_ENABLE
#define TDC_SYS_TEST_7_ENABLE 0
#endif
#if (TDC_SYS_TEST_7_ENABLE != 0) && (TDC_SYS_TEST_7_ENABLE != 1)
#error "TDC_SYS_TEST_7_ENABLE must be 0 or 1"
#endif
#if TDC_SYS_TEST_7_ENABLE && !defined(ENABLE_UI_CMD)
#error "TDC_SYS_TEST_7_ENABLE requires ENABLE_UI_CMD"
#endif

/* SW-FW-SYS-Test-ID-23 전용. --sys23 start로 알람 판단에만 19%를 주입한다.
 * 전원 종료 판단은 QCC의 실제 잔량을 유지한다. 시험용 내부기 지그에서만 사용.
 * 0=명령/시험 분기 제외, 1=RTT 명령 사용. 한 회차 완료/stop/5초 후 입력 해제. */
#ifndef TDC_SYS_TEST_23_ENABLE
#define TDC_SYS_TEST_23_ENABLE 0
#endif
#if (TDC_SYS_TEST_23_ENABLE != 0) && (TDC_SYS_TEST_23_ENABLE != 1)
#error "TDC_SYS_TEST_23_ENABLE must be 0 or 1"
#endif
#if TDC_SYS_TEST_23_ENABLE && !defined(ENABLE_UI_CMD)
#error "TDC_SYS_TEST_23_ENABLE requires ENABLE_UI_CMD"
#endif

/* SYS-050/051 감사기록 표시 시험. 고정 날짜/가상 주소를 RAM에서만 구성한다.
 * 실제 감사 파일, 장치 시간, 연결 상태는 변경하지 않는다. 시험 후 0으로 사용. */
#ifndef TDC_SYS_TEST_AUDIT_ENABLE
#define TDC_SYS_TEST_AUDIT_ENABLE 0
#endif
#if (TDC_SYS_TEST_AUDIT_ENABLE != 0) && (TDC_SYS_TEST_AUDIT_ENABLE != 1)
#error "TDC_SYS_TEST_AUDIT_ENABLE must be 0 or 1"
#endif
#if TDC_SYS_TEST_AUDIT_ENABLE && !defined(ENABLE_UI_CMD)
#error "TDC_SYS_TEST_AUDIT_ENABLE requires ENABLE_UI_CMD"
#endif

/* 저장/무결성 시스템 시험. 개별로 1을 선택하며 출하 빌드는 모두 0이다.
 * 80 arm은 ISD1 사용자 설정을 백업 후 손상시킨다. 시험용 외부기에서만 사용.
 * 54는 읽기 전용 검사, 73은 파일 실패 주입, 77은 가상 정보 암복호화다. */
#ifndef TDC_SYS_TEST_54_ENABLE
#define TDC_SYS_TEST_54_ENABLE 0
#endif
#ifndef TDC_SYS_TEST_73_ENABLE
#define TDC_SYS_TEST_73_ENABLE 0
#endif
#ifndef TDC_SYS_TEST_77_ENABLE
#define TDC_SYS_TEST_77_ENABLE 0
#endif
#ifndef TDC_SYS_TEST_80_ENABLE
#define TDC_SYS_TEST_80_ENABLE 0
#endif
#if ((TDC_SYS_TEST_54_ENABLE != 0) && (TDC_SYS_TEST_54_ENABLE != 1)) || \
    ((TDC_SYS_TEST_73_ENABLE != 0) && (TDC_SYS_TEST_73_ENABLE != 1)) || \
    ((TDC_SYS_TEST_77_ENABLE != 0) && (TDC_SYS_TEST_77_ENABLE != 1)) || \
    ((TDC_SYS_TEST_80_ENABLE != 0) && (TDC_SYS_TEST_80_ENABLE != 1))
#error "Storage system test switches must be 0 or 1"
#endif
#define TDC_SYS_TEST_STORAGE_ENABLE (TDC_SYS_TEST_54_ENABLE || TDC_SYS_TEST_73_ENABLE || \
                                    TDC_SYS_TEST_77_ENABLE || TDC_SYS_TEST_80_ENABLE)
#if TDC_SYS_TEST_STORAGE_ENABLE && !defined(ENABLE_UI_CMD)
#error "Storage system tests require ENABLE_UI_CMD"
#endif

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



