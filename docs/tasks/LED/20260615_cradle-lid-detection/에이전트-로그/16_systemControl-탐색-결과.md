---
name: 에이전트-16-결과
purpose: systemControl.c 뚜껑 블록 + EN__SYSTEM_STATE 구조체 탐색 결과
type: agent-log
maturity: draft
tags: [cradle, systemControl, 구현, 탐색]
---

# 에이전트 16 결과 요약

**TL;DR**: ST__SYSTEM_STATE 구조체 systemControl.h:13-23. cradleLidClosed 필드 없음(추가 필요). systemStatus 변수 31라인. CounterAfterCoverClosed 133라인. 뚜껑 열림 173-177, 뚜껑 닫힘 178-193.

## ST__SYSTEM_STATE 구조체 (수정 전)

```c
// systemControl.h:13-23
typedef struct
{
    EN__LED_PATTERN Led_Pattern;
    bool            enable_ISD;
    bool            enablePMIC;
    bool            BLE_Off;
    bool            StimulationIndicatorTriggerLowPower;
    bool            systemOff;
} ST__SYSTEM_STATE;
```

→ `bool cradleLidClosed` 추가 (구현에서 처리됨)

## 주요 라인

| 항목 | 라인 |
|---|---|
| `systemStatus` 변수 선언·초기화 | 31 |
| `CounterAfterCoverClosed` 선언 | 133 |
| 뚜껑 열림 블록 | 173-177 |
| 뚜껑 닫힘 else 블록 | 178-193 |
