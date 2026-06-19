---
name: ati-lta-convergence-검증
purpose: 명제_I(auto-ATI=Re-ATI 동일성) 및 명제_J(ATI Disabled 상태에서 LTA IIR만으로 터치 판정 성립 여부) 데이터시트 근거 검증
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, auto-ati, re-ati, lta, iir, fixed, threshold, delta, 명제_I, 명제_J, datasheet]
---

# 12 — ATI·LTA 수렴 검증가 (명제_I · 명제_J)

> **TL;DR**: 명제_I(auto-ATI=Re-ATI, 큰 맥락에서 동일) — **조건부 참**. 전원 ON 시 IC가 자체 실행하는 Auto-ATI(POR ATI)와 런타임 LTA drift에 의해 발동되는 Re-ATI는 사용하는 ATI 알고리즘은 동일하나 트리거 경로·시점이 다르다. 명제_J(ATI Disabled + LTA IIR만으로 터치 성립 여부) — **참**. 터치 판정식은 `(LTA - Counts) > Touch Threshold`로 ATI Full과 무관하며, LTA IIR이 노터치 기준선을 추적하는 한 threshold 비교는 항상 유효하다. ATI Disabled로 Re-ATI가 없어도 터치 판정 자체는 성립한다.

> [!IMPORTANT]
> 근거: 데이터시트 `02_proxfusion동작.md` §5.5·§5.7·§5.9·§5.10·§5.11, `06_레지스터레퍼런스.md` A.2·A.12, 코드 `tdc_drv_iqs323.c` L334~378·L651·L1102~1158. 추정은 [추정] 표기, DS 미명시는 [DS 미규정] 표기.

---

## 1. 명제_I 검증 — auto-ATI와 Re-ATI는 같은 말인가

### 1.1 용어 정의 (데이터시트 원문 기반)

| 용어 | 데이터시트 위치 | 정의 |
|---|---|---|
| **ATI (Automatic Tuning Implementation)** | §5.9 | IC가 MULT(Divider·Multiplier)와 COMP(Compensation) 값을 자동 산출해 noTouch counts를 ATI Target에 정규화하는 알고리즘. 외부 부품 변경 없이 기기 간 편차를 흡수. |
| **Auto-ATI (POR ATI)** | 코드 주석 L334~378 | 전원 ON(MCLR 리셋) 후 IQS323이 자체적으로 수행하는 최초 1회 ATI. `Reset Event` bit(A.2 bit7)가 set된 동안 ATI Active(A.2 bit5)가 유지된다. |
| **Re-ATI (Automatic Re-ATI)** | §5.10 | 런타임 중 채널 LTA가 `ATI Band` 경계를 이탈할 때 자동 재발동되는 ATI. `System Control`의 `Re-ATI` bit(A.30 bit2)로도 수동 트리거 가능. |

### 1.2 동일성 판정

**알고리즘 레벨에서 동일**: ATI 알고리즘(MULT·COMP 자동 산출 → counts를 ATI Target으로 수렴) 자체는 Auto-ATI와 Re-ATI가 완전히 같다. 데이터시트 §5.9는 단일 ATI 알고리즘을 서술하며, 발동 경로를 구분하지 않는다.

**트리거 경로는 다르다**:

```
Auto-ATI (POR):  전원 ON → MCLR 리셋 → IC 내부 자동 실행
                  → Reset Event bit(A.2 bit7) SET 유지 중 ATI Active(bit5) SET
                  → ATI 완료 후 ATI Active CLEAR

Re-ATI (Runtime): LTA가 [ATI Target ± ATI Band] 경계 이탈
                  → ATI Event bit(A.2 bit4) SET
                  → ATI 알고리즘 재실행
                  → ATI Active(bit5) SET → 완료 후 CLEAR
```

**결론**: "큰 맥락에서 같은 말"이라는 명제_I는 **조건부 참**. 동일 알고리즘을 사용한다는 점에서 동류지만, Auto-ATI는 POR 시 1회성·강제 실행이고 Re-ATI는 LTA drift 조건부 런타임 재실행이므로 완전히 동일한 개념은 아니다.

### 1.3 전원 ON 시 터치 상태에서 auto-ATI 발동 메커니즘

데이터시트 §5.9 원문: "ATI 알고리즘은 짧은 시간에 실행되어 사용자가 인지 못 함."

코드 `tdc_drv_iqs323.c` L334~378 주석:
> "MCLR 리셋 후 IQS323이 자체적으로 Auto-ATI를 수행한다. ATI Active 플래그가 0이 될 때까지 폴링하여 Auto-ATI 완료를 확인한다."

L376 경고 로그: `"[TOUCH] AUTO-ATI: TIMEOUT (터치 중이면 정상)"`

이 주석이 핵심 근거다: **전원 ON 시 터치 상태이면 Auto-ATI가 수렴하지 못하고 timeout이 발생하는 것이 "정상(expected)"으로 간주된다.** 이유는 다음과 같다:

```
전원 ON + 터치 중:
  → MCLR 리셋 → IC Power-On, counts 측정 시작
  → counts = 터치 상태의 낮은 값 (예: 200)
  → ATI 알고리즘: 이 counts(200)를 ATI Target(400)으로 맞추려 MULT/COMP 조정
  → 그러나 ATI Target=400을 counts=200으로 내리려면 MULT를 크게 올려야 함
  → 물리 한계 또는 ATI 알고리즘 수렴 실패 → ATI_ERROR(A.2 bit6) 또는 timeout
  → ATI Active가 내려오지 않음
```

§5.11 ATI Error 조건: "ATI 완료 시점에 Counts가 Re-ATI Boundary 밖이면 ATI Error bit SET."

즉 터치 중 Auto-ATI는 counts를 ATI Target으로 맞출 수 없어 수렴 실패 가능성이 높다. 현 펌웨어는 이 상황을 timeout(예상 범위 내 정상)으로 처리하고 계속 진행한다(L376). 이후 초기화 시퀀스에서 RESEED(L1147)를 실행해 LTA를 현재 counts(터치 상태 포함)로 동기화한다.

---

## 2. 명제_J 검증 — ATI Disabled + LTA IIR만으로 터치 판정 성립 여부

### 2.1 터치 판정식 (데이터시트 원문, §5.7)

데이터시트 §5.7 — 터치 진입 조건:

$$\text{(LTA - Counts)} > \text{Touch Threshold}$$

이 식에 ATI 관련 변수는 등장하지 않는다. **판정의 두 주체는 LTA와 Counts이며, threshold는 레지스터에 고정 기록된 값이다.**

| 변수 | 역할 | ATI와의 관계 |
|---|---|---|
| **Counts** | 매 측정마다 IC가 출력하는 raw 센서값 | ATI는 Counts가 noTouch 기준에서 ATI Target에 정규화되도록 MULT/COMP 조정. ATI 없으면 정규화 안 됨 |
| **LTA** | 노터치 기준선(IIR 추적) | ATI와 무관. §5.5에 따라 매 측정마다 `LTA_new = LTA_old + (Counts - LTA_old) × (Beta/256)` IIR 갱신 |
| **Touch Threshold** | A.17 Touch Settings 레지스터 기록값 | ATI와 무관. 사용자/펌웨어가 직접 기록 |

### 2.2 ATI Disabled 상태에서의 동작 구조

현 펌웨어: `ATI_SETUP_LSB = 0x08` → bits[2:0] = `000` = **ATI Mode Disabled** (코드 L651, 주석 참조).

Disabled 상태에서 IC 동작:
- MULT(Divider·Multiplier)와 COMP(Compensation) 값이 레지스터 기록값에 **고정** (자동 조정 없음)
- noTouch counts는 고정 MULT/COMP에 의해 결정된 값에 머묾 (ATI Target에 정규화 안 됨)
- LTA는 §5.5 IIR로 계속 갱신: `LTA_new = LTA_old + (Counts - LTA_old) × Beta/256`
- 터치 인식 기준: `(LTA - Counts) > Touch Threshold` — **동일하게 유효**

**핵심**: ATI가 하는 일은 noTouch counts를 ATI Target으로 정규화하는 것이다. LTA는 그 noTouch counts를 추적한다. 터치 판정은 LTA(추적된 noTouch 기준)와 현재 Counts의 차이(delta)를 threshold와 비교한다. 따라서:

```
ATI Full 있을 때:
  noTouch Counts ≈ ATI Target(400) → LTA ≈ 400
  터치 시 Counts ≈ 200 → Delta = 400-200 = 200 > Threshold(156) → 터치 인식

ATI Disabled 있을 때 (현 펌웨어):
  noTouch Counts = 고정 MULT/COMP 결과값 (예: 380~420 — 실측값 근거)
  → RESEED 후 LTA ≈ noTouch Counts (예: 400)
  → 터치 시 Counts 감소 → Delta = (noTouch Counts - 터치 Counts) > Threshold → 터치 인식
```

양쪽 모두 **터치 판정식은 동일하게 성립**한다. 차이는 noTouch Counts의 절대값이 기기 간 편차를 가질 수 있다는 것뿐이며, LTA가 그 개별 기기의 noTouch 기준선을 추적하기 때문에 delta(= LTA - Counts) 계산은 여전히 유효하다.

### 2.3 LTA IIR이 ATI Band를 벗어날 때 Re-ATI 스킵의 영향

데이터시트 §5.10: Re-ATI 조건 = `LTA > ATI Target + ATI Band` 또는 `LTA < ATI Target - ATI Band`

**ATI Disabled(현 펌웨어)에서**: Re-ATI 자동 발동 자체가 없다(A.12 bits[2:0]=000). LTA가 ATI Band 경계를 벗어나도 아무 일도 일어나지 않는다.

**이것이 터치 판정에 미치는 영향**:
- Re-ATI의 목적은 MULT/COMP를 재조정해 noTouch counts를 다시 ATI Target에 맞추는 것
- ATI Disabled에서 MULT/COMP는 고정이므로 noTouch counts의 절대값은 변하지 않음
- LTA는 IIR로 그 고정된 noTouch counts를 계속 추적 → LTA ≈ noTouch counts 유지
- 터치 시 Counts 감소 → Delta = LTA - Counts > 0 → threshold 비교 유효
- **Re-ATI 스킵이 threshold 유효성에 영향을 주지 않는다**

단, 장기간 환경 변화(온도·습도)로 noTouch counts가 크게 변하면:
- ATI Full이면 Re-ATI로 MULT/COMP를 재조정해 noTouch counts를 ATI Target으로 복귀
- ATI Disabled면 MULT/COMP 고정이므로 noTouch counts가 환경에 따라 drift
- LTA는 그 drift된 noTouch counts를 계속 추적 → LTA ≈ drift된 noTouch counts
- 터치 시 delta = LTA - 터치Counts → 환경 drift와 무관하게 delta 성립
- **결론**: 환경 drift에도 LTA IIR 추적이 delta를 보존하므로 threshold는 계속 유효하다 [추정: 매우 급격한 환경 변화 시 LTA IIR 추적 지연으로 일시적 false/miss 가능, DS 미규정]

### 2.4 전원 ON 시 RESEED와 LTA 초기화

현 펌웨어 초기화 시퀀스 (L1102~1158):

```
1. ACK Reset Event (L1104)
2. Sensor 설정 (L1116)
3. Touch/Prox Settings (L1122~1131)
4. Events Enable (L1134)
5. write_ati_compensation() — 고정 MULT/COMP 기록 (L1141)
6. RESEED bit SET (L1147, System Control 0x08) → LTA ← 현재 Counts
7. CH Timeout Disable (L1155) — auto-reATI 경로 이중 차단
```

RESEED(§5.5.1 원문): "최신 측정 counts를 취해 LTA를 그 값으로 seed." 즉 터치 중이든 노터치 중이든 RESEED 시점의 counts가 LTA 초기값이 된다.

- **노터치 상태에서 RESEED**: LTA ≈ noTouch Counts → 정상 기준선 설정 → 이후 터치 시 delta 성립
- **터치 상태에서 RESEED**: LTA ≈ 터치 Counts(낮은 값) → delta ≈ 0 → 터치 인식 안 됨 → 손 떼면 noTouch Counts로 Counts 상승 → LTA IIR 추적 시작 → [추정] LTA가 noTouch Counts를 추적하는 데 시간 소요(Beta에 따라 수십 샘플~수백 샘플)

터치 상태에서 RESEED 후 손을 떼면:
```
Counts 상승(손 뗌) → Counts > LTA(낮은 터치 기준값)
  → (LTA - Counts) < 0 < Threshold → 터치 미인식(정상, 손 뗀 상태)
  → LTA IIR: LTA_new = LTA_old + (Counts - LTA_old) × Beta/256
     → Counts > LTA이므로 LTA가 천천히 Counts 방향(noTouch)으로 상승
  → 수렴 후 LTA ≈ noTouch Counts → 다시 터치 시 delta 성립
```

이 수렴 과정 중에는 터치 감지가 정상 동작하지 않을 수 있다 [추정].

### 2.5 명제_J 최종 판정

| 질문 | 판정 | 근거 |
|---|---|---|
| ATI Disabled에서도 터치 판정식 유효한가 | **유효** | §5.7 판정식에 ATI 변수 없음. LTA IIR과 threshold만으로 성립 |
| LTA가 ATI Band를 벗어나도 threshold 유효한가 | **유효** | Re-ATI 스킵이 판정식 자체를 바꾸지 않음. LTA는 IIR로 noTouch 기준선 계속 추적 |
| ATI Full 없이 LTA IIR만으로 터치 성립하는가 | **성립** | 현 펌웨어(ATI Disabled)에서 실제로 이 구조로 동작 중. RESEED → LTA 초기화 → IIR 추적 → delta 판정 |
| 장기 환경 변화에도 유효한가 | **대체로 유효** (DS 미규정) | LTA IIR이 drift를 추적하므로 delta 보존. 급격 변화 시 추적 지연으로 일시 오동작 가능 [추정] |

---

## 3. 명제별 종합 판정

### 명제_I: auto-ATI와 Re-ATI는 큰 맥락에서 같은 말인가

> **판정: 조건부 참 (알고리즘 동일, 트리거 경로·시점 다름)**

- 같은 점: 동일 ATI 알고리즘(MULT/COMP 자동 산출, counts → ATI Target 수렴)을 사용
- 다른 점: Auto-ATI는 POR 시 IC 내부 강제 1회 실행, Re-ATI는 LTA drift 조건부 런타임 재실행
- 전원 ON + 터치 상태: Auto-ATI 수렴 실패(timeout) 가능 — 코드 L376 "(터치 중이면 정상)" 주석이 이를 인지하고 무시하도록 설계됨

### 명제_J(3-1): ATI Disabled에서 LTA IIR만으로 터치 성립하는가

> **판정: 참**

- 터치 판정식 `(LTA - Counts) > Touch Threshold`는 ATI 모드와 완전히 독립
- LTA IIR은 ATI 상태에 무관하게 매 측정마다 noTouch 기준선을 추적
- Re-ATI가 스킵되어도 threshold 비교 자체는 계속 유효
- 현 펌웨어가 ATI Disabled + RESEED + LTA IIR 구조로 실제 동작 중이며 이것이 의도된 설계

---

## 4. 핵심 발견 요약 (8줄 이내)

1. **auto-ATI = POR 시 강제 1회, Re-ATI = LTA drift 조건부**: 같은 ATI 알고리즘이나 발동 경로 다름 (§5.9·§5.10, 코드 L334~378).
2. **전원 ON + 터치 중 Auto-ATI**: counts가 ATI Target으로 수렴 불가 → timeout 예상 범위 내 (코드 L376 주석 "터치 중이면 정상"). RESEED로 LTA를 터치 counts에 맞추고 계속 진행.
3. **터치 판정식은 ATI 독립**: `(LTA - Counts) > Touch Threshold` — LTA와 threshold만 필요 (§5.7). ATI Mode=Disabled여도 판정식 변하지 않음.
4. **LTA IIR은 ATI Band 이탈과 무관하게 동작**: LTA는 매 측정마다 IIR로 noTouch counts를 추적. Re-ATI가 없어도 LTA가 기준선을 유지하면 delta는 유효 (§5.5·§5.6).
5. **Re-ATI 스킵의 실제 영향**: ATI Band 이탈 시 MULT/COMP 재조정이 안 돼 noTouch counts 절대값이 ATI Target에서 벗어날 수 있으나, LTA가 그 값을 추적하므로 delta 판정은 보존됨.
6. **현 펌웨어 구조**: ATI Disabled(0x08) + 고정 MULT/COMP + RESEED → LTA IIR 추적 → delta 판정. 이중 방어로 CH Timeout도 비활성(L1155). 설계적으로 ATI 없이 LTA만으로 운용하는 구조.
7. **명제_I 결론**: 조건부 참. 알고리즘 레벨 동의어이나 Auto(POR 강제) vs Re(drift 조건부) 구분 필요.
8. **명제_J 결론**: 참. ATI Full 없이 LTA IIR 자동 수렴만으로 터치 판정 성립. 단 터치 상태 RESEED 직후 LTA 수렴 지연 기간에는 감도 저하 가능성 [추정].
