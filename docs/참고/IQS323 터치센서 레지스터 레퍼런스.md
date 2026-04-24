# IQS323 터치센서 레지스터 레퍼런스

Azoteq IQS323 ProxFusion 터치센서 IC의 레지스터 설정 참조 문서.
데이터시트 Revision v1.2 (November 2022) 기준.

> **범위**: 터치/프록시미티 이벤트 감지, ATI 캘리브레이션, 채널 설정, I2C 통신에 필요한 레지스터 정보만 수록.
> 핀맵, 전기적 특성, 패키지 사양 등은 원본 데이터시트 참조.

---

## 1. I2C 통신 기본

| 항목 | 값 |
|---|---|
| I2C 주소 (7-bit) | `0x44` |
| Write 주소 | `0x88` |
| Read 주소 | `0x89` |
| 데이터 포맷 | 8-bit 주소, 16-bit 데이터 (Little Endian, LSB first) |
| 속도 | Fast-mode-plus, 최대 1 MHz |
| 예약 주소 | `0x45` (디버그 모드, 사용 금지) |

### 1.1 RDY/IRQ 라인

- Open-drain, Active-LOW
- LOW: 통신 윈도우 열림 (데이터 준비됨)
- HIGH: 통신 윈도우 닫힘
- MCLR(하드 리셋) 기능 겸용

### 1.2 통신 모드

| 모드 | 설명 | System Control (0xC0) Bit7 |
|---|---|---|
| Streaming | 매 사이클마다 RDY 인터럽트 발생 | `0` |
| Event | 활성화된 이벤트 발생 시에만 RDY 인터럽트 | `1` |

### 1.3 Force Communication 시퀀스

Event 모드에서 설정 읽기/쓰기를 위해 강제로 통신 윈도우를 열 때 사용:

```
I2C START → Write 0x44 + ACK → Write 0xFF + ACK → twait(0.1~45ms) → RDY LOW → 통신 시작
```

### 1.4 통신 종료

- 기본: I2C STOP으로 통신 윈도우 종료
- Stop Bit Disable (0xE0 Bit0) 설정 시: `0xFF` 커맨드로 종료

### 1.5 I2C Transaction Timeout

- 레지스터 `0xD1` (16-bit, ms 단위)
- 범위: 2 ~ 230 ms, 기본값 200 ms
- 통신 윈도우가 이 시간 내에 서비스되지 않으면 RDY HIGH로 전환

### 1.6 ATI 중 통신 제한

ATI 실행 중에는 I2C 통신 비활성화. Reset Event 비트가 set된 상태에서 ATI가 실행되면 통신 윈도우가 연속 제공되어 ATI 시간이 길어짐.

---

## 2. 메모리 맵 (전체)

### 2.1 시스템 정보 (Read Only)

| 주소 | 이름 | 비고 |
|---|---|---|
| `0x00`-`0x09` | Version Details | 제품 번호, 버전 정보 |
| `0x10` | System Status | Table A.2 |
| `0x11` | Gesture Status | Table A.3 |
| `0x12` | Slider Coordinates | 16-bit 값 |
| `0x13` | CH0 Filtered Counts | 16-bit 값 |
| `0x14` | CH0 LTA | 16-bit 값 |
| `0x15` | CH1 Filtered Counts | 16-bit 값 |
| `0x16` | CH1 LTA | 16-bit 값 |
| `0x17` | CH2 Filtered Counts | 16-bit 값 |
| `0x18` | CH2 LTA | 16-bit 값 |

### 2.2 Release UI / Movement UI (Read Only)

| 주소 | 이름 |
|---|---|
| `0x20` | CH0 Activation LTA / CH1 Movement LTA |
| `0x21` | CH1 Activation LTA / CH1 Movement LTA |
| `0x22` | CH2 Activation LTA / CH2 Movement LTA |
| `0x23` | CH0 Delta Snapshot / Movement Status |
| `0x24` | CH1 Delta Snapshot |
| `0x25` | CH2 Delta Snapshot |

### 2.3 센서 설정 (Read/Write, 채널별 반복)

| 기준 주소 | Sensor 0 | Sensor 1 | Sensor 2 | 이름 |
|---|---|---|---|---|
| +0 | `0x30` | `0x40` | `0x50` | Sensor Setup |
| +1 | `0x31` | `0x41` | `0x51` | Conversion Frequency Setup |
| +2 | `0x32` | `0x42` | `0x52` | Prox Control |
| +3 | `0x33` | `0x43` | `0x53` | Prox Input and Control |
| +4 | `0x34` | `0x44` | `0x54` | Pattern Definitions |
| +5 | `0x35` | `0x45` | `0x55` | Pattern Selection & Engine Bias Current |
| +6 | `0x36` | `0x46` | `0x56` | ATI Setup |
| +7 | `0x37` | `0x47` | `0x57` | ATI Base (16-bit) |
| +8 | `0x38` | `0x48` | `0x58` | ATI Multipliers and Dividers |
| +9 | `0x39` | `0x49` | `0x59` | Compensation |

### 2.4 채널 설정 (Read/Write, 채널별 반복)

| 기준 주소 | CH0 | CH1 | CH2 | 이름 |
|---|---|---|---|---|
| +0 | `0x60` | `0x70` | `0x80` | Channel Setup |
| +1 | `0x61` | `0x71` | `0x81` | Prox Settings |
| +2 | `0x62` | `0x72` | `0x82` | Touch Settings |
| +3 | `0x63` | `0x73` | `0x83` | Follower Weight |
| +4 | `0x64` | `0x74` | `0x84` | Movement UI Settings |

### 2.5 슬라이더 설정 (Read/Write)

| 주소 | 이름 |
|---|---|
| `0x90` | Slider Setup and Calibration |
| `0x91` | Slider Calibration and Bottom Speed |
| `0x92` | Slider Top Speed |
| `0x93` | Slider Resolution (16-bit) |
| `0x94` | Enable Mask |
| `0x95` | Enable Status Pointer |
| `0x96`-`0x98` | Delta Link 0 / 1 / 2 |

### 2.6 제스처 설정 (Read/Write)

| 주소 | 이름 | 단위 |
|---|---|---|
| `0xA0` | Gesture Enable | - |
| `0xA1` | Minimum Time | ms |
| `0xA2` | Max Tap Time | ms |
| `0xA3` | Max Swipe Time | ms |
| `0xA4` | Min Hold Time | ms |
| `0xA5` | Max Tap Distance | - |
| `0xA6` | Min Swipe Distance | - |

### 2.7 필터 베타 (Read/Write)

| 주소 | 이름 |
|---|---|
| `0xB0` | Counts Filter Betas |
| `0xB1` | LTA Filter Betas |
| `0xB2` | LTA Fast Filter Betas |
| `0xB3` | Activation/Movement LTA Filter Betas |
| `0xB4` | Fast Filter Band (16-bit) |

### 2.8 시스템 제어 (Read/Write)

| 주소 | 이름 | 비고 |
|---|---|---|
| `0xC0` | System Control | Table A.30 |
| `0xC1` | NP Report Rate | ms |
| `0xC2` | LP Report Rate | ms |
| `0xC3` | ULP Report Rate | ms |
| `0xC4` | HALT Report Rate | 범위: 0-3000 |
| `0xC5` | Power Mode Timeout | ms, 범위: 0-65000 |

### 2.9 일반 설정 (Read/Write)

| 주소 | 이름 | 비고 |
|---|---|---|
| `0xD0` | OutA Mask | Section 7.1 |
| `0xD1` | I2C Transaction Timeout | ms, 범위: 2-230 |
| `0xD2` | Event Timeouts | Table A.31 |
| `0xD3` | Events Enable & Activation Settling Threshold | Table A.32/A.33 |
| `0xD4` | Release UI Settings / Movement Timeout | Table A.34/A.35 |

### 2.10 I2C 설정 (Read/Write)

| 주소 | 이름 |
|---|---|
| `0xE0` | I2C Setup |
| `0xE1` | Hardware ID |

---

## 3. 상태 레지스터 상세

### 3.1 System Status (`0x10`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:14 | Current Power Mode | `00`=NP, `01`=LP, `10`=ULP, `11`=Halt |
| 13 | CH2 Touch | `1`=CH2 터치 상태 |
| 12 | CH2 Prox | `1`=CH2 프록 상태 |
| 11 | CH1 Touch | `1`=CH1 터치 상태 |
| 10 | CH1 Prox | `1`=CH1 프록 상태 |
| 9 | CH0 Touch | `1`=CH0 터치 상태 |
| 8 | CH0 Prox | `1`=CH0 프록 상태 |
| 7 | Reset Event | `1`=리셋 이벤트 발생 |
| 6 | ATI Error | `1`=ATI 오류 발생 |
| 5 | ATI Active | `1`=ATI 실행 중 |
| 4 | ATI Event | `1`=ATI 이벤트 발생 |
| 3 | Power Event | `1`=전력 모드 변경됨 |
| 2 | Slider Event | `1`=슬라이더 이벤트 발생 |
| 1 | Touch Event | `1`=터치 이벤트 발생 |
| 0 | Prox Event | `1`=프록 이벤트 발생 |

> Event 모드에서 System Status를 읽어 이벤트 플래그를 클리어해야 연속 인터럽트가 멈춤.

### 3.2 Gesture Status (`0x11`)

| Bit | 이름 | 설명 |
|---|---|---|
| 7 | Busy | `1`=제스처 처리 중 |
| 6 | Event | `1`=제스처 이벤트 발생 |
| 5 | Hold | `1`=Hold 감지 |
| 4 | Flick Negative | `1`=음방향 Flick 감지 |
| 3 | Flick Positive | `1`=양방향 Flick 감지 |
| 2 | Swipe Negative | `1`=음방향 Swipe 감지 |
| 1 | Swipe Positive | `1`=양방향 Swipe 감지 |
| 0 | Tap | `1`=Tap 감지 |

### 3.3 Version Information (`0x00`)

| 주소 | Order Code 001 | Order Code A01 |
|---|---|---|
| `0x00` Product Number | `1106` | `1462` |
| `0x01` Major Version | `1` | `1` |
| `0x02` Minor Version | `3` | `4` |

### 3.4 Hardware ID (`0xE1`)

| 값 | HW 버전 |
|---|---|
| `0xF003` | IQS3dd |
| `0xF004` | IQS3ed |

> Cs Size 레지스터(Table A.7, A.8) 비트 필드가 HW 버전에 따라 다르므로 주의.

---

## 4. 터치/프록시미티 감지 설정

### 4.1 감지 원리

- **Counts**: 센서 측정값 (용량/인덕턴스에 반비례)
- **LTA** (Long-Term Average): 환경 변화를 추적하는 기준값 (터치 중에는 업데이트 안 됨)
- **Delta**: `LTA - Counts` (비반전 채널 기준)

### 4.2 Prox 진입/탈출 조건

비반전 채널, 단방향 감지 기준:

```
진입: (LTA - Counts) > Prox Threshold → Prox Debounce Enter 샘플 연속 만족
탈출: (LTA - Counts) ≤ Prox Threshold → Prox Debounce Exit 샘플 연속 만족
```

### 4.3 Touch 진입/탈출 조건

```
진입: (LTA - Counts) > Touch Threshold
탈출: (LTA - Counts) < (Touch Threshold - Touch Hysteresis)
```

### 4.4 Prox Settings (`0x61`, `0x71`, `0x81`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:12 | Prox Debounce Exit | 0=비활성, 1-15=디바운스 샘플 수 |
| 11:8 | Prox Debounce Enter | 0=비활성, 1-15=디바운스 샘플 수 |
| 7:0 | Prox Threshold | 8-bit 임계값 |

### 4.5 Touch Settings (`0x62`, `0x72`, `0x82`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:12 | Touch Hysteresis | 히스테리시스 = (값/256) x Touch Threshold |
| 7:0 | Touch Threshold | Touch Threshold = (값 x LTA) / 256 |

### 4.6 Event Timeouts (`0xD2`)

채널이 Prox 또는 Touch 상태에 일정 시간 이상 머물면 자동 Reseed (롱터치 타임아웃):

| Bit | 이름 | 계산 |
|---|---|---|
| 15:8 | Touch Event Timeout | 값 x 512 ms |
| 7:0 | Prox Event Timeout | 값 x 512 ms |

> - 채널별로 `0xC0`의 CHx Timeout Disable 비트로 타임아웃 비활성화 가능.
> - 채널 Prox/Touch 타임아웃 사용 시 ULP 모드 사용 금지 → Power Mode를 `Automatic No ULP`로 설정.

---

## 5. ATI (Automatic Tuning Implementation)

### 5.1 개요

ATI는 다양한 센서 전극 용량/인덕턴스에 대해 최적 성능을 제공하기 위한 자동 캘리브레이션 알고리즘.

### 5.2 ATI Setup (`0x36`, `0x46`, `0x56`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:4 | ATI Resolution Factor | ATI TARGET = ACTUAL ATI BASE x (ATI Resolution Factor / 16) |
| 3 | ATI Band | `0`=Small (1/16 x TARGET), `1`=Large (1/8 x TARGET) |
| 2:0 | ATI Mode | 아래 표 참조 |

**ATI Mode 설정값:**

| 값 | 모드 | 설명 |
|---|---|---|
| `000` | Disabled | ATI 비활성 |
| `001` | Compensation Only | 보상만 |
| `010` | ATI from Compensation Divider | 보상 디바이더 기반 |
| `011` | ATI from Fine Fractional Divider | 미세 분주비 기반 |
| `100` | Full | 전체 ATI (권장) |

### 5.3 ATI Base (`0x37`, `0x47`, `0x57`)

16-bit 값. ATI TARGET 계산의 기본값.

```
ATI TARGET = ATI BASE × (ATI Resolution Factor / 16)
```

### 5.4 ATI Multipliers and Dividers (`0x38`, `0x48`, `0x58`)

| Bit | 이름 |
|---|---|
| 15:12 | Fine Fractional Multiplier |
| 11:8 | Fine Fractional Divider |
| 7:4 | Coarse Fractional Multiplier |
| 3:0 | Coarse Fractional Divider |

### 5.5 Compensation (`0x39`, `0x49`, `0x59`)

| Bit | 이름 |
|---|---|
| 15:8 | Compensation Divider |
| 6:0 | Compensation |

> Bit 7은 Reserved.

### 5.6 Automatic Re-ATI

LTA가 ATI Target 주변의 허용 범위(ATI Band)를 벗어나면 자동 Re-ATI 수행:

```
Re-ATI 조건: LTA > (ATI Target + ATI Band) 또는 LTA < (ATI Target - ATI Band)
```

**ATI Band 계산 예시** (ATI Target=800, ATI Band=1/8):
- Band = 800/8 = 100
- Re-ATI 발생: LTA > 900 또는 LTA < 700

### 5.7 ATI Error

ATI 완료 후 Counts가 Re-ATI Boundary 밖이면 ATI Error 플래그 설정.
ATI Error 발생 시 자동 Re-ATI는 트리거되지 않음 → `0xC0`의 Re-ATI 비트로 수동 트리거 필요.

---

## 6. 센서 설정

### 6.1 Sensor Setup (`0x30`, `0x40`, `0x50`)

| Bit | 이름 | 설명 |
|---|---|---|
| 14 | CalCap Rx | `1`=CalCap Rx 선택 |
| 13 | CalCap Tx | `1`=CalCap Tx 선택 |
| 11 | TxA | `1`=TxA 활성 |
| 10:8 | CTx2/CTx1/CTx0 | 각 `1`=해당 CTx 활성 |
| 6 | Release/Movement UI Enable | `1`=Release/Movement UI 활성 |
| 5 | FOSC Tx Frequency | `0`=CONV_FREQ로 설정, `1`=14 MHz |
| 4 | Vbias | `1`=CRx2에 Vbias 출력 |
| 3 | Invert | `1`=채널 로직 반전 |
| 2 | Dual Direct | `1`=양방향 임계값 |
| 1 | Linearise Counts | `1`=Counts 선형화 |
| 0 | Enable Channel | `1`=채널 활성 |

### 6.2 Prox Control (`0x32`, `0x42`, `0x52`)

| Bit | 이름 | 설명 |
|---|---|---|
| 14 | 0v5 Discharge | `1`=활성 |
| 12 | Cs Size (IQS3dd) | `0`=40pF, `1`=80pF |
| 12:11 | Cs Size (IQS3ed) | `01`=40pF, `11`=80pF |
| 9:8 | S/H Bias Select | `00`=2uA, `01`=5uA, `10`=7uA, `11`=10uA |
| 7:6 | Max Counts | `00`=1023, `01`=2047, `10`=4095, `11`=16383 |
| 5:0 | PXS Mode | 아래 표 참조 |

**PXS Mode 설정값:**

| 값 | 모드 |
|---|---|
| `0x10` | Self-Capacitance |
| `0x13` | Mutual-Capacitance |
| `0x1D` | Current Measurement |
| `0x3D` | Inductive |

> IQS3ed와 IQS3dd의 Cs Size 비트 필드가 다름. GUI 생성 헤더 파일을 다른 HW 버전에 사용 금지.

### 6.3 Prox Input and Control (`0x33`, `0x43`, `0x53`)

| Bit | 이름 | 설명 |
|---|---|---|
| 13 | Internal Reference | `1`=내부 레퍼런스 활성 |
| 12 | Prox Engine Bias Current | `1`=활성 |
| 11 | Calibration Capacitor Select | `0`=활성, `1`=비활성 |
| 10:8 | Rx2/Rx1/Rx0 | 각 `1`=해당 Rx 활성 |
| 6 | Dead Time Enable | `1`=Dead Time 활성 |
| 3:2 | Auto Prox Cycle Select | `00`=4, `01`=8, `10`=16, `11`=32 변환 |

> Bit 7=1(고정), Bit 1:0=11(고정) 으로 설정 필요.

### 6.4 Conversion Frequency (`0x31`, `0x41`, `0x51`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:8 | Conv Freq Period | 범위: 0-127 |
| 7:0 | Conv Freq Fraction | `127` 고정 권장 |

**전하 전달 주파수 계산** (FOSC=14 MHz, Dead Time 활성 시):

```
f_xfer = f_osc / (2 × period + 3)
```

| Period 값 | f_xfer |
|---|---|
| 1 | 2 MHz |
| 5 | 1 MHz (Mutual 최대) |
| 12 | 500 kHz |
| 17 | 350 kHz |
| 26 | 250 kHz |
| 53 | 125 kHz |

### 6.5 Pattern Definitions (`0x34`, `0x44`, `0x54`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:12 | Wav Pattern 1 | 파형 패턴 1 |
| 11:8 | Wav Pattern 0 | 파형 패턴 0 |
| 7:4 | Calibration Capacitor | 크기 = 0.5pF x 값 (최대 7 → 3.5pF) |
| 3:0 | Inactive Rxs | `0x00`=Floating, `0x05`=Vbias, `0x0A`=VSS, `0x0F`=VREG |

**측정 타입별 권장 패턴 값** (Wav Pattern Select=`0x00`):

| 측정 타입 | Wav Pattern 0 | Wav Pattern 1 |
|---|---|---|
| Self Capacitance | `0x03` | `0x00` |
| Mutual Capacitance | `0x0E` | `0x00` |
| Inductive | `0x0B` | `0x00` |

### 6.6 Pattern Selection and Engine Bias Current (`0x35`, `0x45`, `0x55`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:12 | Engine Bias Current | 부호 있는 값 (MSB=부호비트), Bias = 값 x 3uA + Trim x 200nA |
| 11:8 | Engine Bias Current Trim | 4-bit trim 값 |
| 7:0 | Wav Pattern Select | 각 Cx에 Pattern 0/1 선택 (Bit3=TxA, Bit2=CTx2, Bit1=CTx1, Bit0=CTx0) |

---

## 7. 채널 설정

### 7.1 Channel Setup (`0x60`, `0x70`, `0x80`)

| Bit | 이름 | 설명 |
|---|---|---|
| 15:8 | Follower Event Mask | System Status 상위 바이트의 이벤트 마스크 |
| 7:4 | Reference Sensor ID | 레퍼런스 센서 채널 번호 선택 |
| 3:0 | Channel Mode | `00`=Independent, `01`=Follower, `10`=Reference |

### 7.2 Follower Weight (`0x63`, `0x73`, `0x83`)

- 16-bit 값
- 실제 Weight = 레지스터 값 / 4096
- 4096 → 레퍼런스를 1:1 직접 추적
- \>4096 → 적극적(aggressive) 추적
- <4096 → 느린(slow) 추적

### 7.3 Movement UI Settings (`0x64`, `0x74`, `0x84`) — Movement UI 오더코드만

| Bit | 이름 | 설명 |
|---|---|---|
| 15:12 | Movement Debounce Exit | 0=비활성, 1-15=디바운스 샘플 수 |
| 11:8 | Movement Debounce Enter | 0=비활성, 1-15=디바운스 샘플 수 |
| 7:0 | Movement Threshold | 8-bit 임계값 |

---

## 8. 필터 설정

IIR 필터 적용. Damping factor = Beta / 256.

### 8.1 Counts Filter Betas (`0xB0`)

| Bit | 이름 |
|---|---|
| 15:8 | LP Counts Beta |
| 7:0 | NP Counts Beta |

### 8.2 LTA Filter Betas (`0xB1`)

| Bit | 이름 |
|---|---|
| 15:8 | LP LTA Beta |
| 7:0 | NP LTA Beta |

### 8.3 LTA Fast Filter Betas (`0xB2`)

| Bit | 이름 |
|---|---|
| 15:8 | LP LTA Fast Beta |
| 7:0 | NP LTA Fast Beta |

> Counts가 감지 방향 반대로 드리프트하면 Fast 필터 활성화. Counts-LTA 차이가 Fast Filter Band(`0xB4`) 이내로 돌아오면 Normal 필터로 복귀.

### 8.4 Activation/Movement LTA Filter Betas (`0xB3`)

| Bit | 이름 |
|---|---|
| 15:8 | LP Activation/Movement LTA Beta |
| 7:0 | NP Activation/Movement LTA Beta |

### 8.5 Fast Filter Band (`0xB4`)

- 16-bit 값
- Fast 필터 활성화 범위 결정

---

## 9. 시스템 제어

### 9.1 System Control (`0xC0`)

| Bit | 이름 | 설명 |
|---|---|---|
| 10 | CH2 Timeout Disable | `1`=CH2 글로벌 타임아웃 비활성 |
| 9 | CH1 Timeout Disable | `1`=CH1 글로벌 타임아웃 비활성 |
| 8 | CH0 Timeout Disable | `1`=CH0 글로벌 타임아웃 비활성 |
| 7 | Interface Type | `0`=Streaming, `1`=Event |
| 6:4 | Power Mode | 아래 표 참조 |
| 3 | Reseed | `1`=Reseed 트리거 |
| 2 | Re-ATI | `1`=Re-ATI 트리거 |
| 1 | Soft Reset | `1`=소프트 리셋 트리거 |
| 0 | ACK Reset | `1`=리셋 이벤트 확인 (Reset Event 플래그 클리어) |

**Power Mode 설정값:**

| 값 | 모드 |
|---|---|
| `000` | Normal Power |
| `001` | Low Power |
| `010` | Ultra Low Power |
| `011` | Halt |
| `100` | Automatic (NP→LP→ULP 자동 전환) |
| `101` | Automatic No ULP (NP→LP만 전환, ULP 진입 안 함) |

### 9.2 Power Mode Timeout (`0xC5`)

- 16-bit 값 (ms), 범위: 0-65000
- `0x00` = 타임아웃 없음 (현재 모드 유지)
- Automatic 모드에서 이 시간 동안 상호작용 없으면 다음 저전력 모드로 전환

### 9.3 Report Rate

| 주소 | 모드 | 단위 |
|---|---|---|
| `0xC1` | Normal Power | ms |
| `0xC2` | Low Power | ms |
| `0xC3` | Ultra Low Power | ms |
| `0xC4` | Halt | ms (범위: 0-3000) |

---

## 10. 이벤트 설정

### 10.1 Events Enable (`0xD3`)

**Release UI 오더코드 (001):**

| Bit | 이름 | 설명 |
|---|---|---|
| 15:8 | Activation Settling Threshold | 8-bit 값 |
| 6 | ATI Error | `1`=ATI Error 이벤트 활성 |
| 4 | ATI Event | `1`=ATI 이벤트 활성 |
| 3 | Power Event | `1`=전력 모드 변경 이벤트 활성 |
| 2 | Slider Event | `1`=슬라이더 이벤트 활성 |
| 1 | Touch Event | `1`=터치 이벤트 활성 |
| 0 | Prox Event | `1`=프록 이벤트 활성 |

**Movement UI 오더코드 (A01):** 상위 바이트는 Reserved.

### 10.2 이벤트 트리거 조건

| 이벤트 | 트리거 조건 |
|---|---|
| ATI Error | ATI 처리 중 오류 발생 |
| ATI Event | ATI 트리거됨 |
| Power | 전력 모드 변경됨 |
| Slider | 슬라이더 제스처 감지됨 |
| Prox | 채널 Prox 진입/탈출 |
| Touch | 채널 Touch 진입/탈출 |

---

## 11. Release UI

장기 터치/프록 이벤트 감지 및 릴리스를 위한 기능. Release UI 오더코드(001)에서만 사용.

### 11.1 동작 원리

1. 터치 감지 → LTA 고정, Activation LTA는 계속 업데이트
2. `|Counts - Activation LTA|` < Activation Settling Threshold가 Delta Snapshot Sample Delay 샘플 동안 유지되면 → Delta Snapshot = LTA - Counts 기록
3. 릴리스 조건:

```
(Counts - Activation LTA) > (Delta Snapshot × Release Delta Percentage / 128)
```

→ 채널 Reseed되어 터치/프록 상태 탈출

### 11.2 Release UI Settings (`0xD4`) — Release UI 오더코드

| Bit | 이름 | 설명 |
|---|---|---|
| 15:8 | Delta Snapshot Sample Delay | 8-bit 값 |
| 7:0 | Release Delta Percentage | 실제 비율 = 값 / 128 |

### 11.3 관련 레지스터

- Activation LTA: `0x20`-`0x22` (Read Only)
- Delta Snapshot: `0x23`-`0x25` (Read Only)
- Activation LTA Filter Betas: `0xB3`

---

## 12. Movement UI

장기 터치 중 움직임 감지 기능. Movement UI 오더코드(A01)에서만 사용.

### 12.1 동작 원리

1. Movement LTA: 터치 중에도 계속 업데이트
2. 터치 상태에서 `|Movement LTA - Counts|` < Movement Threshold가 Movement Timeout 이상 지속 → Reseed되어 터치 상태 클리어
3. 움직임이 있으면 타임아웃이 리셋되어 터치 상태 유지 (웨어 감지용)

> ULP 모드와 함께 사용 금지. Power Mode를 `Automatic No ULP`로 설정.

### 12.2 Movement Timeout (`0xD4`) — Movement UI 오더코드

| Bit | 이름 | 계산 |
|---|---|---|
| 15:0 | Movement Timeout | 값 x 512 ms |

### 12.3 Movement Status (`0x23`)

| Bit | 이름 | 설명 |
|---|---|---|
| 2 | CH2 Movement | `1`=CH2 움직임 감지 |
| 1 | CH1 Movement | `1`=CH1 움직임 감지 |
| 0 | CH0 Movement | `1`=CH0 움직임 감지 |

---

## 13. 리셋 및 초기화

### 13.1 리셋 방법

| 방법 | 설명 |
|---|---|
| 소프트 리셋 | `0xC0` Bit1(Soft Reset)에 `1` 쓰기 |
| 하드 리셋 | MCLR/RDY 핀 LOW (통신 윈도우 밖에서) |

### 13.2 리셋 후 초기화 시퀀스

1. Reset Event 발생 → System Status(`0x10`)의 Bit7 = 1
2. ACK Reset: System Control(`0xC0`)의 Bit0에 `1` 쓰기 → Reset Event 플래그 클리어
3. ACK Reset 전까지:
   - Event 모드 진입 불가 (Streaming 동작 유지)
   - ATI 실행 시 통신 윈도우 연속 제공 → ATI 시간 증가

---

## 14. I2C Setup (`0xE0`)

| Bit | 이름 | 설명 |
|---|---|---|
| 1 | R/W Check Disable | `1`=Read/Write 체크 비활성 |
| 0 | Stop Bit Disable | `1`=STOP 비트 비활성 (0xFF로 통신 종료) |

---

## 15. OutA 핀 설정 (`0xD0`)

| 값 | 동작 |
|---|---|
| `0x0000` | OutA = LOW |
| `0x7FFF` | OutA = HIGH |
| 기타 | 이벤트 인디케이터 (아래 참조) |

- 슬라이더 비활성(채널 수=0): OutA Mask가 System Status의 이벤트 선택
- 슬라이더 활성(채널 수>0): OutA Mask가 Gesture Status의 이벤트 선택
- Bit15: `1`=Active LOW, `0`=Active HIGH

---

## 16. 제스처 설정

### 16.1 Gesture Enable (`0xA0`)

| Bit | 이름 |
|---|---|
| 3 | Hold Enable |
| 2 | Flick Enable |
| 1 | Swipe Enable |
| 0 | Tap Enable |

### 16.2 Gesture Timing

| 주소 | 이름 | 설명 |
|---|---|---|
| `0xA1` | Minimum Time | 최소 시간 (ms) |
| `0xA2` | Max Tap Time | 탭 최대 시간 (ms) |
| `0xA3` | Max Swipe Time | 스와이프 최대 시간 (ms) |
| `0xA4` | Min Hold Time | 홀드 최소 시간 (ms) |
| `0xA5` | Max Tap Distance | 탭 최대 거리 |
| `0xA6` | Min Swipe Distance | 스와이프 최소 거리 |

---

## 17. 워치독 타이머

- 255 ms 내에 킥되지 않으면 소프트웨어 리셋 트리거
- I2C 통신 중에는 Read/Write 발생 시 자동 리셋됨
- I2C START 후 완료하지 않으면 워치독 만료로 디바이스 리셋 발생

---

## 18. 오더코드 차이 요약

| 항목 | 001 (Release UI) | A01 (Movement UI) |
|---|---|---|
| Product Number (`0x00`) | 1106 | 1462 |
| Minor Version (`0x02`) | 3 | 4 |
| UI 기능 | Release UI | Movement UI |
| `0xD3` 상위 바이트 | Activation Settling Threshold | Reserved |
| `0xD4` | Release UI Settings | Movement Timeout |
| Enable Status Pointer (`0x95`) | `0x0552` | `0x0558` |
| Delta Link 값 (CH1) | `0x0472` | `0x0474` |
| Delta Link 값 (CH2) | `0x04B4` | `0x04B8` |

---

*출처: [Azoteq IQS323 Datasheet v1.2](https://www.azoteq.com/images/stories/pdf/iqs323_datasheet.pdf)*
