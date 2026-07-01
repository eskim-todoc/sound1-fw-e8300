---
name: 04 분석 Beta 수렴 정량
purpose: LTA·Counts·Fast Filter Band Beta 설정의 수렴속도 vs 약터치 흡수 트레이드오프 정량 계산과 권고 Beta 범위 도출
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, beta, lta, fast-filter-band, iir, 수렴, 정량]
---

# 04 분석 — Beta 수렴 정량

**TL;DR**: IIR 1극 모델 `y_k=(1−1/2^β)·y(k−1)+x_k/2^β`로 노터치 LTA 수렴시간을 계산. AZD004 권고 LTA β=6~10은 100ms 주기에서 95% 수렴 38~307초로 **목표(1~5초)에 한참 못 미친다**. 1~5초 달성에는 alpha 0.06~0.25(데이터시트 Beta_reg 16~64, 또는 AZD004 b=2~4) 필요. 그러나 단순 β 상향은 약터치를 0.14~0.63초에 흡수해 미탐 유발 → 양립 불가. **해법은 Fast Filter Band 비대칭** — Normal LTA β 느리게(약터치 보존) + Fast LTA β 빠르게(노터치 복귀 방향만). 권고: Counts β alpha≈0.125·0.25, LTA Normal alpha≈0.004~0.008, LTA Fast alpha≈0.125~0.25, Fast Filter Band 8~15cnt. POR 전부 0x0000이라 명시 write 필수.

---

## 1. 목표·입력

| 항목 | 값 | 근거 |
|---|---|---|
| 측정 주기 T | 100ms(요구사항) / 200ms(현 코드 실측) 둘 다 계산 | 요구사항 §4, [`tdc_touch_config.h`] `POLL_INTERVAL` 주석 불일치(직전분석 §68 "100ms 주석 ↔ 실제 200ms") |
| 노터치 카운터 수렴 목표시간 | 1~5초 (stuck 해소 빠름) | 요구사항_7, json 04 핵심질문 |
| 현 Beta write | **0건 → POR 0x0000** (0xB0~0xB4 전부) | [06 §A.26~A.28], 직전분석 §57 "BETA write 0건" |
| 노이즈 밴드 / SNR | 4cnt / 터치 SNR≥5(raw 20cnt) | [적용가이드 §4.4 = AZD125 §10.1] |
| AZD004 Beta 권고 | Counts β=0~4, LTA β=6~10 | [적용가이드 §3.1 = AZD004 §5.2~§5.3] |

> [!NOTE]
> 측정 주기 T는 **IC report rate가 아니라 CM3 SW 폴링 주기**다. IC report rate(0xC1~0xC4)는 POR 0x0000이며 IC 내부 measurement cycle은 수 ms이지만, LTA 갱신은 IC가 자체 cycle마다 수행한다. 본 노드는 "노터치 카운터(LTA)가 stuck 후 수렴하는 체감 시간"을 SW가 관측하는 폴링 주기 기준으로 환산한다. 두 주기를 모두 제시해 어느 쪽이든 결론이 견고하도록 했다. [추정: LTA 갱신이 폴링 주기에 동기된다는 가정. IC 내부 cycle이 더 빠르면 실제 수렴은 더 빠를 수 있음 — 실측 게이트]

---

## 2. IIR 공식·등가 모델 (핵심)

### 2.1 두 출처의 공식과 등가

- **데이터시트 §5.6**: damping factor = Beta_reg / 256. → `LTA_new = LTA_old + (Counts − LTA_old)·(Beta_reg/256)`. [02 §5.6]
- **AZD004 §5.1**: `y_k = (1 − 1/2^β)·y(k−1) + x_k/2^β`. → alpha = 1/2^β. [적용가이드 §3.1]

두 모델은 **alpha(1회 측정의 현재값 반영 비율)** 로 통일된다:

$$\text{alpha} = \frac{\text{Beta\_reg}}{256} = \frac{1}{2^\beta} \quad\Rightarrow\quad \text{Beta\_reg} = \frac{256}{2^\beta} = 2^{(8-\beta)}$$

- pole `a = 1 − alpha`
- **step 응답**(노터치 복귀=LTA가 새 baseline으로 수렴): `잔차(미수렴분) = a^k = (1−alpha)^k`
- 연속시간 시정수: `τ = T / (−ln a)`. alpha 작을 때 `−ln a ≈ alpha` → `τ ≈ T/alpha = T·2^β`
- 정착 step 수: `a^k ≤ 잔차목표` → `k = ln(잔차목표) / ln(a)`

### 2.2 Beta 정의 불일치 — 실측 게이트

> [!WARNING]
> **레지스터에 기록할 실제 숫자는 두 해석이 갈린다 — 실측 게이트.**
>
> | 해석 | 레지스터 기록값 | alpha 산출 | 예: alpha=0.125 일 때 기록값 |
> |---|---|---|---|
> | **해석_A** (데이터시트 §5.6 문자 그대로) | Beta_reg = alpha×256 | alpha = reg/256 | reg = 32 |
> | **해석_B** (AZD004 β=시프트 지수) | β = log2(1/alpha) | alpha = 1/2^reg | reg = 3 |
>
> 데이터시트 §5.6 개념해설표는 "Beta 4비트(0~15)"로 서술하나, [06 §A.26~A.28] 레지스터 맵은 NP/LP 각 **8비트** 필드다. 8비트라면 해석_A(reg 0~255)가 자연스럽다. 단 AZD004가 "LTA β=6~10"을 권고하는데 해석_A로 환산하면 Beta_reg=4,2,1,0.5,0.25 → β=9,10은 정수 표현 불가. 이 모순은 **AZD004의 β가 시프트 지수(해석_B)이고 데이터시트 레지스터에 그 값을 직접 기록**(damping=1/2^reg)할 가능성을 시사한다. [추정]
>
> **결론**: alpha 기준 정착시간 표(§3)는 **어느 해석이든 동일**하므로 견고하다. 레지스터에 넣을 최종 정수만 실측으로 확정한다(작은 값 1개 write → LTA 수렴 step 실측 → 해석_A/B 판별).

---

## 3. 수렴시간 정량표 (계산)

공식: alpha=1/2^β, a=1−alpha, τ=−T/ln a, t95=ln(0.05)/ln(a)·T(95% 수렴), t99 동일 방식.

### 3.1 T = 100ms

| AZD004 β | alpha | Beta_reg(=2^(8−β)) | τ[s] | t63%(=τ) | **t95%** | t99% |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.500 | 128 | 0.14 | 0.14s | 0.43s | 0.66s |
| 2 | 0.250 | 64 | 0.35 | 0.35s | **1.04s** | 1.60s |
| 3 | 0.125 | 32 | 0.75 | 0.75s | **2.24s** | 3.45s |
| 4 | 0.0625 | 16 | 1.55 | 1.55s | **4.64s** | 7.14s |
| 5 | 0.0313 | 8 | 3.15 | 3.15s | 9.44s | 14.5s |
| 6 | 0.0156 | 4 | 6.35 | 6.35s | 19.0s | 29.2s |
| 7 | 0.00781 | 2 | 12.75 | 12.8s | 38.2s | 58.7s |
| 8 | 0.00391 | 1 | 25.55 | 25.6s | 76.5s | 117.7s |
| 9 | 0.00195 | 0.5 | 51.15 | 51.2s | 153.2s | 235.6s |
| 10 | 0.00098 | 0.25 | 102.35 | 102.4s | 306.6s | 471.3s |

### 3.2 T = 200ms (현 코드)

모든 시간이 T=100ms 대비 정확히 2배. 예: β=3 → t95% 4.49s, β=4 → t95% 9.28s.

> [!IMPORTANT]
> **AZD004 권고 LTA β=6~10(=Beta_reg 0.25~4)은 95% 수렴 19~307초(100ms) / 38~613초(200ms)**. 노터치 stuck 해소 목표 1~5초에 **한참 못 미친다**. AZD004 권고는 "느린 환경 드리프트(온도·습도, 분 단위) 추종" 용도라 stuck 해소와 목표가 다르다. 1~5초 수렴에는 **alpha 0.06~0.25(β=2~4, Beta_reg 16~64)** 가 필요하다.

---

## 4. 단순 β 상향의 약터치 흡수 위험 (정량)

LTA β를 일괄 상향(빠른 양방향 추종)하면, **터치 중 LTA halt가 발동하지 않는 약터치/유지터치**(halt 임계 미달)에서 LTA가 counts를 흡수해 delta가 사라진다 → 미탐.

- 모델: `delta(t) = delta0 · a^k`, 흡수시점 `delta0·a^k < threshold` → `t_absorb = ln(thr/delta0)/ln(a)·T`
- 가정: 약터치 delta0 = threshold×1.5, threshold 계수=80(현 코드 `tdc_touch_config.h` §9 주석), halt 미발동, T=100ms

| LTA β | Beta_reg | alpha | t_absorb | 판정 |
|---:|---:|---:|---:|---|
| 2 | 64 | 0.250 | **0.14s** | 위험 — 약터치 즉시 미탐 |
| 3 | 32 | 0.125 | **0.30s** | 위험 |
| 4 | 16 | 0.0625 | **0.63s** | 위험 |
| 5 | 8 | 0.0313 | 1.28s | 위험 |
| 6 | 4 | 0.0156 | 2.57s | 경계 |
| 7 | 2 | 0.00781 | 5.17s | 비교적 안전 |
| 8 | 1 | 0.00391 | 10.36s | 안전 |

> [!CAUTION]
> **1~5초 수렴(β=2~4)과 약터치 보존은 단일 LTA β로 양립 불가.** β=2~4면 약터치가 0.14~0.63초에 흡수되고, 약터치 보존(β=7~8)이면 stuck 수렴이 38~76초로 느려진다. 검토_5(브레인스토밍 §2번·5번 "Beta 양날")가 정확히 이 지점이다.

---

## 5. 해법 — Fast Filter Band 비대칭 (정답)

### 5.1 방향성 원리 [02 §5.6]

데이터시트 §5.6 원문: "counts가 LTA로부터 **sensing 반대 방향**으로 Fast Filter Band 이상 drift하면 fast filtering 적용. 차이가 band 미만이면 normal filter 복귀."

self-cap에서 **sensing 방향 = counts 감소(터치)**, 반대 방향 = counts 증가(노터치 복귀·물체 제거). 따라서:

```
counts > LTA + FastFilterBand  →  fast beta (빠른 LTA 추종)
  = 노터치 복귀/물체 제거로 counts가 LTA 위로 솟을 때만 빠르게 수렴
counts < LTA (터치 방향, delta>0)  →  항상 normal beta (느린 LTA)
  = 약터치 delta 보존 + halt 효과 보강
```

**핵심**: 단일 β 상향은 양방향을 빠르게 만들어 약터치도 흡수한다. Fast Filter Band 비대칭은 **노터치 복귀 방향만 빠르게** 하여, 터치 방향(약터치 흡수 위험 구간)은 느린 normal로 보존한다. 비대칭이 검토_5의 정답.

### 5.2 권고 Beta 조합 (T=100ms 기준)

| 레지스터 | 역할 | 권고 alpha | 해석_A Beta_reg | 해석_B β | 수렴/특성 |
|---|---|---:|---:|---:|---|
| **Counts Filter (0xB0)** | 원시 counts 평활(노이즈 억제) | 0.125~0.25 | 32~64 | 2~3 | t95% 1.0~2.2s, AZD004 "0~4" 권고 부합 |
| **LTA Normal (0xB1)** | 느린 환경 추종 + 약터치 보존 | 0.004~0.008 | 1~2 | 7~8 | t_absorb 5~10s(약터치 안전), AZD004 "6~10" 부합 |
| **LTA Fast (0xB2)** | 노터치 복귀 방향 빠른 수렴 | 0.125~0.25 | 16~64 | 2~4 | t95% 1.0~4.6s = **목표 1~5초 달성** |
| **Fast Filter Band (0xB4)** | fast 발동 임계(절대 cnt) | — | **8~15 cnt** | — | 노이즈 4cnt 위 + 약터치 20cnt 아래 |

> [!NOTE]
> NP/LP 분리(각 8비트 필드, bits7-0=NP, bits15-8=LP). Full ATI·LP 유지(06 노드 전력 결론) 환경에서는 **LP beta가 실제 동작 모드**일 수 있으므로 NP·LP를 동일 권고값으로 함께 write한다. [06 §A.26~A.28]

### 5.3 Fast Filter Band 임계값 — POR=0 함정

> [!WARNING]
> **0xB4 POR = 0x0000.** band=0이면 `|counts−LTA|>0`이 거의 항상 참 → fast가 상시 적용 → Normal beta가 무력화되어 **비대칭 자체가 붕괴**(LTA가 항상 빠르게 추종 = 약터치 흡수). 반드시 명시 write 필요. 권고 8~15cnt = 노이즈 밴드(4cnt)의 2~3.5배, 약터치 raw delta(SNR≥5 → ~20cnt) 미만. [02 §5.6, 적용가이드 §4.4]

---

## 6. 설계 함의

- **명시 write 필수 레지스터 4종**: 0xB0(Counts), 0xB1(LTA Normal), 0xB2(LTA Fast), 0xB4(Fast Filter Band). 전부 POR 0x0000이라 미설정 시 필터 무력/비대칭 붕괴. ATI 완료 후·본격 폴링 전 초기화 시퀀스 말미에 기록(05 임계·debounce write와 동일 위치).
- **레지스터 정수값은 실측 게이트** — 해석_A/B 판별을 위해 alpha=0.125 1개를 reg=32(해석_A) 또는 reg=3(해석_B)으로 시험 write → LTA 수렴 step 실측 → 어느 해석이 맞는지 1회 확정 후 전체 적용.
- **터치 중 LTA halt와 한 쌍**: Fast Filter Band 비대칭은 약터치 보존을 강화하나 완전 보장은 아니다. 명확한 터치는 halt(터치 이벤트 중 LTA 동결, [02 §5.5])가 1차 방어, 약신호는 비대칭이 2차 방어. 02 노드(터치 중 Re-ATI 경계)·halt 임계와 연동 필요.
- **Counts β와 LTA β 분리 효과**: Counts β(0xB0)는 raw 평활(노이즈), LTA β(0xB1/0xB2)는 baseline 추종 — 역할이 다르므로 독립 튜닝. Counts β 과대(alpha 큼)면 노이즈가 delta에 그대로 실려 false touch, 과소면 응답 지연.

## 7. 미해결·실측 게이트

| 항목 | 내용 | 게이트 |
|---|---|---|
| Beta 레지스터 정수 해석 | 해석_A(reg=alpha×256) vs 해석_B(reg=시프트지수) | **실측** — 시험 write 1회로 판별 |
| 측정 주기 T 확정 | 100ms(목표) vs 200ms(현 코드). LTA 갱신이 IC 내부 cycle인지 폴링 동기인지 | **실측** — IC report rate write 여부·실제 수렴 step |
| Fast Filter Band 발동 방향 | self-cap에서 IC가 counts 증가 방향만 fast 거는지 실동작 | **실측** — 노터치 복귀 vs 약터치 수렴 비대칭 확인 |
| halt 임계 | 약터치가 halt 발동 임계 위/아래 어디인지 (비대칭 2차 방어 필요 구간) | **실측** — 02 노드 연동 |
| 약터치 delta0 가정 | threshold×1.5 가정의 실제 분포 | **실측** — 실사용 약터치 delta 측정 |
