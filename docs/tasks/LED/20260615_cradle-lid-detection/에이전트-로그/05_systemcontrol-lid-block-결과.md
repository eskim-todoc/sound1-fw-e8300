---
name: 에이전트-05-결과
purpose: systemControl.c 뚜껑 감지 블록 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, systemControl, lid-detection]
---

# 에이전트 05 결과 요약

**TL;DR**: systemControl.c:161~205 뚜껑 처리 블록 확인. 기존에 4501 틱 타이머 후 systemOff 진입 구조. df_Connected == df_Opened (값 1) 동일값. carryingCaseCoverOpen 갱신 경로 2개 발견.

## 핵심 발견

- `systemControl.c:161~205` — 충전기 연결 블록 전체 확인
- `LED_OnTime_afterCoverClosed = 4501` 틱 (systemControl 호출 주기에 따라 약 7.5분)
- `static int CounterAfterCoverClosed = 0` — 라인 133
- 뚜껑 닫힘 첫 감지 삽입 최적 위치: **라인 178 else 블록 진입 시** (`CounterAfterCoverClosed == 0`)
- `df_Connected == df_Opened == 1`, `df_Disconnected == df_Closed == 2`
- `carryingCaseCoverOpen` 갱신 주체: `readUsbConnectorState()`(GPIO) + `snd_charger_set_state()`(BLE)
- `snd_charger_get_state()` 호출로 main.c에서 chargerState 획득 (라인 440)
