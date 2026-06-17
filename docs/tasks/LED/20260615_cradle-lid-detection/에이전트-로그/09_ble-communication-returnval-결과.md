---
name: 에이전트-09-결과
purpose: bleCommunication() 반환값 구조 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, bleCommunication, 반환값]
---

# 에이전트 09 결과 요약

**TL;DR**: bleCommunication() 반환: ST__BLE_COMMUNICATION_STATE {isdControlCommand, mappingConnection, StimulationIndicatorTrigger, BLE_Off_Command}. isd_state.conneded_ISD만 실제 사용. 더미값 {en__isdStatus_NA, false} 전달 가능. 뚜껑 열림 감지는 반환값으로 불가.

## ST__BLE_COMMUNICATION_STATE 정의

```c
// ble_communication.h:11~18
typedef struct {
    EN__ISD_CONTROL_STATE isdControlCommand;
    bool                  mappingConnection;
    bool                  StimulationIndicatorTrigger;
    bool                  BLE_Off_Command;
} ST__BLE_COMMUNICATION_STATE;
```

## isd_state 사용처

- `ble_communication.c:402`: `remoteControl(isd_state.conneded_ISD)` — conneded_ISD만 사용
- `ble_communication.c:405`: `mappingControl(isd_state)` — 전체 구조체 전달
- 약 절전 루프에서 더미값 `{en__isdStatus_NA, false}` 사용 가능

## 뚜껑 열림 감지 불가 이유

- 반환값 구조체에 뚜껑 상태 필드 없음
- QCC 셧다운 후 0x34 패킷 안 옴 → BLE 패킷 기반 감지 불가
- **권장**: GPIO 폴링 (`snd_charger_get_state().carryingCaseCoverOpen` 또는 `Sys_GPIO_Read()` 직접)
