---
name: 재검증1_repro — 종합-1차 재검증 (repro 관점)
purpose: 30_종합-1차를 재현성·구현가능성·코드 근거 관점에서 adversarial 공격해 새 약점·할루시·미검증 주장을 발굴
type: 에이전트-로그
maturity: in-progress
tags: [touch, iqs323, adversarial, repro, revalidation, synthesis-attack, v3]
---

# 41 — 재검증1_repro: 30_종합-1차 adversarial 공격 (repro 관점)

**TL;DR**: 30_종합-1차를 재현성·구현가능성·코드 사실 관점에서 공격한다. 발굴된 신규 약점 5건 — ① 부팅 counts 읽기 타이밍이 apply_settings 내 flush/delay 없이 즉시 읽어 Race 창이 종합에서 주장한 것보다 크다, ② FACTORY_NOTOUCH_COUNTS=0 시 tdc_notouch_gate_check()가 `return false`로 RESEED를 영구 차단하나 종합은 이 경우 "단일 RESEED"로 모순 설명한다, ③ wait_re_ati_done() 내에 SYS_WATCHDOG_REFRESH() 가 없음을 코드로 확인했으나 종합은 "필수 명시" 수준으로 처리해 실제 구현 누락 리스크를 과소평가한다, ④ tdc_touch_config.h에 신규 define 6개를 추가한다고 했으나 현재 파일에는 이미 10개 define이 존재하고 일부 네이밍이 기존 컨벤션(TDC_TOUCH_CRX0_DISCHARGE_ENABLE 등)과 충돌 가능성이 있다, ⑤ 종합이 "충전기 연결 시 터치 계속"을 전제로 채택했으나 충전 중 Re-ATI(R3_CHARGING)의 구현 위치와 30초 타이머 관리 방법은 종합에서도 미해결이고 이를 미해결로 명기조차 하지 않았다. "수렴" 판단은 보류 — 신규 약점 있음.

---

## 1. 공격 대상 확인 — 종합-1차 핵심 주장 목록

종합(30)의 주요 주장을 repro 관점에서 하나씩 검증한다.

| 주장 | 위치 | repro 가능성 |
|---|---|---|
| A. write_ati_compensation() 이후 RESEED 게이트화 — 삽입점 L1147 교체 | §2-3 | 코드 확인 — L1141~1158 구조 일치. 기술 자체는 정확 |
| B. FACTORY_NOTOUCH_COUNTS=0 기본값 시 사후검증 skip + 단일 RESEED | §3-3 + §2-2 Phase 1 | **모순 — 신규 결함 발굴** (§2.1) |
| C. wait_re_ati_done() = 최대 1초(10×100ms) | §5-3 + §8.결론6 | 코드 L581~623 확인 — 1초 맞음. 단 WDT 미갱신 미해결 리스크 과소평가 (§2.3) |
| D. FACTORY_NOTOUCH_COUNTS=0 시 gate_check() return false → 게이트 항상 실패 | §3-2 코드 | **gate_check()와 reseed 조건부 로직 사이 비결합 모순** (§2.1) |
| E. tdc_touch_config.h 신규 define 6개 | §2-2 빌드가드 세트 | 현재 파일에 이미 10개 define — 네이밍 충돌 미검토 (§2.4) |
| F. 충전 중 Re-ATI R3_CHARGING 30초 주기 구현 | §4 "100% 근접 달성도" | 구현 위치 미기술, 미해결 목록에도 누락 (§2.5) |
| G. 자기치유 — 손 뗌 후 LTA IIR이 noTouch로 수렴 | §4 + §6 | Beta POR default 불확인 → 자기치유 동작 여부 자체 미확인 [확정 필요] (§2.6) |
| H. 사후 검증 — RESEED 직후 counts 재확인 1회, 재시도 max 3회 | §3-3 | Race 창 분석이 μs~ms 수준이라 했으나 실제 I2C read 왕복 포함 시간 미정 (§2.2) |
| I. counts 읽기 타이밍 — apply_settings() 내 노터치 게이트 직전 | §2-3 + §3 | I2C 통신 후 안정화 delay 없이 즉시 읽는 구조 → noise 오통과 위험 (§2.2) |

---

## 2. 신규 약점 상세

### 2.1 [신규_결함_A] FACTORY_NOTOUCH_COUNTS=0 시 gate_check() return false — "단일 RESEED" 주장과 모순 (치명)

**30 종합의 주장 (§3-2 코드)**:
```c
if (TDC_TOUCH_FACTORY_NOTOUCH_COUNTS == 0u) { return false; }  /* 미캘리브 안전 거부 */
```
COUNTS=0이면 `tdc_notouch_gate_check()`가 **무조건 false**를 반환한다.

**30 종합의 또 다른 주장 (§3-3 말미 + Phase 1 명세)**:
> "FACTORY_NOTOUCH_COUNTS=0 기본값일 때: 사후검증 skip + **단일 RESEED**"
> "GATE_ENABLE=0이면 위 세 값은 참조 안 됨 — 0값 언더플로 위험 없음"

**모순 분석**:
- `GATE_ENABLE=0` 상태: `#else` 브랜치에서 기존 코드 `write_register(0xC0, 0x08, 0x00)` (무조건 RESEED) 유지. 이 경우 gate_check()는 호출되지 않으므로 COUNTS=0 → false 문제가 없다. **이 경로는 정확.**
- `GATE_ENABLE=1` + `FACTORY_NOTOUCH_COUNTS=0` 상태: gate_check()가 false → RESEED 보류 경로로 진입. 종합 §3-3이 "단일 RESEED"라고 한 것은 RESEED 발행을 전제하나, gate_check()가 항상 false이면 RESEED가 **영구 차단**된다. timeout 후 "저신뢰 폴백 RESEED + RTT 경보" 경로가 있지만, 이 폴백 경로는 Phase 1 명세(§2-2)에서 "GATE_ENABLE=0이면 참조 안 됨"이라 기술돼 COUNTS=0 상태에서 게이트 활성화(GATE_ENABLE=1) 시 어떤 경로를 타는지 **명시적으로 기술하지 않았다**.

**결론**: `GATE_ENABLE=1` + `FACTORY_NOTOUCH_COUNTS=0` 조합을 테스트 중에 일시 사용하면 RESEED가 매 부팅마다 BOOT_WAIT_MS(timeout) 동안 대기 후 폴백 RESEED로 처리된다 — 부팅 지연이 발생한다. 종합은 "COUNTS=0이면 비활성"이라는 게이트 전체 의도를 반영하지 않고 gate_check() 코드와 상위 흐름 설명이 서로 다른 전제에서 기술됐다.

**요구 확인**: `GATE_ENABLE=1` + `COUNTS=0` 조합에서 timeout 및 폴백 경로를 명시하거나, `GATE_ENABLE=1`이면 반드시 `COUNTS > 0`임을 `#error`로 강제.

---

### 2.2 [신규_결함_B] apply_settings() 내 counts 읽기 — I2C 안정화 delay 없음 + force_window 비용 미계상 (심각)

**30 종합의 주장 (§3-2, §3-3)**:
> "FACTORY_NOTOUCH_COUNTS 범위 확인(레이어_1만, N샘플 레이어_2 제거 — race 창 확대 방지)"
> "RESEED 직후 counts 재확인 1회"

**코드 실측 근거**:
- `tdc_drv_iqs323_apply_settings()` 내 counts 읽기는 **`read_register()`**를 사용한다.
- `read_register()`는 내부에서 `force_window_open()`을 2회 호출한다(`iqs323.c:233·246`).
- `force_window_open()` 1회 최악 45ms + 20ms 딜레이 × 2 = 최악 130ms.
- 이 130ms 동안 counts 값이 안정화돼 있다고 보장할 근거가 없다 — 특히 MCLR+Auto-ATI 완료 직후 counts가 일시 불안정한 구간이 있을 수 있다.

**종합의 처리**:
- §5-2 확인_5에서 "elapsed_ms 타임아웃 계산을 ci_timer_get_tick() 실시간 계산으로 교체 — force_window_open 최대 45ms가 elapsed_ms에 누락"이라고 인식했으나, 이 문제를 **타임아웃 계산 버그** 수준으로만 기술했다.
- 더 중요한 문제 — **gate_check()에 입력되는 counts 값 자체의 신뢰도** — 는 종합에서 미언급이다. Auto-ATI 완료 직후 IC 내부 LTA/counts가 아직 수렴 중인 상태에서 읽은 1샘플로 노터치 여부를 판별하는 것이 신뢰할 수 있는가 [확정 필요].

**요구 확인**: MCLR+Auto-ATI 완료 직후 IC counts 안정화 소요 시간 실측. 레이어_2(N샘플) 제거 결정이 이 타이밍 신뢰도 문제와 연계되어 있음을 주석으로 명시.

---

### 2.3 [신규_결함_C] wait_re_ati_done() 내 SYS_WATCHDOG_REFRESH() 부재 — 코드로 확인됨, 종합은 "명시 필요" 수준 처리 (심각)

**코드 실측 (L581~623)**:
```c
static bool wait_re_ati_done(void)
{
    ...
    for (int i = 0; i < 10; i++)
    {
        ...
        Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 100);  /* 100ms */
    }
    ...
}
```
루프 전체에 `SYS_WATCHDOG_REFRESH()` 호출 **0건** — 코드로 확인됨.

**30 종합의 처리 (§2-2 Phase 3, §8.결론6)**:
> "[!] 블로킹 루프 내 SYS_WATCHDOG_REFRESH() 명시 삽입 필수 (결함_13_B@21, 결함_9@22)"
> "모든 블로킹 루프에 SYS_WATCHDOG_REFRESH() 명시 필요"

**문제**: 종합은 이것을 "구현 시 주의사항"으로 기술했으나, 실제 Phase 3(Re-ATI 활성화) 구현 시 **기존 `wait_re_ati_done()` 함수를 그대로 호출하면 WDT 갱신이 없어 재부팅**이 발생한다. `wait_re_ati_done()`은 static 함수이며 현재 소수 경로에서만 호출되지만, Phase 3에서 런타임 Re-ATI 경로가 추가되면 WDT 만료 위험이 **즉시 현실화**된다.

**종합이 놓친 점**: `wait_re_ati_done()`을 수정하지 않고 Phase 3를 구현하면 기존 `tdc_drv_iqs323_calib_re_ati()` 경로(ATI_CALIB_MODE)도 동시에 영향받는다 — 공장 캘리브레이션 중에도 WDT 만료 위험이 있다. 이 교차 영향을 종합은 언급하지 않았다.

**요구 조치**: Phase 3 구현 이전에 `wait_re_ati_done()` 함수 수정을 **선행 필수 사항**으로 격상. 현재는 "구현 시 명시"로만 처리돼 구현 누락 가능성이 높다.

---

### 2.4 [신규_결함_D] tdc_touch_config.h 신규 define 6개 — 기존 10개 define과 네이밍 일관성 미검토 (보통)

**코드 실측**: 현재 `tdc_touch_config.h`는 10개 define을 가진다:
- `TDC_BOARD_VARIANT`, `TDC_DRV_IQS323_ATI_DUMP_ENABLE`, `TDC_TOUCH_ATI_CALIB_MODE`, `TDC_TOUCH_ATI_CALIB_LED_ENABLE`, `TDC_TOUCH_SLEEP_MEASURE_MODE`, `TDC_TOUCH_CRX1_VSS_ENABLE`, `TDC_TOUCH_CRX1_REF_ENABLE`, `TDC_TOUCH_CRX1_DUMMY_ENABLE`, `TDC_TOUCH_CRX0_DISCHARGE_ENABLE`, `TDC_TOUCH_CRX0_DISCHARGE_LOG`, `TDC_TOUCH_MARGIN_LOG_ENABLE`, `TDC_TOUCH_MARGIN_LOG_INTERVAL`, `TDC_TOUCH_PROX_THRESHOLD_ENABLE` — 총 13개

**30 종합의 제안 (§2-2)**:
```c
#define TDC_TOUCH_SATURATE_DETECT_ENABLE     1u
#define TDC_TOUCH_NOTOUCH_GATE_ENABLE        0u
#define TDC_TOUCH_FACTORY_CALIB_ENABLE       0u
#define TDC_TOUCH_REATI_ON_NOTOUCH_ENABLE    0u
#define TDC_TOUCH_ATI_FULL_ENABLE            0u
#define TDC_TOUCH_CRADLE_REATI_ENABLE        0u
```
추가 3값: `TDC_TOUCH_FACTORY_NOTOUCH_COUNTS`, `TDC_TOUCH_NOTOUCH_MARGIN_STRICT`, `TDC_TOUCH_NOTOUCH_MARGIN_UP`

**문제**:
- `TDC_TOUCH_FACTORY_CALIB_ENABLE` — 기존 `TDC_TOUCH_ATI_CALIB_MODE`와 기능이 중복될 가능성이 있다. 두 define 모두 "공장 캘리브레이션 모드"를 활성화하는 역할이 유사한데, 기존 `ATI_CALIB_MODE` 빌드가드 블록(`iqs323.c:762~869`)과 신규 `FACTORY_CALIB_ENABLE` 블록을 동시에 관리해야 하는 이중 진입점이 생긴다.
- `TDC_TOUCH_ATI_FULL_ENABLE` — 기존 `TDC_DRV_IQS323_ATI_DUMP_ENABLE`과 용어 충돌 위험. "ATI Full"이 "ATI Mode Full" 활성화를 의미하는지 기존 "ATI 덤프 출력"과 혼동될 수 있다.
- 종합은 이 교차 관계를 검토하지 않았다.

**요구 조치**: 신규 define 추가 전 기존 `ATI_CALIB_MODE` 빌드가드와의 역할 분리를 명확히 하고 `tdc_touch_config.h` 주석 헤더에 define 간 의존 관계 도식 추가.

---

### 2.5 [신규_결함_E] 충전 중 Re-ATI(R3_CHARGING) — 구현 위치·타이머 관리 미해결이 미해결 목록에서 누락 (보통)

**30 종합의 처리**:
- §4 달성도 표에서 "환경 드리프트" 항목: "Phase 3 Re-ATI + LTA IIR 런타임 추적 → ✅ (Phase 3 + Beta 실측 후)"로 달성 가능으로 표기
- §6 미해결 목록에 R3_CHARGING 구현 미기술 항목 없음

**검증8(결함_10@22)**: "충전 중 주기 Re-ATI 구현 위치 미기술"을 명시적 결함으로 발굴했으나, 30 종합의 §5(핵심 쟁점) 및 §6(미해결)에 이 항목이 **반영되지 않았다**.

**결론**: R3_CHARGING 구현이 없으면 Phase 3 "환경 드리프트 ✅" 달성도 평가는 과장이다. 충전 중 Re-ATI 없이는 `절전 진입 안 하는 충전 상태`(= 상시 동작)에서 환경 드리프트를 Runtime Re-ATI로 보정하는 경로가 없다 — LTA IIR 단독으로는 게인(MULT/COMP) 드리프트를 흡수 못 한다는 것이 종합 §1-1 자체의 결론이다.

**요구 조치**: §6 미해결 목록에 "충전 중 주기 Re-ATI 구현 위치·타이머 관리 방식 미기술" 항목 추가. §4 달성도 표에서 환경 드리프트 항목을 "△(R3_CHARGING 구현 위치 확정 시)"로 하향.

---

### 2.6 [수렴 확인] 기존 검증8 결함 중 종합에서 올바르게 처리된 항목

repro 관점에서 아래 항목은 종합이 올바르게 흡수했음을 확인한다:

| 항목 | 종합 처리 | repro 판정 |
|---|---|---|
| 삽입점 11번↔14번 충돌(결함_1@22) | write_ati_compensation 항상 실행, 이후 RESEED만 게이트 | 코드 L1141~1158 구조와 일치. 수렴 |
| EEPROM 구조체 확장 선행 금지(결함_2@22) | CFX 파서 확인 전 파일 수정 금지로 강제 | 수렴 |
| wait_re_ati_done 1.5초 환각(결함_2@27) | "최대 1초"로 정정, 코드 L581~623 근거 | 코드 확인됨. 수렴 |
| 구조체 타입 int×4 통일(결함_7@27) | int×4(CFX 호환)로 통일 명시 | 수렴 |
| 빌드가드 #error 컴파일 타임 강제(결함_15@22) | §2-2에 #if/#error 코드 포함 | 수렴 |
| Beta POR 실측 블로킹(블로킹_D) | §5-1에 블로킹_D 명시, Beta 코드 포함 금지 | 수렴 |
| calib_read_ati 빌드가드(결함_3@27) | §8.결론2에 동일 가드 명시 필요 기술 | 수렴 |

---

## 3. 할루시네이션 여부 검토

### 3.1 종합이 인용한 코드 라인 검증

| 주장 | 인용 위치 | 실측 | 판정 |
|---|---|---|---|
| write_ati_compensation() 위치 | L1141 | L1141: `if (!write_ati_compensation())` 확인 | 정확 |
| RESEED 무조건 발행 | L1147 | L1147: `write_register(..., 0x08, 0x00)` 확인 | 정확 |
| CH_TIMEOUT 비활성화 | L1155 `0x00, 0x07` | L1155: `write_register(..., 0x00, 0x07)` 확인 | 정확 |
| wait_re_ati_done() 최대 1초 | 10×100ms | L581~623: for 10회, Sys_Delay ×100 확인 | 정확 |
| tdc_drv_iqs323_reseed() 절전 전용 | L931~948 | L931: `void tdc_drv_iqs323_reseed(void)` — 절전 파라미터 적용 확인 | 정확 |
| SYS_WATCHDOG_REFRESH() apply_settings 내 | L1139 | L1139: `SYS_WATCHDOG_REFRESH()` 확인 | 정확 |
| wait_re_ati_done() 내 WDT 없음 | (주장만) | L581~623 전체 — SYS_WATCHDOG_REFRESH 0건 | **미기술이나 사실임** |

**할루시 판정**: 종합-1차 자체에서 코드 라인 환각 없음. 단 §2-2 Phase 3 부분에서 "블로킹 루프 내 SYS_WATCHDOG_REFRESH() 명시 삽입 필수"라고 했으나 기존 함수가 이미 해당 함수를 호출하지 않는다는 사실을 확인했으므로 이것은 "추가 지시"가 아니라 "기존 함수 수정이 필수"임을 종합이 충분히 강조하지 않은 것이다.

### 3.2 "자기치유는 현재 내장(Beta POR [확정 필요])" 기술 — 미확인 전제의 이중 기술

**종합 §1-1 층위 3**:
> "자기치유는 현재 내장(Beta POR [확정 필요])."

**문제**: `tdc_drv_iqs323.c` 전체에서 0xB0~0xB4 레지스터 write가 0건임은 확인됐으나, IQS323 데이터시트의 POR 테이블에서 Beta default 값이 0인지는 코드로 확인 불가다. 종합은 "[확정 필요]"라고 표기했으나, 동시에 "자기치유는 현재 내장"이라고 확정적으로 기술했다. **Beta POR default가 0이 아닌 경우 자기치유가 이미 동작 중일 수 있고, 0인 경우 자기치유가 현재 전혀 동작하지 않는다** — 두 해석이 180도 다른데 종합이 "내장"을 기정사실로 기술한 것은 과잉 단정이다 [추정].

---

## 4. 미해결 추가 발굴 — 종합 §6에 추가 필요한 항목

| 항목 | 이유 |
|---|---|
| `GATE_ENABLE=1` + `COUNTS=0` 조합 동작 경로 | gate_check() return false → 폴백 경로 명세 필요 (§2.1) |
| Auto-ATI 완료 직후 IC counts 안정화 소요 시간 | 게이트 counts 1샘플 신뢰도 근거 없음 (§2.2) |
| 충전 중 Re-ATI 구현 위치·타이머 관리 방식 | §5·§6에서 누락됨 (§2.5) |
| `TDC_TOUCH_FACTORY_CALIB_ENABLE` ↔ `TDC_TOUCH_ATI_CALIB_MODE` 역할 분리 | 이중 진입점 혼란 (§2.4) |

---

## 5. 최종 판정 — "수렴" 여부

**보류 — 수렴 아님.** 신규 약점 5건 발굴:

1. **[신규_결함_A]** `GATE_ENABLE=1` + `COUNTS=0` 조합에서 gate_check() return false가 "단일 RESEED" 기술과 모순
2. **[신규_결함_B]** apply_settings() 내 counts 1샘플 읽기의 타이밍 신뢰도 및 force_window 비용 미계상
3. **[신규_결함_C]** `wait_re_ati_done()` 내 SYS_WATCHDOG_REFRESH() 0건 코드 확인 — 종합이 "구현 시 추가" 수준으로 처리해 기존 함수 수정 필수성 과소평가
4. **[신규_결함_D]** 신규 define 6개와 기존 13개 define 간 역할 중복·네이밍 충돌 미검토
5. **[신규_결함_E]** R3_CHARGING 구현 미기술이 미해결 목록에 누락 + §4 환경 드리프트 달성도 과장

---

## 6. 핵심 6줄

1. **[신규_A] GATE+COUNTS=0 모순**: `GATE_ENABLE=1` + `FACTORY_NOTOUCH_COUNTS=0` 조합에서 gate_check()가 return false로 RESEED를 영구 차단하나 종합은 이 경우 "단일 RESEED"라 기술해 폴백 경로가 무엇인지 불명확하다 — `#error` 또는 폴백 명세 추가 필요.
2. **[신규_B] counts 1샘플 신뢰도 미검증**: apply_settings() 내 노터치 게이트 직전 counts 읽기는 Auto-ATI 완료 직후 IC 안정화 delay 없는 1샘플이며 force_window 최악 130ms 비용도 미계상 — 게이트 판별 신뢰도의 실측 근거 없음 [확정 필요].
3. **[신규_C] wait_re_ati_done() WDT 0건 확인**: 코드 L581~623에서 SYS_WATCHDOG_REFRESH() 호출 없음을 실측 확인 — Phase 3 Re-ATI 활성화 시 기존 함수 수정 없이 호출하면 즉시 WDT 만료 재부팅 위험, 종합 처리 수준이 부족하다.
4. **[신규_D] define 이중 진입점**: 신규 `TDC_TOUCH_FACTORY_CALIB_ENABLE`이 기존 `TDC_TOUCH_ATI_CALIB_MODE`와 역할 중복 가능 — 두 빌드가드가 별개 코드 블록을 제어하면 공장 캘리브레이션 진입점이 이중화돼 관리 혼란을 야기한다.
5. **[신규_E] R3_CHARGING 미해결 누락**: 충전 중 주기 Re-ATI 구현 위치가 종합 §5·§6 미해결 목록에서 빠져 있어 §4 "환경 드리프트 ✅" 달성도 평가가 과장이다 — 게인 드리프트 경로가 없으면 Phase 3만으로 달성 불가.
6. **[자기치유 과잉 단정]**: 종합 §1-1 "자기치유 현재 내장(Beta POR [확정 필요])"은 POR default를 확인하지 않고 내장을 기정사실로 기술한 과잉 단정 — Beta POR=0이면 자기치유 전무, 0 아닌 값이면 이미 동작 중으로 결론이 180도 달라진다 [추정].
