---
name: BLE 패킷 분석가 결과
purpose: EN__SND_BT_CMD_SYSTEM_INFO_POWER 블록 및 bleSettingPacket 구조 분석 결과
type: tasks/에이전트-로그
maturity: done
tags: [agent-result, ble, packet, system-info-power]
---

# BLE 패킷 분석가 결과

**TL;DR**: 0x34 블록의 `data[0]`=배터리%, `data[1]`=충전기상태. `data[2]`는 현재 미사용 — 추가 가능. 페이로드 크기 19로 여유 충분.

---

## 1. EN__SND_BT_CMD_SYSTEM_INFO_POWER enum

- **파일**: `BleCommunication/ble_commonProtocol.h:40-46`
- **값**: `0x34`

```c
typedef enum {
    EN__SND_BT_CMD_SYSTEM_INFO_BATTERY       = 0x33,
    EN__SND_BT_CMD_SYSTEM_INFO_POWER         = 0x34,
    EN__SND_BT_CMD_SYSTEM_INFO_LED_IND       = 0x35,
    EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE = 0x36,
} EN__SND_BT_CMD_SYSTEM_INFO;
```

## 2. 0x34 블록 현재 data[] 파싱 (ble_communication.c:119~162)

```c
int battery_level     = bleSettingPacket.data[0];  // 배터리 레벨 (%)
int charger_connected = bleSettingPacket.data[1];  // 0=Disconnected, 1=Connected
// data[2] 미사용 ← 크래들 뚜껑 상태 추가 위치
```

switch (charger_connected) 분기:
- `case 0` → `snd_charger_set_state(DISCONNECTED)` + `snd_batt_set_state(DISCHARGING)`
- `case 1` → `snd_charger_set_state(CONNECTED)` + `snd_batt_set_state(CHARGING)`
- `default` → `snd_charger_set_state(RESET)` + 에러 로그

## 3. bleSettingPacket 구조체

- **파일**: `ble_communication.c:18-25`
- `data[todoc_PayloadSize]` — `todoc_PayloadSize = 19` (int 타입, 총 76바이트)
- data[2] 추가 시 페이로드 여유 충분

## 4. EN__SND_CHARGER_STATE enum

- **파일**: `systemControl/batteryNPowerControl.h:53-58`

```c
typedef enum {
    EN__SND_CHARGER_STATE_RESET = 0,
    EN__SND_CHARGER_STATE_CONNECTED,       // = 1
    EN__SND_CHARGER_STATE_DISCONNECTED     // = 2
} EN__SND_CHARGER_STATE;
```

## 5. 핵심 제약사항

- `fetch_readDataForBleSetting()`은 data[] 전체를 자동 복사 — data[2] 추가 시 이 함수 수정 불필요
- 0x35, 0x36 블록은 data[2]를 사용하지 않으므로 충돌 없음
- 응답 패킷 포맷(`0x34 + 0x01`)은 data[2] 추가 후에도 동일 유지
