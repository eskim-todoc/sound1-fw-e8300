#ifndef LED_OUTPUT_H__
#define LED_OUTPUT_H__

/** 롱터치 무시 케이스 보라색 LED 피드백 (디버깅용) - 0 으로 설정 시 기존 동작(무시만) 복원 */
#define TDC_DBG_LONG_TOUCH_IGNORE_LED  0

#include <stdbool.h>
#include <stdint.h>

#include <ci_printf.h>

/* ========================================================================
 *  BLE LED Indication (QCC 경유)
 * ======================================================================== */

typedef enum
{
    TDC_LED_IND_STATE_NONE       = 0,
    TDC_LED_IND_STATE_BATT       = 1,
    TDC_LED_IND_STATE_PAIR       = 2,
    TDC_LED_IND_STATE_OTA_QCC    = 3,
    TDC_LED_IND_STATE_OTA_EZAIRO = 4,
} tdc_led_ind_state_t;

/* ========================================================================
 *  Legacy LED pattern enum (게이트 호환용으로 유지)
 * ======================================================================== */

typedef enum
{
    en__LED_NA = 0,
    en__LED_Map_Error,
    en__LED_MCU_Error,
    en__LED_MCU_Accelerometer_Error,
    en__LED_MCU_FPGA_Error,
    en__LED_MCU_RF_PMIC_Error,
    en__LED_POWER_On,
    en__LED_POWER_Off,
} EN__LED_PATTERN;

/* ========================================================================
 *  LED color enum (GPIO 매핑용, 기존 유지)
 * ======================================================================== */

typedef enum
{
    en__LED_BLACK = 0,
    en__LED_RED,
    en__LED_GREEN,
    en__LED_BLUE,
    en__LED_ORANGE,
    en__LED_SKYBLUE,
    en__LED_PURPLE,
    en__LED_WHITE
} EN__LED_COLOR;

/* ========================================================================
 *  New LED state / source / pattern descriptor
 * ======================================================================== */

typedef enum
{
    LED_ST_NONE = 0,
    LED_ST_IDLE,

    /* 운용 */
    LED_ST_BATT_READY,     /* 녹색 지속 ON */
    LED_ST_IN_USE,         /* 흰색 지속 ON */
    LED_ST_BATT_MID,       /* 노랑 지속 ON */
    LED_ST_BATT_CRITICAL,  /* 노랑 ON 1100ms / OFF 1100ms 점멸 */

    /* Mapping (4종 분화 - 배터리 LOW 임계 20% × ISD 연결 여부) */
    LED_ST_MAPPING_ISD_BATT_READY,    /* 파랑 ON 200ms / OFF 800ms 점멸 (배터리 > 20% & ISD 연결) */
    LED_ST_MAPPING_NO_ISD_BATT_READY, /* 파랑 지속 ON                  (배터리 > 20% & ISD 미연결) */
    LED_ST_MAPPING_ISD_BATT_LOW,      /* 보라 ON 100ms / OFF 900ms 점멸 (배터리 ≤ 20% & ISD 연결) */
    LED_ST_MAPPING_NO_ISD_BATT_LOW,   /* 보라 지속 ON                  (배터리 ≤ 20% & ISD 미연결) */

    /* BLE led_ind */
    LED_ST_PAIR,       /* 파랑 ON 500ms / OFF 500ms 점멸 (+1000ms latch) */
    LED_ST_OTA_QCC,    /* 녹색 ON 1100ms / OFF 1100ms 점멸 */
    LED_ST_OTA_EZAIRO, /* 녹색 ON 180ms / OFF 180ms 점멸 */

    /* 에러 (색/패턴 공통 180/360 빨강) */
    LED_ST_ERROR_MAP,
    LED_ST_ERROR_MCU,
    LED_ST_ERROR_ACCEL,
    LED_ST_ERROR_FPGA,
    LED_ST_ERROR_PMIC,

    /* 게이트 */
    LED_ST_POWER_ON,
    LED_ST_POWER_OFF,

    /* [DBG] 롱터치 무시 케이스 피드백 - TDC_DBG_LONG_TOUCH_IGNORE_LED 로 제어 */
    LED_ST_DBG_LONG_TOUCH_IGNORE, /* 보라 ON 180ms / OFF 180ms × 3회 버스트 */

    LED_ST__MAX
} led_state_t;

typedef enum
{
    LED_SRC_POWER, /* POWER_ON / POWER_OFF */
    LED_SRC_ERROR,
    LED_SRC_BLE_IND, /* PAIR / OTA_QCC / OTA_EZAIRO */
    LED_SRC_MAPPING,
    LED_SRC_BATTERY, /* BATT_CRITICAL / BATT_MID / READY */
    LED_SRC_ISD,     /* IN_USE */
    LED_SRC_DBG,     /* [DBG] 디버깅 전용 임시 소스 - TDC_DBG_LONG_TOUCH_IGNORE_LED */
    LED_SRC__MAX
} led_src_t;

/* LED 패턴 디스크립터.
 *   on_ms     : 한 주기 안에서 LED가 켜져 있는 시간 (ms)
 *   period_ms : 한 주기 전체 길이 (ms). off 시간 = period_ms - on_ms.
 *               on_ms == 0 && period_ms == 0 인 경우 점멸 없이 "지속 ON".
 *   burst_cnt : 0 = 무한 반복, >0 = N회 반복 후 자가 해제 (POWER_ON/POWER_OFF 게이트용).
 *
 * 주의: 본 구조체의 (on_ms, period_ms) 표기와, 사람이 읽는 "ON Xms / OFF Yms"
 *       표기는 다음 관계임. period_ms = on_ms + off_ms.
 *       예) ON 1100ms / OFF 1100ms  ↔  { on_ms=1100, period_ms=2200 } */
typedef struct
{
    EN__LED_COLOR color;
    uint16_t      on_ms;
    uint16_t      period_ms;
    uint8_t       burst_cnt;
} led_pattern_desc_t;

/* ========================================================================
 *  New API (Arbiter + Engine)
 * ======================================================================== */

void        led_request(led_src_t src, led_state_t st);
void        led_arbiter_tick(void);
bool        tdc_led_is_burst_pending(void);
led_state_t led_get_request(led_src_t src);

/* CFX / FIFO ISR 활성 여부 통지 - initialize.c 에서 set / clear.
 * turnOffLED() 가 ISR 의존 fade-off vs. 즉시 OFF 분기 결정에 사용. */
void        led_isr_active_set(bool active);

/* 절전 진입 직전 1 회 호출. 모든 src LED_ST_NONE 강제 + cross-fade
 * Phase A 자연 fade-off 보장 (소요 ~ LED_DIMMING_FADE_MAX_MS + 10 ms).
 * 호출 후엔 LED 가 BLACK 상태로 안정 - turnOffLED() 가 잔상 없이 마무리. */
void        led_force_fade_off(void);

/* ========================================================================
 *  Legacy API (호환용 유지)
 * ======================================================================== */

void                tdc_led_set_ind_state(tdc_led_ind_state_t state);
tdc_led_ind_state_t tdc_led_get_ind_state(void);

void LedPatternOut(EN__LED_PATTERN ledOutputPattern);

void enabletestLED_Trigger(void);
void disabletestLED_Trigger(void);
bool isTestTriggerEanbled(void);

void LED_OUT(void);

void LED_black(void);
void LED_White(void);
void turnOffLED(void);
void LED_Memory_error(void);
void LED_clock_error(void);

void turnON_RedLED(void);
void turnON_GreenLED(void);
void turnON_BlueLED(void);

void led_set_isd_conn_state(int state);

#endif
