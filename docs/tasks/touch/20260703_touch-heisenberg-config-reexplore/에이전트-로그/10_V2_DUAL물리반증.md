---
name: 10-V2-DUAL물리반증
purpose: C-DUAL(SW 차동 CH0-CH1)의 핵심 물리 전제 3종(공통모드 결합·동시측정·co-touch veto)을 IQS323·AZD125 원문 및 회로 자료와 직접 재대조해 반증 여부 판정
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, c-dual, adversarial-verify, differential, common-mode, sequential-scan, co-touch]
---

# 10 · V2 — C-DUAL 물리 반증 (공통모드·동시측정·co-touch)

**TL;DR**: 질문(2) 동시측정 전제는 원문 3곳(p.15·21)으로 **반증 확정** — IQS323은 단일 엔진 순차측정, G1·G2·V3의 미확인을 해소한다. 질문(1) 공통모드는 실측 전 확정 불가하나 Azoteq 공식가이드(AZD125 p.12)상 비대칭이 물리적 기본값. 질문(3) co-touch도 개연성 높음(동일 가이드 경고 확인). 종합: **C-DUAL 핵심 물리 전제 기각**.

## 핵심 판정

| 질문 | 판정 | 확신도 | 근거 |
|---|---|---|---|
| (1) 공통모드 vs 비대칭 결합 | 실측 전 확정 불가, **비대칭 쪽에 무게** | 중 | §2 |
| (2) 동시 vs 순차 측정 | **순차 측정 — 반증 확정** | 높음(원문 직접 재확인) | §1 |
| (3) co-touch veto가 정상터치 억제 | **개연성 높음**(미실측) | 중~높음 | §3 |

**질문 셋 중 하나라도 참이면 실효 부정**(오케스트레이터 조건) — (2)가 이미 확정, (1)·(3)도 부정 방향 — **C-DUAL 실효 부정, 킬 확정**.

## 0. 검증 방법

`iqs323_datasheet.pdf`(68p)·`azd125-capacitive_sensing_design_guide_v1.2.pdf`(41p) 원문을 본 세션에서 `pdftotext`로 직접 추출 후 원어(영문) 재대조. 프로젝트 내 한국어 정리본(레퍼런스·데이터시트 폴더)의 인용은 참고만 하고 전부 원문 대조로 재확인 — 파생 문서를 신뢰하지 않는다.

---

## 1. 질문(2) — 동시 vs 순차 측정: 순차 확정 (G1·G2·V3의 "미확인" 해소)

**원문 3건 직접 확인(verbatim)**:

| 원문 인용 | 위치 | 함의 |
|---|---|---|
| "The IQS323 contains a single ProxFusion® module that uses patented technology to measure and process the sensor data." | §5 도입부, **p.15** | 채널마다 독립 엔진이 아니라 **엔진 1개** — 병렬 동시변환 불가 구조 |
| "Other sensor channels are sampled at a slower rate in order to optimize power consumption" | §5.2 ULP, **p.15** | 채널마다 **다른 샘플링 주기** 가능 — 완전 동시(lock-step)라면 성립 불가한 서술 |
| "The Inactive Rxs in the Pattern Definitions register sets the state of any Rxs that are **not selected for the currently executing conversion**." | §6.1, **p.21** | "현재 실행 중인 변환"(단수)이라는 표현 자체가, 활성 채널이 여럿이어도 특정 순간엔 **하나만 변환 중**임을 전제 |

세 인용 모두 본 세션에서 원문 재확인(할루시네이션 없음). "sequential"·"simultaneous"라는 단어 자체는 데이터시트에 없으나, 위 세 서술을 종합하면 **동시변환을 지지하는 대안적 해석이 성립하지 않는다** — 이는 추론이지 원문 직접 인용은 아니므로 "확립(원문 종합)"으로 표기.

**독립 교차근거(Azoteq 공식, 타 문서)**: `azd125-capacitive_sensing_design_guide_v1.2.pdf` §3.2.4(**p.12**) 원문: "Sensors that are routed next to each other, but sensed in **different time slots** are typically **not prone to the effects of crosstalk**." — Azoteq 스스로 자사 정전용량 IC 아키텍처를 "채널별 타임슬롯 분리"로 서술한다. 채널-채널 간 상호 크로스토크 논의이나, "시분할"이 Azoteq IC의 기본 동작 모델이라는 방증으로 위 3건과 정합.

> [!IMPORTANT]
> **C-DUAL 실효 타격**: §1의 `corrected = raw_delta0 - k×raw_delta1` 공식은 CH0·CH1이 **같은 순간**의 애그레서를 겪는다는 전제 위에 있다. 순차 변환이 확정되면, 두 값은 시간차 Δt(추정 — 실측 게이트) 뒤에 떠진 별개 스냅샷이다. SYSCLK(30.72MHz, 주기 약 32.6ns)·SPI 토글은 Δt(전형적 정전용량 charge-transfer 채널 1회 변환 시간은 ATI Target(Sound1 약 400 counts, `데이터시트/02` 각주)/변환주파수(최대 약 1MHz)로 역산하면 대략 수백 µs 규모로 추정 — **원문에 직접 명시된 수치 아님, 본 노드의 유도치**) 대비 수천~수만 배 빠르므로, 순간 결합량이 "동일 크기로 얼렸다 나눠 담기"보다 "서로 다른 위상을 우연히 표본"하는 쪽에 가깝다. 특히 실제 실패 양상(순간 threshold 넘어 오터치)이 평균 잡음이 아니라 **단발성 스파이크**라면, 순차 스냅샷은 상쇄에 필요한 상관성을 구조적으로 담보하지 못한다(가설이나 시간축 두 자릿수 이상 격차에 근거한 강한 개연성).

---

## 2. 질문(1) — 공통모드 vs 비대칭(CRX0 중앙/CRX1 외곽) 결합

**P2(H4) 자체 판정**: `02_P2_노이즈EMC하이젠베르크.md`는 "CRX0(중앙)/CRX1(외곽) 비대칭 배치상 국소 fF 트레이스 결합(I2C·SYSCLK)은 상대적 차동성"을 **가설(H4)**로만 제시하며, §9에서 스스로 "PCB 레이아웃 실측 미확인… 정성적 추정에 불과"라고 명시한다 — 즉 이 축은 P2 내부에서도 **확정된 반증이 아니라 미확정 가설**이다. C-DUAL의 지지 가설("공용 VDD/VREG 리플 → 공통모드")도 동급 미확정이다(C-DUAL §2 자인).

**본 노드가 추가한 독립 판단 근거(Azoteq 공식 문서, 신규)**: `azd125` §3.2.4(**p.12**) 원문: "Digital signals such as PWM signals, I2C or SPI are active during a capacitive measurement, **unlike other capacitive traces**. It is recommended that the signals be kept a minimum of **4 mm** away from the capacitive sensor traces…" — Azoteq는 SPI/I2C류 디지털 신호를 "다른 정전용량 트레이스와 달리" 항상 활성인 별개 애그레서 범주로 분류하고, 그 저감책을 **오직 물리적 거리(≥4mm)** 로만 제시한다. 시분할·공통모드 개념이 이 항목엔 아예 등장하지 않는다.

> [!NOTE]
> **판정 근거**: 공통모드 상쇄가 성립하려면 CRX0·CRX1 트레이스가 SPI/SYSCLK 발원지로부터 **같은 거리·같은 기하**를 가져야 한다. 그러나 회로 실측(`회로 구성·분석.md` §2·§7.2, P4 교차 확인): CRX0(중앙 필)→R31(100Ω)→C51(100pF)→J3, CRX1(외곽 링)→C52(100nF)→R32(0Ω)→J4 — **서로 다른 RC망·다른 커넥터**로 라우팅되며 의도적 매칭(디퍼렌셜 페어·guard 대칭 등) 설계 흔적이 없다. 공통모드는 "의도적으로 설계돼야 얻어지는" 성질이지 "두 신호를 같은 IC가 읽는다"는 사실만으로 공짜로 따라오지 않는다 — **입증 책임은 공통모드 쪽에 있고, 현재 그 입증이 없다.**

**결론**: 확정 반증은 아니나(실측 없이는 확언 불가), 물리적 디폴트는 비대칭 쪽 — C-DUAL이 "공통모드가 그럴듯하다"는 전제로 §2 표에서 지지·반증을 대등하게 놓은 것은 **균형이 맞지 않는다**(비대칭 쪽이 더 무겁다). **미확인(실측 게이트)** — Phase 1 실측(§3 C-DUAL 원문)으로만 해소 가능.

---

## 3. 질문(3) — D패드 동심구조 co-touch veto의 정상터치 억제 회귀

**P4 재확인(`04_P4_전극패드물리설계.md` §2-3)**: 중앙 채움(CRX0)+외곽 박형 링(CRX1)이 "**하나의 물리 버튼 표면 아래**"에 위치. 절대 치수(링 폭·링-필 갭)는 **미확인**(치수선 없음, 실측 필요) — 이 점은 P4 스스로도 최우선 게이트로 인정.

**본 노드가 추가한 독립 근거(Azoteq 공식, 신규)**: `azd125` §3.2.2(**p.12**) 원문: "The spacing between electrodes should be sufficient to avoid **false touch detections, like triggering two buttons when only one button-press was intended**." — Azoteq 자신이 버튼 응용에서 전극 간격 부족 시 정확히 이 실패 양상("의도한 건 하나인데 둘 다 반응")을 명시 경고한다.

**물리적 판단**: 자기정전용량(self-cap)의 프린징 필드는 전극 구리 경계에서 물리적으로 끊기지 않고 오버레이 위로 퍼진다(§5.4/§6.1 개념). CRX0·CRX1이 **동심으로 맞닿아 있고, 사용자가 버튼 하나를 누르는 단일 동작**이 전제라면 — 별개의 두 버튼 사이 최소 간격 확보조차 어려운 "인접 버튼" 케이스보다 **더 나쁜 조건**(애초에 분리 의도가 없는 하나의 표면)이다. 정량 크기(손끝 접촉 지름 대비 링 위치)는 미확인이나, 방향성 판단은 회의적 쪽이 타당하다.

**결론**: **미확인(실측 게이트)**이나 개연성 높음. C-DUAL 자신이 "최대 리스크"로 이미 지목한 항목을 Azoteq 공식 문서의 동일 경고로 보강 확인 — 반증 방향 강화.

---

## 4. 부가 확인 (요청 범위 밖이나 검증 과정에서 확보, 참고용)

- **G2 §4.3 가설_A/B 상충 해소**: `Prox Input and Control`(0x43) reset=`0x01CF` 비트분해(A.9, p.54) 직접 재계산 → bit8(CRx0)=1, bit9(CRx1)=0 — **Sensor1(CH1)의 reset 기본값은 물리 CRx0(=CH0과 동일 핀)을 향한다(가설_A 확정, 가설_B 기각)**. C-DUAL의 실측 게이트 목록(§6-3)이 우선 해소해야 할 항목 중 하나가 본 노드에서 해결됨.
- **C-DUAL §3 레지스터 유도값 검산**: 목표값 `0x02CF`(MSB 0x02=CRx1 선택, LSB 0xCF=reset 유지) 비트분해 재검산 — 예약비트(bit15-14=0, bit7=1, bit4=0, bit1-0=11) 전부 reset과 동일하게 보존, 침범 없음. **정확 — 할루시네이션 없음**(V3의 동일 항목 검증과 수렴).
- **ATI Setup(0x46) reset=0x040C=Full 확인**: A.12(p.55) 재계산 결과 bit[2:0]=100=Full 일치. **정확**(V3 재확인과 수렴).

## 5. 살아날 여지 (정직히 기록)

닫힌 문제 아님 — 다음 조건이 **실측으로 확인되면** 부분 생존 가능:
1. fake_func_sleep 재현 중 CH0·CH1 raw counts 상관계수가 실제로 높게 나오는 경우(질문 1의 비대칭 우려를 실측이 뒤집는 경우).
2. Δt(채널 간 변환 시간차)가 SPI/SYSCLK 버스트 지속시간보다 충분히 짧아, 순차임에도 동일 버스트를 함께 포착하는 경우.
3. 손끝 실측(전극 델타)에서 CRX0 터치 시 CRX1 반응이 threshold 이하로 무시 가능한 수준인 경우(질문 3 기각).

세 조건 모두 C-DUAL 자신이 이미 제안한 "Phase 1(계측 전용, veto 미적용)" 실측으로만 검증 가능 — 문서 대조만으로는 어느 쪽도 확정할 수 없다는 것이 정직한 결론이다. 단, 사전 확률은 위 §1-3 근거상 **낮게** 본다(세 조건이 동시에 모두 충족돼야 하는데 각각이 개별적으로 불리한 쪽에 무게가 실려 있음).

## 6. 종합 판정

C-DUAL이 성립하려면 "CH0·CH1이 같은 순간, 비슷한 크기로, 터치와 무관하게" 공통 잡음을 겪어야 한다. 본 노드는 이 셋 중 **동시성을 원문 직접 재확인으로 반증**했고, 나머지 둘(공통모드 크기·터치 무관성)도 Azoteq 공식 설계 가이드의 일반 원칙(디지털 신호는 거리로만 저감, 버튼 간격 부족은 오검출 유발)에 비추어 부정적 방향으로 기운다. **하드 할루시네이션은 발견되지 않았다**(C-DUAL 자신의 레지스터 인용은 정확 — 문제는 인용의 사실성이 아니라 물리적 전제의 근거 부족과 신규 확인된 반증 사실). 은수님께 정직하게 보고할 결론: **C-DUAL은 "약함"이 아니라 핵심 전제 중 하나(동시성)가 명확히 반증됐고, 나머지도 실측 없이는 옹호할 근거가 없다 — 조기 기각이 타당하다.**

---

## 근거

- `iqs323_datasheet.pdf` p.15(§5 도입·§5.2 ULP)·p.18-19(§5.11 ATI Error)·p.21(§6.1 Inactive Rxs)·p.35(§9 메모리맵 0x13-0x18·0x40-0x49)·p.54-55(A.9·A.12) — 본 세션 `pdftotext` 직접 추출·재확인
- `azd125-capacitive_sensing_design_guide_v1.2.pdf` p.12(§3.2.2 Spacing between Electrodes·§3.2.4 Crosstalk, 디지털 신호 4mm 규칙 포함) — 본 세션 직접 추출·재확인
- `05_C-DUAL_SW차동.md`(검증 대상 전문), `02_G2_SW측정채널능력.md` §4.3(가설_A/B), `01_G1_referencetracking실능력.md` §5(핸드오프 미확인 인계)
- `20260703_touch-mutual-cap-review/에이전트-로그/02_P2_노이즈EMC하이젠베르크.md`(H4)·`04_P4_전극패드물리설계.md`(§2-3, 동심구조)
- `참고/touch/개선/회로 구성·분석.md` §2·§7.2(CRX0/CRX1 RC망·커넥터 실측)
- `11_V3_DUAL할루시네이션감사.md`(선행 병렬 감사 — 레지스터 인용 5건 하드 할루시네이션 0건 확인, 동시/순차 축은 "미확인"으로 남김 — 본 노드가 이를 해소)
