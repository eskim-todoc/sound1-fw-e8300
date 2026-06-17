---
name: 방전autoATI-C1-datasheet분석
purpose: 데이터시트 메커니즘 검증자(C1) — auto-ATI/Re-ATI(02 §5.9~5.11)와 200ms CRX0 토글·측정공백·0x30 비트의 상호작용을 데이터시트 원문으로 검증 (Q1~Q4)
type: 개선
maturity: stable
tags: [touch, iqs323, auto-ati, re-ati, discharge, sensor-setup, ati-band, datasheet, persona-c1]
---

# C1 · 데이터시트 메커니즘 검증 — auto-ATI 중 CRX0 수동방전 공존 (Q1~Q4)

> **TL;DR**: 데이터시트(02 §5.9~5.11·5.4~5.5, 06 A.5·A.10·A.12)를 원문 인용해 4질문을 검증한 결과 **공존은 조건부불가**다. ① Q1 — auto-ATI/Re-ATI는 LTA가 ATI Band(±1/8 Target) 밖으로 drift하면 자동 발동(02 §5.10). 200ms마다 CH0 disable→enable 토글은 enable 직후 카운트 튐을 만들고, 이것이 LTA를 흔들면 Re-ATI 트리거 요인이 된다. 더 결정적으로 **ATI 진행 중 측정 채널을 disable하면 ATI 입력(counts)이 끊겨 수렴 자체가 교란**된다(02 §5.9 — ATI는 counts를 ATI Base/Target에 맞추는 알고리즘이므로 측정 중단은 직접 타격). ② Q2 — CH1만 Disabled + CH0만 Full은 데이터시트상 채널별 ATI Setup(0x36/0x46 별개, 06 A.12)이라 구성 가능하나, CH0 측정공백 동안 CH0 LTA는 동결이 아니라 **갱신 대상**(02 §5.5)이라 공백/복원 노이즈가 LTA·Re-ATI에 유입된다. ③ Q3 — enable 복원 시 카운트 튐 → LTA가 ATI Band 이탈 → **Re-ATI 재트리거 → 터치-방전-Re-ATI 진동** 위험 실재. ④ Q4 — `0x30 LSB 0x02`는 데이터시트 A.5 비트정의로는 **Linearise(bit1)=1 + Enable(bit0)=0**이며 Inactive Rxs(VSS)는 0x34 A.10 별개 레지스터다. 코드의 "CRX0 VSS" 해석과 데이터시트가 불일치(실칩 미확정). 더해 disable 단계에서 **MSB(TX_SELECT)를 0x00으로 덮어써 CTx0 선택까지 클리어**한다. **결론: auto-ATI Full과 200ms 0x30 토글 공존은 데이터시트 메커니즘상 불가에 가깝고, 권장 대안은 Re-ATI 자동 억제(부팅 1회 ATI + 수동 Re-ATI) + 방전 유지.**

> [!IMPORTANT]
> 근거: 데이터시트 [02 §5.9~5.11](../../데이터시트/02_proxfusion동작.md)·[06 A.5/A.10/A.12](../../데이터시트/06_레지스터레퍼런스.md), 코드 `tdc_drv_iqs323.c`(`discharge_crx0` 938·`sensor_setup` 440·`is_auto_ati_done_single_read` 312·`apply_settings` 1023)·`.h`(INACTIVE_RXS 87~89·ATI Setup 50~53). 데이터시트·코드 라인 인용, 추정은 "확인 필요" 명시.

---

## 0. 검증 대상 정리

| Q | 질문 | 핵심 데이터시트 |
|---|---|---|
| Q1 | 200ms CRX0 enable 토글(0x30 0x02↔0x01)이 auto-ATI/Re-ATI 수렴을 교란? ATI 중 채널 disable 거동 | 02 §5.9·5.10·5.11 |
| Q2 | 방전 중 CH0 측정공백(CalCap CH1만 cycle 유지)이 CH0 LTA·판정·ATI에 영향? CH1만 Disabled + CH0만 Full 가능? | 02 §5.4·5.5, 06 A.12 |
| Q3 | 방전 후 enable 시 카운트 튐이 LTA를 흔들어 Re-ATI 재트리거? 진동 위험? | 02 §5.5·5.10 |
| Q4 | 0x30 LSB 토글(0x02↔0x01)이 auto-ATI 관련/다른 Sensor Setup 비트를 건드리나? | 06 A.5·A.10 |

---

## 1. Q1 — ATI 진행 중 CRX0 disable 토글이 수렴을 교란하는가

### 1.1 데이터시트 원문 (auto-ATI 메커니즘)

- **02 §5.9 (line 528~534)**: "ATI 트리거 시 먼저 divider·multiplier로 **counts를 ATI Base에 최대한 근접**시키고, 그 다음 Compensation Value·Divider로 **ATI Target에 근접**." 즉 ATI 알고리즘의 **입력은 실측 counts**다.
- **02 §5.9 (line 538)**: "ATI 알고리즘은 짧은 시간에 실행되어 사용자가 인지 못 함" — ATI는 단발 수렴 과정(연속 측정에 의존).
- **02 §5.10 (line 643~649)**: "re-ATI는 채널 LTA가 `ATI Band`(ATI Target 중심) 밖으로 drift할 때 실행", 06 A.12 — ATI Band=1(Large)=1/8×Target. Sound1 코드 ATI_SETUP MSB 0x04 → bit3 ATI Band=0(MSB 0x04=0b0000_0100, LSB 0x08 bit3=1 → **확인 필요**: bit3는 LSB의 bit3, ATI_SETUP_LSB 0x08=0b0000_1000 → bit3=1=Large 1/8). ATI Target = ATI Base × Res/16 (02 line 633: 100×64/16=400 가정 시 band=±50).

### 1.2 메커니즘 판정

ATI(또는 Re-ATI)는 **측정 채널의 연속 counts를 입력**으로 divider/multiplier/compensation을 수렴시키는 알고리즘이다(02 §5.9). 200ms 토글은 매 폴링마다:

1. `0x30 LSB=0x02, MSB=0x00` write → **CH0 Enable=0**(측정 중단) + (코드 의도상 CRX0 VSS)
2. `0x30 LSB=0x01, MSB=0x01` write → CH0 재 enable

ATI가 진행 중인 순간에 CH0를 disable하면:

- **ATI 입력 단절**: 측정 채널이 꺼지면 ATI가 수렴 목표로 삼을 counts가 끊긴다. 데이터시트는 "ATI 진행 중 측정 채널을 disable했을 때의 거동"을 **명시 규정하지 않는다**(02 §5.9~5.11에 해당 케이스 부재 — **확인 필요**). 명시 부재 자체가 위험 신호다: ATI는 채널이 활성·측정 중임을 전제로 설계됐다.
- **실증 근거(코드)**: 현재 펌웨어가 **운용 중 ATI를 Disabled로 고정**한 직접 이유가 "Full auto-ATI 발동 시 I²C 무응답 사례"(드라이버 주석 `tdc_drv_iqs323.c:645` "운용 중 stuck-touch → auto-reATI → ATI_ERROR → I2C 무응답 경로 차단"). ATI 발동과 통신/측정 교란이 이미 관측됐다.

> [!WARNING]
> **Q1 판정: 교란한다(조건부불가).** ATI/Re-ATI는 연속 counts 입력에 의존하는데(02 §5.9), 200ms 토글은 측정 채널을 주기적으로 끊는다. ATI 수렴 윈도우(02 §5.9 "짧은 시간")가 200ms 토글 사이에 정확히 끼는 보장이 없어, 토글이 ATI 진행 구간과 겹치면 수렴 입력이 오염된다. 데이터시트는 이 케이스를 규정하지 않아(명시 부재) 정상 거동을 보장할 수 없다.

---

## 2. Q2 — 측정공백이 CH0 LTA·판정·ATI에 미치는 영향 / 채널별 ATI 분리 가부

### 2.1 채널별 ATI Setup 분리 (CH1만 Disabled + CH0만 Full) — 데이터시트상 가능

- **06 A.12**: ATI Setup은 채널별 레지스터(CH0=0x36, CH1=0x46, CH2=0x56). ATI Mode bits[2:0]도 채널별. → **CH1=Disabled(000) + CH0=Full(100) 구성 자체는 데이터시트상 가능**.
- 코드도 이미 채널 분리 운용: `apply_settings`가 CH0 ATI Setup(0x36)과 CH1 ATI Setup(0x46, `sensor_setup` line 502 DUMMY_ATI_SETUP=0x08 Disabled)을 별도 write. CH1 Disabled는 "CalCap 부하 auto-ATI 수렴 실패 → 전역 ATI_ERROR 방지"(코드 주석 500·.h 111).

### 2.2 그러나 ATI Error는 전역 비트

- **02 §5.11 (line 662~664)**: "**어느 채널이든** ATI 완료 시점에 Counts가 Re-ATI Boundary 밖이면 System Status의 ATI Error bit set." ATI Error(0x10 bit6)는 채널별이 아니라 **전역 1비트**다(06 System Status). CH1을 Disabled로 두면 CH1은 ATI를 안 돌리므로 CH1발 ATI Error는 회피되나, **CH0를 Full로 돌리면 CH0 ATI 결과가 전역 ATI Error를 좌우**한다.

### 2.3 CH0 측정공백이 LTA에 미치는 영향

- **02 §5.5 (line 314)**: "LTA는 환경 변화 추적 위해 천천히 갱신되며 **touch·proximity 이벤트 중에는 frozen**." → 노터치 시 CH0 LTA는 **동결이 아니라 매 측정 IIR 갱신**(02 line 325: `LTA_new = LTA_old + (Counts−LTA_old)×Beta/256`).
- 방전으로 CH0가 disable되는 200ms 구간 동안 CH0는 측정이 없다. 재 enable 직후 첫 측정 counts는 방전(VSS 강제)·재충전 직후라 정상값에서 튈 수 있고, 이 튄 counts가 IIR로 CH0 LTA에 섞인다. CalCap CH1이 measurement cycle을 유지(00 §2)해도 **CH1 cycle은 CH0 counts를 대신 만들어주지 못한다**(채널별 독립 측정, 02 §5.12 Independent).

> [!IMPORTANT]
> **Q2 판정: 조건부 가능(구성)·실질 위험(동작).** 채널별 ATI 분리(CH1 Disabled + CH0 Full)는 06 A.12상 구성 가능하다. 그러나 (1) CH0 ATI Error가 전역 비트를 좌우(02 §5.11)하고, (2) 노터치 CH0 LTA는 동결이 아니라 갱신 대상이라(02 §5.5) 200ms 공백·복원 노이즈가 LTA에 유입돼 §3의 Re-ATI 진동 위험으로 이어진다.

---

## 3. Q3 — 복원 시 카운트 튐 → LTA 흔들림 → Re-ATI 진동

### 3.1 메커니즘 체인 (데이터시트 원문)

1. **enable 복원 직후 카운트 튐**: 방전(0x30 LSB 0x02, 코드 의도상 CRX0→VSS)으로 입력이 강제 방전된 뒤 재 enable하면 첫 conversion counts가 정상 노터치값에서 벗어남(과도). (데이터시트에 토글-복원 과도 수치 부재 — **확인 필요**.)
2. **LTA 오염**: 02 §5.5 — 노터치 LTA는 IIR로 매 측정 갱신. 튄 counts가 LTA를 ATI Target에서 멀어지게 민다.
3. **Re-ATI 발동 경계**: 02 §5.10 (line 645~649) — "LTA가 ATI Band(±Target/8) 밖으로 drift하면 re-ATI 실행." 예: Target=400, Band=1/8 → LTA<350 또는 LTA>450이면 자동 Re-ATI.
4. **진동**: Re-ATI가 발동하면(02 §5.10 — ATI Event bit set) ATI가 다시 counts를 재캘리브레이션. 그 사이 또 200ms 토글이 들어오면 §1대로 ATI 입력이 끊긴다 → ATI 완료 시 Counts가 Boundary 밖 → **ATI Error(02 §5.11)** → (코드 경로상) I²C 무응답 위험. 토글·Re-ATI가 서로를 트리거하는 **진동 루프** 형성 가능.

> [!WARNING]
> **Q3 판정: 진동 위험 실재(조건부불가).** 복원 카운트 튐 → LTA가 ATI Band(02 §5.10) 이탈 → Re-ATI 자동 발동 → ATI 입력이 다음 200ms 토글에 또 끊김 → ATI Error(02 §5.11) → I²C 무응답(코드 실증). Band가 Large(1/8)라 다소 여유는 있으나, 200ms 주기 토글의 과도 counts가 반복 누적되면 진동 진입 가능. 데이터시트는 과도 진폭을 규정하지 않아 안전 마진을 보장할 수 없다.

---

## 4. Q4 — 0x30 LSB/MSB 토글이 ATI 비트·다른 Sensor Setup 비트를 건드리나

### 4.1 데이터시트 A.5 비트정의 vs 코드 해석 (정면 불일치)

- **06 A.5 (line 206~223)** Sensor Setup(0x30) — 상위 바이트=TX_SELECT, 하위=SENSOR_SETUP:
  - LSB bit0 = **Enable Channel**, bit1 = **Linearise Counts**, bit2 = Dual Direction, bit3 = **Invert**.
  - MSB bit8 = **CTx0**, bit9 CTx1, bit10 CTx2, bit13 CalCap Tx, bit14 CalCap Rx.
- **코드가 쓰는 값**(`discharge_crx0` 942·947, `.h` 89):
  - 방전: `LSB=0x02(INACTIVE_RXS_CRX0_VSS), MSB=0x00`
  - 복원: `LSB=0x01, MSB=0x01`

데이터시트 A.5 비트정의대로 해석하면:

| write | LSB 해석 (A.5) | MSB 해석 (A.5) |
|---|---|---|
| 방전 0x02/0x00 | bit1 **Linearise=1**, bit0 Enable=**0** | CTx0=**0** (송신핀 선택 해제) |
| 복원 0x01/0x01 | bit0 Enable=1, Linearise=0 | CTx0=1 |

- **Inactive Rxs(VSS)는 0x30이 아니라 0x34**: 06 A.10 (line 313~322) — Inactive Rxs는 Pattern Definitions(0x34) bits[3:0]에 있고 0x0A=VSS. 코드 상수명 `INACTIVE_RXS_CRX0_VSS`(.h 89)는 0x30 LSB에 0x02를 쓰는데, **데이터시트 A.5에 0x30 LSB의 "Inactive Rxs/VSS" 필드는 없다**. 즉 코드 변수명·주석의 "CRX0 VSS" 의미와 데이터시트 0x30 비트정의가 불일치.

### 4.2 함의

- 데이터시트대로면 방전 write `0x02`는 **VSS 방전이 아니라 Linearise bit를 켜고 Enable을 끄는 것**이다. 복원 `0x01`은 Linearise를 끄고 Enable만 켠다. 즉 200ms마다 **Linearise bit가 켜졌다 꺼졌다** 반복 → self-cap counts 부호/선형성에 영향 가능(02 §5.4.1 line 289~304: Linearise set 시 counts 방향 반전 → Invert 동반 필요).
- **MSB를 0x00으로 덮어쓰는 부작용**: 방전 단계가 MSB=0x00을 쓰면 **CTx0 송신핀 선택까지 클리어**된다(06 A.5 bit8). 복원이 MSB=0x01로 되살리나, 데이터시트상 self-cap은 "CRx와 CTx 동일핀 동시 set 필수"(02 §5.12.1 line 674)이므로 매 200ms CTx0가 잠깐 빠지는 것은 측정 구성을 흔든다.
- **auto-ATI 비트는 0x30에 없음**: ATI Mode/Band는 0x36(A.12) 별개라 토글이 ATI Setup 레지스터를 직접 건드리진 않는다(Q4 직접 답). 그러나 위처럼 **Enable·Linearise·CTx0를 매 주기 건드리므로**, ATI가 의존하는 측정 구성(채널 활성·핀 선택·counts 선형성)을 간접 교란한다.

> [!IMPORTANT]
> **Q4 판정: ATI Setup 레지스터(0x36)는 직접 안 건드리나, 0x30 토글이 Enable(bit0)·Linearise(bit1)·CTx0(MSB bit8)를 매 200ms 건드려 ATI 입력 측정 구성을 간접 교란.** 코드의 "0x02=CRX0 VSS" 해석은 데이터시트 A.5(0x02=Linearise+Enable off)와 불일치하며 VSS는 0x34 A.10 소관 — **실칩 동작 미확정(확인 필요, 4_타당성검증 B2와 동일 미해결 항목)**.

---

## 5. 종합 — 공존 가능 여부 (데이터시트 메커니즘 관점)

| Q | 판정 | 핵심 근거 |
|---|---|---|
| Q1 | 교란(조건부불가) | ATI는 연속 counts 입력 의존(02 §5.9), 200ms disable 토글이 입력 단절. ATI 중 disable 거동 데이터시트 미규정 |
| Q2 | 구성 가능·동작 위험 | 채널별 ATI 분리는 06 A.12상 가능. 단 ATI Error 전역(02 §5.11) + 노터치 LTA 갱신(02 §5.5)로 공백 노이즈 유입 |
| Q3 | 진동 위험 실재 | 복원 튐→LTA가 ATI Band 이탈(02 §5.10)→Re-ATI→ATI Error→I²C hang 루프 |
| Q4 | 간접 교란 + 해석 불일치 | 0x30 토글이 Enable/Linearise/CTx0 건드림. "0x02=VSS" 해석 데이터시트 A.5와 불일치(미확정) |

### 종합 판정: **조건부불가**

auto-ATI **Full**과 200ms 0x30 토글의 동시 운용은 데이터시트 메커니즘상 다음 두 충돌이 핵심이다:

1. **ATI는 연속 측정 입력에 의존**(02 §5.9)하는데 토글이 측정 채널을 주기적으로 끊는다(Q1).
2. **복원 카운트 튐이 LTA를 ATI Band 밖으로 밀어 Re-ATI를 자동 재트리거**(02 §5.10)할 수 있고, 그 결과 ATI Error→I²C 무응답(코드 실증)이라는 알려진 실패 모드로 수렴한다(Q3).

여기에 0x30 비트 해석 불일치(Q4, B2 미해결)와 ATI Error 전역성(Q2)이 위험을 가중한다.

### 권장 대안 (Q6 ③ 기반)

> [!IMPORTANT]
> **데이터시트 메커니즘상 가장 안전한 공존 형태는 "auto Re-ATI 억제 + 방전 유지"다.**
> - **ATI Mode를 Full이 아닌 부팅 1회 ATI(또는 Compensation Only) + 자동 Re-ATI 비활성**으로 두고, 필요 시 **수동 Re-ATI**(02 §5.11 — Re-ATI bit는 master가 수동 set, 자동 트리거 없음)를 방전이 멈춘 안정 구간에만 트리거한다.
> - 이렇게 하면 (a) ATI 진행 구간이 200ms 토글과 겹치는 윈도우를 제거하고(Q1 해소), (b) 노터치 LTA 드리프트에 의한 자동 Re-ATI 진동을 차단(Q3 해소)하며, (c) 방전(물리 전하 제거)은 그대로 유지한다.
> - **선결 검증(데이터시트 게이트)**: ① 0x30 LSB 0x02의 실칩 동작(VSS인가 Linearise인가, B2) RTT/오실로 실측, ② 방전을 0x30 토글 대신 **Inactive Rxs(0x34 A.10 bits[3:0]=0x0A VSS)** 경로로 옮길 수 있는지 — 0x34는 Enable/CTx 비트와 분리돼 있어 측정 구성을 덜 흔든다(Q6 ⑥, C4 영역으로 인계).

> [!NOTE]
> 본 분석은 데이터시트 원문 메커니즘에 한정한다. ATI 발동 시 I²C 무응답의 정확한 원인, 1ch 전류, 실칩 0x30 매핑은 모두 **실측 게이트**(4_타당성검증 §실측필요)에 의존하며 단정 불가다. 코드 통합 충돌점은 C2, 대안 설계는 C4, 레이어 분리(ESD vs LTA)는 C5가 다룬다.
