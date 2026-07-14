---
name: recapsulation-design-analyst
purpose: func_normal() 배터리→LED 로직의 margin-free 재캡슐화 설계 옵션(A/B/C) 제시 + LED arbiter·override·순서 의존 보존 방안 설계 (구현 아님, 읽기 전용 분석)
type: agent-log
maturity: experimental
tags: [main.c, led, battery, refactor, hysteresis, margin-free, tdc_, design-options]
---

# 재캡슐화 설계 옵션 (margin-free) + LED arbiter 결합 진단

> [!NOTE]
> **TL;DR**: 배터리 관련 `LED_ST_*`는 4종(IDLE 포함 시)·매핑 관련은 4종, 총 8종이 배터리 percent에 의존한다. margin-free 재캡슐화로 3가지 옵션을 제시한다 — **옵션 A(단일 임계 상수)**는 현 코드에서 sticky 분기만 들어내는 최소변경안, **옵션 B(테이블 구동)**는 레거시 `updateBatteryLevel()`의 "경계값 테이블 → 등급" 골격을 노이즈 가드 없이 재현한 구조로 하드코딩 제거 효과가 가장 크다, **옵션 C(레벨 기반)**는 이미 존재하고 이미 margin-free인 `snd_batt_get_level()`(7단계, `batteryNPowerControl.c:47~88`)을 재사용하지만 20%p 그리드에 값이 고정되어 있어 배터리 블록의 CRITICAL 컷(10%)을 그대로 재현할 수 없다는 치명적 제약이 있다(반면 매핑 블록의 LOW 컷 20%는 이 그리드와 정확히 일치 — 부분 채택 가치 있음). **종합 추천은 배터리 블록에 옵션 B**(옵션 A는 저위험 대안), **매핑 블록에는 옵션 C의 부분 적용도 고려 가능**. 보존 필수 항목(override 2단·배터리→ISD→매핑 순서·`led_set_isd_conn_state()` 봉인·pct/level 공유·RESET→IDLE)은 세 옵션 모두에서 동일하게 지켜지도록 설계했다.

## 0. 기준선(baseline) 확인

작업트리 미커밋 변경(`git diff -- src/2__cm3/Cortex-M3-src/main.c`) 확인 결과, 배터리 임계값이 `40/41/65/64` → `10/12/80/78`로 되돌아가 있다(줄 수 변화 없음, 값만 4곳 치환). 이는 요구사항 §6 이슈_1의 "은수님 WIP"에 해당하며, 본 문서는 **이 실측값(10/12/80/78)을 baseline으로** 설계한다.

```diff
- else if (pct < 40 /*10*/)                                          → else if (pct < 10)
- else if (pct < 41 /*12*/ && prev_batt_st == LED_ST_BATT_CRITICAL)   → else if (pct < 12 && prev_batt_st == LED_ST_BATT_CRITICAL)
- else if (pct >= 65 /*80*/)                                         → else if (pct >= 80)
- else if (pct >= 64 /*78*/ && prev_batt_st == LED_ST_BATT_READY)     → else if (pct >= 78 && prev_batt_st == LED_ST_BATT_READY)
```
(`main.c:563~569`, 실측)

블록 라인 구간(선행 `20260709_func-normal-refactor/에이전트-로그/04_led-request-logic-diagnostician.md`와 동일, 값만 변경돼 라인 수는 불변):
- 전체: `main.c:538~655`
- 배터리: `main.c:542~578`
- ISD: `main.c:580~618`
- 매핑: `main.c:620~654`

## 1. `LED_ST_` 상태 목록 — 배터리·매핑 관련

`LedOutput.h:61~98`의 `led_state_t` 중 배터리 percent에 직접 의존하는 항목:

| 그룹 | 값 | 의미 | 근거 |
|---|---|---|---|
| 배터리 (3+1) | `LED_ST_IDLE` | 배터리 정보 미수신(RESET) 시 판정 보류 placeholder | `LedOutput.h:64`, `main.c:558~562` |
| | `LED_ST_BATT_CRITICAL` | 노랑 점멸(1100/1100ms) | `LedOutput.h:70` |
| | `LED_ST_BATT_MID` | 노랑 지속 ON | `LedOutput.h:69` |
| | `LED_ST_BATT_READY` | 녹색 지속 ON | `LedOutput.h:67` |
| 매핑 (4) | `LED_ST_MAPPING_ISD_BATT_READY` | 파랑 점멸(200/800ms) — 배터리>20% & ISD 연결 | `LedOutput.h:73` |
| | `LED_ST_MAPPING_NO_ISD_BATT_READY` | 파랑 지속 ON — 배터리>20% & ISD 미연결 | `LedOutput.h:74` |
| | `LED_ST_MAPPING_ISD_BATT_LOW` | 보라 점멸(100/900ms) — 배터리≤20% & ISD 연결 | `LedOutput.h:75` |
| | `LED_ST_MAPPING_NO_ISD_BATT_LOW` | 보라 지속 ON — 배터리≤20% & ISD 미연결 | `LedOutput.h:76` |

**몇 단계인가**: 배터리 블록은 실질 3단계 분류(CRITICAL/MID/READY, percent 2개 컷) + RESET 전용 IDLE 1종(총 4 enum 값). 매핑 블록은 배터리 2단계(LOW/READY, percent 1개 컷) × ISD 연결여부 2분기 = 4 enum 값(+연결 안 됐을 때 `LED_ST_NONE`). 즉 배터리 percent가 실제로 나누는 등급 수는 **배터리 블록 3등급, 매핑 블록 2등급**이며 서로 다른 컷을 쓴다(배터리: 10/80, 매핑: 20).

참고로 레거시 `EN__BATTERY_LEVEL`(`batteryNPowerControl.h:17~26`)은 20%p 간격의 **7단계**(`0per/0~20/20~40/40~60/60~80/80~100/100per`)이며, 현 `snd_batt_get_level()`(`batteryNPowerControl.c:47~88`)이 이 enum을 percent로부터 이미 **margin-free**(경계 비교만, static/이력 없음)로 산출하고 있다 — 옵션 C의 근거.

## 2. 설계 옵션 (margin-free)

세 옵션 모두 공통으로:
- `prev_batt_st`(`main.c:555`)를 **완전히 제거**한다(margin 자체가 이 변수 하나에 의존하므로, 제거가 곧 margin 제거).
- RESET→IDLE 분기(`main.c:558~562`)는 그대로 최우선 분기로 유지한다.
- override 2단 패턴(§3)과 배터리→ISD→매핑 순서(§3)는 옵션 선택과 무관하게 호출부(`func_normal()`)에서 동일하게 보존한다.

### 옵션 A — 단일 임계 상수 (if-else, sticky 분기만 제거)

```c
/* main.c 파일 스코프, 배터리 블록 함수 정의 위 */
#define TDC_BATT_CRITICAL_PCT   10   /* pct < 10  -> CRITICAL (구주석 옛값과 동일) */
#define TDC_BATT_READY_PCT      80   /* pct >= 80 -> READY */

static led_state_t tdc_led_request_battery(int pct, bool ovr_batt_active, bool batt_is_reset_state)
{
    led_state_t batt_st;

    if (!ovr_batt_active && batt_is_reset_state)
    {
        batt_st = LED_ST_IDLE;
    }
    else if (pct < TDC_BATT_CRITICAL_PCT)
    {
        batt_st = LED_ST_BATT_CRITICAL;
    }
    else if (pct >= TDC_BATT_READY_PCT)
    {
        batt_st = LED_ST_BATT_READY;
    }
    else
    {
        batt_st = LED_ST_BATT_MID;
    }

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
        led_request(LED_SRC_BATTERY, batt_st);

    return batt_st;   /* ISD 블록 재사용 */
}
```

**트레이드오프**
- (+) 원본 대비 diff가 가장 작다(sticky 2개 분기 + `prev_batt_st` static 삭제뿐) — 리뷰·회귀 확인이 쉽다.
- (+) `#define` 2개로 매직넘버 명명은 되지만, "테이블/함수로 걷어냈다"는 인상은 약하다 — `if-else` 사슬 자체는 그대로 남는다.
- (-) 등급을 늘리거나(예: LOW 경고 1단계 추가) 컷을 조정하려면 여전히 분기 코드를 손봐야 한다(데이터 편집이 아니라 코드 편집).
- 레거시 `updateBatteryLevel()`과의 구조적 유사성: **낮음** (레거시는 경계값을 구조체/테이블로 나열했지 if-else 사슬이 아니었다 — `batteryNPowerControl.c:353~408`의 `batteryBoundary.dischargingBatterBoundary.battery_boundary_*` 참조).

### 옵션 B — 테이블 구동 (경계값 배열 순회)

```c
typedef struct
{
    int         min_pct;   /* 이 값 이상이면 해당 상태로 분류 (배열은 내림차순, 첫 매치 채택) */
    led_state_t state;
} tdc_batt_led_bin_t;

/* 내림차순 필수 — 값 변경/추가는 이 표만 편집하면 됨. 매직넘버가 코드가 아니라 데이터가 된다. */
static const tdc_batt_led_bin_t s_tdc_batt_led_table[] =
{
    { 80, LED_ST_BATT_READY    },   /* pct >= 80        (구주석 옛값과 동일) */
    { 10, LED_ST_BATT_MID      },   /* 10 <= pct < 80    */
    {  0, LED_ST_BATT_CRITICAL },   /* pct < 10 (fallback, 항상 마지막에 매치) */
};
#define TDC_BATT_LED_TABLE_LEN  (sizeof(s_tdc_batt_led_table) / sizeof(s_tdc_batt_led_table[0]))

static led_state_t tdc_led_request_battery(int pct, bool ovr_batt_active, bool batt_is_reset_state)
{
    led_state_t batt_st = LED_ST_BATT_CRITICAL;  /* 테이블 매치 실패 시 안전측 기본값 */

    if (!ovr_batt_active && batt_is_reset_state)
    {
        batt_st = LED_ST_IDLE;
    }
    else
    {
        for (size_t i = 0; i < TDC_BATT_LED_TABLE_LEN; ++i)
        {
            if (pct >= s_tdc_batt_led_table[i].min_pct)
            {
                batt_st = s_tdc_batt_led_table[i].state;
                break;
            }
        }
    }

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
        led_request(LED_SRC_BATTERY, batt_st);

    return batt_st;
}
```

**트레이드오프**
- (+) 레거시 `updateBatteryLevel()`의 핵심 골격("경계값 나열 → 등급 결정")을 **노이즈 가드(추세 반전 방지 margin) 없이** 재현 — "updateBatteryLevel() 정신을 지금 구조에 맞게 되살린다"(요구사항 목표_1)에 구조적으로 가장 부합.
- (+) 등급 추가/컷 변경이 배열 한 줄 편집으로 끝난다 — 하드코딩 제거 효과가 세 옵션 중 가장 크다.
- (-) "내림차순 정렬·fallback 행 필수" 같은 불변식을 주석/assert로 지켜야 한다(정렬이 깨지면 조용히 오분류). 정적 분석/단위테스트가 없는 임베디드 환경에서는 이 불변식이 리뷰에 의존한다.
- (-) 3단계뿐인 현 규모에는 배열+루프가 약간 과설계로 보일 수 있으나, 매핑 블록과 **동일 헬퍼 패턴을 공유**할 수 있다는 이점이 있다(§4에서 재사용).

### 옵션 C — 레벨 기반 (`snd_batt_get_level()` 재사용)

```c
/* EN__BATTERY_LEVEL(7단계, batteryNPowerControl.h:17~26) -> led_state_t 매핑.
 * snd_batt_get_level()(batteryNPowerControl.c:47~88)은 이미 margin-free(정적 상태 없음). */
static const led_state_t s_tdc_batt_level_to_led[] =
{
    [en__batteryPower_0per]     = LED_ST_BATT_CRITICAL,
    [en__batteryPower_0btw20]   = LED_ST_BATT_CRITICAL,   /* 0 < pct < 20 */
    [en__batteryPower_20btw40]  = LED_ST_BATT_MID,        /* 20 <= pct < 40 */
    [en__batteryPower_40btw60]  = LED_ST_BATT_MID,
    [en__batteryPower_60btw80]  = LED_ST_BATT_MID,
    [en__batteryPower_80btw100] = LED_ST_BATT_READY,      /* 80 <= pct < 100 */
    [en__batteryPower_100per]   = LED_ST_BATT_READY,
};

static led_state_t tdc_led_request_battery(bool ovr_batt_active, int ovr_pct, bool batt_is_reset_state)
{
    led_state_t batt_st;

    if (!ovr_batt_active && batt_is_reset_state)
    {
        batt_st = LED_ST_IDLE;
    }
    else if (ovr_batt_active)
    {
        /* UI 오버라이드는 percent 단위로 주입되므로(tdc_ui_command_override_battery_percent()),
         * 레벨 재분류용 헬퍼가 별도로 필요 — 아래 "치명적 제약" 참고 */
        batt_st = tdc_led_pct_to_battery_state_fallback(ovr_pct);
    }
    else
    {
        batt_st = s_tdc_batt_level_to_led[snd_batt_get_level()];
    }

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
        led_request(LED_SRC_BATTERY, batt_st);

    return batt_st;
}
```

**트레이드오프 — 치명적 제약**
- `snd_batt_get_level()`의 등급 경계는 **0/20/40/60/80/100의 20%p 고정 그리드**다(`batteryNPowerControl.c:54~87`). 매핑 블록의 LOW 컷(20%)은 이 그리드와 **정확히 일치**하지만, 배터리 블록의 CRITICAL 컷(10%, baseline)은 그리드 상 어떤 경계에도 없다 — 가장 가까운 그리드 선은 0 또는 20뿐이다.
- 즉 옵션 C를 배터리 블록에 그대로 쓰면 **CRITICAL 컷이 10%에서 20%(또는 0%)로 이동**하는 "마진 제거를 넘어선 추가 사양 변경"이 강제된다. 이는 요구사항이 요청한 범위(마진 제거)를 벗어나므로, 배터리 블록에는 옵션 C를 **비추천**한다.
- 오버라이드(`tdc_ui_command_override_battery_percent()`)가 percent 단위라서, 레벨 기반으로 가면 오버라이드 경로에 percent→상태 변환을 별도로 둬야 하는 이중 로직 문제도 생긴다(위 스켈레톤의 `tdc_led_pct_to_battery_state_fallback()`).
- (+) 반면 **매핑 블록**은 LOW 컷이 20%로 그리드와 일치하므로, `snd_batt_get_level() <= en__batteryPower_0btw20` 형태로 옵션 C를 부분 채택할 가치가 있다(§4에서 다룸).
- 레거시와의 유사성: 출력 타입(`EN__BATTERY_LEVEL`)은 레거시와 동일하지만, 레거시가 자체 CFX 측정(`readBatteryLevel_FromCFX()`)이었던 것과 달리 지금은 QCC 제공 percent를 그대로 재분류하는 것이라 "재사용"이라기보다 "동일 enum을 신규 함수가 새로 소비"하는 관계에 가깝다.

### 옵션 비교 종합 및 추천

| 기준 | A (단일 임계) | B (테이블) | C (레벨) |
|---|---|---|---|
| diff 크기 | 최소 | 중간 | 중간~큼(오버라이드 이중화) |
| 하드코딩 제거 효과 | 낮음(if-else 유지) | **높음**(데이터화) | 중간(그리드 제약) |
| updateBatteryLevel 정신 부합 | 낮음 | **높음**(경계 테이블 구조 재현) | 중간(enum 재사용, 구조는 다름) |
| 배터리 10%/80% 컷 재현 | 정확 | 정확 | **불가**(그리드 스냅 필요) |
| 매핑 20% 컷 재현 | 정확 | 정확 | **정확**(그리드 일치) |
| 오버라이드 경로 단순성 | 단순 | 단순 | 복잡(percent→level 변환 필요) |

**추천**: 배터리 블록은 **옵션 B**를 1순위로 제안한다 — "updateBatteryLevel() 정신을 되살리되 마진(추세 반전 방지 가드)은 걷어낸다"는 요구사항 문구에 구조적으로 가장 근접하고, 매직넘버를 실질적으로 데이터로 옮긴다. 최소 변경·최소 리스크를 우선한다면 **옵션 A**도 합리적 대안(diff가 작아 리뷰 부담이 적음). **옵션 C는 배터리 블록에는 비추천**(10% 컷 재현 불가)하되, 매핑 블록에는 20% 컷이 그리드와 일치하므로 부분 채택을 §4에서 별도 제안한다.

## 3. 보존 필수 항목 (설계 불변식)

| # | 항목 | 근거 | 세 옵션 공통 보존 방법 |
|---|---|---|---|
| 1 | `ENABLE_UI_CMD` override 2단(입력 override + 출력 게이트) | `main.c:548~554`(입력), `575~577`(출력); ISD `581~588`; 매핑 `622~634` | 입력 override(`ovr_batt_active`/`pct` 대체)는 `func_normal()` 호출부에서 계산해 파라미터로 전달, 출력 게이트(`tdc_ui_command_is_led_override(LED_SRC_*)`)는 각 신규 함수 내부 `led_request()` 직전에 유지 |
| 2 | ISD 미연결 시 배터리 LED 재송출 + 배터리→ISD→매핑 순서 | `main.c:604~610`; 상호 참조는 선행 04 진단서 §4 | `tdc_led_request_battery()` 반환값 `batt_st`를 `tdc_led_request_isd(isd_conn, batt_st)`에 전달, `tdc_led_request_isd()` 반환값 `isd_conn`을 `tdc_led_request_mapping()`에 전달 — 호출 순서를 이 데이터 의존이 강제 |
| 3 | `led_set_isd_conn_state()`를 ISD 함수 마지막에 봉인 | `main.c:613~616` 주석(1-tick IN_USE 잔상 race 방지) | `led_set_isd_conn_state()` 호출을 `tdc_led_request_isd()` 함수의 **마지막 statement**로 캡슐 내부에 봉인 — 호출자에 노출하지 않아 순서 실수 원천 차단 (선행 04 진단서 §2.2와 동일 결론, 옵션 A/B/C 선택과 무관) |
| 4 | 매핑 블록이 `pct`(또는 level)를 공유 | `main.c:550`(계산) → `628/630`(재사용) | `func_normal()`이 `pct`(옵션 A/B) 또는 `pct`+`snd_batt_get_level()`(옵션 C 부분채택, §4) 를 **1회만 계산**해 배터리 함수와 매핑 함수 양쪽에 동일 스냅샷을 인자로 전달 — 오버라이드 활성 시에도 동일 값 보장 |
| 5 | `EN__SND_BATT_STATE_RESET` → IDLE | `main.c:558~562` | 세 옵션 모두 `batt_is_reset_state` 판정을 percent/level 분류보다 **최우선 분기**로 유지 (옵션 B/C에서도 테이블·룩업 이전에 조기 처리) |

## 4. 매핑 블록 마진 제거 설계

원본(`main.c:627~631`):
```c
static bool s_map_low_active = false;
if (pct <= 20)
    s_map_low_active = true;
else if (pct >= 22)
    s_map_low_active = false;
    /* pct == 21 구간은 직전 상태 유지 */
```

margin-free 단일 임계(권장 — 옵션 A/B 계열과 스타일 통일):
```c
#define TDC_MAP_LOW_BATT_PCT   20   /* pct <= 20 -> LOW (구 진입 컷과 동일 값 채택) */

static void tdc_led_request_mapping(int pct, bool map_conn, bool isd_conn)
{
    bool map_low_active = (pct <= TDC_MAP_LOW_BATT_PCT);   /* static 제거 — 매 iteration 순수 계산 */

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_MAPPING))
#endif
    {
        if (map_conn)
        {
            led_state_t map_st = map_low_active
                ? (isd_conn ? LED_ST_MAPPING_ISD_BATT_LOW  : LED_ST_MAPPING_NO_ISD_BATT_LOW)
                : (isd_conn ? LED_ST_MAPPING_ISD_BATT_READY : LED_ST_MAPPING_NO_ISD_BATT_READY);
            led_request(LED_SRC_MAPPING, map_st);
        }
        else
        {
            led_request(LED_SRC_MAPPING, LED_ST_NONE);
        }
    }
}
```

`s_map_low_active`가 `static bool`에서 지역 `bool`로 바뀌는 점에 주의 — margin이 있던 원본은 "직전 상태 유지"가 필요해 static이 필수였지만, margin-free 단일 임계에서는 매 iteration `pct`만으로 완전히 결정되므로 **static 자체가 불필요**해진다(순수 함수화). 이는 마진 제거의 직접적 부산물이며 별도 side effect가 없다.

**컷값 선택 이슈**(요구사항 이슈_2와 동형): 원본 sticky band는 `pct==21` 1개 구간이었다. 단일 임계로 합칠 때 `pct<=20`(LOW 쪽 편입, 위 예시) 또는 `pct<21`/`pct<22`(READY 쪽 편입) 중 선택이 필요 — 값 자체의 우열은 없고 은수님 승인 필요(계획 단계에서 확정).

**옵션 C 부분 채택안**(참고, §2 옵션 C 논의와 연결):
```c
bool map_low_active = (snd_batt_get_level() <= en__batteryPower_0btw20);  /* pct < 20 */
```
이 표현은 `en__batteryPower_0btw20`이 `0 < percent < 20`이므로 `pct < 20`(위 예시의 `pct <= 20`과 경계 1이 다름 — `pct==20`을 LOW/READY 어느 쪽으로 볼지 재확인 필요)과 사실상 같다. 배터리 블록과 달리 매핑 블록은 그리드(20%p)와 컷(20%)이 우연히 일치하므로, "레벨 재사용" 이점(percent 직접 판정 회피, 이미 존재하는 `snd_batt_get_level()` 재사용)을 매핑 블록에서만 취하는 것도 유효한 절충안이다. 다만 배터리 블록과 매핑 블록의 판정 방식이 서로 달라지는(하나는 percent, 하나는 level) 비대칭이 생기므로, 일관성을 우선한다면 두 블록 모두 percent 기반(옵션 A/B 계열)으로 통일하는 편이 무난하다 — 최종 선택은 계획 승인 단계로 위임.

## 5. 명명 제안 (`tdc_` 접두어)

| 범주 | 이름 | 비고 |
|---|---|---|
| 함수 | `tdc_led_request_battery()` | 배터리 블록 캡슐화 (옵션 A/B/C 공통 시그니처 형태) |
| | `tdc_led_request_isd()` | ISD 블록 캡슐화 (마진 제거와 무관, 순서 보존용 — 선행 04 진단서 제안 재확인) |
| | `tdc_led_request_mapping()` | 매핑 블록 캡슐화 |
| 상수(옵션 A) | `TDC_BATT_CRITICAL_PCT`, `TDC_BATT_READY_PCT`, `TDC_MAP_LOW_BATT_PCT` | 단일 임계 — ENTER/EXIT 접미사 불필요(margin-free이므로) |
| 타입(옵션 B) | `tdc_batt_led_bin_t` | `{ min_pct, state }` 구조체 |
| 테이블(옵션 B) | `s_tdc_batt_led_table[]` | file-static, 내림차순 불변식 주석 필수 |
| 룩업(옵션 C) | `s_tdc_batt_level_to_led[]` | `EN__BATTERY_LEVEL` 인덱스 배열 |

기존 프로젝트 관례상 `tdc_` 접두 함수(`tdc_ui_command_*`, `tdc_led_set_ind_state`, `tdc_charger_set_cradle_cover_state` 등)와 일관되며, ENTER/EXIT 접미사가 사라지는 것 자체가 "margin 제거"를 이름에서도 드러내는 효과가 있다(선행 04 진단서의 `BATT_CRITICAL_ENTER_PCT`/`_EXIT_PCT` 쌍은 이번 margin-free 개조 대상에서 폐기).

## 자체 검토

### 지침 준수 여부
| 지침 | 준수 |
|---|---|
| 읽기 전용(코드 미수정) | 준수 — `src/` 파일은 `Read`/`Grep`만 수행, `Edit`/`Write` 미사용 |
| 근거 우선(`파일:라인`) | 준수 — 모든 인용에 실측 라인 표기 |
| 로그 직접 Write, frontmatter 포함 | 준수 |
| 선행 04 진단서(margin-preserving) 참조·개조 | 준수 — §0에서 라인 대응 확인, §2에서 선행 시그니처 대비 옵션 A/B/C 명시 |

### 논리적 정합성
| 점검 | 결과 |
|---|---|
| LED_ST 단계 수 근거 제시 | ✅ (§1, `LedOutput.h:61~98` 직접 인용) |
| 옵션 A/B/C 모두 구체 시그니처+스켈레톤+트레이드오프 제공 | ✅ (§2) |
| 보존 필수 5항목이 옵션 선택과 독립적으로 성립함을 확인 | ✅ (§3 — 특히 `led_set_isd_conn_state()` 봉인은 배터리 옵션과 무관) |
| 옵션 C의 20%p 그리드 제약을 수치로 검증 | ✅ (`batteryNPowerControl.c:54~87` 경계값 0/20/40/60/80/100과 baseline 컷 10/80 대조) |
| 매핑 컷(20%)이 옵션 C 그리드와 우연히 일치함을 발견·활용 | ✅ (§4 부분 채택안) |

### 미해결 이슈 (승인 게이트로 위임)
| 이슈 | 처리 |
|---|---|
| 배터리 블록 옵션 A vs B 최종 선택 | 계획 승인 단계 — 본 문서는 B를 1순위 추천, A를 저위험 대안으로 병기 |
| 매핑 블록 단일 컷값(20 vs 21 vs 22) 및 percent-based vs level-based 방식 통일 여부 | 계획 승인 단계 — §4에서 절충안 제시, 최종은 은수님 결정 |
| 옵션 B 테이블의 "내림차순 불변식"을 코드로 강제할지(assert/정적 검사) 여부 | 계획 단계에서 구현 세부로 결정 — 본 문서는 설계 옵션 제시까지가 범위 |
