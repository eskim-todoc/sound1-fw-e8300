---
name: 05_정독_arduino코드
purpose: Arduino 예제코드(IQS323.cpp·.ino·EV_KIT.h·addresses.h)를 정독해 초기화 시퀀스·RDY 윈도우·SW reset·레지스터 기본값·상태 비트 읽기를 추출하고, 현 tdc_drv_iqs323.c와 대조한다
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, arduino, 예제코드, 시퀀스대조, ev-kit]
---

# 05 정독 — Arduino 예제코드

**TL;DR**: Arduino 예제코드는 POR→제품확인→리셋확인→SW리셋→설정쓰기→ReATI→ATI대기→데이터읽기→이벤트모드 9단계 시퀀스를 인터럽트 기반 RDY 윈도우로 구동한다. 현 드라이버와 가장 큰 차이는 (1) 예제는 ATI Mode=Full로 ReATI 실행하나 현 드라이버는 ATI Mode=Disabled+고정 보상값으로 대체, (2) 예제는 런타임 리셋 감지 시 전체 재초기화 루프 복귀, 현 드라이버는 WDT 재부팅으로 대응, (3) 예제에는 자동 드리프트 보정 메커니즘이 없음이다.

---

## 1. begin() 초기화·run() 메인루프 시퀀스

### 1-1. begin() [IQS323.cpp §begin]

```
Wire.begin() + setClock(400kHz)
→ iqs323_state.state = IQS323_STATE_START
→ iqs323_state.init_state = IQS323_INIT_VERIFY_PRODUCT
```

begin()은 I2C 초기화와 상태 변수 세팅만 수행한다. 실제 통신·초기화는 run() 내부의 init()이 담당한다.

인터럽트 등록: `attachInterrupt(RDY_PIN, iqs323_ready_interrupt, CHANGE)` — RDY가 LOW로 내려갈 때 `iqs323_deviceRDY = true`, HIGH로 올라갈 때 `false`. [IQS323.cpp §iqs323_ready_interrupt]

### 1-2. init() 내부 9단계 FSM [IQS323.cpp §init]

| 단계 | init_state | 동작 |
|---|---|---|
| 1 | VERIFY_PRODUCT | 제품번호(0x00) 읽기 → IQS323 확인 |
| 2 | READ_RESET | System Status(0x10) 읽기 → reset bit 확인 |
| 3 | CHIP_RESET | (리셋 비트 없으면) SW_Reset → 100ms 대기 → READ_RESET 재진입 |
| 4 | UPDATE_SETTINGS | writeMM() — 전체 메모리맵 12그룹 연속 쓰기 |
| 5 | ACK_RESET | System Control(0xC0) ACK_RESET_BIT 세팅 |
| 6 | ATI | ReATI() — System Control RE_ATI_BIT 세팅 |
| 7 | WAIT_FOR_ATI | readATIactive() 폴링 — ATI_ACTIVE_BIT==0 될 때까지 |
| 8 | READ_DATA | queueValueUpdates() — SYSTEM_STATUS+GESTURES+COORDS+CH0~CH2 Counts/LTA 18바이트 일괄 |
| 9 | ACTIVATE_EVENT_MODE | setEventMode() — System Control EVENT_MODE_BIT 세팅 |

모든 단계는 `if(iqs323_deviceRDY)` 가드 안에서만 실행된다 — RDY 윈도우가 열릴 때만 진행.

### 1-3. run() 메인루프 FSM [IQS323.cpp §run]

```
STATE_START → STATE_INIT(init()반복) → STATE_RUN
STATE_RUN: iqs323_deviceRDY 시 queueValueUpdates() → STATE_CHECK_RESET
STATE_CHECK_RESET: reset bit 확인 → 리셋이면 STATE_START 복귀, 아니면 new_data_available=true → STATE_RUN
STATE_SW_RESET: iqs323_deviceRDY 시 SW_Reset() → STATE_RUN
```

메인루프(loop())에서 `iqs323.run()` 호출 후 `new_data_available` 플래그 확인하는 구조다.

---

## 2. force_I2C_communication · RDY 윈도우 처리 방식

### 2-1. force_I2C_communication [IQS323.cpp §force_I2C_communication]

```c
if (!iqs323_deviceRDY)  // RDY가 HIGH(닫힘) 상태일 때만
{
    Wire.write(0xFF);   // address 0xFF write → IQS323 RDY 윈도우 강제 열기
    Wire.endTransmission(STOP);
    iqs323_deviceRDY = false;  // 명시 클리어 (인터럽트가 올라오기 전 선행 클리어)
}
```

0xFF 주소에 1바이트 write를 보내면 IQS323이 통신 윈도우(RDY LOW)를 연다. 현 드라이버의 `force_window_open()`과 동일 메커니즘이다. [IQS323.cpp §force_I2C_communication]

### 2-2. RDY 윈도우 경계 처리 [IQS323.cpp §readRandomBytes, §writeRandomBytes]

STOP 조건 전송 후 `iqs323_deviceRDY = false` 명시 클리어 — 윈도우가 닫힐 때까지 대기하지 않고 플래그로 상태 관리한다.

readRandomBytes 내 `do { requestFrom() } while (Wire.available()==0)` — 응답 지연 시 재시도 루프 포함.

---

## 3. SW reset · power mode 전이 처리

### 3-1. SW_Reset [IQS323.cpp §SW_Reset]

```c
readRandomBytes(0xC0, 2, buf, RESTART);  // 현재 SYSTEM_CONTROL 보존 읽기
buf[0] = setBit(buf[0], IQS323_SW_RESET_BIT);
writeRandomBytes(0xC0, 2, buf, STOP);    // SW_RESET_BIT 세팅
```

Read-Modify-Write 패턴 — 다른 비트 보존 후 SW_RESET_BIT만 세팅. 리셋 후 100ms 대기 [IQS323.cpp §CHIP_RESET 단계].

### 3-2. Power Mode 읽기 [IQS323.cpp §get_PowerMode]

SYSTEM_STATUS 레지스터(0x10) MSB[bit1:0] = POWER_EVENT_BIT_1:POWER_EVENT_BIT_0:

| 값 | 모드 |
|---|---|
| 0b00 | NORMAL_POWER |
| 0b01 | LOW_POWER |
| 0b10 | ULP |
| 0b11 | HALT |

Power mode는 System Status에서 읽기만 한다. Power mode 전환은 Power Mode Timeout(0xC5) 레지스터가 제어하며, 예제에서는 `POWER_MODE_TIMEOUT = 0x07D0 (= 2000ms)`로 설정된다 [EV_KIT.h §POWER_MODE_TIMEOUT].

### 3-3. Event Mode [IQS323.cpp §setEventMode]

SYSTEM_CONTROL(0xC0) EVENT_MODE_BIT 세팅 — 이후 IQS323은 이벤트(터치/근접/ATI/전원) 발생 시에만 RDY를 내린다. 예제 init 마지막 단계에서 활성화된다.

---

## 4. 레지스터 write 순서 · 3-Projected-Buttons EV-Kit 기본 설정값

### 4-1. writeMM() write 순서 [IQS323.cpp §writeMM]

12그룹을 순서대로 단일 RDY 윈도우 내에서 RESTART(repeated start) 연속 전송:

```
1.  Sensor 0 Setup      0x30 ~ 0x39  (20바이트)  RESTART
2.  Sensor 1 Setup      0x40 ~ 0x49  (20바이트)  RESTART
3.  Sensor 2 Setup      0x50 ~ 0x59  (20바이트)  RESTART
4.  Channel 0 Settings  0x60 ~ 0x67  (8바이트)   RESTART
5.  Channel 1 Settings  0x70 ~ 0x77  (8바이트)   RESTART
6.  Channel 2 Settings  0x80 ~ 0x87  (8바이트)   RESTART
7.  Slider Config       0x90 ~ 0x9B  (18바이트)  RESTART
8.  Gesture Setup       0xA0 ~ 0xAD  (14바이트)  RESTART
9.  Filter Betas        0xB0 ~ 0xB9  (10바이트)  RESTART
10. Power/System        0xC0 ~ 0xCB  (12바이트)  RESTART
11. General Settings    0xD0 ~ 0xD9  (10바이트)  RESTART
12. I2C Setup           0xE0         (1바이트)   STOP (마지막)
```

순서 핵심: Sensor → Channel → Slider → Gesture → Filter → Power → General → I2C.

### 4-2. 3-Projected-Buttons EV-Kit 핵심 기본값 [EV_KIT.h]

#### ATI 설정 (Sensor 0 기준)

| 레지스터 | 주소 | 값 | 의미 |
|---|---|---|---|
| ATI_SETUP_0 (LSB) | 0x36 | 0x8C | ATI Band·Resolution·**ATI Mode bits** |
| ATI_SETUP_1 (MSB) | 0x36 | 0x02 | |
| ATI_BASE_0 (LSB) | 0x37 | 0xC8 | ATI Target 200 |
| ATI_BASE_1 (MSB) | 0x37 | 0x00 | |
| ATI_COARSE (S0) | 0x38 | 0x21 | Multiplier/Coarse 초기값 |
| ATI_FINE (S0) | 0x39 | 0x69 (S0), 0x5C (S1), 0x69 (S2) | Compensation/Fine 초기값 |

`ATI_SETUP_LSB=0x8C`: bits[2:0]=100 = **ATI Mode=Full** — ReATI가 ATI_COARSE/FINE 모두 자동 조정한다. 이것이 예제의 기본 모드이고, **현 드라이버는 0x08(Mode=Disabled)로 교체** 후 고정값 직접 write한다. [현 드라이버 §write_ati_compensation]

#### 필터 설정 [EV_KIT.h §Filter Betas]

| 레지스터 | 주소 | 값 | 의미 |
|---|---|---|---|
| NP_COUNTS_FILTER | 0xB0 LSB | 0x02 | Normal Power IIR beta(Counts) |
| LP_COUNTS_FILTER | 0xB0 MSB | 0x02 | Low Power IIR beta(Counts) |
| NP_LTA_FILTER | 0xB1 LSB | 0x05 | Normal Power LTA beta |
| LP_LTA_FILTER | 0xB1 MSB | 0x05 | Low Power LTA beta |
| NP_LTA_FAST_FILTER | 0xB2 LSB | 0x03 | NP LTA Fast beta |
| FAST_FILTER_BAND | 0xB4 | 0x001E | Fast Filter 임계 delta |

#### Power / System 설정 [EV_KIT.h §Power mode & System Settings]

| 설정 | 값 | 환산 |
|---|---|---|
| SYSTEM_CONTROL | 0x50 | bit6=Streaming Mode(이벤트모드 아님), bit4=EventMode 아님 [추정: 초기값, 이후 setEventMode로 덮어씀] |
| NP_REPORT_RATE | 0x0010 | 16ms (Normal Power) |
| LP_REPORT_RATE | 0x003C | 60ms (Low Power) |
| ULP_REPORT_RATE | 0x00A0 | 160ms (Ultra Low Power) |
| HALT_REPORT_RATE | 0x0BB8 | 3000ms (Halt) |
| POWER_MODE_TIMEOUT | 0x07D0 | 2000ms — 이 시간 비활동 후 다음 Power 단계로 전환 |

#### Prox/Touch 임계값 설정 [EV_KIT.h §Channel 0~2 Settings]

| 설정 | 주소 | 값 | 의미 |
|---|---|---|---|
| CH0_PROX_THRESHOLD | 0x61 LSB | 0x14 | Prox 임계 계수 20 |
| CH0_PROX_DEBOUNCE | 0x61 MSB | 0x44 | 진입/해제 디바운스 4회 |
| CH0_TOUCH_THRESHOLD | 0x62 LSB | 0x28 | Touch 임계 계수 40 |
| CH0_TOUCH_HYSTERESIS | 0x62 MSB | 0x00 | 히스테리시스 없음 |

CH0·CH1·CH2 모두 동일한 임계값 적용.

#### Events Enable 설정 [EV_KIT.h §General Settings]

`EVENTS_ENABLE = 0x0B` → bits: touch_event·ati_event·prox_event 활성 (bit1:touch, bit3:ati, bit0:prox) [추정: 비트 매핑은 User Guide 확인 필요].

#### I2C Timeout [EV_KIT.h]

`I2C_TIMEOUT = 0x00C8 = 200ms` — 200ms 동안 I2C 통신 없으면 IQS323이 통신 윈도우 강제 종료.

---

## 5. System Status · touch/prox state 비트 읽기

### 5-1. queueValueUpdates — 일괄 read [IQS323.cpp §queueValueUpdates]

매 RDY 윈도우에서 주소 0x10(SYSTEM_STATUS)부터 18바이트 순차 읽기:

```
0x10 SYSTEM_STATUS[0:1]   (2 bytes)
0x11 GESTURES[0:1]        (2 bytes)
0x12 SLIDER_COORDINATES   (2 bytes)
0x13 CH0_COUNTS           (2 bytes)
0x14 CH0_LTA              (2 bytes)
0x15 CH1_COUNTS           (2 bytes)
0x16 CH1_LTA              (2 bytes)
0x17 CH2_COUNTS           (2 bytes)
0x18 CH2_LTA              (2 bytes)
```

18바이트 일괄 burst read — SYSTEM_STATUS부터 CH2_LTA까지 연속 주소라 가능.

### 5-2. SYSTEM_STATUS 비트 레이아웃 [IQS323.cpp §channel_touchState, §channel_proxState, §get_PowerMode, §checkReset, §readATIactive]

**SYSTEM_STATUS[0] (LSB)**:

| 비트 | 이름 | 용도 |
|---|---|---|
| IQS323_ATI_ACTIVE_BIT | ATI 진행 중 플래그 | readATIactive() |
| IQS323_SHOW_RESET_BIT | 리셋 발생 플래그 | checkReset() |
| IQS323_SLIDER_EVENT_BIT | 슬라이더 이벤트 | getSliderEvent() |

**SYSTEM_STATUS[1] (MSB)**:

| 비트 | 이름 | 용도 |
|---|---|---|
| IQS323_POWER_EVENT_BIT_0 | Power Mode bit0 | get_PowerMode() |
| IQS323_POWER_EVENT_BIT_1 | Power Mode bit1 | get_PowerMode() |
| IQS323_CH0_TOUCH_BIT | CH0 Touch | channel_touchState() |
| IQS323_CH1_TOUCH_BIT | CH1 Touch | channel_touchState() |
| IQS323_CH2_TOUCH_BIT | CH2 Touch | channel_touchState() |
| IQS323_CH0_PROX_BIT | CH0 Prox | channel_proxState() |
| IQS323_CH1_PROX_BIT | CH1 Prox | channel_proxState() |
| IQS323_CH2_PROX_BIT | CH2 Prox | channel_proxState() |

터치·근접 비트는 모두 SYSTEM_STATUS[1](MSB)에서 읽는다. 현 드라이버의 `tdc_drv_iqs323_read_status()`도 동일 레지스터를 읽는다 [현 드라이버 §tdc_drv_iqs323_read_status].

---

## 6. 현 tdc_drv_iqs323.c 대조 — 시퀀스 차이·누락·추가

### 6-1. 초기화 시퀀스 비교

| 항목 | Arduino 예제 | 현 드라이버 (tdc_drv_iqs323.c) |
|---|---|---|
| 리셋 방식 | POR 후 SW_Reset으로 클린 부팅 보장 | MCLR 하드 리셋 → Auto-ATI 1회 수행 |
| 제품 확인 | getProductNum() 읽기 | [자료 미명시] — 없음 |
| 리셋 확인 | SHOW_RESET_BIT 폴링 후 ACK | `confirm_reset_event()` 폴링 후 `ack_reset_event()` — 동일 패턴 |
| 설정 쓰기 | writeMM() — 12그룹 burst | 개별 write_register() — 필요 레지스터만 선택 쓰기 |
| ATI 방식 | ReATI 실행 후 ATI_ACTIVE 폴링 대기 | Auto-ATI 대기(`wait_auto_ati_done`) → 고정 보상값 write(ATI Mode=Disabled)로 전환 |
| 이벤트 모드 | setEventMode() (EVENT_MODE_BIT) | [자료 미명시] — 예제와 같은 event mode 설정 없음 |
| 데이터 읽기 | queueValueUpdates() 18바이트 burst | read_register() 개별 호출 (필요 레지스터만) |

### 6-2. RDY 윈도우 처리 비교

| 항목 | Arduino 예제 | 현 드라이버 |
|---|---|---|
| RDY 감지 | 인터럽트(CHANGE) → iqs323_deviceRDY 플래그 | GPIO 폴링 `is_rdy_window_opened()` |
| 윈도우 강제 열기 | force_I2C_communication() — 0xFF write | force_window_open() — 0xFF write, 동일 메커니즘 |
| 윈도우 닫힘 대기 | STOP 후 iqs323_deviceRDY=false 클리어만 | wait_rdy_window_closed() — GPIO 폴링 타임아웃 포함 |
| Repeated Start | readRandomBytes에서 RESTART 명시 사용 | write_register 내부: write(addr)→wait→write(data) — 별도 윈도우 분리 방식 |

**주목**: 현 드라이버는 read_register()를 force_window→write addr→wait close→force_window→read data→wait close의 2-윈도우 구조로 구현한다. Arduino 예제는 RESTART로 single window에서 addr+data를 처리한다.

### 6-3. 런타임 리셋 감지 비교

| 항목 | Arduino 예제 | 현 드라이버 |
|---|---|---|
| 런타임 리셋 감지 | STATE_CHECK_RESET → SHOW_RESET_BIT 확인 → 재초기화 루프 복귀 | [추정] tdc_touch.c에서 ATI_ERROR / 이상 상태 시 WDT 재부팅 트리거 |
| SW_Reset 요청 | 시리얼 'r' 명령 → STATE_SW_RESET 상태로 전환, 다음 RDY에서 실행 | tdc_drv_iqs323 레이어에는 없음 — 상위 레이어(tdc_touch) 담당 |

### 6-4. 예제에 있으나 현 드라이버에 누락된 부분

1. **제품번호 확인 단계**: 예제는 제품번호 read로 IQS323 존재 확인. 현 드라이버는 생략.
2. **Event Mode 활성화**: 예제는 init 마지막에 EVENT_MODE_BIT를 세팅해 이벤트 기반 RDY 제어로 전환. 현 드라이버는 SYSTEM_CONTROL 0x07(CH timeout 비활성)만 쓰고 EVENT_MODE_BIT 설정 없음 — 따라서 현 드라이버는 streaming mode(주기적 RDY) 방식으로 동작하는 것으로 보인다 [추정: 현 드라이버가 force_window_open으로 폴링하는 이유].
3. **런타임 리셋 복구 루프**: 예제는 STATE_CHECK_RESET에서 리셋 감지 시 전체 재초기화. 현 드라이버는 WDT 재부팅으로 대체.
4. **burst read**: queueValueUpdates()의 18바이트 burst는 현 드라이버에서 채택되지 않음 — 개별 레지스터 read 방식.

### 6-5. 현 드라이버에서 추가된 부분 (예제 없음)

1. **ATI Mode=Disabled + 고정 보상값**: 예제는 ATI Mode=Full(0x8C)로 ReATI 실행. 현 드라이버는 ATI_SETUP_LSB=0x08(Disabled)로 쓰고 MULT/COMP를 사전 측정값으로 고정. 이것이 v3 핵심.
2. **Reseed**: `write_register(SYSTEM_CONTROL, 0x08, 0x00)` — RESEED_BIT 세팅으로 LTA를 현재 Counts 기준 재초기화. 예제에 없음.
3. **CH timeout 비활성화**: `write_register(SYSTEM_CONTROL, 0x00, 0x07)` — MSB bit[2:0]=111 → CH0·CH1·CH2 stuck-touch timeout 비활성. Auto-reATI 트리거 경로 이중 차단.
4. **CRX1 더미채널**: CalCap 더미 모드(sensor_setup)는 예제에 없음.
5. **절전 설정 분기**: `tdc_drv_iqs323_apply_sleep_settings()` — 절전 시 Touch Threshold=255, 고정 보상값 별도 적용. 예제에 없음.
6. **write_register 2-윈도우 read 방식**: 임베디드 환경(인터럽트 없음, 폴링)에 맞게 설계.

---

## 7. 미해결 항목 기여

### 미해결_1: ATI 실패 조건

예제코드 관점에서는 `wait_re_ati_done()` 없이 `readATIactive()` 폴링(ATI_ACTIVE_BIT==0)으로만 완료 판정한다. ATI 성공·실패 구분 비트 처리가 없다. ATI_ERROR 비트는 예제에서 읽지 않는다 — ATI 실패 정확 조건을 예제코드에서 얻을 수 없음. [자료 미명시]

단, EV-Kit 기본 ATI_SETUP_LSB=0x8C (ATI Mode=Full)에서 ATI_TARGET=200(0x00C8), COARSE/FINE 초기값이 S0=0x2169, S1=0xC15C, S2=0x2169로 제조사 EV-Kit 대상으로 맞춰져 있다. 우리 회로는 C52 DC-Block과 CRX1 더미채널 등 다른 하드웨어 조건이라 이 값의 직접 적용 불가.

### 미해결_2: 환경 드리프트 정량

예제코드에 환경 드리프트 정량 정보 없음. [자료 미명시] 단, 필터 beta 값(NP_LTA=5, NP_COUNTS=2)이 확인되었으므로 LTA 추적 속도 계산의 입력으로 사용 가능.

### 미해결_3: 드리프트 자동보정 메커니즘

예제코드에는 Re-ATI 외 별도 드리프트 자동보정(Follow UI 등) 메커니즘이 없다. [자료 미명시] LTA 필터(IIR)가 baseline을 점진 추적하는 것이 유일한 드리프트 보정이다. Reseed(0x08 bit)는 즉각 LTA=Counts 강제 재초기화이며, 예제에는 없고 현 드라이버에만 있다.

---

## 8. v3 아키텍처 관점 함의

1. **ATI Mode=Disabled 대체 근거 확인**: 예제의 ATI Mode=Full(ReATI) 대신 ATI Mode=Disabled+고정 보상값을 쓰는 현 드라이버 방향은, 예제코드가 제시하는 표준 흐름에서 의도적으로 이탈한 것임이 명확하다. 이탈 목적은 Auto-reATI 경로 차단(stuck-touch → timeout → auto-reATI → ATI_ERROR 경로). v3 설계 근거로 타당성 보강.

2. **Event Mode 미채택 함의**: 예제는 init 마지막에 EVENT_MODE_BIT를 활성화해 불필요한 RDY를 억제한다. 현 드라이버는 EVENT_MODE_BIT를 설정하지 않아 streaming mode(주기적 RDY 발생)로 동작하는 것으로 추정된다 [추정]. 이는 현 드라이버가 force_window_open()으로 매번 윈도우를 강제 열어야 하는 이유와 일치할 수 있다. EVENT_MODE_BIT 채택 시 불필요한 RDY 감소로 노이즈 영향 줄일 수 있으나, 현재 폴링 구조 변경 필요.

3. **CH timeout 비활성화**: 예제 EV-Kit.h의 EVENTS_ENABLE=0x0B에는 CH timeout 비활성화 설정이 없다. 현 드라이버가 `write_register(0xC0, 0x00, 0x07)`로 MSB[2:0]=111 추가 설정한 것은 ATI Mode=Disabled와 이중 방어임이 코드 주석으로 확인된다.

4. **Reseed 활용**: 예제코드에 없는 Reseed(0x08 bit)를 현 드라이버가 초기화 마지막과 절전 진입 시 사용한다. LTA를 현재 Counts 기준으로 즉시 재초기화하는 용도로, 고정 보상값 적용 후 LTA baseline을 올바른 노터치 기준점으로 맞추는 핵심 단계다.
