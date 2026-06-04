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

#endif /* TDC_TOUCH_CONFIG_H_ */
