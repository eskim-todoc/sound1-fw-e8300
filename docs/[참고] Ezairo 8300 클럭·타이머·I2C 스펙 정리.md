# Ezairo 8300 클럭·타이머·I2C 스펙 정리

## 개요

Ezairo 8300 / RSL15 계열 SoC 의 **클럭 분배, 일반 목적 타이머, I2C 마스터 모드** 관련 스펙을 공식 레퍼런스에서 발췌·정리한 문서. 저전력 전환·타이머 주기 재계산·I2C 속도 조정 등 작업 시 PDF 를 다시 열지 않고 본 문서만으로 판단할 수 있도록 구성.

**출처**
- `[참고] Ezairo 8300 Hardware Reference.pdf` (1098p) — 회로·레지스터 레벨 스펙
- `[참고] Ezairo 8300 Firmware Reference.pdf` (578p) — SDK API 스펙

각 항목에 원본 페이지 번호를 `HW p.311` 형식으로 병기.

---

## 1. 클럭 체계

### 1.1 SYSCLK 소스 (HW §10.1)

| 소스 | 주파수 | 비고 |
|---|---|---|
| **CCO (Current-Controlled Oscillator)** | 기본 7.68 MHz, 배수 ×1/×2/×4 | 기본 소스. `ANALOG_OSC_CTRL_1_MULT` 로 배수 설정. ×3 은 지원 안 됨 |
| Standby Oscillator | 25 kHz | 초저전력 standby 모드 전용 |
| EXTCLK (DIO35) | 외부 입력 | 분주(1~16) 가능 |

선택 레지스터: `CLK_CFG_0_SYSCLK_SEL` (HW p.322)

### 1.2 SYSCLK 하위 파생 클럭 (HW §10.2)

```mermaid
graph LR
    SYSCLK[SYSCLK]
    SYSCLK -->|/SDMCLK_PRESCALE<br>6-bit, 1~64| SDMCLK[SDMCLK<br>권장 3.84/7.68/15.36 MHz]
    SYSCLK -->|/ADCCLK_PRESCALE<br>5-bit, 1~32| ADCCLK_S[ADCCLK]
    SDMCLK -->|선택| ADCCLK_S
    ADCCLK_S --> ADCCLK[ADCCLK<br>권장 3.84 MHz]
    SYSCLK -->|/SLOWCLK_PRESCALE<br>7-bit, 1~128| SLOWCLK_S[SLOWCLK]
    ADCCLK --> |선택| SLOWCLK_S
    SLOWCLK_S --> SLOWCLK[SLOWCLK<br>최적 1.28 MHz]
    SLOWCLK --> |/32 고정| SLOWCLK_DIV32[SLOWCLK_DIV32<br>= 40 kHz]
    SLOWCLK_DIV32 --> TIMER[일반 목적 타이머]
    SLOWCLK_DIV32 --> WDT[워치독 타이머]
    SYSCLK -->|/UCLK_PRESCALE<br>12-bit, 1~4096| UCLK_S[UCLK]
    ADCCLK --> |선택| UCLK_S
    SYSCLK --> |/1 고정| UARTCLK_S[UARTCLK]
    ADCCLK --> |선택| UARTCLK_S
    SYSCLK --> I2C[I2C SCL<br>/((PRESCALE+1)*3)]
```

**권장값 요약** (HW §10.2)
- SLOWCLK: **1.28 MHz 고정** (시스템 최적화 기준). 다른 값은 "unexpected or incorrect behavior" 가능.
- SDMCLK: 3.84 / 7.68 / 15.36 MHz
- ADCCLK: 3.84 MHz

### 1.3 레지스터 필드 (HW §10.4)

| 레지스터 | 필드 | 범위 | 매핑 |
|---|---|---|---|
| `CLK_CFG_1` [22:16] | `SLOWCLK_PRESCALE` | 1 ~ 128 | 값 N → `SLOWCLK_PRESCALE_N`, hex = N-1 |
| `CLK_CFG_1` [13:8] | `SDMCLK_PRESCALE` | 1 ~ 64 | 동일 |
| `CLK_CFG_1` [4:0] | `ADCCLK_PRESCALE` | 1 ~ 32 | 동일 |
| `CLK_CFG_2` [11:0] | `UCLK_PRESCALE` | 1 ~ 4096 | 2의 제곱 주요값: 2, 4, 8, ..., 4096 |

### 1.4 Standby 모드 (HW §9.4)

**실제 "Standby 모드"** 는 SYSCLK 을 **25 kHz Standby Oscillator** 로 전환한 상태. CCO 는 꺼짐. 다음을 반드시 거침:
1. LSAD 비활성화
2. VDDA CP 저전력 모드, VDDC/VDDM CP 비활성화
3. VDDM 트림 조정 (standby 레벨)
4. `ANALOG_PWR_CTRL_ANALOG_DISABLE` = 1
5. CCO 멀티플라이어 disable
6. `CLK_CFG_0_SYSCLK_SEL` = STANDBY
7. VDDC/VDDM retention 트림

**"Standby 모드가 아닌 저전력"** — CCO 가 유지되고 주파수만 낮춰진 상태는 ci_power.c 의 방식이며 공식 "Standby 모드" 와 구분됨. 본 문서에서는 이를 **CCO 저주파 모드**로 칭함.

### 1.5 Standby 모드에서 I2C (HW §9.4)

> "The D_I2C interface bus is disconnected by default when in standby mode, unless the `D_I2C_CFG_CONNECT_IN_STANDBY` bit from the D_I2C_CFG register is set."

- **공식 Standby 모드**에서만 해당. CCO 저주파 모드에서는 I2C 자동 disconnect 없음.
- `D_I2C_CFG` [29] `CONNECT_IN_STANDBY` 로 제어 가능.

### 1.6 SLOWCLK_DIV32 의 특이 동작 (HW §10.2.4)

> "The SLOWCLK_DIV32 divider is disabled so that it runs at the SYSCLK rate when the STANDBYCLK is selected as the SYSCLK source."

- 공식 Standby 모드에서만 `/32` 가 우회되어 SLOWCLK_DIV32 = SYSCLK(=25kHz) 이 됨.
- CCO 저주파 모드에서는 SLOWCLK_DIV32 = SLOWCLK/32 공식 유지.

### 1.7 주파수 전환 가이드 (HW §10.3.2)

> "For optimal performance, limit frequency-based clock throttling to steps of no more than 400% of the lower frequency."

- 한 번에 최대 4배 스텝까지만 권장. 예: 3.84 → 15.36 MHz (4배) OK, 3.84 → 19.2 MHz (5배) 는 중간 주파수(7.68) 경유 필요.
- `Sys_Trims_SetOperatingFrequency` 직접 호출 시에도 동일 원칙 권장.

---

## 2. 일반 목적 타이머 (HW §19.8)

### 2.1 공통 스펙

- **개수**: 4개 (TIMER0 ~ TIMER3) + SysTick
- **카운터**: 24-bit 다운 카운터
- **프리스케일**: 3-bit (1, 2, 4, 8, 16, 32, 64, 128 중 선택, 2의 거듭제곱만)
- **모드**: Single-shot / Multi-shot / Free-run / DIO 인터럽트 캡처

### 2.2 타이머 클럭 소스 (HW p.763)

> "The general-purpose timers are clocked from **SLOWCLK, divided by 32**."

→ **타이머 기본 클럭 = SLOWCLK_DIV32**

SLOWCLK = 1.28 MHz 기준 → SLOWCLK_DIV32 = **40 kHz** (= 25 μs 주기)

### 2.3 타이머 주기 공식 (HW p.763 원문)

$$T = \frac{2^{\text{PRESCALE}} \times (\text{TIMEOUT\_VALUE} + 1)}{f_{\text{SLOWCLK\_DIV32}}}$$

**중요한 필드 특성**:
- `PRESCALE` 은 **2 의 지수** (field value 0-7 → 실제 분주 1, 2, 4, 8, 16, 32, 64, 128). 단순 "+1" 아님.
- `TIMEOUT_VALUE` 는 **`+1` 된 값**이 사이클 수. N 쓰면 N+1 사이클.
- 2 field 의 특성이 다름에 주의.

**SLOWCLK = 1.28 MHz, PRESCALE_1 (=2^0) 기준 계산표**

| TIMEOUT_VALUE | 2^P × (N+1) | 주기 |
|---:|---:|---:|
| 19 | 20 | 500 μs |
| 39 | 40 | 1.0 ms |
| 199 | 200 | 5.0 ms |
| 399 | 400 | 10.0 ms |
| 3999 | 4000 | 100 ms |
| 19999 | 20000 | 500 ms |

**긴 주기 용도 (PRESCALE_128 = 2^7 기준)**

| TIMEOUT_VALUE | 2^7 × (N+1) | 주기 |
|---:|---:|---:|
| 155 | 19968 | 499.2 ms |
| 311 | 39936 | 998.4 ms |
| 467 | 59904 | 1497.6 ms |
| 623 | 79872 | 1996.8 ms |

> 기존 `ci_timer.h` 헤더의 `OTE_1_5_GEN_TIMER_TICK_1MS_PM_NORMAL = 39` 가 1 ms 에 해당하는 정확한 값.

### 2.4 레지스터

| 레지스터 | 필드 | 역할 |
|---|---|---|
| `TIMER*_CFG0` [26:24] | `PRESCALE` | 0~7 → 1, 2, 4, 8, 16, 32, 64, 128 |
| `TIMER*_CFG0` [23:0] | `TIMEOUT_VALUE` | 타임아웃 카운트 |
| `TIMER*_CFG1` [10:8] | `MULTI_COUNT` | 멀티샷 횟수 |
| `TIMER*_CFG1` [0] | `MODE` | `TIMER_SHOT_MODE` (0) / `TIMER_FREE_RUN` (1) |
| `TIMER*_CTRL` [1] | `START` | 1 = 시작/재시작 |
| `TIMER*_CTRL` [0] | `STOP` | 1 = 중지 |
| `TIMER*_CTRL` [8] | `BUSY` | 타이머 동작 중 |

### 2.5 SDK API (FW §18.26)

```c
void Sys_Timer_Config(TIMER_Type *timer, uint32_t cfg0, uint32_t cfg1, uint32_t timeout);
void Sys_Timer_Start(TIMER_Type *timer);
void Sys_Timer_Stop(TIMER_Type *timer);
```

- `cfg0`: `TIMER_PRESCALE_*` 중 택1
- `cfg1`: `TIMER_SHOT_MODE` or `TIMER_FREE_RUN`, MULTI_COUNT, DIO int 설정 OR 조합
- `timeout`: `TIMEOUT_VALUE` 필드 값

---

## 3. 워치독 타이머 (HW §19.9)

- 클럭 소스: **SLOWCLK_DIV32 추가 /32** (실질적으로 SLOWCLK / 1024)
- 13-bit 카운터
- **리프레시 레지스터**: `D_SYSTEM_CTRL` 에 `SYSTEM_WATCHDOG_REFRESH` 비트 쓰기
- 두 번째 타임아웃 시 전체 시스템 리셋

### 3.1 타임아웃 옵션 (SLOWCLK = 1.28 MHz 기준)

| 심볼 | 값 |
|---|---|
| `WATCHDOG_TIMEOUT_1M6` | 1.6 ms |
| `WATCHDOG_TIMEOUT_25M6` | 25.6 ms |
| `WATCHDOG_TIMEOUT_102M4` | 102.4 ms |
| `WATCHDOG_TIMEOUT_204M8` | 204.8 ms |
| `WATCHDOG_TIMEOUT_409M6` | 409.6 ms |
| `WATCHDOG_TIMEOUT_819M2` | 819.2 ms |
| `WATCHDOG_TIMEOUT_1638M4` | 1.64 s |
| `WATCHDOG_TIMEOUT_3276M8` | **3.28 s** (기본값, 0xB) |

다른 SLOWCLK 주파수에선 비례 조정.

### 3.2 강제 리셋

```c
D_WATCHDOG_RESET = 0x87FDF035;   // WATCHDOG_FORCE_RESET 키
```

---

## 4. I2C 마스터 모드 (HW §18.4)

### 4.1 SCL 주파수 공식 (HW p.613)

> `MASTER_PRESCALE` 필드 [15:8] of `I2C*_CFG`:
> "SCL is prescaled by **(PRESCALE + 1) × 3**."

```
SCL = SYSCLK / ((PRESCALE + 1) × 3)
```

### 4.2 최소 SYSCLK 요구조건 (HW §18.4.3)

| I2C 모드 | 최대 SCL | 필요한 최소 SYSCLK |
|---|---|---|
| Standard | 100 kbps | 충분 |
| **Fast-mode** | 400 kbps | **1.5 MHz** 이상 |
| Fast-mode Plus | 1 Mbps | **6 MHz** 이상 |

### 4.3 PRESCALE 상수값 (HW p.615-617)

필드는 (PRESCALE+1)×3 결과값으로 네이밍됨. 주요값 발췌:

| 심볼 | hex | (PRESCALE+1)×3 | SCL @ 2.56MHz | SCL @ 30.72MHz |
|---|---|---|---|---|
| `D_I2C_MASTER_PRESCALE_3` | 0x00 | 3 | 853 kHz | 10.24 MHz |
| `D_I2C_MASTER_PRESCALE_6` | 0x01 | 6 | 427 kHz | 5.12 MHz |
| `D_I2C_MASTER_PRESCALE_12` | 0x03 | 12 | 213 kHz | 2.56 MHz |
| `D_I2C_MASTER_PRESCALE_24` | 0x07 | **24** | **107 kHz** | 1.28 MHz |
| `D_I2C_MASTER_PRESCALE_48` | 0x0F | 48 | 53 kHz | 640 kHz |
| `D_I2C_MASTER_PRESCALE_120` | 0x27 | 120 | 21 kHz | 256 kHz |
| (커스텀) | 0x4F | **240** | **10.7 kHz** | **128 kHz** |
| `D_I2C_MASTER_PRESCALE_768` | 0xFF | 768 | 3.3 kHz | 40 kHz |

> 현재 코드(`driver_i2c.h:12`)는 커스텀 값 `0x4F` 사용 → 하드웨어 사전 정의 심볼이 아닌 직접 지정한 값.

### 4.4 기타 필드 (`I2C*_CFG`, HW §18.4.5.1)

| 비트 | 필드 | 역할 |
|---|---|---|
| 30 | `REPEATED_START_INT_ENABLE` | 반복 스타트 인터럽트 |
| **29** | **`CONNECT_IN_STANDBY`** | **공식 Standby 모드에서 I2C 라인 유지 여부** |
| 28 | `TX_DMA_ENABLE` | |
| 27 | `RX_DMA_ENABLE` | |
| 26 | `TX_INT_ENABLE` | |
| 25 | `RX_INT_ENABLE` | |
| 24 | `BUS_ERROR_INT_ENABLE` | |
| 23 | `OVERRUN_INT_ENABLE` | |
| 22 | `STOP_INT_ENABLE` | |
| 21 | `AUTO_ACK_ENABLE` | |
| 20:16 | `SLAVE_PRESCALE` | 슬레이브 모드 전용 |
| **15:8** | **`MASTER_PRESCALE`** | **마스터 모드 SCL 분주** |
| 7:1 | `SLAVE_ADDRESS` | |
| 0 | `SLAVE` | 슬레이브 모드 enable |

### 4.5 인터럽트 종류

- `TX_INT` — 송신 가능 시
- `RX_INT` — 수신 완료 시
- `BUS_ERROR_INT` — 버스 에러
- `OVERRUN_INT` — RX 오버런
- `STOP_INT` — Stop 조건 감지
- `REPEATED_START_INT` — 반복 Start (슬레이브 모드)

---

## 5. Sys_Trims_SetOperatingFrequency (FW §18.27.4.15)

```c
unsigned int Sys_Trims_SetOperatingFrequency(unsigned int frequency_index);
```

- 내부 CCO 기본 주파수(multiplier 적용 전)를 변경
- `frequency_index`: `SYS_FREQ_*` 심볼 사용
- 반환: `SYS_ERRNO_NO_ERROR` 성공 / `SYS_ERRNO_NO_MATCH` 해당 주파수에 대한 캘리브레이션 없음

### 5.1 알려진 SYS_FREQ_* 심볼 (PDF 내 언급)

| 심볼 | 주파수 | 비고 |
|---|---|---|
| `SYS_FREQ_7M68` | 7.68 MHz | `DEFAULT_FREQ_IDX` (FW p.290) |
| `SYS_FREQ_15M36` | 15.36 MHz | |
| `SYS_FREQ_29M44` | 29.44 MHz | `OSC_MUL_LIMIT1` |
| `SYS_FREQ_31M36` | 31.36 MHz | |
| `SYS_FREQ_59M52` | 59.52 MHz | `OSC_MUL_LIMIT2` |
| `SYS_FREQ_2M56` | 2.56 MHz | **PDF 미언급**, 헤더에 정의되어 있음 (ci_power.c 에서 사용) |
| `SYS_FREQ_30M72` | 30.72 MHz | **PDF 미언급**, 헤더에 정의되어 있음 (ci_power.c 에서 사용) |

> 전체 목록은 SDK 헤더 (`trims.h` 내 부근) 참조. PDF 에는 예시로 몇 개만 나열.

### 5.2 주파수 변경 주의사항

- 변경 후 **약 5 ms 안정화 대기** 권장 (ci_power.c 관행)
- VDDC/VDDM 공급전압 트리밍을 해당 주파수에 맞게 설정해야 함 (HW §9.1.2.2)
- Calibration table 이 RAM 에 로드되어 있어야 함 (Sys_Trims_LoadManuTable)

---

## 6. 빠른 참조 체크리스트

### 6.1 SYSCLK 바꿀 때 확인할 사항

- [ ] SYSCLK 목표 주파수가 `SYS_FREQ_*` 목록에 있는가
- [ ] 변경 스텝이 4배 이하인가 (아니면 중간 주파수 경유)
- [ ] VDDC/VDDM 트림이 목표 주파수에 적합한가
- [ ] SLOWCLK 이 1.28 MHz 유지되도록 `SLOWCLK_PRESCALE` 재계산했는가
- [ ] SDMCLK, ADCCLK 가 권장 주파수 유지되도록 `*_PRESCALE` 재계산했는가 (오디오 사용 시)
- [ ] I2C PRESCALE 재계산했는가 (사용 시)
- [ ] UART baud rate 재계산했는가 (사용 시)

### 6.2 실측 확인 포인트

- 타이머 주기: `Sys_GPIO_Toggle(DIO*)` 를 TIMER IRQ 에 삽입하고 오실로스코프 측정
- I2C SCL: 로직 아날라이저로 SCL 라인 주파수 직접 측정
- 워치독: 리프레시 주기를 강제로 늘려 리셋 발생 시점 확인

---

## 참고 페이지 인덱스

| 주제 | PDF | 페이지 |
|---|---|---|
| Clock Generation | HW | §10.1 p.311 |
| Clock Distribution | HW | §10.2 p.312-317 |
| Clock Throttling | HW | §10.3 p.317-320 |
| Clock 레지스터 | HW | §10.4 p.321-332 |
| Standby Mode | HW | §9.4 p.300-301 |
| I2C Interfaces | HW | §18.4 p.601-630 |
| I2C CFG 레지스터 | HW | §18.4.5.1 p.613-617 |
| 일반 목적 타이머 | HW | §19.8 p.762-767 |
| 워치독 | HW | §19.9 p.768-770 |
| STANDBY_MODE API | FW | §18.25 p.227-228 |
| TIMER API | FW | §18.26 p.229-231 |
| TRIMS API | FW | §18.27 p.232-248 |
