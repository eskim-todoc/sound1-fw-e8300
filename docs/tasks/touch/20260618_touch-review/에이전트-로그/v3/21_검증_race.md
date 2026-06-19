---
name: 검증_race — 게이트·타이밍 결함 adversarial 공격
purpose: 11~18 계획 8종을 '게이트 race·타이밍 결함' 관점에서 공격해 동의가 아닌 결함을 발굴한다
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, race, timing, gate, adversarial, verification, v3]
---

# 21_검증_race — 게이트 race·타이밍 결함 adversarial 공격

**TL;DR**: 11~18 계획 8종을 race·타이밍 결함 관점에서 공격한 결과, 결정적 결함 3건(게이트 원자성 착각·RESEED 수렴 지연·크래들 Re-ATI I2C 순서)과 고위험 설계 허점 5건을 발굴했다. 계획들이 공통으로 가정하는 "counts 읽기 → RESEED 발행이 원자적"은 거짓이며, stable_count 기반 레이어_2는 race 창을 오히려 넓힌다.

---

## 0. 공격 방법론

각 계획 파일의 핵심 시퀀스를 추출하고, 다음 3개 공격 축으로 검토한다.

- **축_R**: Race window — 노터치 판별과 RESEED/Re-ATI 발행 사이에 터치가 끼어들 수 있는가
- **축_T**: Timing assumption — 타이밍 상수(delay·timeout·sample count)가 실측 없이 추정값으로 설계되었고, 그 오설정이 결함을 유발하는가
- **축_D**: Dependency break — 선행 조건(공장값·Beta·I2C 순서)이 깨질 때 폴백이 오히려 더 나쁜 상태를 만드는가

---

## 1. 11_계획_부팅시퀀스 — 결함 발굴

### 결함_11_A: Auto-ATI 채택 경로의 MULT/COMP 읽기 누락 [축_R, 결정적]

계획은 "노터치 확정 시 ATI_SETUP(Mode=Disabled)만 write, MULT/COMP는 Auto-ATI 산출값 그대로 유지"라고 설명한다(`§3-1` 채택 경로). 그러나 이 계획에서 **MULT/COMP를 명시적으로 읽어 검증하거나 저장하는 단계가 없다**.

Auto-ATI 완료 후 MCLR이 재발행되거나 IC가 부팅 노이즈로 레지스터를 재초기화한 경우, MULT/COMP 값이 POR 상태(초기값)로 돌아갈 수 있다. 계획이 "유지된다"고 가정하는 MULT/COMP의 유효성을 검증하는 코드가 없다.

**영향**: 채택 경로를 탔지만 실제 MULT/COMP가 POR값이면 보드편차 보정 효과가 없다. 더 나쁘게는 POR MULT/COMP와 현재 counts의 조합이 터치 판정 threshold 마진을 극히 좁힌 채로 동작할 수 있다 [추정: POR MULT/COMP 값 미확인].

**수정 방향**: 채택 경로 진입 시 `tdc_drv_iqs323_calib_read_ati()`로 MULT/COMP를 실제로 읽고 정상 범위 여부를 확인한 후 채택 여부를 결정해야 한다. 12번 계획의 `calib_read_ati()` 재활용이 이미 설계되어 있으나 11번 채택 경로에는 포함되지 않았다.

---

### 결함_11_B: RESEED 사후 검증의 race 창 역설 [축_R, 고위험]

계획 §2 다이어그램에서 `post_verify` 단계가 "RESEED 직후 counts 재확인 → 이탈 시 재시도 최대 3회"로 설계되어 있다. 그러나 **RESEED 발행(write_register)과 사후 counts 읽기 사이에는 반드시 I2C 왕복 지연이 존재한다**.

I2C 400kHz(Fast-mode) 기준, write_register 1회 = 약 0.2ms, 이후 read_register 1회 = 약 0.2ms. 총 약 0.4ms의 race 창이 존재한다. 사후 검증이 통과했더라도 이 0.4ms 사이에 터치가 시작되면 LTA는 이미 노터치 counts로 RESEED됐으므로 이 race는 사후 검증 이전에 무해하다.

**실제 결함**: race 창의 위험 방향이 반대다. 문제는 RESEED 발행 이전에 터치가 끼어드는 것이지, RESEED 이후 사후 검증 중이 아니다. 계획의 사후 검증은 "RESEED가 오염된 상태로 발행됐는지"를 확인하는 것이 아니라 "RESEED 후 터치가 시작됐는지"를 확인하는 것이다. 두 가지는 완전히 다른 정보다. RESEED 이후 터치가 시작된 것은 LTA는 올바르게 seed됐으므로 정상 동작(LTA IIR이 추적) 경우에 해당하며 race가 아니다.

**결론**: 사후 검증이 race 감지 목적에 적합하지 않게 설계됐다. 실제 race(판별~RESEED 사이 터치)는 사전에 막아야 하며 사후에는 감지 불가능하다.

---

### 결함_11_C: stable_count 레이어_2가 race 창을 확대함 [축_R, 고위험]

계획 14(노터치 게이트 코드)의 `TDC_TOUCH_NO_TOUCH_STABLE_SAMPLES=3`, `poll_ms=50ms` 설계가 11번 계획에 흡수된다. N=3, 50ms 간격이면 3번째 샘플 확인 후 RESEED가 발행되기까지 최소 100ms(두 번째~세 번째 샘플 간격)가 소요된다.

그런데 **마지막 샘플 읽기와 RESEED write 사이는 무방비 race 창**이다. stable_count 증가로 인해 소요된 100ms 동안 터치 여부는 계속 변할 수 있다. 3번째 샘플이 통과한 직후 터치가 시작되면 RESEED가 오염된다. N을 늘릴수록 race가 줄어드는 것이 아니라 **오히려 전체 게이트 통과 시간이 늘어나 race가 발생할 총 시간이 늘어난다**.

**더 나쁜 경우**: poll_ms=50ms는 [추정]이라고 명시되어 있다. 부팅 중 다른 초기화 루틴이 I2C 버스를 사용하면 실제 폴링 간격이 50ms를 훨씬 초과할 수 있다. 이 경우 game 창이 더 넓어진다 [확정 필요: 부팅 중 I2C 버스 경합 실측].

---

## 2. 12_계획_공장캘리저장 — 결함 발굴

### 결함_12_A: RESEED 완료 후 50ms 안정화 가정의 타이밍 결함 [축_T, 결정적]

계획 §4-3에서 "RESEED 완료 후 50ms 안정화 대기 후 noTouch Counts 읽기"라고 설계한다. 이 50ms는 [추정]으로 명시되어 있다.

**결정적 문제**: RESEED는 `write_register(0xC0, 0x08, 0x00)` write 성공 시 완료로 처리된다. 그러나 IQS323 내부에서 RESEED 비트를 처리하고 LTA를 실제로 갱신하는 시간은 I2C write 완료 시점과 다를 수 있다. 데이터시트 §5.5.1에서 RESEED 처리 완료를 알리는 별도 status bit가 있는지 확인되지 않았다 [확정 필요]. 50ms가 충분하다는 근거가 없다.

만약 실제 LTA 갱신에 50ms 이상이 걸린다면, 저장된 noTouch Counts는 RESEED 이전 LTA 상태를 반영한 counts가 될 수 있다. 공장값으로 저장되는 noTouch Counts의 신뢰도가 근본적으로 불확실하다.

---

### 결함_12_B: userSettingValue 구조체 확장 시 CFX 측 기존 필드 오프셋 파괴 위험 [축_D, 결정적]

계획 §3-1에서 `cfx_cm3_sharedMemory.h:89~98`의 `userSettingValue` 구조체 끝에 4개 `int` 필드를 추가하는 옵션_A를 권고한다. 문서 자체도 "CFX 파서가 구조체 크기를 하드코딩하면 필드 추가 시 오프셋 불일치 발생" 위험을 인정한다([확정 필요]).

**축_D 공격**: CFX 코드를 리뷰하기 전에 이 계획을 구현하면, CFX가 기존 7개 필드를 올바르게 읽지 못하게 되는 회귀가 발생할 수 있다. 특히 `mapNum`, `stimulVolume`, `audioVolume` 등 런타임 동작에 직결되는 필드가 파괴된다. 이는 touch baseline 문제보다 훨씬 심각한 회귀다.

계획이 "선행 확인 필수"라고 명시하고 있으나, Phase 2 구현 게이트로만 언급할 뿐 이것이 차단 조건임을 명확히 하지 않는다. 실제 구현 단계에서 빌드가드만 보고 Phase 2를 활성화하다가 이 선행 확인을 건너뛸 위험이 있다.

---

## 3. 13_계획_크래들훅 — 결함 발굴

### 결함_13_A: tdc_drv_iqs323_reseed_normal() 호출 후 I2C 완료 확인 없이 func_cradle_lid_closed_loop() 진입 [축_T, 고위험]

계획 §2-3에서 `led_force_fade_off()` 직후 `tdc_drv_iqs323_reseed_normal()` 호출 후 즉시 `func_cradle_lid_closed_loop()`를 호출한다. `reseed_normal()`은 `write_register(0xC0, 0x08, 0x00)` 1회 write 성공으로 완료된다.

**결함**: `func_cradle_lid_closed_loop()` 내부에서 IQS323 I2C 통신이 "완전 중단"된다고 전제하고 있다. 그런데 RESEED write 직후 IQS323이 내부 처리 중인 상태에서 `func_cradle_lid_closed_loop()`가 RDY 신호를 무시하거나 I2C를 비활성화하면, RESEED 처리가 중단되거나 I2C 버스가 지저분한 상태로 남을 수 있다 [추정: func_cradle_lid_closed_loop() 내 I2C 비활성화 메커니즘 미확인].

더 중요하게는, RESEED가 실제로 완료되지 않은 상태에서 뚜껑이 닫히면 목적(뚜껑 닫힘 직전 baseline 갱신)이 달성되지 않는다.

**수정 방향**: RESEED write 후 최소 IQS323의 measurement cycle(데이터시트 §5.2 NP mode report rate, 약 16ms [확정 필요]) 이상 대기하거나, RESEED 완료 status를 확인한 후 루프 진입을 허용해야 한다.

---

### 결함_13_B: Phase 2 Re-ATI ~1.5초 블로킹이 WDT timeout을 일으킬 수 있음 [축_T, 고위험]

계획 §3-2에서 `wait_re_ati_done()`이 "10회 폴링, 100ms×10=1초 상한"으로 설계되어 있고, Phase 2 Re-ATI가 ~1.5초 블로킹 허용이라고 명시한다. 이 블로킹 중 `SYS_WATCHDOG_REFRESH()`가 호출되는지 확인이 필요하다.

**공격**: `wait_re_ati_done()` 내부는 현재 `static` 구현이다(`iqs323.c:581`). 원래 어느 맥락에서 호출되도록 설계됐는지에 따라 WDT 갱신 코드가 없을 수 있다. Phase 2에서 이 함수를 `main.c:742` 맥락(단순 폴링 루프 밖)에서 호출하면, WDT reset 발생이 Re-ATI를 방해하고 의도치 않은 재부팅을 유발할 수 있다 [확정 필요: wait_re_ati_done() 내 WDT 갱신 여부].

---

## 4. 14_계획_노터치게이트코드 — 결함 발굴

### 결함_14_A: is_no_touch_window()의 overflow 미방어 [축_D, 중간]

계획 §2-2 코드에서:
```c
const uint16_t hi = base + margin;   /* overflow 가능성: counts 최대값 < 65535 전제 */
```
코드 자체에 주석으로 overflow 가능성이 인정되어 있다. `base=TDC_TOUCH_FACTORY_NO_TOUCH_COUNTS`(임시 450)와 `margin=80` 조합에서는 overflow가 없다. 그러나 Phase 2에서 실측 후 EEPROM 값을 로드할 때 손상된 EEPROM 값(예: `0xFFFF`)이 `base`로 로드되면 `base + margin`이 uint16_t overflow로 `79(0x004F)`가 되어 hi < lo 역전이 발생한다. 이 경우 `counts >= lo && counts <= hi`가 항상 false가 되어 게이트가 영구 닫힌다.

**더 심각한 경우**: hi < lo 역전 시 정상 noTouch counts도 게이트를 통과하지 못해 5000ms timeout 폴백이 매 부팅마다 발생한다. 부팅 시간이 5초 늘어나는 기능 퇴행이다.

**수정 방향**: `hi = base + margin` 계산 시 uint32_t로 올리고 Max Counts 상한과 함께 확인하거나, EEPROM 로드 시 범위 체크(12번 계획의 `COUNTS_MIN/MAX`)를 게이트 판별 함수 내에서 재확인해야 한다.

---

### 결함_14_B: 게이트 폴링 루프의 elapsed_ms 계산 오류 가능성 [축_T, 중간]

계획 §2-3 코드에서:
```c
int32_t tick_start = ci_timer_get_tick();
while ((ci_timer_get_tick() - tick_start) < (int32_t)poll_ms) { ... }
elapsed_ms += poll_ms;
```

`elapsed_ms`는 실제 소요 시간이 아니라 명목 poll_ms를 누적한다. 만약 `read_ch0_filtered_counts()`나 `is_no_touch_window()` 실행 시간이 poll_ms보다 길다면(I2C 재시도, 인터럽트 지연 등), 실제 경과 시간은 `elapsed_ms`보다 크다. 따라서 timeout 검사가 느슨해지고, 실제로는 5000ms를 훨씬 초과한 후에도 루프가 계속 실행될 수 있다.

역방향 문제: I2C 실패로 `read_ch0_filtered_counts()`가 즉시 false를 반환하면 루프가 poll_ms 대기 없이 매우 빠르게 반복되지만 `elapsed_ms += poll_ms`로 카운트되어, 실제 5000ms보다 훨씬 일찍 timeout 판정이 날 수 있다 [추정: I2C 실패 후 break 경로 확인 필요].

---

## 5. 15_계획_드리프트LTA — 결함 발굴

### 결함_15_A: Beta=0 POR default 가정이 잘못됐을 경우 write_filter_betas()가 드리프트를 파괴함 [축_D, 고위험]

계획 §1.3에서 "BETA write 0건 → POR default 0x0000"이라고 확정으로 처리하고 있다. 그러나 문서 자체 마지막 부분에서 "IQS323 데이터시트가 Beta POR default를 0이라 명시하는지 실측으로 교차 확인 필요 [확정 필요]"라고 인정한다.

**공격**: POR default Beta가 0이 아니라 이미 적절한 값(예: 4~8)으로 설정되어 있다고 가정해 보자. 이 경우 현재 펌웨어는 실제로 LTA IIR이 정상 동작 중이다. `write_filter_betas()`로 Beta를 새로 write하면 기존 동작을 변경하는 것이 된다. 잘못된 LSB/MSB 비트 레이아웃으로 write하면(계획에서 비트 레이아웃이 [확정 필요]로 남아 있음) 현재 동작하는 LTA 추적을 망가뜨릴 수 있다.

**Beta 레지스터 write 자체가 회귀 위험을 가진다**. 이 계획은 "현재 Beta=0이므로 반드시 설정해야 한다"는 전제에 기반하지만, 그 전제가 검증되지 않았다.

---

### 결함_15_B: LTA IIR freeze 중 Beta 설정의 무효화 [축_T, 중간]

계획 §1.1에서 "터치 또는 proximity 이벤트 중 LTA 갱신 중단(§5.5 확정)"이라고 명시한다. 그런데 부팅 직후 `s_boot_touch_ignore`가 활성 상태에서 터치가 인식되면, 상위 레이어에서 터치 이벤트가 억제된다. 그러나 **IQS323 IC 내부에서는 터치 이벤트가 발생하고 있으므로 LTA freeze가 진행된다**.

Beta를 설정해도 LTA freeze 중에는 Beta가 동작하지 않는다. `s_boot_touch_ignore` 5초 기간 동안 터치가 지속되면 Beta 설정 효과가 5초 이상 지연된다. 계획은 "Beta 설정 후 LTA가 정상 추적 중인 상태가 된다"고 설명하나, 이는 freeze가 없을 때만 성립한다.

---

## 6. 16_계획_사후검증 — 결함 발굴

### 결함_16_A: reseed_with_post_verify()가 FACTORY_NOTOUCH_COUNTS=0일 때 사후검증을 완전히 건너뜀으로써 race 감지 회로가 없음 [축_D, 결정적]

계획 §1-2 코드:
```c
#if (TDC_DRV_IQS323_FACTORY_NOTOUCH_COUNTS > 0u)
    /* 사후 counts 재확인 */
    ...
#else
    /* 공장값 미확정: 사후검증 없이 성공 반환 (현재 동작 보존) */
    return true;
#endif
```

계획이 "FACTORY_NOTOUCH_COUNTS=0 기본값으로 현재 동작 완전 보존"이라고 강조하는 것은 정확하다. 그러나 이 설계에서 **FACTORY_NOTOUCH_COUNTS가 0인 상태에서 Phase 1을 배포하고 실측을 미루면, race 감지 기능이 없는 채로 운영하는 기간이 무기한 연장된다**.

더 심각한 것은 이 함수의 이름이 `reseed_with_post_verify`인데, post_verify가 실제로 동작하지 않는다. 코드를 검토하는 다른 개발자가 함수명을 보고 사후 검증이 활성화된 것으로 오해할 수 있다. **이름과 동작의 불일치가 `write_and_verify` 문제(분석.md §5-4)와 동일한 패턴으로 재발하고 있다**.

---

### 결함_16_B: 재시도 루프에서 Sys_Delay() 사용이 WDT와 충돌할 수 있음 [축_T, 중간]

계획 §1-2 코드에서 재시도 간 딜레이:
```c
Sys_Delay(TDC_DRV_IQS323_RESEED_RETRY_DELAY_MS);
```

`Sys_Delay()`가 WDT를 갱신하는지 확인이 필요하다. 최대 3회 재시도이고 delay가 10ms이므로 WDT 관점에서 직접적 문제는 없다. 그러나 이 함수가 호출되는 맥락인 `apply_settings()`가 이미 MCLR+Auto-ATI 완료 후 타이트한 시퀀스에서 호출되는데, 거기에 최대 30ms(3회 × 10ms)의 추가 블로킹이 삽입된다. 이 30ms가 부팅 시퀀스의 타이트한 타이밍에 영향을 주는지 [확정 필요: apply_settings() 호출 맥락의 WDT 갱신 빈도].

---

## 7. 17_계획_방전트랙 — 결함 발굴

### 결함_17_A: 방전이 게이트의 전제 조건임에도 계획 순서에서 방전 트랙이 게이트보다 나중에 위치함 [축_D, 고위험]

계획 §5-1에서 "방전 트랙은 노터치 게이트 아키텍처의 전제 조건 중 하나다"라고 명확히 인정한다. ESD 누적이 심하면 counts가 noTouch 윈도우 이탈 → 게이트 영구 닫힘이다.

**공격**: 18번 통합 아키텍처 계획의 Phase 순서를 보면 Phase 0(포화 감지 stub) → Phase 1(ATI_Error 골격) → Phase 2(노터치 게이트) → Phase 3(Re-ATI) → Phase 4(크래들)로 진행된다. 방전 트랙의 실측1은 "단계_1: 실측1 수행"으로 언급되지만 Phase 번호에 포함되지 않는다. **실측1(방전 효과 검증)이 Phase 2(노터치 게이트) 활성화 전에 반드시 완료되어야 하는 명시적 블로킹 조건으로 설정되어 있지 않다**.

만약 실측1 완료 전에 Phase 2를 활성화하면, ESD 상황에서 게이트가 영구 닫히는 현상이 "게이트 구현 버그"로 오진될 위험이 있다.

---

## 8. 18_계획_통합아키텍처 — 결함 발굴

### 결함_18_A: S4_GATE_CHECK의 "레이어_3: 손뗌 급증 없음" 체크가 race 창을 역설적으로 늘림 [축_R, 결정적]

상태머신 §2-1에서 노터치 게이트가 3개 레이어로 구성된다:
- 레이어_1: counts ∈ factory±MARGIN
- 레이어_2: N샘플 분산 < σ_thr
- 레이어_3: 손뗌 급증 없음

레이어_2와 레이어_3는 추가 샘플을 수집하는 시간을 소비한다(14번 계획 기준 N=5, `TDC_NOTOUCH_N_SAMPLES=5`, 50ms × 5 = 250ms). 이 250ms 동안 counts가 안정적으로 노터치 범위에 있음을 확인한 직후, RESEED/Re-ATI를 발행하기 전까지의 순간이 가장 위험한 race 창이다.

**역설**: N이 클수록, 레이어가 많을수록, 게이트 통과에 더 많은 시간이 걸리고, 그만큼 사용자가 손을 올려놓다가 마지막 순간에 게이트를 통과시킬 확률은 줄어들지 않는다. 마지막 샘플이 통과한 직후 터치가 시작되면 RESEED가 오염된다. **게이트는 race를 제거하는 것이 아니라 race 직전 조건을 확인하는 것뿐이다**.

이 근본 한계를 계획들이 "race 치명도 낮음(저확률)"으로 처리하는 것은 타당하다. 그러나 레이어_2·3의 추가 샘플 수집이 race 확률을 실질적으로 낮추는지, 아니면 오히려 게이트 소요 시간 증가로 사용자가 게이트 중에 손을 갖다 댈 기회를 늘리는지 정량적 근거가 없다 [확정 필요].

---

### 결함_18_B: 빌드가드 TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE과 TDC_TOUCH_ATI_FULL_ENABLE 동시 활성 강제가 코드에서 강제되지 않음 [축_D, 중간]

계획 §3-1에서 "TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE=1 활성화 전 TDC_TOUCH_ATI_FULL_ENABLE=1 동시 설정 필수"라고 [!IMPORTANT]로 강조한다.

**공격**: 이 의존성이 코드 레벨에서 강제되지 않는다. 두 빌드가드가 별도 `#define`이므로 실수로 `REATI_ON_NOTOUCH_ENABLE=1, ATI_FULL_ENABLE=0`으로 빌드할 수 있다. 이 경우 Re-ATI가 발동되지만 ATI_Error 핸들러가 없어 ATI_Error 발생 시 영구먹통 경로가 열린다고 계획 자체가 경고한다.

**수정 방향**: `tdc_touch_config.h`에 컴파일 타임 assert 추가가 필요하다:
```c
#if TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE && !TDC_TOUCH_ATI_FULL_ENABLE
#error "REATI_ON_NOTOUCH requires ATI_FULL_ENABLE to be set"
#endif
```
이 1줄이 없으면 계획의 [!IMPORTANT] 경고는 코드 리뷰에서 놓칠 수 있다.

---

## 9. 공통 결함 패턴 요약

| 결함 ID | 파일 | 축 | 심각도 | 핵심 |
|---|---|---|---|---|
| 결함_11_A | 11번 | R | 결정적 | Auto-ATI 채택 경로에서 MULT/COMP 유효성 검증 없음 |
| 결함_11_B | 11번 | R | 고위험 | 사후 검증이 race 오염 방향을 잘못 감지함 (RESEED 이후 터치 시작 vs RESEED 전 터치) |
| 결함_11_C | 11, 14번 | R | 고위험 | stable_count 레이어_2가 race 창을 오히려 확대함 |
| 결함_12_A | 12번 | T | 결정적 | RESEED 완료 후 50ms 안정화가 추정값, RESEED 완료 status 미확인 |
| 결함_12_B | 12번 | D | 결정적 | CFX userSettingValue 확장 시 기존 필드 오프셋 파괴 위험 |
| 결함_13_A | 13번 | T | 고위험 | RESEED write 후 완료 미확인 상태로 IQS323 I2C 중단 루프 진입 |
| 결함_13_B | 13번 | T | 고위험 | Re-ATI 1.5초 블로킹 중 WDT 갱신 여부 미확인 |
| 결함_14_A | 14번 | D | 중간 | is_no_touch_window() hi 계산 uint16_t overflow 미방어 |
| 결함_14_B | 14번 | T | 중간 | elapsed_ms 명목 누적으로 실제 timeout 오차 |
| 결함_15_A | 15번 | D | 고위험 | Beta POR default 가정 미검증 상태에서 write_filter_betas() 적용 시 현재 동작 파괴 가능 |
| 결함_15_B | 15번 | T | 중간 | LTA freeze 중 Beta 무효화로 설정 효과 지연 |
| 결함_16_A | 16번 | D | 결정적 | FACTORY_NOTOUCH_COUNTS=0 시 함수명과 달리 사후 검증 완전 비활성 — write_and_verify 동일 패턴 재발 |
| 결함_16_B | 16번 | T | 중간 | Sys_Delay() 내 WDT 갱신 여부 미확인 |
| 결함_17_A | 17번 | D | 고위험 | 방전 실측1이 Phase 2 게이트의 명시적 블로킹 조건으로 설정되지 않음 |
| 결함_18_A | 18번 | R | 결정적 | 다중 레이어 게이트가 race 창을 역설적으로 늘릴 수 있음 |
| 결함_18_B | 18번 | D | 중간 | REATI+ATI_FULL 동시 활성 의존성이 코드 레벨에서 강제되지 않음 |

---

## 10. 핵심 발견 — 결정적 결함 3건

### 발견_1: 게이트의 원자성 착각 (결함_11_C, 결함_18_A)

모든 계획이 공통으로 가정하는 "N샘플 안정성 확인 후 RESEED"는 race를 제거하지 않고 race 발생 시각을 미룰 뿐이다. 마지막 샘플 통과 직후 RESEED write 발행 전까지의 구간(I2C 왕복 0.2ms 이상)은 원천적으로 race 가능하다. 레이어를 추가할수록 게이트 소요 시간이 늘어나 race 발생 기회 창이 오히려 넓어진다. 이 근본 제약을 인정하고 "race는 제거 불가, 치명도 낮음으로 수용"이라는 현재 방침은 옳으나, 레이어 추가가 race 확률을 낮춘다는 식의 서술은 정확하지 않다. 레이어 추가의 실익은 race가 아닌 "노이즈로 인한 오통과 방지"임을 명확히 해야 한다.

### 발견_2: RESEED 완료 status 미확인 (결함_12_A, 결함_13_A)

12번·13번 계획 모두 `write_register(0xC0, 0x08, 0x00)` write 성공을 RESEED 완료로 처리한다. IQS323의 RESEED 처리가 I2C write 완료와 동시에 완료되는지, 아니면 IC 내부에서 추가 처리 시간이 필요한지 데이터시트 확인이 없다. 이 가정이 틀리면 공장 캘리브레이션 데이터(12번)와 크래들 RESEED(13번) 모두 신뢰할 수 없는 기반 위에 세워진다.

### 발견_3: CFX 구조체 확장 회귀 (결함_12_B)

공장 캘리브레이션 EEPROM 저장(12번)이 전체 아키텍처의 선행 조건인데(18번 §1 [!IMPORTANT]), 그 저장 경로에 CFX 측 검토 없이는 실행 불가능한 회귀 위험이 있다. "확정 필요"로 표기하고 있으나, 이것이 Phase 2 전체의 블로킹 조건이라는 점이 Phase 계획 표(18번 §5)에 명시적으로 보이지 않는다. Phase 2 활성화 체크리스트에 "CFX 파서 크기 확인 완료"를 필수 항목으로 추가해야 한다.

---

## 11. 계획 반영 권고 (우선순위순)

| 우선순위 | 대상 계획 | 권고 |
|---|---|---|
| P0 | 12, 13번 | RESEED write 후 IC 내부 완료 타이밍 데이터시트 확인 — §5.5.1 "RESEED 처리 완료 status" 유무 파악 |
| P0 | 18번 | `tdc_touch_config.h`에 `#if REATI && !ATI_FULL → #error` 컴파일 타임 강제 추가 |
| P1 | 11번 | 채택 경로 진입 시 `calib_read_ati()`로 MULT/COMP 유효성 확인 후 채택 결정 |
| P1 | 12번 | Phase 2 활성화 체크리스트에 "CFX 파서 크기 확인 완료" 필수 항목 추가 |
| P1 | 17번 | 18번 Phase 계획에 "Phase 2 전 실측1(방전 효과) 완료"를 명시적 블로킹 조건으로 추가 |
| P2 | 14번 | `hi = base + margin` overflow 방어 코드 추가 (`uint32_t` 캐스트 또는 상한 clamp) |
| P2 | 16번 | `reseed_with_post_verify` 함수명을 동작에 맞게 교체하거나, FACTORY=0일 때 함수명에 "(stub)" 표기 추가 |
| P2 | 11, 14번 | 레이어_2·3의 실익을 "race 감소"가 아닌 "노이즈 오통과 방지"로 설명 수정 |
