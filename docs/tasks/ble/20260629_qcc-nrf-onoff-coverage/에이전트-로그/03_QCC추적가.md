---
name: QCC 호출처 추적가 — snd_qcc set_isd/set_mode 전수 분석
purpose: 각 호출처의 시스템 상태 컨텍스트를 추적해 NRF_On_OFF 커버리지 판정에 기여
type: tasks
maturity: experimental
tags: [ble, qcc, nrf, call-trace, coverage]
---

# QCC 호출처 추적가 분석

**TL;DR**: `snd_qcc_set_isd`의 실질 호출처는 `isd_interface.c:248/253/261/266` 4곳(main.c:508은 `#else` 死코드). `snd_qcc_set_mode`는 부팅 NORMAL 1곳 + SHUTDOWN 3곳(절전·크래들 뚜껑 열림·OTA 리셋) + 주석 1곳. GPIO 출력만 수행하며 결정 로직은 상위 호출자에 있다.

---

## 1. snd_qcc_set_isd 호출처 전수

### 1-1. main.c:508 — **컴파일 제외 (死코드)**

```c
// main.c:491~509
#if 1  // 기본 코드  ← 항상 true
    isd_state = isd_interface(...);  // 실제 실행 경로
#else  // BLE 강제로 동작시키기 위한 코드
    isd_state.conneded_ISD     = true;
    isd_state.isd_controlState = en__isdStatus_stimul_10V_Ok;
    snd_qcc_set_isd(SND_QCC_ISD_CONNECTED);  // ← 절대 실행 안 됨
#endif
```

- **판정**: `#if 1 … #else` 구조상 `#else` 블록은 전처리기가 완전 제거. 빌드에 포함되지 않는 디버깅 잔재.

---

### 1-2. isd_interface.c:243~268 — 4개 실제 호출

함수 시그니처: `ST__ISD_STATUS isd_interface(bool isd_enable, bool mappingConnection, EN__ISD_CONTROL_STATE isdControlCommand)` (line 162)

`stimulationParameterSettingIsDone`은 함수 진입 시 `false`로 초기화(line 164), `stimulationStandAlone()` 반환값으로만 갱신(line 228, 조건: `isd_controlState == en__isdStatus_stimul_10V_Ok && !mappingConnection`).

| 라인 | 분기 조건 | set_isd 인자 | 시스템 상태 의미 |
|------|-----------|--------------|-----------------|
| :248 | `mappingConnection == true` AND `isd_controlState >= en__isdStatus_stimul_10V_Ok` | CONNECTED | 매핑 앱 연결 + ISD 10V 이상 → QCC에 ISD 연결 알림 |
| :253 | `mappingConnection == true` AND `isd_controlState < en__isdStatus_stimul_10V_Ok` | DISCONNECTED | 매핑 앱 연결이지만 ISD 10V 미달(진행 중 or 미연결) |
| :261 | `mappingConnection == false` AND `stimulationParameterSettingIsDone == true` | CONNECTED | 일반 동작 중 standalone 자극 파라미터 설정 완료 |
| :266 | `mappingConnection == false` AND `stimulationParameterSettingIsDone == false` | DISCONNECTED | 일반 동작 중 파라미터 설정 미완료 (초기 상태 포함) |

> **핵심**: set_isd는 매 cycle `isd_interface()` 호출 시마다 재평가된다(main.c:492, 메인루프 매 cycle). 0.6초 지연 로직은 없다.

---

## 2. snd_qcc_set_mode 호출처 전수

| 파일:라인 | 인자 | 트리거 조건 | 상태 컨텍스트 |
|-----------|------|-------------|---------------|
| `initialize.c:511` | NORMAL | 부팅 초기화 완료 (SPI 초기화 직후) | 정상 부팅 시 QCC 기동. 이후 QCC SPI 통신 가능 상태로 전환 |
| `main.c:800` | SHUTDOWN | ~~크래들 뚜껑 닫힘~~ | **주석 처리** — 크래들 약절전 진입 시 QCC도 끄려던 의도였으나 비활성화. 코드 주석: "충전기 연결 시 QCC는 절전 미진입, SPI 패킷 수신 유지" |
| `main.c:829` | SHUTDOWN | 크래들 뚜껑 열림 패킷 수신 후 | `func_cradle_lid_closed_loop()` 내 `tdc_cradle_get_cover_state() == df_Connected` 조건. 직후 20ms 딜레이 → `SYS_WATCHDOG_RESET()` (재부팅) |
| `main.c:871` | SHUTDOWN | 절전 모드 진입 시퀀스 | `func_sleep()` 내. FPGA 리셋 → NRF_Off_Command → **QCC SHUTDOWN** → FPGA SLEEP → PMIC off → LED off 순서 |
| `ci_ble_control_boot.c:117` | SHUTDOWN | OTA 파일 쓰기 완료 후 | 응답 패킷 송신 후. 코드 주석: "2초 이상 QCC_CTRL=0 유지해야 QCC가 꺼진다." 2200ms 딜레이 → `SYS_WATCHDOG_RESET()` |

### func_cradle_lid_closed_loop 진입 조건 (main.c:736)
```c
if (systemState.cradleLidClosed && !isd_state.conneded_ISD)
```
ISD 연결 중이면 진입 차단. 약절전 루프 안에서는 QCC를 끄지 않고(main.c:800 주석) BLE SPI 수신 유지.

### func_sleep 진입 경로 (main.c:308~311)
```c
while (1) {
    func_normal();
    func_sleep();  // func_normal() 반환 시 항상 실행
}
```
`func_normal()`이 `systemState.systemOff == true`로 반환할 때 진입.

---

## 3. QCC 핀 동작 요약 (snd_qcc.c 직독)

| 함수 | 핀 | High | Low |
|------|----|------|-----|
| `snd_qcc_set_isd(CONNECTED)` | DIO13 (ISD_CHECK) | High | — |
| `snd_qcc_set_isd(DISCONNECTED)` | DIO13 (ISD_CHECK) | — | Low |
| `snd_qcc_set_mode(NORMAL)` | DIO24 (QCC_CTRL) | High | — |
| `snd_qcc_set_mode(SHUTDOWN)` | DIO24 (QCC_CTRL) | — | Low |

함수 내부에는 상태 판단 로직 없음. GPIO 셋/클리어만 수행.

---

## 4. NRF_On_OFF 5개 조건 vs QCC 커버리지 매핑

| NRF 조건 | NRF 동작 | QCC 대응 경로 | 커버 여부 |
|----------|---------|--------------|----------|
| ① ISD 연결 이력 후 끊김 0.6초 지연 → BLE off | NRF_Off_Command (死코드) | `set_isd(DISCONNECTED)` — 즉시(지연 없음) | **partial**: 기능은 대응하나 0.6초 지연 로직 없음 |
| ② ISD 미연결 초기 → BLE off | NRF_Off_Command (死코드) | `set_isd(DISCONNECTED)` — 매 cycle 재평가 | **covered**: isd_interface.c:266 |
| ③ mappingConnection → BLE on 강제 유지 | NRF_On_Command (死코드) | `set_isd(CONNECTED)` — isd_interface.c:248 조건 충족 시 | **covered** (ISD 10V 조건 추가) |
| ④ Mapping_BLE_Off → 일시 BLE off | NRF_Off_Command (死코드) | QCC 측 직접 대응 없음 | **uncovered**: `BLE_Off_Command` 변수가 QCC 제어로 연결되지 않음 |
| ⑤ global_BLE_Off(충전기 연결) → BLE off | NRF_Off_Command (死코드) | main.c:800 주석 처리로 비활성화 | **uncovered**: 충전기 연결 시 QCC는 약절전 루프에서도 동작 유지 |

> **주목**: 조건 ④⑤ 미커버는 설계 의도일 수 있음. QCC는 독립 BLE 칩으로 ISD 상태 힌트(ISD_CHECK 핀)만 수신하며, BLE on/off 결정 주체가 QCC 내부 펌웨어로 위임된 구조로 추정됨(QCC 내부 펌웨어는 단정 불가).

---

## 5. SHUTDOWN vs NRF "BLE 광고 off" 의미 비교

- **NRF_Off_Command**: BLE 광고(Advertising) 중단 명령 수준 (추정). 단, 본문 전부 주석 = 실제로는 아무 효과 없었음.
- **set_mode(SHUTDOWN)**: QCC_CTRL=Low → QCC 칩 전원/동작 자체 차단. ci_ble_control_boot.c 주석 "2초 이상 유지해야 꺼진다". 단순 광고 off가 아닌 **칩 레벨 셧다운**.
- **의미 차이**: NRF off = BLE soft-disable(추정) vs QCC SHUTDOWN = 하드웨어 레벨 전력 차단. 스펙트럼이 다름.
