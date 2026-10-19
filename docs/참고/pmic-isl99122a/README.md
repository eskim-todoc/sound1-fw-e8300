---
name: PMIC ISL9122A 데이터시트 정리
purpose: TX 전원 PMIC 의 동작 모드·레지스터 맵·주의사항을 데이터시트에서 뽑아 재사용 가능하게 정리
type: reference/부품
applies_to: [Sound1]
tags: [pmic, isl9122a, buck-boost, bypass, i2c, 데이터시트, tx전원]
---

# PMIC ISL9122A - 동작 모드와 레지스터

**TL;DR**: 자극 TX 전원을 만드는 **벅-부스트 레귤레이터**다. **«VIN 이 VOUT 보다 높으면 통과, 낮아지면 승압» 하는 모드는 없다** — VIN>VOUT 이면 **강압(Buck)** 해서 VOUT 을 `VSET` 으로 유지한다. Bypass 는 **VIN 이 VOUT ±1% 인 좁은 구간에서만** 자동으로 걸리는 전이 구간이고, `Forced Bypass` 는 **스위칭을 아예 끄는 별개 모드라 승압하지 않고 과전류 보호도 없다**. 모드는 `CONV_CFG(0x12)` 의 **`FMODE[3:2]`** 로 고른다.

> [!NOTE]
> 원문: [`ISL9122AIINZ-T.pdf`](ISL9122AIINZ-T.pdf) — Renesas FN8947 **Rev.1.03** (2025-10-09), 30쪽.
> 아래 «§» 는 데이터시트 절 번호, «p.N» 은 쪽수다.

## 1. 이 부품이 무엇인가

| 항목 | 값 | 출처 |
|---|---|---|
| 종류 | **비반전 벅-부스트 스위칭 레귤레이터** (입력이 출력보다 높아도 낮아도 된다) | p.1 |
| 입력 `VIN` | **1.8 ~ 5.5V** | p.1 |
| 출력 `VOUT` | **1.8 ~ 5.375V** (I2C 로 조정) | p.1 |
| 출력 전류 | 최대 **500mA** (`VIN > VOUT > 2.5V` 조건) | p.1 |
| 정지 전류 `IQ` | 규정 중 **1300nA** · Forced Bypass **120nA** · Shutdown **8nA** | p.1 |
| I2C 7비트 주소 | **`0x18`** (base version, 트림으로 고정) | §5.11, p.21 |
| I2C 속도 | 표준 100kbit/s · 고속 400kbit/s | §5.11, p.20 |

> [!IMPORTANT]
> **EN 핀이 LOW 면 I2C 도 죽는다.** 레지스터 접근은 IC 가 enable 상태일 때만 된다(§5.1, §6 서두). OTP 로딩 중에도 안 된다.

## 2. 동작 모드 - 은수님 질문에 대한 답

### 2.1 「VIN 높으면 바이패스, 낮아지면 부스트」가 맞는가

**절반만 맞다. 서로 다른 두 가지를 하나로 섞은 이해다.**

| 진술 | 판정 | 근거 |
|---|---|---|
| 입력이 낮아지면 **부스트로 승압**한다 | **맞다** | §5.6 - `VIN < VOUT` 이면 순수 Boost |
| 입력이 출력보다 **크면 입력을 출력으로 바이패스**한다 | **아니다** | §5.6·§5.7 - `VIN > VOUT` 이면 **순수 Buck 으로 강압**해서 `VOUT` 을 `VSET` 값으로 유지한다. 통과시키지 않는다 |
| 그 둘을 **«바이패스 모드»** 하나가 한다 | **아니다** | Bypass 는 **`VIN ≈ VOUT` (±1%) 구간에서만** 자동으로 걸리는 전이 상태다 (§2.2) |

**즉 «높으면 그대로 쓰고 낮아지면 승압» 하는 IC 모드는 존재하지 않는다.** 그 거동이 필요하면 **펌웨어가 배터리 전압을 보고 `FMODE` 를 직접 토글**해야 하고, 그때 쓰는 것이 `Forced Bypass` 인데 **과전류 보호가 없다**(§2.4).

### 2.2 실제로 존재하는 «동작 상태» 셋 (자동 전환)

`FMODE = 0x0`(Normal) 일 때 IC 가 `VIN` 과 `VOUT` 관계를 보고 **스스로** 오간다.

| VIN 대 VOUT | 상태 | 무슨 일이 일어나나 |
|---|---|---|
| `VIN > VOUT` | **Buck** | 스위치 D 상시 닫힘·C 상시 열림. A·B 가 동기 벅으로 동작해 **강압** |
| **`VIN ≈ VOUT` (±1%)** | **Bypass (자동)** | Buck↔Boost 전이를 매끄럽게 하려는 구간. §「Auto Bypass Thresholds `VIN_BYP = ±1% × VOUT`」 (p.9) |
| `VIN < VOUT` | **Boost** | 스위치 A 상시 닫힘·B 상시 열림. C·D 가 동기 부스트로 동작해 **승압** |

> [!NOTE]
> §5.9 원문: *"When the output voltage is close to the input voltage, the ISL9122A rapidly and smoothly switches between Boost, Bypass, and Buck modes to maintain the regulated output voltage."*
>
> **핵심은 «to maintain the regulated output voltage»** 다. 세 상태 전부 **`VSET` 을 지키려는** 동작이지, 입력을 그대로 내보내려는 동작이 아니다.

부하에 따라 **PFM ↔ PWM** 도 자동으로 오간다(§5.8). 가벼우면 PFM(효율), 무거우면 PWM.

### 2.3 `FMODE[3:2]` - I2C 로 고르는 것

**Auto Bypass 를 켜고 끄는 별도 비트는 없다.** 데이터시트 특징란의 *"Selectable Forced and Auto Bypass"* 는 아래 넷 중 고른다는 뜻이다.

| `FMODE` | 이름 | 동작 | 쓸 때 |
|---|---|---|---|
| **`0x0`** | **Normal** (리셋 기본값) | Buck/Bypass/Boost + PFM/PWM **자동 전환** | **평상시. 지금 펌웨어가 쓰는 값** |
| `0x1` | **RESERVED** | — | **절대 쓰지 말 것** (데이터시트 명시) |
| `0x2` | **Forced PWM** | PFM 없이 항상 PWM | 스위칭 주파수 변동을 줄이고 `VOUT` 정확도를 높인다. **입력 전류가 늘어난다** |
| `0x3` | **Forced Bypass** | **스위칭을 끈다.** A·D·E 를 켜서 입력→인덕터→출력 직결 | 전압 규정이 필요 없을 때 소비를 최소화(`IQ` 120nA) |

### 2.4 `Forced Bypass` 의 위험 - 반드시 읽을 것

> [!CAUTION]
> **1. 과전류 보호가 없다.** §5.10 원문: *"Note: There is no overcurrent protection in Bypass mode."* 자극 출력 레일에 이 모드를 걸어 두면 단락 시 보호가 안 된다.
>
> **2. 승압하지 않는다.** 스위칭을 껐으므로 `VIN` 이 떨어지면 `VOUT` 도 같이 떨어진다. 배터리가 빠지면 자극 전압도 같이 빠진다.
>
> **3. 모드 전환을 연달아 하려면 1ms 이상 띄운다.** §5.10 원문: *"If the part has to repeatedly bounce between the Forced Bypass and Regulation modes, there should be at least 1ms delay between the successive mode setting I2C commands."*
>
> **4. I2C 로 IC 를 꺼도 Forced Bypass 는 유지된다.** `EN_AND = 0` 으로 꺼도 Forced Bypass 상태로 남는다(Table 5).

## 3. 레지스터 맵 (전체)

| 주소 | 이름 | 타입 | 리셋 | 내용 |
|---|---|---|---|---|
| `0x02` | `RO_REG1` | R | `0x4C` | 하드웨어 식별 |
| `0x03` | `INTFLG_REG` | R | `0x00` | 고장 플래그. **읽으면 지워진다** |
| `0x11` | `VSET` | R/W | 주문 사양 | 출력 전압 설정 |
| `0x12` | **`CONV_CFG`** | R/W | `0x81` | **모드 포함 변환기 설정** |
| `0x13` | `INTFLG_MASK` | R/W | `0x00` | 과전류 처리 방식·EN 오버라이드 |

### 3.1 `0x02 RO_REG1` (읽기 전용)

| 비트 | 이름 | 값 | 뜻 |
|---|---|---|---|
| 7:6 | `FAMILY_ID` | `0x1` | ISL9122 stand-alone converter family (rev.E 한정) |
| 5:3 | `HW_REV` | `0x4` | 하드웨어 리비전 **E** |
| 2:0 | `RAIL_VAR` | `0x0` | High voltage input Buck-Boost (ISL9122A) |

**정상 부품이면 `0x02` 를 읽었을 때 `0b01_100_000 = 0x60`** 이 나온다. 통신 확인용으로 쓸 수 있다.

### 3.2 `0x03 INTFLG_REG` (읽기 전용, read-clear)

| 비트 | 이름 | 뜻 |
|---|---|---|
| 3 | `INT3` | 전압 설정 **하한 초과** (`VSET` 이 `0x48` 아래로) |
| 2 | `INT2` | 전압 설정 **상한 초과** (`VSET` 이 `0xD7` 위로) |
| 1 | `INT1` | **과열** |
| 0 | `INT0` | **과전류** |

> [!IMPORTANT]
> **읽으면 지워진다.** 1이 읽혔다면 «과거에 있었다» 는 뜻이고, **현재 상태를 보려면 한 번 더 읽어야 한다**(§6.2).

### 3.3 `0x11 VSET`

```
VOUT = VSET × 0.025V          (25mV 스텝)
하한 1.8V   = 0x48 (72)
상한 5.375V = 0xD7 (215)
```

범위 밖을 쓰면 **한계값으로 램프하고 `INTFLG_REG` 의 해당 플래그가 선다.** 램프 속도는 `CONV_CFG` 의 `DVSRATE` 가 정한다.

### 3.4 `0x12 CONV_CFG` - 리셋값 `0x81`

| 비트 | 이름 | 리셋 | 내용 |
|---|---|---|---|
| 7 | `EN_AND` | `1` | `0` = EN 핀 HIGH 로 I2C 만 깨우고 변환기는 안 켠다 (I2C 로 이 비트에 1을 써야 시작) · `1` = EN 핀 HIGH 로 바로 시작 |
| 6 | `DISCH` | `0` | I2C 로 변환기를 껐을 때 방전 저항(약 160Ω) 연결 여부 |
| 5:4 | `DVSRATE` | `0` | 출력 전압 변경 시 슬루율. `0`=3.125 · `1`=6.25 · `2`=0.78125 · `3`=1.5625 mV/µs |
| **3:2** | **`FMODE`** | `0` | **§2.3 참조** |
| 1 | `CONV_RSVD` | `0` | 예약. 쓰지 말 것 |
| 0 | `TYPE1` | `1` | `0` = Type I 오차증폭기(과도응답 우선) · `1` = **Type II**(정상상태 정확도 우선) |

> [!CAUTION]
> **`TYPE1 = 1`(Type II)과 `OC_FAULT_MODE = 0x2/0x3` 을 같이 쓰면 안 된다**(Table 5 주석). 지금 펌웨어는 `TYPE1=1` 이고 `INTFLG_MASK` 를 쓰지 않아 `OC_FAULT_MODE` 가 리셋값 `0x0`(Hiccup)이므로 **문제없다.**

#### 값 조합 참조

| `CONV_CFG` | `FMODE` | 뜻 |
|---|---|---|
| **`0x81`** | `0x0` | **Normal** — 지금 펌웨어 기본값 |
| `0x85` | `0x1` | **RESERVED. 쓰지 말 것** |
| `0x89` | `0x2` | Forced PWM |
| `0x8D` | `0x3` | Forced Bypass |

### 3.5 `0x13 INTFLG_MASK`

| 비트 | 이름 | 리셋 | 내용 |
|---|---|---|---|
| 7:6 | `OC_FAULT_MODE` | `0` | `0`=**Hiccup**(100ms 쉬고 재시도) · `1`=Shutdown(I2C/EN 재시작 필요) · `2`=Current Limit(**Type I 에서만**) · `3`=예약 |
| 5 | `EN_OR` | `0` | `1` = EN 핀 무시하고 계속 enable (푸시버튼 ON 구현용) |

## 4. 보호 기능

| 기능 | 동작 | 출처 |
|---|---|---|
| 과전류 | `OC_FAULT_MODE` 에 따라 Hiccup / Shutdown / Current Limit. **인덕터 피크 전류 감시**(`ILIM` 2.5A, `VIN<2.5V` 면 비례 축소) | §5.4, p.9 |
| 과열 | `TSD` 초과 시 스위칭 정지 → `TSDHYS` 만큼 내려가면 **소프트스타트부터 다시** | §5.5 |
| 저전압 잠금 | `VIN` 이 UVLO 위로 올라오고 EN 이 HIGH 여야 기동 | §5.3 |
| **Bypass 중 과전류** | **없다** | §5.10 |

## 5. 이 제품에서의 실제 동작

| 항목 | 값 | 근거 |
|---|---|---|
| 펌웨어가 쓰는 `CONV_CFG` | **`0x81`** = **Normal** | `tdc_drv_isl9122.h` `TDC_DRV_PMIC_DEFAULTVALUE_CONV_CFG` |
| 리셋 시 `VSET` | `75` → **1.875V** | `TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE` |
| 자극 TX 사용 범위 | `140`~`214` → **3.5 ~ 5.35V** | `TDC_DRV_PMIC_MIN_TX_POWER_VALUE` ~ `TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE` |

**입력이 리튬 배터리(약 3.0~4.2V)이고 출력이 3.5~5.35V 이므로 대부분의 구간에서 `VIN < VOUT` 이다 → 거의 항상 Boost 로 돈다.** 출력을 3.5V 로 낮춘 상태에서 배터리가 만충(4.2V)이면 그때만 Buck 구간에 들어간다.

> [!NOTE]
> `tdc_isd_fpga.c` 의 `tdc_isd_fpga_read_tx_power_level()` · `tdc_isd_fpga_write_change_tx_power_level()` 은 **이름과 달리 FPGA 를 거치지 않는다.** `tdc_drv_isl9122_*_register()` 를 그대로 부르는 얇은 래퍼이고, 실패 디바운스만 얹었다. PMIC 는 CM3 의 I2C 에 직접 붙어 있다.

## 6. 참고한 절

| 절 | 쪽 | 내용 |
|---|---|---|
| Overview · Features | 1 | 전기 사양 요약 |
| Electrical Specifications | 9 | `VIN_BYP = ±1% × VOUT` · `ILIM` |
| §5.1~5.5 | 18 | EN · 소프트 방전 · 기동 · 과전류 · 과열 |
| §5.6~5.8 | 19~20 | 벅-부스트 토폴로지 · PWM · PFM |
| **§5.9~5.10** | **20** | **`VIN≈VOUT` 거동 · Forced 모드** |
| §5.11 | 20~22 | I2C 프로토콜 |
| **§6.1~6.5** | **23~25** | **레지스터 정의** |

## 7. 폴더 이름 오타

이 폴더는 `pmic-isl99122a` 인데 부품명은 **`ISL9122A`** 다(`9`가 하나 많다). 링크가 걸려 있어 바꾸지 않았다. 정리하려면 폴더명과 이 문서의 참조를 함께 고쳐야 한다.
