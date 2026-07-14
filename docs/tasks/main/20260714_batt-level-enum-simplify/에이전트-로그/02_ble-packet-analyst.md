---
name: BLE 패킷 배터리 전송 조사 (인바운드·아웃바운드 전수)
purpose: "EN__BATTERY_LEVEL enum 제거가 BLE 패킷에 영향을 주는지" 판정 — BLE/원격 통신에서 배터리 percent·level이 송·수신 양방향으로 어떤 형태로 오가는지 전수 조사
type: agent-log
maturity: experimental
tags: [main, battery, EN__BATTERY_LEVEL, ble, 0x33, 0x34, remoteControl, mappingControl, agent-log]
---

# 페르소나_02 — BLE 패킷 배터리 전송 조사

**TL;DR**: 인바운드는 선행 분석(03_qcc-source-margin-analyst.md) 그대로 재확인 — `0x34`(Power info) 수신 시 `snd_batt_set_percent()`로 percent만 저장(`ble_communication.c:130`). 아웃바운드는 **4개 경로**를 신규 확인: `0x33`(레거시 Battery 응답), `0x43`(리모콘 외부기기 상태), `0x67`(매핑 deviceStatus), `en__mapping_live_stimulation` 서브커맨드7(장치상태) — **전부 `readBatteryPercentage()`/`snd_batt_get_percent()`를 통한 raw percent(0~100, int)** 이며 `EN__BATTERY_LEVEL`/`snd_batt_get_level()`은 BLE 폴더 전체(`ble_communication.c`, `remoteControl.c`, `mappingControl.c`, `ble_commonProtocol.h`) 어디에도 **단 한 번도 등장하지 않는다**(grep 0건). 공유메모리 경유 배터리 필드(`batteryLevel_CfX_to_CM3`)는 CFX→CM3 방향의 고아 코드(호출처 없음)로 BLE와 무관. **결론: enum은 BLE 패킷에 전혀 관여하지 않으며, enum 제거는 BLE 코드에 영향 없음.**

## 1. 조사 범위·방법

- `src/2__cm3/Cortex-M3-src/BleCommunication/` 전체: `ble_communication.c/.h`, `ble_commonProtocol.h`, `remoteControl.c/.h`, `remoteControl_read_SP_para.c/.h`, `mappingControl.c/.h`
- 관련 심볼 전 트리 grep: `snd_batt_set_percent`, `snd_batt_get_percent`, `snd_batt_get_level`, `readBatteryPercentage`, `EN__BATTERY_LEVEL`, `en__batteryPower_`, `battery`/`Battery`/`BATT`, `cfx_cm3_sharedMemoryAll`
- 선행 참고: `docs/tasks/main/20260714_battery-led-refactor/에이전트-로그/03_qcc-source-margin-analyst.md` (0x34 수신 경로 이미 조사됨) — 본 문서에서 재확인 + 아웃바운드로 범위 확장

## 2. 인바운드 (QCC → E8300): 0x34 Power Info — 선행 분석 재확인

`ble_communication.c:118-165`, `setting_nrf_ble_adv_info()` 내 `EN__SND_BT_CMD_SYSTEM_INFO_POWER` 분기:

```c
battery_level     = bleSettingPacket.data[0];  // 배터리 레벨 (실제로는 percent 값)
charger_connected = bleSettingPacket.data[1];  // 충전기 연결 상태
cradle_lid_state  = bleSettingPacket.data[2];  // 크래들 뚜껑 상태

snd_batt_set_percent(battery_level);  // ble_communication.c:130 — 가공 없는 단순 대입

switch (charger_connected)
{
    case 0: snd_charger_set_state(EN__SND_CHARGER_STATE_DISCONNECTED); snd_batt_set_state(EN__SND_BATT_STATE_DISCHARGING); break;
    case 1: snd_charger_set_state(EN__SND_CHARGER_STATE_CONNECTED);    snd_batt_set_state(EN__SND_BATT_STATE_CHARGING);    break;
    default: snd_charger_set_state(EN__SND_CHARGER_STATE_RESET); snd_batt_set_state(EN__SND_BATT_STATE_RESET); break;  // 이상값 방어
}
```

- 저장 대상: `s_snd_batt_percent`(전역, `batteryNPowerControl.c:18`) — **int, 0~100 percent**. `EN__BATTERY_LEVEL` 타입 저장 없음.
- 응답 패킷: `Tx_dataBuff = {EN__SND_BT_CMD_SYSTEM_INFO_POWER, 1}` (수신 확인 ACK만, 배터리 값 재전송 없음) — `ble_communication.c:157-158`.
- state/charger: `snd_batt_set_state()`(`EN__SND_BATT_STATE_*`)·`snd_charger_set_state()`(`EN__SND_CHARGER_STATE_*`) 별도 enum이지만, 이들은 `EN__BATTERY_LEVEL`과 **다른 enum**(충전 상태/방전 상태용, 배터리 잔량 등급용 아님) — 이번 조사 대상(EN__BATTERY_LEVEL) 무관.
- 선행 문서(03) 대비 추가 확인 사항 없음 — 동일 결론 재확인.

## 3. 아웃바운드 (E8300 → 앱/QCC): 배터리 송신 경로 4건 확인

**있다.** 확인 결과 다음 4개 응답 패킷이 배터리 percent를 payload에 싣는다. 모두 **`readBatteryPercentage()`(또는 동일 값인 `snd_batt_get_percent()`) 경유 — raw int percent**이며 `snd_batt_get_level()`은 어디에도 관여하지 않는다.

### 3.1 `0x33` EN__SND_BT_CMD_SYSTEM_INFO_BATTERY (레거시, "사실상 쓸모없어진 명령")

`ble_communication.c:90-117`, `setting_nrf_ble_adv_info()` 분기:

```c
if (snd_batt_get_state() != EN__SND_BATT_STATE_RESET)
{
    batt_percent = snd_batt_get_percent();      // :100 — percent 직접
}
else
{
    batt_percent = 0xFF;                        // RESET 상태 센티널
}
charger_state = snd_charger_get_state().chargerConnectorPluggedIn;

Tx_dataBuff[tx_index++] = EN__SND_BT_CMD_SYSTEM_INFO_BATTERY;  // 0x33
Tx_dataBuff[tx_index++] = batt_percent;   // 바이트 인덱스 1
Tx_dataBuff[tx_index++] = charger_state;  // 바이트 인덱스 2 (0:RESET,1:CONNECTED,2:DISCONNECTED)
```

- 트리거: QCC가 헤더 `0x33`만 담은 요청 패킷을 보내면 `bleCommunication()`(`ble_communication.c:334-342`)이 `fetch_readDataForBleSetting()`으로 `bleSettingPacket.command`를 세팅 → 다음 루프에서 `setting_nrf_ble_adv_info()`가 이 분기로 응답.
- 주석(`ble_communication.c:96-97`): "배터리 레벨을 QCC에게 수신한 이후로만 0xFF가 아닌 값을 전송한다. **사실상 QCC가 배터리 레벨을 측정하기로 한 뒤로 쓸모가 없는 명령이 되었다.**" — QCC가 준 값을 되돌려주는 순환 구조(레거시 유지 코드로 보임), 그러나 **코드는 아직 살아있고 실제로 SPI TX에 실린다.**
- 형태: **percent(int, 0~100 또는 0xFF 센티널)**. enum 아님.

### 3.2 `0x43` en__remoteControl_readStatusOfExtenalDevice (리모콘, "외부기기 상태 읽기")

`remoteControl.c:964-989`:

```c
case en__remoteControl_readStatusOfExtenalDevice:  // 0x43 (ble_commonProtocol.h:54)
{
    value = readBatteryPercentage();     // :966
    if (value > 100) value = 100;
    else if (value < 0) value = 0;

    bufferForSPI_tx[tx_index++] = remoteDataPacket.command;     // command loop-back
    bufferForSPI_tx[tx_index++] = value;                        // 배터리 잔량  ← 바이트 인덱스 1
    bufferForSPI_tx[tx_index++] = readProgramMapNum();          // 맵 번호
    bufferForSPI_tx[tx_index++] = readStimulVolume();           // 최대 출력
    bufferForSPI_tx[tx_index++] = readAudioVolume();            // 볼륨
    bufferForSPI_tx[tx_index++] = readLED_indicatorOnOff();     // LED 알림
    bufferForSPI_tx[tx_index++] = readTeleCoil_OnOff();         // 텔레코일
    bufferForSPI_tx[tx_index++] = readStimulIndicator_OnOff();  // 자극 알림

    writeDataToSpiTxBuff(bufferForSPI_tx, tx_index);
    clearRemoteColtrolCommand();
}
```

- opcode `0x43` = `en__remoteControl_readStatusOfExtenalDevice`, `ble_commonProtocol.h:54` 주석: "43 (리모콘 사용) // 확인 완료: 헤더 only 패킷" — 리모콘(외부기기) 쪽 상태 조회 요청에 대한 응답.
- 형태: `readBatteryPercentage()` 결과를 0~100으로 **클램프**(`remoteControl.c:968-975`)한 **int percent**. enum 아님.

### 3.3 `0x67` en__mapping_deviceStatus (매핑, "장치 상태")

`mappingControl.c:2060-2092`:

```c
case en__mapping_deviceStatus:  // 0x67 (ble_commonProtocol.h:95)
{
    bufferForSPI_tx[buffer_tx_index++] = en__mapping_deviceStatus;  // command loop-back
    bufferForSPI_tx[buffer_tx_index++] = ISD_state.conneded_ISD ? 1 : 2;  // 내부기 연결 상태
    bufferForSPI_tx[buffer_tx_index++] = readBatteryPercentage();  // :2076 — 배터리 레벨(주석) = 실제로는 percent
    errorCode = readErrorCode();
    bufferForSPI_tx[buffer_tx_index++] = (int) errorCode.ISD_ErrorFlag;
    bufferForSPI_tx[buffer_tx_index++] = 0;

    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
    clear_mappingCommand();
}
```

- 주석은 "배터리 레벨"이라 적혀 있으나 실제 호출 함수는 `readBatteryPercentage()` — **네이밍만 "레벨"이지 값 자체는 percent**. `EN__BATTERY_LEVEL`/`snd_batt_get_level()` 호출 아님. 클램프 없음(0~100 밖 값 방어 없음, 3.2와 차이).

### 3.4 `en__mapping_live_stimulation`(0x66) 서브커맨드 7 `en__readDeviceStatus` ("장치 상태 읽기")

`isd_interface_mapping_Live.c:438-471` (호출 체인: 앱 → `en__mapping_live_stimulation`(0x66, `ble_commonProtocol.h:94`) → `mappingControl.c:941-943`에서 서브커맨드 `en__readDeviceStatus`(=7, `isd_interface_mapping_Live.h:16`)로 라우팅 → `liveStimulation()` 내부 처리):

```c
case en__readDeviceStatus:
{
    bufferForSPI_tx[buffer_tx_index++] = p_mappingPacket->command;   // command loop-back
    bufferForSPI_tx[buffer_tx_index++] = en__readDeviceStatus;        // 서브커맨드 echo
    bufferForSPI_tx[buffer_tx_index++] = ISD_state.conneded_ISD ? 1 : 2;  // 연결 상태

    // value=(int)readBatteryLevel();   ← 죽은 주석, readBatteryLevel()은 코드베이스 전체에 정의 없음
    value = readBatteryPercentage();     // :460
    bufferForSPI_tx[buffer_tx_index++] = value;

    writeDataToSpiTxBuff(bufferForSPI_tx, buffer_tx_index);
}
```

- 흥미로운 발견: `// value=(int)readBatteryLevel();`(:459) 죽은 주석이 남아있으나, `readBatteryLevel()`(Percentage 없이)이라는 함수는 **코드베이스 어디에도 정의돼 있지 않다**(grep 0건) — 과거 존재했다가 삭제됐거나 오탈자로 추정. 현재 라이브 코드는 `readBatteryPercentage()`를 쓴다. 이 죽은 주석도 `EN__BATTERY_LEVEL` enum과는 무관(이름만 유사).
- 형태: **int percent**, 클램프 없음.

### 3.5 `readBatteryPercentage()` 정의 — 4경로 공통 소스

`batteryNPowerControl.c:330-334`:
```c
int readBatteryPercentage(void)
{
    return s_snd_batt_percent;
}
```
`snd_batt_get_percent()`(`batteryNPowerControl.c:37-40`)와 **동일한 전역(`s_snd_batt_percent`)을 읽는 별도 wrapper**일 뿐 — 3.1은 `snd_batt_get_percent()`를 직접 호출하고 3.2~3.4는 `readBatteryPercentage()`를 호출하지만 결과값은 완전히 동일하다. 두 함수 모두 `EN__BATTERY_LEVEL`을 반환하지 않는다.

## 4. enum(EN__BATTERY_LEVEL) 관여 여부 — 전수 grep 결과: 0건

```
grep "EN__BATTERY_LEVEL|snd_batt_get_level|en__batteryPower_" src/2__cm3/Cortex-M3-src/BleCommunication/**
→ No matches found
```

`ble_communication.c`, `ble_communication.h`, `ble_commonProtocol.h`, `remoteControl.c`, `remoteControl.h`, `remoteControl_read_SP_para.c/.h`, `mappingControl.c`, `mappingControl.h` — **BLE 통신 폴더 전체에서 `EN__BATTERY_LEVEL`도 `snd_batt_get_level()`도 `en__batteryPower_*` 값도 단 한 곳도 등장하지 않는다.** (`readBatteryPercentage`/`snd_batt_get_percent`/`snd_batt_set_percent` 호출 4+2건만 존재, §2·§3 전수 나열.)

**판정: BLE 패킷(인바운드·아웃바운드 모두)은 배터리 정보를 오직 raw percent(int, 0~100)로만 주고받는다. `EN__BATTERY_LEVEL` enum은 BLE 코드 경로에 전혀 관여하지 않는다.**

## 5. 공유메모리(`cfx_cm3_sharedMemoryAll`) 경유 간접 경로 — 확인, BLE 무관

`cfx_cm3_sharedMemoryAll` 참조 22개 파일 전수 확인 결과, 배터리 관련 필드는 `cfx_cm3_sharedMemory.h`에 2개:

```c
int batteryLevel_CfX_to_CM3;   // :151 (구조체 필드, int — EN__BATTERY_LEVEL 아님)
int batteryCalibrationValue;   // :218
```

- `readBatteryLevel_FromCFX()`(`cfx_cm3_sharedMemory.c:191-194`)가 `batteryLevel_CfX_to_CM3`를 읽어 반환하지만, **이 함수의 호출처가 코드베이스 전체에 없다**(grep 0건) — CFX(DSP) 자체 ADC 측정치를 CM3로 넘기던 레거시 경로로, 선행 작업(`docs/tasks/main/20260714_battery-led-refactor`)에서 `updateBatteryLevel()`(유일 호출처)이 제거되며 함께 고아가 된 것으로 보인다(`batteryNPowerControl.c:336-338` 주석 "dead code가 되어 제거함(2026-07-14)" 참조).
- 이 필드는 이름에 "Level"이 들어가지만 타입이 `int`(원시 ADC 파생값)이지 `EN__BATTERY_LEVEL` enum이 아니며, 방향도 **CFX(DSP)→CM3**이지 BLE로 나가는 방향이 아니다. BLE 아웃바운드 패킷(§3의 4경로) 어디도 이 필드를 참조하지 않는다 — 전부 `s_snd_batt_percent`(QCC 0x34가 채워준 값)만 사용.
- 참고로 `snd_charger_get_state()`(`batteryNPowerControl.c:90-93`)는 `cfx_cm3_sharedMemoryAll.chargerState`를 읽어 §3.1의 0x33 응답 payload에 싣는다 — 이는 충전기 연결 상태(`ST__USB_CONNECTOR`)이지 배터리 레벨 enum이 아니므로 이번 조사 대상 밖.

**결론: 공유메모리를 경유해 `EN__BATTERY_LEVEL`이 간접적으로 BLE로 나가는 경로는 없다.**

## 6. 종합 판정

| 항목 | 결론 |
|---|---|
| 인바운드(0x34) 배터리 저장 형태 | percent(int, `snd_batt_set_percent`) — 선행 분석과 동일 |
| 아웃바운드 배터리 송신 패킷 존재 여부 | **있음, 4건**(0x33 레거시, 0x43 리모콘, 0x67 매핑 deviceStatus, 0x66 서브커맨드7 liveStimulation deviceStatus) |
| 아웃바운드 배터리 형태 | **전부 percent(int, `readBatteryPercentage()`/`snd_batt_get_percent()`)** — level 아님 |
| `EN__BATTERY_LEVEL`/`snd_batt_get_level()` BLE 관여 | **전무(grep 0건, BLE 폴더 전체)** |
| 공유메모리 경유 간접 노출 | 없음 — 관련 필드(`batteryLevel_CfX_to_CM3`)는 고아 코드(호출처 없음)이며 방향도 BLE 아님 |
| **enum 제거의 BLE 영향** | **없음** — BLE 패킷 송·수신 어느 경로도 `EN__BATTERY_LEVEL`을 참조하지 않으므로, enum·`snd_batt_get_level()`을 제거해도 BLE 통신 코드(`ble_communication.c`, `remoteControl.c`, `mappingControl.c`, `isd_interface_mapping_Live.c`)는 **한 줄도 수정할 필요가 없다** |

## 자체 검토

### 지침 준수 여부
| 항목 | 준수 |
|---|---|
| 읽기 전용, `src/` 무수정 | 예 — Read/Grep/Bash(파일 탐색만) 사용, Edit/Write는 본 문서에만 |
| 추측·확인 엄격 구분 | 예 — §5 고아 코드 판단은 "grep 0건" 근거 명시, §3.4 죽은 함수(`readBatteryLevel`)는 "정의 없음" 명시 |
| 파일:라인 근거 | 예 — 전 절 `파일:라인` 표기 |
| 선행 분석(03) 재확인·중복 배제 | 예 — §2는 재확인만, 신규 분석은 §3(아웃바운드) 이후 집중 |

### 논리적 정합성
- 요구사항 질문_2("BLE 패킷 관련 배터리 송신 경로가 있는가")에 대해 "없다"고 성급히 결론내리지 않고 grep 재검증으로 4개 경로를 실제 발견 — 코드 표면(0x34 인바운드)만 보고 넘어가지 않은 것이 이번 조사의 핵심 기여.
- "배터리 레벨"이라는 한글 주석(3.3)에 현혹되지 않고 실제 호출 함수(`readBatteryPercentage()`)를 추적해 percent임을 확인 — 주석과 실제 타입/함수명이 불일치하는 사례를 명시적으로 지적함.
- enum 미관여 판정은 단일 파일이 아니라 BLE 폴더 전체(8개 소스/헤더) grep으로 뒷받침 — 근거의 범위를 명시.

### 미해결 이슈
- `0x33`(레거시 Battery) 응답이 실제로 QCC/앱 어느 쪽에서 아직 폴링하는지는 이 저장소 범위에서 확인 불가(QCC/앱 펌웨어 밖, 요구사항 §5.2 범위외_1과 동일 성격) — 다만 이번 조사 목적(enum 관여 여부)엔 영향 없음(살아있든 죽었든 percent만 사용).
- `readBatteryLevel()`(Percentage 없는 이름) 죽은 주석의 유래(과거 실제 함수였는지 오탈자였는지)는 git blame 추가 조사가 필요하나, enum 제거 판정과 무관하므로 본 조사 범위 밖으로 유보.
