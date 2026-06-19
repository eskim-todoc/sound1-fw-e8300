---
name: datasheet-ati-verification
purpose: IQS323 데이터시트 근거로 ATI Full(auto-ATI) 동작 — 빈도·지속시간·트리거 조건·I2C 차단 메커니즘·상태 비트 위치 검증
type: agent-log
maturity: stable
tags: [touch, iqs323, ati, auto-ati, re-ati, i2c, system-status, datasheet-verification]
---

# 01 데이터시트 ATI 동작 검증

> **TL;DR**: IQS323 ATI Full 모드의 auto-ATI(Re-ATI)는 **이벤트성** — 채널 LTA가 Re-ATI Boundary를 이탈할 때만 발동. 지속시간은 데이터시트 미명시("짧은 시간"만 언급). ATI 진행 중 I2C는 명시적으로 차단되며(§8.4), ATI_Active(0x10 bit5)·ATI_Error(bit6)·ATI_Event(bit4)가 System Status 0x10에 위치한다.

---

## 검증 근거 파일

- 데이터시트 원문 정리: `docs/참고/touch/데이터시트/02_proxfusion동작.md` (§5.9~5.11)
- I2C 인터페이스: `docs/참고/touch/데이터시트/05_i2c인터페이스.md` (§8.4, §8.12, §8.15)
- 레지스터 레퍼런스: `docs/참고/touch/데이터시트/06_레지스터레퍼런스.md` (A.2, A.12, A.30)
- 종합 레퍼런스: `docs/참고/touch/레퍼런스/IQS323-레지스터-맵.md`
- 원본 PDF: `docs/참고/touch/iqs323_datasheet.pdf` (Azoteq v1.11, 2025-08)

---

## 명제_B: auto-ATI 지속시간·빈도·트리거

### B-1. auto-ATI는 주기적인가, 이벤트성인가?

**결론: 이벤트성(Re-ATI 트리거 조건 충족 시에만 발동)**

데이터시트 §5.10 원문:

> "IQS323 automatically detects when a channel is outside of its designed operating range and automatically triggers a re-ATI."
>
> "re-ATI occurs when the channel LTA has drifted out of the ATI Band"

Re-ATI Boundary 수식:
```
Re-ATI Boundary = ATI Target ± ATI Band
```

- `ATI Band` 설정: `ATI Setup`(0x36/0x46/0x56) bit3
  - 0 = Small: 1/16 × ATI Target
  - 1 = Large: 1/8 × ATI Target
- **발동 조건**: 채널 LTA가 `LTA > (ATI Target + ATI Band)` 또는 `LTA < (ATI Target − ATI Band)` 를 충족할 때

예시(원문): ATI Target=800, ATI Band=1/8 → band=100 → LTA>900 또는 LTA<700 시 Re-ATI 실행.

주기적 타이머 기반 Re-ATI는 데이터시트에 **없음**. 완전히 조건 기반 트리거.

### B-2. auto-ATI 1회 지속시간

**결론: 데이터시트 미명시**

데이터시트 §5.9 원문에는 다음 문장만 있다:

> "The ATI algorithm runs in a short amount of time without user notice."

"짧은 시간(short amount of time)" 이상의 정량 수치(ms)는 **데이터시트에 없음**. AZD004 앱노트를 참조하도록 간접 유도하나 해당 앱노트는 본 검증 범위 외.

[추정] 2단계 수렴(MULT → COMP) 알고리즘 특성상 수십~수백 ms 범위로 추정되나, 데이터시트 수치 없음.

### B-3. auto-ATI 빈도

**결론: 데이터시트 미명시 — LTA drift 속도에 의존**

LTA가 ATI Band를 벗어나는 속도는 환경 변화 속도(온도·습도·기계적 변형)와 LTA Beta(§5.6 IIR 필터) 설정에 따라 달라진다. 데이터시트는 Re-ATI 최소 간격·최대 빈도 수치를 명시하지 않는다.

---

## 명제_A: ATI 진행 중 I2C 차단 메커니즘

### A-1. I2C 차단 명시 여부

**결론: 데이터시트에 명시적으로 차단됨**

데이터시트 §8.4 원문:

> "While the Reset Event bit in the System Status register is not set, I²C communication is disabled during ATI."

해석:
- `System Status`(0x10)의 `Reset Event` bit(bit7)이 **set이 아닌 상태** → ATI 중 I2C 통신 disabled
- 즉 ATI 실행 중에는 RDY window가 열리지 않아 마스터가 I2C 통신을 시작할 수 없다.

> [!IMPORTANT]
> 이 규정은 **POR 직후 ATI** 및 **런타임 Re-ATI 모두에 동일하게 적용**된다. §8.4는 조건을 "Reset Event bit가 set이 아닌 동안"으로 명시하며, 런타임 Re-ATI는 Reset 없이 발생하므로 Reset Event bit가 set되지 않은 상태 → ATI 중 I2C 차단 규칙이 적용된다.

### A-2. RDY 핸드셰이크와 ATI_Active 비트

ATI 진행 중에는:
- `ATI_Active` bit(0x10 bit5) = 1 로 set되어 ATI 상태 표시
- RDY line: ATI 완료 전까지 comm window를 열지 않음 → 마스터 측에서 I2C START를 발행해도 응답 없음(또는 §8.10에 따라 `0xEE` 반환)

### A-3. ATI 완료 후 상태 보고

- ATI 완료(성공): `ATI Event` bit(0x10 bit4) = 1 set → 마스터 read 시 clear
- ATI 완료(실패): `ATI Error` bit(0x10 bit6) = 1 set
- Event mode에서 두 이벤트 모두 `Events Enable`(0xD3) bit6(ATI Error)·bit4(ATI Event)로 개별 활성화 가능

---

## ATI_Active / ATI_Error 상태 비트 위치

`System Status` 레지스터 **0x10** (A.2, Read Only):

| 비트 | 필드 | 의미 |
|---|---|---|
| bit5 | **ATI_Active** | 0 = ATI 비활성 · 1 = ATI 진행 중 |
| bit6 | **ATI_Error** | 0 = 이상 없음 · 1 = ATI 완료 후 counts가 Re-ATI Boundary 밖 |
| bit4 | **ATI_Event** | 0 = 없음 · 1 = ATI 트리거됨 (마스터 read 시 clear) |

> [!IMPORTANT]
> `ATI_Error`는 **전역 비트(채널 구분 없음)**. 3채널 중 어느 하나라도 ATI 실패 시 set. 채널별 ATI error 레지스터는 없다(A.2 전체에서 Touch/Prox만 채널별).

ATI Error 발생 시 **자동 재시도 없음** — §5.11 원문:

> "A re-ATI is not automatically triggered when ATI Error occurs. The ATI Error bit is set and it is up to the master to manually trigger a re-ATI by setting the Re-ATI bit in System Control."

마스터가 `System Control`(0xC0) bit2(`Re-ATI`) = 1 로 수동 트리거해야 하며, IQS323이 완료 후 자동 clear.

---

## System Control Re-ATI 트리거

`System Control` 레지스터 **0xC0** (A.30, Read/Write):

| 비트 | 필드 | 값/의미 |
|---|---|---|
| bit2 | **Re-ATI** | 0 = No Re-ATI · 1 = Trigger Re-ATI (완료 후 IC 자동 clear) |

---

## Program Flow에서의 ATI 위치

§8.15 Program Flow(Figure 8.3):

```
POR → ① Ack Reset → ② Write Settings → ③ ATI All → Runtime Loop
                                                         │
                                             RDY Low → Read Events
                                                         │
                                             ATI Error? YES → ③ ATI (재실행)
```

- POR 후 ATI 완료 전: I2C 차단 (§8.4) → Ack Reset로 Reset Event 확인 후에야 Write Settings 가능
- 런타임 Re-ATI(auto): ATI Error 감지 시 Re-ATI bit 수동 set → 재실행

---

## 검증 결론 요약

| 항목 | 결론 | 근거 |
|---|---|---|
| auto-ATI 성격 | **이벤트성** — LTA가 Re-ATI Boundary 이탈 시만 발동 | §5.10 |
| 지속시간 | **데이터시트 미명시** ("short amount of time"만 언급) | §5.9 |
| 빈도 | **데이터시트 미명시** — 환경 drift 속도 의존 | §5.10 |
| 트리거 조건 | `LTA > (ATI_Target + ATI_Band)` 또는 `LTA < (ATI_Target − ATI_Band)` | §5.10 |
| I2C 차단 여부 | **명시적 차단** — ATI 중 Reset Event bit set 아닌 상태 → I2C disabled | §8.4 |
| 차단 메커니즘 | RDY window 미개방 → 마스터 통신 불가 | §8.6, §8.7 |
| ATI_Active 위치 | System Status 0x10 **bit5** | A.2 |
| ATI_Error 위치 | System Status 0x10 **bit6** (전역 비트) | A.2, §5.11 |
| ATI_Event 위치 | System Status 0x10 **bit4** | A.2 |
| ATI Error 자동 재시도 | **없음** — 마스터가 0xC0 bit2(Re-ATI) 수동 set 필요 | §5.11 |
