---
name: 절전·전원전이 분석가 — func_normal() 645~723행
purpose: func_normal() 5구역 분석 중 NRF/UI polling·QCC 배터리 타임아웃 POWER_OFF·크래들 뚜껑·절전 진입 가드 구역의 상태 수명·결합도 분석
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: stable
tags: [main, func_normal, refactor, power-transition, led-arbiter]
---

# 05. 절전·전원전이 분석가 — `main.c:645~723`

**TL;DR**: 645~657행(NRF/UI polling)은 `iterationFlag==true` 안, 659~728행(QCC 타임아웃→POWER_OFF·크래들·systemOff 가드)은 while(1) 매 회전마다 항상 도는 "iteration 밖" 코드다. `qcc_batt_timeout`/`qcc_timeout_poweroff_started`는 지역 bool로, 02구역 fake_0x34 블록에서 세팅되고 본 구역에서 소비되어 `systemState.systemOff=true`까지 이어진다. 절전 가드가 조회하는 `LED_SRC_BLE_IND`는 인접 텍스트 블록이 아니라 `bleCommunication()`(475~524구역, `ble_communication.c`)이 전역 LED arbiter(`LedOutput.c`)에 써넣은 상태라 — 함수 분해 시 텍스트 순서가 아니라 arbiter 모듈 의존으로 봐야 하며, `break`는 반환값(`bool shouldEnterSleep`)으로 치환 가능하다.

## 1. "iteration 안" vs "iteration 밖" 경계

```
437  while (1)
439      if (iterationFlag == true)
...
660         NRF_On_OFF(...)
662-665     tdc_ui_command_set_mapping_connected(...); tdc_ui_command_poll();
668         disable_iteration();          // ★ 경계 — iterationFlag 를 false로 되돌림
669      }  // 끝, iteration
671      main_counter++;
672      update_CM3Status_toCFX(main_counter);
679      if (qcc_batt_timeout) { ... }               // QCC 타임아웃 POWER_OFF 시퀀스
694      if (systemState.cradleLidClosed && ...) { ... }  // 크래들 뚜껑
701      if (systemState.systemOff == true) { ... break; }  // 절전 가드
739  }  // 끝, while
```

- **645~668행(NRF_On_OFF 호출·UI 커맨드 polling·`disable_iteration()`)은 `if (iterationFlag == true)` 블록 안**이다 — 즉 타이머(추정: Timer 3 ISR 또는 유사 주기 트리거)가 `iterationFlag`를 세팅해줄 때만 한 번 실행되고, 실행 후 668행 `disable_iteration()`이 즉시 플래그를 내려 다음 트리거까지 재실행을 막는다.
- **671~728행(main_counter 증가, QCC 타임아웃 체크, 크래들 뚜껑, systemOff 가드)은 `if` 블록 밖**이라 `while(1)`이 도는 한 매 회전마다 무조건 실행된다. 657행 주석("led_arbiter_tick()은 Timer 3 ISR에서 직접 구동... main loop의 I2C/EEPROM 폴링 블록으로 인한 fade/PWM jitter 회피")이 이 프로젝트의 설계 철학을 보여준다 — **시간 민감한 것(LED fade 틱)은 ISR로, 무거운 I2C/폴링은 iterationFlag 게이트로 스로틀링, 그리고 상태 전이 판정(타임아웃 카운트다운·systemOff 진입)은 게이트 없이 매 스핀마다 도는 빠른 폴링 루프**로 나눈 구조다. `disable_iteration()`이 668행에서 바로 호출되는 이유도 "무거운 작업은 1회만 하고, 이후 스핀들은 671행 이후 가벼운 조건 체크만 반복"하기 위함으로 읽힌다.
- 679행 `qcc_batt_timeout` 체크가 iteration 밖에 있는 이유는 주석(674~678)에 명시: QCC가 0x34 미수신 상태에선 `systemControl()`이 `df_Default` 게이트에 막혀 자체 파워오프 시퀀스에 도달하지 못하므로, 이 게이트 우회 로직 자체가 `systemControl()` 호출(iteration 안)과 독립적으로, LED burst 완료 여부(`tdc_led_is_burst_pending()`)를 매 스핀마다 폴링해야 하기 때문 — burst 완료 감지는 iterationFlag 주기보다 빠르게 확인할 필요가 있다.

## 2. `qcc_batt_timeout` / `qcc_timeout_poweroff_started` 전체 생명주기

| 단계 | 위치 | 무슨 일 | 변수 종류 |
|---|---|---|---|
| 선언·초기화 | `main.c:353-354` (func_normal 진입 시, while 루프 밖) | `bool qcc_batt_timeout = false;` `bool qcc_timeout_poweroff_started = false;` | 지역변수(스택) — **static 아님** |
| 리셋 근거 | `main.c:350-352` 주석 | "절전 후 func_normal 재진입 시 false로 리셋되어 무한 재절전 방지" — `func_normal()`이 `func_sleep()`에서 깨어나 다시 호출될 때마다 이 두 변수는 자동으로 false로 재초기화됨 (반면 `fake_0x34_done`은 `static`이라 유지됨 → 타임아웃 재감지 자체를 차단) | — |
| 세팅 (02구역) | `main.c:471` (fake_0x34 static 블록, `iterationFlag==true` 안) | `ci_timer_get_tick() - fake_0x34 > 3000`이면 `qcc_batt_timeout = true;` (QCC 0x34 배터리 정보 3초간 미수신) | 세팅 1회만 (fake_0x34_done static 가드로 재진입 차단) |
| 소비 1단계 (05구역, 본 담당) | `main.c:679-686` (iteration 밖, 매 스핀) | `if (qcc_batt_timeout)` 최초 진입 시(`!qcc_timeout_poweroff_started`): `led_request(LED_SRC_POWER, LED_ST_POWER_OFF)` 요청 + `qcc_timeout_poweroff_started = true` | 1회만 진입 (자기 자신을 가드) |
| 소비 2단계 (05구역) | `main.c:687-690` | 이후 스핀마다: `else if (!tdc_led_is_burst_pending())` — POWER_OFF burst 애니메이션 완료를 매 스핀 폴링, 완료되면 `systemState.systemOff = true;` | burst 완료까지 매 스핀 재평가 |
| 하류 소비 (05구역) | `main.c:701-728` 절전 가드 | `systemState.systemOff == true`면 BLE map/pair/OTA 활성 여부 체크 → 활성이면 **`systemState.systemOff = false`로 되돌림**(725행 도달 못함, `qcc_batt_timeout`/`qcc_timeout_poweroff_started`는 그대로 true 유지) → 다음 스핀에 679행 재진입, `qcc_timeout_poweroff_started`가 이미 true이므로 바로 687행 분기로 가서 다시 `systemOff=true` 재시도(재시도 루프). 비활성이면 `led_force_fade_off(); break;` | — |

**핵심**: 이 두 변수는 "QCC 배터리 정보 미수신"이라는 단일 이벤트를 **엣지 트리거(671행 세팅)→레벨 유지(679행 이후 매 스핀 재확인)→최종 `systemState.systemOff`로 합류**시키는 3단 파이프라인이다. `systemState.systemOff`에 합류한 이후로는 절전 가드(701행)가 QCC 타임아웃 기원인지 다른 기원(예: `systemControl()` 내부에서 세팅된 정상 파워오프)인지 구분하지 않고 동일하게 처리한다 — 즉 **가드 로직(701~728) 자체는 `qcc_batt_timeout`을 몰라도 된다**. 결합은 오직 "679~691행이 `systemState.systemOff`를 true로 만드는 하나의 소스"라는 지점에서만 존재한다.

## 3. 절전 가드의 `led_get_request(LED_SRC_BLE_IND)` 결합 — 정정 및 실제 구조

작업 지시문은 "이전 구역(LED 요청 블록)이 이미 설정한 상태를 다시 읽는 구조"로 서술했으나, grep 검증 결과 **LED 요청 블록(541~655행)은 `LED_SRC_BLE_IND`를 전혀 건드리지 않는다** (해당 블록은 `LED_SRC_BATTERY`/`LED_SRC_ISD`/`LED_SRC_MAPPING`만 세팅). `LED_SRC_BLE_IND`는 다음 두 지점에서만 세팅된다:

- `LedOutput.c:346-370` `tdc_led_set_ind_state()` — `BleCommunication/ble_communication.c`에서 호출 (03구역 담당, `main.c:515` `bleCommunication(isd_state)` 호출 경로 안에서 트리거될 수 있음)
- `LedOutput.c:694-697` — arbiter 내부 자체 로직(페어링 관련 유지 조건)

즉 707행 `led_get_request(LED_SRC_BLE_IND)`가 읽는 값은:
1. **인접 텍스트 블록이 아니라 전역 LED arbiter 모듈(`LedOutput.c`의 static `s_req[]` 배열)의 pull-API**이다. `led_request()`/`led_get_request()`는 함수 로컬 변수가 아니라 모듈 전역 상태를 통한 **cross-cutting 결합**이며, 함수 안에서 텍스트로 몇 줄 위인지는 실행 의미상 무관하다.
2. 값의 최신성은 "이번 iteration에서 475~524구역(특히 `bleCommunication()`)이 실행되었는가"에 달려 있다. `if (iterationFlag==true)` 게이트가 걸려 있으므로, 절전 가드(iteration 밖, 매 스핀 실행)가 도는 여러 스핀 중 대부분은 **직전 iteration에서 세팅된 값을 그대로 재사용**하게 된다 — 이는 버그가 아니라 의도된 설계(주석 705~706 "BLE 활성 검사는 Arbiter src 요청을 직접 본다 — tdc_led_get_ind_state()는 BLE가 set한 후 NONE으로 reset 안 보내면 잔존하기 때문")이지만, **절전 가드를 별도 함수로 뽑을 때 "이 함수가 매번 최신 iteration 결과를 본다는 보장이 없다"는 사실은 계약(contract)에 명시해야 한다**.

**분해 제약**: 이 결합은 순서 제약이 아니라 **모듈 의존성 제약**이다 — 절전 가드 함수는 `LedOutput.c`의 `led_get_request()`/`tdc_led_is_burst_pending()` API에 의존하므로, 분해 후에도 `LedOutput.h`를 include하고 이 두 pull-API를 그대로 호출하면 된다. 파라미터로 넘길 필요는 없다(전역 arbiter이므로). 단, **호출 순서만은 유지해야 한다** — 절전 가드 함수 호출이 `bleCommunication()` 호출(같은 iteration 안, 03구역) 이후, 그리고 679~691행(QCC 타임아웃→systemOff 세팅) 이후에 와야 한다(그래야 이번 스핀에서 세팅된 `systemState.systemOff`를 가드가 평가 가능).

## 4. 별도 함수로 뽑을 때 제안 시그니처

```c
/* 예시 시그니처 — behavior-preserving 목표 */
static bool tdc_power_transition_check(ST__SYSTEM_STATE *systemState,          /* in-out: systemOff 필드를 되돌릴 수 있음(714행) */
                                        const ST__BLE_COMMUNICATION_STATE *bleState,
                                        const volatile ST__ISD_STATUS *isdState,
                                        bool qcc_batt_timeout,
                                        bool *qcc_timeout_poweroff_started);   /* in-out: 649행에서 갱신 */
```

- **입력**: `systemState`(cradleLidClosed·systemOff 필드 읽기+쓰기 필요 → 포인터 또는 반환 구조체), `BLE_communicationState.mappingConnection`(map_active 판정용, 707행), `isd_state.conneded_ISD`(크래들 뚜껑 차단 조건 694행), `qcc_batt_timeout`(값만 읽음), `qcc_timeout_poweroff_started`(읽고 갱신).
- **부작용**:
  - `led_request(LED_SRC_POWER, LED_ST_POWER_OFF)` (683행) — 그대로 함수 내부에서 호출 유지 가능(전역 arbiter 호출은 순수하지 않지만 부작용 자체가 "요청"이라는 명확한 단일 책임이라 함수 내부에 남겨도 무방).
  - `led_force_fade_off()` (696행 크래들, 725행 절전) — 마찬가지로 내부 호출 유지.
  - `func_cradle_lid_closed_loop()` (697행) — **주의: 이 함수는 내부 `while` 루프 + `SYS_WATCHDOG_RESET()`으로 재부팅하며 "도달 불가"(698행 주석)로 반환하지 않는다.** 별도 함수로 뽑아도 이 호출 지점에서 진짜로 리턴되지 않으므로, `break`로 다루는 상위 `while(1)`과는 별개로 **크래들 뚜껑 분기는 절전 가드 함수와 분리하는 편이 안전**하다(호출자 쪽 제어 흐름을 오도하지 않기 위해). 최소한 이 함수 안에 넣더라도 "return 하지 않을 수 있다"를 함수 doc에 명시해야 함.
  - `ci_printw`/`ci_printi` 로그 — 부작용이지만 순서·타이밍에 영향 없어 그대로 유지 무방.
- **`break` → 반환값 치환**: 726행 `break`(무한루프 탈출, ULP 진입)를 함수로 뽑으면 `bool shouldEnterSleep`(또는 3-state: `enum { KEEP_RUNNING, ENTER_CRADLE_SLEEP, ENTER_ULP_SLEEP }`)을 반환하고, 호출부(`while(1)` 내부)에서:
  ```c
  PowerTransitionResult r = tdc_power_transition_check(&systemState, &BLE_communicationState, &isd_state,
                                                        qcc_batt_timeout, &qcc_timeout_poweroff_started);
  if (r == ENTER_ULP_SLEEP) { break; }
  /* ENTER_CRADLE_SLEEP 은 func_cradle_lid_closed_loop() 내부에서 이미 재부팅하므로 사실상 도달 안 함 */
  ```
  로 바꾸면 원래 `break` 시맨틱을 100% 보존하면서 순수 판정 로직을 분리할 수 있다.
- **추천 분해 경계**: (a) `qcc_batt_timeout` → `systemOff` 변환(679~691행)과 (b) 크래들 뚜껑(694~699행)과 (c) systemOff 가드+`break`(701~728행)는 **서로 다른 단일 책임**이므로 하나의 거대 함수보다 2~3개 작은 함수로 쪼개는 편이 낫다. 단, (a)와 (c)는 `systemState.systemOff` 필드를 통해서만 결합되어 있어 순서만 지키면 독립적으로 추출 가능. NRF_On_OFF·UI polling(645~665행, iteration 안)은 이 절전 전이 로직과 상태 공유가 없어 완전히 별개 함수(예: `tdc_power_periodic_io_tick()`)로 분리 가능.

## 요약 표 — 상태 변수 결합 매트릭스

| 변수/API | 선언 위치 | 세팅 구역 | 소비 구역(본 담당) | 함수 경계 시 처리 |
|---|---|---|---|---|
| `qcc_batt_timeout` | 353행, 지역 bool | 02구역(471행) | 05구역(679행) | 파라미터로 값 전달(읽기 전용) |
| `qcc_timeout_poweroff_started` | 354행, 지역 bool | 05구역 자기 자신(684행) | 05구역(681,687행) | 포인터 파라미터(읽기+쓰기) 또는 05구역 내부 static |
| `systemState.systemOff` | 338행 구조체 필드 | 03구역(systemControl 내부) + 05구역(689,715행) | 05구역(701행) | in-out 포인터/구조체 반환 |
| `LED_SRC_BLE_IND` (arbiter) | `LedOutput.c` 전역 static | 03구역 경유 `ble_communication.c` | 05구역(707행) | 함수 분해 무관, `LedOutput.h` API 그대로 호출(전역 모듈 결합, 순서만 유지) |
| `func_cradle_lid_closed_loop()` | — | — | 05구역(697행) | 반환 안 할 수 있음 — 별도 분기로 격리 권장 |
