# 2__cm3 LED 운용 분석 및 수정 방향 (Rev.4, 구현 착수본)

작성자: 김은수
작성일: 2026-04-15
상태: **구현 착수 확정본**. 열린 이슈는 자체 검증 후 모두 결정으로 반영.

Rev.4 변경점:
- Rev.3 오류 정정: `LED_ST_IN_USE` 색상 `en__LED_GREEN` → `en__LED_WHITE` (스펙 `led_ind_state.JPG`의 "사용 중(내부기 연결됨, 착용 상태)" 흰색 지속과 정합).
- 우선순위 표 IN_USE 항목 "녹색 지속" → "흰색 지속".

근거 코드/문서:
- `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.{c,h}`
- `src/2__cm3/Cortex-M3-src/systemControl/systemControl.c`
- `src/2__cm3/Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.{c,h}`
- `src/2__cm3/Cortex-M3-src/systemControl/batteryNPowerControl.{c,h}`
- `src/2__cm3/Cortex-M3-src/BleCommunication/remoteControl.c`
- `src/2__cm3/Cortex-M3-src/main.c`
- `src/2__cm3/99_includeBoard/Board_OTE_ver1_5.h`
- `docs/led_ind_state.JPG`

---

## 1. 요구사항 (누적 확정)

### 1.1 라이프사이클 / 에러
- 초기화 OK → Power On LED 표시, 버스트 완료 후 정상 운용 진입.
- 초기화 실패 → Power On 요청을 **아예 내지 않고** Error LED로 직행.
- 운용 중 에러 발생 → Error LED가 기존 표시를 즉시 선점.
- **Power Off 표시만 Error를 선점**.

### 1.2 BLE `led_ind`
- `led_ind`가 `NONE`이 아닐 때 해당 상태 표시.
- 단, `led_ind == BATT`는 별도 색/패턴을 갖지 않고 **본체 배터리 판정을 표시하라는 트리거**로만 해석. (실색/패턴은 본체 규칙이 결정)

### 1.3 ISD 연결 / 배터리 (충·방전 무관)
- ISD 연결 시 배터리 레벨별 LED 세분화 전부 폐기 → **IN_USE 단일**.
- 배터리 레벨 판정 (충/방전 무관):
  - `level ≥ 80%` → READY (녹색 지속)
  - `10% ≤ level < 80%` → BATT_MID (노랑 1100/1100)
  - `level < 10%` → BATT_CRITICAL (노랑 180/180)
- 히스테리시스 ±2% (BATT_CRITICAL 진입 <10%, 해제 ≥12%; READY 진입 ≥80%, 해제 <78%).

### 1.4 BATT_CRITICAL 오버라이드
- ISD 연결 중이라도 BATT_CRITICAL 표시 (IN_USE 선점).
- **사용자 LED 비활성화** (`readLED_indicatorOnOff() == 2`) 시 억제.

### 1.5 Mapping 연결 표시
- `BLE_communicationState.mappingConnection == true` 일 때:
  - ISD 미연결 → 파랑 300ms ON / 300ms OFF 점멸.
  - ISD 연결 → 파랑 지속 ON.
- BATT_CRITICAL / IN_USE / READY / BATT_MID를 선점.

### 1.6 Power On / Off 게이트
- `systemControl()`은 **Power On 버스트 완료**(`current_led_pattern != en__LED_POWER_On`) 후 정상 운용 로직 진입.
- `systemControl()`은 **Power Off 버스트 완료**(`current_led_pattern != en__LED_POWER_Off`) 후 저전력 모드 진입.
- 이 핸드셰이크(버스트 완료 시 엔진이 `updateLED_OutputPattern(en__LED_NA)` 자가 호출)는 **보존**.

---

## 2. 열린 이슈 자체 검증 결과 (확정)

| # | 이슈 | 검증 근거 | 결정 |
|---|---|---|---|
| 1 | 충전 중 표시 폐지 여부 | 사용자 요구 §1.3 명시. 스펙 이미지의 "충전 중 빨강 느린 점멸"은 충·방전 통합 규칙과 상충. | **폐지**. 충전 여부는 LED에 반영하지 않음. |
| 2 | 사용자 LED off 억제 범위 | 기존 코드 `LedOutput.c` L1258/1271/1287/1300에서 ISD Stimulation/Standby 계열만 부분 적용. 일관성 없음. 사용자 §1.4는 BATT_CRITICAL만 명시. | **ERROR / POWER_ON / POWER_OFF 예외, 나머지 전부 억제**. (안전·기기 생존 피드백만 유지) |
| 3 | TH_READY, TH_CRITICAL 수치 | 스펙 이미지: 사용 준비 = 80%↑, 즉시 충전 = <10%. `snd_batt_get_percent()`로 %값 직접 접근 가능. | **TH_READY=80%, TH_CRITICAL=10%, hysteresis ±2%**. |
| 4 | PAIR latch | 스펙 ** 주석 "인증 시간이 매우 짧아 LED 표시 불가능할 수 있음". 1주기(360ms) 확보 필요. | **최소 latch 500ms** (요청 해제 후에도 500ms 유지). |
| 5 | BLE `led_ind=BATT` 충돌 | BLE가 배터리 판정을 별도로 내린다고 가정하면 본체 안전 판정과 불일치 위험. | **별도 상태 제거**. `led_ind=BATT`는 본체 배터리 판정 표시를 트리거하는 신호로만 사용. (본체는 이미 상시 판정 중이므로 사실상 no-op.) |
| 6 | POWER_ON/OFF 버스트 색/시간 | 스펙 이미지에 Power On/Off 항목 없음. 기존값은 출하 제품에서 검증됨. | **기존값 유지**. POWER_ON: SKYBLUE 80ms/300ms × 5회. POWER_OFF: BLUE 100ms/300ms × 4회. |
| 7 | `LED_IS_ACTIVELOW` / `LED_B_pin_CFX_test` 정합성 | `Board_OTE_ver1_5.h` L128 `#define LED_IS_ACTIVEHIGH`, L129 `// #define LED_IS_ACTIVELOW` 주석. `LED_B_pin_CFX_test`는 어디에도 정의 없음. | **Active High 3채널 RGB 경로** (`LedOutput.c` L1044~1156) 기준 구현. ACTIVELOW / CFX_test 경로는 기존 `#if` 분기 유지(타 보드 호환). |
| 8 | 초기화 단계 Error 처리 | `systemControl.c` L165~170, L490~499: 에러 시 POWER_ON 미요청. | **"초기화 완료 & 에러 없음"에서만 POWER_ON 요청**. ERROR와 POWER_ON 동시 래치 불가. |

---

## 3. 최종 아키텍처

### 3.1 레이어
```
 [각 소스]              [Arbiter]             [Pattern Engine]           [HW]
 error.c        ─┐
 init flow      ─┤
 battery/ISD    ─┤── led_request(src, st) ─► prio 테이블 ─► LedPatternOut ─► LED_OUT() ─► GPIO
 ble_comm.c     ─┤      user_off 플래그                     (디스크립터 테이블,
 mapping flow   ─┤      PAIR 500ms latch                     burst 자가 해제)
 power on/off   ─┘
```

### 3.2 상태 enum
```c
typedef enum {
    LED_ST_NONE = 0,
    LED_ST_IDLE,

    // 운용
    LED_ST_READY,
    LED_ST_IN_USE,
    LED_ST_BATT_MID,
    LED_ST_BATT_CRITICAL,

    // Mapping
    LED_ST_MAPPING_NO_ISD,   // 파랑 300/300 점멸
    LED_ST_MAPPING_ISD,      // 파랑 지속 ON

    // BLE led_ind
    LED_ST_PAIR,             // 파랑 180/180 (+500ms latch)
    LED_ST_OTA_QCC,          // 녹색 1100/1100
    LED_ST_OTA_EZAIRO,       // 녹색 1100/1100

    // 에러 (색/패턴 공통, 구분은 이벤트 로그)
    LED_ST_ERROR_MAP,
    LED_ST_ERROR_MCU,
    LED_ST_ERROR_ACCEL,
    LED_ST_ERROR_FPGA,
    LED_ST_ERROR_PMIC,

    // 게이트
    LED_ST_POWER_ON,
    LED_ST_POWER_OFF,

    LED_ST__MAX
} led_state_t;

typedef enum {
    LED_SRC_POWER,     // POWER_ON / POWER_OFF
    LED_SRC_ERROR,
    LED_SRC_BLE_IND,   // PAIR / OTA_QCC / OTA_EZAIRO (BATT는 별도 상태 아님)
    LED_SRC_MAPPING,
    LED_SRC_BATTERY,   // BATT_CRITICAL / BATT_MID / READY
    LED_SRC_ISD,       // IN_USE
    LED_SRC__MAX
} led_src_t;
```

### 3.3 우선순위
| prio | 상태 | 비고 |
|---|---|---|
| 100 | POWER_OFF | 버스트 완료까지 유지 (게이트). Error 선점 |
| 95  | POWER_ON  | 버스트 완료까지 유지 (게이트). 에러 조건에서는 요청 자체가 안 남 |
| 90  | ERROR_* | 180/180 빨강. Power Off 외 전부 선점 |
| 80  | OTA_QCC / OTA_EZAIRO | 1100/1100 녹색 |
| 75  | MAPPING_ISD | 파랑 지속 |
| 75  | MAPPING_NO_ISD | 파랑 300/300 |
| 70  | PAIR | 파랑 180/180, latch 500ms |
| 60  | BATT_CRITICAL | 노랑 180/180, 사용자 off 시 억제 |
| 40  | IN_USE | 흰색 지속 |
| 30  | READY | 녹색 지속 |
| 20  | BATT_MID | 노랑 1100/1100 |
| 10  | IDLE | OFF |
| 0   | NONE | — |

### 3.4 패턴 디스크립터
```c
typedef struct {
    EN__LED_COLOR color;
    uint16_t      on_ms;
    uint16_t      period_ms;   // 0 = 지속 ON
    uint8_t       burst_cnt;   // 0 = 무한, >0 = N회 후 자가 해제
} led_pattern_desc_t;

static const led_pattern_desc_t k_led_patterns[LED_ST__MAX] = {
    [LED_ST_NONE]           = { en__LED_BLACK,  0,   0,  0 },
    [LED_ST_IDLE]           = { en__LED_BLACK,  0,   0,  0 },

    [LED_ST_READY]          = { en__LED_GREEN,  0,   0,  0 },
    [LED_ST_IN_USE]         = { en__LED_WHITE,  0,   0,  0 },
    [LED_ST_BATT_MID]       = { en__LED_ORANGE, 1100, 2200, 0 },
    [LED_ST_BATT_CRITICAL]  = { en__LED_ORANGE, 180,  360,  0 },

    [LED_ST_MAPPING_NO_ISD] = { en__LED_BLUE,   300,  600,  0 },
    [LED_ST_MAPPING_ISD]    = { en__LED_BLUE,   0,    0,    0 },

    [LED_ST_PAIR]           = { en__LED_BLUE,   180,  360,  0 },
    [LED_ST_OTA_QCC]        = { en__LED_GREEN,  1100, 2200, 0 },
    [LED_ST_OTA_EZAIRO]     = { en__LED_GREEN,  1100, 2200, 0 },

    [LED_ST_ERROR_MAP]      = { en__LED_RED,    180,  360,  0 },
    [LED_ST_ERROR_MCU]      = { en__LED_RED,    180,  360,  0 },
    [LED_ST_ERROR_ACCEL]    = { en__LED_RED,    180,  360,  0 },
    [LED_ST_ERROR_FPGA]     = { en__LED_RED,    180,  360,  0 },
    [LED_ST_ERROR_PMIC]     = { en__LED_RED,    180,  360,  0 },

    // 게이트 — 출하 검증값 유지
    [LED_ST_POWER_ON]       = { en__LED_SKYBLUE, 80,  300, 5 },
    [LED_ST_POWER_OFF]      = { en__LED_BLUE,   100,  300, 4 },
};
```
- `period_ms = on_ms + off_ms`. 예: `300/300` → `period=600, on=300`.
- `burst_cnt > 0` 패턴 완료 시 엔진이 `updateLED_OutputPattern(en__LED_NA)` 호출 + Arbiter에 "burst_done(LED_SRC_POWER)" 통지 → `systemControl`의 기존 `geteLED_OutputPattern()` 비교 로직 그대로 동작.

### 3.5 Arbiter
```c
static led_state_t s_req[LED_SRC__MAX];
static uint32_t    s_pair_latch_until_tick;   // PAIR latch
static bool        s_power_burst_in_progress; // 게이트 플래그(엔진에서 set/clear)

void led_request(led_src_t src, led_state_t st) {
    // PAIR latch: 요청이 들어오면 최소 500ms 유지
    if (src == LED_SRC_BLE_IND && st == LED_ST_PAIR) {
        s_pair_latch_until_tick = ci_timer_get_tick() + 500;
    }
    s_req[src] = st;
}

void led_arbiter_tick(void) {
    bool user_off = (readLED_indicatorOnOff() == 2);

    // PAIR latch 처리: 해제 요청이 와도 latch 동안 유지
    if (s_req[LED_SRC_BLE_IND] != LED_ST_PAIR
        && ci_timer_get_tick() < s_pair_latch_until_tick) {
        s_req[LED_SRC_BLE_IND] = LED_ST_PAIR;
    }

    led_state_t best = LED_ST_IDLE;
    int         max_p = -1;

    for (int src = 0; src < LED_SRC__MAX; ++src) {
        led_state_t st = s_req[src];
        int p = led_prio_of(st);

        // 사용자 LED off: ERROR / POWER_ON / POWER_OFF 외 전부 억제
        if (user_off && !led_is_error(st)
            && st != LED_ST_POWER_ON && st != LED_ST_POWER_OFF) {
            continue;
        }
        if (p > max_p) { max_p = p; best = st; }
    }

    LedPatternOut(led_state_to_enum(best));   // 기존 시그니처 유지
}
```

### 3.6 패턴 엔진 (통합 실행기)
```c
static void led_engine_run(led_state_t st, bool reset) {
    const led_pattern_desc_t* p = &k_led_patterns[st];
    static uint16_t timer_ms = 0;
    static uint8_t  burst_done_cnt = 0;

    if (reset) { timer_ms = 0; burst_done_cnt = 0; }

    if (p->period_ms == 0) {                    // 지속 ON
        LED_outputColor = p->color;
        return;
    }
    LED_outputColor = (timer_ms < p->on_ms) ? p->color : en__LED_BLACK;

    timer_ms++;
    if (timer_ms >= p->period_ms) {
        timer_ms = 0;
        if (p->burst_cnt > 0) {
            burst_done_cnt++;
            if (burst_done_cnt >= p->burst_cnt) {
                // 게이트 자가 해제: 기존 관례 유지
                updateLED_OutputPattern(en__LED_NA);
                s_req[LED_SRC_POWER] = LED_ST_NONE;
                s_power_burst_in_progress = false;
                burst_done_cnt = 0;
            }
        }
    } else {
        if (p->burst_cnt > 0) s_power_burst_in_progress = true;
    }
}
```
- 기존 `LedPatternOut(EN__LED_PATTERN)`은 새 엔진으로 리라우팅되는 **래퍼**로 남김.

---

## 4. 각 소스의 요청 규칙

### 4.1 `LED_SRC_POWER`
- `systemControl`이 Power On 진입 조건 성립 시 `led_request(POWER, POWER_ON)`.
- Power Off 조건 성립 시 `led_request(POWER, POWER_OFF)`.
- 버스트 완료 시 엔진이 자동 `LED_ST_NONE`으로 되돌림 (systemControl이 해제 호출할 필요 없음).
- `systemControl`은 기존처럼 `geteLED_OutputPattern() != en__LED_POWER_On/Off`를 게이트로 사용 가능. 신규 API `led_is_power_burst_in_progress()` 병행 제공.

### 4.2 `LED_SRC_ERROR`
- `error.c` 또는 초기화 루틴에서 에러 래치 시 해당 `LED_ST_ERROR_*` 요청.
- 복구 시 `led_request(ERROR, LED_ST_NONE)`.
- 초기화 중 에러 발생 → POWER_ON 요청 자체를 내지 않음 → 자연히 ERROR만 표시.

### 4.3 `LED_SRC_BLE_IND`
- `remoteControl.c`/`ci_ota.c`에서 상태 수신 시:
  - `TDC_LED_IND_STATE_OTA_QCC` → `LED_ST_OTA_QCC`
  - `TDC_LED_IND_STATE_OTA_EZAIRO` → `LED_ST_OTA_EZAIRO`
  - `TDC_LED_IND_STATE_PAIR` → `LED_ST_PAIR` (latch 500ms 자동)
  - `TDC_LED_IND_STATE_BATT` → **요청 없음** (본체 배터리 판정이 이미 처리)
  - `TDC_LED_IND_STATE_NONE` → `LED_ST_NONE`

### 4.4 `LED_SRC_MAPPING`
- 매 tick `main.c` 루프에서:
  ```c
  if (BLE_communicationState.mappingConnection) {
      led_request(MAPPING, isd_state.conneded_ISD ? MAPPING_ISD : MAPPING_NO_ISD);
  } else {
      led_request(MAPPING, LED_ST_NONE);
  }
  ```

### 4.5 `LED_SRC_BATTERY`
- `batteryNPowerControl` 또는 `systemControl`에서 %값 기반 판정:
  ```c
  int pct = snd_batt_get_percent();
  static led_state_t prev = LED_ST_IDLE;

  led_state_t st;
  if      (pct < 10)                           st = LED_ST_BATT_CRITICAL;
  else if (pct < 12 && prev == LED_ST_BATT_CRITICAL) st = LED_ST_BATT_CRITICAL; // hyst
  else if (pct >= 80)                          st = LED_ST_READY;
  else if (pct >= 78 && prev == LED_ST_READY)  st = LED_ST_READY;               // hyst
  else                                         st = LED_ST_BATT_MID;

  prev = st;
  led_request(BATTERY, st);
  ```
- 충·방전 상태는 참조하지 않음.

### 4.6 `LED_SRC_ISD`
- `isd_state.conneded_ISD == true` → `led_request(ISD, LED_ST_IN_USE)`.
- 아니면 `LED_ST_NONE`.
- Mapping이 선점하는 경우(§4.4)는 prio에서 자동 해결.

### 4.7 사용자 LED off 플래그
- Arbiter 최종 선택 단계에서만 적용. 소스별 요청은 항상 기록 (상태 복귀 시 지연 없음).

---

## 5. 기존 코드 대응표

| 기존 | 처리 |
|---|---|
| `en__LED_ISD_StimulationOut_batteryNormal/Low` | 제거 → `LED_ST_IN_USE` |
| `en__LED_StandbyForconneded_ISD_batteryNormal/Low` | 제거 → `LED_ST_READY` / `LED_ST_BATT_MID` / `LED_ST_BATT_CRITICAL` |
| `en__LED_MappingConneted_ISD_Connected/Unconnected_BatteryNormal/Low` (4종) | 제거 → `LED_ST_MAPPING_ISD` / `LED_ST_MAPPING_NO_ISD` |
| `en__LED_BatteryChargingLevel_0per ~ 100per` (7종) | 제거 (충전 레벨 dot-count 기능 폐지). 충전 중에도 §1.3 레벨 매핑 사용 |
| `en__LED_Map_Error / MCU_Error / Accelerometer_Error / FPGA_Error / RF_PMIC_Error` | 타이밍 180/360 통일. 소스 식별은 이벤트 로그 |
| `en__LED_POWER_On / POWER_Off` | 유지 (게이트). 색/시간 기존값 유지 |
| `en__LED_IND_BATT_STATE` | 제거 (BLE BATT는 no-op) |
| `en__LED_IND_PAIR_STATE` | `LED_ST_PAIR`로 통합 |
| `tdc_led_set_ind_state / get_ind_state` | 유지 (BLE 수신 경로에서 상태 저장 용도). `led_request(BLE_IND, …)` 호출을 이 set 내부에 추가 |
| `readLED_indicatorOnOff()` | 유지. Arbiter에서 일괄 참조 (소스별 개별 분기 제거) |
| `LED_OUT()` | 유지. GPIO 매핑 로직은 그대로 |
| `LED_IS_ACTIVEHIGH` 경로 (Board_OTE_ver1_5) | 기준 경로로 삼음 |
| `LED_IS_ACTIVELOW`, `LED_B_pin_CFX_test` 경로 | `#if` 분기 유지(타 보드) |

---

## 6. 마이그레이션 단계 (구현 순서)

1. **헤더 정의**: `led_state_t`, `led_src_t`, `led_pattern_desc_t`, 프로토타입 추가. 기존 `EN__LED_PATTERN`은 유지(래퍼용).
2. **`led_state_to_enum()` 매핑 함수** 및 **패턴 디스크립터 테이블** 구현.
3. **`led_engine_run()`** 구현 + 기존 개별 함수(`LED_Map_error`, `sitimulationOutputOnLED_*`, …) 삭제.
4. **`LedPatternOut()`** 내부를 `led_engine_run(state_of(enum), reset)`으로 교체. 게이트 자가 해제 로직 보존 검증.
5. **Arbiter** (`led_request`, `led_arbiter_tick`) 추가. `main.c` 루프에서 `LedPatternOut(systemStatus.Led_Pattern)` 호출 직전에 `led_arbiter_tick()` 호출.
6. **소스별 `led_request` 삽입**:
   - `systemControl.c`의 `systemStatus.Led_Pattern = X` 직접 대입 제거. POWER_ON/OFF만 남김.
   - Error 경로 → `led_request(ERROR, ...)`.
   - 배터리/ISD/Mapping 요청 라인 추가.
   - `remoteControl.c` / `ci_ota.c`의 BLE 수신부 → `led_request(BLE_IND, ...)`.
7. **PAIR latch** 500ms 동작 검증 (강제 주입 테스트).
8. **사용자 LED off 검증**: BATT_CRITICAL / MAPPING / READY 등 억제, ERROR / POWER_* 표시 유지 확인.
9. **Dead enum 삭제** (`en__LED_ISD_*`, `MappingConneted_*`, `StandbyForconneded_*`, `BatteryChargingLevel_*`, `IND_BATT_STATE`).
10. **회귀 테스트**: Power On → READY 전이, Power Off 버스트 후 systemOff, 에러 주입, OTA, mapping 연결/ISD 연결 조합.

---

## 7. 검증 계획

### 7.1 단위 (`tests/led/`)
- 디스크립터별 on/period 오차 ±1ms (tick 시뮬).
- Arbiter 우선순위 선택: 모든 (src×state) 조합에서 최상위 prio 선택 확인.
- 사용자 off 플래그: 억제 대상/예외 대상 분리 확인.
- PAIR latch: 입력이 `NONE`으로 바뀐 후에도 500ms 동안 `PAIR` 유지.
- 히스테리시스: 9%→11%→9% 변화 시 BATT_CRITICAL 진입/유지/해제 확인.

### 7.2 통합 / HW
- 오실로스코프: 180/360, 1100/2200, 300/600 파형 ±5% 이내.
- 시나리오:
  - (a) 초기화 OK → POWER_ON 버스트 → READY.
  - (b) 초기화 실패 → ERROR 즉시.
  - (c) 운용 중 ERROR 주입 → 기존 표시 선점.
  - (d) POWER_OFF 요청 → ERROR보다 우선.
  - (e) mapping 연결 + ISD 미연결 → 파랑 300/300.
  - (f) mapping 연결 + ISD 연결 → 파랑 지속.
  - (g) 배터리 9% + ISD 연결 → BATT_CRITICAL. 사용자 off → OFF.
  - (h) 배터리 9% + ERROR → ERROR (상위 prio).
  - (i) OTA_QCC 수신 → 녹색 1100/2200.
  - (j) PAIR 100ms 짧은 이벤트 → 500ms latch로 표시 확인.

---

## 8. 요약

- **열린 이슈 8건 모두 결정 완료**. 구현 착수 가능.
- 아키텍처: **소스별 `led_request` → Arbiter(prio + user_off + PAIR latch) → 패턴 엔진(디스크립터 테이블, 버스트 자가 해제) → GPIO(Active High)**.
- Power On/Off 버스트 자가 해제는 `updateLED_OutputPattern(en__LED_NA)`로 유지 → `systemControl`의 기존 게이트 조건(`geteLED_OutputPattern()`) 호환.
- 배터리는 `snd_batt_get_percent()` 기반 80%/10% + ±2% 히스테리시스. 충·방전 무관.
- 사용자 LED off는 Arbiter 전역 적용, ERROR/POWER_ON/POWER_OFF만 예외.
- Mapping 연결은 파랑 300/300 점멸(ISD 미연결) 또는 파랑 지속(ISD 연결)로 BATT_CRITICAL을 선점.
- Board_OTE_ver1_5는 `LED_IS_ACTIVEHIGH`로 정의됨(확인 완료) → 해당 경로 기준.
