---
name: 06_adversarial-verify
purpose: 01~05 구역 분석 문서(static/지역 변수 생명주기·구역 간 결합)를 main.c 원본과 대조해 반증 검증
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: stable
tags: [main.c, func_normal, refactor, adversarial-verify, coverage-gap]
---

## TL;DR

01~05 문서의 static 변수 목록(`fake_0x34`/`fake_0x34_done`/`prev_batt_st`/`s_map_low_active`)과 `qcc_batt_timeout`/`qcc_timeout_poweroff_started` 생명주기, 04번 문서의 LED 분해(598행류 배터리 재송출·604행류 `led_set_isd_conn_state()` race 방지 이동)는 **원본 코드와 전부 일치**한다. 다만 원본을 처음부터 직접 재조사한 결과 **불일치 2건**을 발견했다 — ① `conneded_ISD`/`mappingConnection`(main.c:347-348) 지역변수가 선언 후 어디서도 읽히거나 쓰이지 않는 완전한 dead code인데 어느 문서도 "죽은 변수"로 명시하지 않았고(01번은 존재만 나열), ② 5개 구역의 실제 담당 범위가 오케스트레이터 원래 지시(320~723행)를 그대로 따르는 바람에 **func_normal()의 진짜 끝(~742행, `return 0;`)까지 19줄이 통째로 분석 밖에 남았다** — 이 구간에는 `func_sleep()`과 동일하게 정상 반환하지 않는(watchdog reset으로 재부팅하는) 또 다른 탈출 경로 `fake_func_sleep()`(`tdc_get_fake_op_mode()==1`일 때 진입)가 있는데 6개 문서 중 어느 것도 이 함수를 언급하지 않는다.

## 검증 방법

- `main.c`의 `func_normal()` 실측 범위를 직접 Read로 재확인: 함수 시작 `main.c:334`(`int func_normal(void)`), 끝 `main.c:741`(`return 0;`)/`742`(닫는 `}`) — 오케스트레이터 원 지시서(`00_오케스트레이터-입력.md`)의 "320~723행"보다 실제로 14~19줄 더 길다.
- `awk 'NR==334,NR==742' main.c | grep -n static` 로 static 변수 전수 재조사.
- `grep -n "conneded_ISD\b"` / `mappingConnection` 로 지역변수 실제 사용처 재조사(구조체 필드 `isd_state.conneded_ISD`/`BLE_communicationState.mappingConnection`와 이름이 겹치므로 bare 식별자만 필터링).
- 01~06 전체 md에서 `fake_op_mode`/`fake_func_sleep`/`SYS_WAIT_FOR_INTERRUPT`/`SYS_WATCHDOG_REFRESH` 언급 여부 grep.

## 1. static/지역 변수 완전성 검증

`awk 'NR==334,NR==742' main.c | grep -n static` 결과, func_normal() 안의 static 변수는 정확히 4개뿐임을 확인:

| 변수 | 실측 위치(main.c) | 담당 문서 | 일치 여부 |
|---|---|---|---|
| `fake_0x34` | 458 | 02 | 일치 |
| `fake_0x34_done` | 459 | 02 | 일치 |
| `prev_batt_st` | 555 | 04 | 일치 |
| `s_map_low_active` | 627 | 04 | 일치 |

지시문이 예시로 든 6개 항목(`prev_batt_st`·`s_map_low_active`·`fake_0x34`·`fake_0x34_done`·`qcc_batt_timeout`·`qcc_timeout_poweroff_started`) 중 뒤 2개는 static이 아니라 함수 진입 시 매번 재초기화되는 지역 bool(main.c:353-354)이며, 이 역시 02번(세팅 지점, 471행)과 05번(소비 지점, 679-691행) 양쪽에서 정확히 다뤄졌다. **6개 항목 전부 5개 문서 어딘가에 정확히 기술되어 있음 — 누락 없음.**

01번 문서는 func_normal() 최상위 지역변수 12개(`batteryLevel`/`ledPattern`/`systemState`/`isd_state`/`usbConnectorState`/`BLE_communicationState`/`mcuErrorCode`/`powerButtonPushed`/`conneded_ISD`/`mappingConnection`/`qcc_batt_timeout`/`qcc_timeout_poweroff_started`)를 §3에서 나열했는데, 이는 main.c:336-354 선언부와 **정확히 일치**(1:1 대조 완료, 초과·누락 없음).

### 불일치 1 — `conneded_ISD`/`mappingConnection` dead 변수 미표시

- `grep -n "conneded_ISD\b" main.c` 결과: 347행(선언, `bool conneded_ISD = false;`) 외에는 전부 `isd_state.conneded_ISD`(구조체 필드, 별개 심볼)만 매치되고, bare `conneded_ISD` 단독 사용은 없음.
- `mappingConnection` bare 식별자도 동일 — 348행 선언(`bool mappingConnection = false;`) 외에는 전부 `BLE_communicationState.mappingConnection`(구조체 필드)만 매치.
- 즉 이 두 지역변수는 **선언 후 프로그램 어디서도 읽거나 다시 쓰이지 않는 완전한 dead code**다. 01번 문서 §3은 이 두 변수의 "존재"를 나열하고 "ISD 정보 출력 블록이 이 변수들을 읽거나 쓰지 않는다"고 서술했지만, 이는 국소적(그 블록 한정) 관찰일 뿐 "함수 전체에서 죽은 변수"라는 사실은 어느 문서도 명시하지 않았다.
- **왜 문제가 될 수 있는가**: 구조체 필드와 이름이 완전히 같아(`conneded_ISD` bare vs `isd_state.conneded_ISD`) 리팩토링 도중 grep/치환 작업 시 혼동 소지가 있다. 다만 동작 자체에는 영향 없음 — 이 변수들을 아예 제거해도 behavior-preserving(컴파일 결과 무관한 단순 미사용 선언 삭제). 계획.md 단계에서 "정리 대상 dead code"로 명시해두면 좋음.

## 2. 04번 문서(LED 요청 로직) 분해의 behavior-preserving 여부 재검증

지시문이 지목한 두 지점(작업 지시서상 근사 라인 598/604, 실측 라인은 04번 문서 및 원본 Read 기준 610/616)을 원본과 대조:

- **배터리 LED 재호출** (`led_request(LED_SRC_BATTERY, batt_st)`, 실측 610행, ISD 미연결 else 분기 안): 04번 문서 §4 표에 "`batt_st` ... ISD 블록이 재사용 — ISD 미연결 시 604~605에서 `led_request(LED_SRC_BATTERY, batt_st)` 재송출"로 정확히 기술되어 있고, §5 제안 함수 `tdc_led_request_isd(bool isd_conn, led_state_t batt_st)`가 `batt_st`를 파라미터로 받아 동일 분기를 그대로 재현함 — **일치**.
- **`led_set_isd_conn_state()` race 방지 위치 이동** (실측 616행): 04번 문서 §2.1에서 `LedOutput.c:87,709`의 `s_isd_conn`/`led_arbiter_tick()` 관계까지 파고들어 "1-tick IN_USE 잔상 race" 발생 메커니즘을 정확히 재구성했고, §2.2·§5에서 이 호출을 새 함수의 **마지막 statement로 캡슐화**(호출자에 노출 금지)하도록 명시 — **일치, 오히려 원본 주석보다 상세**.
- 두 지점 모두 원본 코드(main.c:591-616)와 04번 문서의 서술·제안 코드가 라인 단위로 대조해도 의미상 어긋남 없음. **불일치 없음.**

## 3. `qcc_batt_timeout` 생명주기 — 02번 vs 05번 교차검증

| 단계 | 02번 문서 서술 | 05번 문서 서술 | 원본(main.c) | 모순 여부 |
|---|---|---|---|---|
| 선언 | "353행에서 매 func_normal() 호출마다 false로 재선언되는 비-static 지역변수" | "353-354행(func_normal 진입 시, while 루프 밖) ... 지역변수(스택) — static 아님" | 353-354행 정확히 일치 | 없음 |
| 세팅 | "471행에서 한 번 true로 세팅" | "main.c:471(fake_0x34 static 블록, iterationFlag==true 안) ... qcc_batt_timeout = true" | 471행 일치 | 없음 |
| 소비 | "679~691행(같은 while(1) 루프, iteration 게이트 바깥)에서 if (qcc_batt_timeout)로 소비" | "679-686/687-690행 — 최초 진입 시 POWER_OFF 요청, 이후 burst 완료 시 systemOff=true" | 679-691행 일치 | 없음 |
| 재설정 없음 확인 | "같은 func_normal() 호출 내에서는 그 값을 되돌리는 코드가 없음" | (705행 절전 가드에서 systemOff는 되돌릴 수 있어도) qcc_batt_timeout/qcc_timeout_poweroff_started 자체를 되돌리는 코드는 없다고 명시 | grep 재확인: `qcc_batt_timeout`/`qcc_timeout_poweroff_started` 대입은 353·354(초기화)·471·684(true 세팅)뿐, false 재대입 없음 | 없음 |

**02번과 05번은 서로 모순 없이 완전히 맞아떨어진다.** 재확인 grep(`grep -n "qcc_batt_timeout\|qcc_timeout_poweroff_started" main.c`) 결과도 두 문서가 인용한 라인(353,354,471,679,681,684)과 정확히 일치.

## 4. 구역 경계 커버리지 — 겹침·누락 확인

`00_오케스트레이터-입력.md`의 원 지시 범위는 "main.c:320-723"였으나, 원본을 직접 Read한 결과 `func_normal()`의 실제 범위는 **334행(`int func_normal(void)`) ~ 742행(닫는 `}`, `return 0;`은 741행)**이다. 즉 오케스트레이터가 지시한 상한선(723행) 자체가 실제 함수 끝보다 19줄 짧다.

| 구간(실측) | 담당 | 커버리지 |
|---|---|---|
| 334-361 (시그니처+지역변수 선언+초기값) | 01(§3에서 변수 목록으로 다룸) + 02/05(qcc_batt_timeout 관련 353-354 교차 인용) | 커버됨(분산되어 있으나 누락 없음) |
| 362-435 (CFX 대기+Initialize+ISD 출력) | 01 | 커버됨 |
| 437-485 (iteration 게이트+센서읽기+fake_0x34) | 02 | 커버됨 |
| 486-491 (주석 전용, 코드 없음) | 명시적 담당자 없음 | 문제 없음(실행 코드 없는 주석) |
| 492-536 (systemControl~모드 플래그) | 03 | 커버됨 |
| 538-655 (LED 3분기) | 04 | 커버됨 |
| 657-668 (led_arbiter 주석+NRF_On_OFF+UI polling+disable_iteration) | 05 | 커버됨 |
| 669-728 (main_counter+qcc_batt_timeout 소비+크래들+systemOff 가드) | 05 | 커버됨 |
| **730-741 (fake sleep mode 체크+SYS_WATCHDOG_REFRESH+SYS_WAIT_FOR_INTERRUPT+return 0)** | **없음** | **누락** |

구역 간 **겹침(중복 주장)은 없음** — 각 문서가 인접 구역의 변수를 인용할 때는 모두 "핸드오프"·"교차 확인" 목적의 참조이지, 같은 코드를 서로 다르게 해석하는 충돌은 발견되지 않았다.

### 불일치 2 — 730~741행(함수 진짜 끝부분) 전체가 분석 밖

```c
730  if (tdc_get_fake_op_mode() == 1)  // fake sleep mode가 맞을 때
731  {
732      ci_printw("[GD] try to enter fake sleep mode. \r\n");
733      led_force_fade_off();
734      fake_func_sleep();
735  }
736
737  SYS_WATCHDOG_REFRESH();
738  SYS_WAIT_FOR_INTERRUPT;
739  }  // 끝, while
740
741  return 0;
```

- `grep`으로 01~06(00 포함) 전 문서를 재검색한 결과 `fake_op_mode`/`fake_func_sleep`/`SYS_WAIT_FOR_INTERRUPT` 문자열은 **어디에도 등장하지 않는다.**
- `fake_func_sleep()`(main.c:810)을 직접 Read해 확인한 결과, 이 함수는 `changePcmOutputMode`/`write_FPGA_reset`/PMIC·FPGA off·LED off 수행 후 `#if 1 while(1) { ... }`(main.c:835~) 형태의 CFX ULP 대기 루프에 진입한다 — `func_sleep()`(01번 문서가 분석한, watchdog reset 전까지 정상 반환하지 않는 함수)와 **동형의 "정상 반환하지 않는 탈출 경로"**로 강하게 추정된다.
- 즉 func_normal()에는 05번 문서가 다룬 `break`(701~726행, systemOff 경로) 외에 **두 번째 탈출 경로**(`tdc_get_fake_op_mode()==1`일 때 `fake_func_sleep()` 진입, 정상 반환 없이 별도 리셋/ULP 경로로 빠짐)가 존재하는데, 이를 언급한 문서가 하나도 없다.
- **행동 보존 관점에서 실질적 위험**: 계획.md에서 "iteration 밖 절전 전이 로직"을 함수로 뽑을 때 05번 문서의 제안(§4, `tdc_power_transition_check` 등)만 따르면 이 fake-sleep 분기가 새 함수 경계 밖에 방치되거나, 반대로 잘못 흡수되어 순서가 바뀔 수 있다 — 05번 제안 시그니처의 반환값(`shouldEnterSleep` 등)에 이 세 번째 상태(수면 진입 아님/일반 systemOff 진입/fake-sleep 진입)가 반영돼 있지 않다.

## 결론 요약

| 문서 | 판정 |
|---|---|
| 01_boot-sequence-analyst | 전부 일치 (지역변수 12개 목록 정확) |
| 02_sensor-read-debug-analyst | 전부 일치 (static 2개 + qcc_batt_timeout 세팅부 정확) |
| 03_system-control-orchestration-analyst | 전부 일치 (겹침·모순 없음) |
| 04_led-request-logic-diagnostician | 전부 일치 (598/604행류 두 지점 모두 정확히 다룸) |
| 05_power-transition-analyst | 불일치/누락 항목: **730-741행(fake sleep mode 탈출 경로, `fake_func_sleep()`) 전체가 담당 범위·분석 밖** — 원 지시 범위(~723행) 자체가 실제 함수 끝(742행)보다 짧아서 발생한 구조적 누락 |
| (전체 5개 문서 공통) | 불일치/누락 항목: **`conneded_ISD`/`mappingConnection`(347-348행) dead 지역변수가 "죽은 코드"로 명시되지 않음** — 동작에는 영향 없으나 리팩토링 시 혼동 방지를 위해 계획.md에서 정리 대상으로 명시 권장 |

## 핸드오프 — 계획.md 작성 시 참고

- **필수**: 730-741행(`tdc_get_fake_op_mode()`/`fake_func_sleep()` 분기 + 마무리 watchdog refresh/wait-for-interrupt)을 05번 담당 구역에 명시적으로 편입시켜 재검토 후 계획에 반영할 것. 특히 `fake_func_sleep()`이 `func_sleep()`처럼 정상 반환하지 않는지 별도 확인(1줄 Read로 충분, 810~ 이하 전체를 한 번 더 확인 권장) 후, 05번이 제안한 절전 전이 함수의 반환 계약에 "fake-sleep 진입" 상태를 3번째 케이스로 추가할 것.
- 부차: `conneded_ISD`/`mappingConnection`(347-348행) dead 변수는 계획.md의 "정리 대상(동작 불변, 단순 삭제)" 목록에 `p_isd_info`(01번이 이미 지적)와 함께 추가.
