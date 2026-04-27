
#include <hw.h>
#include <stdbool.h>

#include "processorDirective.h"
#include "board.h"
#include "LedOutput.h"
#include "cfx_cm3_sharedMemory.h"

#include <ci_timer.h>
#include <ci_util.h>  /* delay_ms */

/* ========================================================================
 *  LED Dimming (타이머 3 1ms tick 기반 PWM 듀티 변조)
 * ========================================================================
 *  led_engine_run() 이 매 1ms 호출되어 패턴 디스크립터 + 경과 시간으로
 *  perceived brightness(0~255) 를 산출, CIE 1931 L* 곡선 LUT 로
 *  physical PWM duty 로 변환 → LED_OUT() 이 GPIO ON/OFF 결정.
 *
 *  Fade 효과:
 *   - 점멸 ON 구간:  fade-in / peak / fade-out
 *                    (fade 시간은 ON 시간에 맞춰 자동 조정 — on_ms / 3,
 *                     단 LED_DIMMING_FADE_MAX_MS 를 상한으로 cap)
 *                    → 모든 패턴에서 정점 (max brightness) 도달 보장
 *   - 색상 전환:     이전 색 fade-out (FADE_MAX_MS) 후 새 색 fade-in (FADE_MAX_MS)
 *   - 지속 ON:       perceived = 255 → PWM full duty
 *
 *  사람 눈 인지 곡선 (CIE 1931 Lightness L*):
 *   - 사람 눈은 밝기에 비선형 반응 (Stevens' Power Law: ≈ luminance^0.33)
 *   - 시간에 따른 perceived 를 선형 증가시키면 사람 눈에 균등하게 인식됨
 *   - LUT 가 perceived (0~255) 를 physical PWM (0~255) 으로 비선형 매핑
 *   - 출처: https://en.wikipedia.org/wiki/Relative_luminance
 *           https://en.wikipedia.org/wiki/Stevens%27s_power_law
 * ======================================================================== */
#define LED_DIMMING_FADE_MAX_MS  30//15   /* fade-in / fade-out 시간 상한 */
#define LED_DIMMING_FADE_DIVISOR 3    /* 점멸 fade 자동 조정: on_ms / N */
#define LED_DIMMING_PWM_STEPS    10   /* 1ms × 10 = 10ms = 100Hz PWM */

static uint8_t s_led_pwm_on_count = LED_DIMMING_PWM_STEPS;  /* 0 ~ STEPS */

/* ISR 에서 engine/LED_OUT 을 일시 정지하는 플래그.
 * turnOffLED() 처럼 main loop 가 직접 LED state 를 조작하는 구간의
 * ISR engine 경쟁 방지용. set → 수동 fade → clear 순서로 사용. */
static volatile bool s_led_isr_suspended = false;

/* 비차단 fade-off 상태머신 — turnOffLED() / led_force_fade_off() 진입 시
 * ACTIVE 로 전환되며, ISR 의 led_arbiter_tick() 이 매 tick step 진행. */
typedef enum
{
    LED_FADE_OFF_IDLE   = 0,
    LED_FADE_OFF_ACTIVE,
} led_fade_off_state_t;

static volatile led_fade_off_state_t s_fade_off_state = LED_FADE_OFF_IDLE;
static volatile uint16_t             s_fade_off_t     = 0;
static volatile uint16_t             s_fade_off_max   = 0;  /* 30 (turnOff) / 40 (force) */

/* CFX_0 / FIFO_5 ISR 활성 여부 — initialize.c 에서 set / clear.
 * turnOffLED() / led_force_fade_off() 가 ISR 의존 fade-off vs. 즉시 OFF
 * 분기 결정에 사용. */
static volatile bool s_led_isr_active = false;

/* arbiter ISR 이 정상 구동 가능한 상태인가? (활성 + 일시정지 아님) */
static inline bool led_arbiter_can_run(void)
{
    return s_led_isr_active && !s_led_isr_suspended;
}

void led_isr_active_set(bool active)
{
    s_led_isr_active = active;
}

/* 색상 전환 cross-fade 상태머신
 *   FADE_OUT : 이전 색을 perceived 255 → 0 으로 감소 출력 (timer_ms 진행 보류)
 *   FADE_IN  : 새 색을 perceived 0 → 255 로 증가 출력 (패턴 진행 정상)
 *   NONE     : 평상시 (점멸 fade-in/out 만 적용)
 */
typedef enum
{
    LED_TX_NONE = 0,
    LED_TX_FADE_OUT,
    LED_TX_FADE_IN,
} led_tx_phase_t;

static led_tx_phase_t s_tx_phase      = LED_TX_NONE;
static EN__LED_COLOR  s_tx_prev_color = en__LED_BLACK;
static uint16_t       s_tx_ms         = 0;

/* CIE 1931 Lightness (L*) → relative luminance LUT.
 *
 * 입력: perceived brightness 0~255 (시간 진행을 이 값 선형 증가)
 * 출력: physical PWM duty 0~255 (LED 실제 출력 강도)
 *
 * 표는 Python 으로 자동 생성:
 *   L = i * 100 / 255              (입력 0~255 → CIE L* 0~100)
 *   Y = ((L + 16) / 116) ^ 3       if L > 8
 *   Y = L / 903.3                  if L ≤ 8
 *   PWM = round(Y * 255)
 *
 * 시간에 따라 입력을 선형 증가시키면 사람 눈에 밝기가 균등하게 변하는
 * 것처럼 보인다. 단조 증가, LUT[0]=0, LUT[255]=255 보장.
 */
static const uint8_t k_perceptual_lut[256] = {
      0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   3,   4,
      4,   4,   4,   4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   6,   7,
      7,   7,   7,   8,   8,   8,   8,   9,   9,   9,  10,  10,  10,  10,  11,  11,
     11,  12,  12,  12,  13,  13,  13,  14,  14,  15,  15,  15,  16,  16,  17,  17,
     17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  24,  25,
     25,  26,  26,  27,  28,  28,  29,  29,  30,  31,  31,  32,  32,  33,  34,  34,
     35,  36,  37,  37,  38,  39,  39,  40,  41,  42,  43,  43,  44,  45,  46,  47,
     47,  48,  49,  50,  51,  52,  53,  54,  54,  55,  56,  57,  58,  59,  60,  61,
     62,  63,  64,  65,  66,  67,  68,  70,  71,  72,  73,  74,  75,  76,  77,  79,
     80,  81,  82,  83,  85,  86,  87,  88,  90,  91,  92,  94,  95,  96,  98,  99,
    100, 102, 103, 105, 106, 108, 109, 110, 112, 113, 115, 116, 118, 120, 121, 123,
    124, 126, 128, 129, 131, 132, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150,
    152, 154, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181,
    183, 185, 187, 189, 191, 193, 196, 198, 200, 202, 204, 207, 209, 211, 214, 216,
    218, 220, 223, 225, 228, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
};

/* 점멸 패턴 fade 시간 자동 조정 — on_ms / 3, 단 MAX 까지 cap.
 * 결과: ON 구간 안에서 반드시 정점 (perceived 255) 도달 + peak 유지 ≥ on_ms/3. */
static uint16_t calc_pattern_fade_ms(uint16_t on_ms)
{
    uint16_t one_third = on_ms / LED_DIMMING_FADE_DIVISOR;
    return (one_third < LED_DIMMING_FADE_MAX_MS) ? one_third : LED_DIMMING_FADE_MAX_MS;
}

/* 점멸 패턴 내 시간 t 의 perceived brightness (선형 0~255).
 * fade 시간이 ON 시간에 맞춰 자동 조정되어 정점 도달 보장. */
static uint8_t calc_perceived_pattern(uint16_t t, uint16_t on_ms, uint16_t period_ms)
{
    if (period_ms == 0)  return 255;  /* 지속 ON */
    if (t >= on_ms)      return 0;    /* OFF 구간 */

    uint16_t fade_ms = calc_pattern_fade_ms(on_ms);
    if (fade_ms == 0)    return 255;  /* on_ms < 3 (이론상 미발생, 안전 가드) */

    /* fade-in 영역 */
    if (t < fade_ms)
    {
        return (uint8_t) ((t * 255UL) / fade_ms);
    }
    /* fade-out 영역 */
    if (t >= (uint16_t) (on_ms - fade_ms))
    {
        return (uint8_t) (((on_ms - t) * 255UL) / fade_ms);
    }
    /* 정점 */
    return 255;
}

/* perceived (0~255) → PWM step (0~LED_DIMMING_PWM_STEPS) — CIE L* LUT 경유 */
static uint8_t perceived_to_pwm(uint8_t perceived)
{
    return (uint8_t) (((uint32_t) k_perceptual_lut[perceived] * LED_DIMMING_PWM_STEPS) / 255);
}

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

    [LED_ST_MAPPING_ISD_BATT_READY]    = { en__LED_BLUE,   200, 1000, 0 },  // 파랑  ON 200ms  / OFF 800ms 점멸  (>20%, ISD 연결)
    [LED_ST_MAPPING_NO_ISD_BATT_READY] = { en__LED_BLUE,     0,    0, 0 },  // 파랑 지속 ON                       (>20%, ISD 미연결)
    [LED_ST_MAPPING_ISD_BATT_LOW]      = { en__LED_PURPLE, 200, 1000, 0 },  // 보라  ON 200ms  / OFF 800ms 점멸  (≤20%, ISD 연결)
    [LED_ST_MAPPING_NO_ISD_BATT_LOW]   = { en__LED_PURPLE,   0,    0, 0 },  // 보라 지속 ON                       (≤20%, ISD 미연결)

    [LED_ST_PAIR]           = { en__LED_BLUE,   500, 1000, 0 },   // 파랑  ON 500ms  / OFF 500ms 점멸 (1주기 1000ms)
    [LED_ST_OTA_QCC]        = { en__LED_GREEN,  1100, 2200, 0 },   // 녹색  ON 1100ms / OFF 1100ms
    [LED_ST_OTA_EZAIRO]     = { en__LED_GREEN,  180,  360,  0 },   // 녹색  ON 180ms  / OFF 180ms

    [LED_ST_ERROR_MAP]      = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_MCU]      = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_ACCEL]    = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_FPGA]     = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [LED_ST_ERROR_PMIC]     = { en__LED_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms

    /* 게이트 — ON 180ms · OFF 180ms, fade 15 · peak 150 · fade 15 */
    [LED_ST_POWER_ON]       = { en__LED_SKYBLUE, 180, 360,  5 },   // SKYBLUE ON 180ms / OFF 180ms × 5회 버스트
    [LED_ST_POWER_OFF]      = { en__LED_BLUE,    180, 360,  4 },   // BLUE    ON 180ms / OFF 180ms × 4회 버스트
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

        case LED_ST_MAPPING_ISD_BATT_READY:
        case LED_ST_MAPPING_NO_ISD_BATT_READY:
        case LED_ST_MAPPING_ISD_BATT_LOW:
        case LED_ST_MAPPING_NO_ISD_BATT_LOW: return 75;

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

    /* PAIR latch: 요청이 들어오면 한 주기(1000ms) 보장 — ON 500/OFF 500 패턴 1회 표시 */
    if (src == LED_SRC_BLE_IND && st == LED_ST_PAIR)
    {
        s_pair_latch_until_tick = ci_timer_get_tick() + 1000;
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
 *  Force fade-off — 절전 진입 직전 cross-fade Phase A 보장
 * ========================================================================
 * 문제: POWER_OFF burst 자가 해제 후 다음 led_arbiter_tick() 에서 best 가
 *       BATTERY (BATT_READY 등) 로 변경 → cross-fade Phase B 가 새 색을
 *       LED_outputColor 에 주입 → 곧이은 main loop break → func_sleep() →
 *       turnOffLED() 가 새 색 (예: GREEN) 을 fade-out → 잔상색 인지.
 *
 * 해법: break 전에 모든 src 를 LED_ST_NONE 으로 강제 → best = IDLE → cross-fade
 *       Phase A 가 현재 색 (LED_outputColor) 을 prev_color 로 캡처하고 자연
 *       fade-out → 도달 후 BLACK 안정. 그 후 turnOffLED() 호출 시 이미 BLACK
 *       이므로 잔상 없음.
 * ======================================================================== */
void led_force_fade_off(void)
{
    /* 모든 src 강제 NONE — Arbiter 가 즉시 IDLE 결정하도록 */
    for (int src = 0; src < LED_SRC__MAX; src++)
    {
        s_req[src] = LED_ST_NONE;
    }

    /* PAIR latch 도 무효화 — 잔존 latch 가 IDLE 결정을 막지 않게 */
    s_pair_latch_until_tick = 0;

    /* Phase A 진행 + 안정화 마진. led_arbiter_tick() 이 매 호출 시 LED_OUT()
     * 까지 처리하므로 GPIO 도 같이 갱신. */
    for (int i = 0; i < LED_DIMMING_FADE_MAX_MS + 10; i++)
    {
        led_arbiter_tick();
        delay_ms(1);
    }

    /* ISR (TIMER_3 / CFX_0 / FIFO_5) 의 led_arbiter_tick() 호출을 영구 차단.
     *
     * led_arbiter_tick() 은 main loop 가 아니라 위 3 개 ISR 에서 직접 호출된다.
     * main loop 가 break 후 func_sleep() 안 (ResetNRF / NRF_Off / QCC SHUTDOWN /
     * PMIC OFF / turnOffLED / ci_power_sleep ...) 진행 사이에 IRQ 가 발생하면
     * led_arbiter_tick() → LED_OUT() 이 실행되어 GPIO 가 새 색으로 갱신될 수
     * 있다. 이번 fade-off 이후 절전 진입까지는 LED 상태가 더 변하면 안 되므로
     * 영구 suspend 한다. (다음 부팅 시 static 변수 초기값 false 로 자연 reset.) */
    s_led_isr_suspended = true;
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

        /* Cross-fade 진입 결정 — 진행 중인 fade-out 은 그대로 둔다 */
        if (s_tx_phase != LED_TX_FADE_OUT)
        {
            EN__LED_COLOR new_color = p->color;
            if (LED_outputColor != en__LED_BLACK && LED_outputColor != new_color)
            {
                /* 이전 색이 켜져 있고 새 색이 다르면 fade-out 부터 */
                s_tx_phase      = LED_TX_FADE_OUT;
                s_tx_prev_color = LED_outputColor;
                s_tx_ms         = 0;
            }
            else
            {
                /* 이전이 OFF 였거나 같은 색 → fade-in 직행 */
                s_tx_phase = LED_TX_FADE_IN;
                s_tx_ms    = 0;
            }
        }
    }

    /* Phase A — 이전 색 fade-out (perceived 곡선 적용). timer_ms 진행 보류 */
    if (s_tx_phase == LED_TX_FADE_OUT)
    {
        LED_outputColor = s_tx_prev_color;

        /* perceived 255 → 0 으로 선형 감소, LUT 통해 PWM 변환 */
        uint8_t perceived = (uint8_t) (((LED_DIMMING_FADE_MAX_MS - s_tx_ms) * 255UL)
                                       / LED_DIMMING_FADE_MAX_MS);
        s_led_pwm_on_count = perceived_to_pwm(perceived);

        s_tx_ms++;
        if (s_tx_ms >= LED_DIMMING_FADE_MAX_MS)
        {
            /* fade-out 완료 → 다음 tick 부터 Phase B (새 패턴 시작) */
            s_tx_phase = LED_TX_FADE_IN;
            s_tx_ms    = 0;
        }
        return;
    }

    /* Phase B (또는 NONE) — 새 패턴 정상 진행 */

    /* 출력 색상 결정 */
    if (p->period_ms == 0)
    {
        LED_outputColor = p->color;  /* 지속 ON */
    }
    else
    {
        LED_outputColor = (timer_ms < p->on_ms) ? p->color : en__LED_BLACK;
    }

    /* perceived brightness 산출 (점멸 fade-in/peak/fade-out, 자동 fade 시간) */
    uint8_t perceived = calc_perceived_pattern(timer_ms, p->on_ms, p->period_ms);

    /* Phase B — 새 색 fade-in: ratio 와 perceived 결합 (perceived 단위 곱셈) */
    if (s_tx_phase == LED_TX_FADE_IN)
    {
        uint8_t fade_in_perc = (uint8_t) ((s_tx_ms * 255UL) / LED_DIMMING_FADE_MAX_MS);
        perceived = (uint8_t) (((uint32_t) perceived * fade_in_perc) / 255);

        s_tx_ms++;
        if (s_tx_ms >= LED_DIMMING_FADE_MAX_MS)
        {
            s_tx_phase = LED_TX_NONE;
        }
    }

    /* perceived → PWM (CIE L* LUT) */
    s_led_pwm_on_count = perceived_to_pwm(perceived);

    /* 점멸 주기 진행 */
    if (p->period_ms != 0)
    {
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
}

/* ========================================================================
 *  Arbiter tick -- 매 iteration 호출 (Rev.3 SS3.5)
 * ======================================================================== */

/* Best state 산출 — fade-off 분기에서 재사용을 위해 추출.
 *
 * PAIR latch 갱신 부수효과가 있으나 idempotent (`!= LED_ST_PAIR` 가드).
 * 한 tick 내 두 번 호출되어도 동등 결과. */
static led_state_t compute_best_state(void)
{
    bool        user_off = (readLED_indicatorOnOff() == 2);
    led_state_t best     = LED_ST_IDLE;
    int         max_p    = -1;

    /* PAIR latch 처리: 해제 요청이 와도 latch 동안 유지 */
    if (s_req[LED_SRC_BLE_IND] != LED_ST_PAIR
        && ci_timer_get_tick() < s_pair_latch_until_tick)
    {
        s_req[LED_SRC_BLE_IND] = LED_ST_PAIR;
    }

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

    return best;
}

void led_arbiter_tick(void)
{
    /* turnOffLED() 등 main loop 가 직접 LED state 를 조작하는 구간에서는
     * ISR engine 을 일시 정지해 shared state 경쟁 방지. */
    if (s_led_isr_suspended)
    {
        return;
    }

    led_state_t best = compute_best_state();

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
    /* GPIO R/G/B 순차 호출 사이의 transient 로 의도치 않은 중간색이 보이는
     * 현상 회피 — 직전 LED 색이 ORANGE/SKYBLUE/PURPLE/WHITE 등 두 핀 이상
     * ON 상태였다면, R→G→B 순차 LOW 처리 사이에 단일 핀 ON 색 (GREEN/BLUE)
     * 등이 잠깐 보일 수 있다 (사용자 보고: BLUE 깜빡 후 SKYBLUE 잔상).
     *
     * perceived 곡선으로 brightness 를 점진 감소시키면서 LED_OUT() 을 통해
     * GPIO 를 갱신. PWM duty 0 도달 후엔 LED_OUT() 의 OFF 분기 (GPIO 모두 OFF
     * base) 만 실행 → 추가 GPIO 변화 없음. 마지막 LED_outputColor 도 BLACK
     * 으로 명시 적용.
     *
     * ISR engine 과 경쟁 방지 — 수동 fade 구간 동안 s_led_isr_suspended 로
     * Timer 3 ISR 의 led_arbiter_tick() 일시 정지. */
    s_led_isr_suspended = true;

    for (uint16_t t = 0; t < LED_DIMMING_FADE_MAX_MS; t++)
    {
        uint8_t perceived = (uint8_t) (((uint32_t) (LED_DIMMING_FADE_MAX_MS - t) * 255UL)
                                       / LED_DIMMING_FADE_MAX_MS);
        s_led_pwm_on_count = perceived_to_pwm(perceived);
        LED_OUT();
        delay_ms(1);
    }

    LED_outputColor    = en__LED_BLACK;
    s_led_pwm_on_count = 0;
    LED_OUT();

    s_led_isr_suspended = false;
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
 *  LED 색상별 R/G/B PWM duty cap (%) — 조합색 색감 교정
 * ========================================================================
 *  저항 R13=R14=R15=1.2kΩ + 3.3V_STBY 고정 회로에서 각 조합색의 체감
 *  색감을 의도와 일치시키기 위해 색상마다 R/G/B 채널별 cap 을 독립 설정.
 *
 *  단순 per-channel gain 은 단색 · 조합색 둘 다 동시 만족 불가 — 예를 들어
 *  GREEN 단색을 밝게 유지하면서 ORANGE 의 G 만 약화하는 게 필요한데,
 *  단일 gain 으로는 두 조건 충돌. 색상별 per-channel cap 테이블로 해결.
 *
 *  동작:
 *   - base_duty (s_led_pwm_on_count, 0..LED_DIMMING_PWM_STEPS) 는 Dimming
 *     (perceived + CIE L*) 결과.
 *   - 색상 → cap lookup → 채널별 duty = base × cap / 100
 *   - 채널별 timerCounter 비교로 per-channel PWM 출력
 *
 *  주의:
 *   - PWM_STEPS=10 → cap 10% 단위 양자화.
 *   - 아래 값은 실측 튜닝 전 초안. 보드 테스트 후 육안 매칭으로 갱신.
 * ======================================================================== */

typedef struct
{
    uint8_t cap_pc_r;  /* 0..100 */
    uint8_t cap_pc_g;  /* 0..100 */
    uint8_t cap_pc_b;  /* 0..100 */
} tdc_led_mix_t;

static const tdc_led_mix_t k_led_mix[] = {
    [en__LED_BLACK]   = {   0,   0,   0 },
    [en__LED_RED]     = { 100,   0,   0 },  /* 1.0× 기준 */
    [en__LED_GREEN]   = {   0,  50,   0 },  /* G 감쇠 — 체감 2× 보정 */
    [en__LED_BLUE]    = {   0,   0, 100 },  /* 1.0× 기준 */
    [en__LED_ORANGE]  = {  80,  10,   0 },  /* R 우세 + G 최소 → 주황 */
    [en__LED_SKYBLUE] = {   0,  30,  40 },  /* 총 광량 감쇠 (원 SKYBLUE 가 최고 밝음) */
    [en__LED_PURPLE]  = {  50,   0,  50 },  /* 총 광량 감쇠 */
    [en__LED_WHITE]   = {  30,  30,  30 },  /* G 비중 ↑ → 연보라 제거 */
};

/* ========================================================================
 *  GPIO 극성 helper — Active HIGH / Active LOW / B pin 유무 컴파일 타임 분기
 * ======================================================================== */

static void tdc_led_write_gpio(bool on_r, bool on_g, bool on_b)
{
#if defined(LED_IS_ACTIVELOW)
    if (on_r) Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    else      Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    if (on_g) Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    else      Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
  #if !defined(LED_B_pin_CFX_test)
    if (on_b) Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
    else      Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
  #else
    (void) on_b;
  #endif
#else  /* Active HIGH (Board_OTE_ver1_5 기본) */
    if (on_r) Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
    else      Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
    if (on_g) Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
    else      Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
    if (on_b) Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
    else      Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
#endif
}

/* ========================================================================
 *  LED_OUT — 색상 × Dimming × PWM → GPIO 출력
 * ======================================================================== */

void LED_OUT(void)
{
    static int timerCounter = 0;

    /* Test trigger: 강제 BLUE. 테이블 lookup 으로 일관 처리. */
    EN__LED_COLOR color = isTestTriggerEanbled() ? en__LED_BLUE : LED_outputColor;

    /* 배열 bound 가드 — 미등록 색상은 OFF 로 처리 */
    const tdc_led_mix_t *mix;
    if ((unsigned) color < (sizeof(k_led_mix) / sizeof(k_led_mix[0])))
    {
        mix = &k_led_mix[color];
    }
    else
    {
        static const tdc_led_mix_t k_off = { 0, 0, 0 };
        mix = &k_off;
    }

    /* 채널별 감쇠 duty — base × cap / 100 */
    uint8_t duty_r = (uint8_t) ((uint32_t) s_led_pwm_on_count * mix->cap_pc_r / 100U);
    uint8_t duty_g = (uint8_t) ((uint32_t) s_led_pwm_on_count * mix->cap_pc_g / 100U);
    uint8_t duty_b = (uint8_t) ((uint32_t) s_led_pwm_on_count * mix->cap_pc_b / 100U);

    /* 채널별 PWM 비교 (동일 timerCounter, 서로 다른 duty cap) */
    bool on_r = (timerCounter < duty_r);
    bool on_g = (timerCounter < duty_g);
    bool on_b = (timerCounter < duty_b);

    tdc_led_write_gpio(on_r, on_g, on_b);

    /* PWM 카운터 진행 */
    timerCounter++;
    if (timerCounter >= LED_DIMMING_PWM_STEPS)
    {
        timerCounter = 0;
    }
}
