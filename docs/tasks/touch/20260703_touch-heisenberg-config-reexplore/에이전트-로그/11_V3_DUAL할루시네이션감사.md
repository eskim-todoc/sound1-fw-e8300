---
name: 11-V3-DUAL할루시네이션감사
purpose: C-DUAL·C-RT가 인용한 레지스터 주소·비트필드·reset값·데이터시트 원문 인용 5건을 IQS323 데이터시트 PDF 원문(p.14-30·35-37·49-64)과 코드(tdc_touch_iqs323.c)로 직접 재대조하는 할루시네이션 감사
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, hallucination-audit, register-verification, adversarial-verify, c-dual, c-rt]
---

# 11 · V3 — C-DUAL·C-RT 레지스터 인용 할루시네이션 감사

**TL;DR**: 지정 5항목(0x15/16 disable 시 유효성·0x46 reset·0x43 reset/비트/목표값·ATI Error 전역성·Channel Setup/Follower Weight 비트필드) 전부를 `iqs323_datasheet.pdf` 원문(p.35-37, 49-64, 24-28)과 직접 재대조, 추가로 코드(`tdc_touch_iqs323.c`)까지 교차검증. **레지스터 주소·비트·reset값·원문 인용 5건 모두 원문과 일치 — 하드 할루시네이션 0건.** 유일한 흠은 C-DUAL 요약표가 (1)항을 "확립"으로 표기했으나 본문 §6이 스스로 "미확인"으로 남긴 **내부 불일치(과장)** 1건 — 정정 권고. C-RT 조기기각·C-DUAL 근거약함 자평 모두 사실로 확정됨.

## 감사 방법

원문 직접 Read: PDF p.14-21(§5 ProxFusion 동작), p.22-30(§6-8 부가기능·Reference UI·I2C), p.35-37(§9 Memory Map), p.49-64(부록 A.2·A.5·A.9·A.12·A.15·A.18 등). 코드 Read: `tdc_touch_iqs323.c`(라인번호·write 값 grep 재확인, 수정 없음).

---

## 5항목 개별 대조

### (1) 0x15/0x16이 disable 채널(0x40=0x0000)에서도 유효 counts 반환하는가

| 확인 사실 | 원문 대조 |
|---|---|
| 주소·타입 | 0x15=Channel 1 Filtered Counts, 0x16=Channel 1 LTA, 둘 다 Read Only·16-bit | p.35 메모리맵 **원문 그대로 일치** |
| "Channel Mode 무관" 주장 | 참(그러나 별개 게이트) | Channel Mode는 Channel Setup(0x60/70/80) bit[3:0] 필드(A.15, p.56) — Enable Channel(Sensor Setup 0x40 bit0, A.5 p.51)과는 **완전히 다른 레지스터·다른 게이트**. "Channel Mode 무관"은 참이지만 "disable(Enable Channel=0)에서도 유효"를 입증하지 않음 |
| disable 시 값 유지 여부 | **미확인 — 원문에 직접 서술 없음** | §5.4(p.16)·§5.12.1(p.19) 전문 재확인. §5.12.1 "All channels **in use** must be enabled by setting the Enable Channel bit" — disable된 채널이 "in use"가 아니라는 정황은 있으나, Filtered Counts/LTA가 정지·0·최후값 중 무엇을 유지하는지 명시 문장 자체가 없음 |

**판정**: 레지스터 주소·타입 인용은 정확. 다만 "확립"이라 부를 근거는 **없음** — 미확인(실측 게이트)이 정확한 라벨. **발견(경중: 과장, 하드 할루시네이션 아님)**: C-DUAL 요약표 1행("가능 — read-only 레지스터(0x15/0x16)는 Channel Mode 무관 상시 read 가능 | **확립**(G2 §2.2)")이 C-DUAL 자신의 §6 WF2목록 1번("...disable 채널에서도 값이 갱신되는지 — **원출처 재대조**[필요]")과 내부 모순. 본문은 정직하게 미확인으로 남겼으나 요약표가 앞서 있음 — **"확립"→"미확인"으로 정정 권고**.

### (2) Sensor1 ATI Setup(0x46) reset이 정말 0x040C(Full)인가

원문(p.35, 메모리맵): `0x46 | ATI Setup | 0x040C | See Appendix A.12`. A.12(p.55-56) 비트분해 재계산: `0x040C = 0000 0100 0000 1100` → bit[15:4]=`0000 0100 0000`=64(ATI Resolution Factor) · bit3=1(ATI Band=Large 1/8) · bit[2:0]=`100`=Full(A.12 bullet "100: Full"). **양쪽 후보 인용과 완전 일치. 확인.**

### (3) 0x43 reset 0x01CF·비트해석·목표값(0xCF, 0x02)이 예약비트를 침범하는가

원문(p.35): `0x43 | Prox Input and Control | 0x01CF | A.9`. A.9(p.54) 비트분해: reset MSB=`0x01`(bit8 CRx0=1, bit9/10=CRx1/2=0), LSB=`0xCF`=`1100 1111`(bit7=1**예약·고정1**, bit6=1 Dead Time Enable, bit4=0 예약, bit3-2=`11`=Auto Prox Cycle 32, bit1-0=`11` **예약·고정11**).

C-DUAL 제안 목표값 MSB=`0x02`(bit9 CRx1=1, bit8 CRx0=0), LSB=`0xCF`(reset 그대로) → 합성 `0x02CF`. 예약비트 대조: bit15-14(예약=0) 불변, bit7(예약=1)·bit4(예약=0)·bit1-0(예약=11) **LSB 전체를 reset과 동일하게 두므로 침범 없음**. bit8/9만 CRx0→CRx1로 전환 — **예약비트 침범 없음, 확인**.

> [!NOTE]
> 원문 자체의 사소한 표기 특이점(할루시네이션 아님): A.9 bullet 목록이 bit6(Dead Time Enable) 다음 바로 bit4(Reserved, Set to 0)로 건너뛰어 **bit5을 별도 명명하지 않음**(표는 시각적으로 bit5-4를 Reserved 한 셀로 병합). 두 후보 모두 bit5를 건드리지 않고 reset값(0)을 그대로 두므로 이 표기 특이점은 안전판정에 영향 없음.

### (4) System Status의 ATI Error(bit6)가 정말 전역인가

원문(p.18, §5.11) **verbatim 재확인**: "The ATI Error bit in the System Status register is set if the following is true **for any channel** after the ATI has completed: Counts are outside the Re-ATI Boundary." — "for any channel" 문구 그대로 확인. 구조적 근거: A.2(p.49) 비트표에서 bit6은 "System Flags"(bit0-7) 소속이며, CH0/1/2별 Prox/Touch는 별도의 "Channel Flags"(bit8-13)에 있음 — **채널당 ATI Error 비트는 존재하지 않고 칩 전체에 1개뿐**. **확인 — 전역 맞음.**

### (5) Channel Setup(0x60/0x70) Channel Mode 비트필드·Follower Weight(0x63)

A.15(p.56) 원문: bit[15:8]=Follower Event Mask, bit[7:4]=Reference Sensor ID, bit[3:0]=Channel Mode(`00` Independent·`01` Follower·`10` Reference) — 예약비트 없음(4비트 전부 가용 필드). A.18(p.57): Follower Weight = Weight/4096. Table 7.1(p.27)·Table 7.2(p.28) 원문 재확인 결과 두 후보의 인용 테이블(CH0=Follower/RefID 0x01/Mask 0x00/Weight=bit값/4096, CH1=Reference/RefID 0x00/Mask 0x03/Weight=0x00)과 **완전 일치**, Follower Event Mask 설명문("The reference channel should not ATI if the follower is in a proximity or touch state... only needs to be set if the channel is setup as a reference channel")도 verbatim 일치. **확인.**

---

## 코드 교차검증 (부가, Read 전용·수정 없음)

| 인용 | 코드 실측 | 판정 |
|---|---|---|
| `iqs323.c:272` `write_register(REG_SENSOR1_SETUP, 0x00, 0x00)` (CH1 disable) | grep 확인, 라인·값 일치, 주석 "CH1 disable(단일채널)" | **확인** |
| 0x46/Channel Setup(0x60/70/80)/Follower 레지스터 코드 정의·사용 전무 | `#define REG_` 17개 전수 grep — 0x46·0x60·0x70·0x80·FOLLOWER 매치 0건 | **확인** |
| 0xC0 트리거 `0x54,0x07`(Re-ATI)/`0x58,0x07`(Reseed) | 라인 447·454 일치. 0x54=`0101 0100`→PowerMode101(Automatic No ULP)+bit2 Re-ATI=1; 0x58=bit3 Reseed=1; MSB 0x07=`0000 0111`→bit8-10(CH0-2 Timeout Disable) 전부=1 | **확인**(C-DUAL "CH0·CH1·CH2 전부 커버" 주장과 일치) |
| `events_enable()` 0xD3=`0x52,0x00` | 라인 300-303 일치. 0x52=`0101 0010`→bit6 ATI Error·bit4 ATI Event·bit1 Touch Event=1 | **확인** |

---

## 종합 표

| # | 항목 | 판정 | 하드 할루시네이션 |
|---|---|---|---|
| 1 | 0x15/16 disable 시 유효성 | 주소/타입 확인, "유효성" 자체는 미확인(원문 침묵) | 없음 — 단 C-DUAL 요약표 "확립" 라벨 과장 1건 |
| 2 | 0x46 reset=0x040C | 확인 | 없음 |
| 3 | 0x43 reset·목표값 예약비트 | 확인(침범 없음) | 없음 |
| 4 | ATI Error 전역성 | 확인 | 없음 |
| 5 | Channel Setup·Follower Weight | 확인 | 없음 |
| 부가 | 코드 라인·값 4건 | 확인 | 없음 |

**레지스터 주소·비트필드·reset값·원문 인용 전체에서 하드 할루시네이션 0건.** C-RT·C-DUAL 모두 데이터시트를 성실하게 원문 그대로 인용했다.

---

## AC 클럭/SPI 노이즈 실제 저감 근거 재확인 (본 감사 범위 내 재확인)

- **C-RT**: G1이 인용한 §7.3 원문("This subtraction is done when the primary sensing channel is in a touch or proximity state" — 상쇄 연산 정의 자체에 포함된 조건, "For example..."는 별도 문단으로 분리) **verbatim 재확인 완료**. "clock"·"switching"·"SPI"·"noise"·"AC" 등 어휘는 §5-8(p.14-30) 전체 재확인 범위에서 1건도 등장하지 않음. → **근거 없음, 조기 기각 확정.**
- **C-DUAL**: IC 문서화 기능이 아니므로 데이터시트가 이 기법 자체를 다루지 않음(확립도 반증도 아닌 침묵, 원문 p.14-30 전체 재확인 결과 동일 결론). 유일한 물리 가설(공유 VDD/VREG 공통노이즈)과 반증 가설(D패드 동심 co-touch, 순차변환 가능성) 모두 **동등하게 미검증** — 본 감사(레지스터 인용 대조)로는 어느 쪽도 강화·반증되지 않음. **"약함" 자평 그대로 확인.**

---

## 최종 결론

- **C-RT**: 기각 확정. 레지스터·원문 인용이 전부 정확하며, 그 정확한 인용 자체가 "AC 근거 없음 + idle 상태 게이팅 불일치"라는 원 기각 사유를 재확증한다.
- **C-DUAL**: "근거강도 약함" 판정 확정. 본 감사에서 새로 발견된 강화 근거도, 새로운 반증도 없음. 발견된 유일한 결함(요약표 "확립" 라벨 과장)은 오히려 판정을 한 단계 더 신중한 방향(미확인)으로 낮추는 정정이다.
- **살아날 여지(정직히 기록)**: C-DUAL의 §3 "Phase 1(계측 전용, veto 미적용)" — fake_func_sleep 재현 중 CH0·CH1 raw 상관관계와 co-touch 유무를 **실측**해 §2의 미확인 전제(동시성·공통결합·touch-blind 여부)가 긍정적으로 확인되는 경우에 한해 생존 가능. 이는 본 노드(레지스터 인용 감사)의 범위 밖이며 별도 실측 게이트가 필요하다.
- **잔존 불확실성**: (a) disable/enable 채널의 Filtered Counts·LTA 갱신 거동(데이터시트 미서술), (b) CH0·CH1 동시 vs 순차 변환 여부(§5-8 재확인 범위 내 명시 없음), (c) 0x43 bit11 Calibration Capacitor Select의 "0=enabled" 표기와 §5.12.3 "clear시 미사용" 서술 간 상충(로컬 레퍼런스 문서가 이미 정직하게 미해결로 표기, 본 감사 범위인 5항목과는 별개), (d) 실제 손가락 터치 시 CRX1(외곽 링)의 co-touch 반응 여부(전극 실측 필요).

---

## 근거

- `iqs323_datasheet.pdf` p.14-21(§5 ProxFusion 동작 전문), p.22-30(§6-8, Reference UI Table 7.1·7.2 원문), p.35-37(§9 Memory Map), p.49-64(부록 A.2·A.5·A.9·A.12·A.15·A.18) — 본 세션 직접 Read
- `05_C-DUAL_SW차동.md`, `04_C-RT_referencetracking.md` — 인용 대조 대상
- `06_레지스터레퍼런스.md`, `IQS323-레지스터-맵.md` — 로컬 레퍼런스 교차 대조(전부 원문과 일치 확인)
- `tdc_touch_iqs323.c` 17-34행(REG_ 정의 grep)·266-282행(sensor_setup)·300-303행(events_enable)·420-455행(ATI/Re-ATI/Reseed 트리거) — Read 전용, 수정 없음
- `01_G1_referencetracking실능력.md` — §7.3 원문 인용 출처 교차 재확인
