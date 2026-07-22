/*
 * tdc_touch_iqs323.h
 *
 * IQS323 I2C 직접 접근 (HAL/포트 추상 없음). 3레이어 탈피 간결 재작성 2026-06-20.
 *
 * FullATI 제어가 필요로 하는 6동작(read_status / apply_settings / re_ati / reseed /
 * mclr / is_ati_done)만 직접 I2C 로 노출한다. 레지스터 helper / RDY 윈도우 / force comm /
 * 시퀀스 보조는 전부 .c 내부 static (헤더 노출 0). 제어 판정(게이트/stuck/롱터치)은 0건 ?
 * 그것은 순수 FSM(tdc_touch_logic) 소관이고 본 레이어는 read/액션 동작만 제공한다.
 */

#ifndef TDC_TOUCH_IQS323_H_
#define TDC_TOUCH_IQS323_H_

#include <stdbool.h>
#include <stdint.h>

#include <hw.h>
#include <tdc_hal_i2c.h>
#include <tdc_printf.h>
#include <tdc_hal_timer.h>

#include <tdc_touch_config.h>

/* **********************************************************************
 * 통신 / 타이밍 상수 (L1 HW 고유)
 */
#define TDC_TOUCH_IQS323_SLAVE_ADDR        0x44
#define TDC_TOUCH_IQS323_RDY_PIN           DIO16
#define TDC_TOUCH_IQS323_WIN_OPEN          0
#define TDC_TOUCH_IQS323_WIN_CLOSED        1
#define TDC_TOUCH_IQS323_MAX_WAIT_OPEN_MS  45
#define TDC_TOUCH_IQS323_MAX_WAIT_CLOSE_MS 20
#define TDC_TOUCH_IQS323_MCLR_HOLD_MS      1
#define TDC_TOUCH_IQS323_BOOT_WAIT_MS      50
#define TDC_TOUCH_IQS323_DEFAULT_DELAY_MS  (SystemCoreClock / 1000)

#define TDC_TOUCH_IQS323_RDY_PIN_CFG_OUTPUT (DIO_2X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define TDC_TOUCH_IQS323_RDY_PIN_CFG_INPUT  (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)

/* **********************************************************************
 * Touch 임계 (FullATI 계수 기반: 절대 = 계수 x LTA / 256 이므로 계수 < 32 필수).
 * Beta/Power 레지스터 정수값은 [실측 게이트] - 시험 write 1회 판별 후 고정.
 */
#ifndef TDC_TOUCH_IQS323_THRESHOLD
#define TDC_TOUCH_IQS323_THRESHOLD 80 /* 절대임계 = 계수 x LTA / 256 ~ 125 @ LTA399 (터치 D 160~190의 약 78%, 약결합 D~40의 3배+). 수렴에 막혀 158 도달난 → 80으로 하향. 8비트(0~255) */
#endif
#ifndef TDC_TOUCH_IQS323_HYSTERESIS
#define TDC_TOUCH_IQS323_HYSTERESIS 8 /* Hysteresis 필드값(4비트 0~15, bits[15:12]). 실제 hyst = (H/256) x Threshold - 5cnt */
#endif
#ifndef TDC_TOUCH_IQS323_PROX_THRESHOLD
#define TDC_TOUCH_IQS323_PROX_THRESHOLD 255 /* Prox Threshold 계수. 255 = prox 사실상 무력화(진입점~LTA) → LTA freeze는 touch만 담당. [실측 게이트: prox 단위 절대/계수] */
#endif

/* **********************************************************************
 * 상태 read 결과 (4-tuple) - 순수 FSM 의 단일 입력 채널.
 *   read 실패/글리치(0xEEEE) 시 ok=false, 나머지 false.
 */
typedef struct
{
    bool ok;         /* read 성공 (통신 실패/0xEEEE 글리치 시 false) */
    bool pressed;    /* CH0 Touch (System Status bit9) */
    bool prox;       /* CH0 Prox (System Status bit8) - 계측/진단용 (FSM 미사용) */
    bool ati_error;  /* ATI Error (bit6) - 드리프트 신호 */
    bool ati_active; /* ATI burst 진행 중 (bit5) */
} tdc_touch_iqs323_status_t;

/* **********************************************************************
 * 공개 6동작
 */
/* 1. 상태 1회 read. 반환 true = 통신 정상. */
bool tdc_touch_iqs323_read_status(tdc_touch_iqs323_status_t *out);

/* 2. 운용 설정 일괄 적용 (Full ATI 원자: ACK -> setup -> 임계 -> Beta/Power -> 0x36 Full ->
 *    부팅 Re-ATI -> RESEED). 부팅 1회 호출. 부팅 Re-ATI 완료 대기는 내부 블로킹 1회. */
void tdc_touch_iqs323_apply_settings(void);

/* 3. Re-ATI 트리거 (0xC0 bit2, 논블로킹). 완료는 다음 read 의 ati_active==0 로 판정. */
bool tdc_touch_iqs323_re_ati(void);

/* 4. RESEED 트리거 (0xC0 bit3 only, 논블로킹). 절전 감도 미동반 - 순수 LTA 재동기. */
bool tdc_touch_iqs323_reseed(void);

/* 5. MCLR 하드리셋 (DIO16 OUTPUT->LOW->INPUT, 약 51ms 블로킹). IQS323 POR -> Auto-ATI. */
void tdc_touch_iqs323_mclr(void);

/* 6. Auto-ATI 완료 여부 1회 read (논블로킹). 실패/진행 중 -> false. */
bool tdc_touch_iqs323_is_ati_done(void);

/* **********************************************************************
 * 디버그 계측 (LTA/Counts 실측 튜닝용) - 노말 폴링에서만 사용, FSM 입력 아님.
 */
typedef struct
{
    bool     ok;     /* read 성공 (통신 실패 시 false) */
    uint16_t counts; /* CH0 Filtered Counts (0x13) */
    uint16_t lta;    /* CH0 LTA (0x14) */
} tdc_touch_iqs323_debug_t;

/* 디버그 계측 1회 read: 0x13(Counts) + 0x14(LTA) 연속 4바이트(auto-increment) 1윈도우.
 * 실패 시 ok=false 반환(계측만 생략, FSM 무영향). */
bool tdc_touch_iqs323_read_debug(tdc_touch_iqs323_debug_t *out);

/* **********************************************************************
 * 보조
 */
void tdc_touch_iqs323_set_ulp(void);
void tdc_touch_iqs323_clear_ulp(void);
bool tdc_touch_iqs323_is_ulp(void);

bool tdc_touch_iqs323_public_settings(uint8_t threshold, uint8_t hysteresis);

#endif /* TDC_TOUCH_IQS323_H_ */
