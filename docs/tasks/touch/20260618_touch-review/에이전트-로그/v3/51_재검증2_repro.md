---
name: touch-review-v3-재검증2-repro
purpose: 49_종합-재검증1을 repro(재현성·구현가능성·코드 근거) 관점에서 adversarial 공격해 새 약점·할루시·미검증 주장을 발굴
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, adversarial, repro, revalidation, round2, v3]
---

# 51 — 재검증2_repro: 49_종합-재검증1 adversarial 공격 (repro 관점)

**TL;DR**: 49_종합-재검증1을 재현성·구현가능성·코드 근거 관점에서 공격한다. 발굴된 신규 약점 5건 — ① `tdc_notouch_gate_check()` 언더플로 취약: MARGIN_STRICT가 COUNTS보다 크면 uint32 캐스팅에도 불구 `lo` 계산이 래핑해 전체 범위 통과, ② `write_ati_compensation()` ATI_SETUP 분기 코드(§2-1)에서 ATI Mode Full 레지스터값을 "데이터시트 §5.9 확인 후"로 미룬 채 실제 코드로 제안했으나 해당 레지스터 비트 배치가 종합 전체에서 단 한 번도 확인되지 않아 구현 불가 할루시 위험이 높다, ③ `s_boot_touch_ignore`와 노터치 게이트의 공존 설계(§4 Phase 1 확인_10)가 단순 "해제 조건 정합성 확인"으로 처리됐으나 실제로는 두 가드의 활성 순서가 READY 전이와 맞물려 기존 `s_boot_touch_ignore` 가드를 통과한 터치 이벤트가 노터치 게이트 완료 전에 상위 로직에 도달하는 경로가 존재한다, ④ Phase 3 R3_CHARGING 비블로킹 설계에서 "trigger 발행 → 다음 폴링에서 ATI_Active 확인"으로 기술했으나 IQS323 데이터시트에서 ATI_Active 비트 존재 여부가 단 한 번도 검증되지 않은 미확인 필드다, ⑤ 49_종합이 "TDC_TOUCH_RTT_TUNING 분기와 공장값 게이트 신뢰도 충돌"을 `신규_TDC_RTT_TUNING` 고위험 항목으로 인식했으나 실제 코드에서 RTT_TUNING이 `write_ati_compensation()` 내부에서 MULT/COMP를 런타임으로 교체하는 것이 `write_ati_compensation()` 내 ATI_SETUP 분기 제안(§2-1)과 결합 시 ATI_SETUP만 조건분기되고 MULT/COMP는 s_tdc_normal_tuning 런타임값으로 덮어씌워져 공장 캘리브 EEPROM 로드 경로(§2-3)가 사실상 무력화되는 삼중 충돌이 발생함을 종합이 지적하지 않았다. 수렴 보류 — 신규 약점 5건.

---

## 0. 공격 대상 및 방법

- **타깃**: `49_종합-재검증1.md` 전체, 특히 §2(치명 결함 6건 반영)·§4(정제된 Phase 계획)·§5(블로킹 목록)
- **관점**: repro — 재현성·구현가능성·코드 사실 일치 여부. 이전 재검증1에서 repro 관점(41)이 지적한 항목 중 49_종합이 흡수했다고 선언한 것들을 재검증 포함.
- **근거**: 코드 직접 확인(`iqs323.c:696~733`, `tdc_touch_config.h` 전체, `main.c:807~858`), 이전 로그(41·45 참조).

---

## 1. 41_repro 지적 항목 중 49_종합의 처리 적절성 재검증

| 41_repro 지적 | 49_종합 처리 | repro 판정 |
|---|---|---|
| GATE+COUNTS=0 모순 → `#error` 강제 | §3 빌드가드에 `#error "NOTOUCH_GATE_ENABLE requires FACTORY_NOTOUCH_COUNTS != 0"` 추가 | **흡수 확인** — 컴파일 타임 차단 수렴. |
| wait_re_ati_done() WDT 0건 확인 → 기존 함수 수정 필수 격상 | §4 Phase 3 "SYS_WATCHDOG_REFRESH() 추가 — 함수 수정 필수 선행 사항으로 격상" | **흡수 확인** — 그러나 격상됐다는 선언만 있고 수정 방법(루프 내 어느 위치, 몇 ms마다)은 여전히 미기술. |
| 충전 중 Re-ATI 달성도 과장 | §7 달성도 표 "환경 드리프트 → △" 하향 | **흡수 확인** |
| FACTORY_CALIB_ENABLE ↔ ATI_CALIB_MODE 역할 이중화 | §3 말미 Note에 "역할 분리: ATI_CALIB_MODE=RTT 덤프+수동 캘리브, FACTORY_CALIB_ENABLE=공장 공정 자동 저장" | **부분 흡수** — 분리 선언은 있으나 두 경로가 동일 `write_ati_compensation()` 함수를 공유하면 `ATI_FULL_ENABLE` 분기(§2-1)가 ATI_CALIB_MODE 경로에도 적용돼야 하는지 미기술. **신규 약점_E로 등록** (하단 §2.5). |
| counts 1샘플 신뢰도 (Auto-ATI 직후 안정화 delay) | 49_종합 전체에서 미언급 | **미흡수** — 확인_10(§6)으로 별도 항목 추가됐으나 이것은 s_boot_touch_ignore 해제 정합성이며, counts 1샘플 신뢰도(Auto-ATI 완료 직후 IC 내부 수렴 소요) 문제는 49_종합에서도 여전히 누락. **신규 약점_D로 등록** (하단 §2.4에서 재발굴). |

---

## 2. 신규 약점 상세

### 2.1 [신규_A] `tdc_notouch_gate_check()` 언더플로 취약 — uint32 캐스팅이 래핑 보호 불완전 (치명)

**49_종합의 코드 (§4 Phase 1)**:
```c
static bool tdc_notouch_gate_check(uint16_t counts)
{
    uint32_t lo = (uint32_t)TDC_TOUCH_FACTORY_NOTOUCH_COUNTS
                  - TDC_TOUCH_NOTOUCH_MARGIN_STRICT;
    uint32_t hi = (uint32_t)TDC_TOUCH_FACTORY_NOTOUCH_COUNTS
                  + TDC_TOUCH_NOTOUCH_MARGIN_UP;
    return ((uint32_t)counts >= lo && (uint32_t)counts <= hi);
}
```

**문제**: `TDC_TOUCH_NOTOUCH_MARGIN_STRICT`가 `TDC_TOUCH_FACTORY_NOTOUCH_COUNTS`보다 크면 `lo` 계산이 uint32 래핑(underflow)되어 `lo ≈ 0xFFFF...`가 된다. 이 상태에서 `counts >= lo` 조건은 항상 false — 게이트가 영구 거부 상태가 되지만 래핑이 발생했다는 진단 경로가 없다.

구체 시나리오: 공장 캘리브 수행 전 임시로 `FACTORY_NOTOUCH_COUNTS=100`, `NOTOUCH_MARGIN_STRICT=150`으로 테스트 설정하면 `lo = (uint32_t)100 - 150`이 C unsigned 산술에서 `lo = 0xFFFFFF9A`가 되어 counts=100이 게이트를 통과 못 한다. #error 가드는 `COUNTS==0` 조합만 차단하고 `COUNTS < MARGIN_STRICT` 조합을 차단하지 않는다.

**종합의 누락**: §4 Phase 1 코드에서 언더플로 가드(`COUNTS > MARGIN_STRICT` 사전 체크 또는 부호 없는 뺄셈 보호)가 없다. MARGIN_STRICT가 0 기본값이라 현재는 무해하지만, 실측_A 후 MARGIN 값 설정 시 이 함수를 수정하지 않으면 잠재 언더플로 버그가 잠복한다.

**요구 조치**: `#if TDC_TOUCH_NOTOUCH_MARGIN_STRICT >= TDC_TOUCH_FACTORY_NOTOUCH_COUNTS` → `#error "MARGIN_STRICT must be < FACTORY_NOTOUCH_COUNTS"` 컴파일 타임 가드 추가. 또는 gate_check() 내에서 `if (TDC_TOUCH_FACTORY_NOTOUCH_COUNTS <= TDC_TOUCH_NOTOUCH_MARGIN_STRICT) return false;` 런타임 가드 추가.

---

### 2.2 [신규_B] ATI_SETUP Mode=Full 레지스터값 미확인 — 구현 불가 할루시 위험 (치명)

**49_종합의 코드 (§2-1)**:
```c
#if TDC_TOUCH_ATI_FULL_ENABLE
    write_register(SENSOR0_ATI_SETUP, ATI_SETUP_FULL_LSB, ATI_SETUP_MSB);
#else
    write_register(SENSOR0_ATI_SETUP, TDC_DRV_IQS323_ATI_SETUP_LSB, ATI_SETUP_MSB);
#endif
```

그리고 곧바로:
> "[!IMPORTANT] ATI_FULL_ENABLE=1일 때 ATI Mode Full 레지스터값은 데이터시트 §5.9 확인 후 확정 [확정 필요]"

**문제**: `ATI_SETUP_FULL_LSB`가 어떤 값인지 종합 전체(30·49)에서 단 한 번도 기술되지 않았다. 현재 코드에서 `TDC_DRV_IQS323_ATI_SETUP_LSB = 0x08`(Mode=Disabled, bits[2:0]=000)임은 `iqs323.c:651`에서 확인된다. ATI Mode Full에 해당하는 bits[2:0] 값은 데이터시트 §5.9에서 확인해야 하는데, v1·v2·v3 에이전트 로그 전체를 통해 이 값이 정의된 곳이 없다.

Phase 3 구현 시 개발자가 `ATI_SETUP_FULL_LSB`를 임의로 추정하면 (예: 0x07이나 0x03) ATI가 활성화되지 않거나 예기치 않은 모드(Partial ATI 등)로 진입할 수 있다. 이것은 코드 스니펫 형태로 제시됐으나 핵심 상수가 `[확정 필요]` 상태인 미완성 스니펫이다.

**종합의 과잉 표현**: §2-1 코드 블록은 실제 구현 가능한 코드처럼 제시됐으나, `ATI_SETUP_FULL_LSB` 정의 없이는 컴파일조차 불가다. `[확정 필요]` 주석으로 면책했지만, 구현 코드 형태로 제시한 것이 독자를 오도할 수 있다 [추정: 미완성 코드를 완성 코드처럼 보이게 하는 표현 방식이 구현 단계에서 상수 누락 버그를 유발할 위험이 있음].

**요구 조치**: §2-1 코드 블록에서 `ATI_SETUP_FULL_LSB`를 `0x??u /* [확정 필요] DS §5.9 bits[2:0] ATI Mode Full 값 */`으로 명시하거나, 코드 블록 전체를 "구현 의도 설계도" 형식으로 표현을 바꾸어 실제 상수값이 포함된 것처럼 오해받지 않도록 수정.

---

### 2.3 [신규_C] R3_CHARGING 비블로킹에서 "ATI_Active 비트" 사용 — 데이터시트 미확인 필드 (심각)

**49_종합의 Phase 3 R3_CHARGING 명세 (§2-4)**:
```
구조: 비블로킹 선호 — re_ati_trigger() 발행 후 다음 폴링에서 ATI_Active 확인
      (블로킹 wait_re_ati_done()은 1초 race 창으로 사용 금지)
```

**문제**: `ATI_Active`라는 비트 필드가 IQS323 데이터시트에 실제로 존재하는지 v1·v2·v3 전체 로그 어디에도 확인되지 않았다. 현재 코드 `wait_re_ati_done()`(L581~623)은 `ati_event`(ATI 완료 이벤트)와 `ati_error`(ATI 오류)를 읽는데, 이것은 `SYSTEM_STATUS` 레지스터의 비트 필드다. "ATI_Active"가 별도 비트로 존재해 "ATI 진행 중" 상태를 나타낸다는 근거가 없다.

만약 ATI 진행 중 상태를 읽는 비트가 없고 완료 시 `ati_event` 플래그만 set된다면, 비블로킹 설계에서 "다음 폴링에서 ATI_Active 확인"이라는 것은 실제로 불가능하다 — 폴링에서 `ati_event`가 set됐는지 확인하는 구조로 대체해야 하며 `ati_event`가 폴링 주기(200ms) 내에 clear되지 않도록 처리해야 한다 [확정 필요].

**종합의 할루시 위험**: "ATI_Active 확인"이라는 표현이 IQS323의 실제 레지스터 비트 이름처럼 기술됐으나 이것이 실제 비트 필드인지, 아니면 "ATI 완료 이벤트(ati_event) 확인"을 의미하는 개념적 설명인지 모호하다. Phase 3 구현 시 이 모호성이 잘못된 레지스터 접근으로 이어질 위험이 있다.

**요구 조치**: 블로킹_H(신규) 추가 — 비블로킹 Re-ATI 폴링 루프 설계 전 `ati_event` 플래그의 자동 clear 타이밍(데이터시트 §5.10·§5.11)과 "ATI 진행 중" 상태를 나타내는 비트 존재 여부 확인.

---

### 2.4 [재발굴_D] Auto-ATI 완료 직후 counts 1샘플 신뢰도 — 49_종합에서도 미흡수 (심각)

41_repro §2.2에서 "apply_settings() 내 counts 1샘플 읽기의 Auto-ATI 완료 직후 안정화 delay 부재"를 지적했다. 49_종합이 이 항목을 흡수했는지 확인한다.

**49_종합의 처리 확인**:
- §5 블로킹 목록: 블로킹_A(noTouch counts 절대값 분포), 블로킹_B(방전 실측), 블로킹_C(CFX 파서), 블로킹_D(Beta POR), 블로킹_E(Max Counts), 블로킹_F(write_ati_compensation 레지스터 목록), 블로킹_G(MCLR POR 완전성). **Auto-ATI 완료 직후 IC counts 안정화 소요 시간 항목이 없다.**
- §6 확인 목록: 확인_7~10. Auto-ATI 후 안정화 delay 항목 없음.

**재발굴 근거**: 부팅 시퀀스는 MCLR → Auto-ATI(비블로킹 폴링 감지, ~1.5초) → `write_ati_compensation()` → noTouch 게이트 → RESEED다. Auto-ATI가 완료되는 시점에서 `ati_event`가 set되는 것과 IC 내부에서 counts 값이 새 MULT/COMP 기준으로 완전히 수렴하는 것 사이에 시간 차이가 있을 수 있다 [추정]. 이 불안정 구간에서 1샘플만으로 게이트를 판별하면 오통과 위험이 있다. 이것은 MCLR POR 완전성(블로킹_G)과 별개의 타이밍 문제다.

**요구 조치**: 블로킹_I(신규) 추가 — Auto-ATI ati_event 감지 후 IC counts 수렴까지 추가 안정화 delay 필요 여부 데이터시트 §5.9 확인 + RTT 실측 (실측_A 수행 시 병행 가능).

---

### 2.5 [신규_E] RTT_TUNING × ATI_SETUP 분기 × EEPROM 로드 삼중 충돌 — 49_종합의 처리가 불완전 (치명)

**배경**: 49_종합은 45_회귀에서 발굴된 `TDC_TOUCH_RTT_TUNING` 분기 문제를 `신규_TDC_RTT_TUNING` 고위험 항목(§9 잔존 미해결)으로 분류하고 2차 재검증 대상으로 미뤘다.

**repro 공격 — 삼중 충돌 구조**:

```
시나리오: TDC_TOUCH_RTT_TUNING=1 + TDC_TOUCH_ATI_FULL_ENABLE=1 + TDC_TOUCH_FACTORY_CALIB_ENABLE=0(일반 부팅)

실행 순서:
1. tdc_drv_iqs323_apply_settings() 진입
2. EEPROM 로드 경로(§2-3): calib.mult_lsb/comp_lsb → s_mult_lsb/s_comp_lsb 설정
3. write_ati_compensation() 호출
4. 내부 ATI_SETUP 분기(§2-1): ATI_FULL_ENABLE=1 → ATI Mode=Full write ← OK
5. MULT/COMP 분기(iqs323.c:705~730):
   #if TDC_TOUCH_RTT_TUNING → s_tdc_normal_tuning.ati_valid=true이면
   → s_tdc_normal_tuning.mult_lsb/comp_lsb 사용
   → EEPROM 로드값(s_mult_lsb/s_comp_lsb)은 무시됨 ← 충돌
```

3가지 기능이 동시 활성 시:
- ATI_SETUP은 Full 모드로 올바르게 설정됨
- 그러나 MULT/COMP는 EEPROM 로드값이 아닌 RTT 런타임 튜닝값으로 덮어써짐
- ATI Full이 이 MULT/COMP를 기준으로 보정 실행 → EEPROM 공장 캘리브 값은 완전 무시됨

이 삼중 충돌은:
- 45_회귀 §2에서 "RTT_TUNING과 공장값 게이트 신뢰도 충돌"로 부분 발굴됨
- 49_종합 §2-3 EEPROM 로드 경로 추가로 충돌 면적이 더 확대됐음에도 불구 RTT_TUNING 관련 분기가 EEPROM 로드 경로와 어떻게 상호작용하는지 설계에 없음

**종합의 누락**: §2-3 EEPROM 로드 경로 코드(§2-3)에서 `s_mult_lsb`/`s_comp_lsb`를 설정하지만 이것이 `write_ati_compensation()` 내부의 `TDC_TOUCH_RTT_TUNING` 분기에 의해 우회될 수 있음을 전혀 언급하지 않았다.

**요구 조치**: Phase 2·3 구현 전 `TDC_TOUCH_RTT_TUNING`과 `TDC_TOUCH_FACTORY_CALIB_ENABLE`의 동시 활성을 `#error`로 차단하거나, RTT_TUNING 경로가 EEPROM 로드값을 우선 사용하도록 `write_ati_compensation()` 내부 우선순위 재설계. 빌드가드 세트(§3)에 신규 충돌 가드 추가 필수.

---

## 3. 할루시네이션 여부 재검토

### 3.1 49_종합 §2-4 "wait_re_ati_done() 최악 ~1.9초" 주장

**49_종합의 주장**: "wait_re_ati_done() 블로킹 최악 ~1.9초(Sys_Delay 1초 + force_window_open ~0.9초)"

**코드 근거**: 
- L586~623: for 10회, 각 회차 `Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 100)` = 100ms
- 최악(모든 회차 timeout): 10 × 100ms = 1,000ms = 1초

`force_window_open()`은 `read_register()` 내부에서 호출되는데, `wait_re_ati_done()`의 `read_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS, ...)` 호출이 매 회차에 있다. force_window_open 최악 45ms × 10회 = 450ms 추가 가능.

그러나 49_종합이 인용한 "44_통신전력 발굴: ~1.9초"는 force_window_open 포함 1초+0.9초 = 1.9초로 계산한 것으로 보인다. 44_통신전력 노드를 직접 참조하지 않아 이 계산의 정확성은 [확정 필요]지만, 방향은 맞다 — 단 45ms × 10회 = 450ms이므로 1초 + 450ms = 최악 1.45초가 더 정확할 수 있다. 1.9초는 약간의 과대 추정일 수 있으나 안전 측이므로 보수적으로 허용 가능 [추정].

**판정**: 경미한 과대 추정 의심 [추정]. 치명적 할루시는 아님.

### 3.2 49_종합 §4 Phase 1 "write_ati_compensation() 항상 실행 유지"와 §2-1 ATI_SETUP 분기의 논리 일관성

**주장 A (§4 Phase 1)**: "write_ati_compensation() 항상 실행 유지"
**주장 B (§2-1)**: "write_ati_compensation() 내 ATI_SETUP write를 빌드가드로 분기"

이 두 주장은 모순처럼 보이지 않는다 — "함수는 항상 호출하되 내부에서 ATI_SETUP write를 조건부로"라는 의미다. 그러나 §2-1 말미 Note에서 "Phase 3 착수 시 이 분기 설계 필수"라고만 하고, 실제로 `write_ati_compensation()`의 호출자(apply_settings 내 L1141)와 함수 내부 수정 범위가 명확하게 분리되지 않아 독자가 "함수 전체를 조건부로 하는 것인가"로 오해할 가능성이 있다. 이것은 표현 모호성이지 내용 모순은 아님.

**판정**: 할루시 아님. 표현 명확화 필요 수준.

---

## 4. 49_종합이 흡수했다고 선언한 항목 중 재검증에서 미흡 판정된 것

| 항목 | 49_종합 처리 | 재검증2 판정 |
|---|---|---|
| wait_re_ati_done() 수정 방법 상세 | "수정 필수 선행 사항으로 격상" 선언 | 선언만 있고 루프 내 삽입 위치·WDT timeout 값 기준 미기술. 미흡 |
| ATI_CALIB_MODE ↔ FACTORY_CALIB_ENABLE 역할 분리 | Note에서 개념 분리 설명 | FULL_ENABLE 분기가 ATI_CALIB_MODE 코드 경로에도 적용되는지 미기술. 신규_E와 연계 미흡 |
| Auto-ATI 후 counts 1샘플 신뢰도 | 미언급 (41_repro §2.2 항목) | 블로킹_I 신규로 등록 필요 |
| RTT_TUNING 분기 대응 | 2차 재검증 대상으로 연기 | 본 라운드에서 EEPROM 로드 경로 추가로 삼중 충돌 확인 — 즉시 빌드가드 필요 |

---

## 5. 수렴 여부 판정

**수렴 보류 — 신규 약점 5건 발굴.**

| ID | 항목 | 심각도 |
|---|---|---|
| 신규_A | gate_check() MARGIN_STRICT > COUNTS 시 uint32 언더플로 → 영구 게이트 거부 (진단 없음) | 치명 |
| 신규_B | ATI_SETUP_FULL_LSB 미정의 — 구현 불가 할루시 위험 | 치명 |
| 신규_C | R3_CHARGING "ATI_Active 비트" 데이터시트 미확인 | 심각 |
| 재발굴_D | Auto-ATI 완료 후 counts 안정화 소요 시간 (41_repro §2.2 재발굴, 49_종합 미흡수) | 심각 |
| 신규_E | RTT_TUNING × ATI_FULL_ENABLE × EEPROM 로드 삼중 충돌 — EEPROM 공장값 무시 경로 | 치명 |

기존 수렴 확인된 항목(GATE+COUNTS=0 `#error`, 사후 재시도 제거, EEPROM 로드 경로 신규 추가, 달성도 하향, 블로킹_G 추가)은 재검증2에서도 수렴 유지.

---

## 6. 핵심 6줄

1. **[신규_A 치명] gate_check() 언더플로**: MARGIN_STRICT > COUNTS 시 `lo = (uint32_t)COUNTS - MARGIN_STRICT`가 uint32 wrap-around해 게이트 영구 거부 상태가 됨 — 현재 MARGIN=0이라 잠복하지만 실측_A 후 값 설정 즉시 발현 가능하며 `#if MARGIN_STRICT >= COUNTS` `#error` 빌드가드가 없다.
2. **[신규_B 치명] ATI_SETUP_FULL_LSB 미정의**: §2-1 코드 스니펫의 핵심 상수 `ATI_SETUP_FULL_LSB`가 v1·v2·v3 전체에서 단 한 번도 데이터시트 §5.9로부터 확인되지 않아 Phase 3 구현 시 개발자가 임의 추정하면 ATI Full 진입 실패 또는 예기치 않은 모드 진입 버그가 발생한다.
3. **[신규_C 심각] ATI_Active 비트 미확인**: R3_CHARGING 비블로킹 설계의 "다음 폴링에서 ATI_Active 확인"이 IQS323 데이터시트에 실재하는 비트인지 v3 전체에서 검증되지 않음 — 비트가 없으면 비블로킹 설계 자체가 재설계 필요하며 블로킹_H로 추가해야 한다.
4. **[재발굴_D 심각] counts 1샘플 안정화**: Auto-ATI ati_event 감지 직후 IC 내부 counts가 새 MULT/COMP 기준으로 완전 수렴하는 데 추가 delay가 필요한지가 41_repro에서 지적됐으나 49_종합 블로킹 목록에 여전히 없음 — 실측_A 수행 시 병행 확인을 블로킹_I로 명시 추가 필요.
5. **[신규_E 치명] 삼중 충돌**: `TDC_TOUCH_RTT_TUNING=1` + `ATI_FULL_ENABLE=1` + EEPROM 로드 경로(§2-3) 동시 활성 시 `write_ati_compensation()` 내 RTT_TUNING 분기가 EEPROM 공장 캘리브값을 런타임 튜닝값으로 덮어써 Phase 2·3 기반 전체가 무효화됨 — 빌드가드 세트(§3)에 `RTT_TUNING && FACTORY_CALIB_ENABLE → #error` 추가 필수.
6. **수렴 보류**: 아키텍처 원리는 여전히 유효하고 49_종합의 치명 결함 6건 처리는 수렴 확인됐으나, 신규 치명 3건(신규_A·B·E) + 심각 2건(신규_C·재발굴_D)이 Phase 1·3 착수 전에 추가로 해소되어야 한다. 현 상태로 Phase 1 구현 착수는 미권고.
