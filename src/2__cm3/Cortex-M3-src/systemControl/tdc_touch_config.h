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
#  define TDC_DRV_IQS323_ATI_DUMP_ENABLE  1
#endif

/* 3. ATI 캘리브레이션 모드 (LED 이진 출력)
 *    0: 비활성 (운용 모드)                              ← 프로덕션 기본값
 *    1: 활성 — auto-ATI 후 MULT/COMP 16비트를 LED 이진수로 무한 출력
 *
 *    Note: processorDirective.h 의 Board_is_* 와는 의미 레이어가 다르다
 *          (Board_is_*=핀 매핑, TDC_BOARD_VARIANT=드라이버 캘리브레이션). */
#ifndef TDC_TOUCH_ATI_CALIB_MODE
#  define TDC_TOUCH_ATI_CALIB_MODE  1
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
#  define TDC_TOUCH_SLEEP_MEASURE_MODE  1
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
 *    참고: 5번(VSS_ENABLE)과 상호 배타 — 동시 활성 시 컴파일 오류 */
#ifndef TDC_TOUCH_CRX1_REF_ENABLE
#  define TDC_TOUCH_CRX1_REF_ENABLE  1
#endif

#if TDC_TOUCH_CRX1_VSS_ENABLE && TDC_TOUCH_CRX1_REF_ENABLE
#  error "TDC_TOUCH_CRX1_VSS_ENABLE 과 TDC_TOUCH_CRX1_REF_ENABLE 은 상호 배타 — 하나만 활성화하세요"
#endif

/* 7. CRX0 VSS 방전 — 매 폴링 직전 CH0 일시 비활성+접지로 ESD 전하 제거
 *    조건 없이 항상 방전 → 먹통 예방(평소) + 먹통 치료(이미 ESD 누적 시)
 *    방전 후에도 실제 터치 정전용량(손가락)은 유지됨 — 오감지 없음
 *    0: 비활성 (기본값, 원래 동작 유지)
 *    1: 활성   — tdc_touch_get_state() 호출마다 CH0 disable(CRX0=VSS) → enable → read */
#ifndef TDC_TOUCH_CRX0_DISCHARGE_ENABLE
#  define TDC_TOUCH_CRX0_DISCHARGE_ENABLE  1
#endif

#endif /* TDC_TOUCH_CONFIG_H_ */
