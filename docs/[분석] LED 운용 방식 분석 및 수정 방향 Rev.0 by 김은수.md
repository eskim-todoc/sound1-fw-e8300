# 2__cm3 LED 운용 분석 및 수정 방향

작성자: 김은수
작성일: 2026-04-14
근거: `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.{c,h}`,
      `src/2__cm3/Cortex-M3-src/systemControl/systemControl.c`,
      `src/2__cm3/Cortex-M3-src/main.c`,
      `docs/led_ind_state.JPG`

---

## 1. 현재 구조 요약

### 1.1 레이어
```
systemControl.c  →  systemStatus.Led_Pattern (EN__LED_PATTERN)
       │
main.c (Line 514)
       │
LedPatternOut(pattern)      ──►  pattern별 전용 함수 호출
       │                           (Led_powerOn, LED_Map_error,
       │                            sitimulationOutputOnLED_*, …)
       │                           → LED_outputColor 갱신
       │
LED_OUT()                   ──►  PWM duty + GPIO 출력
                                 (LED_IS_ACTIVELOW / LED_B_pin_CFX_test 분기)
```

- 주기: `main.c`에서 1ms 루프 기반 호출 추정(각 함수 내부 `timerCounter++` → ms 단위).
- 보드/회로 분기: `LED_IS_ACTIVELOW`, `LED_B_pin_CFX_test` 매크로로 GPIO 극성/채널 수 분리.
- PWM: `LED_OUT()`에서 `pwmTime_ms=10`, `pwmDuty_rate=2~3`으로 밝기 제어.

### 1.2 패턴 enum (`EN__LED_PATTERN`)
현재 정의된 패턴:

- 에러 계열: `en__LED_Map_Error`, `MCU_Error`, `Accelerometer_Error`, `FPGA_Error`, `RF_PMIC_Error`
- 전원: `en__LED_POWER_On`, `en__LED_POWER_Off`
- 자극/스탠바이: `en__LED_ISD_StimulationOut_batteryNormal/Low`,
  `en__LED_StandbyForconneded_ISD_batteryNormal/Low`
- 매핑 연결: `en__LED_MappingConneted_ISD_Connected/Unconnected_BatteryNormal/Low`
- 배터리 충전 레벨: `en__LED_BatteryChargingLevel_0per` ~ `100per` (7단계, WHITE 사용)
- **신규(미구현) — `// Sound1, LED Indication 추가 by QCC`**
  - `en__LED_IND_BATT_STATE`
  - `en__LED_IND_PAIR_STATE`

`tdc_led_ind_state_t` (NONE/BATT/PAIR/OTA_QCC/OTA_EZAIRO)가 별도 enum으로 존재하지만
`set/get`만 있고 **`LedPatternOut()` switch에서 사용되지 않음** → dead path.

### 1.3 우선순위 처리
- 헤더 상단 주석에 "1순위: 에러, 2순위: POWER On/Off, 3순위: (공란)"만 적혀 있음.
- **실제 코드는 우선순위 로직 없음.** `systemState.Led_Pattern` 한 변수를 그때그때 덮어씀(`systemControl.c`).
- 충돌 가능 시나리오가 존재:
  - 에러 발생 + 배터리 저전압 + BLE 인증 → 마지막으로 쓰인 값이 표시됨.

---

## 2. 스펙 이미지(`led_ind_state.JPG`) 요구사항

| 우선순위 | 상태 | 색상 | 패턴 | 표시 조건 | ON(ms) | OFF(ms) | 주기(Hz) | 비고 |
|---|---|---|---|---|---|---|---|---|
| 5 | 에러 | 빨강 | 빠른 점멸 | 시스템 에러 | 180 | 180 | 2.8 | 60601-1 근거 |
| — | 배터리 충전 중 | 빨강 | 느린 점멸 | 충전 중 | 1100 | 1100 | 0.45 | |
| — | 배터리 <10% | 노랑 | 빠른 점멸 | 배터리 <10% | 180 | 180 | 2.8 | |
| — | 배터리 중간 | 노랑 | 느린 점멸 | 10–80% | 1100 | 1100 | 0.45 | |
| — | 사용 준비 | 녹색 | 계속 ON | 80%↑ & 내부기 미연결 | ON | — | — | |
| — | BLE 인증 진행 중 | 파랑 | 빠른 점멸 | 인증/연결 중 | 180 | 180 | 2.8 | ** |
| 2 | DFU 중 (OTA‑QCC) | 녹색 | 느린 점멸 | QCC 업데이트 | 1100 | 1100 | 0.45 | |
| 3 | DFU 중 (OTA‑Ezairo) | 녹색 | 느린 점멸 | Ezairo 업데이트 | 1100 | 1100 | 0.45 | |
| — | 사용 중 | 녹색 | 계속 ON | — | ON | — | — | |
| — | 꺼져 있음 | — | — | — | — | — | — | |

주석:
- `*` ON/OFF 시간은 E8300 구현 완성도 고려하여 추후 수정 가능.
- `**` 인증 진행 시간이 매우 짧으면 실제 LED 표시가 불가능할 수 있음.

---

## 3. 스펙 vs 현재 코드 갭

### 3.1 타이밍 불일치
| 스펙 | 현재 코드 | 갭 |
|---|---|---|
| 에러: 180/180 (2.8Hz) | `LED_Map_error`: 100/400(=500ms, 2Hz), `LED_MCU_error`: 500/500(1Hz) 등 제각각 | 전 에러 공통 180/180으로 통일 필요 |
| 배터리 <10%: 180/180 노랑 | 배터리 저전압 표현이 Orange(=노랑 근사) **지속 ON**만 존재(`standby_isdNotConnectedLED_batteryLow`) | 빠른 점멸 전용 핸들러 추가 필요 |
| 배터리 10–80%: 1100/1100 노랑 | 해당 구간용 "노랑 느린 점멸" 없음 | 신규 핸들러 필요 |
| 배터리 80%↑(사용 준비): 녹색 지속 ON | `standby_isdNotConnectedLED_batteryNormal()` = GREEN 지속 → 일치 | OK, 조건(80%↑) 판정만 보강 |
| BLE 인증: 파랑 180/180 | `MappingConneted_ISD_Connected_BatteryNormal` = BLUE 200/800(1Hz) | 신규 빠른 점멸 필요 |
| OTA(QCC/Ezairo): 녹색 1100/1100 | **완전 미구현** | 필수 추가 |
| 사용 중: 녹색 지속 ON | `sitimulationOutputOnLED_batteryNormal` = GREEN 200/800 점멸 | **점멸→지속 ON으로 의미 변경 필요** |
| 충전 중: 빨강 1100/1100 | 충전 중 표시 패턴은 "레벨 도트 카운트" 형태(`batteryChargingLevelLED_*`) | 스펙은 단순 느린 점멸 하나로 단순화 |

### 3.2 우선순위 부재
스펙은 숫자로 우선순위(에러=5, DFU=2/3 등) 명시.
현재 코드는 `Led_Pattern = X` 덮어쓰기만 함 → **우선순위 결정 레이어 없음**.

### 3.3 dead code
- `TDC_LED_IND_STATE_*` (set/get만 있고 사용처 없음)
- `en__LED_IND_BATT_STATE`, `en__LED_IND_PAIR_STATE` (enum만 있고 `switch`에서 처리 안 함)
- `LED_IS_ACTIVELOW`가 정의되지 않은 브랜치도 `LED_OUT()`에 존재 → 보드 매크로 정합성 점검 필요.

### 3.4 기타 문제
- `batteryChargingLevelLED_0btw20()` 등에서 `resetTimerCounter`가 들어와도 `longtimeCounter`/`outputCycleCounter`는 리셋 안 됨 → 패턴 재진입 시 초기 위상 어긋날 수 있음.
- `sitimulationOutputOnLED_*` 재진입 타이머 리셋은 `shortTimerCounter`만 초기화.

---

## 4. 수정 방향 제안

### 4.1 아키텍처: 2단 구조로 분리
```
 [각 서브시스템]               [Arbiter]             [Pattern Engine]
 error.c        ─┐
 battery.c      ─┤─►  led_request(src, state)  ─► 우선순위 테이블 ─► LedPatternOut()
 ble_comm.c     ─┤
 ci_ota.c       ─┤
 systemControl  ─┘
```
- `led_request()`에 소스별 상태를 쌓고, Arbiter가 **매 tick 최우선 1개 선택** → `LedPatternOut()` 호출.
- 현재처럼 `Led_Pattern`을 직접 덮어쓰는 방식 폐기.

### 4.2 패턴 엔진 일반화
개별 함수(`LED_Map_error`, `sitimulationOutputOnLED_*` 등)들이 구조는 거의 같음(color, onTime, period).
→ 공통 구조체로 축약:

```c
typedef struct {
    EN__LED_COLOR color;
    uint16_t      on_ms;
    uint16_t      period_ms;   // 0 = 지속 ON
    uint8_t       burst_cnt;   // 0 = 무한, >0 = N회 후 종료
} led_pattern_desc_t;

static void led_run_pattern(const led_pattern_desc_t* p, bool reset);
```

스펙 표를 그대로 const 테이블로 내려서 유지보수 포인트를 1곳으로 집중.

```c
static const led_pattern_desc_t k_led_patterns[] = {
    [en__LED_MCU_Error]            = { en__LED_RED,   180, 360, 0 }, // 2.8Hz
    [en__LED_Map_Error]            = { en__LED_RED,   180, 360, 0 },
    [en__LED_IND_BATT_CHARGING]    = { en__LED_RED,  1100,2200, 0 }, // 0.45Hz
    [en__LED_IND_BATT_CRITICAL]    = { en__LED_ORANGE,180, 360, 0 }, // <10%
    [en__LED_IND_BATT_MID]         = { en__LED_ORANGE,1100,2200, 0 },// 10-80%
    [en__LED_IND_READY]            = { en__LED_GREEN, 0,   0,   0 }, // 지속 ON
    [en__LED_IND_PAIR_STATE]       = { en__LED_BLUE,  180, 360, 0 },
    [en__LED_IND_OTA_QCC]          = { en__LED_GREEN,1100,2200, 0 },
    [en__LED_IND_OTA_EZAIRO]       = { en__LED_GREEN,1100,2200, 0 },
    [en__LED_IND_IN_USE]           = { en__LED_GREEN, 0,   0,   0 },
    [en__LED_NA]                   = { en__LED_BLACK, 0,   0,   0 },
};
```

### 4.3 우선순위 테이블
스펙 숫자(5=에러 최상위)를 코드화:

```c
typedef enum { LED_PRIO_OFF=0, LED_PRIO_READY, LED_PRIO_IN_USE,
               LED_PRIO_BATT_MID, LED_PRIO_BATT_CHARGING,
               LED_PRIO_OTA, LED_PRIO_PAIR,
               LED_PRIO_BATT_CRITICAL, LED_PRIO_ERROR } led_prio_t;
```
Arbiter는 소스별 요청 중 `max(prio)` 선택. 동일 prio 충돌 시 FIFO 혹은 고정 순서.

### 4.4 타이밍 정합
- 1ms tick 기반 유지(현재와 동일). `on_ms`/`period_ms` 필드로 제어하면 스펙 표와 1:1 매칭 가능.
- 60601-1 근거 (180/180)는 주석에 박제.

### 4.5 마이그레이션 단계
1. `led_pattern_desc_t` + 공통 엔진 도입 (기존 함수는 래퍼로 유지 → 회귀 방지).
2. 스펙 표 패턴을 테이블에 먼저 추가 (`en__LED_IND_*` enum들 switch에 연결).
3. `systemControl.c`의 `Led_Pattern = X` 직접 대입을 `led_request(src, state)`로 교체.
4. 우선순위 Arbiter 도입 후, 기존 enum 중복(`Battery*`, `Stimulation*_battery*`) 정리.
5. dead enum/함수 제거, `TDC_LED_IND_STATE_*` 또는 `EN__LED_PATTERN` 중 하나로 통일.

### 4.6 검증
- 단위 테스트(`tests/`): 각 패턴이 지정 on/period를 내는지 tick 시뮬레이션으로 체크.
- 보드 시험: 오실로스코프로 GPIO 하강/상승 측정 → 스펙 180/180, 1100/1100 오차 ±5% 이내.
- 우선순위 시험: 강제로 다중 상태 주입(에러+BLE+배터리<10%) → 에러 표시 유지 확인.

---

## 5. 열린 이슈(확인 필요)

1. 배터리 충전 중 LED는 스펙상 **단일 빨강 느린 점멸**. 현재 구현된 "충전 레벨 0~100% dot-count" 표시와 **어느 쪽을 유지할지** 정책 결정 필요.
2. "사용 준비(녹색 ON)"의 조건인 "배터리 80%↑"가 실제 사용 시나리오와 맞는지(에너지 여유 기준) 하드웨어팀 확인 필요.
3. `**` 주석대로 BLE 인증 시간이 180ms 미만이면 시각화가 불가능 → 최소 표시 시간(latch) 도입 여부 결정 필요(예: 최소 500ms 보장).
4. `LED_IS_ACTIVELOW` / `LED_B_pin_CFX_test` 매크로의 보드별 정의가 현행 Board_OTE_ver1_5 기준으로 맞는지 확인 필요.
5. 스펙에 있는 "꺼져있음" 상태는 전원 OFF와 같은지, 저전력 대기와 같은지 정의 필요.

---

## 6. 요약

- 현재 LED 모듈은 **패턴별 중복 함수 + 단일 전역 변수 덮어쓰기** 구조 → 스펙의 우선순위/타이밍을 수용하기 어려움.
- 스펙 대비 타이밍 불일치(180/180, 1100/1100), OTA 미구현, 우선순위 부재가 핵심 갭.
- 수정 방향: **(1) 패턴 디스크립터 테이블 + 공통 엔진**, **(2) 소스별 요청 + 우선순위 Arbiter**, **(3) dead enum 정리**.
- 마이그레이션은 래퍼로 감싸 회귀 없이 단계적 교체 가능.
