---
name: 04_정독_userguide운영
purpose: IQS323 User Guide v1.1 정독 — GUI/스트리밍 운영 절차를 펌웨어 I2C 제어 시퀀스로 매핑하고, v3 아키텍처 대비 시사점 정리
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, userguide, i2c, streaming, ack-reset, soft-reset, ati, reseed, system-status, filter-beta]
---

# 04 정독 — IQS323 User Guide v1.1 운영 절차 매핑

**TL;DR**: User Guide §2.5~2.6의 Streaming 통신 개시·ACK Reset 절차, §3.5 Command Buttons(Ack Reset·Soft Reset·ATI All·Reseed), §3.6 System Status 비트, §4 Device Setup(ATI·Filter Beta) 전반을 정독하여, GUI 절차를 펌웨어 I2C 시퀀스로 매핑하고 현 tdc_drv_iqs323.c 구현과 대조한다.

---

## 1. §2.5 Streaming 통신 개시 — 펌웨어 매핑

[자료 §2.5] "Click on 'Start Streaming' to initiate communications with the IQS323."

GUI가 Start Streaming을 누르면 내부적으로:
1. I²C 주소 자동 탐색 (IQS323은 여러 I²C 주소 변종 존재)
2. Device version 정보 읽기
3. 설정·스트리밍 확인 또는 오류 보고

**펌웨어 매핑**: 펌웨어에서는 GUI의 "Start Streaming"에 해당하는 별도 단계가 없다. IQS323은 전원 인가 후 자동으로 운전을 시작하며, 펌웨어는 MCLR 리셋 후 RDY 핀 모니터링을 시작하는 것이 이에 상응한다. I²C 주소 탐색은 `TDC_DRV_IQS323_SLAVE_ADDR` 고정으로 처리되어 있다.

[자료 §2.5] 전원 상태·I²C 주소·버전·설정·스트리밍 확인을 순차 보고한다고 명시.

---

## 2. §2.6 Acknowledge Reset — 펌웨어 매핑 (핵심)

[자료 §2.6] "Click on the red text button 'ACK Reset' to clear the reset event flag. The 'ACK Reset' text should change colour to black, indicating successful reset acknowledgement."

[자료 §2.6] "The IQS323 starts in streaming mode... The default settings are not an appropriate baseline for a production application."

### 의미 정리

- **ACK Reset**: IC 파워온 또는 리셋 후 반드시 수행해야 하는 필수 첫 단계.
- [자료 §3.5.1] "The 'Ack Reset' button clears the IQS323's reset flag by writing the Acknowledge Reset bit to the IC. **This should be the first step after powering on any Azoteq IQS-device.** On start-up, the IC will set its reset flag to indicate that a reset event has occurred."
- [자료 §3.6.1] System Status의 "Device Reset" 비트 — "A reset event has occurred, and all settings have been reset to defaults."

### 현 펌웨어 구현 대조

현 `tdc_drv_iqs323_apply_settings()` 내부:
```
ack_reset_event()  → write_register(SYSTEM_CONTROL, 0x01, 0x00)
confirm_reset_event() → System Status의 reset_event 비트가 소거될 때까지 폴링
```

- **ACK Reset 비트**: SYSTEM_CONTROL 레지스터 LSB bit[0]=1 write.
- ACK 후 System Status의 `reset_event`가 소거됨을 `confirm_reset_event()`로 검증.
- **자료 권고 순서와 일치**: 파워온(MCLR) → ACK Reset → 설정 write 순서를 펌웨어가 올바르게 따르고 있다.

> [!NOTE]
> ACK Reset 전까지 IC는 "Reset 발생 상태"를 유지하며, 이 상태에서도 I²C 통신 윈도우를 제공한다([자료 §2.6] 묵시적 — GUI가 스트리밍을 먼저 시작하고 ACK Reset을 별도 버튼으로 제공하는 구조에서 확인). 현 `wait_auto_ati_done()` 주석("Reset Event가 SET된 동안에는 ATI 중에도 통신 윈도우가 제공되므로 ACK Reset 전에 호출해야 한다")과 일치한다.

---

## 3. §3.5.2 Soft Reset

[자료 §3.5.2] "The 'Soft Reset' button issues a command to the IQS323 to perform a soft reset. **This can be used to clear any configured settings back to their default values.**"

### 의미 정리

- Soft Reset = 설정을 IC 기본값으로 되돌리는 SW 명령.
- 실행 후 반드시 ACK Reset + 재설정 write가 필요.
- [추정] SYSTEM_CONTROL 레지스터의 Soft Reset 비트를 write하는 것으로 추정되나, 자료에 레지스터 주소·비트 위치는 [자료 미명시].

### 현 펌웨어 대조

현 펌웨어는 Soft Reset을 사용하지 않는다. MCLR 하드 리셋(`mclr_reset()`)으로 대신하므로, 설정이 항상 기본값으로 초기화된 후 재설정 시퀀스가 실행된다. Soft Reset은 IC를 물리적으로 재시작하지 않아도 설정을 초기화할 수 있는 대안이나, 현재 구조(부팅 = WDT 재부팅)에서는 불필요하다.

---

## 4. §3.5.3 ATI All — 펌웨어 매핑 (핵심)

[자료 §3.5.3] "The 'ATI All' button **writes the Re-ATI command** to the IQS323. The ATI routine is a calibration algorithm on the IC that will recalibrate all the sensors to their target or reference counts."

[자료 §3.5.3] "Once ATI is complete, **the GUI reads all the IQS323 settings to update any parameters that the ATI routine may have changed.**"

### 의미 정리

1. Re-ATI 명령(SYSTEM_CONTROL 레지스터의 re_ati 비트 write) → IC가 전 채널 ATI 실행.
2. ATI 완료 후 IC가 ATI 관련 레지스터(MULT/COMP 등)를 자동 갱신.
3. GUI는 ATI 완료 후 레지스터를 다시 읽어 변경값을 확인.

### ATI 완료 판정

[자료 §3.6.1]:
- **ATI Active**: ATI 진행 중.
- **ATI Error**: ATI 실패 (하나 이상의 채널 캘리브레이션 실패).
- **ATI Event**: ATI 완료, 일부 캘리브레이션 값이 갱신되었을 수 있음.

### 현 펌웨어 구현 대조

```c
re_ati_trigger()     → SYSTEM_CONTROL.re_ati = 1 write
wait_re_ati_done()   → System Status 폴링: ati_error 또는 ati_event 확인
```

- 운용 모드에서는 Re-ATI를 실행하지 않고 사전 측정된 MULT/COMP 고정값을 직접 write(ATI Mode=Disabled).
- `TDC_DRV_IQS323_ATI_DUMP_ENABLE` 모드에서만 Re-ATI 실행 후 DUMP.
- **자료와의 차이**: 자료 절차는 ATI 후 레지스터를 읽어 갱신값 확인을 권고. 현 펌웨어 운용 모드는 ATI 자체를 Disabled로 차단하므로 해당 단계가 없다. 이는 의도된 v3 설계(고정 보상값으로 ATI 의존성 제거)와 일치한다.

---

## 5. §3.5.4 Reseed — 펌웨어 매핑 (핵심)

[자료 §3.5.4] "The 'Reseed' command can be used to **update the LTA of the ProxFusion channel by setting it equal to the counts**. Note that the **Reseed command may trigger an ATI routine if the resulting LTA is significantly different from the target**."

### 의미 정리

- Reseed: LTA를 현재 counts 값으로 강제 동기화.
- **중요**: Reseed 후 LTA가 ATI target과 크게 다르면 ATI가 자동 트리거될 수 있다.
- 이는 미해결_3(드리프트 자동보정) 및 미해결_1(ATI 실패 조건)과 직결.

### 현 펌웨어 구현 대조

```c
// tdc_drv_iqs323_apply_settings() 운용 모드 말미:
write_register(SYSTEM_CONTROL, 0x08, 0x00)  // Reseed

// tdc_drv_iqs323_reseed() — 절전 진입 시:
write_register(SYSTEM_CONTROL, 0x08, 0x00)  // Reseed (0x08 = Reseed 비트)
```

- 운용 모드 초기화 말미와 절전 진입 시에 Reseed 사용 중.
- **위험 시나리오**: 자료에 따르면 Reseed 후 LTA가 ATI target과 크게 다를 경우 ATI 자동 실행. 현 펌웨어는 ATI Mode=Disabled + CH timeout Disable 이중 차단이므로, Reseed가 ATI를 트리거하더라도 차단된다[추정]. 단 ATI Mode=Disabled인 상태에서 Reseed가 ATI를 트리거하는지 여부는 [자료 미명시].

---

## 6. §3.6.1 System Status 비트 전체 정리

[자료 §3.6.1] System Status 레지스터에서 읽는 이벤트:

### Device Status

| 비트명 | 의미 | 현 펌웨어 사용 |
|---|---|---|
| ATI Active | ATI 진행 중 | `is_auto_ati_done_single_read()` — ati_active==0 확인 |
| ATI Error | 하나 이상 채널 캘리브레이션 실패 | `tdc_drv_iqs323_read_status()` — ati_error 플래그 반환 |
| Device Reset | 리셋 발생, 설정 기본값으로 초기화 | `confirm_reset_event()` — reset_event 소거 확인 |

### Touch-Prox Status

| 비트명 | 의미 | 현 펌웨어 사용 |
|---|---|---|
| CH0~CH2 Touch/Prox | 각 채널 근접·터치 상태 | `tdc_drv_iqs323_read_status()` — ch0_touch 비트 확인 |

### Current Power Mode

| 비트명 | 의미 | 현 펌웨어 사용 |
|---|---|---|
| Power Mode | 현재 전력 모드 | [자료 미명시 세부] — 현 펌웨어에서 직접 읽지 않음 |

### Events (RDY 핀 트리거 조건)

| 비트명 | 의미 | 현 펌웨어 활성화 |
|---|---|---|
| Prox Event | Prox 상태 변화 | 비활성화 (`EVENT_DISABLE`) |
| Touch Event | Touch 상태 변화 | **활성화** (`EVENT_ENABLE`) |
| Slider Event | 슬라이더 제스처 감지 | 비활성화 |
| Power Event | 전력 모드 변화 | 비활성화 |
| ATI Event | ATI 완료, 캘리브레이션값 갱신 가능 | **활성화** (`EVENT_ENABLE`) |

> [!NOTE]
> 현 펌웨어 `events_enable()`에서 prox_event=Disable, touch_event=Enable, ati_error=Enable, ati_event=Enable. Power/Slider는 비활성. 이 구성은 RDY 핀이 Touch 이벤트와 ATI 이벤트 시에만 LOW가 되도록 설정하여 불필요한 통신 윈도우 개방을 줄인다.

---

## 7. §4.1 Power and System Settings

[자료 §4.1] "Event reporting is possible when the I²C interface is enabled and at least one of the event bits is enabled. **As shown in Figure 4.1, all event bits are disabled by default.**"

### 시사점

기본값에서 모든 이벤트 비트가 비활성화되어 있다. 현 펌웨어는 `events_enable()`에서 touch·ati_error·ati_event를 명시적으로 활성화하므로 올바르게 처리되고 있다.

[자료 §4.1] report rate, power mode timeout, channel event timeout, ultra-low power 설정도 이 탭에서 구성.

**channel event timeout**: stuck-touch 시 자동 re-ATI를 트리거하는 타이머. 현 펌웨어는 `SYSTEM_CONTROL MSB bit[0..2]`로 CH0~CH2 timeout 비활성화(ATI Mode=Disabled와 이중 방어). [자료 §4.1]에서 "channel event timeout" 언급은 Power and System Settings 범위임을 확인.

---

## 8. §4.2 Channel Sensor Settings

[자료 §4.2] "sensing mode, conversion frequency, channel pin selection, and other channel settings."

현 펌웨어 관련 설정:
- `sensor_setup()` — CRX1 ESD 더미채널(CalCap), Sensor2 비활성, Sensor0 CTx0 활성.
- [자료 §4.2]는 GUI 탭 설명이며 레지스터 세부는 datasheet 참조 지시. 세부 비트 정의는 [자료 미명시].

---

## 9. §4.3 Channel ATI Settings — 핵심

[자료 §4.3] "ATI mode, ATI resolution, ATI band, ATI base and target, mirror values and compensation values."

[자료 §4.3 수식]:
```
ATI Target = Actual ATI Base × (ATI Resolution Factor / 16)
```

[자료 §4.3] "Set the ATI base and target values according to the sensitivity requirement. Refer to the IQS323 Datasheet."

### 현 펌웨어 대조

현 펌웨어 고정 보상값(ATI Mode=Disabled):
- `ATI_SETUP_LSB = 0x08`: bits[2:0]=000 → ATI Mode=Disabled, bits[5:3]=001 → ATI Band, bits[7:6]=ATI Resolution Factor 등.
- `ATI_SETUP_MSB = 0x04`: ATI Base 관련.
- `ATI_MULT`, `ATI_COMP`: 보드 변종별 사전 측정값.

[자료 §4.3]에서 ATI Target 계산식 제공. 현 설계에서는 ATI Mode=Disabled이므로 IC가 이 수식을 사용해 자동 보상값을 구하지 않고, 펌웨어가 사전 측정한 MULT/COMP를 직접 write한다.

---

## 10. §4.4 Channel UI Settings

[자료 §4.4] "channel proximity and touch thresholds, debounce values, and hysteresis settings... **reference and follower channel settings can also be configured**."

### 시사점

- Reference·Follower 채널 설정: 자료에서 언급하나 세부 메커니즘은 [자료 미명시]. C52 DC-block 회로 제약으로 현 펌웨어는 Reference 모드(`TDC_TOUCH_CRX1_REF_ENABLE`)를 선택적으로 지원하나 기본 비활성.
- Touch threshold·hysteresis: `touch_settings_impl()` 에서 설정. 미해결_2(환경 드리프트 정량)와 관련해 threshold 여유를 어떻게 잡을지에 영향.

---

## 11. §4.6 Release UI Settings

[자료 §4.6] "activation settling threshold, release delta percentage, and delta snapshot sample delay."

현 펌웨어는 Release UI를 별도로 설정하지 않는다. 현 구조(LTA 추적 기반 delta 비교)에서 release는 touch threshold 해소로 자연 처리되므로 [자료 미명시] 수준의 기능.

---

## 12. §4.7 Filter Beta Settings — 핵심

[자료 §4.7] "**counts filter beta, LTA filter beta, LTA fast filter beta, and fast filter band**."
"Filter beta settings include **normal power and ultra-low power** filter beta settings."

### 의미 정리

- **Counts filter beta**: 측정값(raw counts) IIR 필터 계수. 클수록 느린 추적(노이즈 강함, 반응 느림).
- **LTA filter beta**: Long Term Average IIR 필터 계수. 클수록 드리프트 추적이 느림.
- **LTA fast filter beta**: 빠른 환경 변화 시 LTA가 더 빠르게 추적하도록 하는 보조 beta. fast filter band 이내의 delta에서 활성화[추정, 자료 세부 메커니즘 미명시].
- **fast filter band**: 빠른 필터로 전환되는 delta 기준 경계.
- **운전 모드별 분리**: Normal Power와 ULP(Ultra-Low Power)에서 각각 별도 beta 값 설정 가능.

### 현 펌웨어 대조

현 펌웨어(`tdc_drv_iqs323_apply_settings()`)는 Filter Beta 레지스터를 명시적으로 write하지 않는다 — IC 기본값(reset 상태)을 그대로 사용하는 것으로 보인다.

**미해결_3 관련 시사점**: LTA filter beta가 드리프트 자동보정의 핵심 파라미터. 기본값으로 충분한지, 또는 튜닝이 필요한지는 환경 실측 없이 판단 불가([자료 미명시] — User Guide는 파라미터 존재만 안내, 권고값 없음).

---

## 13. §5 Reference Design — 회로 대조

[자료 §5.1] EV01 General-Purpose Stamp 회로도:
- CRX0, CRX1, CRX2 각각 470Ω 직렬 저항 연결.
- RDY/MCLR 핀 공유 (LK1 점퍼로 PROG/RDY 선택).
- 4.7kΩ I²C 풀업(SDA, SCL, RDY/MCLR).

[자료 §5.2.2] Touch Buttons Module:
- CRX0~CRX2 470Ω 직렬.
- MCLR/RDY 핀, 4.7kΩ 풀업.

**현 회로 제약과 대조**:
- 레퍼런스 설계는 C52 DC-block 없음 — 자료 회로가 Reference UI 드리프트 보정을 전제로 설계되었을 가능성[추정].
- CRX1 ESD 더미채널 구성은 레퍼런스 설계에 없는 커스텀 구현.
- RDY/MCLR 핀 공유 구조(LK1)와 현 펌웨어 `mclr_reset()` 구현 방식(GPIO OUTPUT→LOW→INPUT)이 일치.

---

## 14. 펌웨어 I2C 초기화 시퀀스 종합 (자료 기준 vs 현 구현)

### 자료 권고 순서 (GUI 절차에서 역산)

```
1. 전원 인가 (Power On)
2. IC 자체 Auto-ATI 실행 (내부)
3. ACK Reset → System Status reset_event 소거 확인
4. 설정 write (Channel Sensor, ATI, UI, Filter Beta 등)
5. [선택] ATI All (Re-ATI 명령) → ATI 완료 대기 → 레지스터 읽기
6. [선택] Reseed → LTA 동기화
7. 스트리밍 운전 (RDY 핀 이벤트 기반 폴링)
```

### 현 펌웨어 실제 순서 (`tdc_drv_iqs323_apply_settings()` 운용 모드)

```
1. MCLR 리셋 (mclr_reset) — 전원 인가와 동등 효과
2. IC Auto-ATI 완료 대기 (is_auto_ati_done_single_read 상태머신 폴링)
3. ACK Reset → reset_event 소거 확인 (ack_reset_event + confirm_reset_event)
4. 설정 write:
   a. sensor_setup() — 채널 구성
   b. touch_settings() — Touch threshold/hysteresis
   c. prox_settings() — Prox threshold
   d. events_enable() — 이벤트 활성화
5. write_ati_compensation() — ATI Mode=Disabled + 고정 MULT/COMP write (Re-ATI 없음)
6. Reseed (SYSTEM_CONTROL 0x08) — LTA 동기화
7. CH timeout 비활성화 (SYSTEM_CONTROL MSB 0x07)
8. 스트리밍 운전 개시
```

**자료 대비 차이점 및 평가**:

| 항목 | 자료 절차 | 현 펌웨어 | 평가 |
|---|---|---|---|
| 파워온 방법 | 전원 인가 | MCLR 리셋 (GPIO 제어) | 동등 — MCLR은 full reset |
| ACK Reset 위치 | 첫 단계 | Auto-ATI 완료 후 | [추정] Auto-ATI 중 통신 허용이므로 선후 순서 유연할 수 있음 |
| ATI 실행 | Re-ATI All 권고 (GUI) | ATI Mode=Disabled, 고정 보상값 | v3 의도적 차이 — ATI 의존성 제거 |
| Filter Beta | GUI에서 설정 | 기본값 사용 (미설정) | 잠재적 튜닝 포인트 |
| Reseed | LTA 동기화용 | 초기화 말미 + 절전 진입 시 | 자료 의도와 일치 |

---

## 15. 미해결 항목별 기여

### 미해결_1: ATI가 실패하는 정확한 조건·경계

[자료 §3.6.1] ATI Error: "The IQS323 failed to calibrate one or more channels correctly." — 실패 조건 상세는 [자료 미명시]. User Guide는 현상(ATI Error 비트)만 정의. 원인·경계 정량은 datasheet 또는 AZD004 참조 필요.

### 미해결_2: 환경 드리프트 카운터 변화 정량

User Guide는 Filter Beta 파라미터(counts filter beta, LTA filter beta)의 존재와 Normal/ULP별 분리를 언급하나, 환경별 변화 수치는 [자료 미명시]. LTA filter beta 튜닝으로 드리프트 추적 속도 조절 가능함을 확인.

### 미해결_3: 드리프트 자동보정(Re-ATI 외) 메커니즘

[자료 §3.5.4] LTA filter beta + Reseed가 드리프트 보정의 주요 메커니즘임을 간접 확인. Reseed는 LTA를 즉시 현재 counts로 설정하는 수동 트리거. 자동 드리프트 보정은 LTA IIR 필터(slow tracking)가 담당[추정, 자료 명시적 설명은 AZD004에 위임]. [자료 §3.1.1] "The LTA is the Long Term Average of the counts signal. **It tracks slow variations in the environment**, and is used as a reference to detect movement; refer to AZD004 for more details."
