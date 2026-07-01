/*
 * tdc_touch_config.h
 *
 * 터치 빌드 토글 (기능 ON/OFF) — 현재 활성 토글 없음. 3레이어 탈피 간결 재작성 2026-06-20.
 *
 *   FullATI 단순안 전환으로 폐기된 토글:
 *     - 보드별 고정 보상값 선택(TDC_BOARD_VARIANT MINI/DEVELOP/PACKAGE)
 *       → Full ATI 는 IC 가 게인을 자동 산출하므로 보드 변형 선택이 불필요.
 *         (핀 매핑은 processorDirective.h 의 Board_is_* 가 담당 — 본 매크로와 무관)
 *     - 개발용 모드(SLEEP_MEASURE·RTT_TUNING·ATI_CALIB·ATI_DUMP), CRX1 변형, CRX0 방전.
 *
 *   시간 '수치'는 tdc_touch_time.h 가 단일 소유(역할 분리).
 *   향후 기능 ON/OFF 빌드 토글이 필요하면 본 파일에 추가한다(빌드 -D 외부 주입은 #ifndef 가드).
 */

#ifndef TDC_TOUCH_CONFIG_H_
#define TDC_TOUCH_CONFIG_H_

/* 노말 폴링(200ms)마다 LTA/Counts/임계 계측을 RTT(ci_printd)로 출력 — 실측 튜닝용.
 * 튜닝 완료 후 0 으로 두면 계측 블록이 컴파일 단계에서 완전 제거(런타임 비용 0). */
#ifndef TDC_TOUCH_DEBUG_PRINT_ENABLE
#  define TDC_TOUCH_DEBUG_PRINT_ENABLE  1
#endif

#endif /* TDC_TOUCH_CONFIG_H_ */
