
#include <hw.h>
#include <stdbool.h>

#include "processorDirective.h"
#include "board.h"
#include "tdc_led_output.h"
#include "cfx_cm3_sharedMemory.h"

#include <tdc_hal_timer.h>
#include <tdc_util.h>  /* tdc_util_delay_ms */
#include <tdc_printf.h>

/* ========================================================================
 *  LED Dimming (타이머 3 1ms tick 기반 PWM 듀티 변조)
 * ========================================================================
 *  led_engine_run() 이 매 1ms 호출되어 패턴 디스크립터 + 경과 시간으로
 *  perceived brightness(0~255) 를 산출, CIE 1931 L* 곡선 LUT 로
 *  physical PWM duty 로 변환 → tdc_led_out() 이 GPIO ON/OFF 결정.
 *
 *  Fade 효과:
 *   - 점멸 ON 구간:  fade-in / peak / fade-out
 *                    (fade 시간은 ON 시간에 맞춰 자동 조정 - on_ms / 3,
 *                     단 LED_DIMMING_FADE_MAX_MS 를 상한으로 cap)
 *                    → 모든 패턴에서 정점 (max brightness) 도달 보장
 *   - 색상 전환:     이전 색 fade-out (FADE_MAX_MS) 후 새 색 fade-in (FADE_MAX_MS)
 *   - 지속 ON:       perceived = 255 → PWM full duty
 *
 *  사람 눈 인지 곡선 (CIE 1931 Lightness L*):
 *   - 사람 눈은 밝기에 비선형 반응 (Stevens' Power Law: ~ luminance^0.33)
 *   - 시간에 따른 perceived 를 선형 증가시키면 사람 눈에 균등하게 인식됨
 *   - LUT 가 perceived (0~255) 를 physical PWM (0~255) 으로 비선형 매핑
 *   - 출처: https://en.wikipedia.org/wiki/Relative_luminance
 *           https://en.wikipedia.org/wiki/Stevens%27s_power_law
 * ======================================================================== */
#define LED_DIMMING_FADE_MAX_MS  30//15   /* 점멸 패턴 매 cycle fade 시간 상한 (ON 시작 fade-in / ON 끝 fade-out) */
#define LED_DIMMING_FADE_DIVISOR 3    /* 점멸 fade 자동 조정: on_ms / N */

/* 색상 전환 cross-fade 시간 (s_tx_phase 의 FADE_OUT / FADE_IN 각각).
 *
 * 점멸 패턴 매 cycle 내부 fade (LED_DIMMING_FADE_MAX_MS = 30 ms) 와 분리.
 * burst 내부 fade 의 2 배 → 색상 전환이 "신중한 동작" 으로 인지되어
 * 연속 정보 갱신 (BATT 색 변화 등) 의 부드러움 ↑. 모든 LED 상태 전환 시
 * 1 회 적용 (지속 ON, 점멸, burst 모두 진입 시 동일하게 1 회 fade-out / fade-in). */
#define LED_DIMMING_TX_FADE_MS   60

/* POWER_ON / POWER_OFF 진입 시 leading OFF 시간 (음악적 쉼표).
 *
 * 두 단계 사용자 인지 시퀀스의 분리감 보전 - burst 시퀀스 직전에 명확한
 * OFF gap 을 확보해 직전 색/이벤트 (부트로더 SKYBLUE 또는 BATT 색 등) 와
 * 본 burst 가 시각적으로 분리되도록 한다.
 *
 * - POWER_ON: 부트로더 → CM3 인계 gap 보전 (부트로더 OFF phase 180 → 0
 *   단축 대응, bootloader-power-on-indicator §8). 부트로더 자체 잔여
 *   ~100 ms + 본 매크로 400 ms ~ 500 ms 분리.
 * - POWER_OFF: 직전 색 (BATT 녹/노랑/주황 등) cross-fade FADE_OUT 후
 *   본 매크로 시간 OFF → BLUE burst.
 *
 * 적정 가이드: burst 내부 OFF (180 ms) 의 약 2~3 배 (음악 ~1 박자 휴식). */
#define LED_POWER_LEAD_OFF_MS    400
#define LED_DIMMING_PWM_STEPS    10   /* 1ms × 10 = 10ms = 100Hz PWM */

static uint8_t s_led_pwm_on_count = LED_DIMMING_PWM_STEPS;  /* 0 ~ STEPS */

/* ISR 에서 engine/tdc_led_out 을 일시 정지하는 플래그.
 * tdc_led_turn_off() 처럼 main loop 가 직접 LED state 를 조작하는 구간의
 * ISR engine 경쟁 방지용. set → 수동 fade → clear 순서로 사용. */
static volatile bool s_led_isr_suspended = false;

/* 비차단 fade-off 상태머신 - tdc_led_turn_off() / tdc_led_force_fade_off() 진입 시
 * ACTIVE 로 전환되며, ISR 의 tdc_led_arbiter_tick() 이 매 tick step 진행. */
typedef enum
{
    LED_FADE_OFF_IDLE   = 0,
    LED_FADE_OFF_ACTIVE,
} led_fade_off_state_t;

static volatile led_fade_off_state_t s_fade_off_state = LED_FADE_OFF_IDLE;
static volatile uint16_t             s_fade_off_t     = 0;
static volatile uint16_t             s_fade_off_max   = 0;  /* 30 (turnOff) / 40 (force) */

/* CFX_0 / FIFO_5 ISR 활성 여부 - initialize.c 에서 set / clear.
 * tdc_led_turn_off() / tdc_led_force_fade_off() 가 ISR 의존 fade-off vs. 즉시 OFF
 * 분기 결정에 사용. */
static volatile bool s_led_isr_active = false;

static volatile int s_isd_conn = false;

void tdc_led_set_isd_conn_state(int state)
{
    s_isd_conn = state;
}

/* arbiter ISR 이 정상 구동 가능한 상태인가? (활성 + 일시정지 아님) */
static inline bool led_arbiter_can_run(void)
{
    return s_led_isr_active && !s_led_isr_suspended;
}

void tdc_led_isr_active_set(bool active)
{
    s_led_isr_active = active;
}

/* 색상 전환 cross-fade 상태머신
 *   FADE_OUT : 이전 색을 perceived 255 → 0 으로 감소 (LED_DIMMING_TX_FADE_MS, timer_ms 보류)
 *   LEAD_OFF : POWER_ON / POWER_OFF 진입 시 leading OFF (LED_POWER_LEAD_OFF_MS, timer_ms 보류)
 *   FADE_IN  : 새 색을 perceived 0 → 255 로 증가 (LED_DIMMING_TX_FADE_MS, 패턴 진행 정상)
 *   NONE     : 평상시 (점멸 fade-in/out 만 적용)
 */
typedef enum
{
    LED_TX_NONE = 0,
    LED_TX_FADE_OUT,
    LED_TX_LEAD_OFF,
    LED_TX_FADE_IN,
} led_tx_phase_t;

static led_tx_phase_t s_tx_phase      = LED_TX_NONE;
static tdc_led_color_t  s_tx_prev_color = TDC_LED_COLOR_BLACK;
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

/* 점멸 패턴 fade 시간 자동 조정 - on_ms / 3, 단 MAX 까지 cap.
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

/* perceived (0~255) → PWM step (0~LED_DIMMING_PWM_STEPS) - CIE L* LUT 경유 */
static uint8_t perceived_to_pwm(uint8_t perceived)
{
    return (uint8_t) (((uint32_t) k_perceptual_lut[perceived] * LED_DIMMING_PWM_STEPS) / 255);
}

/* ========================================================================
 *  OTA DFU 테스트용 FW 이미지 변종 - LED 색상 분기
 * ========================================================================
 *  하나의 펌웨어 소스가 운용용 App 이미지와 Factory Reset 이미지 두 변종으로
 *  빌드되어 OTA DFU 테스트 시 어느 이미지가 동작 중인지 LED 색으로 즉시
 *  식별 가능하도록 한다.
 *
 *  변종         | TDC_LED_ST_IN_USE | TDC_LED_ST_POWER_ON
 *  -------------|---------------|------------------
 *  APP (운용)   | WHITE         | SKYBLUE
 *  FACT_RESET   | PURPLE        | WHITE
 *
 *  사용:
 *   - 운용/정식 빌드: 매크로 미정의 → 자동 디폴트 APP. 별도 작업 불필요.
 *   - Factory Reset 테스트 빌드: 본 파일 디폴트를 FACTORY_RESET 으로 변경
 *                                또는 빌드 옵션 -DTDC_FW_VARIANT=TDC_FW_VARIANT_FACTORY_RESET.
 * ======================================================================== */
#define TDC_FW_VARIANT_APP             0
#define TDC_FW_VARIANT_FACTORY_RESET   1

#ifndef TDC_FW_VARIANT
#define TDC_FW_VARIANT  TDC_FW_VARIANT_APP   /* 디폴트: 운용 (APP) */
#endif

#if (TDC_FW_VARIANT == TDC_FW_VARIANT_FACTORY_RESET)
    #define TDC_FW_LED_IN_USE_COLOR    TDC_LED_COLOR_PURPLE
    #define TDC_FW_LED_POWER_ON_COLOR  TDC_LED_COLOR_WHITE
#elif (TDC_FW_VARIANT == TDC_FW_VARIANT_APP)
    #define TDC_FW_LED_IN_USE_COLOR    TDC_LED_COLOR_WHITE
    #define TDC_FW_LED_POWER_ON_COLOR  TDC_LED_COLOR_SKYBLUE
#else
    #error "TDC_FW_VARIANT 미지원 값. TDC_FW_VARIANT_APP 또는 TDC_FW_VARIANT_FACTORY_RESET 만 허용."
#endif

/* ========================================================================
 *  Pattern Descriptor Table (Rev.3 SS3.4)
 * ======================================================================== */

/* 표기 안내:
 *   디스크립터 값 (on_ms, period_ms) ↔ 사람이 읽는 (ON, OFF) 환산:
 *     period_ms = on_ms + off_ms
 *   예) { on_ms=1100, period_ms=2200 }  ⇒  ON 1100ms / OFF 1100ms
 *   각 행의 // 주석에 ON/OFF 형식으로 같이 표기. */
static const tdc_led_pattern_desc_t k_led_patterns[TDC_LED_ST__MAX] = {
    [TDC_LED_ST_NONE]           = { TDC_LED_COLOR_BLACK,   0,    0,    0 },  // 지속 OFF
    [TDC_LED_ST_IDLE]           = { TDC_LED_COLOR_BLACK,   0,    0,    0 },  // 지속 OFF

    [TDC_LED_ST_BATT_READY]     = { TDC_LED_COLOR_GREEN,   0,    0,    0 },  // 녹색 지속 ON
    [TDC_LED_ST_IN_USE]         = { TDC_FW_LED_IN_USE_COLOR, 0,    0,    0 },  // 지속 ON (App=WHITE / FactRst=PURPLE)
    [TDC_LED_ST_BATT_MID]       = { TDC_LED_COLOR_ORANGE,  0,    0,    0 },  // 노랑 지속 ON
    [TDC_LED_ST_BATT_CRITICAL]  = { TDC_LED_COLOR_ORANGE,  1100, 2200, 0 },  // 노랑  ON 1100ms / OFF 1100ms

    [TDC_LED_ST_MAPPING_ISD_BATT_READY]    = { TDC_LED_COLOR_BLUE,   200, 1000, 0 },  // 파랑  ON 200ms  / OFF 800ms 점멸  (>20%, ISD 연결)
    [TDC_LED_ST_MAPPING_NO_ISD_BATT_READY] = { TDC_LED_COLOR_BLUE,     0,    0, 0 },  // 파랑 지속 ON                       (>20%, ISD 미연결)
    [TDC_LED_ST_MAPPING_ISD_BATT_LOW]      = { TDC_LED_COLOR_PURPLE, 200, 1000, 0 },  // 보라  ON 200ms  / OFF 800ms 점멸  (≤20%, ISD 연결)
    [TDC_LED_ST_MAPPING_NO_ISD_BATT_LOW]   = { TDC_LED_COLOR_PURPLE,   0,    0, 0 },  // 보라 지속 ON                       (≤20%, ISD 미연결)

    [TDC_LED_ST_PAIR]           = { TDC_LED_COLOR_BLUE,   500, 1000, 0 },   // 파랑  ON 500ms  / OFF 500ms 점멸 (1주기 1000ms)
    [TDC_LED_ST_OTA_QCC]        = { TDC_LED_COLOR_GREEN,  1100, 2200, 0 },   // 녹색  ON 1100ms / OFF 1100ms
    [TDC_LED_ST_OTA_EZAIRO]     = { TDC_LED_COLOR_GREEN,  180,  360,  0 },   // 녹색  ON 180ms  / OFF 180ms

    [TDC_LED_ST_ERROR_MAP]      = { TDC_LED_COLOR_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [TDC_LED_ST_ERROR_MCU]      = { TDC_LED_COLOR_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [TDC_LED_ST_ERROR_ACCEL]    = { TDC_LED_COLOR_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [TDC_LED_ST_ERROR_FPGA]     = { TDC_LED_COLOR_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms
    [TDC_LED_ST_ERROR_PMIC]     = { TDC_LED_COLOR_RED,    180,  360,  0 },   // 빨강  ON 180ms  / OFF 180ms

    /* 게이트 - ON 180ms · OFF 180ms, fade 30 · peak 120 · fade 30 (LED_DIMMING_FADE_MAX_MS) */
    [TDC_LED_ST_POWER_ON]       = { TDC_FW_LED_POWER_ON_COLOR, 180, 360,  4 },   // ON 180ms / OFF 180ms × 4회 버스트 (App=SKYBLUE / FactRst=WHITE)
    [TDC_LED_ST_POWER_OFF]      = { TDC_LED_COLOR_BLUE,    180, 360,  4 },   // BLUE    ON 180ms / OFF 180ms × 4회 버스트

    /* [DBG] 롱터치 무시 케이스 피드백 */
    [TDC_LED_ST_DBG_LONG_TOUCH_IGNORE] = { TDC_LED_COLOR_PURPLE, 180, 360, 3 },  // 보라 ON 180ms / OFF 180ms × 3회
};

/* ========================================================================
 *  Priority Table (Rev.3 SS3.3)
 * ======================================================================== */

static int led_prio_of(tdc_led_state_t st)
{
    switch (st)
    {
        case TDC_LED_ST_POWER_OFF:      return 100;
        case TDC_LED_ST_POWER_ON:       return 95;

        case TDC_LED_ST_ERROR_MAP:
        case TDC_LED_ST_ERROR_MCU:
        case TDC_LED_ST_ERROR_ACCEL:
        case TDC_LED_ST_ERROR_FPGA:
        case TDC_LED_ST_ERROR_PMIC:     return 90;

        case TDC_LED_ST_OTA_QCC:
        case TDC_LED_ST_OTA_EZAIRO:     return 80;

        case TDC_LED_ST_MAPPING_ISD_BATT_READY:
        case TDC_LED_ST_MAPPING_NO_ISD_BATT_READY:
        case TDC_LED_ST_MAPPING_ISD_BATT_LOW:
        case TDC_LED_ST_MAPPING_NO_ISD_BATT_LOW: return 75;

        case TDC_LED_ST_PAIR:           return 70;

        case TDC_LED_ST_BATT_CRITICAL:  return 60;

        case TDC_LED_ST_IN_USE:         return 40;

        case TDC_LED_ST_BATT_READY:     return 30;

        case TDC_LED_ST_BATT_MID:       return 20;

        case TDC_LED_ST_IDLE:           return 10;

#if TDC_LED_DBG_LONG_TOUCH_IGNORE
        case TDC_LED_ST_DBG_LONG_TOUCH_IGNORE: return 76;  /* [DBG] MAPPING(75)보다 약간 높음 */
#endif

        default:                    return 0;
    }
}

static bool led_is_error(tdc_led_state_t st)
{
    return (st >= TDC_LED_ST_ERROR_MAP && st <= TDC_LED_ST_ERROR_PMIC);
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
            tdc_led_request(TDC_LED_SRC_BLE_IND, TDC_LED_ST_PAIR);
            break;
        case TDC_LED_IND_STATE_OTA_QCC:
            tdc_led_request(TDC_LED_SRC_BLE_IND, TDC_LED_ST_OTA_QCC);
            break;
        case TDC_LED_IND_STATE_OTA_EZAIRO:
            tdc_led_request(TDC_LED_SRC_BLE_IND, TDC_LED_ST_OTA_EZAIRO);
            break;
        case TDC_LED_IND_STATE_BATT:
            /* BATT는 별도 상태 아님 -- 본체 배터리 판정이 이미 처리 (no-op) */
            break;
        case TDC_LED_IND_STATE_NONE:
        default:
            tdc_led_request(TDC_LED_SRC_BLE_IND, TDC_LED_ST_NONE);
            break;
    }
}

tdc_led_ind_state_t tdc_led_get_ind_state(void)
{
    return sg_led_ind_state;
}

void tdc_led_enable_test_trigger(void)
{
    testLED_Trigger = true;
}

void tdc_led_disable_test_trigger(void)
{
    testLED_Trigger = false;
}

bool tdc_led_is_test_trigger_enabled(void)
{
    return testLED_Trigger;
}

/* ========================================================================
 *  LED output color (엔진이 설정, tdc_led_out()이 GPIO 출력)
 * ======================================================================== */

static tdc_led_color_t LED_outputColor = TDC_LED_COLOR_BLACK;

/* ========================================================================
 *  Arbiter (Rev.3 SS3.5)
 * ======================================================================== */

static tdc_led_state_t s_req[TDC_LED_SRC__MAX];
static uint32_t    s_pair_latch_until_tick;

/* burst 패턴 (POWER_ON / POWER_OFF 등 burst_cnt > 0) 의 진행 상태 추적.
 * set 책임: `tdc_led_request()` 가 burst 패턴 요청 즉시 true (외부 호출 시점).
 * clear 책임: `led_engine_run()` 이 burst 자가 해제 시 false (LED 핸들러 내부).
 * 의도: timer/tick 무관 시작·끝 명확화 - tdc_sys_control_step 의 종료 검출 race 회피. */
static bool        s_tdc_burst_pending;

void tdc_led_request(tdc_led_src_t src, tdc_led_state_t st)
{
    if (src >= TDC_LED_SRC__MAX)
    {
        return;
    }

    /* PAIR latch: 요청이 들어오면 한 주기(1000ms) 보장 - ON 500/OFF 500 패턴 1회 표시 */
    if (src == TDC_LED_SRC_BLE_IND && st == TDC_LED_ST_PAIR)
    {
        s_pair_latch_until_tick = tdc_hal_timer_get_tick() + 1000;
    }

    /* burst 패턴 요청 즉시 pending flag set - timer 기반 set 의 timing race 회피. */
    if (st < TDC_LED_ST__MAX && k_led_patterns[st].burst_cnt > 0)
    {
        s_tdc_burst_pending = true;
    }

    s_req[src] = st;
}

bool tdc_led_is_burst_pending(void)
{
    return s_tdc_burst_pending;
}

tdc_led_state_t tdc_led_get_request(tdc_led_src_t src)
{
    if (src >= TDC_LED_SRC__MAX)
    {
        return TDC_LED_ST_NONE;
    }
    return s_req[src];
}

/* ========================================================================
 *  Force fade-off - 절전 진입 직전 cross-fade Phase A 보장
 * ========================================================================
 * 문제: POWER_OFF burst 자가 해제 후 다음 tdc_led_arbiter_tick() 에서 best 가
 *       BATTERY (BATT_READY 등) 로 변경 → cross-fade Phase B 가 새 색을
 *       LED_outputColor 에 주입 → 곧이은 main loop break → func_sleep() →
 *       tdc_led_turn_off() 가 새 색 (예: GREEN) 을 fade-out → 잔상색 인지.
 *
 * 해법: break 전에 모든 src 를 TDC_LED_ST_NONE 으로 강제 → best = IDLE → cross-fade
 *       Phase A 가 현재 색 (LED_outputColor) 을 prev_color 로 캡처하고 자연
 *       fade-out → 도달 후 BLACK 안정. 그 후 tdc_led_turn_off() 호출 시 이미 BLACK
 *       이므로 잔상 없음.
 * ======================================================================== */
void tdc_led_force_fade_off(void)
{
    /* 모든 src 강제 NONE - Arbiter 가 즉시 IDLE 결정하도록 */
    for (int src = 0; src < TDC_LED_SRC__MAX; src++)
    {
        s_req[src] = TDC_LED_ST_NONE;
    }

    /* PAIR latch 도 무효화 - 잔존 latch 가 IDLE 결정을 막지 않게 */
    s_pair_latch_until_tick = 0;

    /* 호출 사이트는 main.c:638 sleep 진입 한 곳뿐 - ISR 활성 가정.
     * 그러나 안전상 동일 검사 후 즉시 OFF + suspend. */
    if (!led_arbiter_can_run())
    {
        LED_outputColor    = TDC_LED_COLOR_BLACK;
        s_led_pwm_on_count = 0;
        tdc_led_out();
        s_led_isr_suspended = true;
        return;
    }

    /* 비차단 fade-off 시작 (40 ms) - 호출자(main.c:638) 는 break 로 즉시 main loop
     * 탈출, func_sleep() 의 NRF/QCC/PMIC OFF 처리 동안 ISR 이 자연 tick 으로
     * fade 진행 → tdc_led_turn_off() 진입 시점엔 BLACK 안정 도달 (또는 진행 중). */
    s_fade_off_state = LED_FADE_OFF_ACTIVE;
    s_fade_off_t     = 0;
    s_fade_off_max   = LED_DIMMING_TX_FADE_MS + 10;  /* cross-fade FADE_OUT 시간 + 10 ms 마진 */

    /* sleep 진입 동안 LED 보호 - fade 완료 후 ISR 차단.
     *
     * arbiter 가드 순서가 fade-off step 분기 → suspended 가드 순이라 본 set
     * 이후에도 진행 중인 fade 는 끝까지 진행된다 (tdc_led_arbiter_tick 가드 1 참조).
     * fade 완료 시 가드 1 에서 IDLE 로 reset 된 뒤로는 가드 2 (suspended) 에서
     * 차단되어 IRQ 발생해도 LED 갱신 없음.
     *
     * 다음 부팅 시 static 변수 초기값 false 로 자연 reset. */
    s_led_isr_suspended = true;
}

/* ========================================================================
 *  Pattern Engine (Rev.3 SS3.6)
 * ======================================================================== */

static void led_engine_run(tdc_led_state_t st, bool reset)
{
    const tdc_led_pattern_desc_t *p = &k_led_patterns[st];
    static uint16_t timer_ms       = 0;
    static uint8_t  burst_done_cnt = 0;

    if (reset)
    {
        timer_ms       = 0;
        burst_done_cnt = 0;

        /* `s_tdc_burst_pending` 의 set 책임은 `tdc_led_request()` 이관 - 본 위치 set 제거
         * (Fix B-LED-2 추가분 폐기). tdc_led_request 시점 set 으로 fade-out Phase A 동안
         * 에도 pending true 보장 → tdc_sys_control_step 종료 검출 race 본질적 해소. */

        /* Cross-fade 진입 결정 - 진행 중인 fade-out 은 그대로 둔다 */
        if (s_tx_phase != LED_TX_FADE_OUT)
        {
            tdc_led_color_t new_color = p->color;
            if (LED_outputColor != TDC_LED_COLOR_BLACK && LED_outputColor != new_color)
            {
                /* 이전 색이 켜져 있고 새 색이 다르면 fade-out 부터 */
                s_tx_phase      = LED_TX_FADE_OUT;
                s_tx_prev_color = LED_outputColor;
                s_tx_ms         = 0;
            }
            else if (st == TDC_LED_ST_POWER_ON || st == TDC_LED_ST_POWER_OFF)
            {
                /* POWER_ON / POWER_OFF 진입 - 직전 시각 사건과 burst 사이에
                 * 분리감 보전 (음악적 쉼표). leading OFF 후 FADE_IN 자가 전이.
                 * (이전 색이 켜진 채 색상이 다르면 위 FADE_OUT 분기로 들어가
                 *  fade-out 종료 시 LEAD_OFF 로 다시 분기된다.) */
                s_tx_phase = LED_TX_LEAD_OFF;
                s_tx_ms    = 0;
            }
            else
            {
                /* 이전이 OFF 였거나 같은 색 → fade-in 직행 */
                s_tx_phase = LED_TX_FADE_IN;
                s_tx_ms    = 0;
            }
        }
    }

    /* Phase A - 이전 색 fade-out (perceived 곡선 적용). timer_ms 진행 보류 */
    if (s_tx_phase == LED_TX_FADE_OUT)
    {
        LED_outputColor = s_tx_prev_color;

        /* perceived 255 → 0 으로 선형 감소, LUT 통해 PWM 변환 */
        uint8_t perceived = (uint8_t) (((LED_DIMMING_TX_FADE_MS - s_tx_ms) * 255UL)
                                       / LED_DIMMING_TX_FADE_MS);
        s_led_pwm_on_count = perceived_to_pwm(perceived);

        s_tx_ms++;
        if (s_tx_ms >= LED_DIMMING_TX_FADE_MS)
        {
            /* fade-out 완료 - POWER_ON / POWER_OFF 진입 시 leading OFF 끼워넣기.
             * 그 외 (지속 ON, 점멸 패턴 진입) 는 즉시 Phase B (새 패턴 FADE_IN). */
            if (st == TDC_LED_ST_POWER_ON || st == TDC_LED_ST_POWER_OFF)
            {
                s_tx_phase = LED_TX_LEAD_OFF;
            }
            else
            {
                s_tx_phase = LED_TX_FADE_IN;
            }
            s_tx_ms = 0;
        }
        return;
    }

    /* Phase 0 - POWER_ON / POWER_OFF 진입 시 leading OFF (분리감 보전, 음악적 쉼표).
     * timer_ms 진행 보류 - LED_POWER_LEAD_OFF_MS 경과 후 FADE_IN 으로 전이. */
    if (s_tx_phase == LED_TX_LEAD_OFF)
    {
        LED_outputColor    = TDC_LED_COLOR_BLACK;
        s_led_pwm_on_count = 0;

        s_tx_ms++;
        if (s_tx_ms >= LED_POWER_LEAD_OFF_MS)
        {
            s_tx_phase = LED_TX_FADE_IN;
            s_tx_ms    = 0;
        }
        return;
    }

    /* Phase B (또는 NONE) - 새 패턴 정상 진행 */

    /* 출력 색상 결정 */
    if (p->period_ms == 0)
    {
        LED_outputColor = p->color;  /* 지속 ON */
    }
    else
    {
        LED_outputColor = (timer_ms < p->on_ms) ? p->color : TDC_LED_COLOR_BLACK;
    }

    /* perceived brightness 산출 (점멸 fade-in/peak/fade-out, 자동 fade 시간) */
    uint8_t perceived = calc_perceived_pattern(timer_ms, p->on_ms, p->period_ms);

    /* Phase B - 새 색 fade-in: ratio 와 perceived 결합 (perceived 단위 곱셈) */
    if (s_tx_phase == LED_TX_FADE_IN)
    {
        uint8_t fade_in_perc = (uint8_t) ((s_tx_ms * 255UL) / LED_DIMMING_TX_FADE_MS);
        perceived = (uint8_t) (((uint32_t) perceived * fade_in_perc) / 255);

        s_tx_ms++;
        if (s_tx_ms >= LED_DIMMING_TX_FADE_MS)
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
                    /* (디버그) POWER_ON 패턴 종료 강조 - V10 SPI race 측정 트레이스
                     * st 가 TDC_LED_ST_POWER_ON 일 때만 출력. POWER_OFF 등은 영향 없음. */
                    if (st == TDC_LED_ST_POWER_ON)
                    {
                        TDC_PRINTF_I("\r\n");
                        TDC_PRINTF_I("################################################################\r\n");
                        TDC_PRINTF_I("###  [POWER-ON  END]     t3 = %d ms\r\n", tdc_hal_timer_get_t3_tick());
                        TDC_PRINTF_I("################################################################\r\n");
                        TDC_PRINTF_I("\r\n");
                    }

                    /* 게이트 자가 해제: 기존 관례 유지 */
                    s_req[TDC_LED_SRC_POWER]       = TDC_LED_ST_NONE;
#if TDC_LED_DBG_LONG_TOUCH_IGNORE
                    if (st == TDC_LED_ST_DBG_LONG_TOUCH_IGNORE)
                        s_req[TDC_LED_SRC_DBG] = TDC_LED_ST_NONE;  /* DBG burst 자가 해제 */
#endif
                    s_tdc_burst_pending        = false;
                    burst_done_cnt             = 0;
                }
            }
        }
        /* Phase B 진행 중 set 분기 제거 - `s_tdc_burst_pending` set 책임은
         * `tdc_led_request()` 가 단독 보유 (요청 시점 즉시 set, fade-out 무관). */
    }
}

/* ========================================================================
 *  Arbiter tick -- 매 iteration 호출 (Rev.3 SS3.5)
 * ======================================================================== */

/* Best state 산출 - fade-off 분기에서 재사용을 위해 추출.
 *
 * PAIR latch 갱신 부수효과가 있으나 idempotent (`!= TDC_LED_ST_PAIR` 가드).
 * 한 tick 내 두 번 호출되어도 동등 결과. */
static tdc_led_state_t compute_best_state(void)
{
    bool        user_off = (readLED_indicatorOnOff() == 2);
    tdc_led_state_t best     = TDC_LED_ST_IDLE;
    int         max_p    = -1;

    /* PAIR latch 처리: 해제 요청이 와도 latch 동안 유지 */
    if (s_req[TDC_LED_SRC_BLE_IND] != TDC_LED_ST_PAIR
        && tdc_hal_timer_get_tick() < s_pair_latch_until_tick)
    {
        s_req[TDC_LED_SRC_BLE_IND] = TDC_LED_ST_PAIR;
    }

    for (int src = 0; src < TDC_LED_SRC__MAX; ++src)
    {
        tdc_led_state_t st = s_req[src];
        int         p  = led_prio_of(st);

        /* 사용자 LED off: ERROR / POWER_ON / POWER_OFF 외 전부 억제 */
        if (user_off && !led_is_error(st)
            && st != TDC_LED_ST_POWER_ON && st != TDC_LED_ST_POWER_OFF)
        {
            if (s_isd_conn == 1)
            {
            continue;
            }
        }

        if (p > max_p)
        {
            max_p = p;
            best  = st;
        }
    }

    return best;
}

void tdc_led_arbiter_tick(void)
{
    /* (가드 1) Fade-off step - suspended 가드보다 먼저 처리.
     *
     * 이유: tdc_led_force_fade_off() 가 fade 시작과 동시에 s_led_isr_suspended = true
     *       로 sleep 진입 보호를 걸어둔다. 만약 suspended 가드가 먼저면 fade 가
     *       진행 안 됨. fade-off step 분기를 먼저 두어 fade 는 끝까지 진행되고,
     *       완료 후엔 본 분기를 빠져나가 (가드 2) 로 차단된다. */
    if (s_fade_off_state == LED_FADE_OFF_ACTIVE)
    {
        /* 새 high-priority 요청 검사 - best != IDLE 이면 fade-off 즉시 중단 후
         * 정상 arbiter path 진행.
         *
         * 출력 상태를 BLACK 으로 강제 리셋한 뒤 fall-through:
         *   리셋이 없으면 cross-fade Phase A (FADE_OUT) 가 잔존 LED_outputColor 를
         *   기준으로 가동되며, 새 패턴 첫 frame 출력이 cross-fade 길이만큼 지연된다.
         *   POWER_ON 직후 첫 펄스가 fade-in 으로 좁아진 뒤 ~48 ms 공백이 생기던
         *   현상이 이 경로에서 발생. BLACK 리셋으로 engine_run() 의 cross-fade 분기가
         *   FADE_IN 으로 직행하여 패턴이 의도된 형태로 시작된다.
         * (정책: ERROR 등 진입 시 기존 fade 중단 + 새 패턴 fade 적용.) */
        if (compute_best_state() != TDC_LED_ST_IDLE)
        {
            LED_outputColor    = TDC_LED_COLOR_BLACK;
            s_led_pwm_on_count = 0;
            tdc_led_out();
            s_fade_off_state = LED_FADE_OFF_IDLE;
            /* fall through to (가드 2) 검사 후 정상 arbiter */
        }
        else
        {
            /* fade-off step 진행 (perceived 선형 감소) */
            uint8_t perceived = (uint8_t) (((uint32_t) (s_fade_off_max - s_fade_off_t) * 255UL)
                                            / s_fade_off_max);
            s_led_pwm_on_count = perceived_to_pwm(perceived);
            tdc_led_out();

            if (++s_fade_off_t >= s_fade_off_max)
            {
                LED_outputColor    = TDC_LED_COLOR_BLACK;
                s_led_pwm_on_count = 0;
                tdc_led_out();
                s_fade_off_state = LED_FADE_OFF_IDLE;
            }
            return;
        }
    }

    /* (가드 2) Suspended - main loop 가 직접 LED state 를 조작하던 구간 보호.
     * 본 작업 후엔 tdc_led_force_fade_off() 의 sleep 진입 보호가 유일한 set 사이트. */
    if (s_led_isr_suspended)
    {
        return;
    }

    tdc_led_state_t best = compute_best_state();

    /* 패턴 엔진 구동 */
    static tdc_led_state_t prev_best = TDC_LED_ST_NONE;
    bool reset = (prev_best != best);
    prev_best  = best;

    led_engine_run(best, reset);

    /* GPIO 출력 */
    tdc_led_out();
}

/* ========================================================================
 *  tdc_led_pattern_out -- Legacy 래퍼 (직접 호출 시 하위 호환)
 * ======================================================================== */

void tdc_led_pattern_out(tdc_led_pattern_t ledOutputPattern)
{
    /* 새 아키텍처에서는 tdc_led_arbiter_tick()이 모든 처리를 담당.
     * 이 함수는 기존 호출 지점 호환을 위해 남겨둔 빈 래퍼.
     * LED 출력은 tdc_led_arbiter_tick() 내에서 이루어진다. */
    (void) ledOutputPattern;
}

/* ========================================================================
 *  Direct color / utility (기존 유지)
 * ======================================================================== */

void tdc_led_black(void)
{
    LED_outputColor = TDC_LED_COLOR_BLACK;
}

void tdc_led_white(void)
{
    LED_outputColor = TDC_LED_COLOR_WHITE;
}

void tdc_led_memory_error(void)
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

void tdc_led_clock_error(void)
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

void tdc_led_turn_off(void)
{
    /* 직전 LED 색의 perceived 곡선을 점진 감소시키며 BLACK 으로 안정시킨다.
     * 두 핀 이상 ON 상태 (ORANGE/SKYBLUE/PURPLE/WHITE) 에서 R→G→B 순차 LOW
     * 사이의 transient 로 의도치 않은 중간색이 보이는 현상 회피.
     *
     * ISR 비활성 (tdc_sys_init-time) 또는 영구 suspended (sleep 진입 후) 구간에서는
     * arbiter 가 fade-off step 을 진행할 수 없으므로 즉시 OFF 1 회로 마무리. */
    if (!led_arbiter_can_run())
    {
        LED_outputColor    = TDC_LED_COLOR_BLACK;
        s_led_pwm_on_count = 0;
        tdc_led_out();
        return;
    }

    /* 비차단 fade-off 시작 - 다음 ISR tick 부터 tdc_led_arbiter_tick() 의 가드 1
     * 분기가 매 1 ms perceived 감소 step 진행 (총 30 ms). */
    s_fade_off_state = LED_FADE_OFF_ACTIVE;
    s_fade_off_t     = 0;
    s_fade_off_max   = LED_DIMMING_FADE_MAX_MS;
}

void tdc_led_turn_on_red(void)
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

void tdc_led_turn_on_green(void)
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

void tdc_led_turn_on_blue(void)
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
 *  LED 색상별 R/G/B PWM duty cap (%) - 조합색 색감 교정
 * ========================================================================
 *  저항 R13=R14=R15=1.2kΩ + 3.3V_STBY 고정 회로에서 각 조합색의 체감
 *  색감을 의도와 일치시키기 위해 색상마다 R/G/B 채널별 cap 을 독립 설정.
 *
 *  단순 per-channel gain 은 단색 · 조합색 둘 다 동시 만족 불가 - 예를 들어
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
    [TDC_LED_COLOR_BLACK]   = {   0,   0,   0 },
    [TDC_LED_COLOR_RED]     = { 100,   0,   0 },  /* 1.0× 기준 */
    [TDC_LED_COLOR_GREEN]   = {   0,  50,   0 },  /* G 감쇠 - 체감 2× 보정 */
    [TDC_LED_COLOR_BLUE]    = {   0,   0, 100 },  /* 1.0× 기준 */
    [TDC_LED_COLOR_ORANGE]  = {  80,  10,   0 },  /* R 우세 + G 최소 → 주황 */
    [TDC_LED_COLOR_SKYBLUE] = {   0,  30,  40 },  /* 총 광량 감쇠 (원 SKYBLUE 가 최고 밝음) */
    [TDC_LED_COLOR_PURPLE]  = {  50,   0,  50 },  /* 총 광량 감쇠 */
    [TDC_LED_COLOR_WHITE]   = {  30,  30,  30 },  /* G 비중 ↑ → 연보라 제거 */
};

/* ========================================================================
 *  GPIO 극성 helper - Active HIGH / Active LOW / B pin 유무 컴파일 타임 분기
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
 *  tdc_led_out - 색상 × Dimming × PWM → GPIO 출력
 * ======================================================================== */

void tdc_led_out(void)
{
    static int timerCounter = 0;

    /* Test trigger: 강제 BLUE. 테이블 lookup 으로 일관 처리. */
    tdc_led_color_t color = tdc_led_is_test_trigger_enabled() ? TDC_LED_COLOR_BLUE : LED_outputColor;

    /* 배열 bound 가드 - 미등록 색상은 OFF 로 처리 */
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

    /* 채널별 감쇠 duty - base × cap / 100 */
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
