---
name: 52_분석_ATI동작
purpose: IQS323 ATI / ATI Error / Re-ATI 정밀 분석 — LTA vs ATI Band 개념 분리, D=39 약결합 상태의 IC 판정 근거 도출
type: 분석
maturity: stable
tags: [touch, iqs323, ati, ati-error, re-ati, lta, fast-filter-band, analysis]
---

# ATI 동작 정밀 분석

> **TL;DR**: D=39 약결합(LTA=394, Counts=355)은 IC가 '정상'으로 보는 상태. LTA는 Fast Filter Band(10) 때문에 수렴이 차단됨. 'LTA 밴드'(터치 판정용 delta)와 'ATI Band'(드리프트 감지용 LTA 허용범위)는 완전히 별개 개념. D=39는 ATI Error 트리거 조건과 무관.

---

## 0. 계측값 · 설정 ground truth 정리

| 항목 | 값 | 출처 |
|---|---|---|
| Counts (CH0 Filtered) | 355 | RTT 계측 |
| LTA | 394 | RTT 계측 |
| D = LTA − Counts | 39 | RTT 계측 |
| Touch Threshold (절대) | ~156 | `0x62` k=102, 공식 k×LTA/256≈156 |
| Fast Filter Band | 10 counts | `0xB4 = 0x000A` |
| ATI Base | 100 counts | `0x37 = 0x0064` (DS §9 기본값 그대로) |
| ATI Resolution Factor | 64 | `0x36 = 0x040C` bits[15:4] |
| ATI Target | ≈400 counts | = Base × (ResolutionFactor / 16) = 100 × 4 |
| ATI Band | Large = 1/8 × ATI Target | `0x36` bit3=1 → DS A.12 |
| ATI Band 경계 | 350 ~ 450 counts | ±(1/8 × 400) = ±50 |
| ATI Mode | Full | `0x36` bits[2:0]=100 |
| LTA Normal Beta (NP/LP) | 16 / 16 | `0xB1 = 0x1010` |
| LTA Fast Beta (NP/LP) | 2 / 2 | `0xB2 = 0x0202` |
| Power Mode | Automatic No ULP, PM Timeout=0 → 항상 NP | `0xC0`, `0xC5=0` |

---

## 1. ATI Target과 ATI Band 정의

### 1.1 ATI Target

데이터시트 §5.9 공식:

```
ATI Target = ATI Base × (ATI Resolution Factor / 16)
           = 100 × (64 / 16)
           = 400 counts
```

ATI Full Mode가 완료되면 **CH0 Counts ≈ 400** 이 되도록 Multiplier/Divider·Compensation을 IC가 자동 선택한다.

ATI 완료 후 Reseed가 즉시 뒤따라 (`0xC0 bit3`) LTA ← 현재 Counts ≈ 400으로 동기화된다.
계측값 LTA=394는 ATI Target 400에서 약 6 counts 낮은 실제 캘리브레이션 결과다.

### 1.2 ATI Band — 드리프트 감지 경계

데이터시트 §5.10, A.12:

```
ATI Band 설정  : Large = 1/8 × ATI Target   (0x36 bit3=1)
Band 크기       : 1/8 × 400 = 50 counts
Re-ATI 경계    : ATI Target ± Band
                = 400 ± 50
                = 350 ~ 450 counts
```

> [!IMPORTANT]
> **ATI Band의 비교 대상은 LTA이지 Counts(또는 D)가 아니다.**
>
> 데이터시트 §5.10 원문:
> "re-ATI is triggered when the channel's **LTA** drifts outside of the ATI Band (centred on the ATI Target)"
>
> 조건 요약:
> - `LTA > 450` 또는 `LTA < 350` → Automatic Re-ATI 발동
> - **Counts, D=LTA−Counts 는 ATI Band 판정에 관여하지 않는다**

---

## 2. 'LTA 밴드'(사용자 표현) vs 'ATI Band' — 개념 분리

사용자가 언급한 "LTA 밴드를 벗어난다(D=39)"는 터치 감지용 delta 개념이다.
이를 ATI Band와 혼동하면 안 된다. 두 개념은 **목적·비교 대상·수치 모두 다르다**.

| 항목 | 터치 판정 delta | ATI Band |
|---|---|---|
| **수식** | D = LTA − Counts | LTA ∉ [ATI_Target ± Band] |
| **비교 대상** | Counts vs LTA | LTA vs ATI Target |
| **목적** | 터치 감지 | 런타임 드리프트 감지 → Re-ATI |
| **Sound1 현재값** | D = 39 | LTA = 394, Band = [350, 450] |
| **해당 상황** | D(39) < THR(156) → NOT_TOUCH | LTA(394) ∈ [350, 450] → 정상 범위 |
| **레지스터** | 0x62 (Touch Settings) | 0x36 bit3 (ATI Band), 0x37 (ATI Base) |

**결론**: D=39가 "LTA 밴드를 벗어난다"는 표현은 터치 판정 맥락에서는 맞지만(delta가 IC 내부 Fast Filter Band 10을 초과), ATI Band 맥락에서는 별개 개념이다. D=39는 ATI Error/Re-ATI 트리거와 **무관**하다.

---

## 3. LTA가 수렴하지 않는 이유 — Fast Filter Band 메커니즘

### 3.1 IIR 필터 2종

데이터시트 §5.6:

```
Normal LTA 필터: LTA_new = LTA + (Counts − LTA) × (Beta_Normal / 256)
Fast LTA 필터:  LTA_new = LTA + (Counts − LTA) × (Beta_Fast / 256)
```

Sound1 설정:
- LTA Normal Beta = 16/256 ≈ 6.25% per tick (NP)
- LTA Fast Beta   = 2/256  ≈ 0.78% per tick

### 3.2 Fast Filter Band 10의 역할 (핵심)

데이터시트 §5.6:

> "Fast Filter Band determines when the fast beta filter is used. If counts drifts by more than the Fast Filter Band from the LTA in the **sensing opposite direction**, fast filtering is applied. When the difference between counts and LTA is less than the Fast Filter Band, normal filtering resumes."

**"sensing opposite direction"** = 터치 방향의 반대, 즉 self-cap에서는 Counts가 LTA **위로** 올라가는 방향.

현재 상황 D=39:
- Counts = 355, LTA = 394
- **Counts < LTA** (Counts가 LTA 아래) → 이것은 **터치 방향** (sensing direction)
- Fast Filter Band 조건 = Counts가 LTA 위로 10 이상 벗어날 때 (sensing opposite direction)

### 3.3 Normal Beta 16에서 왜 수렴 안 되나?

Normal Beta=16일 때 LTA 수렴 속도:

```
매 tick 이동량 = (Counts − LTA) × (16/256)
              = (355 − 394) × 0.0625
              = −39 × 0.0625
              = −2.44 counts/tick
```

즉 LTA는 매 측정마다 2.44 counts씩 Counts(355) 방향으로 이동해야 한다. 그런데 사용자 관측에서 "몇 분째 LTA=394 고정"이다.

**이유 — 터치 상태 판정과 LTA 동결 규칙**

데이터시트 §5.5:
> "LTA is updated to track environmental changes, and is **frozen during touch and proximity events**."

현재 D=39 < THR=156이므로 IC가 "NOT_TOUCH"로 보는 게 맞다. 그렇다면 LTA 동결 조건(터치 이벤트)이 없으니 수렴해야 하는데…

**실제 원인 추정 2가지**:

#### 가설_1: Prox 상태 진입 (가장 유력)

0xD3 Events에서 `Prox Event` bit = 0으로 설정되어 **SW에게 prox 이벤트는 보고되지 않는다**. 그러나 IC 내부적으로 Prox 상태가 set될 수 있다.

코드(`0xD3 = 0x52`):
```
bit0 Prox Event = 0  ← RDY 인터럽트 미발생, 하지만 내부 상태는 존재
```

Prox 임계(0x61 Prox Settings) = IC 기본값 0x0000. 데이터시트 A.16에 따르면 Prox Threshold 8-bit = 0이면 임계=0. D=39 > 0 이므로 이론적으로 Prox 상태 진입 가능성이 있다.

**Prox 상태 진입 시**: LTA 동결 → D=39가 영구 유지 → "몇 분째 고정" 완벽 설명.

> [!NOTE]
> Prox Threshold=0의 실제 동작은 데이터시트에 명시 없음. 0이 "무한 임계(= prox 비활성)"인지 "0 이상이면 진입"인지 IC 내부 구현 의존. Sound1이 prox를 SW에서 무시하나 IC 내부 prox 상태 자체는 설정에 따라 활성일 수 있다.

#### 가설_2: 핀셋 물체 고정에 의한 Counts 안정화

핀셋이 전극 위에 고정되어 Counts = 355가 안정적으로 유지되면, LTA는 Beta=16이더라도 수렴하려 할 텐데 관측에서는 안 된다. 이는 가설_1(Prox 동결)을 더 지지한다.

**실제적 결론**: LTA=394가 몇 분째 수렴하지 않는 가장 유력한 원인은 **IC 내부 Prox 상태 진입으로 LTA가 동결된 것**이다. Prox 설정을 명시적으로 0x61에 Write하지 않았으므로 IC 기본값(0x0000, Threshold=0)이 적용된 상태다.

---

## 4. ati_error 비트 set 조건과 D=39와의 관계

### 4.1 데이터시트 정의

데이터시트 §5.11:

> "After the ATI algorithm has run, error checking is performed. For any channel, if the counts is outside of the Re-ATI boundary at the time the ATI algorithm completes, then the ATI Error bit in System Status is set."

**ati_error set 조건 요약**:
1. ATI 알고리즘이 **실행 완료된 후** 에러 검사를 수행
2. 그 시점에 Counts가 Re-ATI 경계 [350, 450] 밖에 있으면 set

**조건 도식**:
```
ATI 실행 완료 → Counts ∈ [350, 450]?
   Yes → ati_error = 0 (정상)
   No  → ati_error = 1 (ATI Error)
```

### 4.2 D=39 상황에서 ati_error 여부

현재 상태:
- Counts = 355
- ATI Band = [350, 450]
- **355 ∈ [350, 450]** → ATI 직후에는 정상 범위

D=39(= LTA 394 - Counts 355)는 ATI Error 판정과 **무관**하다. ATI Error는 오직:
- "ATI 알고리즘 실행 완료 시점의 Counts 절대값"이 ATI Band 경계를 벗어났을 때만 set된다

현재 Counts=355는 ATI Band [350, 450] **내부**이므로, 부팅 ATI가 정상 완료되었다면 **ati_error는 set되지 않는다**.

> [!IMPORTANT]
> D=39가 어떤 값이든, Counts가 ATI Band 내에 있는 한 ati_error는 set되지 않는다.
> ati_error는 ATI 재보정 후 결과가 목표 범위를 벗어났다는 의미이지, 현재 터치 delta가 크다는 의미가 아니다.

---

## 5. Auto Re-ATI 조건 — IC 자동 vs 마스터 수동

### 5.1 Automatic Re-ATI (§5.10)

데이터시트 §5.10:

> "IQS323 detects when a channel has drifted outside of its designed operating range and automatically triggers a re-ATI."
>
> "Re-ATI is triggered when the channel's LTA drifts outside of the ATI Band"

조건: `LTA > 450` 또는 `LTA < 350` → IC가 **자동으로** Re-ATI 실행.

현재 LTA = 394 ∈ [350, 450] → **Automatic Re-ATI 미발동**.

### 5.2 ATI Error 후 Re-ATI (§5.11)

데이터시트 §5.11:

> "When ATI Error is set, **re-ATI is not automatically triggered**. The master must trigger re-ATI by setting the Re-ATI bit in System Control."

**핵심**: ATI Error 발생 후 자동 복구 없음. 마스터(MCU)가 `0xC0 bit2 = 1`로 수동 트리거해야 한다.

### 5.3 Sound1 Re-ATI 게이트 분석

`tdc_touch_logic.c` Re-ATI 게이트 조건 (L154~161):
```c
if (curr_state == TDC_TOUCH_STATE_NOT_TOUCH
    && !in->ati_active && in->ati_error          // ← ati_error 필수
    && in->now_ms >= st->re_ati_cooldown_until_ms)
```

ati_error = 0인 현 상황에서 이 게이트는 **발동하지 않는다**. 이는 설계 의도대로다.

### 5.4 종합 — Auto Re-ATI가 도는 정확한 경로

```
경로_1: LTA가 ATI Band 밖으로 drift
         → IC 자동 Re-ATI (마스터 관여 없음)
         → ATI Event bit set (마스터 폴링 시 clear)

경로_2: ATI 실행 후 Counts가 ATI Band 밖
         → ati_error set
         → Re-ATI 자동 미발동 (§5.11)
         → 마스터가 Re-ATI bit 수동 set 필요
         → Sound1: tdc_touch_logic Re-ATI 게이트 (ati_error 조건) 발동

경로_3: 부팅 시 명시 Re-ATI
         → tdc_touch_iqs323_apply_settings() 내 0xC0 bit2 write
         → wait_ati_done_blocking() 완료 대기
```

---

## 6. 현 상태(LTA=394, Counts=355, D=39)에 대한 IC의 판정

| 항목 | 판정 | 근거 |
|---|---|---|
| **터치 상태** | NOT_TOUCH | D(39) < THR(156) |
| **ATI Band** | 정상 범위 | Counts(355) ∈ [350, 450] |
| **ati_error** | 0 (unset) | ATI 완료 시 Counts가 Band 내 |
| **Automatic Re-ATI** | 미발동 | LTA(394) ∈ [350, 450] |
| **Re-ATI 게이트** | 비활성 | ati_error=0 |
| **LTA 수렴 여부** | 동결 (유력: Prox 상태 진입) | Prox Threshold=0(기본값), D=39 > 0 → Prox 상태 가능 |

**IC는 현재 상태를 '정상 NOT_TOUCH'로 인식한다.** LTA가 수렴하지 않는 이유는 ATI/Re-ATI 문제가 아니라 Prox 상태 진입에 의한 LTA 동결 가능성이 가장 높다.

---

## 7. 사용자 의문 항목별 직접 답변

### 의문_1: "터치가 아닌데 LTA가 Counts에 수렴 안 됨"

**답**: IC 내부 Prox 상태 진입이 유력 원인. Prox 상태는 SW 이벤트 비활성(0xD3 bit0=0)과 무관하게 내부적으로 LTA를 동결시킨다. 0x61 Prox Settings를 명시적으로 Write하지 않아 Prox Threshold=0(기본값)이 적용됨. D=39 > Prox Threshold=0이면 prox 조건 충족 가능.

### 의문_2: "D=39면 'LTA 밴드'를 벗어나는 것 아닌가?"

**답**: "LTA 밴드(Fast Filter Band)"와 "ATI Band"는 다른 개념.
- Fast Filter Band(10) = LTA가 Counts보다 **위로** 10 이상 벗어날 때 빠른 LTA 필터 적용. 현재 LTA가 Counts 아래인 방향이므로 Fast Filter Band 조건도 해당 없음.
- ATI Band([350, 450]) = LTA 드리프트 감지 경계. D=39는 이 비교에 관여하지 않음.

### 의문_3: "auto ATI가 안 돌면 ATI 에러가 나고 Re-ATI 해야 하는가?"

**답**: 순서가 반대다.
- ATI Error = ATI **실행 후** Counts가 Band 밖일 때 set
- Automatic Re-ATI = LTA가 Band 밖일 때 발동 (ati_error와 독립)
- 현재 상황: LTA(394) ∈ [350, 450] → Automatic Re-ATI 미발동. Counts(355) ∈ [350, 450] → ati_error = 0. 따라서 Re-ATI 불필요.

---

## 8. 파생 리스크 — Prox 상태 진입 확인 필요

현재 0x61이 IC 기본값 0x0000(Prox Threshold=0)으로 방치된 상태다. Prox가 활성이면:
1. 약결합 물체(핀셋 등)가 전극에 닿을 때 Prox 상태 진입 → LTA 동결 → D=39 고정
2. Prox 이벤트는 SW에 보고되지 않으므로 이 상태를 감지할 방법이 없음
3. Prox 상태가 영구 지속되면 LTA가 환경 드리프트를 추적하지 못함

**권고**: 0x61에 Prox Threshold = 0xFF(사실상 무한대, prox 비활성화와 동등) 또는 Prox Debounce Enter/Exit를 명시 Write해서 Prox 상태를 의도적으로 제어할 필요가 있음. 이는 별도 작업으로 분리 권장.
