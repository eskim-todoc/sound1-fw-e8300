---
name: 에이전트-12-결과
purpose: 아키텍처 & 통합 설계 결과 요약 (Rev.1)
type: agent-log
maturity: draft
tags: [cradle, architecture, 통합설계]
---

# 에이전트 12 결과 요약

**TL;DR**: 전체 데이터 흐름 확정. 변경 파일 5개, 신규 심볼 5개. 뚜껑 닫힘 첫 감지 → func_cradle_lid_closed_loop() → GPIO 폴링 → 워치독 리셋. CFX 유지, QCC 2.2초 딜레이 필수.

## 전체 데이터 흐름

```
QCC → 0x34 패킷 → ble_communication.c
  → data[2] 파싱
  → tdc_charger_set_cradle_cover_state(data[2])
    → batteryNPowerControl.c → carryingCaseCoverOpen 갱신
  → systemControl() 다음 호출 시 carryingCaseCoverOpen 읽기
    → 뚜껑 닫힘 첫 감지 (s_tdc_cradle_cover_closed_edge)
    → systemState.cradleLidClosed = true
  → func_normal() 내 감지 후 break
  → func_cradle_lid_closed_loop() 호출
    → 약 절전 진입 시퀀스 실행
    → 폴링 루프 (GPIO + bleCommunication)
    → 뚜껑 열림(GPIO) → SYS_WATCHDOG_RESET() 재부팅
```

## 변경 파일 요약

| 파일 | 변경 | 핵심 내용 |
|---|---|---|
| `batteryNPowerControl.h` | 수정 | 신규 setter/getter 선언 |
| `batteryNPowerControl.c` | 수정 | `tdc_charger_set_cradle_cover_state()` 구현 + snd_charger_set_state() 수정 |
| `ble_communication.c` | 수정 | data[2] 파싱 + setter 호출 |
| `systemControl.c` | 수정 | 엣지 플래그 + 첫 닫힘 신호 |
| `main.c` | 수정 | `func_cradle_lid_closed_loop()` 신설 |

## 신규 심볼

| 심볼 | 파일 | 역할 |
|---|---|---|
| `tdc_charger_set_cradle_cover_state(int)` | batteryNPowerControl.c | data[2] → carryingCaseCoverOpen |
| `tdc_cradle_get_cover_state(void)` | batteryNPowerControl.c | 상태 조회 getter |
| `s_tdc_cradle_cover_state` | batteryNPowerControl.c | 내부 상태 저장 |
| `s_tdc_cradle_cover_closed_edge` | systemControl.c | 첫 닫힘 엣지 플래그 |
| `func_cradle_lid_closed_loop(void)` | main.c | 약 절전 루프 |
