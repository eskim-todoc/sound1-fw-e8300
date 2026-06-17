---
name: 에이전트-10-결과
purpose: carryingCaseCoverOpen 갱신 경로 설계 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, carryingCaseCoverOpen, 상태갱신]
---

# 에이전트 10 결과 요약

**TL;DR**: carryingCaseCoverOpen 갱신 주체 2개: readUsbConnectorState()(GPIO 주기) + snd_charger_set_state()(BLE). snd_charger_set_state(CONNECTED) 시 df_Disconnected 강제 덮어쓰기 문제. 신규 tdc_charger_set_cradle_cover_state() 함수(방법 B) 권장.

## 갱신 주체 분석

| 함수 | 갱신 방식 | 위험 |
|---|---|---|
| `readUsbConnectorState()` | GPIO 홀센서 → 주기 갱신 | BLE 값 덮어쓰기 가능 |
| `snd_charger_set_state(CONNECTED)` | `= df_Disconnected` 강제 설정 | BLE 값 즉시 소실 |

## 권장 방법 (B)

- 신규 `tdc_charger_set_cradle_cover_state(int state)` 함수 신설
- `snd_charger_set_state()` 내 `carryingCaseCoverOpen` 갱신 라인 제거
- BLE data[2] → 신규 함수 → 공유메모리 갱신

## 타이밍 분석

- BLE 0x34 수신 → `tdc_charger_set_cradle_cover_state()` 호출 → 즉시 공유메모리 갱신
- 다음 `systemControl()` 호출(약 100ms 후)에서 즉시 반영됨
- `readUsbConnectorState()` 주기 호출이 GPIO 값으로 덮어쓸 위험 — `snd_charger_set_state()` 수정으로 감소
