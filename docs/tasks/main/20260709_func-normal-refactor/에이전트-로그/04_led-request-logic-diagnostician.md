---
name: led-request-logic-diagnostician
purpose: func_normal() 내 "LED source requests (Rev.3)" 블록(배터리/ISD/매핑 3분기)의 책임·static 상태·순서 의존성·결합 관계 진단 — 분해 계획 수립의 근거 자료
type: agent-log
maturity: experimental
tags: [main.c, led, refactor, func_normal, hysteresis, static-state]
---

# LED source requests (Rev.3) 블록 진단 보고서

> [!NOTE]
> **TL;DR**: `func_normal()`의 LED 3분기(배터리/ISD/매핑)는 실행 순서 의존 체인(배터리→ISD→매핑)을 가진다. `prev_batt_st`·`s_map_low_active`는 파일 전역에서 참조되지 않는 순수 지역 상태이므로 각 신규 함수 내부에 `static`으로 그대로 옮겨도 behavior-preserving이 보장된다. ISD 블록은 `batt_st`(배터리 산출물)를 재사용하고, 매핑 블록은 `isd_conn`(ISD 산출물)을 재사용 — 이 데이터 흐름이 함수 분해 시 시그니처와 호출 순서를 결정한다. `led_set_isd_conn_state()` 호출 순서(led_request 이후)는 ISR race 방지용이므로 절대 보존 필요.

## 0. 라인 번호 안내

작업 지시서의 근사 라인(526~643)과 현재 워킹트리 실측 라인이 다소 어긋난다(`git status`상 `main.c`가 unstaged 수정 상태 — 이전 세션에서 라인이 밀렸을 가능성). 본 문서는 **실측 라인 번호**(2026-07-09 현재 워킹트리, `Read` 도구로 직접 확인)를 기준으로 인용한다. 3-분기 블록 자체(배터리/ISD/매핑, `#ifdef ENABLE_UI_CMD` 오버라이드 패턴 포함)는 지시서 설명과 내용상 완전히 일치하므로 분석 대상 식별에는 문제 없음.

- 블록 전체: `main.c:538~655` (`/* === LED source requests (Rev.3) === */` 주석 ~ 닫는 `}`)
- 배터리 하위 블록: `542~578`
- ISD 하위 블록: `580~618`
- 매핑 하위 블록: `620~654`

## 1. 배터리 블록 (main.c:542~578)

```c
542	                /* Battery (SS4.5) -- hysteresis +/-2%
543	                 * Rev.5 추가: QCC로부터 0x34(Power info) 수신 전까지는 배터리 상태가
544	                 *            EN__SND_BATT_STATE_RESET 이고 percent = 0 이므로,
545	                 *            pct 기반 판정(pct<10 → BATT_CRITICAL)이 그대로 적용되면
546	                 *            부팅 초기에 노란색 LED가 잠깐 켜지는 현상이 발생한다.
547	                 *            따라서 RESET 상태에서는 pct 판정을 건너뛰고 IDLE로 요청한다. */
548	#ifdef ENABLE_UI_CMD
549	                bool ovr_batt_active = tdc_ui_command_override_battery_active();
550	                int  pct             = ovr_batt_active ? (int) tdc_ui_command_override_battery_percent() : snd_batt_get_percent();
551	#else
552	                bool ovr_batt_active = false;
553	                int  pct             = snd_batt_get_percent();
554	#endif
555	                static led_state_t prev_batt_st = LED_ST_IDLE;
556	                led_state_t        batt_st;
557	
558	                if (!ovr_batt_active && snd_batt_get_state() == EN__SND_BATT_STATE_RESET)
559	                {
560	                    batt_st = LED_ST_IDLE;
561	                }
562	                else if (pct < 40 /*10*/)
563	                    batt_st = LED_ST_BATT_CRITICAL;
564	                else if (pct < 41 /*12*/ && prev_batt_st == LED_ST_BATT_CRITICAL)
565	                    batt_st = LED_ST_BATT_CRITICAL;
566	                else if (pct >= 65 /*80*/)
567	                    batt_st = LED_ST_BATT_READY;
568	                else if (pct >= 64 /*78*/ && prev_batt_st == LED_ST_BATT_READY)
569	                    batt_st = LED_ST_BATT_READY;
570	                else
571	                    batt_st = LED_ST_BATT_MID;
572	
573	                prev_batt_st = batt_st;
574	#ifdef ENABLE_UI_CMD
575	                if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
576	#endif
577	                    led_request(LED_SRC_BATTERY, batt_st);
```
(라인 번호는 실측 기준 1개씩 밀려 있을 수 있음 — 논리 인용은 위 §0 실측 라인 538~655 범위 내에서 정확함)

### 1.1 임계값 히스테리시스 로직 설명

현재 하드코딩 값(40/41, 65/64)과 주석에 남은 옛값(10/12, 80/78)을 비교하면 설계 의도가 드러난다:

| 상태 전이 | 진입 조건 | 유지(sticky) 조건 | 이탈 |
|---|---|---|---|
| → CRITICAL | `pct < 40` (즉 pct ≤ 39) | `pct < 41 && prev==CRITICAL` → pct==40일 때 직전이 CRITICAL이면 유지 | `pct ≥ 41`이면 이탈 |
| → READY | `pct ≥ 65` | `pct ≥ 64 && prev==READY` → pct==64일 때 직전이 READY면 유지 | `pct < 64`이면 이탈 |
| → MID | 위 두 조건 모두 불성립 | — | — |

즉, **진입 임계값과 이탈 임계값을 다르게 두어(hysteresis) pct가 경계값 부근에서 오락가락(chattering)할 때 LED 상태가 매 iteration 깜빡이는 것을 방지**하는 구조다. 옛 주석값(10/12, 80/78)은 진입/이탈 간 2%p 폭의 sticky band였으나, 현재 값(40/41, 65/64)은 **sticky band가 딱 1%p(pct==40 또는 pct==64)로 좁아져** 있다. 이는 배터리 percent 산출 방식이 바뀌어 임계값 자체가 재조정된 결과로 추정되며, 리팩토링 범위에서는 **값을 그대로 보존**해야 한다(사양 변경 아님, 지시사항 준수).

### 1.2 `prev_batt_st`(static)의 역할

- **읽기**: 564행·568행 조건식에서 "이번 계산 시작 시점의 직전 확정 상태"를 참조 — 현재 pct가 sticky band(40 또는 64)에 있을 때 CRITICAL/READY를 유지할지 판정하는 유일한 근거.
- **쓰기**: 573행에서 이번 iteration에 확정된 `batt_st`를 다음 iteration을 위해 저장. **UI 오버라이드로 최종 `led_request()` 송출이 억제되더라도(575행 가드) 이 저장은 항상 실행됨** — 즉 상태 머신 자체는 오버라이드와 무관하게 매 iteration 갱신된다.
- **생존 범위**: 이 변수는 `main.c` 파일 스코프 밖에서 참조되지 않음(grep 확인, `src/2__cm3` 전체 검색 결과 `main.c` 외 매치 0건). 단, `static`이므로 프로그램 전역 수명 — `func_normal()`이 절전(`func_sleep()`) 후 재호출되어도 값이 리셋되지 않고 유지된다(외곽 `while(1){ func_normal(); func_sleep(); }` 구조, main.c:323~327). 함수 분해 시 이 "절전 사이클을 넘나드는 지속성"도 그대로 보존해야 함.

### 1.3 함수 분해 시 static 처리 제안

**결론: 신규 함수 내부에 `static`으로 그대로 유지(옵션 A)를 권장.** 근거:
- 이 변수를 참조하는 코드는 이 블록 하나뿐(호출자·타 모듈 참조 없음) → 굳이 호출자가 상태를 들고 다니며 인자로 넘길 이유가 없음.
- C의 함수-지역 `static`은 파일-스코프 `static`과 동일하게 링크 유닛 수명 동안 값을 유지하므로, `main()`의 `while(1)` 구조·`func_normal()` 재호출 패턴과 무관하게 지금과 완전히 동일한 지속성을 가짐 → behavior-preserving 요건 충족.
- 옵션 B(호출자가 `led_state_t *prev_batt_st_io` 같은 포인터를 들고 있다가 전달)는 `func_normal()`에 새 지역변수를 추가해야 하고, 캡슐화도 나빠짐(배터리 LED 세부 상태를 호출자가 알아야 함) — 이번처럼 "그 함수 하나만 쓰는 상태"에는 과설계.

```c
/* 제안 시그니처 */
static led_state_t tdc_led_request_battery(bool ovr_batt_active, int pct, bool batt_reset_state, bool led_override_active);
/* 반환값: 이번 iteration에 확정된 batt_st (ISD 블록이 재사용하므로 반환 필요) */
```

인자 `batt_reset_state`는 558행의 `snd_batt_get_state() == EN__SND_BATT_STATE_RESET` 판정 결과를 호출자가 미리 평가해 넘기거나, 함수 내부에서 직접 `snd_batt_get_state()`를 호출해도 무방(순수 조회, 부수효과 없음 — 어느 쪽이든 behavior 동일). `led_override_active`는 `tdc_ui_command_is_led_override(LED_SRC_BATTERY)` 결과.

## 2. ISD 블록 (main.c:580~618)

```c
580	                    /* ISD (SS4.6) */
581	#ifdef ENABLE_UI_CMD
582	                bool isd_conn = tdc_ui_command_override_isd_active() ? tdc_ui_command_override_isd_value() : isd_state.conneded_ISD;
583	#else
584	                bool isd_conn = isd_state.conneded_ISD;
585	#endif
586	#ifdef ENABLE_UI_CMD
587	                if (!tdc_ui_command_is_led_override(LED_SRC_ISD))
588	#endif
589	                /* IMPORTANT: 내부기 연결 해제시 위에서 구한 배터리 레벨에 대한 LED를 켜도록 유도했다. */
590	                {
591	                    if (isd_conn)
592	                    {
593	                        if (readLED_indicatorOnOff() == 1)
594	                        {
595	                            led_request(LED_SRC_ISD, LED_ST_IN_USE);
596	                        }
597	                        else
598	                        {
599	                            led_request(LED_SRC_ISD, LED_ST_NONE);
600	                        }
601	                    }
602	                    else
603	                    {
604	                        led_request(LED_SRC_ISD, LED_ST_NONE);
605	                        led_request(LED_SRC_BATTERY, batt_st);
606	                    }
607	
608	                    /* s_req[LED_SRC_ISD] 확정 후 게이트 갱신 — 연결 해제 전환 시
609	                     * s_isd_conn=0 과 s_req[ISD]=NONE 사이에 TIMER_3 ISR 이 끼어들어
610	                     * 1-tick IN_USE(백색) 잔상이 뜨던 race 를 방지하기 위해 분기 뒤로 이동. */
611	                    led_set_isd_conn_state(isd_conn);
612	                }
```

### 2.1 `led_set_isd_conn_state()`가 분기 밖(뒤)으로 이동한 이유

`LedOutput.c:87,709`를 확인하면 `s_isd_conn`은 **`led_arbiter_tick()`**(Timer 3 ISR에서 구동, `compute_best_state()` 내부 709행)이 소비하는 전역 상태다. 사용자가 LED off 설정(`readLED_indicatorOnOff()==2`)일 때, `s_isd_conn==1`이면 ERROR/POWER 외 소스를 억제하지 않고 그대로 통과시키는 게이트 역할을 한다.

main 루프는 두 개의 별도 전역 상태를 갱신한다 — `s_req[LED_SRC_ISD]`(`led_request()`가 씀)와 `s_isd_conn`(`led_set_isd_conn_state()`가 씀). 이 둘은 **원자적으로 갱신되지 않으므로**, ISD 연결이 끊기는 전환 순간에 두 호출 사이로 ISR(`led_arbiter_tick`)이 끼어들면 "새 `s_isd_conn=0`이지만 아직 `s_req[ISD]`는 이전 값(IN_USE)" 또는 그 반대의 불일치 조합이 한 틱 동안 노출될 수 있다 — 주석이 말하는 **1-tick IN_USE(백색) 잔상 race**다.

`led_set_isd_conn_state(isd_conn)`을 `led_request(LED_SRC_ISD, ...)` 호출들(591~606) **뒤로** 옮긴 것은, `s_req[LED_SRC_ISD]`가 이번 iteration의 최종값으로 확정된 **직후**에 `s_isd_conn`을 갱신함으로써 두 상태가 불일치로 노출되는 창(window)을 최소화하기 위함이다(완전한 원자성 보장은 아니지만, "먼저 س_req 확정 → 그다음 s_isd_conn 확정" 순서를 지켜야 잔상 위험이 줄어듦).

### 2.2 함수 분해 시 순서 보존 방안

**분해해도 이 순서(led_request 확정 → led_set_isd_conn_state)는 함수 내부에 그대로 유지**해야 한다 — 즉 `led_set_isd_conn_state()` 호출을 새 함수 `tdc_led_request_isd()`의 **가장 마지막 statement**로 두고, 그 함수를 호출하는 `func_normal()` 쪽에서 순서를 바꾸거나 이 호출만 따로 빼서 나중에 실행하는 일이 없도록 한다(즉, 이 호출을 함수 캡슐 밖으로 노출하지 말 것 — 캡슐 내부에 봉인해야 순서 실수를 원천 차단).

```c
/* 제안 시그니처 */
static bool tdc_led_request_isd(bool isd_conn_raw, bool led_override_active, led_state_t batt_st);
/* 반환값: isd_conn (매핑 블록이 재사용하므로 반환 필요)
 * 주의: led_override_active(=true, 즉 override 중)일 때도 s_isd_conn 갱신 자체는
 *       원본 코드상 587행 가드에 걸려 611행까지 통째로 스킵된다 — 이 "오버라이드 시
 *       s_isd_conn도 갱신 안 함" 동작을 함수 분해 후에도 그대로 보존해야 함(가드를
 *       함수 진입부 조기 return 이 아니라 611행 호출까지 포함하는 전체 블록에 걸어야 함). */
```

`readLED_indicatorOnOff()`는 이 블록 로컬 관심사(다른 블록과 공유 없음, grep 확인 완료)이므로 함수 내부에서 직접 호출해도 무방.

## 3. 매핑 블록 (main.c:620~654)

```c
620	                /* Mapping (SS4.4) — 배터리 레벨(LOW 임계 20%) × ISD 연결 여부 4종 분기.
621	                 * LOW 진입 pct ≤ 20, 해제 pct ≥ 22 (±2% 히스테리시스). */
622	#ifdef ENABLE_UI_CMD
623	                bool map_conn = tdc_ui_command_override_map_active() ? tdc_ui_command_override_map_value() : BLE_communicationState.mappingConnection;
624	#else
625	                bool map_conn = BLE_communicationState.mappingConnection;
626	#endif
627	                static bool s_map_low_active = false;
628	                if (pct <= 20)
629	                    s_map_low_active = true;
630	                else if (pct >= 22)
631	                    s_map_low_active = false;
632	                    /* pct == 21 구간은 직전 상태 유지 */
633	#ifdef ENABLE_UI_CMD
634	                if (!tdc_ui_command_is_led_override(LED_SRC_MAPPING))
635	#endif
636	                {
637	                    if (map_conn)
638	                    {
639	                        led_state_t map_st;
640	                        if (s_map_low_active)
641	                        {
642	                            map_st = isd_conn ? LED_ST_MAPPING_ISD_BATT_LOW : LED_ST_MAPPING_NO_ISD_BATT_LOW;
643	                        }
644	                        else
645	                        {
646	                            map_st = isd_conn ? LED_ST_MAPPING_ISD_BATT_READY : LED_ST_MAPPING_NO_ISD_BATT_READY;
647	                        }
648	                        led_request(LED_SRC_MAPPING, map_st);
649	                    }
650	                    else
651	                    {
652	                        led_request(LED_SRC_MAPPING, LED_ST_NONE);
653	                    }
654	                }
```

### 3.1 `s_map_low_active`(static)와 pct 히스테리시스(20/22)

배터리 블록과 **동형 구조**이나 구현 스타일이 다르다:
- 배터리 블록은 "진입 조건 + (sticky 조건 && prev==상태)"를 매 분기마다 반복 기술.
- 매핑 블록은 **`s_map_low_active`라는 불리언 래치**를 별도로 유지하며, `pct<=20`이면 true로 래치, `pct>=22`면 false로 해제, `pct==21`(중간 1%p band)은 이전 값을 그대로 유지(632행 주석). 즉 sticky band 폭이 2%p(pct==21 하나뿐이므로 실제로는 1%p) — 배터리 블록과 마찬가지로 "진입/해제 임계값을 다르게 둬 채터링 방지"라는 동일 목적이나, **코드 형태는 상태 플래그를 먼저 확정한 뒤 그 플래그로 LED 상태를 계산**하는 2단계 구조라 배터리 블록의 "즉석 if-else 체인" 방식보다 가독성이 좋음(참고할 리팩토링 스타일 후보).
- `s_map_low_active` 역시 `main.c` 밖에서 참조 없음(grep 확인) → §1.3과 동일 논리로 **신규 함수 내부 static 유지**가 behavior-preserving.

### 3.2 `#ifdef ENABLE_UI_CMD` 오버라이드 패턴 비교

3개 블록 모두 **동일한 2단 오버라이드 패턴**을 공유한다:

| 단계 | 배터리 | ISD | 매핑 |
|---|---|---|---|
| ① 입력값 오버라이드 (raw override) | `ovr_batt_active` → `pct` 대체 (549~550) | `tdc_ui_command_override_isd_active()` → `isd_conn` 대체 (582) | `tdc_ui_command_override_map_active()` → `map_conn` 대체 (623) |
| ② 출력 게이트 오버라이드 (최종 `led_request` 억제) | `tdc_ui_command_is_led_override(LED_SRC_BATTERY)` (575) | `tdc_ui_command_is_led_override(LED_SRC_ISD)` (587) | `tdc_ui_command_is_led_override(LED_SRC_MAPPING)` (634) |

3개 블록이 **완전히 동형** — 함수 분해 시 이 2단 패턴을 공통 헬퍼(예: 인자로 `src`를 받는 매크로나 인라인 헬퍼)로 묶는 것도 가능하나, 이번 작업 범위는 "구조 개선(분해)"이지 "패턴 통합"까지 요구되지 않았으므로 **1차 분해에서는 각 함수 내부에 그대로 복제 유지**를 권장(과설계 방지, behavior-preserving 리스크 최소화). 패턴 통합은 별도 후속 작업으로 분리 제안.

## 4. 3개 블록 간 공유 입력 · 상호 참조 관계

| 항목 | 산출 블록 | 소비 블록 | 비고 |
|---|---|---|---|
| `pct` (int, 배터리 %) | 배터리 블록 내부 계산 (549~550) | 배터리(자기 자신) + **매핑 블록(628, 630)** | 동일 enclosing 스코프라 원본은 변수 재사용. 분해 시 `func_normal()`이 1회 계산해 양쪽에 파라미터로 전달해야 함(각 함수가 따로 재조회하면 `tdc_ui_command_override_*` 재호출 자체는 무해하나 굳이 중복 유지 불필요) |
| `isd_state.conneded_ISD` (bool) | `isd_interface()` 호출 결과 (506행, LED 블록 진입 전 이미 확정) | ISD 블록(582/584)에서 raw 입력으로 소비 | LED 블록 외부 산출물, 3블록 중 어느 것도 이 값을 다시 쓰지 않음 |
| `BLE_communicationState.mappingConnection` (bool) | `bleCommunication()` 호출 결과 (515행) | 매핑 블록(623/625)에서 raw 입력으로 소비 | 526행 분기(매핑모드 전환)에서도 별도 사용되나 LED 3블록과는 독립 |
| `batt_st` (led_state_t) | **배터리 블록 산출물** (556~573) | **ISD 블록**이 재사용 — ISD 미연결 시 604~605에서 `led_request(LED_SRC_BATTERY, batt_st)` 재송출 | 핵심 순서 의존: 배터리 블록이 ISD 블록보다 반드시 먼저 실행돼야 함 |
| `isd_conn` (bool, 오버라이드 반영 후 확정값) | **ISD 블록 산출물** (582/584에서 확정) | **매핑 블록**이 재사용 — 642/646에서 `isd_conn ? ... : ...` 분기 | 핵심 순서 의존: ISD 블록이 매핑 블록보다 반드시 먼저 실행돼야 함 |
| `prev_batt_st` (static) | 배터리 블록 내부 전용 | 배터리 블록(자기 자신, 다음 iteration) | 타 블록 미참조 |
| `s_map_low_active` (static) | 매핑 블록 내부 전용 | 매핑 블록(자기 자신, 다음 iteration) | 타 블록 미참조 |

**핵심 결론**: 실행 순서는 반드시 **배터리 → ISD → 매핑**이어야 한다(현재 코드 순서와 동일). 배터리는 독립적으로 먼저 계산 가능(외부 의존 없음, `pct`·`snd_batt_get_state()`만 조회). ISD는 배터리의 `batt_st`를 필요로 하므로 배터리 이후. 매핑은 ISD의 `isd_conn`을 필요로 하므로 ISD 이후. 이 순서를 어기면 ISD 블록의 604~605행(ISD 미연결 시 배터리 LED 재송출)과 매핑 블록의 642/646행(ISD 연결 여부에 따른 매핑 LED 색상 분기)이 이전 iteration의 낡은 값을 참조하게 되어 **행동이 바뀐다** — behavior-preserving 위반 리스크의 최대 지점.

## 5. 분해 제안 (구체 시그니처)

```c
/* main.c 파일 스코프 static — 각 함수 정의 직전 또는 함수 내부에 위치.
 * (main.c 파일 안에서만 쓰이므로 extern 노출 불필요, static 유지) */

#define BATT_CRITICAL_ENTER_PCT   40   /* 구주석 옛값 10 */
#define BATT_CRITICAL_EXIT_PCT    41   /* 구주석 옛값 12 */
#define BATT_READY_ENTER_PCT      65   /* 구주석 옛값 80 */
#define BATT_READY_EXIT_PCT       64   /* 구주석 옛값 78 */
#define MAP_LOW_BATT_ENTER_PCT    20
#define MAP_LOW_BATT_EXIT_PCT     22

/* 1) 배터리 — 반환값 batt_st 는 ISD 블록이 필요로 함 */
static led_state_t tdc_led_request_battery(int pct, bool ovr_batt_active, bool batt_is_reset_state)
{
    static led_state_t prev_batt_st = LED_ST_IDLE;   /* 함수 내부 static 유지 — behavior-preserving */
    led_state_t        batt_st;

    if (!ovr_batt_active && batt_is_reset_state)
    {
        batt_st = LED_ST_IDLE;
    }
    else if (pct < BATT_CRITICAL_ENTER_PCT)
        batt_st = LED_ST_BATT_CRITICAL;
    else if (pct < BATT_CRITICAL_EXIT_PCT && prev_batt_st == LED_ST_BATT_CRITICAL)
        batt_st = LED_ST_BATT_CRITICAL;
    else if (pct >= BATT_READY_ENTER_PCT)
        batt_st = LED_ST_BATT_READY;
    else if (pct >= BATT_READY_EXIT_PCT && prev_batt_st == LED_ST_BATT_READY)
        batt_st = LED_ST_BATT_READY;
    else
        batt_st = LED_ST_BATT_MID;

    prev_batt_st = batt_st;   /* 오버라이드 여부와 무관하게 항상 갱신 — 원본 574행 동작 보존 */

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
        led_request(LED_SRC_BATTERY, batt_st);

    return batt_st;
}

/* 2) ISD — batt_st 를 인자로 받고(배터리 미연결 시 재송출용), isd_conn 을 반환(매핑 블록이 필요) */
static bool tdc_led_request_isd(bool isd_conn, led_state_t batt_st)
{
#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_ISD))
#endif
    {
        if (isd_conn)
        {
            led_request(LED_SRC_ISD, (readLED_indicatorOnOff() == 1) ? LED_ST_IN_USE : LED_ST_NONE);
        }
        else
        {
            led_request(LED_SRC_ISD, LED_ST_NONE);
            led_request(LED_SRC_BATTERY, batt_st);
        }

        /* 순서 고정: led_request() 확정 직후 반드시 마지막에 호출 — race 방지 주석(main.c:608~610) 근거 보존 */
        led_set_isd_conn_state(isd_conn);
    }

    return isd_conn;
}

/* 3) 매핑 — isd_conn 을 인자로 받음 */
static void tdc_led_request_mapping(int pct, bool map_conn, bool isd_conn)
{
    static bool s_map_low_active = false;   /* 함수 내부 static 유지 */

    if (pct <= MAP_LOW_BATT_ENTER_PCT)
        s_map_low_active = true;
    else if (pct >= MAP_LOW_BATT_EXIT_PCT)
        s_map_low_active = false;
    /* pct 중간 구간은 직전 상태 유지 */

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(LED_SRC_MAPPING))
#endif
    {
        if (map_conn)
        {
            led_state_t map_st = s_map_low_active
                ? (isd_conn ? LED_ST_MAPPING_ISD_BATT_LOW  : LED_ST_MAPPING_NO_ISD_BATT_LOW)
                : (isd_conn ? LED_ST_MAPPING_ISD_BATT_READY : LED_ST_MAPPING_NO_ISD_BATT_READY);
            led_request(LED_SRC_MAPPING, map_st);
        }
        else
        {
            led_request(LED_SRC_MAPPING, LED_ST_NONE);
        }
    }
}

/* func_normal() 내부 호출부 (원본 541~655행 대체) */
{
    bool ovr_batt_active = false;
    int  pct;
#ifdef ENABLE_UI_CMD
    ovr_batt_active = tdc_ui_command_override_battery_active();
    pct = ovr_batt_active ? (int) tdc_ui_command_override_battery_percent() : snd_batt_get_percent();
#else
    pct = snd_batt_get_percent();
#endif
    bool batt_is_reset_state = (snd_batt_get_state() == EN__SND_BATT_STATE_RESET);

    led_state_t batt_st = tdc_led_request_battery(pct, ovr_batt_active, batt_is_reset_state);

    bool isd_conn_raw;
#ifdef ENABLE_UI_CMD
    isd_conn_raw = tdc_ui_command_override_isd_active() ? tdc_ui_command_override_isd_value() : isd_state.conneded_ISD;
#else
    isd_conn_raw = isd_state.conneded_ISD;
#endif
    bool isd_conn = tdc_led_request_isd(isd_conn_raw, batt_st);

    bool map_conn;
#ifdef ENABLE_UI_CMD
    map_conn = tdc_ui_command_override_map_active() ? tdc_ui_command_override_map_value() : BLE_communicationState.mappingConnection;
#else
    map_conn = BLE_communicationState.mappingConnection;
#endif
    tdc_led_request_mapping(pct, map_conn, isd_conn);
}
```

### 5.1 매직넘버 상수화 대상 (값 불변, 이름만 부여)

| 상수명 | 값 | 원본 라인 |
|---|---|---|
| `BATT_CRITICAL_ENTER_PCT` | 40 | 562 |
| `BATT_CRITICAL_EXIT_PCT` | 41 | 564 |
| `BATT_READY_ENTER_PCT` | 65 | 566 |
| `BATT_READY_EXIT_PCT` | 64 | 568 |
| `MAP_LOW_BATT_ENTER_PCT` | 20 | 628 |
| `MAP_LOW_BATT_EXIT_PCT` | 22 | 630 |

주석에 남은 옛값(10/12/80/78)은 `#define` 옆 주석으로 그대로 이관해 이력 보존 권장(값 변경 아님, 순수 이름 부여).

### 5.2 오버라이드 원시값(raw override) 계산 위치

`ovr_batt_active`/`isd_conn_raw`/`map_conn`의 **1단계 입력 오버라이드**(`tdc_ui_command_override_*`) 계산은 각 신규 함수 안으로 넣지 않고 **`func_normal()` 호출부에 남겨두는 편**을 권장한다. 이유: 이 계산에는 `ENABLE_UI_CMD` 매크로 분기가 있고, 이를 함수 내부로 옮기면 각 신규 함수가 `tdc_ui_command.h`에 개별 의존하게 되어 결합도가 오히려 늘어난다. 호출부에서 값을 확정해 파라미터로 넘기면 3개 신규 함수는 "이미 확정된 bool/int 값 + 오버라이드 게이트(`tdc_ui_command_is_led_override`)"만 알면 되므로 더 단순해진다. (단, §5의 예시 코드는 게이트 체크(2단계, 출력 억제용)는 각 함수 내부에 유지 — 이건 각 함수 고유 소스 ID와 결합돼 있어 내부가 자연스러움.)

## 6. 검증 체크리스트 (분해 후 대조용)

- [ ] `prev_batt_st`·`s_map_low_active` 갱신이 오버라이드 활성 여부와 무관하게 매 iteration 실행되는가 (원본 574/628~631행 동작)
- [ ] 배터리 → ISD → 매핑 호출 순서가 유지되는가
- [ ] `led_set_isd_conn_state()`가 ISD 블록의 `led_request()` 호출들보다 반드시 뒤에 실행되는가
- [ ] ISD 미연결 시 배터리 LED 재송출(`led_request(LED_SRC_BATTERY, batt_st)`)이 유지되는가
- [ ] 매직넘버 6개(§5.1) 값이 하나도 바뀌지 않았는가(진입/이탈 값 순서 오기입 주의 — 예: EXIT을 ENTER로 착각하는 실수)
- [ ] `#ifdef ENABLE_UI_CMD` 블록 없이 컴파일(매크로 미정의 빌드)도 동일하게 동작하는가
