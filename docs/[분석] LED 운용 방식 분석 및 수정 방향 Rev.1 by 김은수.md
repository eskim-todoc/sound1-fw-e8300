# 2__cm3 LED 운용 분석 및 수정 방향 (Rev.1)

작성자: 김은수
작성일: 2026-04-14
근거:
- `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.{c,h}`
- `src/2__cm3/Cortex-M3-src/systemControl/systemControl.c`
- `src/2__cm3/Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.{c,h}` (`readLED_indicatorOnOff`, `changeLED_indicatorOnOff`)
- `src/2__cm3/Cortex-M3-src/BleCommunication/remoteControl.c`
- `src/2__cm3/Cortex-M3-src/main.c`
- `docs/led_ind_state.JPG`

Rev.1 변경점: 시스템 동작 요구사항(전원 인가 → 에러 우선 → BLE led_ind → ISD 연결/배터리 단순화 → critical 배터리 오버라이드 + 사용자 비활성화)을 반영해 아키텍처/우선순위/상태 매핑 재설계.

---

## 1. 현재 구조 요약 (변경 없음)

### 1.1 레이어
```
systemControl.c  →  systemStatus.Led_Pattern (EN__LED_PATTERN)
       │
main.c (L514)
       │
LedPatternOut(pattern)   ── 패턴별 전용 함수 호출
       │                    (Led_powerOn, LED_Map_error,
       │                     sitimulationOutputOnLED_*, …)
       │                    → LED_outputColor 갱신
       │
LED_OUT()                ── PWM duty + GPIO 출력
                             (LED_IS_ACTIVELOW / LED_B_pin_CFX_test 분기)
```

### 1.2 핵심 문제점
- `systemState.Led_Pattern`을 **덮어쓰기만** 함 → 우선순위/선점 레이어 없음.
- 에러/OTA/배터리 critical 등 상황이 동시에 성립할 때, 마지막으로 쓰인 값이 표시됨.
- `tdc_led_ind_state_t`와 `en__LED_IND_BATT_STATE`, `en__LED_IND_PAIR_STATE`는 enum/API만 있고 **`LedPatternOut()` switch에서 사용되지 않음(dead path)**.
- 사용자 LED 비활성화 플래그는 `readLED_indicatorOnOff()`로 이미 존재(값 `2`가 Off)하지만 **일부 케이스에만 적용**됨 → 일관성 없음.

---

## 2. 요구사항 정리 (Rev.1 신규)

### 2.1 라이프사이클
1. **전원 인가 → 초기화**
   - 초기화 성공 → **Power On LED** 표시.
   - 초기화 중 이슈 발생 → 해당 **Error LED** 표시(Power On LED보다 우선).
2. **Power On 이후 정상 운용 구간**
   - 어떤 경로에서든 에러가 발생하면 **즉시 Error LED 표시**.
   - 단, **Power Off 표시** 중에는 Error LED가 Power Off를 가리지 못함(Power Off만 예외).
3. **Power Off 표시** 이후 종료.

### 2.2 BLE `led_ind` 우선 표시
- BLE 패킷 수신 결과 `led_ind`가 `NONE`이 아니면 **해당 led_ind를 표시**.
- 대상: `TDC_LED_IND_STATE_BATT`, `_PAIR`, `_OTA_QCC`, `_OTA_EZAIRO`.
- 에러/ Power Off보다는 아래, 일반 운용 상태(사용 중/사용 준비 등)보다는 위.

### 2.3 ISD 연결/배터리 표시 단순화
- 기존의 "ISD 연결 + 배터리 Normal/Low" 조합별 깜빡임/색상 차별화를 **폐기**.
- 내부기(ISD) 연결이 확인되면 **"사용 중 (내부기 연결됨)" 단일 상태**로 통합 → 녹색 지속 ON.
- 배터리 충전 중/방전 중 구분 없이 **배터리 레벨만으로** 판정:
  - 레벨 ≥ `TH_READY` → **사용 준비** (녹색 지속 ON)
  - `TH_CRITICAL` ≤ 레벨 < `TH_READY` → **배터리 완충 아님** (노랑 느린 점멸, 1100/1100)
  - 레벨 < `TH_CRITICAL` → **배터리 즉시 충전 필요** (노랑 빠른 점멸, 180/180)
- `TH_READY`, `TH_CRITICAL` 임계값은 스펙/제품정의에서 확정 필요(열린 이슈).

### 2.4 Critical 배터리 오버라이드
- **배터리 즉시 충전 필요**는 ISD 연결 상태(사용 중)여도 **반드시 표시**(사용 중 지속 ON을 선점).
- 단, **사용자가 LED 표시 기능을 비활성화** (`readLED_indicatorOnOff() == 2`) 한 경우 표시하지 않음.
- Error / Power Off / BLE led_ind 표시보다는 하위.

---

## 3. 상태/우선순위 설계

### 3.1 우선순위 테이블 (높을수록 먼저)
| prio | 상태 | 표기 | 비고 |
|---|---|---|---|
| 100 | POWER_OFF | 파랑 점멸(기존 `Led_powerOff`) | Error도 선점 못함 |
| 90 | ERROR (Map/MCU/Accel/FPGA/PMIC) | 빨강 180/180 | 초기화 이슈 & 운용 중 이슈 동일 |
| 80 | BLE led_ind = OTA_QCC | 녹색 1100/1100 | |
| 80 | BLE led_ind = OTA_EZAIRO | 녹색 1100/1100 | |
| 70 | BLE led_ind = PAIR | 파랑 180/180 (`**`) | |
| 70 | BLE led_ind = BATT | 노랑 계열(세부는 2.3 규칙) | BLE가 배터리 표시를 명령 |
| 60 | BATT_CRITICAL (<TH_CRITICAL) | 노랑 180/180 | ISD 연결 상태도 선점. 사용자 비활성화 시 억제 |
| 50 | POWER_ON (초기화 완료 announce) | 하늘색 단발 점멸(기존) | N회 후 자동 종료 → 그다음 상태로 전이 |
| 40 | IN_USE (ISD 연결됨) | 녹색 지속 ON | 기존의 ISD-연결 + 배터리 Normal/Low 조합을 이 하나로 통합 |
| 30 | READY (ISD 미연결 & 배터리 ≥ TH_READY) | 녹색 지속 ON | |
| 20 | BATT_MID (TH_CRITICAL ≤ 배터리 < TH_READY) | 노랑 1100/1100 | |
| 10 | IDLE | OFF | |
| 0  | NONE | — | |

규칙:
- 에러는 "Power On 이후" 구간에서 발생해도 **기존 상태를 선점**해서 표시.
- Power Off 표시가 시작되면 그 위의 에러 표시는 **Power Off 종료까지 가려짐**.
- Critical 배터리(prio 60)는 IN_USE(40)/READY(30)보다 위 → **ISD 연결 중에도 표시**. 단 사용자 LED 비활성화 시 prio 60을 `LED_PRIO_OFF`로 강등.
- BLE led_ind가 NONE이 아닌 동안에는 prio 70~80 유지.

### 3.2 상태 머신 (개념)
```
        +------------------+
(reset) |  INIT            |
  ──────►                  │── init OK ─► POWER_ON_INDICATE ── done ─► RUN
        │                  │
        │ init fail ─► ERROR (latched, 우선순위 항상 적용)
        +------------------+

RUN:  매 tick 마다 led_request(src, state) 집계 → arbiter가 최상위 prio 선택
        ├── error 발생 ─► prio 90 (Power Off 요청 전까지 유지)
        ├── power off 요청 ─► prio 100 (종료 시퀀스)
        └── 그 외: BLE led_ind / BATT_CRITICAL / IN_USE / READY / BATT_MID / IDLE
```

---

## 4. 상태 매핑 규칙

### 4.1 배터리 레벨 → 상태
```c
if (led_disabled_by_user && !is_error && !is_power_off)
    req = LED_NONE;  // critical도 포함해 억제
else if (battery_level < TH_CRITICAL)
    req = BATT_CRITICAL;   // prio 60, ISD 연결 여부 무관
else if (isd_connected)
    req = IN_USE;          // prio 40 (배터리 레벨 세분화 제거)
else if (battery_level >= TH_READY)
    req = READY;           // prio 30
else
    req = BATT_MID;        // prio 20
```
- `isd_connected`는 **충전 중/방전 중과 독립**. 충전 상태 자체로는 LED 색/패턴을 결정하지 않음 (스펙의 "충전 중 빨강 1100/1100"은 **미채택**, 열린 이슈 5.1 참조).
- 사용자 비활성화 플래그는 **에러/Power Off는 예외로 두고** 나머지 운용 상태만 억제 (안전성 관련 표시는 끄지 않도록 하는 것이 합리적 → 열린 이슈 5.2).

### 4.2 BLE led_ind 매핑
| `tdc_led_ind_state_t` | 표시 |
|---|---|
| NONE | 요청 없음 |
| BATT | 4.1 규칙으로 프록시(BATT_CRITICAL/MID 혹은 OFF) |
| PAIR | 파랑 180/180 |
| OTA_QCC | 녹색 1100/1100 |
| OTA_EZAIRO | 녹색 1100/1100 |

### 4.3 에러 소스
에러 소스(`error.c` 등)가 **래치**되면 운용 도중 해소 전까지 Error LED를 유지. 복구 후에만 prio 90 해제.

---

## 5. 현재 코드와의 갭 / 수정 포인트

### 5.1 제거 / 통합 대상
- `en__LED_ISD_StimulationOut_batteryNormal`, `…_batteryLow`
- `en__LED_StandbyForconneded_ISD_batteryNormal`, `…_batteryLow`
- `en__LED_MappingConneted_ISD_Connected_BatteryNormal/Low`
- `en__LED_MappingConneted_ISD_Unconnected_BatteryNormal/Low`
- `en__LED_BatteryChargingLevel_0per` ~ `100per`
→ **모두 `IN_USE` / `READY` / `BATT_MID` / `BATT_CRITICAL` 네 상태로 축약**.

### 5.2 추가/연결 대상
- `en__LED_IND_BATT_STATE`, `en__LED_IND_PAIR_STATE` (현재 미연결) → BLE led_ind 경로에 연결.
- OTA_QCC / OTA_EZAIRO 패턴 신규 구현.
- Error 통합 패턴(180/180, 빨강) — 현재 Map/MCU/Accel/FPGA/PMIC 각각 타이밍이 다름 → 모두 180/180으로 통일(세부 에러 식별이 필요하면 별도 로그/이벤트 채널로 분리).

### 5.3 타이밍 정합
| 상태 | 스펙 | 현재 | 조치 |
|---|---|---|---|
| ERROR | 180/180 (2.8Hz) | 100/400, 500/500 등 제각각 | **180/180 통일** |
| BATT_CRITICAL | 180/180 노랑 | 없음 | **신규** |
| BATT_MID | 1100/1100 노랑 | 없음 | **신규** |
| READY (80%↑ & ISD 미연결) | 녹색 지속 | `standby_isdNotConnectedLED_batteryNormal` = GREEN 지속 | 유지 |
| IN_USE (ISD 연결) | 녹색 지속 | 현재는 200/800 점멸 | **지속 ON으로 변경** |
| PAIR | 180/180 파랑 | 200/800 파랑 | **180/180으로 변경** |
| OTA_QCC / OTA_EZAIRO | 1100/1100 녹색 | 없음 | **신규** |
| POWER_ON / OFF | 기존 유지 | 기존 | 유지 |

---

## 6. 아키텍처 제안

### 6.1 레이어
```
 [각 서브시스템]                [Arbiter]                [Pattern Engine]
 error.c        ─┐
 init flow      ─┤
 battery.c      ─┤── led_request(src, state, meta) ─► prio 테이블 ─► LedPatternOut(pattern)
 ble_comm.c     ─┤
 ci_ota.c       ─┤
 systemControl  ─┘
                                (user LED off flag 반영:
                                 ERROR/POWER_OFF는 예외, 나머지 억제)
```

### 6.2 API 초안
```c
typedef enum {
    LED_SRC_INIT,        // 초기화/Power On Announce
    LED_SRC_ERROR,       // 에러 래치
    LED_SRC_POWER_OFF,   // 종료 시퀀스
    LED_SRC_BLE_IND,     // BLE 수신 led_ind
    LED_SRC_BATTERY,     // 배터리 레벨 기반
    LED_SRC_ISD,         // ISD 연결 여부
    LED_SRC_N
} led_src_t;

typedef enum {
    LED_ST_NONE = 0,
    LED_ST_POWER_ON,
    LED_ST_POWER_OFF,
    LED_ST_ERROR_MAP,
    LED_ST_ERROR_MCU,
    LED_ST_ERROR_ACCEL,
    LED_ST_ERROR_FPGA,
    LED_ST_ERROR_PMIC,
    LED_ST_OTA_QCC,
    LED_ST_OTA_EZAIRO,
    LED_ST_PAIR,
    LED_ST_BATT_CRITICAL,
    LED_ST_BATT_MID,
    LED_ST_READY,
    LED_ST_IN_USE,
    LED_ST_IDLE,
} led_state_t;

void     led_request(led_src_t src, led_state_t st);
void     led_arbiter_tick(void);   // 매 1ms 혹은 10ms 주기로 호출
```

### 6.3 패턴 엔진 일반화
```c
typedef struct {
    EN__LED_COLOR color;
    uint16_t      on_ms;       // 0 = 지속 ON 아님 체크
    uint16_t      period_ms;   // 0 = 지속 ON (on_ms 무시)
    uint8_t       burst_cnt;   // 0 = 무한, >0 = N회 후 자동 종료(POWER_ON용)
} led_pattern_desc_t;

// 스펙 표를 그대로 내려쓴 const 테이블
static const led_pattern_desc_t k_led_patterns[] = {
    [LED_ST_ERROR_MAP]     = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_MCU]     = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_ACCEL]   = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_FPGA]    = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_PMIC]    = { en__LED_RED,    180, 360, 0 },
    [LED_ST_OTA_QCC]       = { en__LED_GREEN, 1100,2200, 0 },
    [LED_ST_OTA_EZAIRO]    = { en__LED_GREEN, 1100,2200, 0 },
    [LED_ST_PAIR]          = { en__LED_BLUE,   180, 360, 0 },
    [LED_ST_BATT_CRITICAL] = { en__LED_ORANGE, 180, 360, 0 },
    [LED_ST_BATT_MID]      = { en__LED_ORANGE,1100,2200, 0 },
    [LED_ST_READY]         = { en__LED_GREEN,  0,   0,   0 },
    [LED_ST_IN_USE]        = { en__LED_GREEN,  0,   0,   0 },
    [LED_ST_IDLE]          = { en__LED_BLACK,  0,   0,   0 },
    // POWER_ON/OFF는 기존 전용 함수 유지(버스트 시퀀스 특성상 별도 관리)
};
```
- 에러 5종은 색/패턴이 동일하지만 **래치/복구 플래그 관리는 소스별**. 다른 진단 채널에서 구분하도록 함.

### 6.4 Arbiter 의사코드
```c
void led_arbiter_tick(void) {
    led_state_t chosen = LED_ST_IDLE;
    int         max_prio = 0;
    bool        user_off = (readLED_indicatorOnOff() == 2);

    for (each src in LED_SRC_N) {
        led_state_t st = req_table[src];
        int p = prio_of(st);

        // 사용자 LED off 시 에러/POWER_OFF 외에는 전부 억제
        if (user_off && !is_error(st) && st != LED_ST_POWER_OFF) continue;

        if (p > max_prio) { max_prio = p; chosen = st; }
    }
    LedPatternOut(map_state_to_enum(chosen));
}
```

### 6.5 마이그레이션 단계
1. **Arbiter + 패턴 엔진 도입**. 기존 `LedPatternOut()`는 내부에서 새 엔진을 호출하도록 래핑 (회귀 차단).
2. `systemControl.c`의 `systemStatus.Led_Pattern = X` 직접 대입을 `led_request(src, st)`로 교체.
3. 에러 소스에서 `led_request(LED_SRC_ERROR, ...)` 호출 라인 삽입 (`error.c`, 초기화 루틴).
4. BLE 수신 경로에서 `led_request(LED_SRC_BLE_IND, ...)` 호출.
5. 배터리 판정 1곳(`batteryNPowerControl.c`)에서 4.1 규칙으로 `led_request(LED_SRC_BATTERY, ...)`.
6. ISD 연결/미연결 판정으로 `led_request(LED_SRC_ISD, IN_USE | READY)`.
7. 사용하지 않게 된 `en__LED_ISD_*`, `Mapping*`, `BatteryChargingLevel_*` 제거.

### 6.6 검증
- 단위(`tests/`): 패턴 디스크립터별 on/period 오차 ±1ms 이내 (tick 시뮬).
- 시나리오:
  - 초기화 OK → Power On 시퀀스 N회 후 READY/IN_USE 전이.
  - 초기화 실패 → Error LED 즉시 표시, Power On 시퀀스 미실행.
  - 운용 중 에러 주입 → 즉시 Error 선점. Power Off 요청 시 Power Off가 Error 선점.
  - BLE `led_ind`=OTA_QCC 주입 → 녹색 1100/1100.
  - ISD 연결 상태 + 배터리 < TH_CRITICAL → BATT_CRITICAL 표시. 사용자 LED off 주입 시 OFF.
  - 배터리 < TH_CRITICAL + 에러 동시 → Error 유지(상위 prio).
- HW: 오실로스코프로 180/180, 1100/1100 파형 측정, 공차 ±5% 이내.

---

## 7. 열린 이슈 (결정 필요)

1. **충전 중 표시**: 스펙 이미지의 "충전 중 빨강 느린 점멸"을 채택할지, 본 Rev.1처럼 **충전 여부와 무관하게 배터리 레벨 기준** 매핑으로 통일할지.
   - 현 제안: 통일(요구사항 §2.3 반영). 채택하려면 Arbiter에 `CHARGING` prio 별도 도입.
2. **사용자 LED 비활성화의 범위**: Critical 배터리까지 억제해도 안전상 문제 없는지 확인(요구사항 §2.4는 "표시되지 않아야 함"). 에러/Power Off도 억제 대상인지 확정 필요.
3. `TH_READY`, `TH_CRITICAL` 임계치 수치(예: 80%, 10%)와 히스테리시스 폭.
4. Power On Announce 중 **에러 발생 시 즉시 전환** 정책 (요구사항상 에러 우선이므로 즉시 전환이 합리적).
5. BLE `led_ind=BATT`가 들어올 때, 본 펌웨어의 배터리 판정값과 충돌하면 **어느 쪽을 우선**할지 (현 제안: BLE led_ind prio 70 > 본체 배터리 판정 prio 20/60 ⇒ BLE 우선).
6. BLE 인증(PAIR) 180ms 미만인 짧은 이벤트의 **최소 표시 시간 latch** 도입 여부 (스펙 `**` 주석 참조).
7. `LED_IS_ACTIVELOW` / `LED_B_pin_CFX_test` 매크로의 Board_OTE_ver1_5 정합성.

---

## 8. 요약

- 요구사항 §2.1~§2.4를 반영하면 LED 상태는 **에러(POWER_OFF 예외) > BLE led_ind > 배터리 critical > Power On → IN_USE/READY/BATT_MID/IDLE** 순의 우선순위로 단순화된다.
- ISD 연결 여부와 배터리 레벨을 조합하던 기존 다중 상태(Stimulation/Standby/Mapping × Normal/Low, 충전 0~100% 7단)는 **4상태(IN_USE / READY / BATT_MID / BATT_CRITICAL)로 축약**.
- Critical 배터리는 IN_USE를 선점하되 **사용자 LED off 플래그로 억제**.
- 구조는 **소스별 요청 → Arbiter → 패턴 엔진(디스크립터 테이블)** 의 2단 분리로 재작성하며, 기존 `LedPatternOut()`는 래퍼로 유지해 단계적 교체.
