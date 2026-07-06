---
name: 01-G1-referencetracking-실능력
purpose: IQS323 Reference UI(reference tracking)가 DC 드리프트만 상쇄하는지 AC 클럭/SPI 노이즈도 상쇄하는지 데이터시트 원문(페이지 확인)으로 판정 — 재탐색 make-or-break
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, reference-tracking, reference-ui, heisenberg, ati, verification]
---

# 01 · G1 — reference tracking 실능력 (make-or-break 판정)

**TL;DR**: IQS323 데이터시트 원문(p.15·26·27 직접 대조) 확인 결과, reference tracking(Reference UI)은 **온도·습도로 인한 DC count drift만** 상쇄 대상으로 명시되며 AC 클럭/SPI 스위칭 노이즈 언급은 §5.1·§7.3 어디에도 없다(**근거 없음**). 더구나 이 상쇄는 **primary 채널이 이미 touch/prox 상태일 때만** 작동하도록 원문에 명시 게이팅되어(p.26), 노터치 상태에서의 오인식(false touch 진입) 방지에는 애초에 관여하지 않는다. SW-only 레지스터 설정은 가능하나(확인), 하이젠베르크(경로_2) 회피의 원리적 근거는 없다.

## 핵심 판정 (결론 선행)

| 질문 | 판정 | 근거 |
|---|---|---|
| (a) DC 드리프트 상쇄하는가 | **확립 — 그렇다** | p.26 §7.3 원문, p.15 §5.1 |
| (a) AC 클럭/SPI 스위칭 노이즈도 상쇄하는가 | **근거 없음** (원문에 언급 자체가 부재) | §5.1·§7.3 전문 대조, 아래 §1 |
| (a-보너스) 노터치 상태의 오인식 방지에 관여하는가 | **아니다 — 확립** (touch/prox 상태 게이팅) | p.26 §7.3 원문 |
| (b) 참조 채널은 물리 전극이 필요한가 | **확립 — 그렇다** | p.26 §7.3 원문 |
| (c) SW-only(레지스터만)로 설정 가능한가 | **확립 — 그렇다** | A.15/A.18, 현 코드 대조 |
| (d) Full ATI·ATI에러·첫터치해제와 충돌하는가 | **직접 충돌 근거 없음, 단 리스크 1건 확인** | 아래 §4 |

---

## 1. (a) 무엇을 상쇄하는가 — DC 전용, AC 근거 없음

**원문 직접 대조 (p.26, §7.3 Reference UI)**:
> "A reference channel adjusts the LTA of the primary sensing channel by subtracting the change in LTA of the reference channel from the LTA of the primary sensing channel. **This subtraction is done when the primary sensing channel is in a touch or proximity state.** The Reference UI eliminates the effect of **count drift** on the measurement."
>
> "For example, in wear detect applications the dielectric parameters of the PCB and sensor elements are likely to change over time... the **drift in counts due to temperature and/or humidity** is accounted for..."

동일 취지가 §5.1(p.15) 채널 옵션 설명(데이터시트/02 §5.1 요약)에도 "동일한 환경(온도·습도)에 노출된 위치에 기준 채널을 배치… 공통 드리프트를 제거"로 반복된다. **두 절 전문을 대조했으나 "clock", "switching", "SPI", "noise", "AC", "고속/순간" 등 빠른 교란을 지시하는 단어는 전혀 등장하지 않는다.** 명시된 상쇄 대상은 오직 "count drift"·"temperature and/or humidity"뿐이다.

**구조적으로도 AC를 못 잡는 이유 (확립, 레지스터 근거)**:
1. 상쇄 연산이 **Counts(raw)가 아니라 LTA 단위**에서 일어난다(§7.3 "subtracting the change in **LTA**"). LTA는 "slowly updated"(p.16 §5.5)로 명시된 IIR 필터 값이며, Beta 필드가 4비트(0~15)라 damping factor(=Beta/256) 최대치가 **약 5.9%/샘플**로 상한이 걸려 있다(`데이터시트/02_proxfusion동작.md` §5.6). 단발성 AC 충격이 raw Counts에 실려도 LTA에 반영되는 비중은 그 샘플 하나당 최대 5.9%뿐이라 구조적으로 강하게 감쇠된다.
2. §5.5(p.16) 원문: "During a touch or proximity event, the LTA is frozen." — Reference UI가 정확히 **이 동결 구간**을 메우려고 설계된 기능이라는 것이 (a)의 두 번째 확립 사실로 이어진다(아래).

**AC 노이즈 상쇄 가설에 대한 판정**: 데이터시트에 "reference tracking이 클럭/스위칭 노이즈를 상쇄한다"는 문장은 **없다**. 상호용량 검토 노드([`06_P6_대안트레이드오프결정.md`](../../20260703_touch-mutual-cap-review/에이전트-로그/06_P6_대안트레이드오프결정.md#L51))가 지적한 "준-DC에서만 유리, 빠른 스위칭엔 근거 약함" 논리와 방향이 일치하며, 본 노드는 reference tracking에 대해 이를 **원문 페이지 대조로 확정**한다.

### 1-1. 보너스 확립 사실 — touch/prox 게이팅 (원래 질문보다 중요할 수 있음)

§7.3 원문("This subtraction **is done when** the primary sensing channel is in a touch or proximity state")은 예시가 아니라 **상쇄 연산의 정의 자체에 포함된 조건**이다(바로 다음 문장이 "For example, in wear detect applications..."로 별도 시작 — 조건문과 사례가 문단으로 분리됨). 이는 §5.5의 "터치 중 LTA 동결"과 정확히 맞물린다 — Reference UI는 **"터치가 오래 지속되는 동안 자기 LTA가 얼릴 수밖에 없어 드리프트를 놓치는" 문제를 메우는 기능**이지, 범용 상시 노이즈제거 기능이 아니다.

**함의**: fake_func_sleep()의 오인식은 **노터치(idle) 상태에서 발생**한다(터치 안 했는데 터치로 오판). Reference UI의 상쇄는 primary가 **이미 touch/prox 상태일 때만** 작동하므로, 노터치 상태에서의 오인식 진입 자체를 막는 경로가 **원리적으로 존재하지 않는다**. DC냐 AC냐를 떠나, **게이팅 조건 하나만으로 이미 본 문제의 사용 사례에 안 맞는다** — 이것이 (a)의 가장 강한 반증 포인트다.

---

## 2. (b) 참조 채널이 물리적으로 무엇을 감지하는가

원문(p.26): "The reference channel **sensor** should be exposed to the same conditions as the sensing channel, and the user should not be able to affect the counts of the reference channel."

- 참조 채널은 **내부(internal) 신호가 아니라 실제 물리 전극을 가진 센싱 채널**이다 — 일반 채널과 동일하게 자신의 `Sensor Setup`(CTx/CRx 페어)을 갖는다(§5.12.1 방식 동일 적용, p.19).
- 요구되는 물리 조건은 두 가지뿐: ① primary와 **동일 환경**(온도·습도) 노출, ② **사용자 터치가 닿지 않는** 위치.
- "single reference channel은 multiple follower 가능, follower는 multiple reference 불가"(p.26) — n:1(참조 1개 : 팔로워 다수) 토폴로지만 지원.
- **미확인(실측 필요)**: Sound1의 CRX1·C52 배선(과거 CalCap 더미 용도, 오케스트레이터 컨텍스트)이 "primary와 동일 환경 + 터치 무관"이라는 두 물리 조건을 실제로 만족하는지는 **PCB 레이아웃 실측 영역**이며 본 노드(데이터시트 전담) 근거 밖이다. AZD125(레퍼런스 채널 설계 앱노트, p.26에서 지목)는 본 노드 근거 목록에 없어 **미열람 — 필요 시 별도 확인**.

---

## 3. (c) SW-only 설정법 (레지스터, HW 무수정)

**레지스터 경로 (전부 SW-only, A.15/A.18 확인 · `06_레지스터레퍼런스.md`)**:

| 레지스터 | 주소(CH0/CH1) | 필드 | 값 |
|---|---|---|---|
| Channel Setup | 0x60 / 0x70 | Channel Mode(bit3-0) | CH0=01(Follower), CH1=10(Reference) |
| Channel Setup | 0x60 / 0x70 | Reference Sensor ID(bit7-4) | CH0=0x01(CH1 지목), CH1=0x00(미사용) |
| Channel Setup | 0x60 / 0x70 | Follower Event Mask(bit15-8) | CH0=0x00(미사용), CH1=0x03(CH0 Prox+Touch bit) |
| Follower Weight | 0x63 | 16비트 | CH0만 사용, 값/4096 (4096=직접추적) |

원문(p.27, Table 7.1) Follower Event Mask 설명: "The reference channel should not ATI if the follower is in a proximity or touch state... The Follower Event Mask **only needs to be set if the channel is setup as a reference channel**." Table 7.2 예시(p.28)는 CH0=Follower/CH1=Reference 구성 그대로 확인.

**필수 선행 조건**(p.20 원문, §5.12.1 인접): "When **not** using the Reference UI, the Channel Mode... must be set to 'Independent'." — 즉 reset 기본값(0x0000)이 이미 Independent라 **현재 상태는 이 규칙을 만족**하고 있으며, 변경 시에만 위 표의 필드를 명시적으로 써야 한다.

**현재 코드 대조** (`tdc_touch_iqs323.c`, 코드 Read만·수정 없음):
- Line 272: `write_register(REG_SENSOR1_SETUP, 0x00, 0x00)` — CH1(Sensor1, 0x40)을 완전 disable(Enable Channel=0, CTx0=0). 오케스트레이터 컨텍스트와 일치.
- `REG_` 정의 전체 grep 결과, **Channel Setup 레지스터(0x60/0x70/0x80) 자체를 쓰는 코드가 전무** — 현재 CH0/CH1 모두 Channel Mode는 하드웨어 reset 기본값 0x0000(Independent)에 머물러 있다. 즉 reference tracking 관련 필드는 **완전히 손대지 않은 처녀지**이며, 도입 시 새 레지스터 write 4~5개 추가로 충분하다(SW-only, HW 무수정 조건 충족).
- CH1을 Reference로 쓰려면 최소 ① `REG_SENSOR1_SETUP`(0x40)을 CRX1 실채널로 enable(현재의 `0x00,0x00` disable을 대체), ② 0x70(Channel1 Setup)·0x60(Channel0 Setup)·0x63(Follower Weight) 신규 write 필요.

**결론**: (c)는 **확립 — SW-only로 가능**. 단, CH1 전극(CRX1/C52)이 (b)의 물리 조건을 만족하는지는 별개 확인 필요.

---

## 4. (d) Full ATI·ATI 에러(Re-ATI)·첫 터치 해제와 상호작용

### 4-1. Follower Event Mask ↔ Automatic Re-ATI (§5.10)

원문(p.19, §5.10): "A re-ATI is executed when the LTA of a channel drifts outside of the **ATI Band**... `Re-ATI Boundary = ATI Target ± ATI Band`." 이는 **채널별** 평가다(각 채널이 자신의 ATI Setup 0x36/0x46/0x56에서 독립적으로 Band를 가짐, A.12).

Follower Event Mask는 정확히 **"팔로워가 touch/prox 상태인 동안 reference 채널 자신의 (재)ATI를 막는" 인터록**이다(p.27 Table 7.1 원문 확정). 이는 INV-CTRL-1(Full ATI 사용)이 CH1(Reference)에도 적용될 경우, CH0가 눌려있는 동안 CH1이 엉뚱하게 재보정되는 것을 막아준다 — **상호작용은 있으나 충돌이 아니라 설계된 안전장치**다.

### 4-2. 전역 ati_error(§5.11) ↔ 채널 추가 리스크 (확립 사실, 유의 필요)

원문(p.19, §5.11): "The ATI Error bit... is set if the following is true **for any channel** after the ATI has completed: Counts are outside the Re-ATI Boundary." — **채널 구분 없는 전역 비트**임을 원문으로 재확인(레퍼런스/IQS323-레지스터-맵.md §2와 일치).

**리스크**: CH1(Reference)을 Full ATI로 활성화하면, CH1 자신의 ATI가 (예: 부적절한 전극 위치·기생 용량 범위로) Re-ATI Boundary를 벗어날 경우 **CH0와 무관하게** 전역 `ati_error`가 set되고, INV-CTRL-2(ATI 에러 처리)가 요구하는 Re-ATI 트리거가 발동한다. 즉 **활성 채널을 1개(CH0)에서 2개(CH0+CH1)로 늘리면 ATI 에러 발생 표면적이 늘어날 개연성**이 있다 — 이는 하이젠베르크 회피와 무관하게 CH1 도입 자체의 부작용으로 후보 평가 시 반영 필요.

**미확인**: `System Control`(0xC0) `Re-ATI` bit(A.30)을 세팅하면 **모든 활성 채널**의 ATI가 재실행되는지, 아니면 실패한 채널만 선택적으로 재실행되는지 — 원문 §5.10·§5.11·A.30 어디에도 "all channels" 또는 "the failing channel"이라는 명시적 범위 지정 문구가 없다. 레지스터 구조상(채널별 필드 없이 System Control 하나에 bit 1개) **전역 재실행일 개연성이 높으나 확정 문구는 없음(미확인 — 실측 또는 Azoteq 문의 필요)**.

### 4-3. 첫 터치 해제(INV-CTRL-3)와의 상호작용

Reference UI의 LTA 상쇄는 CH0가 touch/prox **상태인 동안만** 작동하고(§1-1), 상태를 벗어나는 순간(해제) 그 상쇄가 어떻게 종료되는지에 대한 별도 원문 서술은 확인되지 않았다(**미확인**). 다만 해제 판정 자체(§5.7 Threshold/Hysteresis 공식)는 Reference UI 유무와 무관하게 CH0 자신의 Counts·LTA 비교로 이루어지므로, **직접적 충돌 근거는 없음** — 단 CH1 도입이 CH0의 LTA 절대값에 어떤 부가 편향을 남기고 해제되는지는 실측 전 단정 불가.

---

## 5. 종합 및 후속 스코프 경계

| 항목 | 결론 |
|---|---|
| AC 클럭/SPI 노이즈 상쇄 | **근거 없음** — 조기 기각 사유로 충분 |
| DC 드리프트 상쇄 | 확립되나 **노터치 상태엔 미작동**(게이팅) → 본 문제(idle 오인식)에 애초 부적합 |
| SW-only 실현성 | 확립 — 가능(레지스터 4~5개, 코드 처녀지) |
| 3불변식과의 관계 | 직접 충돌 근거 없음, 단 전역 ati_error 리스크 표면적 증가 우려 |

**본 노드 범위 밖 — 핸드오프**: touch/prox 게이팅을 우회해 CH1의 raw Counts/LTA를 FW가 직접 읽어 **매 사이클 SW 차감**하는 방식(IC-native Reference UI가 아닌 커스텀 SW 상쇄)은 이 게이팅 제약을 받지 않을 수 있으나, 이는 **C-DUAL 후보(SW 차동)의 영역**이며 본 G1(IC-native reference tracking) 판정 대상이 아니다. 단, C-DUAL도 CH1이 애그레서 노이즈를 primary와 "같은 타이밍에 같은 크기로" 겪는지(동시성)는 IQS323이 단일 ProxFusion 모듈(p.15, "a single ProxFusion module")로 채널을 처리한다는 사실과 관련되나, **채널 간 변환이 순차(시분할)인지를 명시하는 원문 문장은 이번 검토 범위(p.14-28)에서 확인하지 못함(가설 — 구조적 추정, 미확인)**. 필요 시 C-DUAL 검증 단계에서 §8(I2C) 또는 타이밍 관련 절 재확인 권고.

---

## 근거

- `데이터시트/02_proxfusion동작.md` §5.1(reference tracking 요약)·§5.5(LTA)·§5.6(Beta) / 원문 p.15-16
- `데이터시트/04_부가기능·UI.md` §7.3(Reference UI)·Table 7.1·7.2 / 원문 p.26-28
- `데이터시트/06_레지스터레퍼런스.md` A.15(Channel Setup)·A.18(Follower Weight)·A.2(System Status, 전역 ati_error)·A.12(ATI Setup)
- `레퍼런스/IQS323-레지스터-맵.md` §2(ati_error 전역 확인)
- `iqs323_datasheet.pdf` p.14-21, 24-28 (§5 ProxFusion 전체, §6 Hardware, §7.1-7.5 직접 원문 대조)
- `tdc_touch_iqs323.c` line 272(REG_SENSOR1_SETUP disable), 전체 `#define REG_` grep(Channel Setup 미사용 확인) — Read 전용, 수정 없음
- `20260703_touch-mutual-cap-review/에이전트-로그/06_P6_대안트레이드오프결정.md` L51 (준-DC 전용 논리 교차 참조)
