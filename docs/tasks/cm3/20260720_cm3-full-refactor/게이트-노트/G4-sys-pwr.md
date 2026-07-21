---
name: G4 sys/pwr 게이트 노트
purpose: G4(systemControl·initialize·error·battery/power) 이관 + systemControl FSM 분해(흡수분 이행) 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g4, systemcontrol, fsm, power, battery]
---

# G4 — sys/pwr (시스템 제어 · 전원)

**TL;DR**: 이번 리팩토링의 **최대 난이도 게이트**. 파일 이관·rename 에 더해 **흡수된 `main/20260715_systemcontrol-fsm-decompose` 설계를 이행**한다(281줄 · `static` 7개 → ctx 구조체 + 핸들러 분해). **회귀 1순위 = `isd_disconnection_counter` 초기값 2000 승계** — zero-init 하면 부팅 직후 전원버튼이 먹통이 된다.

## 1. 파일 매핑

| 현행 | 신규 | 도메인 |
|---|---|---|
| `Cortex-M3-src/systemControl/systemControl.c/.h` | `sys/tdc_sys_control.c/.h` | sys |
| `Cortex-M3-src/systemControl/initialize.c/.h` | `sys/tdc_sys_init.c/.h` | sys |
| `Cortex-M3-src/error.c/.h` | `sys/tdc_sys_error.c/.h` | sys |
| `Cortex-M3-src/systemControl/batteryNPowerControl.c/.h` | `pwr/tdc_pwr_battery.c/.h` | pwr |
| `Gen1_5/common/ci_battery.c/.h` | `pwr/tdc_pwr_lsad.c/.h` | pwr (LSAD 배터리 측정) |
| `Gen1_5/common/ci_power.c/.h` | `pwr/tdc_pwr_clock.c/.h` | pwr (클럭·절전 전환) |

`Gen1_5/common/` 은 이 게이트로 **완전 소멸**한다(G1 에서 나머지 이관 완료).

## 2. 흡수분 이행 — systemControl FSM 분해

`main/20260715_systemcontrol-fsm-decompose` 의 ⑤ 계획을 이 게이트에서 이행한다. 해당 작업은 승인 대기 상태에서 본 리팩토링에 흡수됐다(2026-07-20).

### 2.1 dead static 3건 제거 (실증 완료)

| 심볼 | 근거 |
|---|---|
| `PowerOn_StartCounter` | 쓰기 2 · **읽기 0** |
| `normalModeCounter` | 쓰기 2(`++` 후 `& 0xFFFF` 마스킹까지) · **읽기 0** |
| `CounterAfterCoverClosed` | 쓰기 3 · **읽기 0** |

원본 주석(`systemControl.c:220`)이 이미 "별도 cleanup 으로 위임 (사용처 dead 확인됨)"이라 자백하고 있었다. 본 게이트가 그 이행이다.

### 2.2 상태 구조체 — 초기값 승계 (회귀 1순위)

| 원본 | 초기값 | 신규 필드 |
|---|---|---|
| `StartFlag` | `false` | `start_flag` |
| `s_tdc_cradle_cover_closed_edge` | `false` | `cradle_cover_closed_edge` |
| `isPowerOffEnabled` | `false` | `poweroff_enabled` |
| `PowerOff_StartCounter` | `0` | `poweroff_start_counter` |
| `ISD_Disconnection_counter` | **`Df_Disconnection_BLE_Time_ms`(2000)** | `isd_disconnection_counter` |
| `lowBatteryIndicatorCounter` | `0` | `low_batt_indicator_counter` |
| `prev_batteryChargerConnectionStatus` | `df_Defalut` | `prev_carrying_case_state` (**이름 정정** — 실제로 담는 값이 `carryingCasePluggedIn`) |

> [!CAUTION]
> **zero-init 금지.** `isd_disconnection_counter` 가 0 이면 `< 300` 판정이 부팅 직후 참이 되어 **전원버튼이 무시되는 회귀**가 난다.

### 2.3 핸들러 분해

| 핸들러 | 담당 | 특기 |
|---|---|---|
| `handle_error()` | 에러 LED 요청 | 상태 없음 |
| `gate_power_button()` | 매핑/ISD 최근연결 시 버튼 무시 | **반환값 필수** — 지역 수정이 이후 파워오프 트리거로 전파돼야 함 |
| `handle_poweroff()` | 파워오프 진행/탈출 | `very_low_battery` 입력 필요(탈출 조건) |
| `handle_running()` | 저배터리 알림 · ISD 카운터 · 3분 타임아웃 | **반환값 = 자극 트리거** |
| `handle_charging()` | 충전기 연결 시 처리(크래들 엣지 포함) | |

### 2.4 1-tick 지연 (유지)

`conneded_ISD` / `mappingConnected` 는 지난 tick 값이다(순환 의존). 소비처 4곳 전부 시간 누적·인간 조작 스케일이라 무해 — 기존 주석 유지.

## 3. 불가침

| 항목 | 방침 |
|---|---|
| `LSAD_IRQHandler` | SDK 벡터 링크 — rename 금지 |
| 공유 ABI | `ST__USB_CONNECTOR` · `EN__SYSTEM_OP_MODE` 등 **공유 헤더 정의 타입은 보류 목록** |
| `tdc_drv_max17262` | G3 에서 이월된 사장 후보 — 배터리 도메인과 함께 판단 |
| 동작 보존 | FSM 분해는 구조 변경만. 분기 경로별 등가 정적 대조 |

## 4. 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| sys | `tdc_sys_control`(FSM + 핸들러 5) · `tdc_sys_init` · `tdc_sys_error` | **[로직설명]** (전이 등가 대조) |
| pwr | `tdc_pwr_battery` · `tdc_pwr_lsad` · `tdc_pwr_clock` | **[로직설명]** |

의존: `sys/control` → `pwr/battery` · `led` · `isd` 통과 전제. `pwr/battery` → `hal/i2c`(G3) · `pwr/lsad` 통과 전제.

## 5. 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | (구현 후) |
| 검증_공통_2 (공유 ABI 무변경) | (구현 후) |
| 검증_공통_3 (균형·훅) | (구현 후) |
| 검증_공통_4 (사장 실증) | (구현 후) |
| **검증_G4_특화_1 (초기값 승계)** | `isd_disconnection_counter` = 2000 — **회귀 1순위** |
| **검증_G4_특화_2 (초기값 전수)** | 원본 7개 ↔ 신규 7개 대조 |
| **검증_G4_특화_3 (버튼 전파)** | `gate_power_button()` 반환값 재대입 |
| **검증_G4_특화_4 (자극 트리거 전파)** | `handle_running()` 반환값 반영 |
| **검증_G4_특화_5 (분기 등가)** | 충전기 3분기 및 하위 경로 |
| 검증_공통_5 (빌드·실기) | ⏳ 은수님 게이트 — **전원버튼·절전·충전·크래들 확인 권장** |
