---
name: 재검증2_race — 49_종합-재검증1 adversarial 공격 (race 관점)
purpose: 49_종합-재검증1을 race·타이밍·원자성·미검증 주장 관점에서 공격해 새 약점·놓친 시나리오·할루시네이션·미검증 주장을 발굴
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, adversarial, race, timing, atomicity, revalidation, round2, v3]
---

# 52 — 재검증 라운드2 (race 관점) adversarial 공격: 49_종합-재검증1 대상

**TL;DR**: 49_종합-재검증1을 race·타이밍·원자성 관점에서 공격한 결과, 수렴 선언된 항목 4건이 실제로 미해소이거나 새 race 경로를 내포하고 있음을 발굴했다. 치명 2건(절전 RESEED 오염→ULP 즉시 재부팅 루프 경로가 49에서 여전히 단순화됨·비블로킹 R3_CHARGING 설계가 race를 이전보다 복잡하게 만드는 신규 경로 생성), 고위험 2건(TDC_TOUCH_RTT_TUNING 분기와 FACTORY_NOTOUCH_COUNTS 기준이 동기화 불가능한 race 조건 내포·EEPROM 로드 경로 설계가 MCLR→ATI→apply_settings 순서와 충돌하는 race 생성), 중간 2건(s_boot_touch_ignore 해제 조건과 노터치 게이트 완료 시점의 race·wait_re_ati_done 내 ATI_Error 감지 후 re_ati_trigger 재귀 경로가 49에서 처리되지 않음). 직전 수렴 선언 10개 항목 중 5개에 새 결함이 숨어 있다.

---

## 0. 공격 방법론

**페르소나**: race 관점 adversarial — 동의 없음, 결함만 발굴.

**입력**: 49_종합-재검증1.md 전체 + 코드 ground truth(iqs323.c·main.c·tdc_touch_config.h 직독 확인).

**공격 기준**:
- 49가 "수렴"으로 선언한 항목이 실제 코드에서 race를 은닉하고 있는가
- 49가 "치명 6건 반영"이라고 한 수정 내용이 새 race 경로를 생성했는가
- 49가 제시한 정제 설계 코드 스니펫이 race 안전성을 갖추는가

**공격 축**:
- **축_R**: race window — 해소됐다고 선언한 race가 실제 코드 순서에서 여전히 열려 있는가
- **축_G**: ghost race — 49가 도입한 새 설계 자체가 기존에 없던 race 경로를 만드는가
- **축_U**: unverified premise — 확정됐다고 처리했으나 실제로는 추정인 전제를 근거로 race 가 없다고 단정하는가
- **축_S**: sequencing — apply_settings 내 레지스터 write 순서와 새 설계 코드의 삽입점이 race 없이 정합하는가

---

## 1. 공격_1 — 절전 RESEED 오염→ULP 즉시 터치 감지→재부팅 루프가 49에서 여전히 단순화됐다 [치명, 축_R]

### 1-1. 49의 처리

49_종합-재검증1 §1-2 "절전/부팅 통합 유지(조건부 확정)":
> "절전 복귀 = WDT 리셋 재부팅 → MCLR → Auto-ATI → 노터치 게이트."
> "단: MCLR이 IC LTA 레지스터를 포함한 전 레지스터를 POR 초기화하는지 데이터시트 §4.1/§5.5 미확인 → 블로킹_G(신규)"

블로킹_G를 추가하는 것으로 절전→재부팅 경로의 race를 '조건부 확정'으로 처리했다. **그러나 MCLR 완전 POR 확인 문제와 별개로, 절전 RESEED 오염→ULP 루프 즉시 재부팅 경로 자체에 대한 분석이 49에서 누락됐다.**

### 1-2. 코드 기준 실제 경로 (직독 확인)

```
main.c:877~882 — func_sleep() pressed 대기 루프
main.c:944     — tdc_drv_iqs323_apply_sleep_settings()
main.c:948     — ci_power_sleep() (SYSCLK 30.72M→2.56M)
main.c:950     — i2c_set_master_prescale()
main.c:952     — tdc_drv_iqs323_reseed()
                     → iqs323.c:934~948: touch_settings_impl(SLEEP_THRESHOLD, SLEEP_HYSTERESIS)
                                         prox_settings(SLEEP_PROX_THRESHOLD)
                                         write_register(0xC0, 0x08, 0x00)  ← RESEED
```

pressed==false 확인(main.c:877) 후 RESEED 발행(main.c:952)까지 **apply_sleep_settings + ci_power_sleep + i2c_set_master_prescale 순서**가 실행된다. 이 구간에서 사용자가 터치를 재개하면 RESEED 시 LTA=터치 counts.

RESEED 후 ULP 폴링 루프(main.c:958~)에서 IQS323은 SLEEP_TOUCH_THRESHOLD(낮은 감도)로 동작한다. LTA=터치 counts 상태에서 손이 계속 올려져 있으면 **counts < LTA → delta > SLEEP_THRESHOLD → 즉시 터치 감지 → WDT 리셋 재부팅**.

재부팅 후 MCLR → auto-ATI → 노터치 게이트. 이 시점에도 손이 올려져 있으면 게이트 실패 → (GATE_ENABLE=0 기본값으로) 무조건 RESEED 폴백 → 재부팅 직후 또 터치 감지 → **무한 재부팅 루프**.

### 1-3. 49의 미처리

42_재검증1_race 공격_3에서 이 경로를 "고위험"으로 발굴했고, 49 §8 미해결 표에는 없다. §1-2 "절전/부팅 통합 유지 (조건부 확정)" 선언이 이 경로를 암묵적으로 해소한 것으로 처리했으나 실제로는 분석하지 않았다.

**GATE_ENABLE=0(기본값, Phase 1 미활성) 환경에서 이 루프는 현재도 재현 가능하다** — 이것은 Phase 4 가상 시나리오가 아니라 현재 코드의 실 결함이다. 49가 "블로킹_G 실측 후 처리"로 미룬 것은 무관한 블로킹을 핑계로 삼은 것이다.

**요구 조치**: 절전 진입 RESEED~ULP 루프의 "오염 LTA → 즉시 재부팅" 경로를 §8 미해결/원리상 해소 불가 표에 명시 추가. Phase 1 노터치 게이트가 **절전 진입 RESEED에도 적용돼야 하는지** 별도 분석 필요. 현재 49 계획에는 절전 진입 RESEED 게이트화가 없다.

---

## 2. 공격_2 — 비블로킹 R3_CHARGING 설계가 ATI_Error 핸들러 레이어와 신규 race를 만든다 [치명, 축_G]

### 2-1. 49의 처리

49 §2-4 R3_CHARGING 명세:
```
R3_CHARGING: df_Connected=true 상태에서 30초 주기 Re-ATI
  트리거 위치: tdc_touch_process() 내 타이머
  구조: 비블로킹 선호 — re_ati_trigger() 발행 후 다음 폴링에서 ATI_Active 확인
        (블로킹 wait_re_ati_done()은 1초 race 창으로 사용 금지)
  노터치 게이트: 30초 주기 도달 시 현재 counts로 게이트 통과 확인 후 트리거
```

### 2-2. 비블로킹 Re-ATI의 신규 race 경로

비블로킹 구조(trigger → 다음 폴링 ATI_Active 확인)는 **ATI 진행 중 폴링 주기(200ms) 동안 정상 터치 판정이 계속된다**. 코드 현황:

- `tdc_touch_process()`는 매 200ms 호출 → counts 읽기 → 터치 판정 → discharge_crx0()
- Re-ATI 비블로킹 트리거 후 ATI 수행 중(약 1~1.5초) 다음 3~7회 폴링이 실행됨
- ATI 수행 중 IQS323은 데이터시트 §8.4에 따라 "I²C disabled during ATI" [추정: 데이터시트 §8.4 정확한 문구 확인 필요]
- 폴링 counts 읽기 → force_window_open 실패 → 0xEEEE 반환 → 터치 판정 무효값 처리 필요

**신규 race**: ATI 진행 중 폴링에서 counts 읽기가 실패(I²C 응답 없음)하면, 49 §2-2에서 "NOT_TOUCH 명시 강제" 보완_4가 적용돼야 하지만, R3_CHARGING 비블로킹 설계에는 **ATI 진행 중 폴링 결과 처리 로직이 없다**. 49 §4 Phase 3 명세 어디에도 "Re-ATI 진행 중 폴링 결과 = NOT_TOUCH 강제"가 명시되지 않았다.

더 심각한 경로:
```
R3_CHARGING 트리거(200ms 폴링 N번째) → re_ati_trigger() → ATI 시작
200ms 후 폴링 N+1번째 → ATI_Active 아직 true → "ATI 진행 중" 상태 유지
                        → 이 폴링에서 터치 시작 → counts 읽기 = I²C 무응답 [추정]
                        → 이전 last_state 반환 or 0xEEEE 처리 분기 필요
200ms 후 폴링 N+2번째 → ATI 완료 → ATI_Error 없음 → 정상 Re-ATI 성공
                       → 그러나 Re-ATI 수행 중 터치 상태의 물리적 정전용량으로 게인 수렴됨
                       → MULT/COMP가 터치 게인 기준으로 왜곡된 채 고정
```

이 경로는 1초 blocking race 창을 비블로킹으로 분산시켰지만, **실제 race를 분산 폴링 구간 전체로 확산**시켰다. 치명도가 낮아진 게 아니라 감지하기 더 어려운 형태로 변환됐다.

**49의 미처리**: 49 §2-4 R3_CHARGING 명세에 "비블로킹 구조 선호"라고만 명시하고, ATI 진행 중 폴링 결과 처리·터치 중 Re-ATI 완료 시 왜곡된 게인 저장 방지 방법이 없다.

**요구 조치**: 비블로킹 R3_CHARGING 설계에 "ATI 진행 중 플래그"를 추가해, 플래그 활성 기간 동안 터치 판정 결과를 NOT_TOUCH 강제하고 ATI 완료 후 게이트 재확인(현재 counts가 게이트 통과하는지)을 수행하는 3단계 구조 명세 필요. 없으면 비블로킹 전환이 blocking race를 "분산 오염 race"로 변환할 뿐이다.

---

## 3. 공격_3 — TDC_TOUCH_RTT_TUNING 분기와 FACTORY_NOTOUCH_COUNTS가 동기화 불가능한 race 조건을 내포한다 [고위험, 축_U]

### 3-1. 49의 처리

49 §9(수렴도) 잔존 미해결 표:
> "신규_TDC_RTT_TUNING: TDC_TOUCH_RTT_TUNING 분기와 공장값 게이트 신뢰도 충돌 → 고위험"

**고위험으로 분류만 했고 분석·요구 조치가 없다.** 45_재검증1_회귀 회귀_2에서 발굴한 이 결함이 49에서 단 한 줄 표 항목으로만 처리됐다.

### 3-2. 코드 기준 실제 충돌 경로 (직독 확인)

`iqs323.c:705~730 write_ati_compensation()`:
```c
#if TDC_TOUCH_RTT_TUNING
    uint8_t m_l = s_tdc_normal_tuning.ati_valid
                  ? s_tdc_normal_tuning.mult_lsb
                  : TDC_DRV_IQS323_ATI_MULT_LSB;
    ...
#else
    write_register(..., TDC_DRV_IQS323_ATI_MULT_LSB, TDC_DRV_IQS323_ATI_MULT_MSB);
#endif
```

**RTT_TUNING=1 + ati_valid=true** 환경:
1. 공장 캘리브레이션 시 MULT/COMP=RTT 튜닝값으로 게인 설정 → counts 측정 → FACTORY_NOTOUCH_COUNTS 저장
2. 이후 RTT 세션 종료 or ati_valid 리셋 → 다음 부팅에서 ati_valid=false → 컴파일 타임 상수 MULT/COMP 적용
3. FACTORY_NOTOUCH_COUNTS는 RTT 튜닝 게인 기준 counts. 지금은 다른 게인(컴파일 타임 상수)이 적용됨
4. 노터치 게이트: 현재 counts(다른 게인 기준)를 FACTORY_NOTOUCH_COUNTS(RTT 게인 기준)와 비교 → **게이트 기준 불일치**

이것은 race window가 아닌 **게인 기준 동기화 race** — MULT/COMP가 런타임 조건에 따라 달라지는 비결정론적 상태에서 공장 기준값과 현재 counts의 비교가 무의미해지는 구조적 결함이다.

**49에서 미처리된 이유**: 이 결함은 Phase 1 착수 전에 `TDC_TOUCH_RTT_TUNING=0`을 확정하거나 FACTORY_NOTOUCH_COUNTS를 RTT 튜닝 환경과 비-RTT 환경 별도 저장으로 분리하지 않으면 해소 불가다. 49는 이 해소 방법을 제시하지 않았다.

**요구 조치**: Phase 1 착수 전 블로킹 조건으로 "TDC_TOUCH_NOTOUCH_GATE_ENABLE=1과 TDC_TOUCH_RTT_TUNING=1의 동시 활성 금지 #error 추가"를 명시. 게이트 신뢰도가 RTT 튜닝 상태에 의존하는 한 공장 캘리브레이션 절차에서 반드시 RTT_TUNING 상태를 명문화해야 한다.

---

## 4. 공격_4 — EEPROM 로드 경로 설계가 MCLR→ATI→apply_settings 순서와 race를 일으킨다 [고위험, 축_S]

### 4-1. 49의 처리

49 §2-3 EEPROM 로드 경로:
```c
/* tdc_drv_iqs323_apply_settings() 내 수정 흐름 */
#if TDC_TOUCH_FACTORY_CALIB_ENABLE == 0
  if (tdc_touch_calib_load_eeprom(&calib)) {
    s_mult_lsb = calib.mult_lsb;
    s_comp_lsb = calib.comp_lsb;
  }
#endif
write_ati_compensation();
```

### 4-2. 순서 race 분석

실제 `tdc_drv_iqs323_apply_settings()` 내 현재 순서 [추정: 코드 직독 필요, 전체 함수 구조 미확인]:
1. MCLR → auto-ATI(비블로킹) → ATI 완료 대기
2. `write_ati_compensation()` 호출 (ATI_SETUP write → MULT/COMP write)

49 §2-3 설계는 `tdc_drv_iqs323_apply_settings()` 내에서 EEPROM 로드 후 `write_ati_compensation()`을 호출한다. 그런데:

**auto-ATI 완료 시점 race**: MCLR 후 auto-ATI가 수행되면 IC가 내부적으로 MULT/COMP를 재산출한다. `write_ati_compensation()` 호출 시 이 auto-ATI 결과가 IC 레지스터에 있는 상태에서 EEPROM 로드값(또는 컴파일 타임 상수)으로 **덮어쓴다**. 이것이 현재 동작의 의도이고 원리적으로 문제없다.

**신규 race 경로(EEPROM 로드 경로 추가 후)**:
```
부팅 → MCLR → auto-ATI 완료
     → apply_settings() 진입
       → EEPROM 로드 시도 (I²C read — EEPROM 접근)
       → EEPROM 접근 실패 or 유효 데이터 없음 → 컴파일 타임 상수 폴백
       → write_ati_compensation() (ATI_SETUP=Disabled + MULT/COMP write)
       → RESEED 게이트 (GATE_ENABLE=0이면 무조건 RESEED)
```

EEPROM 접근이 실패할 때 폴백 동작이 49 §2-3 코드 스니펫에는 `/* else: 컴파일 타임 고정 상수 */` 한 줄로만 처리됐다. **EEPROM 접근 실패 후 컴파일 타임 상수로 폴백하면 이전에 EEPROM에 저장된 공장 보정값이 무시된다.** 폴백 경로에서 RTT 경보 없이 조용히 다른 게인을 적용하는 것은 현장 디버깅 불가 상황을 만든다.

더 중요한 순서 문제: **EEPROM 로드를 언제 시도하는가?** EEPROM 접근은 CFX와 I²C 버스를 공유할 수 있다 [추정: 블로킹_C(CFX 파서 크기 확인) 필요 이유]. CFX가 완전히 초기화되기 전(부팅 초기 타이밍)에 EEPROM 접근이 시도되면 버스 충돌 race가 발생할 수 있다.

**49의 미처리**: §2-3 EEPROM 로드 경로 설계에 "접근 실패 RTT 경보", "CFX 초기화 완료 후 접근 시점 보장", "타임아웃 처리"가 없다. 이 세 가지가 없으면 EEPROM 로드 경로가 조용한 폴백으로 운용 중 감지 불가 상태로 동작할 수 있다.

**요구 조치**: EEPROM 로드 경로에 (1) 접근 실패 RTT 경보 필수, (2) CFX 초기화 완료 시점과 EEPROM 접근 순서 의존 명시, (3) 유효 데이터 마커(magic number/CRC) 검증 후 로드 구조 명시.

---

## 5. 공격_5 — s_boot_touch_ignore 해제 조건과 노터치 게이트 완료 타이밍의 race [중간, 축_R]

### 5-1. 49의 처리

49 §4 Phase 1 "기존 s_boot_touch_ignore와의 공존":
> "두 가드 역할 분리: 노터치 게이트는 RESEED 시점 사전 방어, s_boot_touch_ignore는 READY 전이 후 폴링 이벤트 사후 억제. 상보적으로 동작."
> "충돌 시나리오: 게이트 통과 후 올바른 RESEED 완료 → s_boot_touch_ignore 여전히 활성(직전 READY 전이 시 터치 이벤트 있었다면) → 터치 억제 구간 발생 가능. Phase 1 구현 시 상호 해제 조건 정합성 확인 필요 (확인_10 신규)."

"확인_10 추가"로 처리했다. **그러나 race 구체 시나리오를 분석하지 않았다.**

### 5-2. 실제 race 경로

```
[부팅] MCLR → auto-ATI → apply_settings() 진입
  → 노터치 게이트 확인(counts 읽기) → 통과 → RESEED(LTA = noTouch counts) ← 올바름
  → READY 전이 발생(apply_settings 완료 후 tdc_touch.c에서 state=READY 전이)
  → READY 전이 시점에 사용자 터치 이벤트 있으면 s_boot_touch_ignore = true
  → 이후 폴링에서 터치 있어도 s_boot_touch_ignore=true 상태이면 이벤트 억제
  → s_boot_touch_ignore 해제 조건: 터치가 해제되면(NOTOUCH 폴링) false로 리셋
```

**race**: READY 전이가 터치 상태에서 발생하면 `s_boot_touch_ignore=true` → 노터치 게이트가 올바르게 RESEED를 완료했어도 사용자가 터치를 해제하고 다시 터치해야 인식된다. 이것은 "올바른 RESEED 후에도 터치 무응답"의 정상적으로 재현되는 race다.

더 나쁜 경로: 노터치 게이트 통과 후 RESEED → READY 전이 → 터치(s_boot_touch_ignore=true) → 터치 해제 → `s_boot_touch_ignore=false` **그러나 LTA가 이 시간 동안 IIR로 이동했으면 초기 RESEED 값과 달라졌을 수 있다** → 다음 폴링 시 게이트 기준과 다른 LTA로 동작.

**49의 미처리**: "확인_10 추가"만으로 처리하고 해제 조건 race 경로를 구체화하지 않았다. Phase 1 구현 시 이 경로가 존재하면 "올바르게 구현했는데 왜 터치가 안 되느냐"는 현장 불량 보고로 이어진다.

**요구 조치**: Phase 1 구현 설계에 "노터치 게이트 RESEED 완료 시 s_boot_touch_ignore를 능동 리셋하거나, RESEED 완료 후 READY 전이를 한 폴링 뒤로 지연하는 구조" 선택지 명시.

---

## 6. 공격_6 — wait_re_ati_done() 내 ATI_Error 즉시 반환 후 상위 핸들러 재귀 경로가 49에서 미처리됐다 [중간, 축_G]

### 6-1. 49의 처리

49 §4 Phase 3:
> "ATI_Error 핸들러: wait_re_ati_done() 레이어와 상위 폴링 루프 레이어 중 어느 쪽에서 처리할지 명시. 재귀 호출 차단 구조 설계(ATI_Error 중 재트리거 차단 플래그)."

"설계 필요"로 명시했지만 구체 설계를 제시하지 않았다.

### 6-2. 코드 기준 현재 wait_re_ati_done() 동작 (직독 확인)

```c
/* iqs323.c:606~609 */
if (status.elements.lsb.ati_error == TDC_DRV_IQS323_ATI_ERROR)
{
    ci_printe("[TOUCH] RE-ATI: ATI ERROR \r\n");
    return false;
}
```

ATI_Error 감지 시 `false` 반환 후 함수 종료. 상위 호출자(tdc_touch_process 또는 R3_CHARGING 트리거 코드)가 `false` 반환을 처리해야 한다.

**race 경로(비블로킹 R3_CHARGING와 결합 시)**:
```
R3_CHARGING 트리거 → re_ati_trigger() → 비블로킹(다음 폴링에서 ATI_Active 확인)
다음 폴링 → ATI_Error 감지 → 핸들러 발동 → re_ati_trigger() 재발행
                               → 다음 폴링 → ATI_Error 또 감지 → 재발행...
```

비블로킹 구조에서 ATI_Error 핸들러가 "pressed==true면 s_ati_error_pending=true"로 처리되면:
- 터치 해제 이벤트 감지 시 re_ati_trigger() 재발행
- 비블로킹이므로 다음 폴링에서 또 ATI_Error 가능
- 재발행 횟수 상한이 49 설계에 없음 → 제어되지 않는 재시도 루프

wait_re_ati_done()을 blocking으로 쓰는 경우:
- 내부 10회 × 100ms = 최대 1초 루프
- ATI_Error 반환 후 상위에서 바로 re_ati_trigger() 호출 시 wait_re_ati_done()이 재호출 → **2초 blocking 가능**
- WDT 갱신 없으면 WDT 리셋 → 무한 부팅 루프

**49의 미처리**: "재귀 차단 플래그 설계 필요"로만 남기고 플래그 구체 동작·상위/하위 레이어 처리 분담이 없다. 비블로킹 R3_CHARGING과 결합 시 재시도 상한이 없는 구조는 구현자에게 명시적 재귀 차단 코드 요구를 전달하지 못한다.

**요구 조치**: Phase 3 설계에 "ATI_Error 재시도 상한 N회(N=[확정 필요], 초기 제안 3회)" + "재시도 소진 후 FIXED 기저 유지 + RTT 경보 발행" 흐름 명세 추가.

---

## 7. 수렴 여부 판정

### 수렴 확정 (49 선언 유지)

| 항목 | 판정 |
|---|---|
| 사후 검증 재시도 3회 제거 + RTT 경보만 | 수렴 (42 치명 공격_1 반영 완료) |
| ATI_SETUP 분기(빌드가드 ATI_FULL_ENABLE) 필수 | 수렴 (회귀_1 반영 완료) |
| EEPROM 로드 경로 필요성 인지 | 수렴 (필요성 인식, 단 §4에서 설계 세부 결함 신규 발굴) |
| #error 빌드가드 3종 세트 완성 | 수렴 |
| 달성도 환경 드리프트·게이트 race 하향 | 수렴 |
| 블로킹_D Phase 1 착수 전 격상 | 수렴 |
| MARGIN 수치 제안 금지 | 수렴 |

### 신규 약점 (2차 재검증에서 발굴)

| # | 약점 | 축 | 심각도 |
|---|---|---|---|
| 공격_1 | 절전 RESEED 오염→ULP 즉시 재부팅 루프 경로 49에서 단순화 → 현재 코드 실 결함이나 §8 미해결 표 미수록 | R | **치명** |
| 공격_2 | 비블로킹 R3_CHARGING이 ATI 진행 중 폴링 처리 부재 + 터치 중 ATI 완료 시 왜곡 게인 저장 방지 미명시 | G | **치명** |
| 공격_3 | RTT_TUNING 분기와 FACTORY_NOTOUCH_COUNTS 게인 기준 동기화 race — 49에서 표 한 줄로만 처리 | U | **고위험** |
| 공격_4 | EEPROM 로드 경로 설계가 실패 시 RTT 경보 없음·CFX 초기화 순서 의존 미명시 | S | **고위험** |
| 공격_5 | 노터치 게이트 RESEED 완료 후 s_boot_touch_ignore 해제 race → 터치 무응답 재현 경로 구체 분석 없음 | R | **중간** |
| 공격_6 | wait_re_ati_done ATI_Error 후 비블로킹 R3_CHARGING 결합 시 재시도 상한 없는 재귀 루프 | G | **중간** |

---

## 8. 핵심 6줄

1. **절전 RESEED→ULP 즉시 재부팅 루프(치명)**: pressed==false 후 수백 ms 뒤 RESEED 발행 사이 터치 재개 시 LTA=터치 counts → ULP에서 즉시 WDT 리셋 재부팅 → GATE_ENABLE=0 환경에서 무조건 RESEED → 재부팅 루프. 49가 블로킹_G에 귀속했으나 이 루프는 MCLR POR 문제와 무관하게 현재 코드에서 재현 가능하며 §8 미해결 표에 없다.

2. **비블로킹 R3_CHARGING의 분산 race(치명)**: 비블로킹 전환은 1초 blocking race를 없애는 대신 ATI 진행 중 200ms 폴링 3~7회에 걸쳐 분산 오염 race를 생성한다 — ATI 진행 중 폴링 결과 처리 정책(NOT_TOUCH 강제·ATI_active 플래그)과 ATI 완료 후 게이트 재확인이 49 R3_CHARGING 명세에 없다.

3. **RTT_TUNING 분기 게인 기준 race(고위험)**: TDC_TOUCH_RTT_TUNING=1 환경에서 ati_valid 상태에 따라 MULT/COMP가 달라지므로 FACTORY_NOTOUCH_COUNTS 기준과 현재 counts의 게인 기준이 불일치할 수 있다 — 49 §9 표에 한 줄만 있고 해소 방법(#error 금지 가드·공장 절차 명문화)이 없다.

4. **EEPROM 로드 설계 조용한 폴백(고위험)**: 49 §2-3 EEPROM 로드 코드 스니펫에 접근 실패 RTT 경보·CFX 초기화 순서 의존·유효 데이터 마커 검증이 없어, 실패 시 컴파일 타임 상수로 조용히 폴백하는 현장 감지 불가 경로가 설계됐다.

5. **s_boot_touch_ignore 해제 race(중간)**: 노터치 게이트 RESEED 완료 후 READY 전이 시점 터치가 있으면 s_boot_touch_ignore=true → 올바른 RESEED 후에도 터치 무응답 구간 발생. 49에서 "확인_10 추가"만으로 처리했으며 Phase 1 구현 설계에 능동 리셋 방법이 없다.

6. **ATI_Error 비블로킹 재귀 상한 부재(중간)**: 49 §4 Phase 3에서 "재귀 차단 플래그 설계 필요"로만 남겨, 비블로킹 R3_CHARGING과 결합 시 ATI_Error 재시도 횟수 상한이 없는 루프 구조가 구현자에게 전달된다 — 재시도 상한 N회·소진 후 FIXED 기저 유지 RTT 경보 명세가 Phase 3 계획에 추가돼야 한다.
