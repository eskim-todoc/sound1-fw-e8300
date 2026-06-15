---
name: 은수님의견-A3-beta필터동역학검증
purpose: 은수님 제안 중 "빠른 BETA로 LTA 수렴 가속(M5)" 검증 — Q4(약한/느린 터치 흡수·Fast Filter Band·과도구간 오인식) BETA·필터 동역학 관점 분석
type: 개선
maturity: stable
tags: [touch, iqs323, beta, filter, fast-filter-band, lta, review, persona-a3]
---

# A3 · BETA·필터 동역학 검증 (Q4)

> **TL;DR**: 페르소나 A3(BETA·필터 동역학)가 은수님 명제 **M5(빠른 BETA로 LTA 수렴 가속)** 와 검증질문 **Q4**(약한/느린 터치 흡수·Fast Filter Band 상호작용·과도구간 오인식)를 데이터시트·코드로 검증한다. 결론: **빠른 BETA는 양날의 검**이다. (1) Beta는 4비트(0~15)라 최대 alpha≈5.9%로 상한이 막혀 "최대한 빠르게"라도 IC-내부 report rate(~20ms) 기준 수십 샘플이 필요 — 200ms 폴링과는 무관하게 IC 내부에서 빠름. (2) **결정적 위험**: 빠른 LTA BETA는 **약하거나 느린(천천히 누르는) 터치를 LTA가 추격 흡수**해 delta가 threshold에 도달하기 전에 사라지게 만들어 **터치 미인식**을 유발할 수 있다 — self-cap 감소 방향이라 §5.5 frozen이 걸리기 전 구간이 특히 취약. (3) **Fast Filter Band(0xB4)는 현재 0**이라 fast filter가 항상/전혀 동작하지 않는 모호 상태(확인 필요)이며, 켜면 손 뗀 직후 과도구간을 가속 수렴시켜 은수님 4단계엔 도움이나 동시에 약터치 흡수 위험을 키운다. (4) **현재 모든 BETA 레지스터(0xB0~0xB4)는 코드가 한 번도 write하지 않아 POR default 0x0000** — 즉 NP/LP BETA 모두 0이다(코드 확인). 은수님 안은 이 레지스터를 새로 세팅하는 것을 전제로 하나, 그 값 선정은 약터치 미인식 vs 수렴속도의 trade-off 실측 튜닝이 필수다.

> [!IMPORTANT]
> 근거: 데이터시트 [02 §5.5·5.6·5.7](../../데이터시트/02_proxfusion동작.md), [06 A.26~A.29(0xB0~0xB3)·Fast Filter Band(0xB4)](../../데이터시트/06_레지스터레퍼런스.md), [01 전력모드·report rate](../../데이터시트/01_개요·전기·타이밍.md). 코드: `tdc_drv_iqs323.c/.h`·`tdc_touch.c`. 추정은 "(확인 필요)" 표기.

---

## 0. 임무 범위와 명제 매핑

| 항목 | 본 분석 담당 | 비고 |
|---|---|---|
| **M5** | 주 검증 | "빠른 BETA로 LTA 수렴 가속" — 성립/부작용 |
| **Q4** | 핵심 질문 | 약/느린 터치 흡수·Fast Filter Band·과도구간 오인식 |
| M2·M3·M6 | 인접(간접) | LTA seed·자기정상화는 A2/A5 주관, BETA가 그 속도를 좌우하므로 동역학 측면만 보조 |

> 은수님 6단계 원문: *"추가로 LTA가 카운트를 최대한 빠르게 추적하도록 BETA 조정."* 본 분석은 이 "빠르게"가 **무엇을 가속하고 무엇을 망가뜨리는가**를 정량·정성 분해한다.

---

## 1. 데이터시트 BETA 메커니즘 (확정 사실)

### 1.1 IIR 갱신식과 Beta 상한

LTA 갱신은 IIR(EMA) 필터다(02 §5.5 개념해설, §5.6):

```
LTA_new = LTA_old + (Counts − LTA_old) × (Beta / 256)
```

- `alpha = Beta / 256`. **Beta는 4비트(0~15)** → alpha **최대 ≈ 5.9%**(Beta=15), 최소 ≈ 0.4%(Beta=1) [02 §5.6 표, 06 A.26~A.29].
- 즉 은수님이 말한 "최대한 빠르게"의 물리적 상한은 **alpha 5.9%/샘플**이다. 한 번에 절대 따라잡지 못하고 항상 강한 스무딩이 걸린다(02 §5.6 "항상 강한 스무딩").

> [!IMPORTANT]
> **수렴 시간 정량(데이터시트 식 기반 계산, 확인 필요)**: alpha=5.9%(Beta=15)에서 LTA가 목표의 90%까지 수렴하는 데 필요한 샘플 수 ≈ ln(0.1)/ln(1−0.059) ≈ **약 38 샘플**. 63%(시정수 1τ)는 약 16 샘플. Beta=4(alpha 1.6%)면 90%에 약 143 샘플로 4배 가까이 느려진다. → "BETA를 키우면 빠르다"는 방향은 맞으나 **단일 샘플 즉시 수렴은 불가**, 여전히 수십 샘플이 필요하다.

### 1.2 샘플 = IC 내부 conversion, 200ms 폴링 아님 (중요 구분)

> [!WARNING]
> **흔한 오해 차단**: 위 "샘플"은 펌웨어의 **200ms I²C 폴링**이 아니라 **IC 내부 측정(conversion) 주기**다. NP는 "매 Report Rate마다 모든 채널 전체 conversion"(01 §전력모드)이며 self-cap NP report rate는 일반적으로 수~수십 ms 범위(01 §3.4 표에 모드별 report rate 존재, 정확값 확인 필요). 따라서 LTA 수렴은 IC 내부에서 진행되고 펌웨어 200ms 폴링은 그 결과를 **관측만** 한다. → 은수님 안의 BETA 수렴은 **펌웨어 폴링 주기와 독립**으로 빠르게 일어날 수 있다(M5에 우호적). 단 실제 NP report rate 실측이 수렴시간 환산의 전제(확인 필요).

### 1.3 Fast Filter Band (02 §5.6, 06 0xB4)

- **동작 조건**: counts가 LTA로부터 **sensing 반대 방향으로 Fast Filter Band 이상** drift하면 fast beta(0xB2, A.28)로 전환, 차이가 band 미만이 되면 normal beta(0xB1)로 복귀.
- self-cap에서 "sensing 방향"은 counts **감소**(터치)다. 따라서 **"sensing 반대 방향"=counts 증가** = **손을 뗀 직후**(터치카운트 300→노터치 500) 구간이다.
- 즉 **Fast Filter Band는 정확히 은수님 4단계(손 뗀 뒤 LTA가 500으로 빠르게 수렴)를 가속하는 메커니즘**이다 — counts가 위로 튀면 fast beta로 LTA를 급속 추격.

---

## 2. 코드 현황 — BETA는 전혀 설정되지 않음 (확정 사실)

`grep`으로 `tdc_drv_iqs323.c/.h`·`tdc_touch.c` 전체에서 **0xB0~0xB4 / Beta / Fast Filter 레지스터 write가 0건**임을 확인했다(0x13 Filtered Counts read만 존재).

| 레지스터 | 주소 | 코드 write | 결과 상태 |
|---|---|---|---|
| Counts Filter Betas (A.26) | 0xB0 | 없음 | **0x0000** (NP·LP counts beta = 0) |
| LTA Filter Betas (A.27) | 0xB1 | 없음 | **0x0000** (NP·LP LTA beta = 0) |
| LTA Fast Filter Betas (A.28) | 0xB2 | 없음 | **0x0000** (fast beta = 0) |
| Activation/Movement LTA (A.29) | 0xB3 | 없음 | **0x0000** (Release UI 미사용이라 무관) |
| Fast Filter Band | 0xB4 | 없음 | **0x0000** |

> [!WARNING]
> **공통 입력 §4 "BETA 미설정(reset 0 — 확인 필요)" 을 코드로 확정**: 미설정이 맞다. 모든 BETA·Fast Filter Band가 POR default 0x0000이다.
>
> **Beta=0의 의미(확인 필요·해석 위험)**: 데이터시트는 Beta=0의 동작을 명시하지 않는다. alpha=0/256=0이면 IIR식상 `LTA_new = LTA_old`로 **LTA가 영원히 갱신 안 됨**(완전 동결)으로 해석된다. 그러나 현 펌웨어는 LTA가 동작 중인 것으로 보이며, IC가 Beta=0을 "기본 내부 동작" 또는 별도 default로 해석할 가능성이 있다(데이터시트 미명시 → **실칩 RTT로 LTA 추적 여부 실측 필수**). 만약 정말 0이라 LTA가 동결이라면, 현 시스템이 드리프트 방어를 **200ms CRX0 방전 + 부팅 Reseed**에 전적으로 의존한다는 B1 결론과 정합한다.

→ **은수님 안의 핵심 전제(M5 "BETA 조정")는 현 코드에 존재하지 않는 신규 설정**이다. 즉 은수님 안은 0xB0~0xB4를 새로 write하는 SW 변경을 반드시 수반한다.

---

## 3. Q4 핵심 검증 — 빠른 BETA의 3대 부작용

### Q4-A. 약한/느린 터치를 LTA가 흡수해 미인식? **(가장 큰 위험 — 성립)**

self-cap 터치 판정: `(LTA − Counts) > Touch Threshold`, 코드상 threshold **100**(drv.h 132). 터치 진입 전에는 §5.5 frozen이 **아직 안 걸린 상태**(delta가 threshold 미만이면 touch bit=0 → LTA 갱신 계속).

- **느린 터치(천천히 누름) 시나리오**: 손가락이 천천히 접근하면 counts가 서서히 감소한다. LTA BETA가 크면 LTA도 그 감소를 **함께 추격**한다. delta = (LTA−Counts)가 threshold 100에 **도달하기 전에 LTA가 따라 내려가** delta가 영영 100을 못 넘김 → **터치 미인식**.
- **약한 터치(작은 정전용량 변화) 시나리오**: 접촉이 약해 counts 감소폭 자체가 100 부근이면, 빠른 LTA가 일부를 흡수해 유효 delta가 100 미만으로 깎여 미인식.

> [!CAUTION]
> **이것이 빠른 BETA의 최대 독성**이다. LTA는 "환경 드리프트(느림)는 추적하되 터치(빠름)는 무시"하도록 **느려야** 하는 필터인데(02 §5.5 "갑작스러운 변화는 반영 안 함"), 은수님이 수렴 가속을 위해 BETA를 키우면 **"느린/약한 터치"와 "환경 드리프트"의 구분이 무너진다**. 빠른 BETA가 빠른 터치엔 안전(frozen 전에 한 번에 threshold 돌파)하지만, **느린/약한 터치엔 치명적**이다. 인공와우 사용자 특성상 약하거나 느린 누름이 흔할 수 있어(확인 필요) 실사용 리스크가 크다.

### Q4-B. Fast Filter Band 상호작용

- **순기능(은수님 4단계 우호)**: 손 뗀 직후 counts 급상승(300→500) 구간에서 fast beta가 걸려 LTA를 빠르게 500으로 끌어올림 → 은수님이 원하는 "노터치 수렴 가속"을 **정확히 그 구간에만 선택 적용**. 일반 LTA beta는 작게 유지해 약터치 흡수를 막으면서, Fast Filter Band로 "손 뗀 직후"만 가속하는 **분리 튜닝이 가능**하다. → Q4-A 위험을 완화하는 설계 지렛대.
- **현 상태 문제**: Fast Filter Band=0 → "counts가 LTA로부터 0 이상 반대방향 drift" 조건이 **항상 참**으로 해석되면 fast beta(=0)가 늘 적용, 또는 band=0이 fast 비활성으로 해석될 수도(데이터시트 미명시, **확인 필요**). 어느 쪽이든 현재는 의도된 동작이 아니다.
- **위험**: Fast Filter Band를 너무 작게 잡으면 약터치의 미세 counts 변동(노이즈 포함)에도 fast beta가 발동해 LTA가 약터치를 더 빨리 흡수 → Q4-A 악화. band는 **노이즈보다 크고 약터치 delta보다 작게** 잡아야 하는 좁은 창(확인 필요·실측 튜닝).

### Q4-C. 손 뗀 직후 과도구간(transient) 오인식

은수님 시퀀스 4→5단계 사이(LTA가 300→500으로 수렴 중인 과도구간)에서:

- **과도구간 중 재터치**: LTA가 아직 400(수렴 중)일 때 사용자가 다시 터치해 counts가 320이 되면 delta=80 < 100 → **재터치 미인식**(LTA가 아직 노터치 500에 도달 못 함). 빠른 BETA는 이 과도구간을 **짧게** 만들어 이 위험을 줄인다 → Q4-C에 한해 빠른 BETA가 **유리**.
- **반대로 과도구간 중 LTA overshoot 없음**: IIR은 단조 수렴이라 overshoot로 인한 가짜 음수 delta(오발) 위험은 낮다. 단 Fast Filter Band 전환 경계에서 normal↔fast beta 스위칭 시 LTA 기울기 급변 → delta 미세 jitter 가능(확인 필요, 영향 경미 추정).

> [!NOTE]
> **종합 trade-off**: 빠른 BETA는 **과도구간 단축(Q4-C 유리)**과 **약/느린 터치 흡수(Q4-A 불리)**가 **정면 상충**한다. 동일한 alpha 하나로 둘을 동시에 만족할 수 없다. 해법은 **일반 LTA beta는 작게 + Fast Filter Band로 "손 뗀 직후 상승" 구간만 fast beta로 가속**하는 비대칭 설계뿐이다(self-cap 방향성 덕에 가능).

---

## 4. M5 판정과 은수님 안에의 함의

| 질문 | 판정 | 근거 |
|---|---|---|
| M5 "빠른 BETA로 수렴 가속" 방향이 맞나 | **부분 성립** | alpha∝Beta로 수렴은 빨라지나 4비트 상한(5.9%)·수십 샘플 필요·IC report rate 의존 (§1) |
| 단일 BETA로 안전하게 가속 가능한가 | **불가** | 약/느린 터치 흡수(Q4-A)와 정면 상충 (§3-A) |
| Fast Filter Band로 분리 가속 가능한가 | **가능(조건부)** | self-cap 방향성상 "손 뗀 뒤 상승"만 fast 적용 가능, band 튜닝 실측 필요 (§3-B) |
| 현재 코드가 은수님 안을 지원하나 | **미지원** | 0xB0~0xB4 전부 0x0000, write 0건 — 신규 SW 필요 (§2) |

### 설계 권고 (BETA 동역학 관점)

1. **비대칭 BETA 설계**: `LTA Filter Beta`(0xB1)는 **작게**(예 Beta 2~4, 약터치 흡수 방지) + `LTA Fast Filter Beta`(0xB2)는 **크게**(예 Beta 8~15, 손 뗀 뒤 가속) + `Fast Filter Band`(0xB4)를 **노이즈<band<약터치delta** 창으로 튜닝. 이것이 은수님 6단계를 안전하게 구현하는 유일 경로.
2. **Beta=0 실측 선결**: 현 LTA 갱신 여부를 RTT로 확인(§2 WARNING). 0이 동결이면 은수님 안 도입 시 동작이 크게 바뀌므로 baseline 재측정 필요.
3. **NP report rate 실측**: 수렴 시간(샘플→ms 환산)의 전제(§1.2). 200ms 폴링과 분리해 IC 내부 주기 확인.
4. **약/느린 터치 회귀 테스트 필수**: BETA 상향 시 "천천히 누름·약하게 누름"을 회귀 시나리오로 반드시 측정 — 미인식이 Q4의 1순위 실패 모드.

---

## 5. 제약 정합성 점검

| 제약 | 정합 |
|---|---|
| order 001 = Release UI | A.29(Activation/Movement Beta, 0xB3)는 Release UI/Movement UI 전용 — 현 미사용. 단 Release UI 채택 시(B2) Activation LTA Beta도 동역학에 진입, 동일 약터치 흡수 논리 적용 필요(확인 필요) |
| self-cap 터치 시 counts 감소 | §3 전 분석이 감소 방향 전제 — Fast Filter Band "sensing 반대=증가=손뗌" 해석의 토대 |
| 웜부트 MCLR POR (B4) | 매 부팅 BETA도 POR default 0으로 초기화 → apply_settings에서 매번 재write 필요(은수님 안 도입 시 시퀀스에 0xB0~0xB4 추가 위치 지정해야) |

---

## 6. 발견 요약

- **F1**: 모든 BETA·Fast Filter Band 레지스터(0xB0~0xB4)가 코드에서 write 0건 → POR default 0x0000 (코드 확정). 은수님 "BETA 조정"은 신규 SW 전제.
- **F2**: Beta 4비트 상한(alpha≤5.9%)으로 수렴은 수십 샘플 필요 — "최대한 빠르게"도 즉시 아님. 단 IC 내부 report rate 기준이라 200ms 폴링과 무관하게 빠를 수 있음.
- **F3 (핵심)**: 빠른 LTA BETA의 최대 독성 = **약/느린 터치를 LTA가 흡수해 미인식**(frozen 걸리기 전 구간). 빠른 터치엔 안전, 느린/약한 터치엔 치명적.
- **F4**: Fast Filter Band는 self-cap 방향성상 "손 뗀 직후 상승"만 가속 — 은수님 4단계를 **선택적으로** 도와 F3 위험을 회피할 비대칭 설계 지렛대. 단 band=0 현 상태는 미정의·실측 필요.
- **F5**: 과도구간(Q4-C)에선 빠른 BETA가 오히려 유리(과도 단축). Q4-A와 정면 상충 → 단일 beta로 양립 불가, 비대칭 설계 필수.
