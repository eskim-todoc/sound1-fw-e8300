---
name: 재검증1_race — 종합-1차 adversarial 공격 (race 관점)
purpose: 30_종합-1차.md를 race·타이밍·원자성 관점에서 공격해 새 약점·놓친 시나리오·미검증 주장을 발굴
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, adversarial, race, timing, atomicity, post-verify, sleep, reboot, v3]
---

# 42 — 재검증1(race 관점) adversarial 공격: 종합-1차 공격

**TL;DR**: 30_종합-1차를 race·타이밍·원자성 관점에서 공격한 결과, 사후 검증의 race 방향 역전(치명)·절전 RESEED→재부팅 사이 터치 유지 엣지(고위험)·MCLR POR 완전성 미검증(치명)·충전 중 Re-ATI 1초 블로킹 race 과소평가(고위험)·레이어_1 단방향 하한의 MARGIN_STRICT 값 설정 근거 순환 오류(중간)·크래들 뚜껑 열림 후 WDT 리셋 전 I2C 윈도우 상태 미고려(중간) 등 6개 신규 약점을 발굴했다. 기존 검증(21~28)이 이미 지목한 결함들은 종합-1차가 반영했으나, 반영 내용 자체에 새로운 결함이 생겨난 것이 핵심이다.

---

## 0. 공격 방법론

**페르소나**: race 관점 adversarial — 동의 없음, 결함만 발굴.

**입력**: 30_종합-1차.md 전체.
**검증 기준**: 종합-1차가 주장하는 설계·수정 내용이 실제 코드(`iqs323.c:1141~1158`, `main.c:807~867`, `main.c:869~952`)와 데이터시트 근거, 그리고 v1~v2·분석.md에서 확립된 사실과 정합하는지 확인한다.

**공격 축**:
- **축_R**: Race window — 종합-1차가 수용한 해결책이 실제로 race를 해소하는가, 아니면 race 방향을 잘못 인식하는가
- **축_A**: Atomicity assumption — "판별→RESEED"를 원자 블록으로 가정하는 설계가 있는가
- **축_U**: Unverified premise — "사실"이라고 처리했지만 아직 확정되지 않은 전제가 있는가
- **축_H**: Hidden race — 종합-1차가 새로 도입한 설계 자체가 기존에 없던 race 경로를 만드는가

---

## 1. 공격_1 — 사후 검증 메커니즘의 race 방향 역전 (치명, 축_R+축_A)

### 1-1. 종합-1차의 주장

`30_종합-1차.md §3-3 사후 검증 구현 명세`:
> "사후 검증의 목적: RESEED 발행 전 노터치 판별~발행 사이 race 검출"
> "사후 검증: RESEED 직후 counts를 재읽어 윈도우 이탈 시 → 재시도 필요(LTA IIR이 다시 오염됐으므로)"

### 1-2. race 방향 분석 (코드 기준)

RESEED(`write_register(0xC0, 0x08, 0x00)`) 발행이 완료되면 **LTA ← 현재 counts**가 IC 내부에서 즉각 수행된다[추정: RESEED 처리 타이밍 데이터시트 미확정]. 이후 사후 검증으로 counts를 재읽는 시점에 터치가 시작되면:

```
노터치 판별 → RESEED 발행 → LTA = noTouch counts (올바름)
  → 사후 검증 counts 읽기 시점에 터치 시작
  → counts < LTA → delta > 0 → 윈도우 이탈 가능성
```

**종합-1차의 판단**: "RESEED 직후 counts 재읽어 윈도우 이탈 시 재시도"

**치명적 오류**: 이 시나리오에서 LTA는 이미 올바른 noTouch 값으로 seed됐다. RESEED 이후 터치가 시작된 것은 **정상 동작 경로**다 — LTA = noTouch, 이후 터치 시 delta > threshold → 터치 인식, 손 뗌 시 LTA IIR 추적 재개. 이것은 race가 아니다.

**실제 race 방향**: 노터치 판별~RESEED **발행 사이**에 터치가 끼어드는 것이다. 이 race는 RESEED 완료 시점에 이미 LTA가 터치 counts로 오염된 후여서 **사후에 감지·정정이 불가능하다**. RESEED 발행 후 counts를 읽어 "윈도우 이탈"로 판단해도 그것이 "판별~발행 사이 race 때문"인지 "RESEED 후 정상 터치 시작"인지 구분할 방법이 없다.

### 1-3. 종합-1차가 도입한 재시도 로직의 실제 효과

종합-1차 §3-3: "재시도 3회, 10ms 딜레이"

RESEED 이후 counts가 윈도우 이탈(터치 상태)이면 재시도(RESEED 재발행)를 한다. 그런데:
- 터치 중 RESEED 재발행 → LTA ← 터치 counts로 **재오염**. 재시도가 오염을 고정한다.
- 이 동작은 기존 단순 RESEED보다 **더 나쁘다** — 단순 RESEED는 1회 오염이지만, 재시도 3회는 오염을 3번 확인하고 강화한다.

**종합-1차는 21_검증_race에서 발굴된 결함_11_B(사후 검증 race 방향 오류)를 §3-3에 "해소"라고 적었으나, 해소 내용 자체가 잘못된 race 방향을 전제한 채 재시도 로직을 설계하여 오히려 더 나쁜 결과를 만들었다.**

### 1-4. 올바른 설계 방향

사후 검증의 유일하게 의미 있는 역할은:
- RESEED 직후(~I2C 왕복 이내) counts를 읽어 **예상 noTouch 범위를 심하게 벗어나면** RTT 경보만 발행(재시도 없음). 이는 "판별~발행 사이 race 발생 추정"을 로깅하는 것이지 정정이 아니다.
- LTA IIR이 자기치유하도록 두는 것이 유일한 복구 경로다(종합-1차 §1-1 층위 3 — 자기치유가 이미 내장돼 있다고 주장한 것과 모순됨 없이).

**재시도 3회 로직은 즉시 제거해야 한다.** [확정 필요: 데이터시트 §5.5.1 RESEED 처리 완료 타이밍 확인 후 재설계]

---

## 2. 공격_2 — MCLR이 완전 POR임을 미확인한 채 "절전 파라미터 완전 리셋"을 전제로 설계한 오류 (치명, 축_U)

### 2-1. 종합-1차의 주장

`30_종합-1차.md §1-3`:
> "절전 복귀 = WDT 리셋 → 처음부터 MCLR+Auto-ATI (이전 LTA 완전 리셋)"

`00_오케스트레이터-입력-v3.md §1 전제 2`:
> "CM3 시작 시 MCLR — 사실 (`initialize.c:449` 무조건, `iqs323.c:288`)"

### 2-2. MCLR 완전 POR 주장의 검증 상태

코드 `iqs323.c:288~298` — `mclr_reset()`:
```c
Sys_DIO_Config(TDC_DRV_IQS323_RDY_PIN, TDC_DRV_IQS323_RDY_PIN_CFG_OUTPUT);
Sys_GPIO_Set_Low(TDC_DRV_IQS323_RDY_PIN);
Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * TDC_DRV_IQS323_MCLR_HOLD_MS);
Sys_DIO_Config(TDC_DRV_IQS323_RDY_PIN, TDC_DRV_IQS323_RDY_PIN_CFG_INPUT);
Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * TDC_DRV_IQS323_BOOT_WAIT_MS);
```

MCLR 핀을 Low로 내렸다가 다시 올리는 것이 확인된다. 그러나 **IQS323 데이터시트에서 MCLR이 모든 레지스터를 POR 상태로 완전 초기화하는지, 아니면 일부 레지스터(예: NVM에서 로드하는 값)가 보존되는지 코드 grep만으로 확정할 수 없다**.

절전 진입 시 `tdc_drv_iqs323_apply_sleep_settings()`가 THRESHOLD=255, HYSTERESIS=255, 절전 PROX_THRESHOLD, 고정 ATI 보상값을 IC에 write한다(`iqs323.c:967~1029`). 이 값들이 MCLR 후 POR default로 덮이는지, 아니면 NVM 기반 IC라면 잔존하는지 미확인이다 [확정 필요: 데이터시트 MCLR vs SW Reset 동작 비교].

**치명성**: 절전 파라미터(THRESHOLD=255)가 부팅 후 MCLR에도 잔존한다면, `write_ati_compensation()` 내 write가 ATI_SETUP·MULT·COMP만 덮고 THRESHOLD는 255로 남아 있는 상태로 부팅 게이트가 실행된다. THRESHOLD=255면 터치 감도가 사실상 없어져 noTouch counts가 게이트 윈도우 기준과 전혀 다른 값을 보일 수 있다.

### 2-3. 종합-1차의 처리

종합-1차 §5-1 블로킹 미지수에 MCLR POR 확인이 없다. 25_검증_엣지.md 결함_2에서 이미 지목됐으나, 종합-1차 §5-2 확인 항목에도 포함되지 않았다. **결정적 전제 검증이 빠졌다.**

---

## 3. 공격_3 — 절전 RESEED→WDT 리셋 사이 손 유지 터치 부팅 race (고위험, 축_R)

### 3-1. 시스템 전제 검토

`00_오케스트레이터-입력-v3.md §1 전제 3`:
> "복귀=WDT 리셋 재부팅 → 절전/부팅 baseline 문제 통합"

`main.c:869~883 func_sleep()`:
```c
while (tdc_drv_iqs323_read_status(&pressed, &ati_error) && pressed)
{
    ci_printv("[TOUCH] SLEEP: WAIT TOUCH RELEASE \r\n");
    SYS_WATCHDOG_REFRESH();
    delay_ms(100);
}
```
→ 절전 진입 직전 터치 해제 대기.

### 3-2. 놓친 race 경로

절전 진입 시 터치 해제 확인(`pressed==false` 대기) → `apply_sleep_settings()` → `ci_power_sleep()` → `tdc_drv_iqs323_reseed()` → RESEED 발행 → ULP 루프.

**race 경로**: `pressed==false` 확인과 RESEED 발행 사이에 사용자가 **다시 터치를 시작**할 수 있다. 이 사이에 `apply_sleep_settings()`(THRESHOLD=255, 약 5개 레지스터 write, 최악 ~650ms) + `ci_power_sleep()`(클럭 다운) + `i2c_set_master_prescale()`이 실행되는 동안 창이 열린다.

```
pressed==false 확인 → [최대 수백 ms 지연] → RESEED(LTA ← 현재 counts)
                               ↑
                      이 구간에 사용자 터치 재개 시 RESEED 오염
```

**종합-1차의 처리**: §1-3에서 "절전 진입 RESEED(`iqs323.c:931~948`)는 ULP 루프 전용 — 복귀 후 baseline에 무관"이라고 단순화. 이것은 WDT 리셋 후 재부팅 baseline에 무관하다는 점에서 옳다. **그러나 절전 진입 RESEED 오염이 ULP 루프 동작 품질에 영향 없다는 주장도 묵시적으로 포함되어 있다.**

**실제 위험**: ULP 루프에서 IQS323이 SLEEP_TOUCH_THRESHOLD(낮은 감도)로 동작하는 동안, RESEED 오염으로 LTA = 터치 counts라면 절전 중 터치 인식 루프가 즉시 터치 감지 → WDT 리셋 재부팅을 트리거한다. 즉 **절전 진입 직후 WDT 리셋 → 재부팅으로 이어지는 잘못된 "절전 탈출"**이 발생할 수 있다. 종합-1차는 이 경로를 분석하지 않았다.

---

## 4. 공격_4 — 충전 중 Re-ATI 1초 블로킹 race의 실제 확률 과소평가 (고위험, 축_R+축_U)

### 4-1. 종합-1차의 주장

`30_종합-1차.md §4`:
> 게이트 race: "✅ 충분히 낮은 확률 [추정]"

### 4-2. 충전 중 Re-ATI race의 실제 확률

Phase 3 충전 중 주기 Re-ATI(`R3_CHARGING`, 30초 주기)가 활성화되면 `wait_re_ati_done()` — 최대 1000ms 블로킹(`iqs323.c:581~623`, 10회×100ms)이 실행된다.

부팅 게이트 race: 게이트 통과(레이어_1 확인 ~수 ms) → RESEED 발행(~0.2ms). 총 race 창: **수 ms**.

충전 중 Re-ATI race: 노터치 게이트 통과 → `re_ati_trigger()` 발행 → `wait_re_ati_done()` 폴링 최대 1000ms. **race 창: 최대 1000ms**.

사람이 1000ms 내에 터치를 시작할 확률은 "극히 낮음"이 아니다. 충전 중 인공와우 착용자가 기기를 만지거나 크래들 근처에 손이 있는 상황에서 1초는 충분히 긴 시간이다. 브레인스토밍 §3 "게이트 race 제거 불가, 치명도 낮음"의 근거는 race 창이 "μs~수 ms"임을 전제했다.

**종합-1차는 부팅 게이트(수 ms race)와 충전 중 Re-ATI(1000ms race)를 동일한 "✅ 충분히 낮은 확률"로 처리했다.** 이는 두 시나리오의 race 창 차이(3자리 차이)를 무시한 것이다 [확정 필요: 충전 중 주기 Re-ATI race 확률 별도 평가].

**추가 결함**: Phase 3 충전 중 Re-ATI의 `wait_re_ati_done()` 내에 SYS_WATCHDOG_REFRESH() 없음 — 22_검증_회귀 결함_9에서 지목됐고 종합-1차 §2-1 Phase 4에서 "크래들 Re-ATI SYS_WATCHDOG_REFRESH() 필수"를 명시했으나, **Phase 3 충전 중 Re-ATI의 동일 문제는 Phase 계획에 명시되지 않았다.**

---

## 5. 공격_5 — MARGIN_STRICT 설정 근거의 순환 오류 (중간, 축_U)

### 5-1. 종합-1차의 주장

`30_종합-1차.md §3-2`:
> "`NOTOUCH_MARGIN_STRICT`(하한): Threshold(100)의 40% = 40 제안 [추정 — 실측_A 후 조정]"

그리고 §3-2 코드 주석:
```c
/* MARGIN_STRICT < Threshold/2: 부분 터치를 노터치로 오판 방지 (결함_1@25) */
```

### 5-2. 순환 오류 분석

25_검증_엣지 결함_1에서 정확히 지목된 문제는:
- 부분터치 counts = FACTORY_NOTOUCH - delta_partial (delta_partial < Threshold)
- 즉 부분터치는 **윈도우 하한보다 위**에 있다

MARGIN_STRICT = Threshold × 0.4 = 40으로 설정하면:
- 윈도우 하한 = FACTORY_NOTOUCH - 40
- 부분터치 counts 범위: FACTORY_NOTOUCH - Threshold(-1) = FACTORY_NOTOUCH - 99 이상
  (터치 판정을 "아직" 넘지 않은 부분터치는 delta < 100이므로 counts > FACTORY_NOTOUCH - 100)

즉 FACTORY_NOTOUCH - 99에서 FACTORY_NOTOUCH - 41 사이의 부분터치 counts(range: 59 units)는 MARGIN_STRICT=40 하한(FACTORY_NOTOUCH - 40)보다 낮다 → 게이트 실패. **이 범위의 부분터치는 차단된다.**

그런데 **FACTORY_NOTOUCH - 39에서 FACTORY_NOTOUCH까지의 부분터치**(delta = 0~39, range: 40 units)는 하한보다 높아 **게이트를 통과**한다.

**순환**: "MARGIN_STRICT < Threshold/2 = 50"이면 delta < MARGIN_STRICT인 부분터치(delta 0~MARGIN_STRICT-1)는 게이트를 통과한다. 이 부분터치들이 RESEED를 오염시키면 이후 손 완전 해제 시:
- LTA = FACTORY_NOTOUCH - delta_partial (예: 460)
- 손 해제 후 counts = FACTORY_NOTOUCH (예: 500)
- delta = 460 - 500 = -40 → **음수(noTouch로 인식) — 오류는 없다**

다시 계산: self-cap에서 터치 = counts 감소. 부분터치 시 counts < noTouch. RESEED → LTA = 부분터치 counts(더 낮은 값). 손 해제 → counts 상승(noTouch 방향) → counts > LTA → **delta 음수 → 노터치 인식 — 정상!**

**그런데 종합-1차 §3-2의 예시 코드는 다음과 같이 부분터치를 설명했다**:
> 결함_1@25: MARGIN=80이면 부분터치 counts가 윈도우 통과하여 LTA 오염

윈도우 통과 후 RESEED → LTA = 부분터치 counts. 이후:
- 손 해제 → counts 상승 (noTouch로 복귀) → counts = noTouch = LTA + delta_partial
- delta = LTA - counts = (부분터치 counts) - (noTouch counts) = -(delta_partial) < 0 → **노터치**

**결함**: 실제로 부분터치가 RESEED를 오염시키더라도 손 해제 후에는 LTA가 noTouch보다 낮은 상태 → 이후 delta 음수 = 노터치. 오인식은 없다. 오히려 위험한 방향은 **noTouch 쪽으로 LTA가 내려가** 이후 진짜 터치 시 delta가 더 커지는 것인데, 이는 감도 증가(더 예민해지는 방향)로 false touch 위험이 된다.

종합-1차 §3-2가 설명하는 "부분터치 RESEED 오염 → 터치 미인식"은 self-cap delta 방향을 반대로 계산한 것일 수 있다. [확정 필요: noTouch/touch 방향 counts 실측 — 터치 시 counts 감소 방향 확인]

---

## 6. 공격_6 — 크래들 뚜껑 열림 후 WDT 리셋 전 I2C 윈도우 상태 미고려 (중간, 축_H)

### 6-1. 코드 기준 크래들 열림 시퀀스

`main.c:850~857`:
```c
if (tdc_cradle_get_cover_state() == df_Connected)
{
    snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);
    delay_ms(20);   /* 로그 드레인 */
    SYS_WATCHDOG_RESET();
}
```

`func_cradle_lid_closed_loop()` 내부에서는 IQS323 I2C 통신이 **완전 중단**된다(터치 폴링 없음). 뚜껑 열림 패킷 감지 → `SYS_WATCHDOG_RESET()` 직전 `delay_ms(20)`이 실행된다.

### 6-2. 놓친 race 경로

Phase 4 크래들 Re-ATI가 구현되면 뚜껑 닫힘 루프 진입 직전에 Re-ATI를 실행하고 결과를 EEPROM에 저장한다(종합-1차 §2-1 Phase 4). 뚜껑 열림 후 WDT 리셋 → 재부팅 시 EEPROM 저장이 완료됐는지가 관건이다.

**신규 race 경로**: 뚜껑 닫힘 루프 내에서 EEPROM 저장이 완료되기 전(예: Re-ATI 완료 후 EEPROM write 도중) BLE 패킷으로 뚜껑 열림이 감지되면 → `delay_ms(20)` → `SYS_WATCHDOG_RESET()`. EEPROM write가 20ms 내에 완료되지 않으면 **불완전한 공장 캘리브 데이터가 EEPROM에 남는다**.

종합-1차 §2-1 Phase 4: "Re-ATI 실행 + 결과 EEPROM 저장 반드시 쌍으로 구현"은 명시됐으나, EEPROM write 완료 확인 전 WDT 리셋이 가능한 경로에 대한 보호가 없다.

**추가 문제**: `func_cradle_lid_closed_loop()` 진입 시 IQS323 I2C가 중단된다. 이 중단이 "폴링 호출 안 함"인지, 아니면 "I2C 하드웨어 비활성화"인지에 따라 Phase 4 Re-ATI 삽입 위치(루프 진입 전 vs 루프 안)의 유효성이 달라진다. 종합-1차 §2-1 Phase 4: "위치: main.c:742 앞, led_force_fade_off() 직후"로 루프 진입 전 삽입을 명시했으나, 루프 진입 이후 I2C 복원 경로에 Re-ATI 결과 저장이 연계되지 않는다 [확정 필요: func_cradle_lid_closed_loop() 내 I2C 상태].

---

## 7. 수렴 여부 판정

아래 항목들은 종합-1차가 기존 검증(21~28)의 결함을 올바르게 반영한 것으로 **수렴**으로 판정:

| 항목 | 종합-1차 반영 | 판정 |
|---|---|---|
| 레이어_2·3(N샘플) 제거 | §3-1 반영 완료 | 수렴 |
| 빌드가드 #error 컴파일 타임 강제 | §2-2 반영 완료 | 수렴 |
| CFX 구조체 수정 절대 금지(선행 확인 전) | §5-1 블로킹_C 반영 | 수렴 |
| calib_read_ati() 빌드가드 제약 | §2-1 Phase 2 반영 완료 | 수렴 |
| wait_re_ati_done() 1초 수정 | §2-1 Phase 3, §5-3 반영 완료 | 수렴 |
| 크래들 Phase 1 dead code 폐기 | §2-1 Phase 4 반영 완료 | 수렴 |
| FACTORY_NOTOUCH_COUNTS 단일 소스 0값 기본 | §2-2 반영 완료 | 수렴 |
| 삽입점 충돌(11↔14) 해소 | §2-3 반영 완료 | 수렴 |

---

## 8. 신규 약점 요약표

| # | 약점 | 축 | 심각도 | 종합-1차 위치 |
|---|---|---|---|---|
| 공격_1 | 사후 검증 race 방향 역전 — 재시도 3회가 오히려 오염 고착 | R+A | **치명** | §3-3 |
| 공격_2 | MCLR 완전 POR 미확인 → 절전 파라미터 잔존 시 게이트 counts 기준 붕괴 | U | **치명** | §1-3, §5-1 누락 |
| 공격_3 | 절전 진입 RESEED→ULP 루프에서 터치 oops 재부팅 경로 미분석 | R | **고위험** | §1-3 단순화 |
| 공격_4 | 충전 중 Re-ATI 1000ms race를 부팅 ms race와 동일 "저확률" 처리 | R+U | **고위험** | §4 |
| 공격_5 | MARGIN_STRICT 부분터치 방어 방향 계산 오류 가능성 | U | **중간** | §3-2 |
| 공격_6 | 크래들 열림 WDT 리셋 전 EEPROM 저장 완료 보장 없음 | H | **중간** | §2-1 Phase 4 |

---

## 9. 권고 (우선순위순)

| 우선순위 | 대상 | 권고 |
|---|---|---|
| P0 | §3-3 사후 검증 | 재시도 3회 로직 제거. 사후 검증은 RTT 경보만 발행하고 LTA IIR 자기치유에 위임. RESEED 이후 counts 재읽기는 오염 정정 메커니즘이 아님을 설계 노트에 명시. |
| P0 | §5-1 블로킹 미지수 | 블로킹_G 신규 추가: "IQS323 MCLR이 모든 레지스터를 완전 POR하는지 vs 절전 레지스터 일부 보존되는지" — 데이터시트 §3.4 HW Reset 확인, 필요시 MCLR 후 THRESHOLD 레지스터 read 실측. Phase 1 착수 전 필수. |
| P1 | §1-3 절전재부팅통합 | "절전 진입 RESEED 오염 → ULP 루프 즉시 WDT 리셋 재부팅" 경로 추가 분석. func_sleep() pressed==false 대기 ~ RESEED 발행 사이 race 창(수백 ms) 명시. |
| P1 | §4 달성도 게이트 race | 부팅 게이트(race 창: 수 ms)와 충전 중 Re-ATI(race 창: 최대 1000ms)를 구분해 별도 달성도 평가. Phase 3 `wait_re_ati_done()` 내 SYS_WATCHDOG_REFRESH() 필수 명시(Phase 4에만 있는 현 상태 교정). |
| P2 | §3-2 MARGIN_STRICT | self-cap에서 부분터치 RESEED 오염 후 손 해제 시 delta 방향 재계산 확인. "터치 미인식"이 아닌 "false touch 감도 증가" 방향인지 확정 후 MARGIN_STRICT 의미 재정립. |
| P2 | §2-1 Phase 4 | EEPROM write 완료 확인 후 뚜껑 열림 허용 또는 원자 write 보장 방법 명시. |

---

## 10. 핵심 6줄

1. **사후 검증 race 역전(치명)**: 종합-1차 §3-3의 "RESEED 직후 counts 재확인 → 재시도 3회"는 race 방향이 반대 — RESEED 이후 터치는 정상 동작이며 재시도는 오염을 고착시킨다. 즉시 제거하고 RTT 경보만 발행으로 교체해야 한다.
2. **MCLR POR 완전성 미검증(치명)**: 종합-1차가 "이전 LTA 완전 리셋"을 확정으로 처리했으나 절전 중 write된 THRESHOLD=255 등이 MCLR 후 잔존하면 부팅 게이트 counts 해석 전체가 틀어진다 — 블로킹_G로 Phase 1 착수 전 실측 게이트 추가 필수.
3. **절전 RESEED→ULP 오염 재부팅 경로(고위험)**: 절전 진입 시 pressed==false 확인 후 수백 ms 뒤 RESEED 발행 사이 터치 재개 시 LTA 오염 → ULP 루프 즉시 터치 감지 → WDT 리셋 재부팅 무한 반복 경로를 종합-1차가 "통합 단순화"로 무시했다.
4. **충전 중 Re-ATI 1000ms race 과소평가(고위험)**: 부팅 게이트 race 창(수 ms)과 달리 충전 중 Re-ATI는 최대 1000ms 창 — "충분히 낮은 확률"은 부팅 게이트에만 성립하며, Phase 3 내 SYS_WATCHDOG_REFRESH() 누락도 동시 지적 필요.
5. **MARGIN_STRICT 부분터치 방향 오류(중간)**: 종합-1차 §3-2의 "부분터치 RESEED → 터치 미인식" 설명은 self-cap delta 방향 계산 오류 가능성 — 실제로는 "false touch 감도 증가" 방향일 수 있어 MARGIN_STRICT 설계 근거 재확인 필요.
6. **크래들 열림 EEPROM 경쟁(중간)**: Phase 4 Re-ATI 결과 EEPROM 저장 도중 뚜껑 열림 BLE 패킷 → `delay_ms(20)` → WDT 리셋 시 저장 불완전 — EEPROM write 원자성 보장 또는 write 완료 후 열림 허용 설계 필요.
