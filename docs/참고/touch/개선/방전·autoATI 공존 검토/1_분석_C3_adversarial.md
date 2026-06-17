---
name: 방전autoATI-1분석-C3-adversarial
purpose: auto-ATI Full ⊕ 200ms CRX0 수동방전 공존의 실패 모드 적대적 발굴 (Adversarial 반대자 페르소나)
type: 개선
maturity: experimental
tags: [touch, iqs323, auto-ati, re-ati, discharge, adversarial, i2c-hang, failure-mode]
---

# C3 · Adversarial 반대 — auto-ATI Full ⊕ 200ms 방전 공존의 실패 모드

> **TL;DR**: 공존을 적대적으로 공격한 결과 **불가에 가까운 조건부불가**. 가장 약한 고리 5개: ① **방전 토글이 0x30 LSB·MSB 전체 바이트를 blind write해 Linearise/Invert/Release UI/Dual-Direction 비트를 매 200ms 클리어**(Q4·B2 정면 충돌, 코드 `drv.c:947` 확정) — Release UI(order 001)와 양립 불가. ② **방전이 0x30 LSB enable 비트를 끄므로 ATI 진행 중 채널 disable → Re-ATI 미완·ATI Error·재트리거 진동**(Q1·Q3). ③ **Full ATI = I²C 무응답 실증 사례**(데이터시트 §5.9·드라이버 주석)가 200ms 폴링·방전 통신과 정면 충돌 — verify 없는 best-effort 방전이 hang 중 silently 실패. ④ **방전 측정 공백이 노터치 카운트를 흔들어 LTA가 ATI Band 이탈 → 자가 Re-ATI 폭주**(Q3). ⑤ **레이어 분리가 환상**(Q5) — ESD 물리전하·LTA 드리프트가 같은 0x30·같은 측정 cycle을 공유한다. 권장: 공존 강행 대신 **시간분리(ATI 진행 중 방전 보류) + 방전 RMW(read-modify-write) 필수화**, 그조차 Full ATI I²C hang 실측 통과가 선결.

> [!WARNING]
> 본 문서는 **반대자(adversarial) 페르소나**다. 의도적으로 실패 가능성을 최대화해 가장 약한 고리를 공격한다. "공존 가능"이라는 낙관 가설의 반증을 우선한다. 추정은 "(확인 필요)"로 명시하되, 의심스러우면 불리하게 해석한다.

> [!IMPORTANT]
> 근거: 코드 `tdc_drv_iqs323.c`(`discharge_crx0` 938~952·`apply_settings` 1023·`is_auto_ati_done` 913)·`.h`(INACTIVE_RXS 87~89·status 187~189). 데이터시트 `02_proxfusion동작.md`(§5.9 line 561·§5.10 line 641~658·§5.11 line 660~667). 검증 `4_타당성검증.md` B1·B2·B4. 하이브리드 권고 §3·§5.

---

## 0. 공격 대상 명제

> "auto-ATI Full(부팅 1회 + 자동 Re-ATI 활성) + 200ms CRX0 VSS 수동방전(0x30 0x02↔0x01) + CRX1 CalCap을 **함께** 돌릴 수 있다."

이 명제의 가장 약한 고리를 순서대로 부순다.

---

## 1. 공격 1 (Q4·치명) — 방전이 0x30 전체 바이트를 blind write해 ATI/Release UI 비트를 파괴한다

### 1.1 코드 확정 사실 (인용)

`discharge_crx0()` (`tdc_drv_iqs323.c:942-947`):

```c
/* 방전: CH0 disable + CRX0 VSS */
write_register_discharge(0x30, 0x02, 0x00);   // LSB=0x02, MSB=0x00
/* 복원: enable + ctx0 */
write_register_discharge(0x30, 0x01, 0x01);   // LSB=0x01, MSB=0x01
```

이것은 **read-modify-write가 아니다.** LSB·MSB 전체 바이트를 매 200ms 두 번 **통째로 덮어쓴다.**

### 1.2 무엇이 파괴되는가

데이터시트 [06 A.5] Sensor Setup LSB 비트 정의(검증 B2 인용): **bit0 Enable·bit1 Linearise·bit2 Dual Direction·bit3 Invert·bit6 Release UI Enable.**

| 방전이 쓰는 값 | 켜지는 비트 | 꺼지는(클리어되는) 비트 |
|---|---|---|
| 복원 `LSB=0x01` | bit0 Enable | **bit1 Linearise·bit3 Invert·bit6 Release UI** 전부 0 |
| 방전 `LSB=0x02` | bit1 (Linearise 또는 코드해석상 CRX0_VSS) | bit0 Enable·bit3·bit6 전부 0 |

→ **공존의 가장 약한 고리.** order 001(Release UI)을 쓰려면 self-cap 부호 반전을 위해 Linearise(bit1)+Invert(bit3)가 **상시 set**돼야 하는데(B2: "하나만 켜면 영원히 fire 안 됨 또는 즉시 오발"), 방전이 매 200ms 이 비트들을 **0으로 클리어**한다. 다음 방전 직전까지 200ms 동안 Release UI 로직이 무력화되거나 잘못된 부호로 동작한다.

> [!WARNING]
> **MSB도 위험**: 복원 시 `MSB=0x01`(ctx0)을 쓰는데, sensor_setup의 CH0 MSB 원본도 0x01이라 우연히 일치한다. 그러나 만약 auto-ATI Full 도입 시 CH0 MSB에 다른 패턴/Conversion 비트를 추가하면 방전 복원이 그것까지 **0x01로 되돌려 파괴**한다. 현재 우연히 안전한 것이지 설계적으로 안전한 게 아니다. (확인 필요: CH0 MSB의 auto-ATI 관련 비트 유무)

### 1.3 "ATI Setup은 0x36 별개"라는 방어 논리 반박

낙관론은 "ATI는 0x36, 방전은 0x30이라 충돌 안 함"이라 주장할 것이다. **반박**: ATI Mode는 0x36이 맞지만, **ATI 알고리즘의 입력인 채널 enable·linearise·counts 부호는 0x30**이다. 방전이 0x30 enable을 끄는 순간 그 채널은 ATI 대상에서 빠지고(공격 2), linearise를 끄면 ATI가 정규화하는 counts 부호가 200ms마다 뒤집힌다. **0x36과 0x30은 독립이 아니라 ATI 입력-설정 관계다.**

---

## 2. 공격 2 (Q1·Q3) — 방전이 ATI 진행 중 채널을 disable시켜 수렴을 깨고 진동을 만든다

### 2.1 타이밍 충돌 시나리오

- auto-ATI/Re-ATI는 "짧은 시간에 실행"되나(§5.9 line 538) 정확한 duration은 데이터시트 미명시(확인 필요). 200ms 폴링 주기와 ATI 실행 시간이 겹칠 확률은 0이 아니다.
- 방전 1단계 `0x30 LSB=0x02`는 **bit0 Enable=0** → CH0 disable. ATI 알고리즘이 CH0 divider/multiplier/compensation을 수렴시키는 도중 채널이 사라진다.
- 데이터시트는 "ATI 완료 시점 Counts가 Re-ATI Boundary 밖이면 ATI Error bit set"(§5.11 line 664). **ATI 도중 채널 disable → counts 측정 불능 → ATI 비정상 종료 → ATI Error 가능성**(확인 필요, 칩 거동 미명시이나 불리하게 해석).

### 2.2 Re-ATI 진동(oscillation) 시나리오 — Q3 직격

```
방전(CH0 disable 200ms 주기)
   → 복원 직후 카운트 튐(전하 재충전 과도)
   → LTA가 과도 카운트를 추적, ATI Band(예: Target±1/8) 이탈
   → IC 자동 Re-ATI 트리거(§5.10 line 645)
   → Re-ATI 진행 중 다음 200ms 방전이 또 CH0 disable
   → Re-ATI 미완/Error → 다시 트리거 ...  ← 진동 루프
```

§5.10 예시: Target=800, Band=1/8 → `LTA>900` 또는 `LTA<700`에서 Re-ATI. 방전 복원 직후 과도 카운트가 이 경계를 한 번이라도 넘기면 **자가 재트리거**가 시작되고, 200ms 방전이 매 사이클 이를 재점화한다. **터치-방전-ReATI 3중 진동**이 Q3의 worst case다.

> [!IMPORTANT]
> **반론 차단**: "ATI Band를 넓히면 진동 안 함"이라 할 수 있으나, Band를 넓히면 Re-ATI의 드리프트 보정 목적 자체가 약해진다(애초에 ATI Full을 켜는 이유 상실). 진동을 막으려 Band를 넓히면 auto-ATI 도입 가치가 사라지는 **trade-off 딜레마**.

---

## 3. 공격 3 (제약·치명) — Full ATI I²C 무응답이 200ms 방전 통신을 silently 죽인다

### 3.1 실증 사실

- 데이터시트 §5.9 (line 561): **"Full 모드에서는 ATI 실행 중 I²C 응답 지연이 발생할 수 있다(Sound1에서 확인된 문제)."**
- §5.10 (line 658): **"Full Mode에서 autoATI 발동 시 I²C 무응답 사례가 있었기 때문"** — 이것이 현재 ATI Disabled를 채택한 실증 이유.
- 공통 입력 §4: "현재 ATI Disabled가 채택된 실증 이유: Full auto-ATI 발동 시 I²C 무응답 사례."

### 3.2 방전과의 충돌

방전 write는 **verify 없는 best-effort**(`drv.c:941` 주석 "verify(read-back) 불필요 ... best-effort"). I²C가 ATI 중 무응답이면:

1. `write_register_discharge(0x30, 0x02, ...)` 가 RDY 윈도우 타임아웃(45ms open + 20ms close, 부록)에 걸려 **실패 반환** → `discharge_crx0()`가 `return false`.
2. 이때 **CH0가 disable(0x02) 상태로 남고 복원(0x01)이 실행되지 않을 수 있다** — 1단계는 성공, 2단계 복원이 hang에 걸리는 경우. → **CH0 영구 disable + 측정 정지 + 터치 먹통**.
3. verify가 없으니 SW는 복원 실패를 **감지하지 못한다**(`write_and_verify`도 값 비교 안 함 — 분석 §12). silently 죽는다.

> [!WARNING]
> **가장 위험한 단일 실패**: ATI Full I²C hang 도중 방전 1단계만 들어가고 복원이 막히면, CalCap CH1만 cycle을 유지한 채 CH0는 영구 disable된다. RDY 먹통은 피해도 **CH0 터치 자체가 사라진다.** 현재 방전 구조가 verify 없는 best-effort라서 이 상태를 자가 복구할 길이 없다.

---

## 4. 공격 4 (Q2·Q3) — 측정 공백이 노터치 카운트를 흔들어 ATI/LTA를 오판시킨다

- 방전 동안 CH0는 측정 공백(CalCap CH1만 cycle 유지, 입력 §2). auto-ATI Full에서는 이 공백이 단순 무시되지 않는다.
- 복원 직후 CRX0가 VSS로 방전됐다가 다시 충전되는 **과도 구간 카운트**가 정상 노터치 카운트와 다르다 → LTA가 이 과도값을 흡수.
- 200ms마다 반복되면 LTA에 **방전 과도 성분이 주기적으로 주입**되어 노터치 baseline이 실제보다 흔들린다. 이것이 ATI Band 경계 근처면 공격 2의 Re-ATI 진동으로 연결.
- **Q2 부분(CH1만 Disabled + CH0만 Full) 반박**: 채널별 ATI Mode 분리는 가능하나(0x46이 채널별 ATI Setup), CH0를 Full로 두는 순간 위 측정공백·진동·I²C hang이 전부 CH0에 작용한다. CH1 Disabled는 ATI Error 회피만 할 뿐 CH0 공존 문제를 전혀 해결하지 못한다.

---

## 5. 공격 5 (Q5) — "레이어 분리"는 환상이다

낙관론의 마지막 방어선: "ESD 물리전하 방전과 auto-ATI 드리프트 보정은 다른 레이어니 공존 가능."

**반박**: 메커니즘은 다를지언정 **자원(resource)은 공유한다.**

| 공유 자원 | 방전이 쓰는 방식 | auto-ATI가 쓰는 방식 | 충돌 |
|---|---|---|---|
| **0x30 LSB·MSB** | 전체 바이트 blind write | enable·linearise·invert를 ATI 입력으로 읽음 | 공격 1 |
| **CH0 measurement cycle** | 200ms disable | counts 수렴에 연속 측정 필요 | 공격 2·4 |
| **I²C/RDY 윈도우** | 200ms 2회 write | ATI 중 점유(무응답) | 공격 3 |
| **LTA** | 복원 과도 주입 | Re-ATI 트리거 기준 | 공격 4 |

"다른 레이어"라는 말은 **개념적 분리일 뿐 물리적·코드적으로는 같은 레지스터·같은 cycle·같은 버스를 다툰다.** 레이어 분리 논증은 공존 가능 근거가 되지 못한다. (단 Q5 자체의 답 "방전이 auto-ATI로 대체 불가한 고유 기능인가"는 **참** — ESD 물리누적이 실재한다면 LTA 재캘리브레이션은 전하를 제거하지 못함. 4_타당성검증 A6 정합. 즉 방전을 버릴 수도, auto-ATI와 안전히 합칠 수도 없는 **딜레마**.)

---

## 6. 약한 고리 종합 (severity 순)

| # | 실패 모드 | 근거 | severity | 회피 가능성 |
|---|---|---|---|---|
| 1 | 0x30 blind write가 Linearise/Invert/Release UI 클리어 | `drv.c:947`, B2 | **치명** | RMW로만 회피 (코드 변경 필수) |
| 2 | ATI hang 중 방전 복원 실패 → CH0 영구 disable | §5.9 line561, verify 부재 | **치명** | I²C 통신관리 + verify 도입 선결 |
| 3 | 방전 disable이 ATI/Re-ATI 수렴 중단·진동 | §5.10·§5.11 | 높음 | 시간분리(ATI 중 방전 보류) |
| 4 | 측정공백 과도 카운트가 LTA·Re-ATI 오판 | §5.10 line645 | 높음 | Band 튜닝(가치 상쇄 딜레마) |
| 5 | 레이어 분리 환상 — 자원 공유 충돌 | 종합 | 구조적 | 분리 불가, 시간분리만 완화 |

---

## 7. 평결 — 조건부불가

**무조건 공존(현 방전 구조 그대로 + ATI Full 단순 활성)은 불가.** 근거: 공격 1·2·3이 코드/데이터시트로 확정된 정면 충돌이며, 그중 공격 3(I²C hang 중 복원 실패)은 verify 부재로 **자가 복구 불가능한 치명 결함**. 현재 ATI Disabled를 채택한 실증 이유(I²C 무응답)가 그대로 재현된다.

**조건을 모두 충족하면 조건부가능**이나, 그 조건이 무겁다:
1. 방전을 **RMW(read-modify-write)** 로 전환 — 0x30 LSB enable 비트만 토글, Linearise/Invert/Release UI 보존 (공격 1 해소).
2. **시간분리** — `is_auto_ati_done()`(`drv.c:913`)으로 ATI/Re-ATI 진행 중이면 방전 **보류**, 완료 후에만 실행 (공격 2·3 완화).
3. 방전 write에 **verify + 복원 실패 시 강제 재복원** 로직 (공격 3 치명도 제거).
4. **Full ATI I²C 무응답 실측 통과**(하이브리드 §5 게이트) — 이게 깨지면 위 1~3을 다 해도 공존 불가.
5. ESD 물리누적 실측 — 방전이 정말 필요한지부터 확정(불필요하면 방전 제거가 정답, 공존 문제 소멸).

조건 4가 선결 게이트다. 이것이 통과 못 하면 나머지는 무의미하다.

---

## 8. 권장 대안 (반대자가 차선으로 미는 것)

공존 강행보다 **시간분리 + 빈도 하향**(Q6 ①+②)을 권장한다:

- **ATI 진행 중 방전 보류**(시간분리): 가장 적은 충돌. ATI는 보통 부팅·드리프트 시 가끔, 방전은 상시이므로 충돌 윈도우가 작다. `is_auto_ati_done()` 게이트 1줄로 구현.
- **방전 빈도 하향**(200ms→예: 2~5s): 충돌 확률·LTA 과도주입을 비례 감소. 단 ESD 누적 속도 실측 필요.
- **차선의 차선**: ESD 실측 결과 물리누적이 미미하면 **방전 제거 + auto-ATI 단독**(하이브리드 권고 Phase 3) — 공존 문제 자체를 없애는 가장 깨끗한 길.

> [!IMPORTANT]
> 반대자 결론: **"공존하지 마라. 둘 중 하나를 시간·조건으로 격리하거나, 실측으로 방전을 제거하라."** 무조건 공존은 verify 없는 best-effort 방전이 ATI hang을 만나는 순간 CH0를 silently 죽인다.
