# 2__cm3 LED 운용 분석 및 수정 방향 (Rev.2)

작성자: 김은수
작성일: 2026-04-14

근거:
- `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.{c,h}`
- `src/2__cm3/Cortex-M3-src/systemControl/systemControl.c`
- `src/2__cm3/Cortex-M3-src/systemControl/cfx_cm3_sharedMemory.{c,h}` (`readLED_indicatorOnOff`)
- `src/2__cm3/Cortex-M3-src/BleCommunication/remoteControl.c`
- `src/2__cm3/Cortex-M3-src/main.c`
- `docs/led_ind_state.JPG`

Rev.2 변경점
- Mapping 연결(매핑 프로그램 접속) 시 **파랑 LED 표시 규칙** 반영.
- `systemControl()` 로직이 **Power On / Power Off LED 패턴 완료**를 게이트로 사용하는 구조 반영(버스트 완료 핸드셰이크).

---

## 1. 현재 구조의 핵심 동작 (게이트 메커니즘)

### 1.1 Power On 게이트
`systemControl()` L287~299:
```c
if (StartFlag == false) {
    systemStatus.Led_Pattern = en__LED_POWER_On;
    StartFlag = true;
}
else {
    if (current_led_pattern != en__LED_POWER_On) {
        // ... 여기서부터 정상 운용 로직 수행 (enable_ISD, PMIC, …) ...
    }
}
```
- `current_led_pattern`은 `geteLED_OutputPattern()` 반환값.
- `Led_powerOn()` 내부 N회 버스트 후 `updateLED_OutputPattern(en__LED_NA)`로 자기 자신을 NA로 전환 → `current_led_pattern != en__LED_POWER_On` 조건이 참이 되면서 **정상 운용 루틴 진입**.

### 1.2 Power Off 게이트
L348~375:
```c
if (isPowerOffEnabled) {
    if ((current_led_pattern != en__LED_POWER_Off) && (PowerOff_StartCounter != 0)) {
        // 버스트 완료 → systemOff = true
    }
    PowerOff_StartCounter++;
}
```
- 마찬가지로 `Led_powerOff()` 버스트 완료 시 `current_led_pattern`이 NA로 바뀌면서 저전력 모드로 진입.

### 1.3 결론
`current_led_pattern` 값은 **시스템 상태 머신과 동기된 완료 신호**로 사용 중. 새 아키텍처에서도 반드시 다음 계약을 유지해야 함:

- **Power On / Power Off 패턴은 "지속 요청 + 내부 버스트 완료 시 자동 해제"** 라는 동작을 그대로 제공.
- `systemControl`이 "Power On/Off 패턴이 현재 실제로 나가고 있는가?"를 조회할 수 있어야 함.

---

## 2. 요구사항 통합 (Rev.0 → Rev.2 누적)

### 2.1 라이프사이클 & 에러
- 초기화 OK → Power On LED 표시.
- 초기화 실패 → 해당 Error LED (Power On 생략).
- Power On 완료 후 운용 중 에러 발생 → Error LED 즉시 표시.
- **Power Off 표시만 Error보다 우선**.

### 2.2 BLE `led_ind`
- `led_ind != NONE` 이면 해당 상태 표시 (PAIR / OTA_QCC / OTA_EZAIRO / BATT).

### 2.3 ISD 연결 상태
- ISD 연결 시 배터리 레벨별 LED 세분화 폐기 → **"사용 중(IN_USE)" 단일**.

### 2.4 배터리 매핑 (충/방전 무관)
- `level ≥ TH_READY` → READY (녹색 지속)
- `TH_CRITICAL ≤ level < TH_READY` → BATT_MID (노랑 1100/1100)
- `level < TH_CRITICAL` → BATT_CRITICAL (노랑 180/180)

### 2.5 BATT_CRITICAL 오버라이드
- ISD 연결 중에도 BATT_CRITICAL 표시.
- **사용자 LED 비활성화(`readLED_indicatorOnOff()==2`)** 시 억제.

### 2.6 Mapping 연결 시 파랑 표시 (Rev.2 신규)
- 매핑 프로그램 연결(`BLE_communicationState.mappingConnection == true`) 중:
  - **ISD 미연결** → 파랑 **300ms ON / 300ms OFF 점멸**. (READY / BATT_MID / BATT_CRITICAL 상태여도 파랑 점멸로 덮음)
  - **ISD 연결** → 파랑 **지속 ON**. (IN_USE 녹색 지속을 파랑 지속으로 대체)
- Mapping 연결 표시는 **BATT_CRITICAL보다 상위**. 단, **Error / Power Off / OTA / Power On 버스트 중**보다는 하위.
- 사용자 LED off 시 처리: 운용 상태이므로 억제 대상 (열린 이슈 7.2 확인 필요).

---

## 3. 우선순위 테이블

| prio | 상태 | 색/패턴 | 비고 |
|---|---|---|---|
| 100 | POWER_OFF | 기존 `Led_powerOff` 버스트 | 버스트 완료까지 유지 (게이트) |
| 95  | POWER_ON | 기존 `Led_powerOn` 버스트 | 버스트 완료까지 유지 (게이트). Error 발생 시 Error에 의해 선점 가능(§2.1) |
| 90  | ERROR (Map/MCU/Accel/FPGA/PMIC) | 빨강 180/180 | Power Off만 선점 가능 |
| 80  | OTA_QCC / OTA_EZAIRO | 녹색 1100/1100 | BLE led_ind |
| 75  | MAPPING_CONNECTED | ISD 연결: 파랑 지속 / ISD 미연결: 파랑 300/300 | §2.6 |
| 70  | BLE led_ind = PAIR | 파랑 180/180 | 인증 중. latch 검토(§7.4) |
| 65  | BLE led_ind = BATT | 노랑 계열(본체 배터리 규칙과 동일) | |
| 60  | BATT_CRITICAL | 노랑 180/180 | ISD 연결 중에도 표시, 사용자 off 시 억제 |
| 40  | IN_USE | 녹색 지속 | ISD 연결됨 (mapping 미연결) |
| 30  | READY | 녹색 지속 | ISD 미연결 & 레벨≥TH_READY |
| 20  | BATT_MID | 노랑 1100/1100 | ISD 미연결 & TH_CRITICAL≤레벨<TH_READY |
| 10  | IDLE | OFF | |
| 0   | NONE | — | |

핵심 정리
- Power On / Off는 "게이트" 특성이 있어 `MUST complete`. 새 Arbiter는 이 두 상태에 대해 **`latch=true` + `auto_release_on_burst_done=true`** 속성을 가짐.
- Error는 POWER_ON 버스트를 선점 가능(초기화 실패 시 즉시 에러 표시). 기존 코드도 에러 분기에서 POWER_ON을 쓰지 않음.
- POWER_OFF는 ERROR 선점 가능.
- MAPPING_CONNECTED는 BATT_CRITICAL 선점(§2.6).

---

## 4. 상태 결정 로직 (의사코드)

```c
// 매 tick (1ms)
void led_update_requests(void)
{
    bool user_off = (readLED_indicatorOnOff() == 2);

    // --- 게이트성 상태 ---
    // POWER_ON/OFF는 systemControl이 led_request(LATCH)로 설정,
    // 버스트 완료시 led_engine이 스스로 release → gate_released 플래그 갱신

    // --- 에러 ---
    if (error_latched()) led_request(LED_SRC_ERROR, LED_ST_ERROR_xxx);

    // --- BLE led_ind ---
    switch (ble_led_ind) {
        case TDC_LED_IND_STATE_OTA_QCC:    led_request(BLE, OTA_QCC);    break;
        case TDC_LED_IND_STATE_OTA_EZAIRO: led_request(BLE, OTA_EZAIRO); break;
        case TDC_LED_IND_STATE_PAIR:       led_request(BLE, PAIR);       break;
        case TDC_LED_IND_STATE_BATT:       led_request(BLE, BATT_PROXY); break;
        default: led_request(BLE, NONE);
    }

    // --- Mapping ---
    if (mappingConnection) {
        led_request(MAPPING,
                    isd_connected ? MAPPING_CONN_ISD : MAPPING_CONN_NO_ISD);
    } else {
        led_request(MAPPING, NONE);
    }

    // --- 배터리 / ISD ---
    led_state_t batt;
    if      (battery_level < TH_CRITICAL) batt = BATT_CRITICAL;
    else if (isd_connected)               batt = IN_USE;
    else if (battery_level >= TH_READY)   batt = READY;
    else                                  batt = BATT_MID;
    led_request(BATTERY, batt);

    // --- Arbiter ---
    led_state_t chosen = led_arbiter_pick(user_off);
    LedPatternOut(map_state_to_enum(chosen));
}

static led_state_t led_arbiter_pick(bool user_off)
{
    led_state_t best = IDLE;
    int max_p = -1;

    for (each src) {
        led_state_t st = req[src];
        int p = prio_of(st);

        // 사용자 LED off: ERROR / POWER_OFF 외에는 모두 억제
        if (user_off && !is_error(st) && st != POWER_OFF)
            continue;

        if (p > max_p) { max_p = p; best = st; }
    }
    return best;
}
```

### 4.1 `systemControl()`와의 인터페이스 유지
현재 `systemControl`은 다음 조회를 수행:
```c
if (current_led_pattern != en__LED_POWER_On)  { /* 정상 운용 로직 */ }
if ((current_led_pattern != en__LED_POWER_Off) && (PowerOff_StartCounter != 0)) { /* systemOff */ }
```
→ 새 구조에서도 동일 의미의 조회를 제공해야 함. 두 가지 방법:

(a) `geteLED_OutputPattern()` 의미 유지: Arbiter가 최종 선택한 pattern을 `updateLED_OutputPattern()`으로 갱신하고 버스트 완료 시 `en__LED_NA`로 자가 해제.
- `systemControl`의 기존 `current_led_pattern != en__LED_POWER_On` 비교는 그대로 동작.
- **장점**: 호출 측 수정 최소.

(b) 새 API 추가: `bool led_is_power_on_in_progress(void);`, `bool led_is_power_off_in_progress(void);`
- 명시적이지만 `systemControl` 전반 수정 필요.

**추천**: (a) + (b) 병행. (a)로 기존 호환, (b)는 신규 코드에서 사용.

### 4.2 버스트 완료 시나리오 (게이트 해제)
```
t0  : systemControl → systemStatus.Led_Pattern = en__LED_POWER_On
t0  : main → LedPatternOut(en__LED_POWER_On) → Arbiter: POWER_ON 래치
t0+  : engine.Led_powerOn 버스트 수행 (N회)
tEnd: 버스트 완료 → updateLED_OutputPattern(en__LED_NA) + POWER_ON 래치 해제
tEnd+1ms: systemControl 진입 → current_led_pattern == NA → 정상 운용 로직 실행
```
POWER_OFF도 동일 메커니즘.

---

## 5. 패턴 디스크립터 테이블

```c
typedef struct {
    EN__LED_COLOR color;
    uint16_t      on_ms;
    uint16_t      period_ms;   // 0 = 지속 ON
    uint8_t       burst_cnt;   // 0 = 무한, >0 = N회 후 자동 해제
} led_pattern_desc_t;

static const led_pattern_desc_t k_led_patterns[] = {
    // --- 게이트성 (기존 파형 유지 혹은 스펙으로 통일 — §7.6) ---
    [LED_ST_POWER_ON]     = { en__LED_SKYBLUE,  80, 300, 5 },   // 기존값 유지
    [LED_ST_POWER_OFF]    = { en__LED_BLUE,    100, 300, 4 },   // 기존값 유지

    // --- Error (180/180 통일, 스펙 §1) ---
    [LED_ST_ERROR_MAP]    = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_MCU]    = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_ACCEL]  = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_FPGA]   = { en__LED_RED,    180, 360, 0 },
    [LED_ST_ERROR_PMIC]   = { en__LED_RED,    180, 360, 0 },

    // --- BLE led_ind ---
    [LED_ST_OTA_QCC]      = { en__LED_GREEN, 1100,2200, 0 },
    [LED_ST_OTA_EZAIRO]   = { en__LED_GREEN, 1100,2200, 0 },
    [LED_ST_PAIR]         = { en__LED_BLUE,   180, 360, 0 },

    // --- Mapping (Rev.2) ---
    [LED_ST_MAPPING_NO_ISD] = { en__LED_BLUE,  300, 600, 0 },   // 300/300 → period 600
    [LED_ST_MAPPING_ISD]    = { en__LED_BLUE,   0,   0, 0 },    // 지속 ON

    // --- 배터리 / ISD ---
    [LED_ST_BATT_CRITICAL]= { en__LED_ORANGE, 180, 360, 0 },
    [LED_ST_BATT_MID]     = { en__LED_ORANGE,1100,2200, 0 },
    [LED_ST_IN_USE]       = { en__LED_GREEN,   0,   0, 0 },
    [LED_ST_READY]        = { en__LED_GREEN,   0,   0, 0 },
    [LED_ST_IDLE]         = { en__LED_BLACK,   0,   0, 0 },
};
```

주의
- `period_ms = on_ms + off_ms`. 300/300 점멸이면 `on_ms=300, period_ms=600`.
- `burst_cnt > 0`은 POWER_ON/OFF 전용. 완료 후 엔진이 `updateLED_OutputPattern(en__LED_NA)` 호출로 자가 해제 → Arbiter 래치 해제 → 다음 tick에서 하위 prio가 선택됨.

---

## 6. 마이그레이션 단계

1. **패턴 엔진(디스크립터 테이블) 도입**
   - 기존 개별 함수(`LED_Map_error`, `sitimulationOutputOnLED_*` 등)는 래퍼로 남겨 회귀 차단.
   - `LED_ST_MAPPING_NO_ISD`, `LED_ST_MAPPING_ISD` 신규 추가.
2. **Arbiter 도입** + `led_request(src, state)` API.
3. **게이트 호환 유지**: `updateLED_OutputPattern()` 갱신 경로 유지하여 `geteLED_OutputPattern()` 호환. 신규 API(`led_is_power_on_in_progress` 등) 병행 추가.
4. **`systemControl` 직접 대입 제거** (`systemStatus.Led_Pattern = X`)
   - Charging 구간(충전기 연결 + 커버 오픈)의 `BatteryChargingLevel_*` 7단 대입 제거 → §2.4 매핑으로 대체(열린 이슈 7.1 확인).
   - Mapping 연결 구간 대입 제거 → `led_request(MAPPING, ...)` 로 교체.
   - Stimulation/Standby ISD×Battery 조합 대입 제거 → `led_request(BATTERY, ...)` + `led_request(ISD, IN_USE/READY)`.
   - Error 분기 → `led_request(ERROR, ...)`.
   - POWER_ON/OFF 대입은 유지(게이트 용도).
5. **BLE 경로에서 `led_request(BLE, ...)` 호출**: `remoteControl.c`에서 `changeLED_indicatorOnOff`처럼 `led_ind` 반영 지점 추가.
6. **MAPPING 연결 판정 라인 통합**: `main.c` L490의 `BLE_communicationState.mappingConnection`를 Arbiter 입력으로 연결.
7. **dead enum 제거**: `en__LED_ISD_*`, `en__LED_MappingConneted_*`, `en__LED_StandbyForconneded_*`, `en__LED_BatteryChargingLevel_*`, `en__LED_IND_BATT_STATE`, `en__LED_IND_PAIR_STATE`(이미 활용됨으로 재매핑) 정리.

---

## 7. 열린 이슈 (결정 필요)

1. 충전 중 표시: 본 Rev.2는 **충전여부 무시, 레벨 기준 매핑**으로 통일. 스펙 이미지의 "충전 중 빨강 느린 점멸"과 기존 코드의 "충전 레벨 dot-count(0~100%, 7단)"를 모두 폐기. 채택 여부 확정 필요.
2. 사용자 LED off 플래그의 억제 범위:
   - 확정: BATT_CRITICAL / BATT_MID / READY / IN_USE / MAPPING / BLE led_ind → 억제.
   - 확정: ERROR / POWER_OFF → 표시 유지.
   - 검토 필요: MAPPING_CONNECTED도 억제 대상으로 두는 것이 맞는지(사용자가 매핑 중이라는 사실은 보여주는 게 유용할 수 있음).
3. `TH_READY`, `TH_CRITICAL` 수치 및 히스테리시스(예: READY 80%±2%, CRITICAL 10%±2%).
4. PAIR latch: 180ms 미만 이벤트에 대해 최소 표시 시간 보장 여부.
5. BLE `led_ind=BATT` 수신 시 본체 배터리 판정과 충돌 시 우선순위 (Rev.2 기준: BLE prio 65 > 본체 BATT_MID(20) / BATT_CRITICAL(60) → BLE가 BATT_MID는 선점, BATT_CRITICAL과는 prio 재검토).
6. POWER_ON/OFF 버스트의 색/시간을 스펙 표에 맞출지, 기존(SKYBLUE 80/300×5, BLUE 100/300×4)을 유지할지.
7. `LED_IS_ACTIVELOW` / `LED_B_pin_CFX_test` 매크로 Board_OTE_ver1_5 정합성.
8. 초기화 단계에서 Error가 발생하면 POWER_ON 버스트를 **건너뛰는지/즉시 중단**하는지. 현재 코드는 에러 분기에서 POWER_ON을 쓰지 않음 → **건너뛰기가 자연스러움**. 새 Arbiter에서도 ERROR prio 90 > POWER_ON prio 95는 아니므로 **POWER_ON이 래치되어 있으면 완료 후 ERROR**. 요구사항에 맞게 **ERROR가 래치되면 POWER_ON 래치 즉시 무효화**하는 예외 규칙 추가 필요(ERROR prio를 POWER_ON보다 높게 두거나, 초기화 단계에서는 POWER_ON을 아직 요청하지 않도록 순서 제어).

   **제안**: 초기화 완료 후에만 POWER_ON을 요청 → ERROR가 래치된 상태에서는 POWER_ON을 요청하지 않음. 이러면 prio 관계를 바꾸지 않아도 된다(요구사항 §2.1과 일치).

---

## 8. 요약

- 최종 우선순위: **POWER_OFF > POWER_ON(게이트) > ERROR > OTA > MAPPING_CONNECTED > PAIR > BLE_BATT > BATT_CRITICAL > IN_USE > READY > BATT_MID > IDLE**.
- Mapping 연결 표시(Rev.2): ISD 미연결 시 파랑 300/300 점멸, ISD 연결 시 파랑 지속 ON. BATT_CRITICAL 선점.
- POWER_ON / POWER_OFF는 **버스트 완료가 시스템 상태 전이의 게이트**. 새 구조에서도 `geteLED_OutputPattern()` → `en__LED_NA` 자가 해제 동작을 유지해 기존 `systemControl` 호환.
- 배터리/ISD는 4상태(READY/IN_USE/BATT_MID/BATT_CRITICAL)로 축약. 충전 여부는 LED에 반영하지 않음.
- 사용자 LED off 플래그는 Arbiter에서 전역 적용(ERROR/POWER_OFF 예외).
- 구조: **각 소스 → `led_request()` → Arbiter(우선순위, user-off) → 패턴 엔진(디스크립터 테이블, 버스트 자가 해제) → GPIO 출력**.
