

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

/* 시스템 시험용 DIO35 사용 여부: 0=사용 안 함, 1=GPIO 출력 Low 초기화.
 * SYS-012는 CFX test/tdc_sys_test_config.h의 같은 상수도 1로 빌드한다.
 * 측정 중 CFX가 이 핀을 사용하면 CM3에서 동시에 토글하지 않는다.
 * 실제 보드에서 미사용 핀임을 확인한다. EXTCLK 입력과 함께 쓸 수 없다. */
#ifndef TDC_SYSTEM_TEST_DIO35_ENABLE
#define TDC_SYSTEM_TEST_DIO35_ENABLE 0
#endif
#if (TDC_SYSTEM_TEST_DIO35_ENABLE != 0) && (TDC_SYSTEM_TEST_DIO35_ENABLE != 1)
#tdc_sys_error "TDC_SYSTEM_TEST_DIO35_ENABLE must be 0 or 1"
#endif

/* SW-FW-SYS-Test-ID-7 전용 RTT 명령. 1인 시험 빌드에서만 --sys7을 제공한다.
 * 제품 전하량 보호 설정/판정은 바꾸지 않는다. 시험 후 정상 복귀는 재부팅한다. */
#ifndef TDC_SYS_TEST_7_ENABLE
#define TDC_SYS_TEST_7_ENABLE 0
#endif
#if (TDC_SYS_TEST_7_ENABLE != 0) && (TDC_SYS_TEST_7_ENABLE != 1)
#tdc_sys_error "TDC_SYS_TEST_7_ENABLE must be 0 or 1"
#endif
#if TDC_SYS_TEST_7_ENABLE && !defined(ENABLE_UI_CMD)
#tdc_sys_error "TDC_SYS_TEST_7_ENABLE requires ENABLE_UI_CMD"
#endif

/* SW-FW-SYS-Test-ID-23 전용. --sys23 start로 알람 판단에만 19%를 주입한다.
 * 전원 종료 판단은 QCC의 실제 잔량을 유지한다. 시험용 내부기 지그에서만 사용.
 * 0=명령/시험 분기 제외, 1=RTT 명령 사용. 한 회차 완료/stop/5초 후 입력 해제. */
#ifndef TDC_SYS_TEST_23_ENABLE
#define TDC_SYS_TEST_23_ENABLE 0
#endif
#if (TDC_SYS_TEST_23_ENABLE != 0) && (TDC_SYS_TEST_23_ENABLE != 1)
#tdc_sys_error "TDC_SYS_TEST_23_ENABLE must be 0 or 1"
#endif
#if TDC_SYS_TEST_23_ENABLE && !defined(ENABLE_UI_CMD)
#tdc_sys_error "TDC_SYS_TEST_23_ENABLE requires ENABLE_UI_CMD"
#endif

/* SYS-050/051 감사기록 표시 시험. 고정 날짜/가상 주소를 RAM에서만 구성한다.
 * 실제 감사 파일, 장치 시간, 연결 상태는 변경하지 않는다. 시험 후 0으로 사용. */
#ifndef TDC_SYS_TEST_AUDIT_ENABLE
#define TDC_SYS_TEST_AUDIT_ENABLE 0
#endif
#if (TDC_SYS_TEST_AUDIT_ENABLE != 0) && (TDC_SYS_TEST_AUDIT_ENABLE != 1)
#tdc_sys_error "TDC_SYS_TEST_AUDIT_ENABLE must be 0 or 1"
#endif
#if TDC_SYS_TEST_AUDIT_ENABLE && !defined(ENABLE_UI_CMD)
#tdc_sys_error "TDC_SYS_TEST_AUDIT_ENABLE requires ENABLE_UI_CMD"
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
#tdc_sys_error "Storage system test switches must be 0 or 1"
#endif
#define TDC_SYS_TEST_STORAGE_ENABLE (TDC_SYS_TEST_54_ENABLE || TDC_SYS_TEST_73_ENABLE || \
                                    TDC_SYS_TEST_77_ENABLE || TDC_SYS_TEST_80_ENABLE)
#if TDC_SYS_TEST_STORAGE_ENABLE && !defined(ENABLE_UI_CMD)
#tdc_sys_error "Storage system tests require ENABLE_UI_CMD"
#endif

////////////////////////////////////////////
// 동작 환경 선택
//////////////////

/* EEPROM_LSK_Error · disable_Tx_PowerControl 정의 스텁(각각 #if 0 안)은 제거했다.
 * 둘 다 정의된 적이 없어 이를 보던 분기는 전부 사장이었다.
 * disable_Tx_PowerControl 은 !defined() 로 쓰여 존치 로직은 그대로 살아 있다. */

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
