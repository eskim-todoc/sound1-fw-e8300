---
name: 07-C-FILT-공격적필터ATI임계
purpose: 필터(Counts/LTA Beta·Fast Filter Band)·터치 임계(Threshold/Hysteresis)·ATI(Band/Target) SW 재튜닝으로 잡음성 counts 이탈의 오탐(ATI 에러·false touch)을 억제하는 후보 도출 — INV-CTRL-1~3 보존 전제
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, filter, beta, threshold, hysteresis, ati-band, debounce, candidate]
---

# 07 · C-FILT — 공격적 필터·ATI·임계 재튜닝 후보

**TL;DR**: SW-only 레버 5종을 근거강도순 평가. **Touch Debounce Enter**(잡음 스파이크 연속조건화)가 물리적으로 가장 정합적이나 **레지스터 비트위치 자체가 미확인**(A.17 표와 §5.7 서술 불일치). Counts Beta는 이미 강한 스무딩 상태(추가여력 제한), Touch Threshold 상향은 직접적이나 과거 실패 이력과 정면충돌, Hysteresis는 애초 오탐(진입) 방지 레버가 아님, Fast Filter Band는 방향성 제약으로 근거 약함. ATI Band는 이미 최대치(추가헤드룸 없음). 모든 레버의 유효성은 "노이즈가 지속성이냐 버스트성이냐"라는 미확인 물리 전제에 게이팅된다.

## 0. 판정 요약

| 레버 | 레지스터 | 현재값 | AC 노이즈 저감 근거강도 | 헤드룸 |
|---|---|---|---|---|
| A. Touch Debounce Enter | 0x62 bits[11:8](**미확인**) | 0(추정) | 중간(원리 정합, 비트 미확인) | 있음(단 존재 자체 미확인) |
| B. Counts Beta | 0xB0=0x0202 | β=2 | 중간(원리 확립, 이미 강함) | 제한적(β=1이 하한) |
| C. Touch Threshold(k) | 0x62 LSB | 80 | 강함(공식 확립, 직접적) | 있음(단 과거 실패 이력) |
| D. Touch Hysteresis | 0x62 bits[15:12] | 8 | **오탐(진입) 무관** — release 안정화만 | 있음(최대 15) |
| E. Fast Filter Band | 0xB4 | 10 | 약함(방향성 제약) | 있음(효과 불확실) |
| F. ATI Band | 0x36 bit3 | 1(Large=1/8, 이미 최대) | 근거 있으나 **헤드룸 0** | 없음 |
| G. ATI Target(Resolution Factor) | 0x36 bits[15:4] | 64(Target=400) | **미확인**(게인-노이즈 스케일링 가정에 의존) | 있음(단 리스크 큼) |

---

## A. Touch Debounce Enter — 최유력, 단 비트위치 미확인

**작동 메커니즘**: `delta > Threshold` 조건이 **N+1회 연속** 충족되어야 비로소 CH0_Touch bit set. 조건이 1회라도 미충족되면 카운터가 0으로 리셋(`02_proxfusion동작.md §5.7` NOTE, 원문 취지 재구성). §5.7 원문은 debounce 필요성의 원인으로 명시적으로 "기계적 진동·**전원 노이즈**·RF 간섭·ESD 등이 수 나노초~수 밀리초 단위의 순간적인 counts 변화"를 지목한다 — **fake_func_sleep()의 SPI/SYSCLK 스위칭 교란과 물리적 성격이 가장 가까운 문서상 서술**이다.

**AC 노이즈 저감 근거강도 — 중간(원리는 확립, 적용가능성은 미확인)**: 원리 자체(연속조건화가 순간 스파이크를 배제)는 §5.7에 명시적으로 확립. 그러나 **Touch Settings(0x62) 레지스터에 실제 Debounce Enter 필드가 존재하는지 자체가 근거 상충**:
- A.17 비트표(`06_레지스터레퍼런스.md`)는 0x62를 bits[15:12]=Hysteresis, bits[7:0]=Threshold **2개 필드만** 기재 — bits[11:8]은 표에 없음(비워짐).
- 반면 §5.7 일반 서술(`02_proxfusion동작.md` L466)은 "Debounce는 Prox Settings의 필드와 **Touch Settings의 대응 필드**에서 설정한다(각 4비트)"라 하여 Touch에도 대응 필드가 있다고 암시.
- 비트 산술: Prox Settings(0x61)는 Threshold(8bit)+Enter(4bit)+Exit(4bit)=16bit로 꽉 채워짐(A.16 확인). Touch Settings(0x62)는 Threshold(8bit)+Hysteresis(4bit)=12bit이므로 **남는 4bit(bits[11:8])는 Enter 또는 Exit 둘 중 하나만 수용 가능**(Prox처럼 둘 다는 구조상 불가) — 이는 표 공백에 대한 산술적 추정이지 원문 확인이 아니다.

**필요 레지스터/시퀀스(미확인 명시)**: bits[11:8]이 Debounce Enter가 맞다면, `touch_settings()`(iqs323.c:284-290) MSB 조립식을 `(hyst&0x0F)<<4 | (enter&0x0F)`로 확장. 예: hyst=8·enter=2 → MSB=0x82. §5.7 SW구현 관점 note(L466)의 경험적 출발점 "Enter=2~3"을 그대로 적용 가능. **WAIT**: 필드 존재·정확한 비트폭·Exit 겸용 가능 여부는 실측(레지스터 write 후 debounce 거동 관찰) 또는 원문 PDF §5.7·A.17 인접 페이지(p.17-18) 재대조 전까지 확정 불가.

**3불변식 보존**: 0x62는 INV-CTRL-1(0x36)·INV-CTRL-2(0xC0 Re-ATI 트리거)·INV-CTRL-3(boot_ignore/sleep_ignore FSM, main.c/logic.c)의 어느 코드훅과도 겹치지 않는다(G3 §5 자유변수 목록에 0x61/0x62 명시 포함). Debounce는 IC 내부에서 `pressed`(System Status bit9) 산출 이전 단계에 적용되므로 FSM은 여전히 동일한 `pressed` 신호를 그대로 소비 — **완전 보존**.

**한계·리스크**: (1) 필드 존재 자체가 미확인이라 최악의 경우 이 레버는 실현 불가능. (2) 존재하더라도 응답 지연 증가(Enter=2 시 NP 20ms 주기 기준 +40~60ms, §5.7 L461) — 롱터치·즉각 반응성과 트레이드오프. (3) fake_func_sleep()의 노이즈가 **단발성 스파이크가 아니라 지속적 편향**이라면(§7 게이팅 질문 참고) 이 레버는 무력.

---

## B. Counts Beta 강화 — 이미 강한 상태, 추가 여력 제한적

**작동 메커니즘**: `Filtered Counts`(0x13, threshold 비교에 실제 쓰이는 값)는 raw counts에 IIR `filtered = filtered + (raw-filtered)×(Beta/256)`을 적용한 결과다(`02_proxfusion동작.md §5.6`). Beta가 작을수록(=1 하한) 매 conversion마다 새 입력 반영 비중이 줄어(β=1→α≈0.4%) 순간 노이즈가 누적 없이 감쇠된다.

**AC 노이즈 저감 근거강도 — 중간(원리 확립, 이미 거의 포화)**: 현재 β=2(α≈0.78%, `beta_power_settings` iqs323.c:312)는 이미 4비트 필드의 하한(β=1, α≈0.4%)에 근접 — **추가로 낮출 수 있는 폭이 β=1 한 단계뿐**이다. 이미 강한 스무딩이 걸려 있다는 사실 자체는, 만약 여전히 오탐이 재현된다면 "단발 스파이크가 이미 걸러지고 있는데도 재현된다"는 뜻이 되어 **노이즈가 지속성일 가능성을 오히려 뒷받침**하는 간접 정황이 될 수 있다(§7 참고, 가설).

**필요 레지스터/시퀀스**: `write_register(REG_BETA_COUNTS, 0x01, 0x01)`(NP/LP 동일 β=1). **미확인**: Beta 공식 해석 자체가 두 자료(데이터시트 Beta/256 vs 앱노트 AZD004 1/2^β)에서 상충하며 기존 문서(`터치 임계·계수 레지스터 동작.md §6.2`)가 이미 [실측 게이트]로 지정한 미해결 항목 — 어느 해석이 맞든 β를 더 낮추는 방향(=더 강한 스무딩)은 동일하므로 방향성 판단에는 영향 없음.

**3불변식 보존**: 0xB0은 beta_power_settings() 내부 write로, INV-CTRL 코드훅과 무관 — **완전 보존**.

**한계·리스크**: 실제 터치 반응 지연 증가(β↓ = 추적 느림, 이미 코드 주석이 β=6 LTA 기준 "터치 흡수 최소화" 균형점으로 잡아둔 것과 같은 트레이드오프가 Counts Beta에도 적용). 지속성 노이즈에는 무력(§7).

---

## C. Touch Threshold(k) 상향 — 가장 직접적이나 과거 실패 이력과 충돌

**작동 메커니즘**: 절대임계 = k×LTA/256(A.17 확인). k를 올리면 델타가 더 커야 터치로 판정 — 노이즈 마진 직접 확대.

**AC 노이즈 저감 근거강도 — 강함(공식 확립, 인과 직접적)**: 유일하게 "델타가 임계를 넘느냐"라는 **판정식 자체**를 직접 건드리는 레버 — 노이즈 성격(지속/버스트 무관)에 상관없이 델타가 커진 순간의 오탐 확률을 산술적으로 낮춘다.

**필요 레지스터/시퀀스**: `TDC_TOUCH_IQS323_THRESHOLD`(iqs323.h:46) 상향, `touch_settings()` 경유 0x62 LSB 재write.

**3불변식 보존**: 완전 보존(0x62, G3 자유변수 목록 명시).

**한계·리스크(정량적, 코드 주석 근거)**: 현재 k=80(임계125)은 "실제 터치 D 160~190의 약 78%"(iqs323.h:46 주석)로 이미 마진을 최소화한 값이며, 동 주석은 **"수렴에 막혀 158 도달난 → 80으로 하향"**이라 명시 — 즉 **더 높은 k(약 158 부근)를 이미 한 차례 시도했다가 실패해서 낮춘 이력이 있다**. 그 실패의 정확한 메커니즘(ATI 수렴 문제인지 실터치 미검출인지)은 본 노드 범위 밖(20260622 계열 작업 이력 재확인 필요, **미확인**) — 하지만 "k를 올리는 시도가 이미 한 번 실패했다"는 사실 자체가 이 레버의 최대 리스크다. k를 완만히(예: 80→100) 올려 여지를 보는 절충안은 가능하나, 실터치 델타 하한(약 160)과의 마진이 줄어드는 것은 산술적으로 불가피.

---

## D. Touch Hysteresis 상향 — 오탐(진입) 방지 레버가 **아님**, release 안정화 전용

**작동 메커니즘**: 이탈(release) 판정만 `delta < (Threshold − Hysteresis)`로 낮춤(`터치 임계·계수 레지스터 동작.md §4`, A.17). **진입(entry) 조건에는 전혀 관여하지 않는다** — 진입은 오직 Threshold(레버 C) 단독으로 결정.

**AC 노이즈 저감 근거강도 — 오탐(false entry) 방지에는 근거 없음**: 오케스트레이터가 지목한 문제(노터치 idle 상태에서의 오인식=false entry)에 Hysteresis는 원리적으로 관여하지 않는다. 다만 **INV-CTRL-3(첫 터치 해제 감지)의 신뢰성 보조**에는 간접 기여 가능 — release 경계 부근에서 노이즈로 `pressed` bit가 깜빡이면 FSM의 release debounce 카운터가 매번 리셋되어(G3 §3, boot_ignore/sleep_ignore 게이트가 안 풀림) 게이트 해제가 지연될 수 있는데, Hysteresis를 키우면 이미 released 판정에 들어간 뒤 재진입 문턱이 낮아 이 깜빡임이 줄어든다.

**필요 레지스터/시퀀스**: `TDC_TOUCH_IQS323_HYSTERESIS`(iqs323.h:49) 8→15(4비트 최대), 0x62 MSB 상위니블 재조립.

**3불변식 보존**: 완전 보존.

**한계·리스크**: 4비트 상한(15)이라 실제 hyst는 threshold의 최대 ~5.9%(터치 임계·계수 문서 §4 확인) — 효과 자체가 작다. 오탐(진입) 문제 해결로 오인하면 안 됨.

---

## E. Fast Filter Band 확대 — 약한 근거, 방향성 제약

**작동 메커니즘**: counts가 LTA로부터 **"sensing 반대 방향"으로** Fast Filter Band 이상 벗어나면 LTA가 Fast Beta(빠른 추종)로 전환(`02_proxfusion동작.md §5.6`). 현재 10(0xB4).

**AC 노이즈 저감 근거강도 — 약함**: 원문이 명시하는 적용 방향이 "sensing 반대 방향"(self-cap에서 touch 방향과 반대 = counts 증가 쪽) **한정**이다. 그런데 false touch를 유발하는 노이즈는 정의상 counts를 touch 방향(감소)으로 미는 사건이므로, **Fast Filter Band를 넓혀도 그 방향의 LTA 추종 속도에는 영향이 없다** — LTA가 touch 방향 노이즈를 얼마나 빨리 쫓아가는지는 이미 Normal Beta(레버 B와 별개, 0xB1)가 전담. 즉 이 레버는 "noTouch 복귀 방향의 재정렬 속도"를 조절할 뿐, 오탐(entry) 방지에 대한 인과 경로가 원문상 명확하지 않다.

**필요 레지스터/시퀀스**: `write_register(REG_FAST_FILTER_BAND, 0x1E, 0x00)`(예시 30) — 존재·조립은 확립.

**3불변식 보존**: 완전 보존.

**한계·리스크**: 효과의 방향성 자체가 우리 문제(touch 방향 오탐)와 어긋날 가능성 — 우선순위 최하위로 권고.

---

## F·G. ATI Band·ATI Target — 헤드룸 소진 및 고위험 미확인 영역

**F. ATI Band(0x36 bit3)**: 현재 이미 **Large(1/8, 최대)**(A.12 확인, iqs323.c:437 `0x0C`=bit3=1). 옵션은 Small(1/16)·Large(1/8) 2종뿐 — **더 넓힐 옵션 자체가 존재하지 않는다.** 헤드룸 0, 기각.

**G. ATI Target(Resolution Factor, bits[15:4], 현재 64→Target 400)**: Target을 올리면 Re-ATI Boundary(=Target±1/8·Target)의 **절대 counts 폭**이 커진다(예: Target 800이면 밴드 폭 100 vs 현재 50) — 절대 여유가 커진다는 점에서 이론상 노이즈 마진 확대 가능. 그러나:
1. **물리 가정 미확인**: 이 확대가 실제로 도움되려면 "노이즈가 ATI 게인에 비례해 스케일되지 않는 고정 크기 교란"이어야 한다(코드 근거 없음, 순수 추론) — 만약 노이즈가 게인에 비례 증폭된다면 Target을 올려도 상대 SNR은 그대로다. **판정 불가(실측 필요)**.
2. **G3 범위 충돌 소지**: G3(`03_G3_3불변식코드훅.md §1`)는 "0x36과 그 도달 시퀀스(437·447·451) **자체는 불변**"이라 서술 — 이 문구가 Mode 비트(bits[2:0])만 지칭하는지 Resolution Factor·Band까지 포함하는지 **G3 원문 자체가 모호**하다. G3 §0 계약표는 "Full 모드 유지"만 명시했으나 §1 서술은 레지스터 전체를 지칭하는 것처럼도 읽힌다 — **오케스트레이터/G3 확인 필요**.
3. **역사적 리스크 신호**: `02_proxfusion동작.md L658`(§5.10 인접 NOTE)은 "Sound1은 ATI Mode=Disabled... 이유는 Full Mode에서 autoATI 발동 시 **I²C 무응답 사례**가 있었기 때문"이라 서술한다. 이는 현재 코드 상태(Full Mode 확인됨, iqs323.c:437)와 **불일치하는 구버전 주석으로 추정**(작성 시점 불명, 코드 헤더의 "FullATI 전환"과 시점 선후 미확인) — 그러나 "Full 모드에서 autoATI(§5.10, 런타임 중 IC 자율 재캘리브레이션) 발동 시 I2C 무응답"이라는 **구체적 과거 실패 이력**이 이 프로젝트에 존재한다는 사실 자체는 진지하게 받아들여야 한다.
4. **현재 가시성 0(코드 확인)**: `tdc_touch_iqs323_read_status()`(iqs323.c:347-367)는 bit5(ati_active)·bit6(ati_error)만 추출하고 **System Status bit4(ATI Event, §5.10의 자율 재-ATI 발생 신호, A.2 확인)는 전혀 읽지 않는다** — 즉 자율 Re-ATI가 지금 이 순간에도 조용히 발동 중인지조차 FW가 알 수 없다(확립 사실, 코드 grep 근거).

**종합**: G는 이론적 헤드룸은 있으나 (a) 효과 방향 자체가 불확실하고 (b) 이 프로젝트의 과거 I2C 무응답 이력과 맞물린 위험 신호가 있어 **가장 낮은 우선순위, 실측 없이는 시도 비권고**.

---

## 7. 게이팅 질문 — 모든 레버를 가르는 미확인 물리 전제

**노이즈가 지속성(baseline bias, 매 conversion마다 반복)인가 버스트성(간헐적 discrete spike)인가?** 이 답에 따라:
- 버스트성이면 → 레버 A(Debounce)·B(Counts Beta)가 유효(순간 스파이크를 연속조건·IIR로 흡수).
- 지속성이면 → A·B 모두 결국 수렴해 무력화되고, 레버 C(Threshold 절대 상향)만 유효.

본 노드는 이 판정에 필요한 실측 파형/counts 로그를 확보하지 못했다(**미확인**) — `tdc_touch_iqs323_read_debug()`(0x13/0x14 read, iqs323.h:100-102)가 이미 코드에 존재하므로, fake_func_sleep() 구간에서 이를 활용한 Counts/LTA 시계열 로깅이 구현 전 선행 검증으로 강력 권고된다(단, 현재 read_debug는 "노말 폴링에서만 사용"(iqs323.h:91) — fake_func_sleep 경로 배선은 구현 단계 소관, 본 노드는 코드 변경 없음).

## 8. 종합 권고 순서

1. **선행**: read_debug()로 fake_func_sleep() 중 Counts/LTA 시계열 확보 → §7 게이팅 질문 해소.
2. **1순위**: 레버 C(Threshold 완만한 상향, 80→100 부근) — 근거 가장 강하고 즉시 적용 가능, 단 과거 실패 이력 재확인 병행.
3. **2순위**: 레버 A(Debounce Enter) — 비트위치 확인 후(원문 PDF 재대조) 유효하면 최우선 후보로 격상 가능.
4. **3순위**: 레버 B(Counts Beta=1) — 저위험, 효과 제한적이나 부작용도 작음.
5. **보조**: 레버 D(Hysteresis 최대화) — 오탐방지 아님, INV-CTRL-3 신뢰성 보조 목적으로만.
6. **비권고**: 레버 E(Fast Filter Band), 레버 F(헤드룸 0), 레버 G(미확인 물리가정+역사적 리스크).

## 9. WF2가 반드시 사실 대조할 핵심 주장 목록

1. Touch Settings(0x62) bits[11:8]이 실제 Touch Debounce Enter/Exit 필드인지 — A.17 표와 §5.7 서술 불일치, 원문 PDF 직접 재대조 필요.
2. Threshold 비교 대상이 raw counts가 아니라 Counts Beta 적용 후의 "Filtered Counts"(0x13)라는 것 — 원문 페이지 재확인.
3. Fast Filter Band가 "sensing 반대 방향"에만 적용된다는 것(touch 방향엔 미적용) — 원문 재확인.
4. §5.10 Automatic Re-ATI가 ATI Mode=Full에서도 활성인지(Disabled 전용이 아닌지).
5. `02_proxfusion동작.md L658`의 "Sound1 ATI Mode=Disabled·I2C 무응답" 주석 작성 시점과 현재 Full Mode 전환 시점의 선후 관계 — 구버전 서술인지 확정.
6. G3의 "0x36과 그 도달 시퀀스 자체는 불변"이 Resolution Factor/Band 서브필드까지 포함하는 의도였는지.
7. ATI Target 확대가 노이즈 대비 실제 SNR을 개선하는 방향인지(게인-노이즈 스케일링 가정) — 실측 전 판정 불가.
8. 노이즈가 지속성/버스트성 중 무엇인지(§7) — 모든 레버 유효성의 최상위 게이팅 조건.
9. iqs323.h:46 주석 "158 도달난" 실패의 정확한 원인(ATI 수렴 vs 실터치 미검출) — 20260622 계열 작업 이력 재확인 필요.

## 근거

- `tdc_touch_iqs323.c`(전체 Read, L266-460 시퀀스·레지스터 상수 정의 L17-34) — 코드 Read 전용, 수정 없음.
- `tdc_touch_iqs323.h`(L36-113, THRESHOLD/HYSTERESIS/PROX_THRESHOLD 정의·주석).
- `데이터시트/02_proxfusion동작.md` §5.6(Filter Beta, L354-394)·§5.7(Threshold/Hysteresis/Debounce, L396-491)·§5.9(ATI, L541-639)·§5.10(Automatic Re-ATI, L641-658)·§5.11(ATI Error, L660-667).
- `데이터시트/03_하드웨어설정.md` §6.1(Inactive Rxs, L18-49, 이미 VSS 확인).
- `데이터시트/06_레지스터레퍼런스.md` A.2(System Status bit4 ATI Event, L158-176)·A.9/A.10(L298-329, Inactive Rxs 기본값)·A.12(ATI Setup, L338-346)·A.16/A.17(Prox/Touch Settings, L381-406).
- `터치 임계·계수 레지스터 동작/터치 임계·계수 레지스터 동작.md` §3-6(THR/HYST 공식·Beta 해석 상충 [실측 게이트] 기존 확인).
- `01_G1_referencetracking실능력.md`·`02_G2_SW측정채널능력.md`·`03_G3_3불변식코드훅.md`(그라운드 사실 인용).
