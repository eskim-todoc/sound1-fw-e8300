---
name: datasheet-verify-rdy-ati-lta
purpose: RDY 핸드셰이크·communication window·스트리밍·ATI 밴드·LTA IIR 수렴 4개 항목 데이터시트 근거 확정
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, datasheet, rdy, communication-window, streaming, ati-band, lta, verify]
---

# IQS323 데이터시트 검증 — RDY/Comm Window/스트리밍/ATI Band/LTA

**TL;DR**: 4개 항목 전부 데이터시트(05_i2c인터페이스.md §8, 02_proxfusion동작.md §5) 직접 인용으로 확정. 추정 [추정] 표기 항목 1건(STOP 후 RDY HIGH 복귀 시간 ~200 µs).

---

## 1. RDY 핸드셰이크와 communication window 개폐 조건·타이밍

**데이터시트 근거**: `05_i2c인터페이스.md` §8.6 Ready (RDY) Indicator, §8.7 Communications Window, §8.8 I²C Transaction Timeout

### 개폐 조건

- **window 열림**: IQS323이 master에 보낼 데이터가 있으면 RDY line을 LOW로 당김 → comm window open (§8.7)
- **window 닫힘 — 정상 종료**: master가 I²C STOP 조건 전송 (§8.9). `Stop Bit Disable = 1`인 경우 STOP이 무시되며 `0xFF` end communications command로만 닫힘 (§8.9)
- **window 닫힘 — 타임아웃**: comm window 열린 후 `I²C Transaction Timeout` 시간(기본 200 ms, 범위 2~230 ms) 내에 서비스되지 않으면 IQS323이 RDY line을 자동 release → window 닫힘, 이벤트 손실 (§8.8)

### 타이밍

- window 시작(RDY LOW) 시점부터 timeout 카운트 시작 (§8.8)
- master가 START condition 발행한 순간 → I²C Transaction Timeout 비활성화 → §7.6 watchdog timer로 제어권 전환 (§8.8)
- Force Communication 사용 시: SDA 토글 패턴 전송 후 RDY window 열릴 때까지 `t_wait` = **0.1 ms ~ 45 ms** (typical 범위, §8.13)
- STOP 후 RDY HIGH 복귀 시간: 데이터시트 **미명시** → [추정] 약 200 µs 이내 (§8.9 노트 실측 추정)

### RDY 전기 특성

- open-drain, active LOW (§8.6). RDY/MCLR 핀 겸용(§8.6 각주)
- master가 falling-edge 인터럽트로 사용하고 RDY LOW 시에만 I²C 통신 시작하는 것이 최적 (§8.6)

---

## 2. 스트리밍(streaming) 모드 시 윈도우 동작

**데이터시트 근거**: `05_i2c인터페이스.md` §8.11.1 I²C Streaming, §8.11.2 I²C Event Mode

### Streaming 모드 (§8.11.1)

- `System Control`의 `Interface Selection` bit로 선택
- 활성 전력 모드에 해당하는 report rate마다 **이벤트 유무에 관계없이** RDY를 LOW로 assert → comm window 반복 개방
  - Normal Power: `Normal Power Report Rate` (ms)
  - Low Power: `Low Power Report Rate` (ms)
  - Ultra Low Power: `Auto Prox Cycle Select × Ultra Low Power Report Rate` (ms)
- 결과: report rate = 10 ms이면 100 Hz 인터럽트 발생, 터치 없어도 매 사이클 I²C 트랜잭션 발생

### Event Mode와의 차이 (§8.11.2)

- Event Mode: enabled event 하나 이상이 트리거되거나 device가 reset될 때만 RDY assert → 이벤트 없는 구간은 RDY 침묵
- Streaming은 디버깅·개발 단계에서 임시 활용, 제품 펌웨어는 Event Mode 권장 (§8.11.2 노트)

### Streaming 모드에서 window 메커니즘 자체는 동일

- window 열림·닫힘·timeout 조건은 Event Mode와 동일하게 §8.7~§8.9 적용
- 차이는 window를 여는 **트리거** 빈도뿐 (Streaming = 고정 rate, Event = 이벤트 시에만)

---

## 3. Re-ATI 발동(ATI 밴드 이탈) 조건

**데이터시트 근거**: `02_proxfusion동작.md` §5.10 Automatic Re-ATI, §5.11 ATI Error

### Re-ATI 자동 발동 조건 (§5.10)

- 채널 LTA가 `ATI Band` 설정값 기준 경계 밖으로 drift할 때 자동 발동

$$\text{Re-ATI Boundary} = \text{ATI Target} \pm \text{ATI Band}$$

- 즉: `LTA > (ATI Target + ATI Band)` 또는 `LTA < (ATI Target − ATI Band)` 이면 Re-ATI 트리거
- 예시 (§5.10): ATI Target = 800, ATI Band = 1/8 → band = 100 counts → `LTA > 900` 또는 `LTA < 700`일 때 실행
- Re-ATI 발생 시 `System Status`의 `ATI Event` bit SET, master가 I²C read 시 clear (§5.10)

### ATI Error 조건 (§5.11) — Re-ATI와 구별

- ATI 알고리즘 완료 시점에 어느 채널이든 Counts가 Re-ATI Boundary 밖이면 `ATI Error` bit SET
- **ATI Error 발생 시 Re-ATI가 자동 트리거되지 않음** — master가 `System Control`의 `Re-ATI` bit를 SET해 수동 트리거 필요. `Re-ATI` bit는 IQS323이 자동 clear (§5.11)

### Sound1 특이사항

- Sound1은 ATI Mode = Disabled로 고정 MULT/COMP 사용 → autoATI 비활성. 초기화 시 RESEED로 LTA 재설정으로 대응 (§5.10 노트 참조)

---

## 4. LTA IIR 수렴과 터치 delta 판정

**데이터시트 근거**: `02_proxfusion동작.md` §5.5 Reference Value / Long-Term Average (LTA), §5.6 Filter Betas, §5.7 Proximity and Touch Thresholds

### LTA IIR 수렴 (§5.5, §5.6)

- LTA는 환경 변화 추적을 위해 IIR(Infinite Impulse Response) 필터로 천천히 갱신
- **갱신 수식** (§5.6):

$$\text{LTA}_{\text{new}} = \text{LTA}_{\text{old}} + (\text{Counts} - \text{LTA}_{\text{old}}) \times \frac{\text{Beta}}{256}$$

- Beta는 4비트 (0~15), alpha = Beta/256 → 최대 약 5.9% 반영률 → 항상 강한 스무딩
- NP/LP 전력 모드별 별도 beta 레지스터 적용 (`Counts Filter Betas`, `LTA Filter Betas`, `LTA Fast Filter Betas`)
- **Fast Filter Band**: Counts가 LTA로부터 sensing 반대 방향으로 Fast Filter Band 이상 drift하면 fast beta 필터 적용, 그 미만이면 normal filter 복귀 (§5.6)
- **LTA 동결 조건**: touch 또는 proximity 이벤트 중에는 LTA 갱신 중단 (§5.5) — 터치 중 LTA가 낮은 counts를 학습해 해제 인식 불능 방지

### 터치 delta 판정 공식 (§5.7 — Non-inverted, Dual Direction disabled 기준)

- **Prox 진입**: `(LTA − Counts) > Prox Threshold` 가 `Prox Debounce Enter` 연속 샘플 수 초과 시
- **Touch 진입**: `(LTA − Counts) > Touch Threshold`
- **Touch 이탈**: `(LTA − Counts) > (Touch Threshold − Touch Hysteresis)`
- **Invert bit** (§5.7): mutual-cap·inductive처럼 터치 시 Counts가 증가하는 방향이면 판정 반전 필요
- **Dual Direction** (§5.7): `Counts > (LTA + Threshold)` 또는 `Counts < (LTA − Threshold)` 양방향 판정

### delta 값 정의

- Delta ≡ `LTA − Counts` (Self-Cap, Invert = 0 기준)
- 터치 시 Counts 감소 → Delta 양수 증가 → Threshold 초과 → 터치 진입 (§5.4, §5.7)

---

## 검증 요약표

| 항목 | 데이터시트 근거 섹션 | 확정/추정 |
|---|---|---|
| RDY LOW = window 열림 | §8.6, §8.7 | 확정 |
| Timeout 기본 200ms, 범위 2~230ms | §8.8 | 확정 |
| START 후 timeout 비활성화 → §7.6 watchdog | §8.8 | 확정 |
| STOP으로 window 닫힘, 0xFF로 닫힘(Stop Bit Disable=1) | §8.9 | 확정 |
| STOP 후 RDY HIGH 복귀 ~200 µs | §8.9 노트 | [추정] — 실측 |
| Streaming: report rate마다 window 반복 개방 | §8.11.1 | 확정 |
| Event Mode: 이벤트 시에만 window 개방 | §8.11.2 | 확정 |
| Re-ATI 발동: LTA가 ATI Target ± ATI Band 이탈 | §5.10 | 확정 |
| ATI Error 시 수동 Re-ATI bit 필요 | §5.11 | 확정 |
| LTA IIR: LTA = LTA + (Counts − LTA) × (Beta/256) | §5.6 | 확정 |
| 터치 판정: (LTA − Counts) > Touch Threshold | §5.7 | 확정 |
| 터치 중 LTA 동결 | §5.5 | 확정 |
