/*
 * tdc_touch_config.h
 *
 * 터치 빌드 토글 (기능 ON/OFF) - 현재 활성 토글 없음. 3레이어 탈피 간결 재작성 2026-06-20.
 *
 *   FullATI 단순안 전환으로 폐기된 토글:
 *     - 보드별 고정 보상값 선택(TDC_BOARD_VARIANT MINI/DEVELOP/PACKAGE)
 *       → Full ATI 는 IC 가 게인을 자동 산출하므로 보드 변형 선택이 불필요.
 *         (핀 매핑은 processorDirective.h 의 Board_is_* 가 담당 - 본 매크로와 무관)
 *     - 개발용 모드(SLEEP_MEASURE·RTT_TUNING·ATI_CALIB·ATI_DUMP), CRX1 변형, CRX0 방전.
 *
 *   시간 '수치'는 tdc_touch_time.h 가 단일 소유(역할 분리).
 *   향후 기능 ON/OFF 빌드 토글이 필요하면 본 파일에 추가한다(빌드 -D 외부 주입은 #ifndef 가드).
 */

#ifndef TDC_TOUCH_CONFIG_H_
#define TDC_TOUCH_CONFIG_H_

/* 터치 디버깅은 '계측'과 '출력'을 따로 켠다 (2026-07-27 분리).
 *
 * 예전에는 스위치가 TDC_TOUCH_DEBUG_PRINT_ENABLE 하나뿐이었는데, 정작 RTT 출력만
 * 소스에서 #if 0 으로 막아 두는 바람에 "계측은 도는데 아무 데도 안 쓰이는" 상태가 됐다.
 * (main.c 절전 경로에서는 I2C 읽기까지 매 폴링마다 낭비) 그래서 두 개로 나눴다.
 *
 *   TDC_TOUCH_DEBUG_MEASURE_ENABLE : LTA/Counts/임계 계측값 수집(s_debug_recent_*).
 *                                    BLE 범용 디버깅 명령이 이 값을 읽어가므로 기본 ON.
 *                                    (ble/tdc_ble_general_debug.c 의 accessor 7종)
 *   TDC_TOUCH_DEBUG_PRINT_ENABLE   : 위 값을 RTT(TDC_PRINTF_D)로 찍는다. 실측 튜닝용.
 *                                    0 이면 관련 블록이 통째로 컴파일에서 빠진다(런타임 비용 0).
 *
 * 둘 다 '블록 전체'를 감싼다. 출력만 따로 막지 말 것 - 그러면 계산이 고아로 남는다. */
#ifndef TDC_TOUCH_DEBUG_MEASURE_ENABLE
#  define TDC_TOUCH_DEBUG_MEASURE_ENABLE  1
#endif

#ifndef TDC_TOUCH_DEBUG_PRINT_ENABLE
#  define TDC_TOUCH_DEBUG_PRINT_ENABLE  0
#endif

#endif /* TDC_TOUCH_CONFIG_H_ */
