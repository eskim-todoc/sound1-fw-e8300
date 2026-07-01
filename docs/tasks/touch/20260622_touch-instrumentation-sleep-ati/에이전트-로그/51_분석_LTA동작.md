---
name: 51_분석_LTA동작
purpose: IQS323 self-cap LTA 필터 동작 정밀 분석 — 약결합(D=39 고정) 실기 증상 원인 규명
type: agent-log
maturity: experimental
tags: [touch, iqs323, lta, fast-filter-band, prox, beta, ati-error, freeze]
---

# 51 분석 — LTA 동작 정밀 분석 (D=39 고정 증상 원인 규명)

> **TL;DR**: D=39 고정(LTA 수렴 불가) 증상은 LTA freeze 단독이 아니라 Fast Filter Band + prox 임계 기본값 + LTA freeze 3중 메커니즘의 합작이다. 데이터시트 §5.6 원문 인용 기반으로 각 조건을 순서대로 격리·판정한다.

---

## 0. 실기 ground truth (입력)

```
[T] LTA=394  CNT=355  D=39  THR=156 (k=102  H=8)  not touch  delta <= abs_thr
```

- `D = LTA − Counts = 394 − 355 = 39` — 수 분간 불변
- 터치 미판정 (D=39 < THR=156)
- IC 전력 모드: 항상 NP (`PM_TIMEOUT=0`, `Automatic No ULP`)

---

## 1. LTA freeze 조건 — 데이터시트 §5.5 직접 인용

> **§5.5 원문**:
> "LTA is updated slowly to track environmental changes and is **frozen during touch and proximity events**."

freeze 발동 조건은 `touch 이벤트` 또는 `proximity 이벤트`다. 판정 기준은 각각:

- **Touch 진입** (§5.7): `(LTA − Counts) > Touch Threshold`
- **Prox 진입** (§5.7): `(LTA − Counts) > Prox Threshold`

System Status(0x10) 레지스터 비트(A.2):

| 비트 | 필드 |
|---|---|
| 9 | CH0 Touch |
| 8 | CH0 Prox |

**LTA는 CH0_Touch bit 또는 CH0_Prox bit가 set된 동안 동결된다.**  
Touch threshold(THR=156)는 D=39 < 156이므로 미초과. → Touch freeze 해당 없음.

---

## 2. Prox 임계 기본값과 LTA freeze 가능성 (핵심 가설)

### 2.1 Prox Settings 레지스터 기본값

레지스터 A.16 (0x61 Prox Settings, 기본값 0x0000):

```
bits[7:0]  Prox Threshold = 0x00 = 0
bits[15:12] Prox Debounce Exit  = 0x0 = disabled
bits[11:8]  Prox Debounce Enter = 0x0 = disabled
```

코드(`tdc_touch_iqs323.c`)에서 0x61은 **명시적으로 기록되지 않는다**. 기본값 0x0000이 그대로 적용됨.

**Prox Threshold = 0이면 판정 조건:**

```
(LTA − Counts) > 0
```

즉 D > 0 인 순간 즉시 prox 진입이다. D=39 > 0 이므로 **CH0_Prox bit가 항상 set 상태**.

> **결론**: Prox 임계 기본값(0)이 D=39인 상황에서 prox 상태를 유발하고, §5.5의 "proximity events 중 LTA frozen" 규칙에 의해 **LTA가 완전히 동결**된다.

### 2.2 Debounce 미설정의 영향

Prox Debounce Enter/Exit = 0 (disabled). Debounce 없이 단 1샘플만에 prox 진입·이탈한다. D=39가 지속되는 한 prox는 해제되지 않는다.

---

## 3. Fast Filter Band(10)의 역할과 감소 방향 판정

### 3.1 데이터시트 §5.6 원문 인용

> "**Fast Filter Band**: determines when fast beta filtering is used. When counts have drifted from the LTA in the **sensing direction opposite to the normal sensing direction** (opposite to what would normally cause a touch) by more than the Fast Filter Band, fast filtering is applied to the LTA. When the difference between counts and LTA is less than the fast filter band, normal filtering resumes."

핵심 구절: **"sensing 반대 방향(normal sensing direction의 반대)으로 Fast Filter Band 이상 벗어날 때"** fast beta 적용.

### 3.2 self-cap 방향 매핑

- self-cap 터치 시: counts 감소 → D = LTA − Counts **증가**
- 따라서 **normal sensing direction = counts 감소(D 증가) 방향**
- **반대 방향 = counts 증가(D 감소, 즉 counts > LTA 영역)**

### 3.3 D=39 상황에서의 적용 판정

D=39 는 D > 0, 즉 counts < LTA. 이것은 normal sensing direction(counts 감소)이다.  
→ Fast Filter Band 조건(`counts가 LTA보다 sensing 반대 방향으로 Band 이상`)에 **해당하지 않는다**.

**현재 Fast beta(0x02)는 적용 중이 아니다. Normal LTA beta(0x10)가 활성이다.**

### 3.4 Normal beta=16 수렴 속도 계산

```
alpha = 16 / 256 = 0.0625 (6.25%/샘플)
```

beta=16이면 이론상 충분히 빠른 수렴이다. 수렴이 안 되는 건 beta 값의 문제가 아니라 LTA 자체가 동결되어 있기 때문이다(2항 결론: prox freeze).

---

## 4. ATI Error 발동 가능성

### 4.1 Automatic Re-ATI 조건 (§5.10)

> "re-ATI is performed when the channel LTA drifts outside of the ATI Band"
> Re-ATI Boundary = ATI Target ± ATI Band

ATI Setup(0x36) = 0x040C (A.12):
- ATI Resolution Factor = 64 → ATI Target = ATI Base × (64/16) = 100 × 4 = **400 counts**
- ATI Band = Large (bit3=1) = 1/8 × ATI Target = 1/8 × 400 = **50 counts**
- Re-ATI Boundary = 400 ± 50 = [350, 450]

현재 LTA=394: 350 ≤ 394 ≤ 450 → **LTA가 ATI Band 내에 있다**.

→ **Auto Re-ATI 발동 조건 미충족**. ATI error도 발생하지 않는다.

### 4.2 ATI Error 정의 (§5.11)

> "ATI Error: set if, after ATI is performed, Counts are outside the Re-ATI Boundary"

ATI error는 ATI 실행 직후 counts를 체크하는 것이다. 런타임 중 LTA drift로 발생하는 것이 아니다.  
현재 LTA가 band 내에 있으므로 ATI error도 발생하지 않는다.

**ati_error=0이면 SW Re-ATI 게이트(tdc_touch_logic.c 154~161줄)는 발동하지 않는다.**

---

## 5. Channel Timeout(§5.8) — prox 강제 해제 메커니즘

§5.8:
> "If a channel has been in prox/touch for longer than the time specified by the Event Timeouts register, it will be reseeded and exit that state."

Event Timeouts(0xD2, A.31) — 코드에서 명시적으로 기록 안 함 → **기본값 0x0000**.

```
bits[15:8] Touch Event Timeout = 0 × 512ms = 0 → disabled
bits[7:0]  Prox Event Timeout  = 0 × 512ms = 0 → disabled
```

**Channel Timeout이 비활성화**되어 있으므로 prox 상태가 아무리 길어도 강제 Reseed가 일어나지 않는다.

또한 System Control MSB(0x07): CH0~2 Timeout Disable bit가 모두 set이므로, 설령 0xD2에 값을 쓰더라도 타임아웃이 동작하지 않는다.

---

## 6. SW stuck 에스컬레이션 — NOT_TOUCH 에서의 비동작 확인

`tdc_touch_logic.c` `stuck_eval()`:

```c
if (curr_state != TDC_TOUCH_STATE_TOUCH)  /* 노터치/해제 -> 리셋 */
{
    st->stuck_stage     = 0;
    st->stuck_anchor_ms = now_ms;
    return TDC_TOUCH_ACT_NONE;
}
```

curr_state=NOT_TOUCH이면 stuck 에스컬레이션은 즉시 리셋만 하고 아무 액션도 없다. NOT_TOUCH 상태에서는 SW 차원의 자동 복구 경로가 없다.

---

## 7. 종합 판정 — 3중 고착 메커니즘

| 번호 | 원인 | 데이터시트 근거 | 현 설정 |
|---|---|---|---|
| **원인_1** | **Prox threshold=0** → D>0이면 무조건 prox 진입 | §5.7: `(LTA−Counts) > Prox Threshold` | 0x61 미기록 → 기본값 0x00 |
| **원인_2** | **LTA freeze** — prox 이벤트 중 LTA 갱신 중단 | §5.5: "frozen during touch and proximity events" | CH0_Prox bit 상시 set |
| **원인_3** | **Channel Timeout 비활성** → prox stuck 강제 Reseed 없음 | §5.8: Event Timeouts | 0xD2=0x0000, CHx Timeout Disable=0x07 |

Fast Filter Band(10)는 이 고착에 직접 기여하지 않는다. D=39는 normal sensing 방향(counts < LTA)이므로 Fast beta 조건 자체가 발동 안 된다.

---

## 8. LTA 수렴이 일어나는 조건 (데이터시트 메커니즘)

데이터시트에 명시된 강제 재동기화 방법:

| 방법 | 레지스터 | 조건 |
|---|---|---|
| **Channel Timeout Reseed** (§5.8) | 0xD2 Prox Event Timeout > 0 + CHx Timeout Disable=0 | prox 상태 X초 경과 → 자동 Reseed + prox 해제 |
| **SW Reseed** (§5.5.1) | System Control bit3 | 마스터가 언제든 명령 가능 |
| **SW Re-ATI** (§5.9) | System Control bit2 | ATI 재실행 → counts 재보정 → LTA 수렴 |
| **Auto Re-ATI** (§5.10) | LTA가 [350, 450] 벗어날 때 자동 | 현재 LTA=394: band 내 → 미발동 |

현 코드에서 NOT_TOUCH+prox stuck 상황에 자동 개입하는 SW 경로는 없다.  
(ati_error=0이므로 Re-ATI 게이트 미발동; stuck_eval은 NOT_TOUCH에서 비동작)

---

## 9. 은수님 질문 직접 답변

### 질문_1: 터치 미판정(D=39<THR=156)인데 LTA가 수렴 안 하는 이유

**답**: D > 0 → Prox threshold(기본값 0) 초과 → CH0_Prox=1 → §5.5 "proximity events 중 LTA frozen" 적용. LTA beta=16이 아무리 빨라도 동결 상태에서는 수렴 연산 자체가 실행되지 않는다.

### 질문_2: D=39면 LTA band 이탈인가? Auto ATI 발동인가?

**답**: Re-ATI Boundary = [350, 450]. LTA=394는 band 내부. Auto Re-ATI 발동 조건 **미충족**. Auto Re-ATI는 LTA drift를 체크하는 것이고 현재 LTA는 ATI 직후 상태에서 거의 변하지 않았다.

### 질문_3: ATI error → Re-ATI 필요한가?

**답**: ATI error는 ATI 실행 완료 직후 counts가 Re-ATI Boundary 밖일 때 set된다(§5.11). 현재 LTA=394, counts=355 모두 [350,450] 내에 있으므로 ATI error 미발생. SW Re-ATI 게이트(ati_error 조건)도 미발동.

---

## 10. 조치 방향 (참고)

1. **Prox threshold 명시 설정**: 0x61에 적절한 prox threshold 기록(예: 0x0A~0x14 범위). D > prox_thr일 때만 prox 진입되도록 제어.
2. **또는 Channel Timeout 활성화**: 0xD2에 Prox Event Timeout(예: 0x02 = 1024ms)을 설정하고 System Control MSB의 CH0 Timeout Disable을 clear. 단 §5.8 경고에 따라 ULP 미사용 확인(현재 Automatic No ULP로 이미 충족).
3. **또는 SW 폴링 Reseed**: NOT_TOUCH이면서 D > 임계 지속 시 SW에서 주기적 Reseed 발행(현 stuck_eval NOT_TOUCH 경로에 추가).

> [!WARNING]
> 조치 1(prox threshold 설정)은 가장 근본 수정이다. prox threshold > 0으로 올리면 D=39가 prox 진입을 유발하지 않고 LTA 수렴이 정상 동작한다. 단 prox 기능을 의도적으로 사용한다면 threshold를 너무 낮추면 안 된다.
