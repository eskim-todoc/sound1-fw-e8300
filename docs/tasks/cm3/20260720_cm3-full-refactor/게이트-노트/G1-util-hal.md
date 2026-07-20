---
name: G1 util/hal 게이트 노트
purpose: G1(Gen1_5 common·crc -> hal/·util/) 파일·심볼 매핑, 유닛맵, 검증 기록
type: tasks/게이트노트
applies_to: [Sound1]
tags: [cm3, refactoring, gate, g1, hal, util]
---

# G1 — util/hal (Gen1_5/common · crc)

**TL;DR**: 의존 최다 피포함 계층을 먼저 규약화한다. 파일 7쌍을 `hal/`·`util/` 로 이동 + 심볼 약 60종 rename + **`ci_print*` 매크로 전역 치환(약 600 호출부)**. `ci_battery`/`ci_power` 는 pwr 도메인(G4)으로 이월. IRQ 핸들러명은 SDK 벡터 링크라 불가침.

## 파일 매핑

| 현행 | 신규 | 도메인 |
|---|---|---|
| `Gen1_5/common/ci_uart.c/.h` | `hal/tdc_hal_uart.c/.h` | hal |
| `Gen1_5/common/ci_dio.c/.h` | `hal/tdc_hal_dio.c/.h` | hal |
| `Gen1_5/common/ci_timer.c/.h` | `hal/tdc_hal_timer.c/.h` | hal |
| `Gen1_5/common/tdc_trims.c/.h` | `hal/tdc_hal_trims.c/.h` | hal (케이싱 정규화) |
| `Gen1_5/common/ci_util.c/.h` | `util/tdc_util.c/.h` | util |
| `Gen1_5/common/ci_printf.c/.h` | `util/tdc_printf.c/.h` | util (매크로 계층) |
| `Gen1_5/crc/ci_crc.c/.h` | `util/tdc_crc.c/.h` | util |
| `Gen1_5/common/ci_battery.c/.h` · `ci_power.c/.h` | (이동 없음) | **G4 이월** (pwr 도메인, 승인 약어표) |

`.cproject`: `Gen1_5/common` include 경로가 있는 각 도구 설정에 `hal` · `util` 경로 추가 (기존 경로 유지 — battery/power 잔류).

## 주요 심볼 매핑 (전체는 커밋 diff)

- 함수: `ci_uart_*`→`tdc_hal_uart_*` · `ci_dio_*`→`tdc_hal_dio_*` · `ci_timer_*`→`tdc_hal_timer_*` · `ci_util_*`/`delay_ms*`/`print_sysvar_manu_table`→`tdc_util_*` · `ci_crc_ccitt_calc`→`tdc_crc_ccitt_calc` · `tdc_Trims_SetX`→`tdc_hal_trims_set_x` · `OTE_1_5_gen_DIO_set`→`tdc_hal_dio_set_mode` · `tdc_timer_get_t3_tick`→`tdc_hal_timer_get_t3_tick`
- 매크로: `ci_print{e,w,i,d,v,f}` → `TDC_PRINTF_{E,W,I,D,V}` / `TDC_PRINTF` (약 600 호출부) · `CI_PRINT_EABLE_*`→`TDC_PRINTF_ENABLE_*`(**오타 EABLE 정정**) · `CI_UART_*`→`TDC_HAL_UART_*` · `CI_TIMER_BASE_YEAR`→`TDC_HAL_TIMER_BASE_YEAR`
- 타입: `CI_TIMER_TIME_T` → `tdc_hal_timer_time_t`

## 불가침 (G1 범위)

- **IRQ 핸들러명 유지**: `DIO_0_IRQHandler` · `DIO_1_IRQHandler` · `TIMER_3_IRQHandler` — SDK 벡터 테이블이 심볼명으로 링크
- `SEGGER_RTT_*` 호출부 (라이브러리)
- `ci_battery_*` · `ci_power_*` 심볼 (G4 소관)

## 유닛/모듈 맵

| 모듈 | 유닛 | 검증 라벨 |
|---|---|---|
| hal | uart · dio · timer · trims (각각 독립, 상호 비의존) | **[로직설명]** — rename 은 로직 무변경. 실기 스모크가 통합 검증 |
| util | util(delay/assert) · crc(순수 계산 유닛) · printf(매크로 계층, SEGGER 의존) | crc 는 **[유닛테스트]** 후보(결정론 입출력)이나 이번 게이트는 무변경 rename — [로직설명] |

의존: util/printf ← hal/uart(비활성 `#if` 분기 텍스트 참조) — 테스트 순서상 hal 이 선행이나 로직 무변경이라 동시 게이트 허용.

## 사장 제거 (개별 실증 후 — 별도 커밋)

후보: `uart_init`/`getch`/`set_color`/`clear_color` · `dio_is_set/clear_int_flag_*` 4종 · `timer_uninit` · `delay_ms_long_time` · `print_sysvar_manu_table` · `ci_error_printf`(사용 0 확인됨). **텍스트 전수 grep(비활성 `#if` 분기 포함) 으로 참조 0 실증된 것만 제거.**

## 검증

| 라벨 | 결과 |
|---|---|
| 검증_공통_1 (구 심볼 잔존 0) | ✅ 14패턴 + 추가 발견 3심볼(`ci_timer_get_reference_time_after_self_update` 등, 헤더 밖 정의) 전부 0 |
| 검증_공통_2 (공유 ABI 무변경) | ✅ `cfx_cm3_sharedMemory.h` diff = include 파일명 1줄뿐 |
| 검증_공통_3 (균형·훅) | ✅ 이동 14파일 + 제거 후 9파일 브레이스·전처리기 균형, 커밋 훅 통과 |
| 검증_공통_4 (사장 실증) | ✅ 후보 14건 텍스트 전수(비활성 `#if` 포함) 실증 → **12건 제거** · `uart_uninit` 유지(`initialize.c:203` 호출) · `uart_printf` 유지(`TDC_PRINTF` UART 분기 참조) |
| 검증_공통_5 (빌드·실기) | ✅ PASS — 은수님 확인 (2026-07-20): "빌드 후 정상동작 봤어" → **G1 폐쇄** |

**2차 파생 기록** (상한 규칙 — 제거 안 함): `tdc_hal_dio.c` 의 int flag static 2종은 is_set/clear 제거로 **쓰기 전용**이 됨(IRQ 가 set 만 함). 후속 게이트에서 IRQ 핸들러 정리 시 판단.
