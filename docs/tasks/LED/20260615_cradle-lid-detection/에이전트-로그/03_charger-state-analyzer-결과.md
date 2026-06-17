---
name: 충전기 상태 분석가 결과
purpose: charger_connected 및 EN__SND_CHARGER_STATE 전수 파악 결과
type: tasks/에이전트-로그
maturity: done
tags: [agent-result, charger, state, snd-charger, getter-setter]
---

# 충전기 상태 분석가 결과

**TL;DR**: 충전기 상태 접근은 `snd_charger_get_state()` Getter 경유. `ST__USB_CONNECTOR` 구조체에 `carryingCaseCoverOpen` 필드가 이미 존재하나, 기존 구현에서는 미활용 상태.

---

## 1. EN__SND_CHARGER_STATE enum (batteryNPowerControl.h:53-58)

```c
typedef enum {
    EN__SND_CHARGER_STATE_RESET = 0,
    EN__SND_CHARGER_STATE_CONNECTED,       // = 1
    EN__SND_CHARGER_STATE_DISCONNECTED     // = 2
} EN__SND_CHARGER_STATE;
```

## 2. 내부 변수

- **파일**: `batteryNPowerControl.c:22`
- `volatile EN__SND_CHARGER_STATE s_snd_charger_state = EN__SND_BATT_STATE_RESET;`
- 외부에서 직접 접근 불가 — Getter/Setter 함수만 제공

## 3. Getter/Setter API (batteryNPowerControl.h:66-67)

```c
ST__USB_CONNECTOR snd_charger_get_state(void);        // 공유 메모리 구조체 반환
void              snd_charger_set_state(EN__SND_CHARGER_STATE state);
```

## 4. ST__USB_CONNECTOR 구조체 (cfx_cm3_sharedMemory.h:180-185)

```c
typedef struct {
    int chargerConnectorPluggedIn;  // 충전 USB 케이블 상태
    int carryingCasePluggedIn;      // 크래들 장착 상태
    int carryingCaseCoverOpen;      // 크래들 커버 열림 상태 ← 이미 필드 존재
} ST__USB_CONNECTOR;
```

**상태 값 매핑** (processorDirective.h:181-187):
- `df_Defalut = 0`, `df_Connected = 1`, `df_Disconnected = 2`
- `df_Opened = 1`, `df_Closed = 2`

## 5. snd_charger_set_state() 내 carryingCaseCoverOpen 처리

기존 구현에서 CONNECTED/DISCONNECTED 시 모두 `carryingCaseCoverOpen = df_Disconnected`로 고정 → 뚜껑 상태가 별도로 갱신되지 않음. **본 작업에서 data[2]로 별도 관리 필요.**

## 6. 충전기 상태 갱신 시점

- `ble_communication.c:119~162` (0x34 패킷 수신) → `snd_charger_set_state()` 호출
- `initialize.c:504` (초기화 시) → `EN__SND_CHARGER_STATE_RESET`

## 7. 동기화 위험 평가

- BLE 패킷 처리는 메인 루프에서 실행 → ISR과의 race 없음
- `tdc_led_set_cradle_inhibit()` 또한 메인 루프에서 호출 → `volatile` 불필요
- 단, LED arbiter는 Timer3 ISR에서 실행 → `static bool s_tdc_led_cradle_inhibit`을 `volatile`로 선언 권장
