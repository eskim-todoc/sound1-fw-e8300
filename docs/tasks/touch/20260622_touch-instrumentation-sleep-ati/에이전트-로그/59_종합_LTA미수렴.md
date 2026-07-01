---
name: 59_종합_LTA미수렴
purpose: D=39 약결합 지속 시 LTA 미수렴 원인 규명 — 은수님 의문 3개 답변 + 사각지대 판정 + 해결책 적대적 평가 + 권고
type: 분석
maturity: stable
tags: [touch, iqs323, lta, prox-threshold, channel-timeout, lta-freeze, ati-error, baseline]
---

# 59 종합 — D=39 약결합 LTA 미수렴 분석

> **TL;DR**: D=39 약결합 LTA freeze는 Prox Threshold 기본값=0 → IC 내부 Prox 상시 진입 → §5.5 LTA 동결이 직접 원인. 은수님 3개 의문에 대해 ①LTA freeze(prox) 설명, ②LTA밴드≠ATI밴드 개념 정정, ③ati_error 미발생이 정상임을 데이터시트 근거로 답변. IC Channel Timeout disable + SW stuck TOUCH 전용 = not-touch 약결합은 IC·SW 모두 흡수 불가한 진짜 사각지대. 권고: IC Channel Timeout 활성화(prox timeout 설정, Auto No ULP 유지).

---

## 1. 증상 요약

RTT 계측 결과(핀셋 약결합, 수분 지속):

```
[T] LTA=394  CNT=355  D=39  THR=156 (k=102  H=8)  not touch  delta <= abs_thr
```

- D=39는 THR=156 미만이므로 터치 판정 없음
- LTA(394)가 수분간 Counts(355)로 수렴하지 않고 D=39 고정

---

## 2. 은수님 의문 3개 — 답변 및 개념 정정

### 의문 1: 터치가 아닌데 LTA가 Counts에 수렴하지 않는 이유

**정정 포인트**: "터치 아님 = LTA 자유 수렴" 은 틀린 가정. LTA는 **Prox 이벤트 중에도 동결**된다.

**DS §5.5 원문**: *"LTA is updated slowly to track environmental changes and is frozen during touch and proximity events."*

- Prox 진입 조건(§5.7): `(LTA − Counts) > Prox Threshold`
- 0x61(Prox Settings) 기본값 = 0x0000 → Prox Threshold = 0
- 현재 D = LTA − Counts = 39 > 0 = Prox Threshold → **CH0_Prox 상시 진입**
- CH0_Prox bit(System Status bit8) = 1 → **LTA 갱신 연산 자체 중단**
- LTA Normal Beta=16(0xB1=0x1010)이 설정되어 있어도, LTA 자체가 동결 상태이므로 Beta 값과 무관하게 수렴 불가

**결론**: LTA beta 문제가 아니라 Prox Threshold=0으로 인한 IC 내부 Prox 상태 = LTA freeze.

---

### 의문 2: D=39가 'LTA 밴드'를 벗어나서 Auto ATI가 돌아야 하지 않나

**정정 포인트**: "LTA 밴드"와 "ATI Band"는 **완전히 다른 개념**. D=39는 ATI Band 판정과 무관.

| 용어 | 정의 | 비교 대상 | 역할 |
|---|---|---|---|
| D = LTA − Counts = 39 | 터치 판정용 델타 | Touch Threshold(156)와 비교 | 터치 여부 결정 |
| Fast Filter Band = 10 (0xB4) | LTA 필터 속도 전환 기준 | counts가 LTA로부터 sensing **반대** 방향으로 10cnt 벗어날 때 fast beta 적용 | LTA 수렴 속도 조정 |
| ATI Band = Large(1/8) (0x36 bit3=1) | Auto Re-ATI 발동 기준 | **LTA 절대값** vs ATI Target ± Band | Re-ATI 자동 트리거 |

**ATI Band 계산** (DS §5.10):
- ATI Target = ATI Base × (ATI Resolution Factor / 16) = 100 × (64/16) = 400
- ATI Band(Large) = 1/8 × 400 = 50
- Re-ATI 경계 = 400 ± 50 = [350, 450]
- 현재 LTA = 394 → **ATI Band 내부** → Auto Re-ATI **미발동** (정상)

**Fast Filter Band 추가 분석** (DS §5.6):
- Fast beta 적용 조건: *"counts have drifted from the LTA in the sensing direction opposite to the normal sensing direction"*
- self-cap의 normal sensing direction = counts 감소(터치 시) = D 증가
- 반대 방향 = counts > LTA (D < 0) 영역
- D=39는 counts < LTA = normal sensing 방향이므로 Fast beta 적용 조건 **미충족**
- Fast beta(0xB2=0x0202)는 현재 동작하지 않는다

**결론**: D=39는 터치 판정용 값이며 ATI Band와 무관. Auto Re-ATI는 LTA가 [350,450] 밖으로 나갈 때 발동하므로 LTA=394에서 미발동이 정상.

---

### 의문 3: Auto ATI 안 돌면 ATI 에러가 나고 Re-ATI 해야 하지 않나

**정정 포인트**: ATI Error는 "ATI 알고리즘 실행 완료 시점에 Counts가 ATI Band 밖에 있을 때" 발생. D=39나 ATI 미발동과는 무관.

**DS §5.11 원문**: *"After the ATI algorithm completes, if Counts is outside the Re-ATI Boundary, the ATI Error bit is set."*
**DS §5.11 핵심**: *"After an ATI Error, the re-ATI is not automatically triggered. The master must set the Re-ATI bit."*

- ATI Error 발생 조건: ATI 알고리즘 **실행 완료 후**, 그 시점의 Counts가 [350, 450] 밖에 있을 때
- 현재 Counts = 355 ∈ [350, 450] → ATI 자체가 실행된 적 없고, 실행됐어도 Counts가 band 내부 → **ati_error = 0**
- SW Re-ATI 게이트(tdc_touch_logic.c L155~160): `NOT_TOUCH && !ati_active && ati_error && cooldown` → ati_error=0이므로 **미발동**

**Auto Re-ATI와 수동 Re-ATI 경로 정리**:
- 경로 A(자동): LTA가 [350,450] 밖으로 drift → IC 자동 Re-ATI (현재 미충족)
- 경로 B(수동): ATI Error 후 master가 Re-ATI bit set (현재 ati_error=0이므로 미발동)

**결론**: 현재 IC와 SW 모두 "정상 NOT_TOUCH + 정상 ATI 상태"로 인식하고 있다. ATI Error나 Re-ATI는 발생하지 않는 것이 정상 동작. LTA 비수렴만이 Prox 상태 진입에 의한 이상이다.

---

## 3. 사각지대 판정 — Not-Touch 약결합 LTA freeze

### IC 차원

**Channel Timeout (DS §5.8)**: *"If a channel has been in prox for longer than Event Timeouts register, it will be reseeded and exit that state."*

- 0xD2(Event Timeouts) 기본값 = 0x0000 → Prox Event Timeout = 0 = **disabled**
- 0xC0 MSB = 0x07 → CH0/CH1/CH2 Timeout Disable all **set**
- 두 조건 모두 timeout 비활성 → IC 차원의 자동 prox stuck 복구 **없음**

### SW 차원

stuck_eval() 함수(tdc_touch_logic.c L17~60):
```c
if (curr_state != TDC_TOUCH_STATE_TOUCH)  /* 노터치/해제 -> 리셋 */
{
    st->stuck_stage     = 0;
    st->stuck_anchor_ms = now_ms;
    return TDC_TOUCH_ACT_NONE;
}
```
- curr_state = NOT_TOUCH (D=39 < THR=156) → **즉시 TDC_TOUCH_ACT_NONE 반환**
- stuck 3단계(RESEED→Re-ATI→MCLR)는 TOUCH 상태 전용

Re-ATI 게이트(L155~160):
- ati_error=0 → 게이트 미발동

### 판정

| 복구 메커니즘 | 상태 | 이유 |
|---|---|---|
| IC Channel Timeout (prox) | 비활성 | 0xD2=0x0000 + 0xC0 MSB=0x07 |
| IC Auto Re-ATI | 미발동 | LTA=394 ∈ [350,450] |
| SW stuck_eval | 미동작 | curr_state != TOUCH |
| SW Re-ATI 게이트 | 미발동 | ati_error=0 |

**결론: 진짜 사각지대 확인.** IC도 SW도 "NOT_TOUCH 상태에서 Prox 고착으로 LTA가 동결되는 상황"을 흡수하는 메커니즘이 없다. 핀셋이 올려진 채로 수 분~수 시간 방치되면 LTA가 counts(355) 근처로 수렴하지 못하고 영구 baseline 오염 가능. 이후 실제 터치 시 D가 THR(156)에 빠르게 도달할 수 있어 민감도 이상, 또는 반대로 LTA가 장기 freeze 후 환경 drift를 반영 못해 false positive/negative 가능성.

---

## 4. 해결책 후보 적대적 평가

### (가) IC Channel Timeout 활성화 — prox timeout 설정

**구현**: 0xD2 LSB에 Prox Event Timeout 설정 (예: 10 → 10 × 512ms = 5.12초), 0xC0 MSB에서 CH0 Timeout Disable bit clear.

**효과**:
- D=39 상태가 Timeout 이상 지속 시 IC가 자동 Reseed + CH0_Prox clear
- LTA가 현재 counts(355)로 즉시 동기화되어 freeze 해제
- 이후 counts 변화 추적 정상화

**부작용·리스크**:
- DS §5.3 각주: *"채널 prox/touch timeout 사용 시 ULP 모드 금지."* → 현재 "Automatic No ULP" (0xC0 bits[6:4]=101) 이므로 ULP 차단 **이미 충족** — 충돌 없음
- PM Timeout=0(0xC5=0x0000)으로 자동 강하 자체가 차단되어 있어 절전 영향 없음
- Timeout 값이 너무 짧으면(예: 1 × 512ms = 0.5초) 정상 근접(손 흔들기, hover)을 Reseed로 처리해 baseline 흔들릴 수 있음 → 5~10초(10~20 × 512ms) 이상 권장
- Reseed 후 터치 민감도가 일시적으로 바뀔 수 있음(LTA가 약결합 Counts로 세팅됨) — 실제 터치 시 D가 작아져 판정 오류 가능성 (단, 핀셋 제거 후 일반 환경에서 LTA가 빠르게 복귀)
- 코드 변경 최소: `beta_power_settings()`에서 0xD2 write 1줄 추가 + 0xC0 MSB에서 CH0 bit만 clear

**DS 근거**: §5.8 원문 확인. Auto No ULP 조건 §5.3 확인.

---

### (나) SW "not-touch 약결합 감지" 로직 추가

**구현**: tdc_touch_logic.c에 "NOT_TOUCH 상태에서 D > (prox_level) 이상 N초 지속" 감지 시 RESEED 발행. Logic FSM 확장 필요.

**효과**:
- IC 독립적으로 SW에서 prox stuck 복구
- 임계·타이머를 SW로 튜닝 가능

**부작용·리스크**:
- tdc_touch_logic.c는 현재 "순수 함수(HW/timer 비의존)" 설계 원칙을 지킴 — D 값을 입력으로 받도록 tdc_touch_in_t 구조체 확장 필요, FSM 복잡도 증가
- "prox 수준 임계"를 SW에서 어떻게 정의할지 불명확 (IC 내부 Prox Threshold=0이므로 SW에서 별도 임계 선정 필요 — IC와 이중화 관리 부담)
- prox 감지 전용 RTT 계측값(D) 폴링이 필요하므로 read_debug() 상시 호출 필요 (현재는 계측용 옵션)
- (가)보다 구현 복잡도 높고, IC가 이미 Timeout 메커니즘을 내장하고 있음

---

### (다) Prox Threshold 명시 기록 → IC 수준 Prox 비활성화

**구현**: 0x61(Prox Settings)에 Prox Threshold=0xFF(또는 적당히 큰 값) 명시 Write → D가 255 이상이 아니면 Prox 미진입.

**효과**:
- Prox Threshold=0xFF이면 D ≥ 255 조건이어야 prox 진입 → D=39에서는 Prox 상태 미진입 → LTA freeze 원인 자체 차단
- LTA가 자유롭게 Counts를 추적하므로 약결합 상태에서 수분 후 D가 0에 수렴

**부작용·리스크**:
- Prox Threshold를 사실상 "무력화"하는 것 — 향후 prox 기반 기능(거리 감지, 절전 wake-up 등) 추가 시 방해
- D=39 상황에서 LTA가 Counts로 수렴하면, 이후 실제 터치 시에도 LTA가 이미 낮은 상태라 D가 크게 쌓이지 않을 위험(단, 정상 터치는 D가 100+ 이상이므로 THR=156 넘기 충분)
- "LTA가 counts를 흡수"하면 약결합 물체 존재 중에 새 baseline이 생성 → 물체 제거 시 D가 음수로 반전될 수 있음 (Invert 없이는 음수 D는 터치 미판정이므로 큰 문제 없음)
- (가)의 Channel Timeout보다 덜 정밀: 짧은 prox(예: 손 가까이 hover)도 LTA가 따라가 버려 hover 직후 터치 시 판정이 느릴 수 있음

---

### (라) 현행 유지

**효과**: 코드 변경 없음.

**부작용·리스크**:
- 약결합 물체(핀셋, 이물질, 손목받침)가 올려진 채 장시간 방치 시 LTA가 D=39 수준에서 영구 동결
- 환경(온도·습도) 드리프트를 LTA가 추적하지 못해 오랜 시간 후 실제 터치 임계가 틀어질 수 있음
- 제품 사용 시나리오가 "약결합은 절대 발생 안 한다"는 보장이 없는 한 리스크 미수용 수준

---

## 5. 권고

**권고: (가) IC Channel Timeout 활성화** — prox timeout만 설정, touch timeout은 선택적.

**근거**:
1. DS §5.8이 이 목적으로 설계한 메커니즘 — "물체가 오래 prox/touch 상태이면 Reseed + 상태 해제"
2. 현재 "Auto No ULP" 설정이 이미 ULP 금지 충족 → DS §5.8 각주와 충돌 없음
3. PM Timeout=0으로 LP 자동 강하 차단 상태 → 절전 정책 무변화
4. 코드 변경 최소: `beta_power_settings()` 내 `write_register(REG_EVENTS_ENABLE, ...)` 옆에 `write_register(0xD2, prox_timeout_value, 0x00)` 1줄 추가, `tdc_touch_iqs323_apply_settings()` 내 0xC0 MSB에서 CH0 Timeout Disable bit만 clear
5. (나) 대비 구현 복잡도가 낮고, IC 내장 메커니즘이라 타이밍 정확도 높음
6. (다) 대비 Prox 기능을 완전 파괴하지 않음

**Timeout 값 선택 권고**: Prox Event Timeout = 10 → 10 × 512ms ≈ 5초. 정상 사용에서 5초 이상 손을 hover하는 경우는 없으므로 false Reseed 위험 낮음. RTT 로그로 실기 확인 후 조정.

---

## 6. 실기 판별 방법

1. **prox 상태 확인**: System Status(0x10) MSB bit8(CH0_Prox)를 RTT 계측에 추가. D=39 상태에서 bit8=1이면 prox 진입 확인 → LTA freeze 가설 확정.

2. **Channel Timeout 동작 관찰**: 0xD2 LSB=10 설정 후, 핀셋 약결합 상태 유지. RTT에서 LTA가 5~6초 후 갑자기 Counts 근처(355)로 리셋되면 Timeout Reseed 동작 확인.

3. **Prox Threshold 실험**: 0x61에 Prox Threshold=0x50(80) Write 후 D=39 상태에서 LTA가 Counts로 수렴하기 시작하면 "Threshold=0이 freeze 원인" 가설 확정.

---

## 7. 참조

- DS §5.5: LTA freeze during prox events
- DS §5.7: Prox threshold = (LTA − Counts) > Prox Threshold; A.16(0x61) default=0x0000
- DS §5.8: Channel Timeout → Reseed + prox exit; ULP 금지 각주
- DS §5.10: Auto Re-ATI 발동 = LTA outside ATI Band [350,450]
- DS §5.11: ATI Error = ATI 완료 시 Counts outside band; Re-ATI 수동 트리거
- 코드: tdc_touch_iqs323.c `beta_power_settings()` — 0xD2 미기록, 0xC0 MSB=0x07
- 코드: tdc_touch_logic.c `stuck_eval()` — curr_state != TOUCH 즉시 NONE 반환
