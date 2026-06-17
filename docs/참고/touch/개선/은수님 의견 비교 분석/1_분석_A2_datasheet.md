---
name: 은수님의견-A2-데이터시트메커니즘검증
purpose: 은수님 제안(터치중 auto-ATI + LTA 자기정상화 + 빠른 BETA)을 데이터시트 원문(02 §5.5·5.9·5.10·5.11, 06 A.6·A.7·A.12) 메커니즘으로 Q1·Q2·Q3·Q5 검증
type: 개선
maturity: experimental
tags: [touch, iqs323, auto-ati, lta, reseed, re-ati, review, datasheet-mechanism]
---

# A2 · 데이터시트 메커니즘 검증 (Q1·Q2·Q3·Q5)

> **TL;DR**: 데이터시트 원문으로 은수님 4·5단계의 핵심 전제를 검증한 결과 **M1 조건부지지, M2 부분반박(seed≠LTA), M3 지지, M4·M6 결정적 반박, M5 별도(A3), Q5 양날**. 핵심 결론: **터치 중 auto-ATI(M1)는 max counts·ATI Error 위험 하에 "수렴은 가능"하나, 은수님 안의 성패를 가르는 M6(자기정상화)는 데이터시트상 성립하지 않는다.** ATI는 LTA를 seed하지 않고(§5.9는 MULT/COMP만 조정), LTA seed는 **별도 Reseed**(§5.5.1)가 담당한다. 터치 중 frozen(§5.5·M4)은 "이미 터치로 인식된" 상태 전제인데, 부팅 직후 baseline=터치라 delta≈0 → 터치 미인식 → **frozen 안 됨 → LTA가 터치 카운트를 추적** → 은수님 7단계 역전. Re-ATI(§5.10)는 ATI Mode=Full일 때만 자동, 현 코드는 Disabled라 미발동(Q5).

> [!IMPORTANT]
> 근거: [02 ProxFusion 동작](../../데이터시트/02_proxfusion동작.md) §5.5·5.6·5.9·5.10·5.11, [06 레지스터](../../데이터시트/06_레지스터레퍼런스.md) A.6·A.7·A.12·A.30, [펌웨어 분석](../펌웨어%20구현·시퀀스%20분석.md), [4_타당성검증 B1·B4](../생각정리/4_타당성검증.md), 코드 `tdc_drv_iqs323.c`·`tdc_touch.c`. **추정은 "확인 필요" 표기.**

---

## 1. 임무·범위

데이터시트 메커니즘 관점에서 담당 질문 **Q1·Q2·Q3·Q5**를 원문 인용으로 검증하고, 관련 명제 **M1·M2·M3·M4·M6**의 지지/반박을 판정한다. (M5 빠른 BETA 동역학은 A3, Q6·Q7은 A6 담당이므로 본 문서는 메커니즘 경계만 명시.)

---

## 2. Q1 — 터치 중 auto-ATI가 터치 카운트로 MULT/COMP를 "적절히" 세팅 (M1)

### 판정: 조건부지지 (수렴은 가능, 그러나 max counts·ATI Error 위험)

**메커니즘 (지지 측):** §5.9는 ATI가 "각 채널의 divider·multiplier·compensation을 선택"하며 "ATI 트리거 시 먼저 divider·multiplier로 counts를 `ATI Base`에 최대한 근접시키고, 그 다음 `Compensation Value`·`Compensation Divider`로 ATI Target에 근접"시킨다고 명시한다. ATI 알고리즘은 **입력 counts의 절대값과 무관하게** 그 시점 counts를 ATI Target으로 정규화하는 것이 목적이므로(§5.9 개념해설: "각 채널의 noTouch counts를 특정 목표값으로 정규화"), 입력이 터치 카운트(낮은 값)여도 알고리즘 자체는 그 값을 Target으로 끌어올리도록 MULT/COMP를 잡을 수 있다. → **"세팅된다"는 사실.**

**반박/위험 측 (왜 "적절히"가 아닐 수 있나):**

1. **터치 카운트 = 낮은 값을 Target으로 증폭** → 손 떼면 counts가 상승하는데, self-cap은 노터치가 더 높은 counts(02 §5.7: "터치 시 counts 감소")이므로 노터치 시 counts가 Target을 크게 초과 → **Max Counts 한계 충돌 위험**. §5.4.2: "ATI 설정·하드웨어가 한계 초과 count를 유발하면 conversion 정지, max value가 read됨." Prox Control(0x32) bits[7:6] Max Counts는 1023/2047/4095/16383 중 선택(06 A.7). 터치 카운트를 Target=400으로 정규화하면 노터치는 그 배율만큼 더 큰 counts가 되어 한계 초과 → conversion 정지 가능. → **확인 필요(실측): 터치 시 raw counts와 노터치 raw counts의 비.**

2. **ATI Error 위험** — §5.11: "ATI 알고리즘 완료 시 Counts가 Re-ATI Boundary 밖이면 ATI Error bit set." 터치 중 auto-ATI가 수렴해도, 정규화 기준이 터치 카운트라 노터치 전이 시 LTA가 Boundary(`ATI Target ± ATI Band`, §5.10)를 벗어날 소지가 크다.

3. **현 코드는 CH1 더미에서 이미 이 문제를 회피 중** — 펌웨어 분석 §6: "CH1 ATI Mode=Disabled: CalCap 부하의 auto-ATI 수렴 실패 → 전역 ATI_ERROR(0x10 bit6) SET → 터치 판정 차단을 막기 위함." 즉 auto-ATI 수렴 실패가 전역 ATI_ERROR로 번지는 실증 사례가 코드에 존재한다.

**결론:** M1은 "auto-ATI가 터치 카운트로도 MULT/COMP를 세팅한다"는 부분은 **데이터시트상 성립**하나, "적절히"(노터치 동작 보장)는 max counts·ATI Error 게이트를 통과해야만 성립하는 **조건부**다.

---

## 3. Q2 — auto-ATI 완료 직후 LTA seed 값 / 손 떼면 노터치 추적 (M2·M3)

### 판정: M2 부분반박 (ATI는 LTA를 seed하지 않음), M3 지지 (메커니즘은 맞음)

**결정적 사실 — ATI ≠ LTA seed:** §5.9 ATI 절 전체는 **divider·multiplier·compensation**만 다룬다. LTA를 어떤 값으로 설정한다는 서술이 §5.9에 **없다**. LTA seed는 별도 메커니즘이다:

- §5.5.1 Reseed: "Reseed는 최신 측정 counts를 취해 LTA를 그 값으로 seed → 외부 환경 최신 조건에 LTA 일치. `System Control`의 `Reseed` bit set으로 명령."
- §5.8 Channel Timeout reseed, §5.5 일반 IIR 추적(`LTA_new = LTA_old + (Counts − LTA_old) × Beta/256`).

→ **은수님 2~3단계의 "auto-ATI 직후 LTA가 (터치) 카운트로 시작"은 ATI가 직접 LTA를 잡는다는 의미라면 부정확하다.** 실제로 LTA가 터치 카운트로 출발하는 것은 (a) POR 후 LTA 초기화 + IIR 수렴, 또는 (b) 명시적 Reseed가 그 시점 counts(=터치 카운트)를 잡을 때다. 즉 **오염 실채널은 auto-ATI가 아니라 Reseed/IIR seed**라는 [4_타당성검증 B1]의 결론과 정합한다.

> [!NOTE]
> 현 코드(`apply_settings`)는 auto-ATI를 폐기(FIXED MULT/COMP)하고 8단계에서 명시 **Reseed(0xC0 LSB 0x08)** 로 LTA를 그 시점 counts에 잡는다(`tdc_drv_iqs323.c:1133`). 은수님 안처럼 auto-ATI를 다시 켜도, ATI 자체는 LTA를 안 잡으므로 **ATI 후 별도 Reseed 또는 IIR 수렴이 LTA를 결정**한다. M2의 인과 사슬에서 "ATI→LTA"는 끊어져 있고 그 자리에 Reseed/IIR이 들어간다.

**M3 메커니즘(지지):** "LTA가 노터치 카운트를 추적 → 노터치(500) 수렴 → 이후 터치 시 counts 감소 → 인식"은 §5.5 IIR 추적 + §5.7 판정공식 `(LTA − Counts) > Touch Threshold`로 **메커니즘 자체는 정확**하다. self-cap 부호(터치 시 counts 감소)도 맞다. **단 전제조건은 "그 시점 LTA가 노터치 추적 모드(=frozen 아님)여야 한다"** — 이것이 Q3/M4/M6에서 무너진다.

---

## 4. Q3 (핵심) — 터치 중 frozen 전제 / 부팅 터치 시 delta≈0 → 미인식 → frozen 안 됨? (M4·M6)

### 판정: M4 반박 (frozen은 인식 후에만), M6 결정적 반박 (자기정상화 비성립)

이것이 은수님 안의 성패를 가르는 지점이다. 데이터시트 원문으로 단계별 분해한다.

**① frozen의 전제 (§5.5):** "LTA는 ... touch·proximity 이벤트 중에는 frozen." §5.5 개념해설: "터치 상태에서 LTA가 계속 갱신되면 터치된 counts(낮은 값)를 기준선으로 학습해버린다. ... 이를 막기 위해 터치·근접 이벤트 중에는 LTA 갱신을 멈춘다." → **frozen은 "CHx Touch/Prox bit가 set된 상태"(즉 이미 터치로 인식됨)의 결과**다(06 A.2 System Status bit9 CH0 Touch, §5.7 "상태 진입 시 bit set").

**② 부팅 직후 터치 상태의 delta (§5.7):** 터치 진입 조건은 `(LTA − Counts) > Touch Threshold`. 부팅 직후 baseline(LTA)이 터치 카운트로 seed되면 LTA ≈ Counts → **delta = LTA − Counts ≈ 0 < Threshold → 터치 미인식 → CH0 Touch bit = 0.**

**③ frozen 트리거 안 됨:** CH0 Touch bit = 0이므로 §5.5 frozen 조건("touch·proximity 이벤트 중")이 **충족되지 않는다** → LTA 갱신이 계속된다.

**④ 손가락이 붙어 있는 동안 LTA가 어디로 가나:** 손가락이 계속 붙어 있으면 counts는 터치 카운트(낮은 값)에 머문다. frozen이 아니므로 §5.5 IIR이 **터치 카운트를 계속 추적·재학습**한다. 즉 LTA는 터치 카운트 근처에 더 단단히 고정된다. → **은수님 7단계("터치 해제 전까지 LTA 추적이 멈춘다")의 정반대.** 데이터시트 frozen 메커니즘은 "이미 인식된 터치"를 보호하는 것이지, "부팅 시점 터치"를 보호하지 못한다.

**⑤ 손을 떼면 (자기정상화 시도):** 손을 떼면 counts가 노터치 값(높음)으로 상승. 이때 `delta = LTA − Counts`는 **음수**가 된다(LTA가 터치 카운트≈낮음, Counts=노터치≈높음). self-cap 판정공식 `(LTA − Counts) > Threshold`는 음수 delta에서 절대 충족 안 됨 → 터치 인식 안 됨(정상). 그리고 LTA는 frozen이 아니므로 IIR로 상승하는 counts를 **따라 올라간다** → 결국 노터치(500)로 수렴.

> [!IMPORTANT]
> **여기서 은수님 4·5단계는 부분적으로 구제된다 — 단 Fast Filter Band가 도와줄 때만.** §5.6: "counts가 LTA로부터 sensing 반대 방향으로 Fast Filter Band 이상 drift하면 fast filtering 적용." 손 뗀 직후 counts가 LTA 위로 크게 떨어져(self-cap에서 노터치는 counts↑ = sensing 반대 방향) Fast Filter Band를 넘으면 **fast beta**로 빠르게 LTA가 노터치로 수렴할 수 있다. 그러나 이는 (a) Fast Filter Band(0xB4) 및 LTA Fast Filter Betas(0xB2)가 설정돼 있어야 하고, (b) 현 코드는 **둘 다 reset 0(미설정)** — 펌웨어 분석/06 A.28·B4 기본값 0x0000. → **빠른 수렴 자체가 BETA·Fast Filter 설정에 전적으로 의존**(상세 A3).

**M6 결정적 반박 정리:** 은수님 7단계의 자기정상화 논리는 "터치 지속 부팅 → 해제 전까지 LTA 멈춤"을 전제하나, 데이터시트상 **부팅 시점 터치는 인식되지 않으므로(delta≈0) LTA가 멈추지 않고 오히려 터치 카운트를 추적**한다. 따라서:

- **위험 경로 (M6 반박):** 부팅 중 LTA가 터치 카운트로 단단히 학습됨 → 손 떼면 delta 음수 → 터치 해제는 자연 인식되나, **그 사이 "첫 터치 인식" 능력을 갖추려면 LTA가 노터치로 수렴 완료해야 함.** 수렴 전 구간(과도 구간)에서는 정상 터치도 미인식(LTA가 아직 터치 카운트 근처). 수렴 속도는 BETA 의존(A3).
- **은수님 7단계의 "첫 부팅 시 터치 해제 인식 가능"은 결과적으로 성립**(손 떼면 counts 상승 → LTA 추적). 그러나 그 메커니즘은 은수님이 말한 "frozen 덕분"이 **아니라** "frozen이 안 돼서 LTA가 노터치로 자유롭게 추적하기 때문"이다. **메커니즘 설명이 데이터시트와 반대**다.

**∴ Q3 결론:** delta≈0 → 터치 미인식 → frozen 안 됨은 **데이터시트로 확정(M4 반박)**. 자기정상화(M6)는 "frozen이 자기정상화를 만든다"는 은수님 논리는 **반박**되나, "frozen이 안 되기에 LTA가 결국 노터치로 수렴한다"는 다른 경로로 **결과는 부분 성립** — 단 ① 과도 구간 미인식, ② BETA/Fast Filter 설정 필수라는 두 조건 하에서만. [4_타당성검증 B3]("self-cap 부호상 즉시/영구미해제 둘 다 아님")과 정합.

---

## 5. Q5 — 노터치 카운트가 ATI Target 벗어나면 auto Re-ATI 자동 트리거 → 도움/방해 (§5.10)

### 판정: 양날 (현 코드에선 미발동, 은수님 안처럼 Full로 켜면 진동 위험)

**Re-ATI 자동 트리거 조건 (§5.10):** "re-ATI는 채널 LTA가 `ATI Band`(ATI Target 중심) 밖으로 drift할 때 실행." `Re-ATI Boundary = ATI Target ± ATI Band`. 예: Target 800, Band 1/8 → LTA>900 또는 LTA<700에서 실행.

**결정적 단서 — 자동 트리거는 ATI Mode=Full일 때만:**

- §5.11 IMPORTANT: "**ATI Error 발생 시 re-ATI가 자동 트리거되지 않는다.** master가 `Re-ATI` bit set으로 수동 트리거해야 함."
- §5.10의 "자동 트리거"는 LTA drift 감지 시 IC 내부 알고리즘이 ATI를 다시 도는 것으로, **ATI Mode가 활성(Full 등)일 때** 의미를 가진다. 현 코드는 ATI Setup(0x36) LSB=0x08 → **bits[2:0]=000=ATI Mode Disabled**(06 A.12, `tdc_drv_iqs323.c:651·687`). §5.10 개념해설도 명시: "Sound1은 ATI Mode=Disabled(autoATI 비활성)로 고정 MULT/COMP를 사용한다. ... Full Mode에서 autoATI 발동 시 I²C 무응답 사례가 있었기 때문이다."

> [!NOTE]
> 현 코드 `ATI_SETUP_LSB 0x08` = 0b0000_1000 → bit3(ATI Band)=1=Large(1/8), bits[2:0]=000=Disabled. ATI Band는 1로 설정돼 있으나 ATI Mode가 Disabled라 **Re-ATI는 자동 발동하지 않는다.** Re-ATI를 쓰려면 `re_ati_trigger()`(`tdc_drv_iqs323.c:569`)로 **수동** 트리거하며, 그것도 `calib_re_ati`(829)는 ATI Mode를 Full(0x0C)로 바꾼 뒤 트리거한다(869). 즉 측정/캘리브레이션 경로 한정.

**은수님 4·5단계 관점에서 도움 vs 방해:**

- **도움 측:** 은수님 안처럼 auto-ATI(Full)를 켜면, 손 뗀 후 노터치 counts가 Target 밖이면 Re-ATI가 자동으로 다시 돌아 노터치를 새 baseline으로 정규화 → 터치 카운트로 오염된 보정을 자가 교정. 은수님 9단계("드리프트 이슈 해결")와 정합.
- **방해 측 (진동 위험):** 부팅 중 터치 카운트로 ATI가 수렴 → 손 떼면 노터치 counts가 즉시 Boundary 밖 → **Re-ATI 재발동 → 재보정 중 measurement 일시 정지(§5.4.2 conversion 정지 / I²C 무응답 사례) → 다시 노터치 기준 → 안정.** 이 1회 재보정은 도움이나, 터치↔노터치가 반복되거나 BETA가 빠르면 ATI가 반복 발동(**진동/chatter**)할 수 있다. 또한 **Full Mode auto-ATI의 I²C 무응답이 전원버튼 겸용 시나리오의 근본 이슈**(펌웨어 분석 §14.1, 이슈 "터치 중 ATI 먹통")라 은수님 안이 다시 그 위험을 불러들인다.

**∴ Q5 결론:** Re-ATI 자동 트리거는 **현 코드(Disabled)에선 미발동**. 은수님 안처럼 Full로 켜면 1회 자가교정으로는 도움이나, **부팅 터치→해제 전이에서 재발동 + I²C 무응답 재현 위험**이 있어 양날. ATI Band(1/8)와 BETA 수렴 속도에 따라 진동 가능 — 실측 게이트 필요.

---

## 6. 명제 검증 종합표

| 명제 | 내용 | 판정 | 데이터시트 근거 |
|---|---|---|---|
| **M1** | 터치 중 auto-ATI가 MULT/COMP 적절 세팅 | **조건부지지** | §5.9 정규화는 성립 / §5.4.2 max counts·§5.11 ATI Error 게이트 |
| **M2** | auto-ATI 직후 LTA가 터치 카운트로 seed | **부분반박** | §5.9는 MULT/COMP만 — LTA seed는 §5.5.1 Reseed/IIR (B1 정합) |
| **M3** | LTA가 노터치 수렴 → 터치 시 counts 감소 → 인식 | **지지** | §5.5 IIR + §5.7 `(LTA−Counts)>Thr`, self-cap 부호 정합 |
| **M4** | 터치 인식 시 LTA frozen | **반박(전제 오류)** | §5.5 frozen은 CHx Touch bit set 후 — 부팅 delta≈0이면 미set→미frozen |
| **M6** | 터치 지속 부팅→해제 전 LTA 멈춤→자기정상화 | **결정적 반박** | frozen 안 됨→LTA가 터치 카운트 추적 / 결과는 "frozen이 없어서" 부분 성립 |

> M5(빠른 BETA)는 §5.6 Fast Filter Band가 본 frozen-미성립 시나리오의 수렴 속도를 좌우 — 동역학·부작용은 **A3 담당**. Q6·Q7은 **A6 담당**.

---

## 7. 발견 (Findings)

1. **[가장 중요] ATI는 LTA를 seed하지 않는다.** §5.9 전체가 MULT/COMP/divider만 다루며 LTA 설정 서술이 없다. 은수님 M2의 "auto-ATI 직후 LTA가 터치 카운트로 시작" 인과는 끊어져 있고, 실제 LTA seed는 §5.5.1 Reseed 또는 §5.5 IIR이 담당한다 → [B1] "오염 실채널 = Reseed" 결론과 정확히 정합.

2. **[성패 분기] 부팅 시점 터치는 frozen을 트리거하지 못한다.** baseline=터치라 delta≈0 → CHx Touch bit 미set(§5.7) → frozen 조건 미충족(§5.5) → LTA가 오히려 터치 카운트를 추적. 은수님 7단계의 "frozen 덕분에 자기정상화"는 **메커니즘이 데이터시트와 반대**.

3. **[부분 구제] 결과적 자기정상화는 가능하나 다른 메커니즘·조건부.** frozen이 안 되기에 손 떼면 LTA가 노터치로 자유 추적·수렴 → 결과는 은수님 의도와 같으나, ① 수렴 전 과도 구간 미인식, ② Fast Filter Band(0xB4)·LTA Fast Betas(0xB2) 현재 reset 0 → 빠른 수렴 미보장(A3 게이트).

4. **[Q5] Re-ATI 자동 트리거는 현 코드에서 미발동.** ATI Mode=Disabled(0x36 LSB 0x08)이므로 §5.10 자동 Re-ATI 부재. 은수님 안처럼 Full로 켜면 1회 자가교정 이점 vs 부팅 터치→해제 전이에서 재발동·I²C 무응답 재현 위험(이슈 근원).

5. **[Q1] auto-ATI 수렴은 가능하나 노터치 max counts 충돌 위험.** 터치 카운트(낮음)를 Target으로 정규화 → 노터치 counts가 배율만큼 상승 → Max Counts(06 A.7 bits[7:6]) 초과 시 conversion 정지(§5.4.2). 터치/노터치 raw counts 비 실측 필요.

---

## 8. 결론

은수님 안의 **메커니즘 골격 일부는 성립**(M1 수렴, M3 추적·판정)하나, **성패를 가르는 M4·M6(frozen 기반 자기정상화)은 데이터시트상 반박**된다. 데이터시트가 말하는 frozen(§5.5)은 "이미 인식된 터치"를 보호하는 장치이지 "부팅 시점 터치"를 보호하지 못한다 — 부팅 터치는 delta≈0으로 애초에 인식되지 않기 때문이다. 결과적으로 LTA가 노터치로 수렴해 "첫 부팅 터치 해제 인식"이라는 은수님 목표는 **다른 경로(frozen 부재 → 자유 추적)로 달성 가능**하나, 이는 (a) 과도 구간 미인식, (b) Fast Filter/BETA 설정 의존(A3), (c) Re-ATI 진동·I²C 무응답 재현(Q5)이라는 조건·위험을 동반한다.

**핵심: 은수님 안은 "auto-ATI/frozen"이라는 메커니즘 명명이 데이터시트와 어긋나며, 실제 LTA 거동을 좌우하는 것은 Reseed 시점과 BETA/Fast Filter 설정**이다. [B1]의 결론("zero-base 방어는 auto-ATI 차단이 아니라 Reseed 시딩 시점을 노터치로 게이팅")이 메커니즘 검증으로 재확인된다. 따라서 은수님 안은 **조건부지지** — frozen 논리는 폐기하되, "손 떼면 LTA가 노터치로 수렴한다"는 결과 목표는 Reseed 게이팅 + Fast Filter 설정으로 더 견고히 달성하는 방향으로 재구성해야 한다.

### 실측 게이트 (메커니즘 확정용)

| 항목 | 확인 내용 |
|---|---|
| 터치/노터치 raw counts 비 | Q1 max counts 충돌 여부 (Target 400 정규화 시 노터치 초과?) |
| 부팅 터치 시 LTA 거동 | delta≈0 → frozen 미성립 → LTA가 터치 카운트 추적하는지 RTT 1회 캡처 (M4·M6) |
| Fast Filter Band/Beta 설정 후 수렴 시간 | 손 뗀 후 LTA 노터치 수렴 ms (A3 연계) |
| Full Mode Re-ATI 발동 시 I²C 응답 | 은수님 안 채택 시 무응답 재현 여부 (Q5) |
