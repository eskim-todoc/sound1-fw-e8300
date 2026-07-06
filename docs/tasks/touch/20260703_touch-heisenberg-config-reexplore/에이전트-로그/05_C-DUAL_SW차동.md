---
name: 05-C-DUAL-SW차동
purpose: CH1을 노이즈 목격 채널로 self-cap 활성화해 FW가 CH0-CH1 raw counts/LTA를 매 폴링 차감·비교, 공통 유입 스파이크를 터치 판정에서 veto하는 SW 차동 후보 — IC reference tracking과 별개의 순수 FW 접근
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, sw-differential, ch1, dual-channel, veto, invariants, adversarial-verify]
---

# 05 · C-DUAL — SW 차동 (CH0-CH1 raw counts 차감/비교)

**TL;DR**: CH1을 self-cap(외부 CRX1 또는 내부 CalCap)으로 활성화해 CH0·CH1 raw Counts/LTA를 FW가 매 폴링 차감·비교, 공통 스파이크 감지 시 터치판정을 veto하는 SW 차동. IC 문서화 기능이 아니라 확립·반증 근거 전무 — 근거강도 약함. 전역 ati_error 표면적 증가와 co-touch(동일 버튼면 오반응)로 인한 진짜터치 오거부가 최대 리스크.

## 핵심 판정 요지

| 질문 | 판정 | 근거강도 |
|---|---|---|
| SW-only(HW 무수정)로 구현 가능한가 | **가능** — read-only 레지스터(0x15/0x16)는 Channel Mode 무관 상시 read 가능 | 확립(G2 §2.2) |
| AC 클럭/스위칭 노이즈를 실제 저감하는가 | **미확인 — 데이터시트에 이 기법 자체가 없음**(확립도 반증도 아닌 침묵) | **약함, 전부 미검증 물리 가설** |
| 3불변식과 충돌하는가 | 직접 충돌 근거 없음, 단 **전역 ati_error 표면적 증가**가 C-RT보다 더 노출됨 | 아래 §4 |
| C-RT(IC reference tracking) 대비 이점 | touch/prox 게이팅 없이 항상 작동 가능(§2), Follower Event Mask 불필요(§3) | 구조적 사실 |
| C-RT 대비 약점 | IC 내장 인터록(Follower Event Mask) 부재로 ATI 충돌 방지막 없음 | 아래 §4-2 |

---

## 0. 두 변형 — CH1이 무엇을 "목격"해야 하는가 (핵심 갈림)

CH1의 물리적 실체에 따라 전혀 다른 것을 상쇄한다. 오케스트레이터 질문("CH1이 무엇을 감지해야 상쇄가 성립하는지")의 핵심이 여기다.

| 변형 | CH1 실체 | 잡을 수 있는 것(가설) | 못 잡는 것 | 근거 |
|---|---|---|---|---|
| **a. 외부 CRX1**(추천, 아래 §1은 이 변형 기준) | CRX1(A1)→C52(100nF)→R32(0Ω)→J4, self-cap 실채널화(현재 disable을 대체) | PCB 근접장 트레이스 결합(추정) + IC 공용 VDD/VREG 리플(추정) | CalCap 대비 회로 불확실성 큼 | 회로 구성·분석.md §4·§7.2(CRX0/CH1 회로정수), G2 §4.2(핀 라우팅 SW 자유) |
| **b. 내부 CalCap** | IC 내장 알려진 고정 커패시터(외부 핀 완전 분리) | IC 공용 VDD/VREG 리플·내부 엔진 공통노이즈만(추정) | 외부 PCB 트레이스 근접결합(원리상 절연) | 데이터시트/02 §5.12.3(CalCap이 "외부 전극 대신" 측정경로 대체) |

**변형 a의 치명적 리스크(신규 도출)**: P4(전극패드물리설계, 교차 참조)는 D패드가 **중앙 채움(CRX0)+외곽 링(CRX1)의 동심 구조**이며 "**둘 다 하나의 물리 버튼 표면 아래**"에 있다고 관찰했다([`04_P4_전극패드물리설계.md`](../../20260703_touch-mutual-cap-review/에이전트-로그/04_P4_전극패드물리설계.md) §2-§3). 만약 실제 사용자 손가락이 중앙(CRX0)을 누를 때 인접 외곽 링(CRX1)에도 프린징으로 반응이 실린다면, **진짜 터치도 CH1을 같이 움직여 veto 로직이 이를 "공통노이즈"로 오판, 정상 터치를 거부**할 수 있다 — 이는 G1이 IC reference tracking에 요구한 "참조 채널은 사용자가 접촉할 수 없어야 함"(p.26)과 동일한 요구가 SW 차동에도 그대로 적용됨을 뜻하며, 현재 P4 관찰(동심·단일표면)은 이 요구가 충족되는지 **회의적**이다(미확인 — 실측 필요, 최우선 게이트).

변형 b(CalCap)는 이 co-touch 리스크가 원천 차단되나(외부 핀과 완전 분리), 대신 "PCB 근접장 결합을 잡는다"는 가설 자체가 성립하지 않을 수 있다(내부 고정 커패시터는 외부 전극 트레이스가 겪는 근접 결합에 노출되지 않음).

---

## 1. 작동 메커니즘 (변형 a 기준)

1. CH1을 self-cap Independent 채널로 활성화(Channel Mode 불변경, Reference/Follower 미사용 — C-RT와 결정적 차이).
2. 매 폴링 CH0(0x13/0x14)·CH1(0x15/0x16) raw Counts/LTA를 함께 read.
3. FW 연산(제안, k=1 초기값):
   ```
   raw_delta0 = LTA0 - Counts0
   raw_delta1 = LTA1 - Counts1
   corrected  = raw_delta0 - k × raw_delta1
   ```
4. **veto 방식(권고, 최소침습)**: IC 자체 Threshold(0x62)·bit9 판정은 그대로 두고(불변), `corrected`가 SW 확인임계 미만이면 그 샘플의 `pressed`를 보류/하향 — IC 판정을 대체하지 않고 "공통유입 의심 시 추가 확인 요구"만 얹는다.
   - 대안(비권고, 고위험): IC 판정 완전 대체, FW가 `corrected`만으로 처음부터 threshold 비교 — 새 판정 로직 전체를 코드가 떠안아야 해 검증 부담이 큼. 본 노드는 veto 방식을 주 설계로 제안.
5. **삽입 지점(코드 Read로 확인, 수정 없음)**: `tdc_touch.c:304-309`가 `tdc_touch_iqs323_read_status()`의 `st.pressed`를 그대로 `in.pressed`에 대입하는 조립부 — veto 연산은 이 지점(또는 `tdc_touch_iqs323_read_status()` 내부)에 넣을 수 있다. **아키텍처 긴장**: `tdc_touch_iqs323.h` 자체 주석(9행)이 "판정은 0건, FSM 소관"을 L1 레이어의 설계 원칙으로 명시하는데, CH0-CH1 비교는 최초로 L1(또는 조립부)에 "판정"을 들여오는 것 — 이 원칙과 충돌하므로 `tdc_touch.c`(조립 레이어, 이미 두 레이어를 잇는 위치)에 두는 편이 `iqs323.c`(순수 read)·`logic.c`(순수 FSM) 양쪽의 기존 순수성 계약을 덜 깨뜨린다(본 노드 권고).
6. **실패 안전(신규 설계 요구, 미구현)**: CH1 read 실패 시 veto 미적용(raw IC pressed 그대로 통과) — "부가기능 실패는 FSM 무영향"이라는 기존 `read_debug()` 설계 철학(iqs323.h 90-102행 주석)을 계승하되, veto가 load-bearing이 되므로 이 폴백을 **명시적으로 설계**해야 한다(현재는 디버그 전용이라 실패해도 무해했음 — 판정에 관여하는 순간 실패 처리 정책이 새로 필요).

---

## 2. AC 클럭/스위칭 노이즈 실제 저감 여부 — 근거강도 평가 (정직 판정)

**C-RT와의 결정적 차이**: G1은 IC의 Reference UI가 "count drift"(DC)만 명시하고 AC는 **데이터시트가 명시적으로 언급조차 안 함**을 확인해 "근거 없음"으로 조기 기각했다([`01_G1_referencetracking실능력.md`](01_G1_referencetracking실능력.md) §1). C-DUAL은 애초에 IC 문서화 기능이 아니라 **본 프로젝트가 고안하는 커스텀 FW 기법**이므로, 데이터시트가 이를 확인도 반증도 하지 않는다 — "침묵"이다. 즉 실패 양상이 다르다: C-RT는 "문서가 안 된다고 못박음"으로 죽고, C-DUAL은 "아무도 측정한 적 없는 가설 더미" 위에 서 있다.

**성립에 필요한 미확인 물리 전제 (전부 실측 없이는 확정 불가)**:

| 전제 | 지지 방향(가설) | 반증 방향(가설) | 출처 |
|---|---|---|---|
| CH0·CH1이 같은 시각(또는 통계적으로 동등)에 애그레서에 노출되는가 | fake_func_sleep 애그레서(SYSCLK·QCC·BLE)가 **연속/상시**라 순차측정이어도 통계적 노출은 유사할 수 있음(본 노드 추론) | IQS323이 "단일 ProxFusion 모듈"(p.15)이라 채널 간 변환이 순차(time-multiplexed)일 가능성 — **원문에 순차/동시 명시 문장 없음(미확인)** | G1 §5 핸드오프, G2 §2.2·§6-4 |
| CH0·CH1의 결합 경로가 공통(common-mode)인가 | 같은 IC의 공용 VDD/VREG(1.8V_QCC, R28 0Ω 직결, 전용 LDO 없음)를 두 채널이 공유 — 전원단 리플이라면 채널 무관 공통 개연성(본 노드 추론) | CRX0=중앙/CRX1=외곽 링의 **비대칭 배치**가 국소 fF 트레이스 결합(I2C·SYSCLK)엔 "상대적 차동성"을 시사(P2 H4, 타 작업 교차 참조) — mutual 맥락 가설이나 자기용량 전극 기하 문제라 self-cap 2채널에도 동일 적용 가능 | [`02_P2_노이즈EMC하이젠베르크.md`](../../20260703_touch-mutual-cap-review/에이전트-로그/02_P2_노이즈EMC하이젠베르크.md) H4, 회로 구성·분석.md §2 |
| CH1이 실제 터치엔 반응하지 않는가(touch-blind) | — | D패드 동심 구조·단일 버튼 표면(P4) → 진짜 터치가 CH1도 같이 흔들 가능성(§0 참조, 최대 리스크) | P4 §2-§3 |

**결론**: 지지 근거·반증 근거 모두 **가설 수준**이며 어느 쪽도 실측 없이 확정할 수 없다. C-RT가 "데이터시트가 명시적으로 안 된다고 한다"는 강한 부정 근거를 가진 것과 달리, C-DUAL은 "아무도 모른다"는 약한(및 위험한) 기반 위에 있다 — **근거강도: 약함**. 은수님께 정직하게 보고할 결론은 "물리적으로 그럴듯한 이유는 있으나(전원단 공유), 반증 가설(전극 비대칭·co-touch)도 동등하게 그럴듯해 실측 전엔 우열을 가릴 수 없다"이다.

---

## 3. 필요 레지스터·시퀀스 (변형 a 기준, 미확인 명시)

| 레지스터 | 제안 값 | 목적 | 확인 상태 |
|---|---|---|---|
| `0x40`(Sensor1 Setup) | LSB=0x01(Enable), MSB=0x02(CTx1) | CH1 self-cap enable, Tx를 물리 CTx1로. 현재 disable(0x00,0x00, iqs323.c:272)을 대체 | 본 노드 유도(A.5 대조, 미검증 신규) |
| `0x43`(Sensor1 Prox Input, **현재 코드에 상수 없음**) | LSB=0xCF(reset 유지), MSB=0x02(CRx1=1, CRx0=0) | Rx를 물리 CRx1로 — **reset 기본값은 CRx0을 향함(bit8), 명시 write 없으면 CH0과 같은 핀을 봄** | 본 노드 유도(A.9 직접 대조), G2 §4.3 가설_A와 정합. WF2 재검증 권고 |
| `0x46`(Sensor1 ATI Setup) | 0x0C,0x04(=reset 기본값과 동일, 명시화 권고) | CH1도 Full ATI 명시 — **reset default가 이미 0x040C=Full**(0x36과 동일 패턴)이라 실은 무write로도 이미 충족(A.12 직접 확인) | 확인(reset default 대조) — 명시 write는 안전상 권고일 뿐 필수 아님 |
| `0x60`/`0x70`(Channel Setup) | **미변경**(reset 0x0000=Independent 유지) | IC 내장 Follower/Reference 완전 미사용 — **C-RT 대비 구조적 단순함**(레지스터 4~5개 불필요) | 확인 |
| `0x72`(CH1 Touch Settings, 신규 고려) | 임계 0이 아닌 값 권고(정확한 수치는 실측 게이트) | CH1 자신의 IC 판정 threshold가 reset 0(=사실상 상시 트립)이면 `Touch Event`(0xD3 bit1, 현재 events_enable 0x52로 활성)가 남발돼 RDY 이벤트 폭주 우려 — **본 노드 신규 도출** | 미확인(본 노드가 register 값 대조로 도출한 부작용 가설), 실측 필요 |
| `0x15`/`0x16`(CH1 Counts/LTA) | 매 폴링 read | FW 차동 연산 입력 | 확인 가능(read-only, Channel Mode 무관, G2 §2.2). 단 `i2c_read()`(iqs323.c:89) 로컬 버퍼가 **4바이트 상한**(주석: "0x13+0x14 연속 디버그 read 지원") — 0x13~0x16 4레지스터(8바이트) 통째 read는 버퍼 확장이 필요하거나, 별도 윈도우 2회로 분리(코드 변경 필요 — 본 노드는 Read 전용이라 미실행, 사실만 기록) |
| `0xC0`(Re-ATI/Reseed 트리거) | 기존 그대로(`0x54,0x07` / `0x58,0x07`) | 트리거 호출 자체는 불변 — CH_Timeout Disable(MSB=0x07=bit8·9·10 전부)이 **이미 CH0·CH1·CH2 전부를 커버**하고 있어(A.30 직접 확인) CH1 활성화에 추가 조치 불필요 | 확인(기존 write가 이미 3채널 커버) — 단 Re-ATI/Reseed가 "전역(모든 활성 채널)"으로 실행되는지는 미확인(G1 §4-2 인용 계승) |

**권고 도입 순서(실측 게이트, 프로젝트 관행 `[실측 게이트]` 표기법 계승)**:
1. **Phase 1(계측 전용)**: CH1을 켜고 위 레지스터만 적용, `corrected` 값을 디버그 로그로만 출력(veto 미적용, FSM 무영향) — fake_func_sleep 재현 중 CH0·CH1 상관관계를 실측해 §2 표의 미확인 전제를 검증.
2. **Phase 2(veto 반영)**: Phase 1이 긍정적일 때만 §1의 veto 로직을 조립부에 반영.

---

## 4. 3불변식 보존 검토

| 불변식 | 보존 여부 | 근거 |
|---|---|---|
| **INV-CTRL-1**(Full ATI) | 보존 — 0x36 불변, 0x46은 reset 기본값이 이미 Full(위 §3) | A.12 직접 대조 |
| **INV-CTRL-2**(ATI 에러 Re-ATI) | **구조적으로 보존되나 발생 표면적 증가** — ati_error는 "어느 채널이든"(02_proxfusion동작.md §5.11 원문 "어느 채널이든 ATI 완료 시점에... True이면... set") 전역 1비트. CH1을 활성 채널로 늘리면 CH1 자신의 ATI 수렴 실패도 같은 비트를 세워 기존 3사이트(노말/func_sleep/fake_func_sleep) Re-ATI 로직이 그대로 발동됨(로직 자체는 안 바뀜, 단 발동 빈도가 오를 수 있음) | 02_proxfusion동작.md §5.11(본 노드 직접 재확인), G1 §4-2 |
| **INV-CTRL-3**(첫 터치 해제) | 보존 — G3가 명시("판정 입력 신호원 자체는 자유... 이 3박자 구조만 유지하면 허용"), veto는 `pressed` 단일 bool을 조립부에서 가공할 뿐 게이트/디바운스/RESEED 3박자 구조 자체는 무변경 | G3 §3·§5 |

### 4-1. C-RT 대비 명확히 더 나쁜 점 — 인터록 부재

C-RT(Reference 채널모드)는 `Follower Event Mask`가 "팔로워가 touch/prox 상태인 동안 reference 자신의 (재)ATI를 차단"하는 IC 내장 인터록을 가진다([`01_G1`](01_G1_referencetracking실능력.md) §4-1). C-DUAL은 Channel Mode를 Independent로 유지하므로(§3) **이 인터록이 아예 존재하지 않는다** — CH1이 CH0의 터치 지속 중에도 자기 사정으로 ATI Band를 벗어나면 그대로 전역 `ati_error`가 서고, 3사이트 Re-ATI가 CH0 터치 도중에도 발동될 수 있다(C-RT보다 노출이 큼). 이는 CH1을 활성화하는 모든 SW 차동류 후보의 공통 약점이나, C-DUAL은 이를 회피할 인터록 자체를 쓰지 않기로 설계된 후보라는 점에서 이 리스크를 **그대로 인수**한다.

---

## 5. 한계·리스크 (우선순위 순)

1. **[최대] co-touch 오거부**(변형 a 한정) — §0에서 도출. D패드 동심·단일표면 구조상 진짜 손가락 터치가 CH1도 함께 흔들면 veto가 정상 터치를 노이즈로 오판, 억제한다. IC reference tracking이 요구하는 "참조는 접촉 불가"(p.26) 요건이 SW 차동에도 동일하게 필요하나 현재 전극 형상은 이를 충족한다는 확증이 없다(미확인, 최우선 실측 게이트).
2. **전역 ati_error 표면적 증가**(§4-1) — CH1 도입 자체가 C-RT보다 인터록 없이 더 크게 노출.
3. **동시측정 여부 미확인**(§2 표) — 순차 스캔이면 순간 결합량의 "동일성" 전제가 흔들림. G1·G2 모두 데이터시트 리뷰 범위 내 확인 못 함.
4. **CH1 전극 결합 경로 자체가 미확인** — 변형 a·b 중 무엇이 맞는 가설인지, 심지어 reset 기본값이 물리적으로 어느 핀을 향하는지조차 G2 §4.3에서 상충하는 두 근거가 병존(가설_A/B).
5. **`read_debug()` 재분류 리스크** — 기존에 "실패해도 FSM 무영향"으로 설계된 계측 경로가 veto의 입력이 되면 load-bearing으로 격상되며, 실패 시 폴백 정책을 새로 설계해야 함(§1-6). 누락 시 CH1 read 실패가 터치 판정 자체를 불안정하게 만들 수 있음.
6. **I2C 트래픽 증가(부차적)** — CH1 read 추가로 폴링당 I2C 윈도우가 늘어남. 단 H1(I2C 5배속)은 이미 이 경로에서 반증됐고(prescale 240 유지) 여기서 늘어나는 것은 **속도가 아니라 거래 횟수**라 성격이 다르나, 오케스트레이터가 우려하는 "클럭/SPI 스위칭 결합"과는 별개 채널(터치 IC 자체 I2C 버스)이라 상대적으로 경미한 부차 비용으로 판단(가설).
7. **k(가중치) 미보정** — §1의 `corrected = raw_delta0 - k×raw_delta1`에서 k=1은 임의 시작값. 두 채널의 결합 강도가 다르면(변형 a·b 모두 가능) 과보정/과소보정이 발생 — 실측 캘리브레이션 게이트 필수(§3 Phase 1).
8. **CH1 자체 이벤트 폭주 부작용**(§3 표 0x72 항목) — threshold 미설정 시 CH1의 잦은 자체 touch/prox 토글이 이벤트 트래픽을 늘릴 수 있음(본 노드 도출, 미확인).

---

## 6. WF2가 사실 대조해야 할 핵심 주장 목록

1. `Channel 1 Filtered Counts`(0x15)·`Channel 1 LTA`(0x16)가 Channel Mode 설정과 무관하게 항상 유효한 read 값을 반환하는가(단순 "레지스터가 존재" 이상, disable 채널에서도 값이 갱신되는지) — G2 §2.2/본 노드 §3 인용의 원출처 재대조.
2. `ATI Setup`(0x46)의 reset 기본값이 정말 `0x040C`(Full)인지 — 본 노드가 06_레지스터레퍼런스.md A.12에서 직접 읽었으나 원본 PDF 표와 재대조 필요.
3. `Prox Input and Control`(0x43)의 reset 기본값 `0x01CF`이 bit8(CRx0)=1을 의미하는지, 본 노드가 §3에서 유도한 "0xCF,0x02" 목표값의 비트 조합이 예약비트를 침범하지 않는지 — 신규 유도값이라 최우선 검증.
4. `System Status`의 `ATI Error`(bit6)가 정말 채널 구분 없는 전역 비트인지 — 본 노드가 02_proxfusion동작.md §5.11에서 재확인한 인용문 자체의 정확성.
5. `System Control`의 Re-ATI(bit2)·Reseed(bit3)가 트리거 시 **모든 활성 채널**에 적용되는지, 실패한/특정 채널만인지 — G1이 이미 미확인으로 남긴 항목, 본 후보의 §3·§4 다수 결론이 이 가정에 의존하는 최우선 검증 대상.
6. CH0·CH1의 conversion이 동시인지 순차(time-multiplexed)인지 — G1·G2 공통 미확인, 본 후보의 성패를 좌우하는 물리적 핵심.
7. D패드(중앙 CRX0/외곽 CRX1) 동심 구조에서 실제 손가락 터치가 CH1(외곽 링)에도 유의미한 반응을 일으키는지(co-touch 여부, §0·§5-1의 최대 리스크) — 전극 실측 또는 제품 실기 테스트로만 확정 가능.
8. `Follower Event Mask`가 Channel Mode=Independent 조합(C-DUAL 채택)에는 전혀 관여하지 않는다는 것(A.15 "Reference channel일 때만 필요") — 문면 재확인.

---

## 근거

- `데이터시트/06_레지스터레퍼런스.md` §9(Sensor/Channel 주소표), A.5(Sensor Setup)·A.9(Prox Input and Control)·A.12(ATI Setup)·A.15(Channel Setup)·A.30(System Control) — 본 세션에서 직접 대조
- `데이터시트/02_proxfusion동작.md` §5.11(ATI Error 전역 판정 원문)·§5.12.3(CalCap) — 본 세션에서 직접 대조
- `01_G1_referencetracking실능력.md`(§1·§4, C-RT와의 대비 기준), `02_G2_SW측정채널능력.md`(§2.2·§4, read-only 레지스터·핀 라우팅 자유도), `03_G3_3불변식코드훅.md`(§3·§5, INV-CTRL-3 자유변수 확인)
- `20260703_touch-mutual-cap-review/에이전트-로그/02_P2_노이즈EMC하이젠베르크.md`(H4, 교차 참조 — 전극 비대칭 가설), `04_P4_전극패드물리설계.md`(§2-§3, D패드 동심·단일표면 관찰, 교차 참조)
- `참고/touch/개선/회로 구성·분석.md` §2·§4·§7.2(CRX1/C52/J4 회로정수)
- `tdc_touch_iqs323.h`(9행 L1 설계원칙 주석, 90-102행 read_debug 주석) · `tdc_touch_iqs323.c`(266-282행 sensor_setup, 400-460행 apply_settings, 447행 Re-ATI 트리거, 89-123행 i2c_read 4바이트 상한) · `tdc_touch.c`(304-309행 조립부) — Read 전용, 수정 없음
