---
name: NRF_On_OFF 조건 해부 — 페르소나 02
purpose: NRF_On_OFF() 함수 5개 조건 줄단위 해부, 입력 변수 추적, 死코드 함의, QCC 커버리지 판정
type: tasks
maturity: experimental
tags: [ble, nrf, qcc, 조건해부, 死코드]
---

# NRF_On_OFF 조건 해부

**TL;DR**: `NRF_On_OFF()`는 5개 조건 연쇄로 BLE_OFF 플래그를 결정하지만, 출력 `NRF_Off/On_Command()` 본문이 전부 주석 처리돼 현재 실효가 전혀 없다. 5개 조건 중 QCC 경로에서 명시적으로 커버되는 것은 ①② 일부(ISD 상태 핀 전달)뿐이며, ③(mappingConnection 유지), ④(Mapping_BLE_Off 일시 off), ⑤(global_BLE_Off) 3개는 QCC 경로에 명시적 대응이 없다.

---

## 1. 死코드 출력 확인

```c
// systemControl.c:37-45
void NRF_Off_Command(void)
{
    // Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);  ← 주석 처리
}

void NRF_On_Command(void)
{
    // Sys_GPIO_Set_High(DIO_NUM_NRF_ON_OFF_COMMAND); ← 주석 처리
}
```

**사실**: 두 함수 모두 본문이 주석 처리되어 GPIO 조작이 일어나지 않는다. `NRF_On_OFF()` 내부 조건 판단 결과가 아무리 정교해도 핀 출력 없음.

---

## 2. 내부 상태 변수

| 변수 | 위치 | 성격 | 초기값 |
|---|---|---|---|
| `ISD_ConnectionHistory` | systemControl.c:47 | file-scope `bool` | `false` |
| `deaylCounter` | systemControl.c:52 | function-scope `static int` | `0` |

- `ISD_ConnectionHistory`: ISD가 한 번이라도 연결(10V_Ok 이상)된 적이 있는지 기억
- `deaylCounter`: ISD 연결 이력 후 끊어진 뒤 경과 tick 카운터 (0.6초 = 640 tick)

---

## 3. 5개 결정 조건 줄단위 해부

> **우선순위 원칙**: 조건이 나열 순서대로 `BLE_OFF` 플래그를 덮어씀. 나중 조건이 앞 조건을 무조건 오버라이드.

### 조건 ① — ISD 연결 이력 후 끊김 0.6초 → BLE off

```c
// systemControl.c:55-78
if (isd_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)
{
    ISD_ConnectionHistory = true;   // 연결 이력 기록
}

if (ISD_ConnectionHistory)
{
    if (isd_state.isd_controlState < en__isdStatus_stimul_10V_Ok)
    {
        if (deaylCounter > 640)     // ≈ 0.6초 후
        {
            BLE_OFF               = true;
            ISD_ConnectionHistory = false;  // 이력 클리어
        }
        deaylCounter++;
    }
    else
    {
        deaylCounter = 0;           // ISD 재연결 시 카운터 리셋
    }
}
```

- **발동 조건**: ISD가 한 번 연결된 후 끊어진 상태가 640 tick(≈0.6초) 지속
- **결과**: `BLE_OFF = true`, `ISD_ConnectionHistory = false`
- **주석**: "연결된 내부기의 id에 해당하는 매핑데이터가 읽어들여진 이후에 nrf를 켜야한다" → 매핑 데이터 로딩 완료 전 대기 의도

### 조건 ② — ISD 미연결 초기 상태 또는 0.6초 경과 이후 → BLE off

```c
// systemControl.c:79-82
else  // ISD_ConnectionHistory == false
{
    BLE_OFF = true;  // "내부기가 연결되지 않은 초기 상태 또는 끊어지고 일정 시간이 지난 이후"
}
```

- **발동 조건**: `ISD_ConnectionHistory == false` (미연결 초기 상태 OR 조건①로 클리어된 후)
- **결과**: `BLE_OFF = true`
- 조건①의 `else` 분기 → 사실상 ①의 최종 귀결 상태

### 조건 ③ — mappingConnection → 강제 BLE 유지 (on)

```c
// systemControl.c:85-88
if (mappingConnection)
{
    BLE_OFF = false;  // ①②를 덮어씀
}
```

- **발동 조건**: 매핑 앱(PC/모바일)이 BLE로 연결된 상태
- **결과**: `BLE_OFF = false` — ①②가 true를 세웠더라도 강제 off 취소
- **주석**: "매핑이 연결되어 있으면 내부기 연결이 끊어지더라도 ble를 끄지 않는다"

### 조건 ④ — Mapping_BLE_Off → 일시 off

```c
// systemControl.c:91-94
if (Mapping_BLE_Off)
{
    BLE_OFF = true;  // ③을 덮어씀
}
```

- **발동 조건**: 매핑에서 "최초 내부기 이름 설정" 시 BLE를 잠시 끄는 신호
- **결과**: `BLE_OFF = true` — ③이 false를 세웠더라도 다시 true로
- **입력 추적**: `ble_communication.c:433-453` → `mappingState.BLE_Off` 또는 `remoteControlState.BLE_Off` 중 하나라도 true이면 `BLE_Off_Command = true`

### 조건 ⑤ — global_BLE_Off → 충전기 off (최종 오버라이드)

```c
// systemControl.c:97-100
if (global_BLE_Off)
{
    BLE_OFF = true;  // 모든 앞 조건 덮어씀
}
```

- **발동 조건**: 충전기 연결 시 (`systemStatus.BLE_Off = true`, systemControl.c:165)
- **결과**: `BLE_OFF = true` — 마지막 조건이므로 ①~④ 결과 무관하게 최종 결정
- **반대 경로**: 충전기 미연결 시 `systemStatus.BLE_Off = false` (systemControl.c:204) → global_BLE_Off가 false이므로 조건⑤ 미발동

### 최종 출력 (주석 처리된 死코드)

```c
// systemControl.c:102-109
if (BLE_OFF)
{
    // NRF_Off_Command();   ← 주석
}
else
{
    // NRF_On_Command();    ← 주석
}
```

**BLE_OFF 결정 순서 요약 (뒤가 앞을 덮어씀)**:

```
① ISD 끊김 0.6초 → true
② ISD 미연결 초기 → true
③ mappingConnection → false (①② 취소)
④ Mapping_BLE_Off → true (③ 취소)
⑤ global_BLE_Off → true (③ 취소)
─────────────────────────────────
출력: NRF_Off/On_Command() 주석 → 실제 GPIO 없음
```

---

## 4. 입력 변수 세팅 위치 추적

### `global_BLE_Off` = `systemState.BLE_Off`

| 값 | 세팅 위치 | 조건 |
|---|---|---|
| `true` | systemControl.c:165 | `chargerConnectorPluggedIn == df_Connected` |
| `false` | systemControl.c:204 | `chargerConnectorPluggedIn == df_Disconnected` |

### `Mapping_BLE_Off` = `BLE_communicationState.BLE_Off_Command`

ble_communication.c:433-453:
- `mappingState.BLE_Off == true` → `BLE_Off_Command = true` (line 435)
- `mappingState.BLE_Off == false` → `BLE_Off_Command = false` (line 439)
- 이후 `remoteControlState.BLE_Off == true` → `BLE_Off_Command = true`로 덮어씀 (line 452)

### `mappingConnection` = `BLE_communicationState.mappingConnection`

ble_communication.c:456: `ble_communication_state.mappingConnection = mappingState.mappingConnection`

---

## 5. "死코드라서 현재 실효 없음"의 함의

1. **이중 실효 없음**: NRF_On_OFF()는 main.c:660에서 매 사이클 호출되나, 내부 출력 함수 본문이 주석처리(systemControl.c:37-45). GPIO 조작 없음.
2. **nRF 칩 부재**: QCC로 전환된 시점에서 nRF 물리 칩도 부재(추정)이므로, 설령 주석이 해제되더라도 무효.
3. **로직 자체는 살아있음**: `ISD_ConnectionHistory`, `deaylCounter` 갱신, `BLE_OFF` 플래그 판단은 런타임에 계속 수행됨 — 단지 결과가 아무 데도 안 쓰임.
4. **미래 QCC 대응 참조 값**: 이 로직이 QCC 전환 후 어떻게 대응되어야 하는지의 **명세** 역할은 할 수 있음.

---

## 6. QCC 경로와의 5개 조건 매핑

| 조건 | NRF 로직 | QCC 대응 경로 | 커버 상태 |
|---|---|---|---|
| ① ISD 끊김 0.6초 → off | systemControl.c:64-70 | `snd_qcc_set_isd(DISCONNECTED)` 호출됨(isd_interface.c:253/266) — QCC가 핀 신호로 자율 판단할 것으로 **추정** | **partial** (핀 전달됨, QCC 내부 로직 미확인) |
| ② ISD 미연결 초기 → off | systemControl.c:79-82 | 초기화 시 `snd_qcc_set_isd(DISCONNECTED)` (snd_qcc.c:17) | **partial** (초기화만, 동적 재판단 경로 없음) |
| ③ mappingConnection → 강제 유지 | systemControl.c:85-88 | QCC에 mappingConnection 전달 경로 없음 | **uncovered** |
| ④ Mapping_BLE_Off → 일시 off | systemControl.c:91-94 | QCC에 해당 명령 경로 없음 | **uncovered** |
| ⑤ global_BLE_Off(충전기) → off | systemControl.c:97-100 | 충전기 연결 시 HW 리셋 → `snd_qcc_init()` → SHUTDOWN(snd_qcc.c:18). main.c:800은 주석처리 | **partial** (HW 리셋 경로만, 소프트 off 명령 없음) |

**아키텍처 전환의 의미**: nRF 시대는 CM3이 BLE on/off를 핀으로 직접 명령했다. QCC 시대는 CM3이 ISD_CHECK 핀으로 ISD 연결 상태만 알리고, QCC 내부 펌웨어가 BLE 광고/연결 여부를 자율 결정하는 구조로 **의사결정 주체가 위임**된 것으로 추정된다(QCC 내부 펌웨어 미확인이므로 단정 불가).

`set_mode(SHUTDOWN)`은 NRF의 "BLE 광고만 off"와 달리 QCC 칩 전원 자체를 끄는 더 강한 의미로 추정되며, 절전·OTA 후처럼 **완전 종료** 맥락에서만 사용된다.

---

## 7. 결론

- NRF_On_OFF()는 **완전한 死함수** — 로직은 실행되지만 출력이 없다
- QCC 전환은 조건 ①~⑤ 중 ③④를 명시적으로 커버하지 않는다 → **기능 범위 축소** 또는 **QCC 내부 위임** 중 하나 (QCC 칩 펌웨어 명세 없이 판정 불가)
- `set_mode(SHUTDOWN)`은 BLE 끄기가 아닌 **칩 셧다운** 으로, NRF off와 의미가 다름
- mappingConnection 유지 조건(③)과 Mapping_BLE_Off 일시 off(④)는 QCC 경로에서 **명시적 대응 없음** — 설계 gap으로 기록 권고
