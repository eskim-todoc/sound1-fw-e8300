---
name: touch-review-v3-재검증3-repro
purpose: 59_종합-재검증2를 repro(재현성·구현가능성·코드 근거) 관점에서 adversarial 공격해 새 약점·할루시·미검증 주장을 발굴
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, adversarial, repro, revalidation, round3, v3]
---

# 61 — 재검증3_repro: 59_종합-재검증2 adversarial 공격 (repro 관점)

**TL;DR**: 59_종합-재검증2를 재현성·구현가능성·코드 근거 관점에서 3차 공격한다. 발굴된 신규 약점 4건 + 미수렴 할루시 2건 — ① `re_ati_trigger()`가 MSB를 0x00으로 초기화한 채 write하므로 CH_TIMEOUT 비활성화(0x07) 레지스터 값을 덮어쓰는 부작용이 코드로 확인됨에도 59_종합이 이것을 "확인_13 신규" 확인 항목에만 남긴 채 Phase 계획에 수정 코드를 포함하지 않았다, ② `tdc_touch_config.h`에 59_종합이 제안한 신규 define이 단 하나도 존재하지 않아 §3 빌드가드 세트 전체가 현재 컴파일 불가다, ③ Phase −1 즉시 수정 대상인 `wait_re_ati_done()` WDT 갱신 미삽입이 59_종합 이후에도 코드에 반영되지 않아 현장 버그가 여전히 활성 상태다, ④ 59_종합 §4 Phase 1의 `s_boot_touch_ignore` 통합 설계가 기존 구현(`touch.c:323/352`)을 참조하겠다고 선언했으나 확인_15가 여전히 미해소이며 이 두 가드의 통합 실패 시 READY 전이 직후 노터치 게이트 완료 전 터치 이벤트가 상위 로직에 도달하는 경로가 구체적으로 기술되지 않았다. 수렴 여부: **미수렴** — 신규 약점 4건, 기존 할루시 2건 지속.

---

## 0. 공격 대상 및 방법

- **타깃**: `59_종합-재검증2.md` 전체, 특히 §2(치명 결함 9건)·§3(빌드가드 세트 v2)·§4(Phase 계획 v2)·§5(블로킹 목록)
- **관점**: repro — 재현성·구현가능성·코드 사실 일치. 코드(`iqs323.c`, `tdc_touch_config.h`, `main.c`) 직접 재확인.
- **신규 약점 우선 기준**: 59_종합이 처리했다고 선언한 항목 중 실제 코드와 불일치하는 것, 또는 59_종합이 새롭게 제안한 설계에서 재현 불가한 가정이 있는 것.

---

## 1. 59_종합이 처리했다고 선언한 항목 재검증

| 59_종합 선언 | 코드 확인 결과 | repro 판정 |
|---|---|---|
| Phase −1: WDT 갱신 즉시 삽입 (`iqs323.c:581~623`) | `wait_re_ati_done()` L581~623 전체에 `SYS_WATCHDOG_REFRESH()` 호출 **0건** 여전히 존재 | **미수정 현장 버그 활성** |
| §3 빌드가드 세트 v2: 신규 define 6종+ 추가 | `tdc_touch_config.h` 직접 확인: `TDC_TOUCH_SATURATE_DETECT_ENABLE` 등 59_종합 제안 define **전무** | **현재 컴파일 불가 설계** |
| 확인_13: `re_ati_trigger()` MSB=0x00 CH_TIMEOUT 덮어쓰기 여부 확인 필요 | `re_ati_trigger()` L569~579: `reg.bytes[2] = 0x00` 명시 초기화 후 write → CH_TIMEOUT disable(0x07 MSB) 덮어쓰기 **코드로 확인됨** | **확인 완료가 아니라 실제 버그 확정** |
| 블로킹_H: ATI_Active API 신설 필요 | `tdc_drv_iqs323_read_status()` L1166~1183: `bool *p_ati_error`만 노출, ATI_Active 필드 없음 | **블로킹_H 미해소 정확** |
| §2-4 EEPROM 로드 경로 미설계 (`s_mult_lsb` 변수 신설) | `iqs323.c` 전체: `s_mult_lsb`, `s_comp_lsb`, `s_override_mult_lsb` 정적 변수 **없음** | **미설계 정확 — 그러나 Phase 2 설계가 여전히 사문** |

---

## 2. 신규 약점 상세

### 2.1 [신규_A] `re_ati_trigger()` CH_TIMEOUT 덮어쓰기 버그 — 확인_13이 아닌 즉시 수정 대상 (치명)

**코드 확인 (`iqs323.c:569~579`)**:
```c
static bool re_ati_trigger(void)
{
    tdc_drv_iqs323_reg_system_control_t reg;

    reg.bytes[0]            = TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL;
    reg.bytes[1]            = 0x00;
    reg.bytes[2]            = 0x00;   /* ← MSB 명시 0x00 초기화 */
    reg.elements.lsb.re_ati = TDC_DRV_IQS323_TRIGGER_RE_ATI;

    return write_register(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}
```

`reg.bytes[2]`(MSB)가 0x00으로 초기화되고, `re_ati`는 LSB 필드다. 따라서 `re_ati_trigger()` 호출 시 SYSTEM_CONTROL MSB=0x00이 write된다. `apply_settings()` 말미에서 CH_TIMEOUT을 비활성화한 것은 `write_register(0xC0_addr, 0x00, 0x07)` — MSB=0x07이다. `re_ati_trigger()`가 호출되면 MSB=0x00이 write돼 CH_TIMEOUT 비활성화(bits[2:0]=111)가 **즉시 해제**된다.

**59_종합의 처리**: §2-8에서 "확인_13 신규: re_ati_trigger() MSB=0x00 write로 CH_TIMEOUT 0x07 초기화 부작용 분석 필요"라고 기술하고 "확인 항목"으로 분류했다. 그러나 이것은 분석 대상이 아니라 코드에서 이미 확인되는 사실이다.

**영향**: CH_TIMEOUT이 현재 이중 차단(ATI_Mode_Disabled + CH_TIMEOUT_disable)임을 재확인한 상태에서, `re_ati_trigger()`를 Phase 3에서 호출하면 CH_TIMEOUT이 재활성화된다. 이 상태에서 CH_TIMEOUT이 발동하면 자동 Re-ATI가 트리거되고, ATI_Mode=Full(Phase 3에서 변경 예정)이면 stuck-touch 상황에서 자동 Re-ATI가 노터치 게이트 없이 실행될 수 있다 — 즉 Phase 3 도입 자체가 CH_TIMEOUT 경유 무게이트 Re-ATI 경로를 열 수 있다.

**요구 조치**: `re_ati_trigger()` 수정 — MSB를 0x07로 유지하거나, re_ati bit만 set하고 CH_TIMEOUT 필드를 보존하는 read-modify-write 구조로 변경. 59_종합 §4 Phase 3 "re_ati_trigger() 비블로킹 재진입 가드" 코드에 이 수정 선행 포함 필수.

---

### 2.2 [신규_B] §3 빌드가드 세트 v2 전체가 현재 컴파일 불가 — `tdc_touch_config.h`에 신규 define 없음 (치명)

**코드 확인 (`tdc_touch_config.h` 전체)**:
현재 `tdc_touch_config.h`에 존재하는 define:
- `TDC_BOARD_VARIANT`, `TDC_DRV_IQS323_ATI_DUMP_ENABLE`, `TDC_TOUCH_ATI_CALIB_MODE`, `TDC_TOUCH_ATI_CALIB_LED_ENABLE`, `TDC_TOUCH_SLEEP_MEASURE_MODE`, `TDC_TOUCH_CRX1_VSS_ENABLE`, `TDC_TOUCH_CRX1_REF_ENABLE`, `TDC_TOUCH_CRX1_DUMMY_ENABLE`, `TDC_TOUCH_CRX0_DISCHARGE_ENABLE`, `TDC_TOUCH_CRX0_DISCHARGE_LOG`, `TDC_TOUCH_MARGIN_LOG_ENABLE`, `TDC_TOUCH_MARGIN_LOG_INTERVAL`, `TDC_TOUCH_PROX_THRESHOLD_ENABLE` — 총 13개.

59_종합 §3 빌드가드 세트 v2가 전제하는 define:
- `TDC_TOUCH_SATURATE_DETECT_ENABLE`, `TDC_TOUCH_SATURATE_MAX_COUNTS`, `TDC_TOUCH_NOTOUCH_GATE_ENABLE`, `TDC_TOUCH_FACTORY_NOTOUCH_COUNTS`, `TDC_TOUCH_NOTOUCH_MARGIN_STRICT`, `TDC_TOUCH_NOTOUCH_MARGIN_UP`, `TDC_TOUCH_FACTORY_CALIB_ENABLE`, `TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE`, `TDC_TOUCH_ATI_FULL_ENABLE`, `TDC_TOUCH_CRADLE_REATI_ENABLE`, `TDC_TOUCH_RTT_TUNING` (마지막은 `iqs323.c:15`에는 존재하지만 `tdc_touch_config.h`에는 없음) — 총 10종+ **전무**.

**59_종합의 처리**: 59_종합은 빌드가드 세트를 마치 현재 코드에 존재하는 것처럼 기술했지만, 이것들은 전부 아직 추가되지 않은 신규 define이다. `#if TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE && !TDC_TOUCH_ATI_FULL_ENABLE` 같은 `#error` 가드는 해당 define이 없으면 `#if 0` 으로 처리돼 가드가 **조용히 무효화**된다.

**영향**: 59_종합 §3 전체가 현재 코드에 적용되지 않은 상태이며, 개발자가 검토 없이 define을 추가하면 기존 13개 define과의 네이밍 일관성 확인 없이 진행하게 된다. 특히 `TDC_TOUCH_ATI_FULL_ENABLE`과 기존 `TDC_DRV_IQS323_ATI_DUMP_ENABLE`의 "ATI" 접두어 혼동, `TDC_TOUCH_FACTORY_CALIB_ENABLE`과 기존 `TDC_TOUCH_ATI_CALIB_MODE`의 역할 중복은 41_repro §2.4에서 이미 지적됐으나 59_종합에서도 미해소다.

**요구 조치**: 59_종합 §3 빌드가드 세트를 "현재 코드"가 아닌 "추가 대상 설계"임을 명시. Phase 1 착수 전 `tdc_touch_config.h` 수정 시 기존 define과 역할 중복 검토를 블로킹 항목으로 추가.

---

### 2.3 [신규_C] Phase −1 WDT 갱신 미삽입 — 2차 재검증 이후에도 코드 미수정, 현장 버그 여전히 활성 (긴급)

**배경**: 57_엣지에서 발굴, 59_종합 §2-1에서 "즉시 수정 (Phase 0 이전)" 선언. 그러나 현재 코드(`iqs323.c:581~623`)를 직접 확인한 결과:

```
wait_re_ati_done() 내 for 루프(10회):
  - L592: Sys_Delay(100ms) — WDT 갱신 없음
  - L599: Sys_Delay(100ms) — WDT 갱신 없음
  - L618: Sys_Delay(100ms) — WDT 갱신 없음
```

`SYS_WATCHDOG_REFRESH()` **0건** — 미수정 확인.

**59_종합의 과잉 처리 주장**: §9 수렴 확정 항목에 "wait_re_ati_done() WDT 갱신 미삽입 즉시 수정 필요 → 재검증2 수렴"이라 기술했다. 이것은 "수렴"이 아니라 수정 필요성에 대한 합의다 — 실제 코드 수정은 에이전트 로그 범위 밖이지만, 59_종합이 "수렴 확정"으로 분류한 것이 "코드 수정 완료"로 오독될 위험이 있다.

**영향**: `ATI_CALIB_MODE=1` 경로(`iqs323.c:860`)에서 이미 `wait_re_ati_done()`을 호출 중이므로 현장 버그다. ATI_CALIB_MODE를 활성화한 채 공장 캘리브레이션을 수행하면 WDT 만료 재부팅이 발생할 수 있다. Phase 3 Re-ATI 활성화 이전에도 이 경로로 재부팅이 발생하면 캘리브레이션 데이터가 유실된다.

**요구 조치**: 59_종합 §9 수렴 확정 항목을 "수정 합의(미구현)" 으로 명확히 수정. 실제 코드 diff를 계획.md에 포함하거나 별도 즉시 수정 태스크로 분리.

---

### 2.4 [신규_D] `s_boot_touch_ignore`와 노터치 게이트 통합 경로 — 터치 이벤트 상위 도달 race 구체화 미완 (고위험)

**59_종합의 처리**: §4 Phase 1에서 "`s_boot_touch_ignore` 통합 설계: 노터치 게이트 RESEED 완료 시 강제 clear 또는 READY 전이 1 폴링 지연"이라고 기술. 확인_15(`touch.c:323/352` 확인)를 블로킹으로 남겼다.

**repro 공격**:

1. **`s_boot_touch_ignore`의 현재 set/clear 조건**: 기존 코드 `touch.c:323`에서 READY 전이 시 TOUCH 상태이면 `s_boot_touch_ignore=true` set, `touch.c:352`에서 "TOUCH 해제(NOTOUCH) 감지 시" clear. 이것은 터치 해제를 기다리는 가드다.

2. **노터치 게이트가 추가되면**: READY 전이 → `s_boot_touch_ignore` 평가 → 동시에 노터치 게이트(`tdc_notouch_gate_check()`) 수행 예정. 그런데 `s_boot_touch_ignore`가 READY 전이 시 NOTOUCH 상태이면(=설정 안 됨) 즉시 폴링이 시작된다. 이 폴링 시작 후 노터치 게이트 RESEED 완료 전에 터치가 발생하면, `s_boot_touch_ignore`가 false이므로 터치 이벤트가 상위 로직에 즉시 전달된다.

3. **59_종합의 대응**: "RESEED 완료 시 `s_boot_touch_ignore` 강제 clear 또는 READY 전이 1 폴링 지연"이라고 했으나, 이 두 선택지는 **race를 해소하지 못한다**. RESEED 완료 = 노터치 게이트 통과 후이고, 그 이전 폴링 구간이 여전히 열려 있다. "READY 전이 1 폴링 지연"도 노터치 게이트 완료까지 다수 폴링이 필요할 수 있어 1회 지연으로 부족하다.

4. **정확한 설계 요구**: 노터치 게이트 진행 중 전체 구간(READY 전이 시점 → RESEED 완료 시점)을 `s_boot_touch_ignore`로 억제하거나, 독립적인 `s_gate_in_progress` 플래그를 신설해 이 구간 동안 터치 이벤트를 상위에 전달하지 않아야 한다. 이것이 59_종합 확인_15가 해소한 후 설계해야 할 내용인데, 59_종합은 이 구체 설계 없이 "확인 후 결정"으로만 처리했다.

**요구 조치**: Phase 1 착수 전 노터치 게이트 전체 진행 구간 억제 전략(플래그 신설 또는 `s_boot_touch_ignore` 범위 확장)을 명시. 확인_15를 "확인 후 이 설계 결정" 형태로 명시적 분기 기술 추가.

---

## 3. 기존 할루시 지속 확인 (3차 재검증)

### 3.1 [지속] `ATI_SETUP_FULL_LSB` 미정의 — 59_종합도 미해소 (블로킹_J)

59_종합 §2-3은 `ATI_SETUP_FULL_LSB`를 `0xFFu /* PLACEHOLDER */`로 명시하고 블로킹_J로 분류했다. 이 값은 데이터시트 §5.9 bits[2:0]에서 확인해야 하는데, v1·v2·v3 전체를 통해 확인되지 않았다. 현재 `iqs323.c:700`에서 `write_ati_compensation()`이 `TDC_DRV_IQS323_ATI_SETUP_LSB`를 사용하는데, 이것의 현재 값은 `0x08`(Mode=Disabled, bits[2:0]=000)이다. ATI Mode Full의 bits[2:0] 값이 존재하지 않는 상수(`ATI_SETUP_FULL_LSB`)로 제안된 코드는 컴파일 불가 상태가 유지되며, 59_종합에서 "PLACEHOLDER"로 명시한 것은 3차에도 유효한 블로킹임을 재확인한다.

**판정**: 블로킹_J 미해소 확인. 59_종합의 PLACEHOLDER 처리는 할루시 위험을 낮췄으나 해소는 아님.

### 3.2 [지속] `write_ati_compensation(void)` 인자 전달 메커니즘 — 신규_EEPROM전달 미해소

59_종합 §2-5에서 "선택지_1(인자 추가) 권고"를 기술했으나 현재 `write_ati_compensation(void)` 시그니처는 변경되지 않았고(`iqs323.c:696`), `s_mult_lsb`·`s_comp_lsb` 정적 변수도 없다. 이것은 Phase 2 설계가 여전히 코드에 연결되지 않은 사문이다. 방향 수렴(선택지_1 권고)은 인정하나 구현 전 결정 항목으로만 남은 상태가 3차 재검증에서도 유지된다.

**판정**: 신규_EEPROM전달 미해소 확인. 방향 수렴, 구현 연결 전무.

---

## 4. 59_종합이 "수렴 확정"으로 분류했으나 3차 재검증에서 재평가 필요한 항목

| 59_종합 수렴 확정 | 3차 repro 재평가 |
|---|---|
| wait_re_ati_done() WDT 갱신 수정 필요 합의 | "합의"는 맞으나 "수렴"으로 분류된 것이 "수정 완료"로 오독될 위험. 코드 미수정. **표현 수정 필요** |
| RTT_TUNING 관련 #error 2종 추가 | `tdc_touch_config.h`에 관련 define 없어 #error 자체가 현재 무효. 코드 미추가. |
| SATURATE_DETECT_ENABLE 기본값 수정 방향 결정 | 방향만 결정됐고 코드에 반영 없음 — "수렴" 표현 과잉. |
| 절전 RESEED→ULP 재부팅 루프 §8 명시 | §8에 명시된 것은 사실이나 해소 방법이 "블로킹_G 해소 후 결정"으로 여전히 미해소. |

---

## 5. 신규 약점이 없거나 59_종합에서 올바르게 처리된 항목

| 항목 | 판정 |
|---|---|
| gate_check() 언더플로 #error 추가 (§2-2) | 설계 방향 수렴. 코드 미반영이나 방향 명확. |
| SATURATE_DETECT_ENABLE=1 + MAX_COUNTS=0 오작동 조합 수정 방향 (§2-9) | 선택지_A(ENABLE=0) 권고 수렴. 코드 미반영이나 방향 명확. |
| Phase 4 크래들 삽입점 `main.c:743` 직전 (57_엣지 신규_D) | 해당 라인 기술은 확인_8 범주로 적절히 처리. |
| 블로킹_K(MAX_WAIT_MS_FOR_WINDOW_CLOSE) 신규 추가 | 합리적 블로킹 추가 — 방향 수렴. |
| R3_CHARGING 비블로킹 3단계 구조 설계 방향 | 방향 수렴. 단 블로킹_H(API 신설) + 신규_A(re_ati_trigger CH_TIMEOUT 덮어쓰기) 해소 전 착수 불가. |

---

## 6. 수렴 여부 판정

**미수렴** — 신규 약점 4건, 기존 할루시 2건 지속.

| ID | 항목 | 심각도 | 구분 |
|---|---|---|---|
| 신규_A | `re_ati_trigger()` CH_TIMEOUT 덮어쓰기 버그 코드 확인 — Phase 3 도입 시 무게이트 Re-ATI 경로 개방 | 치명 | 신규 |
| 신규_B | §3 빌드가드 세트 v2 전체 컴파일 불가 — `tdc_touch_config.h` 신규 define 전무 | 치명 | 신규 |
| 신규_C | Phase −1 WDT 갱신 미삽입 코드 미수정 — 현장 버그 여전히 활성, 59_종합 "수렴" 분류 오표현 | 긴급 | 신규 |
| 신규_D | `s_boot_touch_ignore`·노터치 게이트 통합 race 구체 설계 미완 — 게이트 진행 중 터치 이벤트 상위 도달 경로 존재 | 고위험 | 신규 |
| 지속_블로킹_J | `ATI_SETUP_FULL_LSB` 미정의 (PLACEHOLDER 상태 유지) | 치명 | 지속 |
| 지속_EEPROM전달 | `write_ati_compensation()` 인자 전달 메커니즘 코드 연결 없음 | 치명 | 지속 |

**수렴 가능 조건**: 신규_A(re_ati_trigger 수정) + 신규_B(tdc_touch_config.h define 추가) + 신규_C(WDT 수정 완료 명시) + 신규_D(게이트 진행 구간 억제 설계 확정) + 지속_블로킹_J(DS §5.9 확인) + 지속_EEPROM전달(설계 확정)이 모두 해소되어야 한다. 3회 재검증 누적 결과, **아키텍처 원리는 기각되지 않았으나 구현 전 해소 항목이 감소하지 않고 있다.**

---

## 7. 핵심 6줄

1. **[신규_A 치명] `re_ati_trigger()` CH_TIMEOUT 덮어쓰기 코드 확인**: `re_ati_trigger()`가 `reg.bytes[2]=0x00` 초기화 후 write하므로 `apply_settings()` 말미에서 설정한 CH_TIMEOUT 비활성화(MSB=0x07)가 Phase 3 Re-ATI 호출마다 해제된다 — CH_TIMEOUT 재활성 → stuck-touch 시 자동 Re-ATI 발동 → 노터치 게이트 없는 Re-ATI 경로가 열린다. 59_종합이 확인_13으로 연기한 것은 잘못된 분류이며 즉시 수정 대상이다.

2. **[신규_B 치명] 빌드가드 세트 v2 컴파일 불가**: `tdc_touch_config.h`에 59_종합 §3이 제안한 `TDC_TOUCH_SATURATE_DETECT_ENABLE` 등 신규 define이 전무하므로 `#if TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE` 등의 `#error` 가드가 현재 `#if 0`으로 조용히 무효화되어 있다 — 빌드가드 세트 v2는 코드에 반영 전까지 실제 보호 효과가 없다.

3. **[신규_C 긴급] Phase −1 코드 미수정**: 59_종합이 "수렴 확정"으로 분류한 `wait_re_ati_done()` WDT 갱신 삽입이 코드에 반영되지 않았다 — `ATI_CALIB_MODE=1` 공장 캘리브레이션 경로에서 현장 WDT 만료 재부팅 위험이 여전히 활성이며, "수렴" 표현이 "완료"로 오독될 위험이 있다.

4. **[신규_D 고위험] 게이트 진행 구간 억제 설계 미완**: READY 전이 후 노터치 게이트 RESEED 완료 전 구간에 `s_boot_touch_ignore`가 미설정 상태이면 터치 이벤트가 상위 로직에 즉시 전달된다 — "RESEED 완료 시 clear" 또는 "1 폴링 지연" 설계 모두 이 구간을 완전히 억제하지 못하며 `s_gate_in_progress` 플래그 신설 또는 `s_boot_touch_ignore` 범위 확장이 구체 설계로 확정되어야 한다.

5. **[지속 할루시 2건]** `ATI_SETUP_FULL_LSB` PLACEHOLDER(블로킹_J)와 `write_ati_compensation()` 인자 전달 미연결(신규_EEPROM전달)은 3차 재검증에도 미해소 상태이며, 이 두 항목 없이는 Phase 3·Phase 2 구현 착수 자체가 불가하다.

6. **[수렴 판정] 미수렴**: 아키텍처 원리(공장 절대 기준 → 노터치 게이트 → Re-ATI)는 3회 adversarial 공격에서도 기각되지 않았으나, 매 재검증마다 신규 구현 불가 항목이 발굴되고 있다 — 추가 재검증보다는 Phase −1 코드 수정(WDT 갱신, re_ati_trigger CH_TIMEOUT 보존)과 블로킹_J(DS 확인) 해소를 실측과 병행하는 것이 더 생산적이다.
