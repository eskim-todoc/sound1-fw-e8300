---
name: systemControl FSM 분해 - 분석 부록 (Sullivan 유산)
purpose: systemControl.c:367 dead 확정 근거 및 Sullivan GPIO 유산 체인 제거 범위 확정
type: tasks/분석
applies_to: [Sound1]
tags: [systemControl, sullivan, dead-code, gpio, charger, carrying-case, analysis]
---

# 분석 부록 — Sullivan 유산 체인

**TL;DR**: 은수님 제보(Sullivan = USB 케이블 + 별도 캐링케이스)로 `systemControl.c:367` 의 정체가 밝혀졌다. **의도는 방어 코드가 맞으나 Sound1 에서는 도달 불가 = dead.** 근거: `snd_charger_set_state()` 가 `charger`/`carryingCase` 를 항상 **동시 설정**하고, 독립 GPIO 경로(`readUsbConnectorState()`)는 **호출 0** 으로 죽었다. 파생 조사로 **dead 체인 189줄**(활성 GPIO 3함수 + 비활성 FPGA 8심볼 + `readUsbConnectorState()`)을 확정했다.

---

## 1. 은수님 제보 (2026-07-15)

> 이게 과거에는 sullivan이라는 제품이었는데, 그 때는 USB 케이블로 충전이 가능했어. 그리고 충전포트 (캐링케이스가)가 별도로 있었어. 그래서 충전케이블만 연결하면 `chargerConnectorPluggedIn`만, 캐링케이스 사용시에는 `chargerConnectorPluggedIn` + `carryingCasePluggedIn`을 다 확인 해야했어. 그리고 `carryingCaseCoverOpen`으로 캐링케이스 뚜껑이 열렸나 닫혔나를 보는 거였고. (...) 지금은 충전케이블 연결이 안되고 포고 핀으로 크래들을 통해서만 충전되는 구조로 변경되었어.

**은수님 가설**: `:367` 은 "이전 `carryingCase` 가 `Disconnected` 인데 지금 바뀌었다 = 논리적으로 말이 안 되는 상황" 이라 시스템 슬립으로 도피시킨 방어 코드.

---

## 2. 검증 결과 — 가설은 정확하나 Sound1 에서 무효

### 2.1 Sullivan 구조의 화석 (코드 증거)

`cfx_cm3_sharedMemory.c:73~86` 의 `readUsbConnectorState()` 가 **독립 GPIO 2개 + 홀센서**를 읽는다:

| 라인 | 코드 | 의미 |
|---|---|---|
| `:73` | `Sys_GPIO_Read(DIO_PIN_INDEX_for_ChargerConnectorPluggedIn)` | USB 케이블 |
| `:74` | `Sys_GPIO_Read(DIO_PIN_INDEX_for_CarryingCasePluggedIn)` | 캐링케이스 |
| `:86` | `Sys_GPIO_Read(DIO_PIN_INDEX_for_CarryingCaseCoverOpen)` | 홀센서 (뚜껑) |
| `:83~85` | `"커버가 열리면 자석이 홀센서에서 멀어지면서..."` | 주석까지 Sullivan |

**은수님 설명과 정확히 일치.** 이 구조에서 `:367` 참 시나리오:

- `charger == df_Disconnected` (USB 미연결)
- `prev_carryingCase == df_Disconnected` → 현재 연결됨
- 캐링케이스는 충전 기능이 있으므로 `charger` 도 Connected 여야 하는데 아님 → **모순 상태** → 슬립

**Sullivan 에서는 도달 가능했고 방어로 기능했다.**

### 2.2 Sound1 에서 무효화된 이유 (3단 증명)

#### 증명_1 — 두 필드가 항상 같은 값

`batteryNPowerControl.c:74~99` `snd_charger_set_state()` switch:

| QCC 상태 | `chargerConnectorPluggedIn` | `carryingCasePluggedIn` |
|---|---|---|
| `CONNECTED` (`:78~79`) | `df_Connected` | `df_Connected` |
| `DISCONNECTED` (`:86~87`) | `df_Disconnected` | `df_Disconnected` |
| `RESET` (`:94~95`) | `df_Defalut` | `df_Defalut` |

크래들 하나가 두 신호를 겸하므로 **세 케이스 전부 동시 설정**.

#### 증명_2 — 독립 GPIO 경로는 죽었다

`readUsbConnectorState()` 호출처 전수:

| 호출처 | 상태 |
|---|---|
| `main.c:612` | **주석** (`// readUsbConnectorState() 대체`) — QCC 방식으로 전환됨 |
| `batteryNPowerControl.c:297` (`read_BatteryChargerConnectinStatus` 내부) | 그 함수 **호출 0** |
| `batteryNPowerControl.c:305` (`isCarryingCaseConnected` 내부) | 그 함수 **호출 0** |
| `batteryNPowerControl.c:327` (`isCarryingCaseCoverOpen` 내부) | 그 함수 **호출 0** |

→ `readUsbConnectorState()` **실행 0**. GPIO 가 `cfx_cm3_sharedMemoryAll.chargerState` 를 덮어쓰는 일이 없다.

`snd_charger_get_state()` 는 `return cfx_cm3_sharedMemoryAll.chargerState;` (`batteryNPowerControl.c:47~50`) — `snd_charger_set_state()` 가 쓰는 바로 그 구조체다.

#### 증명_3 — `:367` 은 논리적 모순

```
:209 진입      → charger == df_Disconnected
증명_1 에 의해 → 현재 carryingCase == df_Disconnected
:365 else 진입 → prev != 현재  →  prev != df_Disconnected
:367 조건      → prev == df_Disconnected   ← 위와 직접 모순
```

**`:367` 은 항상 거짓. `:365~373` else 블록 전체가 실질 no-op.**

> [!NOTE]
> **`:367` dead 는 "방어가 뚫린 것"이 아니라 "방어가 필요 없어진 것"이다.** Sullivan 은 두 신호가 독립이라 모순이 생길 수 있었지만, Sound1 은 하나의 크래들이 두 이름으로 흐르므로 모순 자체가 성립 불가다. 제거해도 안전하다.

---

## 3. 반증 시도 (adversarial)

| 반증 가설 | 검증 | 결과 |
|---|---|---|
| 가설_2: `carryingCase` 를 독립 설정하는 다른 경로가 있다 | `carryingCasePluggedIn` 쓰기 전수 grep | **반증** — `batteryNPowerControl.c:79/87/95` 뿐, 전부 동시 설정 |
| 가설_3: `snd_charger_get_state()` 가 다른 소스를 본다 | 함수 본문 확인 | **반증** — `return cfx_cm3_sharedMemoryAll.chargerState;` 동일 소스 |
| 가설_4: GPIO 경로가 다른 프로젝트(1__cfx/3__hear)에서 호출된다 | `src` 전체 census | **반증** — 4개 심볼 모두 `2__cm3` 내부만 |
| 가설_5: `#else` 벌(FPGA)이 실제로 컴파일된다 | `processorDirective.h:9~12` | **반증** — `Board_is_OTE_VER_1_5` 만 활성(`:10`), 나머지 3개 주석 처리 |
| 가설_6: 제거 대상이 헤더 경유로 외부에서 쓰인다 | `updateCarryingCaseStatus` / `updateBatteryChargerConnection` / `ST__CARRINGCASE_STATE` census | **반증** — `batteryNPowerControl.c` 밖 사용 0 |

**전 가설 반증. dead 판정 확정.**

---

## 4. 제거 범위 확정

### 4.1 전처리기 가드 경계 (실측)

```
#if 291  (Board_is_TD_DEV_ver_1_4 || OTE_VER_1_3 || OTE_VER_1_4 || OTE_VER_1_5)
   활성 292~344   ← Board_is_OTE_VER_1_5 정의됨 → 컴파일됨
#else 345
   비활성 346~427 ← FPGA I2C 기반 구현, 컴파일 안 됨
#endif 428
```

### 4.2 제거 대상

| 파일 | 범위 | 내용 | 근거 |
|---|---|---|---|
| `batteryNPowerControl.c` | `:291~428` (138줄) | 가드 블록 통째 | 활성 3함수 호출 0 + 비활성 8심볼 컴파일 안 됨 |
| `batteryNPowerControl.h` | `:20~24` | `ST__CARRINGCASE_STATE` | 비활성 구간(`:374`)만 사용 |
| `batteryNPowerControl.h` | 선언 4개 | `read_BatteryChargerConnectinStatus` / `updateCarryingCaseStatus` / `isCarryingCaseConnected` / `isCarryingCaseCoverOpen` | 정의가 사라지므로 |
| `cfx_cm3_sharedMemory.c` | `:66~116` (51줄) | `readUsbConnectorState()` | 호출 0 (증명_2) |
| `cfx_cm3_sharedMemory.h` | `:257` | `readUsbConnectorState()` 선언 | 정의가 사라지므로 |
| `systemControl.c` | `:365~373` | 빈 else 블록 (범위_1) | `:367` 도달 불가 (증명_3) |

**합계 약 189줄 + 선언 5개.**

### 4.3 유지 (제거하면 안 되는 것)

| 대상 | 유지 사유 |
|---|---|
| `DIO_PIN_INDEX_for_*` (`Board_OTE_ver1_5.h:71~73`, `:121~123`) | **`ci_dio.c:109/112/115` 가 저전력 모드 핀 설정에 사용.** 제거 시 빌드 깨짐. HW 레지스터 설정은 루트 지침 §7 5게이트 대상이라 별도 작업 |
| `ST__USB_CONNECTOR` (`cfx_cm3_sharedMemory.h:181~185`) | `snd_charger_set_state()` / `snd_charger_get_state()` / `systemControl()` 이 사용 |
| `df_Opened` / `df_Closed` (`processorDirective.h:186~187`) | `df_Connected`/`df_Disconnected` 의 **의미 별칭**(1/2 동일값). `readUsbConnectorState()` 제거 시 사용처 0 이 되나, `batteryNPowerControl.c:106` 주석(`/* df_Closed == 2 */`)이 의미를 참조. 매크로라 코드 크기 영향 0 → **유지 권장** (제거 시 주석 수정 파급) |

---

## 5. 잔여 리스크

| 리스크 | 평가 |
|---|---|
| `ci_dio.c` 가 설정하는 핀을 아무도 읽지 않게 됨 | **이미 그런 상태**(`readUsbConnectorState()` 호출 0). 이번 제거로 새로 생기는 문제 아님. 저전력 핀 설정은 전기적 목적(누설 방지)일 수 있어 건드리지 않음 |
| `#else` 벌 제거로 구 보드(OTE_VER_1_3/1_4, TD_DEV_1_4) 지원 상실 | **아님** — `#else` 는 그 보드들이 **정의되지 않았을 때** 컴파일된다. `:291` 가드는 4개 보드 중 하나라도 정의되면 활성 구간을 쓰므로, `#else` 는 **어떤 보드 define 도 없을 때**만 유효. 현재 프로젝트에 그런 구성은 없다 |
| `df_Opened`/`df_Closed` 사용처 0 → 컴파일러 경고 | 매크로는 미사용 경고 대상이 아님 (함수/변수와 달리) |
