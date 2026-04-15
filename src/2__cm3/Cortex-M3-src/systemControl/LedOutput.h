#ifndef LED_OUTPUT_H__
#define LED_OUTPUT_H__

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
    LED_ST_READY,
    LED_ST_IN_USE,
    LED_ST_BATT_MID,
    LED_ST_BATT_CRITICAL,

    /* Mapping */
    LED_ST_MAPPING_NO_ISD, /* 파랑 300/300 점멸 */
    LED_ST_MAPPING_ISD,    /* 파랑 지속 ON */

    /* BLE led_ind */
    LED_ST_PAIR,       /* 파랑 180/180 (+500ms latch) */
    LED_ST_OTA_QCC,    /* 녹색 1100/1100 */
    LED_ST_OTA_EZAIRO, /* 녹색 1100/1100 */

    /* 에러 (색/패턴 공통 180/360 빨강) */
    LED_ST_ERROR_MAP,
    LED_ST_ERROR_MCU,
    LED_ST_ERROR_ACCEL,
    LED_ST_ERROR_FPGA,
    LED_ST_ERROR_PMIC,

    /* 게이트 */
    LED_ST_POWER_ON,
    LED_ST_POWER_OFF,

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
    LED_SRC__MAX
} led_src_t;

typedef struct
{
    EN__LED_COLOR color;
    uint16_t      on_ms;
    uint16_t      period_ms; /* 0 = 지속 ON */
    uint8_t       burst_cnt; /* 0 = 무한, >0 = N회 후 자가 해제 */
} led_pattern_desc_t;

/* ========================================================================
 *  New API (Arbiter + Engine)
 * ======================================================================== */

void        led_request(led_src_t src, led_state_t st);
void        led_arbiter_tick(void);
bool        led_is_power_burst_in_progress(void);
led_state_t led_get_request(led_src_t src);

/* ========================================================================
 *  Legacy API (호환용 유지)
 * ======================================================================== */

void                tdc_led_set_ind_state(tdc_led_ind_state_t state);
tdc_led_ind_state_t tdc_led_get_ind_state(void);

EN__LED_PATTERN geteLED_OutputPattern(void);
void            LedPatternOut(EN__LED_PATTERN ledOutputPattern);

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

#endif
