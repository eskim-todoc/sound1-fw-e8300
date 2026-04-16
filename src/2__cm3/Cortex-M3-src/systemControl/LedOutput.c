
#include <hw.h>
#include <stdbool.h>

#include "processorDirective.h"
#include "board.h"
#include "LedOutput.h"
#include "cfx_cm3_sharedMemory.h"

#include <ci_timer.h>

/* ========================================================================
 *  Pattern Descriptor Table (Rev.3 SS3.4)
 * ======================================================================== */

/* 표기 안내:
 *   디스크립터 값 (on_ms, period_ms) ↔ 사람이 읽는 (ON, OFF) 환산:
 *     period_ms = on_ms + off_ms
 *   예) { on_ms=1100, period_ms=2200 }  ⇒  ON 1100ms / OFF 1100ms
 *   각 행의 // 주석에 ON/OFF 형식으로 같이 표기. */
static const led_pattern_desc_t k_led_patterns[LED_ST__MAX] = {
    [LED_ST_NONE]           = { en__LED_BLACK,   0,    0,    0 },  // 지속 OFF
    [LED_ST_IDLE]           = { en__LED_BLACK,   0,    0,    0 },  // 지속 OFF

    [LED_ST_BATT_READY]     = { en__LED_GREEN,   0,    0,    0 },  // 녹색 지속 ON
    [LED_ST_IN_USE]         = { en__LED_WHITE,   0,    0,    0 },  // 흰색 지속 ON
    [LED_ST_BATT_MID]       = { en__LED_ORANGE,  0,    0,    0 },  // 노랑 지속 ON
    [LED_ST_BATT_CRITICAL]  = { en__LED_ORANGE,  1100, 2200, 0 },  // 노랑  ON 1100ms / OFF 1100ms

    [LED_ST_MAPPING_NO_ISD] = { en__LED_BLUE,   1100, 2200, 0 },   // 파랑  ON 1100ms / OFF 1100ms
    [LED_ST_MAPPING_ISD]    = { en__LED_BLUE,    0,    0,    0 },  // 파랑 지속 ON

    [LED_ST_PAIR]           = { en__LED_BLUE,   180,  360,  0 },   // 파랑  ON 180ms  / OFF 180ms
    [LED_ST_OTA_QCC]        = { en__LED_GREEN,  1100, 2200, 0 },   // 녹색  ON 1100ms / OFF 1100ms
    [LED_ST_OTA_EZAIRO]     = { en__LED_GREEN,  180,  360,  0 },   // 녹색  ON 180ms  / OFF 180ms

    [LED_ST_ERROR_MAP]      = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_MCU]      = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_ACCEL]    = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_FPGA]     = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_PMIC]     = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms

    /* 게이트 -- 출하 검증값 유지 */
    [LED_ST_POWER_ON]       = { en__LED_SKYBLUE, 80,  300,  5 },   // SKYBLUE ON 80ms  / OFF 220ms × 5회 버스트
    [LED_ST_POWER_OFF]      = { en__LED_BLUE,   100,  300,  4 },   // BLUE    ON 100ms / OFF 200ms × 4회 버스트
};

/* ========================================================================
 *  Priority Table (Rev.3 SS3.3)
 * ======================================================================== */

static int led_prio_of(led_state_t st)
{
    switch (st)
    {
        case LED_ST_POWER_OFF:      return 100;
        case LED_ST_POWER_ON:       return 95;

        case LED_ST_ERROR_MAP:
        case LED_ST_ERROR_MCU:
        case LED_ST_ERROR_ACCEL:
        case LED_ST_ERROR_FPGA:
        case LED_ST_ERROR_PMIC:     return 90;

        case LED_ST_OTA_QCC:
        case LED_ST_OTA_EZAIRO:     return 80;

        case LED_ST_MAPPING_ISD:
        case LED_ST_MAPPING_NO_ISD: return 75;

        case LED_ST_PAIR:           return 70;

        case LED_ST_BATT_CRITICAL:  return 60;

        case LED_ST_IN_USE:         return 40;

        case LED_ST_BATT_READY:     return 30;

        case LED_ST_BATT_MID:       return 20;

        case LED_ST_IDLE:           return 10;

        default:                    return 0;
    }
}

static bool led_is_error(led_state_t st)
{
    return (st >= LED_ST_ERROR_MAP && st <= LED_ST_ERROR_PMIC);
}

/* ========================================================================
 *  Legacy pattern <-> new state mapping
 * ======================================================================== */

static EN__LED_PATTERN led_state_to_enum(led_state_t st)
{
    switch (st)
    {
        case LED_ST_ERROR_MAP:   return en__LED_Map_Error;
        case LED_ST_ERROR_MCU:   return en__LED_MCU_Error;
        case LED_ST_ERROR_ACCEL: return en__LED_MCU_Accelerometer_Error;
        case LED_ST_ERROR_FPGA:  return en__LED_MCU_FPGA_Error;
        case LED_ST_ERROR_PMIC:  return en__LED_MCU_RF_PMIC_Error;
        case LED_ST_POWER_ON:    return en__LED_POWER_On;
        case LED_ST_POWER_OFF:   return en__LED_POWER_Off;
        default:                 return en__LED_NA;
    }
}

/* ========================================================================
 *  BLE LED Indication state (기존 유지)
 * ======================================================================== */

static bool testLED_Trigger;

static tdc_led_ind_state_t sg_led_ind_state;

void tdc_led_set_ind_state(tdc_led_ind_state_t state)
{
    sg_led_ind_state = state;

    /* BLE 수신 시 Arbiter에 반영 (Rev.3 SS4.3) */
    switch (state)
    {
        case TDC_LED_IND_STATE_PAIR:
            led_request(LED_SRC_BLE_IND, LED_ST_PAIR);
            break;
        case TDC_LED_IND_STATE_OTA_QCC:
            led_request(LED_SRC_BLE_IND, LED_ST_OTA_QCC);
            break;
        case TDC_LED_IND_STATE_OTA_EZAIRO:
            led_request(LED_SRC_BLE_IND, LED_ST_OTA_EZAIRO);
            break;
        case TDC_LED_IND_STATE_BATT:
            /* BATT는 별도 상태 아님 -- 본체 배터리 판정이 이미 처리 (no-op) */
            break;
        case TDC_LED_IND_STATE_NONE:
        default:
            led_request(LED_SRC_BLE_IND, LED_ST_NONE);
            break;
    }
}

tdc_led_ind_state_t tdc_led_get_ind_state(void)
{
    return sg_led_ind_state;
}

void enabletestLED_Trigger(void)
{
    testLED_Trigger = true;
}

void disabletestLED_Trigger(void)
{
    testLED_Trigger = false;
}

bool isTestTriggerEanbled(void)
{
    return testLED_Trigger;
}

/* ========================================================================
 *  Legacy LED pattern tracking (게이트 호환)
 * ======================================================================== */

static EN__LED_PATTERN LedOutputPattern;

void updateLED_OutputPattern(EN__LED_PATTERN Led_Pattern)
{
    LedOutputPattern = Led_Pattern;
}

EN__LED_PATTERN geteLED_OutputPattern(void)
{
    return LedOutputPattern;
}

/* ========================================================================
 *  LED output color (엔진이 설정, LED_OUT()이 GPIO 출력)
 * ======================================================================== */

static EN__LED_COLOR LED_outputColor = en__LED_BLACK;

/* ========================================================================
 *  Arbiter (Rev.3 SS3.5)
 * ======================================================================== */

static led_state_t s_req[LED_SRC__MAX];
static uint32_t    s_pair_latch_until_tick;
static bool        s_power_burst_in_progress;

void led_request(led_src_t src, led_state_t st)
{
    if (src >= LED_SRC__MAX)
    {
        return;
    }

    /* PAIR latch: 요청이 들어오면 최소 500ms 유지 */
    if (src == LED_SRC_BLE_IND && st == LED_ST_PAIR)
    {
        s_pair_latch_until_tick = ci_timer_get_tick() + 500;
    }

    s_req[src] = st;
}

bool led_is_power_burst_in_progress(void)
{
    return s_power_burst_in_progress;
}

led_state_t led_get_request(led_src_t src)
{
    if (src >= LED_SRC__MAX)
    {
        return LED_ST_NONE;
    }
    return s_req[src];
}

/* ========================================================================
 *  Pattern Engine (Rev.3 SS3.6)
 * ======================================================================== */

static void led_engine_run(led_state_t st, bool reset)
{
    const led_pattern_desc_t *p = &k_led_patterns[st];
    static uint16_t timer_ms       = 0;
    static uint8_t  burst_done_cnt = 0;

    if (reset)
    {
        timer_ms       = 0;
        burst_done_cnt = 0;
    }

    /* 지속 ON */
    if (p->period_ms == 0)
    {
        LED_outputColor = p->color;
        return;
    }

    /* 점멸 */
    LED_outputColor = (timer_ms < p->on_ms) ? p->color : en__LED_BLACK;

    timer_ms++;
    if (timer_ms >= p->period_ms)
    {
        timer_ms = 0;
        if (p->burst_cnt > 0)
        {
            burst_done_cnt++;
            if (burst_done_cnt >= p->burst_cnt)
            {
                /* 게이트 자가 해제: 기존 관례 유지 */
                updateLED_OutputPattern(en__LED_NA);
                s_req[LED_SRC_POWER]       = LED_ST_NONE;
                s_power_burst_in_progress  = false;
                burst_done_cnt             = 0;
            }
        }
    }
    else
    {
        if (p->burst_cnt > 0)
        {
            s_power_burst_in_progress = true;
        }
    }
}

/* ========================================================================
 *  Arbiter tick -- 매 iteration 호출 (Rev.3 SS3.5)
 * ======================================================================== */

void led_arbiter_tick(void)
{
    bool user_off = (readLED_indicatorOnOff() == 2);

    /* PAIR latch 처리: 해제 요청이 와도 latch 동안 유지 */
    if (s_req[LED_SRC_BLE_IND] != LED_ST_PAIR
        && ci_timer_get_tick() < s_pair_latch_until_tick)
    {
        s_req[LED_SRC_BLE_IND] = LED_ST_PAIR;
    }

    led_state_t best  = LED_ST_IDLE;
    int         max_p = -1;

    for (int src = 0; src < LED_SRC__MAX; ++src)
    {
        led_state_t st = s_req[src];
        int         p  = led_prio_of(st);

        /* 사용자 LED off: ERROR / POWER_ON / POWER_OFF 외 전부 억제 */
        if (user_off && !led_is_error(st)
            && st != LED_ST_POWER_ON && st != LED_ST_POWER_OFF)
        {
            continue;
        }

        if (p > max_p)
        {
            max_p = p;
            best  = st;
        }
    }

    /* Legacy pattern 업데이트 (게이트 호환) */
    EN__LED_PATTERN legacy = led_state_to_enum(best);
    updateLED_OutputPattern(legacy);

    /* 패턴 엔진 구동 */
    static led_state_t prev_best = LED_ST_NONE;
    bool reset = (prev_best != best);
    prev_best  = best;

    led_engine_run(best, reset);

    /* GPIO 출력 */
    LED_OUT();
}

/* ========================================================================
 *  LedPatternOut -- Legacy 래퍼 (직접 호출 시 하위 호환)
 * ======================================================================== */

void LedPatternOut(EN__LED_PATTERN ledOutputPattern)
{
    /* 새 아키텍처에서는 led_arbiter_tick()이 모든 처리를 담당.
     * 이 함수는 기존 호출 지점 호환을 위해 남겨둔 빈 래퍼.
     * LED 출력은 led_arbiter_tick() 내에서 이루어진다. */
    (void) ledOutputPattern;
}

/* ========================================================================
 *  Direct color / utility (기존 유지)
 * ======================================================================== */

void LED_black(void)
{
    LED_outputColor = en__LED_BLACK;
}

void LED_White(void)
{
    LED_outputColor = en__LED_WHITE;
}

void LED_Memory_error(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void LED_clock_error(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnOffLED(void)
{
    LED_outputColor = en__LED_BLACK;
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnON_RedLED(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnON_GreenLED(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

void turnON_BlueLED(void)
{
#if defined(LED_IS_ACTIVELOW)
#if defined(LED_B_pin_CFX_test)
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
#else
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif

#else
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

/* ========================================================================
 *  LED_OUT -- GPIO 출력 (기존 그대로 유지)
 * ======================================================================== */

#if defined(LED_IS_ACTIVELOW)

#if defined(LED_B_pin_CFX_test)
void LED_OUT(void)
{

    const int pwmTime_ms   = 10;
    int       pwmDuty_rate = 3;

    static int timerCounter = 1;

    bool enablePatternOut;

    if (0 == timerCounter % pwmDuty_rate)
        enablePatternOut = false;
    else
        enablePatternOut = true;

    if (enablePatternOut)
    {
        switch (LED_outputColor)
        {

            case en__LED_BLACK:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_RED:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_GREEN:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_BLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_ORANGE:
            {

                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_SKYBLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_PURPLE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
            case en__LED_WHITE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;

            default:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
            }
            break;
        }
    }
    else
    {

        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    }

#if 1  // LED PWM 사용할 경우

    timerCounter++;
    if (timerCounter >= pwmTime_ms)
        timerCounter = 1;

#endif
}

#else

void LED_OUT(void)
{

    const int pwmTime_ms   = 10;
    int       pwmDuty_rate = 3;

    static int timerCounter = 1;

    bool enablePatternOut;

    if (0 == timerCounter % pwmDuty_rate)
        enablePatternOut = false;
    else
        enablePatternOut = true;

    if (enablePatternOut)
    {
        switch (LED_outputColor)
        {

            case en__LED_BLACK:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_RED:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_GREEN:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_BLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_ORANGE:
            {

                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_SKYBLUE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_PURPLE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_WHITE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;

            default:
            {

                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
        }
    }
    else
    {

        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
    }

#if 1  // LED PWM 사용할 경우

    timerCounter++;
    if (timerCounter >= pwmTime_ms)
        timerCounter = 1;

#endif

    if (isTestTriggerEanbled())
    {
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
    }
}

#endif  //

#else
void LED_OUT(void)
{
    const int pwmTime_ms   = 10;
    int       pwmDuty_rate = 2;  // 3;

    static int timerCounter = 1;

    bool enablePatternOut;

    if (0 == timerCounter % pwmDuty_rate)
    {
        enablePatternOut = false;
    }
    else
    {
        enablePatternOut = true;
    }

    if (enablePatternOut)
    {
        switch (LED_outputColor)
        {
            case en__LED_BLACK:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_RED:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_GREEN:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_BLUE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_ORANGE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_SKYBLUE:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_PURPLE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            case en__LED_WHITE:
            {
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
            default:
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
            }
            break;
        }
    }
    else
    {
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
    }

#if 0  // LED PWM 사용할 경우

    timerCounter++;

    if (timerCounter >= pwmTime_ms)
    {
        timerCounter = 1;
    }

#endif

    if (isTestTriggerEanbled())
    {
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
    }
}

#endif
