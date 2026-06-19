---
name: touch-ati-interaction-검증
purpose: 명제_C "터치 상태(접촉 중)에서는 auto-ATI가 동작하지 않는다" — 데이터시트 근거 검증. LTA freeze vs Re-ATI 트리거 구분, 터치 중 Re-ATI 발동 가능성, 가정_2 판정.
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, auto-ati, re-ati, lta-freeze, datasheet, 명제_C, 가정_2]
---

# 03 — 터치-ATI 상호작용 검증가 (명제_C)

> **TL;DR**: 가정_2 "터치 중 auto-ATI 미동작"은 **부분 정확** 판정. LTA freeze(§5.5)와 Re-ATI 트리거(§5.10)는 완전히 별개 메커니즘이며, 은수님이 둘을 혼동했을 가능성 존재. 터치 중 Re-ATI 자동 발동이 억제된다는 명시적 데이터시트 문구는 없다. Self-cap에서 터치=counts 감소이므로 터치 중 LTA가 freeze되어 있다면 LTA는 ATI Band 밖으로 drift하지 않아 Re-ATI가 억제되는 간접 경로가 존재하나, **부팅 시점 터치는 인식되지 않으므로 freeze 자체가 성립하지 않아** 전제가 무너진다. 현 펌웨어(ATI Mode Disabled)에서는 Re-ATI 자동 발동 자체가 0이므로 가정_2 성립 여부는 실질적 무의미.

> [!IMPORTANT]
> 근거: 데이터시트 `02_proxfusion동작.md` §5.5·§5.9·§5.10·§5.11, `06_레지스터레퍼런스.md` A.2·A.12·A.30, 누적 분석 `A2_datasheet.md` (20260612_eunsu-opinion-review), `C1_datasheet.md` (20260612_discharge-autoati-coexist). 추정은 "(추정)" 표기, 데이터시트 미명시는 "(DS 미규정)" 표기.

---

## 1. 검증 목표

| 질문 | 핵심 |
|---|---|
| **Q_C1** | Re-ATI(auto-ATI) 자동 트리거 조건은 무엇인가 |
| **Q_C2** | 터치(접촉) 중에 그 조건이 억제되는가 — 데이터시트 명시 여부 |
| **Q_C3** | LTA freeze(§5.5)와 Re-ATI 트리거(§5.10)는 같은 메커니즘인가 다른 것인가 |
| **Q_C4** | Self-cap에서 터치=counts 감소이므로 터치 중 Re-ATI 발동 가능성은 있는가 |

---

## 2. Q_C1 — Re-ATI 자동 트리거 조건

### 2.1 데이터시트 원문 (§5.10 Automatic Re-ATI)

> "re-ATI는 채널 LTA가 `ATI Band`(ATI Target 중심) 밖으로 drift할 때 실행."
>
> `Re-ATI Boundary = ATI Target ± ATI Band`
>
> 예시: ATI Target=800, ATI Band=1/8 → LTA>900 또는 LTA<700일 때 실행.
>
> `System Status`의 `ATI Event` bit set, master가 read하면 clear.

**Sound1 설정 기반 Re-ATI Boundary:**
- ATI Target = 100 × (64/16) = 400 counts
- ATI Band = Large(1/8) → Band = 400/8 = 50 counts
- **Re-ATI 경계: LTA < 350 또는 LTA > 450**

### 2.2 전제 조건: ATI Mode가 활성이어야 Re-ATI 자동 발동

- `06 A.12`: ATI Mode bits[2:0] = 000이면 Disabled. Re-ATI 자동 발동은 ATI Mode=Full(100) 이상일 때 의미.
- **현 펌웨어**: `0x36` LSB=`0x08` → bits[2:0]=000=**Disabled** (`iqs323.c:651`).
- **결론**: 현 펌웨어에서는 Re-ATI가 자동으로 발동하지 않는다. §5.10의 "LTA drift → Re-ATI" 경로는 비활성 상태.

> [!NOTE]
> ATI Full 도입 시나리오(은수님 제안)에서만 Q_C2~Q_C4가 실질적 의미를 가진다. 이하 분석은 **ATI Mode=Full인 경우를 가정**한다.

---

## 3. Q_C2 — 터치 중 Re-ATI 트리거 억제 여부

### 3.1 데이터시트 명시 여부

데이터시트 §5.10·§5.11·§5.9 전반을 검토한 결과:

> **"터치(접촉) 상태에서 Re-ATI를 억제한다"는 명시적 문구는 존재하지 않는다 (DS 미규정).**

§5.10은 Re-ATI 억제 조건을 LTA drift 기준으로만 서술하며, 터치 상태를 추가 게이트로 언급하지 않는다.

### 3.2 간접 억제 경로 분석

억제가 발생할 수 있는 **간접 메커니즘**은 존재한다:

**경로 1: 터치 인식 → LTA freeze → LTA가 ATI Band 안에 고정 → Re-ATI 미발동**

```
터치 인식(CHx Touch bit set)
  → §5.5: LTA freeze(터치·prox 이벤트 중 LTA 갱신 멈춤)
  → LTA가 마지막 노터치 기준값에 고정
  → LTA = ATI Target 근처(정상 ATI 후) = ATI Band 안
  → §5.10 Re-ATI 조건(LTA drift) 미충족
  → Re-ATI 발동 안 됨
```

**이 간접 경로가 성립하는 전제**: "터치가 이미 인식되어 CHx Touch bit가 set된 상태"여야 한다.

- §5.5 frozen의 전제: "touch·proximity **이벤트 중에는** frozen" → **CHx Touch/Prox bit가 set된 상태**가 frozen의 원인.
- CHx Touch bit는 §5.7 판정공식 `(LTA − Counts) > Touch Threshold`가 충족될 때만 set된다.

---

## 4. Q_C3 — LTA freeze(§5.5)와 Re-ATI 트리거(§5.10) 구분

### 4.1 두 메커니즘의 독립성

| 항목 | LTA freeze (§5.5) | Re-ATI 트리거 (§5.10) |
|---|---|---|
| **주체** | LTA 갱신 여부 | ATI 알고리즘 재실행 여부 |
| **조건** | CHx Touch/Prox bit = 1 (이미 터치 인식됨) | LTA가 ATI Band 경계 이탈 |
| **효과** | LTA 값이 고정됨 | MULT/COMP 재산출 → counts Target 재조정 |
| **레지스터** | LTA 내부 상태 (§5.5 IIR 갱신 여부) | `ATI Event`(0x10 bit4), `ATI Active`(0x10 bit5) |
| **상호관계** | LTA freeze → LTA가 Band 안에 유지 → Re-ATI 억제(간접) | Re-ATI는 LTA drift를 보고 발동, freeze 조건을 직접 참조하지 않음 |

**결론: 두 메커니즘은 완전히 별개 레이어다.**

- LTA freeze는 "현재 터치 중"을 기록하는 상태 보호 장치.
- Re-ATI 트리거는 "LTA가 ATI Target에서 얼마나 벗어났는가"를 보는 감도 재보정 장치.
- 둘은 우연히 연결되어 있을 뿐 동일 메커니즘이 아니다.

### 4.2 은수님 혼동 가능성 검토

은수님 가정_2 "터치 중 auto-ATI 미동작"의 논거가:

- **논거 A(LTA freeze)**: "터치 중 LTA가 멈추니까 ATI도 안 돌 것이다" → **별개 메커니즘**이므로 LTA freeze가 Re-ATI를 직접 차단하지 않음. 다만 간접 경로(LTA freeze → Band 안 유지 → Re-ATI 조건 미충족)는 존재.
- **논거 B(counts 방향)**: "Self-cap에서 터치 시 counts가 감소하니 LTA가 drift 방향으로 안 간다" → Q_C4에서 상세 분석.

은수님이 LTA freeze와 Re-ATI 억제를 동일시했을 가능성은 있으나, **결과적으로 정상 터치 인식 시에는 간접 억제가 작동**하므로 실용적 오류는 아닐 수 있다.

---

## 5. Q_C4 — Self-cap 터치 중 Re-ATI 발동 가능성

### 5.1 Self-cap 방향성

데이터시트 §5.1·§5.7:
- Self-cap: 터치 시 counts **감소** (C 증가 → 1/C 반비례 → counts 낮아짐)
- 터치 진입: `(LTA − Counts) > Touch Threshold` → Counts가 LTA보다 낮아야 성립

### 5.2 시나리오별 분석

**시나리오_가: 정상 터치 인식 후 접촉 지속**

```
[정상 ATI 완료] → LTA ≈ ATI Target = 400
[터치 시작]     → Counts 감소(예: 200)
                → (LTA - Counts) = 200 > Threshold(156) → 터치 인식
                → CHx Touch bit = 1
                → §5.5 LTA freeze 발동
                → LTA 고정 ≈ 400 (ATI Target 근처)
                → LTA는 Re-ATI Boundary [350, 450] 안
                → §5.10 Re-ATI 조건 미충족
                → 터치 중 Re-ATI 발동 안 됨 (간접 억제)
```

**결론 (시나리오_가)**: 정상 터치 인식 후 접촉 지속 중에는 LTA freeze → ATI Band 안 유지로 Re-ATI가 간접 억제된다. 은수님 가정_2는 이 시나리오에서 **사실상 정확**하다.

**시나리오_나: 부팅 시점 터치(delta≈0 → 터치 미인식)**

```
[부팅] → RESEED 발행 → LTA ← 현재 counts(터치 중이면 낮은 counts)
[LTA가 터치 counts로 seed됨] → LTA ≈ 200 (예시)
[터치 지속]  → Counts ≈ 200
             → (LTA - Counts) ≈ 0 < Threshold
             → 터치 미인식 → CHx Touch bit = 0
             → LTA freeze 발동 안 됨
             → LTA가 IIR로 터치 counts 계속 추적
             → LTA ≈ 200 (ATI Target 400 대비 매우 낮음)
             → LTA < 350 → Re-ATI Boundary 이탈
             → §5.10 Re-ATI 자동 발동 가능성 존재
```

**결론 (시나리오_나)**: 부팅 시점 터치 상황에서는 LTA freeze가 성립하지 않아(터치 미인식), LTA가 터치 counts를 추적하다 ATI Band 경계를 이탈하면 Re-ATI가 자동 발동할 수 있다.

→ **가정_2("터치 중 auto-ATI 미동작")의 전제는 "정상 터치 인식 후"에만 성립. 부팅 시점 터치에서는 반증된다.**

### 5.3 ATI Band 이탈까지의 경로

- ATI Target = 400, Band = ±50 → 경계: [350, 450]
- 부팅 중 LTA가 터치 counts(예: 200~250)를 추적하면 LTA < 350 → 경계 이탈
- BETA 값에 따라 이탈 속도가 달라짐 (현 펌웨어 BETA = POR default 0 → 미설정)
- 이탈 시 Re-ATI 자동 발동 → ATI 알고리즘이 터치 counts를 새 Target으로 보정 → 손 떼면 노터치 counts가 Max Counts 한계 초과 위험(§5.4.2)

---

## 6. ATI 중 I2C 무응답과의 연관

누적 분석(A2_datasheet.md, C1_datasheet.md)에서 확인된 실증 사례:

- ATI Full 사용 시 stuck-touch → auto Re-ATI → ATI_ERROR → I2C 무응답 (코드 주석 `iqs323.c:645`)
- **경로**: LTA drift → Re-ATI 발동 → `ATI Active` bit(0x10 bit5) set → ATI 진행 중 I2C 통신 윈도우 응답 지연 또는 무응답
- §5.11: "ATI Error 발생 시 Re-ATI가 자동 트리거되지 않는다. master가 수동 트리거해야 함."

→ 이 경로가 은수님 가정_2 전략("터치 중엔 I2C 실패 안 남")의 전제를 흔드는 핵심 위험이다:
- 정상 터치 인식 → I2C 실패 없음 (간접 억제 성립)
- 부팅 터치 → LTA drift → Re-ATI 발동 → I2C 실패 가능성 (가정_2 반증 시나리오)

---

## 7. stuck-touch timeout disable과의 관계

현 코드: `CH Timeout Disable` bit set (`iqs323.c:1166`, `System Control` 0xC0 bits[10:8]).

§5.8: Channel Timeout은 "prox/touch 상태가 지정 시간 초과 시 Reseed 후 상태 강제 해제."

- Channel Timeout이 disable되어 있으면 stuck-touch 시 자동 Reseed가 발생하지 않는다.
- 이는 "LTA가 지속적으로 잘못 고정될 때의 자동 탈출" 경로를 차단한다.
- 단, 현 코드의 Timeout disable은 ATI Disabled 상태와 병행이므로, auto Re-ATI가 꺼진 상태에서 Timeout도 꺼진 = 자동 복구 루트가 완전히 없는 구조다.
- **ATI Full로 전환 시 Channel Timeout 정책도 함께 재검토 필요** (§5.8 경고: "ULP 모드 금지" 포함).

---

## 8. 종합 판정

| 질문 | 판정 | 근거 |
|---|---|---|
| **Q_C1**: Re-ATI 트리거 조건 | LTA가 ATI Band 경계[ATI Target ± ATI Band] 이탈 시 자동 발동 | §5.10 원문 |
| **Q_C2**: 터치 중 Re-ATI 억제 여부 | DS 미명시. 간접 억제(freeze → Band 안 유지)는 정상 터치 인식 시에만 성립 | §5.5+§5.10 조합 |
| **Q_C3**: LTA freeze vs Re-ATI 트리거 동일성 | **완전히 별개 메커니즘** — 은수님 혼동 가능성 있으나 결과는 조건부 일치 | §5.5 vs §5.10 |
| **Q_C4**: 터치 중 Re-ATI 발동 가능성 | 정상 터치 인식 후: 간접 억제로 실질적 차단. **부팅 터치 시: LTA drift 후 Re-ATI 발동 가능** | §5.5+§5.7+§5.10 |

---

## 9. 가정_2 최종 판정

> **판정: 부분 정확 (조건부 성립)**

| 조건 | 성립 여부 |
|---|---|
| 현 펌웨어 (ATI Disabled) | **가정_2 자체가 무의미** — Re-ATI 자동 발동 비활성 |
| ATI Full 도입 + 정상 터치 인식 후 | **사실상 성립** — LTA freeze → ATI Band 안 유지 → Re-ATI 간접 억제 |
| ATI Full 도입 + 부팅 시점 터치 | **반증** — delta≈0 → 터치 미인식 → freeze 없음 → LTA drift → Re-ATI 발동 가능 |

**은수님 전략("터치 중엔 I2C 실패 안 남")의 전제 강도:**

- 정상 사용 시나리오(이미 정상 LTA가 잡힌 상태에서 터치): 전제 성립 가능
- 부팅 시점 터치(RESEED가 터치 counts로 LTA를 seed한 직후): 전제 반증 가능
- "10회 연속 I2C 실패 = 고장"의 판별 기준은 부팅 터치 + ATI Full 조합에서 false alarm을 낼 수 있음

---

## 10. 핵심 발견 요약 (8줄 이내)

1. **Re-ATI 트리거 조건**: LTA가 `ATI Target ± ATI Band` 경계를 이탈할 때 자동 발동. 데이터시트는 터치 상태에서 Re-ATI를 차단한다는 명시 문구 없음 (DS 미규정).
2. **LTA freeze ≠ Re-ATI 억제**: 두 메커니즘은 완전히 별개. freeze는 CHx Touch bit=1일 때 LTA 갱신 중단, Re-ATI는 LTA drift 감지 시 ATI 알고리즘 재실행. 혼동 가능.
3. **간접 억제 경로**: 정상 터치 인식(CHx Touch bit set) → LTA freeze → LTA가 ATI Band 안 고정 → Re-ATI 조건 미충족 → 간접 억제. 이 경로에서는 가정_2 실질적 성립.
4. **Self-cap 방향**: 터치 시 counts 감소이므로, 터치 미인식 상태(delta≈0)에서는 LTA가 터치 counts(낮은 값)를 추적하다 ATI Target 아래 경계[Target - Band]를 이탈 가능.
5. **부팅 터치 반증**: 부팅 중 터치 상태에서는 delta≈0 → 터치 미인식 → LTA freeze 없음 → LTA가 터치 counts 추적 → ATI Band 이탈 → Re-ATI 자동 발동 가능. 가정_2 반증 시나리오.
6. **현 펌웨어 실질**: ATI Mode=Disabled(0x36 LSB=0x08, bits[2:0]=000)이므로 Re-ATI 자동 발동 자체가 없어 가정_2는 현재 구조에서 무의미.
7. **ATI Full 도입 시 위험**: 부팅 터치 + ATI Full 조합에서 Re-ATI 발동 → I2C 무응답 재현 경로 존재(코드 실증: `iqs323.c:645` 주석). 이 경로가 은수님 전략의 허점.
8. **전략 강도 종합**: 은수님 전략은 정상 사용 시나리오에서 유효하나, 부팅 시점 터치 + ATI Full 조합에서 취약. "터치 중 I2C 실패 안 남"은 전제 성립 조건을 명시해야 한다.
