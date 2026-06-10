/*
 * tdc_touch_config.h
 *
 * IQS323 터치 드라이버 빌드 설정 — 이 파일 한 곳에서 수정.
 *
 * TDC_BOARD_VARIANT, TDC_DRV_IQS323_ATI_DUMP_ENABLE, TDC_TOUCH_ATI_CALIB_MODE
 * 세 가지를 여기서 관리한다. 빌드 옵션(-D)으로 외부 주입 시에도 이 파일의
 * #ifndef 가드가 충돌을 방지한다.
 */

#ifndef TDC_TOUCH_CONFIG_H_
#define TDC_TOUCH_CONFIG_H_

/* **********************************************************************
 * 보드 변형 상수 (TDC_BOARD_VARIANT 에 아래 값을 사용)
 */
#define TDC_BOARD_VARIANT_MINI     0
#define TDC_BOARD_VARIANT_DEVELOP  1
#define TDC_BOARD_VARIANT_PACKAGE  2

/* ======================================================================
 *  빌드 설정 — 개발·테스트 시 여기서 수정, 프로덕션 전 기본값 복원
 * ====================================================================== */

/* 1. 보드 변형 선택
 *    MINI(0): 나동 PCB, 자석·배터리 없음
 *    DEVELOP(1): 개발 보드
 *    PACKAGE(2): 완제품 조립 (자석·배터리 근접)          ← 프로덕션 기본값 */
#ifndef TDC_BOARD_VARIANT
#  define TDC_BOARD_VARIANT  TDC_BOARD_VARIANT_PACKAGE
#endif

/* 2. ATI 덤프 모드 (시리얼 RTT 출력)
 *    0: 비활성 (운용 모드)                              ← 프로덕션 기본값
 *    1: 활성 — RE-ATI 실행 후 MULT/COMP 값을 시리얼로 출력 */
#ifndef TDC_DRV_IQS323_ATI_DUMP_ENABLE
#  define TDC_DRV_IQS323_ATI_DUMP_ENABLE  0
#endif

/* 3. ATI 캘리브레이션 모드 (LED 이진 출력)
 *    0: 비활성 (운용 모드)                              ← 프로덕션 기본값
 *    1: 활성 — auto-ATI 후 MULT/COMP 16비트를 LED 이진수로 무한 출력
 *
 *    Note: processorDirective.h 의 Board_is_* 와는 의미 레이어가 다르다
 *          (Board_is_*=핀 매핑, TDC_BOARD_VARIANT=드라이버 캘리브레이션). */
#ifndef TDC_TOUCH_ATI_CALIB_MODE
#  define TDC_TOUCH_ATI_CALIB_MODE  0
#endif

/* 3-1. ATI 캘리브레이션 LED 출력 활성화 (TDC_TOUCH_ATI_CALIB_MODE=1 시에만 유효)
 *    0: RTT 출력만 (빠름 — JLink 연결 환경 전용)
 *    1: LED 이진 출력 (기본값 — JLink 없이 육안 확인 가능) */
#ifndef TDC_TOUCH_ATI_CALIB_LED_ENABLE
#  define TDC_TOUCH_ATI_CALIB_LED_ENABLE  0
#endif

/* 4. 절전 모드 ATI 측정 모드 (개발용)
 *    0: 비활성 (운용 모드)                              ← 프로덕션 기본값
 *    1: 활성 — RTT 's' 입력 시 func_sleep 진입,
 *             주변장치 OFF 환경에서 re-ATI 후 ATI 레지스터를 RTT로 반복 출력.
 *             'q' 입력 시 WDT 리셋. */
#ifndef TDC_TOUCH_SLEEP_MEASURE_MODE
#  define TDC_TOUCH_SLEEP_MEASURE_MODE  0
#endif

/* 5. CRX1 GND(VSS) 설정 — ESD 방전 경로 생성
 *    HW 전제조건: C52(100nF) 제거 후 0Ω 쇼트 완료 (J4 ─ R32(0Ω) ─ CRX1 DC 직결)
 *    0: 비활성 — CRX1 Floating (기본값, C52 미수정 상태에서도 안전)
 *    1: 활성   — CRX1 IC 내부 VSS 고정 → J4 패드 ESD 방전 경로 생성    ← C52 단락 후 사용
 *    효과: IQS323 Sensor1 Setup Inactive Rxs = VSS (bits[3:2]=0b10) 적용
 *          CRX1에 누적된 정전기가 IC 내부 VSS로 방전됨 */
#ifndef TDC_TOUCH_CRX1_VSS_ENABLE
#  define TDC_TOUCH_CRX1_VSS_ENABLE  0
#endif

/* 6. CRX1 Reference 채널 모드 — CH0 LTA 느린 환경 드리프트(온도·습도) 보정
 *    C52(100nF) 있는 상태에서도 동작 (AC 경로로 정전용량 측정)
 *    0: 비활성 — CH1 사용 안 함 (기본값)
 *    1: 활성   — CH1을 Reference 모드로 활성화, CH0 LTA 보정 기준값 채널로 사용
 *    효과: IQS323 Channel Setup (0x70) bits[3:0] = 0x02 (Reference 모드)
 *          온도·습도 변화에 의한 CH0 delta 오판 감소 (ESD 급격 변화에는 무효)
 *    참고: 5번(VSS_ENABLE)·6-1번(DUMMY_ENABLE)과 상호 배타 */
#ifndef TDC_TOUCH_CRX1_REF_ENABLE
#  define TDC_TOUCH_CRX1_REF_ENABLE  0
#endif

/* 6-1. CRX1 더미 채널 — 내부 CalCap을 변환 부하로 사용, 외부 CRX1 핀 완전 무관
 *    목적: CH1을 더미로 활성화 → IC measurement cycle 유지
 *          → CH0 discharge(7번) 사용 시 "유일 활성 채널 없음" 문제 방지
 *    원리: 내부 CalCap(0.5pF×N)을 Sensor Setup CalCap Rx/Tx(0x40 bit14/13)로 선택
 *          변환 경로가 IC 내부에서 닫힘 → C52 유무·쇼트 무관, ATI 수렴 안정
 *          0x43(Prox Input)은 미수정(reset 0x01CF 유지) — reserved 비트 보존
 *    0: 비활성 (기본값)
 *    1: 활성   — CH1 enable + CalCap Rx/Tx (C52 유무·쇼트 무관)
 *    상세: docs/참고/touch/IQS323-CalCap-더미채널.md
 *    참고: 5번(VSS_ENABLE)·6번(REF_ENABLE)과 상호 배타 */
#ifndef TDC_TOUCH_CRX1_DUMMY_ENABLE
#  define TDC_TOUCH_CRX1_DUMMY_ENABLE  1
#endif

#if (TDC_TOUCH_CRX1_VSS_ENABLE + TDC_TOUCH_CRX1_REF_ENABLE + TDC_TOUCH_CRX1_DUMMY_ENABLE) > 1
#  error "TDC_TOUCH_CRX1_VSS_ENABLE·REF_ENABLE·DUMMY_ENABLE 중 하나만 활성화하세요"
#endif

/* 7. CRX0 VSS 방전 — 매 폴링마다 CH0 일시 비활성+접지로 ESD 누적 전하 제거
 *
 *    [전제] 6-1(CRX1_DUMMY_ENABLE=1)과 반드시 함께 사용. CH0이 유일 활성 채널이면
 *    disable 순간 measurement cycle이 멈춰 RDY 윈도우 먹통이 되므로, CH1 더미 채널이
 *    cycle을 유지해줘야 한다. (상세: docs/참고/touch/이슈해결/2026-06_CRX1-ESD-더미채널.md)
 *
 *    0: 비활성
 *    1: 활성 (기본값) — 6-1 DUMMY_ENABLE=1 전제 */
#ifndef TDC_TOUCH_CRX0_DISCHARGE_ENABLE
#  define TDC_TOUCH_CRX0_DISCHARGE_ENABLE  1
#endif

/* 8. CRX0 방전 write 로그 — 7번이 200ms 폴링마다 write 2회를 수행해 RTT 범람.
 *    방전 전용 write 헬퍼(write_register_discharge)의 verbose 로그만 제어하며,
 *    다른 시퀀스(sensor_setup 등)의 write 로그와 에러 로그는 영향 없음.
 *    0: 방전 write 로그 억제 (기본값)
 *    1: 방전 write 출력 ([DISCHARGE] 태그) — 방전 시퀀스 디버깅용 */
#ifndef TDC_TOUCH_CRX0_DISCHARGE_LOG
#  define TDC_TOUCH_CRX0_DISCHARGE_LOG  0
#endif

/* 9. 터치 마진 환산 로그 — tdc_touch_get_state()에서 CRX0 방전(7번) 직전 출력.
 *    노말·절전 공통 경로에서 현재 신호를 threshold 계수 단위로 역환산해 RTT 출력.
 *    current_coeff = (LTA-Counts)*256/LTA. 설정 threshold(80)에 근접할수록 터치 직전,
 *    threshold 계수 초과 시 터치 진입. 방전하면 측정값이 다음 사이클용으로 바뀌므로
 *    반드시 방전 전 신선한 LTA/Counts 로 읽어야 마진이 정확하다.
 *    0: 비활성 (기본값)
 *    1: 활성 — 매 폴링마다 [TOUCH] MARGIN ... 출력 (RTT 범람 주의) */
#ifndef TDC_TOUCH_MARGIN_LOG_ENABLE
#  define TDC_TOUCH_MARGIN_LOG_ENABLE  1
#endif

/* 9-1. 터치 마진 로그 출력 주기 (9번 활성 시) — 이 폴링 횟수마다 1회만 마진을 읽는다.
 *    read_touch_margin 은 레지스터 3회 읽기(force communication 다수)를 유발해 self-cap
 *    측정 cycle 을 방해할 수 있으므로, 빈도를 낮춰 측정 교란을 줄인다.
 *    1 = 매 폴링(기존 동작), 권장 10 안팎(200ms 폴링 기준 약 2초마다 1회). */
#ifndef TDC_TOUCH_MARGIN_LOG_INTERVAL
#  define TDC_TOUCH_MARGIN_LOG_INTERVAL  5
#endif

/* 10. Prox Threshold 적용 — 노터치 LTA freeze 방지용 Prox Settings(0x61) 설정.
 *    적용 값은 tdc_drv_iqs323.h 의 (SLEEP_)PROX_THRESHOLD.
 *    실측에서 측정 불안정(터치/노터치 채터링) 유발이 의심되어 기본 비활성으로 둔다.
 *    0: 비활성 — Prox Settings 미설정(reset 0 유지) = prox 적용 전 원래 동작 (기본값)
 *    1: 활성 — 노말 PROX_THRESHOLD / 절전 SLEEP_PROX_THRESHOLD 적용 */
#ifndef TDC_TOUCH_PROX_THRESHOLD_ENABLE
#  define TDC_TOUCH_PROX_THRESHOLD_ENABLE  0
#endif

#endif /* TDC_TOUCH_CONFIG_H_ */
