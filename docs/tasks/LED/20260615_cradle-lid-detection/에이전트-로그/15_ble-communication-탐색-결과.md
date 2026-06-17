---
name: 에이전트-15-결과
purpose: ble_communication.c 0x34 블록 정확한 위치 탐색 결과
type: agent-log
maturity: draft
tags: [cradle, ble_communication, 구현, 탐색]
---

# 에이전트 15 결과 요약

**TL;DR**: 0x34 블록 119-162라인. data[0]/[1] 파싱 124-125라인. data[2] 미파싱. snd_charger_set_state() 호출 136·141·147라인. batteryNPowerControl.h 10라인에서 포함됨.

## 0x34 블록 구조

| 항목 | 라인 |
|---|---|
| 블록 시작 | 119 |
| data[0] 파싱 | 124 |
| data[1] 파싱 | 125 |
| switch 시작 | 133 |
| DISCONNECTED snd_charger_set_state() | 136 |
| CONNECTED snd_charger_set_state() | 141 |
| default snd_charger_set_state() | 147 |
| switch 끝 | 150 (`}`) |
| 블록 끝 | 162 |

## include 확인

- `batteryNPowerControl.h`: 10라인 포함 ✓ (`tdc_charger_set_cradle_cover_state()` 접근 가능)
