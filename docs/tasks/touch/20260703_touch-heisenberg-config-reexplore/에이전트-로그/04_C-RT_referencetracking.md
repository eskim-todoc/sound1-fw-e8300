---
name: 04-C-RT-referencetracking
purpose: CH1을 IC-native Reference 채널로 구성해 공통모드 드리프트를 상쇄하는 후보 — G1 판정(AC 근거 없음) 위에서 정직한 실효성 평가·레지스터 시퀀스·3불변식 보존 분석
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, reference-tracking, channel-mode, candidate, wf1-generate]
---

# 04 · C-RT — Reference Tracking(CH1) 후보

**TL;DR**: Channel Mode=Reference로 CH1을 참조채널화하는 후보. 레지스터·시퀀스는 SW-only로 확립되나, G1 판정대로 AC 클럭/SPI 노이즈 상쇄 근거는 전무하고 상쇄 자체가 primary touch/prox 상태에서만 작동해 idle 오인식 문제에 원리적으로 무관하다. 3불변식 보존은 가능하나 ati_error 발생표면 증가 리스크가 있어 원 목적 기준 조기 기각 권고.

## 결론 먼저 — 원 목적에 부적합(억지 채택 안 함)

G1이 원문(04_부가기능·UI.md §7.3, G1이 p.26-28 직접대조)으로 확정한 대로, Channel Mode=Reference(Reference UI)는 **DC 드리프트(온도·습도) 전용**이며 **AC 클럭/SPI 스위칭 노이즈 상쇄 근거는 전무**하다. 게다가 상쇄 연산이 primary가 touch/prox 상태일 때만 작동(정의 자체 게이팅)하므로, fake_func_sleep()의 오인식이 발생하는 바로 그 상태(idle/노터치)에서는 이 메커니즘이 원리적으로 관여하지 않는다. **본 후보를 하이젠베르크(경로_2, AC 클럭/SPI 결합) 회피책으로 채택할 근거는 없다** — 조기 기각 권고. 이하는 이 구성이 기술적으로 무엇을 하는지, SW-only로 어떻게 구현되는지, 3불변식과 어떻게 얽히는지를 정직하게 명세한다(WF2 대조용 + 별도 목적 참고용).

**범위 경계**: 본 문서는 IC 내장 Channel Mode=Reference(§7.3)만 다룬다. touch/prox 게이팅을 우회하는 FW 자체 raw Counts 차감은 C-DUAL 영역 — G1 §5 핸드오프 참조, 본 문서는 다루지 않음.

---

## (1) 작동 메커니즘 [확립]

- CH0=Follower(Channel Mode 01), CH1=Reference(Channel Mode 10) — Channel Setup(0x60/0x70) bits[3:0].
- 온도·습도 등 공통 드리프트가 CH0·CH1 LTA를 동시에 같은 방향으로 이동 → `CH0 LTA 보정량 = CH1 LTA 변화량 × (Follower Weight/4096)`을 CH0 LTA에서 차감.
- 연산 단위는 **LTA(IIR 저역통과)뿐** — raw Counts는 무관.
- Follower Event Mask(CH1의 0x70 bits[15:8]=0x03)가 "CH0 Prox/Touch 상태 동안 CH1 자신의 ATI를 차단"하는 인터록으로 동작.
- 근거: 04_부가기능·UI.md §7.3/§7.3.1/§7.3.2(Table 7.1·7.2), 06_레지스터레퍼런스.md A.15·A.18 — 본 세션 원문 직접 재확인, G1 §1·§3 선행 확립과 일치.

## (2) AC 클럭/스위칭 노이즈 실제 저감 여부 — 근거강도: **없음(전무)** [확립]

세 가지 독립 근거로 "근거 없음"이 확정적이다(G1 §1 인용 + 본 세션 원문 재확인):

1. **어휘 부재**: §7.3 전문에 "count drift"·"temperature"·"humidity"만 등장, "clock"·"switching"·"SPI"·"noise"·"AC" 부재(04_부가기능·UI.md §7.3, 본 세션 전문 재확인).
2. **구조적 저역통과**: 상쇄가 LTA(IIR) 레벨 연산 — Beta 4비트 상한으로 샘플당 최대 약 5.9% 반영(G1 §1 인용, 02_proxfusion동작.md §5.6). 단발 AC 충격은 구조적으로 강하게 감쇠.
3. **치명적 게이팅**: "This subtraction is done when the primary sensing channel is in a touch or proximity state"(G1 §1 인용 원문, p.26) — 예시가 아니라 **정의 자체에 포함된 조건**. fake_func_sleep() 오인식은 CH0가 **아직 non-touch인 상태에서 touch로 잘못 진입**하는 사건이므로, 상쇄가 작동하는 전제(이미 touch/prox) 자체가 성립하지 않는 시점에 문제가 발생한다 — **인과적으로 무관**.

**판정**: DC 드리프트 상쇄는 확립되나, 그것이 이 재탐색이 풀려는 문제(AC 클럭/SPI 순간 결합)와 **다른 문제**라는 것이 이 항목의 핵심 결론이다.

## (3) 필요 레지스터·시퀀스 [SW-only 확립, 세부 미확인 명시]

| 레지스터 | 주소 | 필드 | 제안값 | 근거 |
|---|---|---|---|---|
| Channel 0 Setup | 0x60 | Channel Mode[3:0] | 01(Follower) | A.15 |
| Channel 0 Setup | 0x60 | Reference Sensor ID[7:4] | 0x01(CH1 지목) | A.15/§7.3.2 |
| Channel 1 Setup | 0x70 | Channel Mode[3:0] | 10(Reference) | A.15 |
| Channel 1 Setup | 0x70 | Follower Event Mask[15:8] | 0x03(CH0 Prox+Touch) | A.15/§7.3.2 |
| Follower Weight | 0x63 | 16bit | 예: 4096(직접추적, 튜닝 필요) | A.18 |
| Sensor1 Setup | 0x40 | CTx1 등 enable | 현재 0x0000(disable, 코드 272행)→활성 라우팅 필요 | G2 §4.2, 코드 대조 |
| Sensor1 Prox Input/Control | 0x43 | CRx1(bit9) | 1(select) | G2 §4.2, A.9 |
| Sensor1 ATI Setup | 0x46 | 전체 | 설계 선택(아래 시퀀스 4번) | **A.12(신규 확인, 본 세션)** |

**신규 확인(본 세션, G3의 미확인 갭 해소)**: 06_레지스터레퍼런스.md A.12를 직접 대조한 결과 Sensor1 ATI Setup이 **0x46에 실재**하며 Sensor0(0x36)과 동일 구조·동일 기본값(0x040C)이다 — G3 §1·§6이 "CH1 전용 ATI 모드 레지스터 존재 여부 미확인(데이터시트 확인 필요)"로 남긴 갭이 해소됨. 단, 코드(`tdc_touch_iqs323.c` 17~34행)에는 `REG_SENSOR0_ATI_SETUP`(0x36)만 정의되어 있고 0x46 관련 정의는 전무(본 세션 grep 직접 확인) — 이 후보 채택 시 신규 매크로·write가 필요하다.

**제안 시퀀스**(미검증 스펙, 구현 안 함 — 코드 무수정):
1. `sensor_setup()`의 CH1 disable write(현재 272행 `0x00,0x00`)를 대체 — Sensor1 Setup(0x40)·Prox Input(0x43)으로 CRx1/CTx1 활성 라우팅.
2. Channel Setup 신규 write: 0x60(CH0=Follower)·0x70(CH1=Reference, Follower Event Mask=0x03).
3. Follower Weight(0x63) write.
4. CH1 ATI 정책 결정(자유 변수, 미확정): (a) CH1도 Full 적용 시 0x46=0x040C write + 부팅 Re-ATI가 CH1도 포함하는지 확인 필요, 또는 (b) CH1을 ATI Mode=Disabled(000)로 남겨 재보정 표면 자체를 없앰 — **§7.3 원문에 Reference 채널이 반드시 ATI 대상이어야 한다는 요구 문구는 없음(가설, 명시적 배제 문구도 없어 미확인)**.
5. 기존 CH0 Full ATI(0x36=0x040C)·부팅 Re-ATI 트리거(0xC0)·wait 시퀀스는 무변경(INV-CTRL-1 계약).

**미확인 목록(실측/추가확인 필요)**:
- CH1 reset 기본값이 물리 CRx0/CRx1 어느 쪽인지 — G2 §4.3 가설_A/B 상충, 미해결.
- CRX1·C52 반대편에 사용자 접촉 가능한 실제 전극 패드 존재 여부 — PCB 실측 필요(G1 §2, G2 §4.4).
- `System Control`(0xC0) bit2 Re-ATI가 전체 채널 재실행인지 실패 채널만인지 — G1 §4-2 미확인.
- CH0·CH1이 동시 변환인지 순차(time-multiplexed)인지 — G1 §5·G2 §2.2 공통 미확인.
- CH0 touch 해제 순간 Reference UI 상쇄가 어떻게 종료되는지 원문 서술 미확인 — G1 §4-3.

## (4) 3불변식 보존 [보존 가능, 조건부]

| 불변식 | 계약(G3 인용) | 이 후보의 영향 | 판정 |
|---|---|---|---|
| INV-CTRL-1(Full ATI) | 0x36 Full 유지+부팅마다 Re-ATI·대기(iqs323.c:437·447·451) | CH0 시퀀스 무변경 가능. CH1 Full 적용 여부는 자유 선택(§3-4) — 어느 쪽이든 CH0 계약 코드훅 자체는 안 건드림 | **보존** |
| INV-CTRL-2(ATI에러 Re-ATI) | 3사이트(노말/func_sleep/fake_func_sleep) 공통 `ati_error&&!ati_active`→Re-ATI 발행(G3 §2) | 전역비트 기반 발행 로직은 무수정 유지 가능(계약 안 깨짐). 단 CH1을 ATI 대상으로 켜면(§3-4-a) CH1 고유 실패(전극 미노출·기생용량 부정합)가 전역 ati_error를 세워 **발행 빈도 증가 개연성**(G1 §4-2 인용) | **계약 보존, 부작용 리스크 별도**(§5) |
| INV-CTRL-3(첫 터치 해제) | 무시게이트+디바운스 해제+RESEED 동반(G3 §3, 판정 입력=CH0 `pressed` bit9 단일) | 게이트·디바운스·RESEED 코드훅 무변경 — Reference UI는 CH0 LTA를 사후 보정할 뿐 판정 입력(threshold 비교)은 그대로 CH0 로직(G1 §4-3, 직접 충돌 근거 없음) | **보존**(해제 시점 상쇄 종료 거동은 미확인) |

## (5) 한계·리스크

1. **목적 부적합(치명적)**: 근본 목적(AC/하이젠베르크 회피)에 대한 근거가 없다(§2). 이 후보가 실제로 해결하는 문제는 "장기(수초~수분) 온습도 드리프트"뿐이며, 오케스트레이터가 정의한 재현 경로(fake_func_sleep, 순간 클럭/SPI 결합)의 문제가 아니다.
2. **게이팅 불일치**: idle 상태 오인식 방지에 원리적으로 무관(§2-3).
3. **HW 실측 의존**: CH1 전극 실재 여부(§3 미확인) 확인 전엔 "Reference 채널 물리조건 충족"을 단정할 수 없다 — 확인 안 되면 §1의 메커니즘 자체가 무효화(reference sensor가 사용자 손 근처면 "환경 전용" 전제가 깨짐).
4. **에러 표면 증가**: CH1을 ATI 대상으로 활성화하면 전역 ati_error 발생 빈도가 늘어날 개연성(G1 §4-2) — INV-CTRL-2 계약은 지켜지나 Re-ATI 발행·재부팅(func_sleep 5회 초과 시 SYS_WATCHDOG_RESET, G3 §2) 빈도가 오히려 늘어날 위험.
5. **레지스터 처녀지 = 검증 데이터 없음**: Channel Setup·Follower Weight는 코드가 전혀 안 써본 레지스터(G1 §3 인용, 본 세션 grep 재확인) — Follower Weight 제안값(4096)은 실측 근거가 아니라 데이터시트 옵션 나열에서 가져온 가설적 시작값일 뿐.
6. **순차측정 시 동시성 약화 가능성**: 설령 상쇄 효과가 있었더라도(§2에서 이미 근거 없음으로 판정됨), CH0·CH1이 순차 변환이면 그 사이 시간차만큼 공통모드 가정이 약화(가설, 미확인).

## (6) WF2가 사실 대조해야 할 핵심 주장 목록

| # | 주장 | 검증 상태(본 세션) |
|---|---|---|
| 1 | Reference UI 상쇄 대상은 count drift(온도·습도)뿐, 원문에 clock/switching/SPI/noise 언급 없음 | 04_부가기능·UI.md §7.3 전문 재확인(원본 PDF 페이지 대조는 G1, p.26) |
| 2 | 상쇄는 primary가 touch/prox 상태일 때만 작동(정의 포함 조건, 예시 아님) | 04_부가기능·UI.md §7.3 "This subtraction is done when..."(G1 p.26 원문 인용, 본 세션 문구 재확인) |
| 3 | Channel Setup(0x60/0x70/0x80) bits[3:0]=Channel Mode(00/01/10), bits[7:4]=Reference Sensor ID, bits[15:8]=Follower Event Mask | 06_레지스터레퍼런스.md A.15(본 세션 직접 재확인) |
| 4 | Follower Weight(0x63/73/83)=Weight/4096 | 06_레지스터레퍼런스.md A.18(본 세션 직접 재확인) |
| 5 | Sensor1 ATI Setup 레지스터가 0x46에 실재(Sensor0=0x36과 동일 구조·기본값 0x040C) | 06_레지스터레퍼런스.md A.12(본 세션 직접 재확인 — **신규**, G3는 미확인으로 남김) |
| 6 | 현재 코드에 Channel Setup·Follower Weight·0x46 관련 write·정의가 전무 | `tdc_touch_iqs323.c` 17~34행 grep(본 세션 직접 확인) |
| 7 | System Status(0x10) bit6 ati_error가 채널 구분 없는 전역 비트 | 06_레지스터레퍼런스.md A.2(본 세션 표 재확인 — "for any channel" 원문 문구 자체의 재대조는 G1 인용에 의존) |
| 8 | CRX1/C52가 Reference 채널의 물리조건(동일환경+비접촉)을 충족하는지 | **미확인** — PCB 실측 필요(G1 §2, G2 §4.4) |
| 9 | CH0·CH1 동시 vs 순차 변환 여부 | **미확인** — 데이터시트 리뷰범위(p.14-28) 내 명시 없음(G1 §5, G2 §2.2) |
| 10 | 0xC0 bit2 Re-ATI가 전체채널/실패채널 중 어느 범위로 재실행되는지 | **미확인**(G1 §4-2) |

## 근거

- `데이터시트/04_부가기능·UI.md` §7.3·7.3.1·7.3.2(Table 7.1·7.2) — 본 세션 전문 직접 Read
- `데이터시트/06_레지스터레퍼런스.md` A.2·A.9·A.12·A.15·A.18 — 본 세션 전문 직접 Read
- `tdc_touch_iqs323.c` 17~34행(REG_ 정의 grep)·265~282행(sensor_setup)·400~460행(apply_settings) — 본 세션 직접 Read, 수정 없음
- `01_G1_referencetracking실능력.md`(전체, make-or-break 판정 인용)
- `02_G2_SW측정채널능력.md` §1·§2·§4(채널모드·CH1 실채널화 경계 인용)
- `03_G3_3불변식코드훅.md` §1·§2·§3(불변식 코드훅 계약 인용)
