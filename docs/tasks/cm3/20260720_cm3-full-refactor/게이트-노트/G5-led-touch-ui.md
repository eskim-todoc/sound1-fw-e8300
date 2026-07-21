---
name: G5 led/touch/ui 게이트 노트
purpose: G5(LedOutput·tdc_touch*·tdc_ui·earpiece) 이동·rename + LED/earpiece 정합 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g5, led, touch, ui, earpiece]
---

# G5 — led/touch/ui

**TL;DR**: 소폭 게이트. touch·ui 는 **이미 `tdc_touch_*`/`tdc_ui_*` 규약 준수**(최근 정비) → 폴더 이동 위주. LED 는 `led_*`/`LED_*` 관례를 `tdc_led_*`/`TDC_LED_*` 로 정합. `earpieceUpate.c`(파일명 오타) 정정. IQS323 은 드라이버 성격이나 touch 계층 결합이 강해 **touch 도메인 내 유지**.

## 1. 파일 매핑

| 현행 | 신규 | 도메인 |
|---|---|---|
| `systemControl/LedOutput.c/.h` | `led/tdc_led_output.c/.h` | led |
| `systemControl/tdc_touch.c/.h` | `touch/tdc_touch.c/.h` | touch (이동만) |
| `systemControl/tdc_touch_logic.c/.h` | `touch/tdc_touch_logic.c/.h` | touch (이동만) |
| `systemControl/tdc_touch_iqs323.c/.h` | `touch/tdc_touch_iqs323.c/.h` | touch (이동만 — §3) |
| `systemControl/tdc_touch_config.h` | `touch/tdc_touch_config.h` | touch (이동만) |
| `systemControl/tdc_touch_time.h` | `touch/tdc_touch_time.h` | touch (이동만) |
| `systemControl/earpieceUpate.c` | `sys/tdc_sys_earpiece.c` | **파일명 오타 정정** + sys 도메인 |
| `systemControl/earpieceUpdate.h` | `sys/tdc_sys_earpiece.h` | sys |
| `Gen1_5/ui/tdc_ui_command.c/.h` | `ui/tdc_ui_command.c/.h` | ui (이동만) |

`Gen1_5/ui/` 소멸. `Cortex-M3-src/systemControl/` 잔여는 `cfx_cm3_sharedMemory` 만 남음(G8 소관).

## 2. LED rename

LED 는 `led_*` 함수 관례가 이미 있으나 `tdc_` 미적용. 정합:
- 함수: `led_request`/`led_arbiter_tick`/`led_get_request` 등 → `tdc_led_*`. 이미 `tdc_led_*` 인 것(`tdc_led_is_burst_pending` 등)은 유지
- 레거시 API: `turnOffLED`/`LED_black`/`LED_White`/`LED_clock_error`/`LED_Memory_error`/`LED_OUT`/`LedPatternOut` → `tdc_led_*`
- 상수: `LED_ST_*`/`LED_SRC_*` (이미 도메인 명확) → `TDC_LED_ST_*`/`TDC_LED_SRC_*`. `EN__LED_PATTERN`/`EN__LED_COLOR` → `tdc_led_pattern_t`/`tdc_led_color_t`
- `en__LED_*` enum 멤버 → `TDC_LED_*`
- IRQ: 해당 없음 (LED arbiter 는 `TIMER_3_IRQHandler`(hal) 가 구동, LedOutput 자체는 IRQ 없음)

## 3. IQS323 판단 — touch 유지

`tdc_touch_iqs323` 은 IQS323 I2C 직접 접근(드라이버 성격, 레지스터 76회 참조)이나:
- **touch 3계층**(`tdc_touch` 얇은 연결 / `tdc_touch_logic` 순수 FSM / `tdc_touch_iqs323` I2C 접근)이 강결합된 하나의 단위
- 이미 `tdc_touch_iqs323_*` 규약 준수 (2026-06-20 정비)
- `tdc_drv_iqs323` 로 분리하면 touch 계층 응집을 깨고 이득 없음

→ **touch 도메인 유지**. 단 `public_touch_settings`(규약 비준수 1건)는 `tdc_touch_iqs323_public_settings` 로 정합.

## 4. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| led | `tdc_led_output`(arbiter FSM + engine + 레거시 API) | **[로직설명]** — rename 뿐 |
| touch | `tdc_touch`(연결) · `tdc_touch_logic`(FSM) · `tdc_touch_iqs323`(I2C) | **[로직설명]** — 이동 뿐 |
| ui | `tdc_ui_command` | **[로직설명]** — 이동 뿐 |
| sys(확장) | `tdc_sys_earpiece` | **[로직설명]** |

의존: `touch` → `hal/i2c`(G3) 통과 전제. `led` → `hal/timer`(G1, arbiter tick) 통과 전제. `ui` → `led`·`touch`·`pwr` 통과 전제.

## 5. 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 10패턴 0 (`en__LED_`·`LED_ST_`·`LED_SRC_`·`led_*`·`LedOutput`·`earpieceUp`·`public_touch_settings` 등). 주석 2건도 신규 명으로 갱신 |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` 무변경 |
| 검증_공통_3 (균형·훅) | ✅ led 2 + touch 8 + ui 2 + earpiece 2 파일 균형 OK · 깨진 include 0 · 훅 통과 |
| 검증_공통_4 (사장 실증) | ⚠️ **제거 보류** — §6 |
| **검증_G5_특화 (LED 상수 정합)** | ✅ `TDC_LED_ST_*`/`TDC_LED_SRC_*`/`TDC_LED_COLOR_*`/`TDC_LED_PATTERN_*` 소비처(main.c·sys 포함) 전수 갱신. enum 명시 매핑(MCU/FPGA/PMIC 대문자 연속 보존) |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-22): "빌드 후 동작 확인했어" → **G5 폐쇄** |

## 6. 사장 판정 — 제거 보류 (11건)

실호출 0 확인: LED 레거시 7(`tdc_led_black`/`white`/`clock_error`/`memory_error`/`pattern_out`/`disable_test_trigger`/`is_test_trigger_enabled`) + touch 4(`tdc_touch_get_state`/`iqs323_set_ulp`/`is_ulp`/`clear_ulp`/`public_settings`).

**이번 게이트에서 제거하지 않는다**:
- LED·touch 는 실기 동작 검증이 특히 중요한 계층 (LED 패턴·ULP 절전)
- ③ `04_사장코드-후보.md` 에 이미 후보로 등재됨 — 도메인 정리와 별개로 일괄 판단이 안전
- touch ULP API(`set_ulp`/`is_ulp`/`clear_ulp`)는 절전 재설계(`touch/20260706`) 미결과 연관 가능

→ G8 이후 또는 별도 사장 정리 작업에서 일괄 실증 후 제거.

> [!NOTE]
> 도구 결함(재발): 사장 검출이 "호출 파일 수 ≤ 1"만 봐서 `tdc_led_out`(자기 파일 6회 호출)을 오탐. 직접 grep 으로 걸러냄 — G1~G3 와 같은 계열.
