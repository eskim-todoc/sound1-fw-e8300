---
name: 에이전트-14-결과
purpose: batteryNPowerControl.h/c 삽입 위치·삭제 대상 탐색 결과
type: agent-log
maturity: draft
tags: [cradle, batteryNPowerControl, 구현, 탐색]
---

# 에이전트 14 결과 요약

**TL;DR**: batteryNPowerControl.h 헤더 끝 69라인. 정적 변수 블록 18-22라인. snd_charger_set_state() 92-142라인. carryingCaseCoverOpen 갱신 3곳: 122·130·138라인. 새 함수 삽입 기준점 143라인.

## batteryNPowerControl.h

| 항목 | 라인 |
|---|---|
| `snd_charger_get_state()` 선언 | 66 |
| `snd_charger_set_state()` 선언 | 67 |
| 헤더 끝 (`#endif`) | 69 |

## batteryNPowerControl.c

| 항목 | 라인 |
|---|---|
| 정적 변수 블록 | 17-22 |
| `snd_charger_set_state()` 시작 | 92 |
| `snd_charger_set_state()` 끝 | 142 |
| `carryingCaseCoverOpen` CONNECTED | 122 |
| `carryingCaseCoverOpen` DISCONNECTED | 130 |
| `carryingCaseCoverOpen` RESET | 138 |
| 새 함수 삽입 기준점 | 143 이후 |
