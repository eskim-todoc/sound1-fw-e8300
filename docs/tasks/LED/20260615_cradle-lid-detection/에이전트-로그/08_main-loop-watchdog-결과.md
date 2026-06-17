---
name: 에이전트-08-결과
purpose: main.c 루프 구조 & 워치독 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, main, watchdog, func_normal]
---

# 에이전트 08 결과 요약

**TL;DR**: main() 루프는 func_normal()→func_sleep() 순서. func_normal:330~781, systemControl():488, bleCommunication():511. 워치독 타임아웃 3.28초, 매 반복 776라인에서 리프레시. SYS_WATCHDOG_RESET()은 강제 타임아웃 트리거.

## func_normal() 주요 호출 라인

| 라인 | 함수 | 비고 |
|---|---|---|
| 488 | `systemControl(7 params)` | 시스템 상태 결정 |
| 502 | `isd_interface(3 params)` | ISD 상태 제어 |
| 511 | `bleCommunication(isd_state)` | BLE 통신 처리 |
| 776 | `SYS_WATCHDOG_REFRESH()` | 매 반복 리프레시 |
| 777 | `SYS_WAIT_FOR_INTERRUPT` | 인터럽트 대기 |
| 737 | `if (systemState.systemOff)` | 절전 진입 분기 |

## 워치독

- 타임아웃: **3.28초** (주석: main.c:899)
- ULP 웨이크업 주기: 200ms
- `SYS_WATCHDOG_RESET()`: 워치독 강제 타임아웃 → 재부팅 (hw.h 외부 매크로)

## func_cradle_lid_closed_loop() 권장 위치

- main.c에 `func_sleep()` 이전에 추가
- `func_normal()` 내 `systemState.cradleLidClosed` 감지 후 break
- main() 루프에서 별도 분기 없이 func_normal() → 검사 → func_cradle_lid_closed_loop() 순서
