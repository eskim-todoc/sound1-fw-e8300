---
name: G9-FSM폴링구조확인
purpose: tdc_touch.c의 tdc_touch_process()와 main.c의 3개 폴링 경로(노말 FSM·func_sleep·fake_func_sleep) 코드 구조를 직접 대조해, 매 tick 무조건 I2C read 여부·RDY 게이트 삽입 정확 위치·비-터치 로직(BLE 등)과의 tick 공유 여부를 확정
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, event-mode, interrupt, polling-loop, fsm, rdy, tdc_touch, main-c, 그라운드-9, 2026-07-05]
---
```

**TL;DR**: 3개 폴링 경로(노말 FSM·func_sleep·fake_func_sleep) 전부 RDY 아닌 경과시간으로 게이트되며, 통과 후 read_status는 무조건 호출된다. 삽입 위치는 tdc_touch.c:301·main.c:1232 2곳. BLE는 100ms tick과 별개인 더 빠른 외곽 tick(iterationFlag)에 얹혀 제거 불가하나, 터치의 100ms 게이트는 이미 격리돼 외곽 tick 안 건드리고 I2C만 게이팅 가능.

---

## 0. 조사 방법·범위

- 코드: `tdc_touch.c`(434행 전체)·`tdc_touch.h`·`tdc_touch_time.h`·`tdc_touch_config.h`(전부 전체)·`tdc_touch_iqs323.c`(500행 전체), `main.c` 90~130행·278~520행·700~1255행(func_normal 전체 + func_sleep·fake_func_sleep 전체)을 본 노드가 직접 Read. `Cortex-M3-src/systemControl/driver_timmer.c`·`Gen1_5/common/ci_timer.c`(전체) 발췌 확인.
- 본 노드는 15번 문서(`15_보충입력_결합시나리오.md`)가 지정한 역할(폴링 루프 구조 확인)에 한정한다. Events/Streaming 트래픽 정량(02번 G2)·RDY 엣지 인터럽트의 물리적 실현 가능성(07번 C4)·자기애그레서 반증(10번 V2)은 선행 문서를 그대로 인용하되 재검증하지 않는다.
- **모든 인용 라인번호는 위 파일을 직접 Read한 결과이며, `Sys_DIO_IntConfig` 관련 grep은 본 노드가 독립적으로 재실행했다**(§3 참조).

---

## 1. 결론 요약

| 항목 | 결론 |
|---|---|
| 확인_1: 매 tick 무조건 I2C read인가 | **부분적으로 그렇다.** 3개 경로 전부 "경과시간 게이트 통과 → `read_status` 무조건 호출"이라는 골격이 동일하다. |
| 확인_2: 이미 조건부 스킵 지점이 있는가 | 있다 — 단 그 조건은 RDY 이벤트가 아니라 **순수 경과시간**(소프트웨어 ms 차분, 또는 HW 타이머 자체)뿐이다. `read_debug()`는 `read_ok` 성공 여부로만 추가 게이팅되며 이 역시 RDY와 무관하다. |
| 위치_1: RDY 게이트 삽입 정확 위치 | `tdc_touch.c:301`(노말 FSM, 300행-302행 사이) / `main.c:1232`(실사용 절전 `func_sleep()`, 1231행-1233행 사이) — 2곳이 핵심. `fake_func_sleep()`(`main.c:907`-`908` 사이)은 3번째 후보지이나 디버그 전용이라 낮은 우선순위. |
| 확인_3: 비-터치 로직이 100ms tick에 얹혀있어 tick 제거가 불가능한가 | **얹혀있는 tick과 터치의 100ms tick은 서로 다른 tick이다.** BLE·ISD·LED는 100ms가 아닌 별개의(CFX 구동) 외곽 tick을 공유해 제거 불가하지만, 터치의 100ms 게이트는 그 외곽 tick과 이미 무관하게 격리돼 있어 **외곽 tick을 그대로 둔 채 I2C 게이팅만 추가 가능**하다. |

---

## 2. 폴링 경로 3곳 전수 확인

### 2.1 경로_1 — 노말(FSM) 경로: `tdc_touch_process()`

파일: `tdc_touch.c`, 함수: `tdc_touch_process()`(273~421행)

init 상태머신(276~292행) 통과 후 **폴링 게이트**(294~300행):

```c
294:    /* --- 폴링 게이팅 (ms 차분) --- */
295:    int now = ci_timer_get_tick();
296:    if (TDC_TOUCH_POLL_INTERVAL_MS > (now - s_poll_tick_old))
297:    {
298:        return false;
299:    }
300:    s_poll_tick_old = now;
301:
302:    /* --- read -> FSM 입력 정규화 (실패 시 status 가 전부 false 보장) --- */
303:    tdc_touch_iqs323_status_t st;
304:    tdc_touch_in_t            in;
305:    in.read_ok    = tdc_touch_iqs323_read_status(&st);   // 무조건 호출
```

`TDC_TOUCH_POLL_INTERVAL_MS=100`(`tdc_touch_time.h:18`, 직접 확인). 게이트 조건은 **경과 ms 차분**뿐이며 RDY 핀·인터럽트 플래그를 전혀 참조하지 않는다. 게이트 통과 직후 305행에서 `read_status`가 무조건 호출된다.

`read_debug`는 311~338행, `TDC_TOUCH_DEBUG_PRINT_ENABLE`(빌드타임, 기본 1, `tdc_touch_config.h:22`) + `in.read_ok`(런타임, 313행)로만 게이팅 — **RDY 무관, read_status 성공 여부에만 의존**한다.

### 2.2 경로_2 — 절전(실사용) 경로: `func_sleep()`

파일: `main.c`, 함수: `func_sleep()`(1164~1252행), ULP 루프 1228~1249행

```c
1224:    ci_timer_init_prescaled(TDC_TOUCH_ULP_TIMER_PRESCALE, TDC_TOUCH_ULP_TIMER_TIMEOUT_VALUE); /* 100.0ms 정확 (tdc_touch_time.h 공식 확인) */
...
1228:    while (1)  // ULP loop
1229:    {
1230:        SYS_WAIT_FOR_INTERRUPT; /* ULP_WAKE_MS 동안 idle */
1231:        SYS_WATCHDOG_REFRESH();
1232:
1233:        ok    = tdc_touch_iqs323_read_status(&st);   // 무조건 호출
```

게이트는 **HW 타이머(TIMER3) 자체**다 — `ci_timer_init_prescaled()`(1224행)로 TIMER3을 정확히 100.0ms 주기로 재설정하고 `SYS_WAIT_FOR_INTERRUPT`(1230행)로 그 인터럽트를 기다린다. **깨어나는 것 자체가 곧 100ms tick**이라 `tdc_touch_process()`처럼 별도 ms차분 소프트웨어 게이트가 없다(불필요). 깨어난 직후 1233행에서 `read_status`가 무조건 호출된다. `read_debug`는 `tdc_touch_sleep_log_debug()`(1037~1050행) 경유 1236행 호출 — 역시 `TDC_TOUCH_DEBUG_PRINT_ENABLE`+`ok`로만 게이팅.

### 2.3 경로_3 — 절전(테스트용) 경로: `fake_func_sleep()`

파일: `main.c`, 함수: `fake_func_sleep()`(810~1031행), ULP 루프 889~1028행

게이트는 T3 tick 변화 감지(894행, `if (last_t3_tick < tdc_timer_get_t3_tick())`) — `SYS_WAIT_FOR_INTERRUPT`(891행)로 깬 뒤 T3 tick이 실제 증가했는지 재확인하는 2중 구조다. 게이트 통과 직후 908~909행에서 `read_status`가 무조건 호출된다.

**본 노드 독립 확인**: 이 경로는 `s_tdc_fake_op_mode`(기본값 0, `main.c:87`)가 1일 때만 진입하며(`main.c:730`), 이 플래그는 BLE 원격 디버그 커맨드(`tdc_remote_general_debug.c:169`)로만 set된다 — 정상 운용 중에는 진입하지 않는다. 재부팅 로직도 `SYS_WATCHDOG_RESET()`이 주석처리(1011행)돼 있어 실제로 리부팅하지 않는 no-op임을 직접 확인했다(선행 G2 문서 §1.2의 "실질 비활성 경로" 판단과 일치).

---

## 3. RDY 게이트 삽입 정확 위치

전제: 코드베이스 전체에 RDY(DIO16)를 소스로 하는 `Sys_DIO_IntConfig` 호출이나 그로부터 set되는 플래그가 **0건**임을 본 노드가 독립적으로 grep 재확인했다(`Sys_DIO_IntConfig` 호출은 `ci_dio.c:121,124`의 DIO34/DIO28 2건뿐이며 그마저 `#if 0`로 비활성 — 선행 C4·V2 문서와 일치). 즉 아래 위치는 "기존 플래그를 읽는 지점"이 아니라 **신설될 RDY 이벤트 플래그(구현 방식은 C4·후보_6 소관)를 검사하는 신규 조건문을 끼워 넣을 자리**다.

| 위치 | 파일:함수:라인 | 삽입 방식 |
|---|---|---|
| 위치_1 | `tdc_touch.c` : `tdc_touch_process()` : **300행과 302행 사이(301행, 현재 공백)** | 기존 경과시간 게이트(296~300행) 통과 직후, `read_status` 호출(305행) 직전. `if (!<RDY 이벤트 있음>) { return false; }` 형태로 기존 298행과 동일한 조기반환 패턴 재사용 가능 |
| 위치_2 | `main.c` : `func_sleep()` : **1231행과 1233행 사이(1232행, 현재 공백)** | `SYS_WATCHDOG_REFRESH()`(1231행) 직후, `read_status` 호출(1233행) 직전. `while(1)` 루프이므로 `if (!<RDY 이벤트 있음>) { continue; }` 형태 가능 |
| 위치_3(참고, 낮은 우선순위) | `main.c` : `fake_func_sleep()` : **907행과 908행 사이** | `bleCommunication()`(899행) 호출 **이후**, `read_status` 호출(908~909행) **이전** — 디버그 전용 경로라 실제 적용 필요성 낮음(§4.4) |

세 위치 모두 기존 조건문 구조를 건드리지 않고 그 안에 새 `if`문 한 줄을 끼워 넣는 형태로 삽입 가능하다 — 기존 게이트(경과시간)와 신규 게이트(RDY)가 AND로 결합되는 2단 게이트가 된다.

---

## 4. 100ms tick과 비-터치 로직의 공유 여부 — 서로 다른 tick을 구분해야 함

### 4.1 외곽 tick — iterationFlag, CFX 구동, 100ms 아님, 공유되어 제거 불가

`main.c`의 `func_normal()` 안 437~739행 `while(1)` 루프는 `iterationFlag`(439행, `if ((iterationFlag == true))`)가 true일 때만 몸체를 실행한다. 이 플래그는 `Cortex-M3-src/systemControl/driver_timmer.c:8~22`의 `CFX_0_IRQHandler()`/`FIFO_5_IRQHandler()`가 매번 호출하는 `enable_iteration()`으로 set된다. 이 파일 자체 주석(직접 확인, 5~6행):

> "Ezairo7150의 CM3에는 타이머가 들어있지 않다. CFX에서 일정주기의 인터럽트를 수신하여 타이머처럼 동작시킨다."

즉 이 tick은 CM3 자체 HW 타이머가 아니라 **CFX가 보내는 주기적 인터럽트를 시스템 tick 대용으로 쓰는 구조**다. `Gen1_5/common/ci_timer.c:130~133`의 `ci_timer_increase_tick()`(이 ISR이 호출)이 `_ci_timer_elapsed_1msec_counter`도 함께 증가시키는데, 이 변수명 자체가 "1msec"을 명시한다 — **코드의 명명 관례상 이 ISR은 약 1ms 주기로 가정된다**(정확한 실측치는 [미확인]). 어느 쪽이든 100ms보다 뚜렷이 빠르다.

이 외곽 tick 몸체(440~670행 부근)에는 `tdc_touch_process()`(452행) 외에도 다음이 **매 iterationFlag 발생마다 무조건** 실행된다(전부 직접 Read 확인): `systemControl()`(492행)·`update_mapNum()`(504행)·`isd_interface()`(506행)·`bleCommunication()`(515행)·`stimulation_IndicatorOut()`(517행). 이들 중 어느 것도 100ms 게이트가 없다 — **이 외곽 tick은 진짜로 비-터치 로직과 공유되며, 터치를 위해 이 tick의 주기를 늦추거나 제거하는 것은 BLE/ISD/자극 제어 자체를 정지시키는 것과 같아 불가능**하다.

### 4.2 내부 tick — tdc_touch.c 사설 100ms 게이트, 이미 완전히 격리됨

§2.1의 폴링 게이트(`tdc_touch.c:296~300`, `s_poll_tick_old`)는 파일 scope `static` 변수이며, 저장소 전체에서 `tdc_touch.c` 밖 어떤 코드도 이를 참조하지 않는다(직접 확인). `tdc_touch_process()`는 외곽 tick이 발생할 때마다(즉 훨씬 빠른 주기로) **호출은 되지만**, 내부 게이트가 막아 100ms 미만 간격에서는 즉시 `return false`, I2C 접근 0회로 끝난다.

이 내부 게이트가 읽는 `ci_timer_get_tick()`(295행)은 **§4.1과 동일한 CFX-ISR 카운터**(`g_ci_timer_main_tick`)를 시간 기준으로 쓴다 — 즉 외곽 tick과 내부 게이트는 **같은 하드웨어 클럭 소스를 공유하지만, "언제 I2C를 부를지"를 결정하는 게이팅 로직 자체는 완전히 분리**돼 있다(외곽은 `iterationFlag`, 내부는 `s_poll_tick_old`). 이 클럭 소스 공유 사실은 게이트 삽입 위치 결론을 바꾸지 않는다.

**따라서 §3 위치_1은 외곽 tick(4.1)을 전혀 건드리지 않고 순수하게 `tdc_touch_process()` 내부에만 삽입 가능하다** — BLE·ISD·LED는 외곽 tick마다 그대로 실행되고, 터치 I2C 접근만 "경과시간 100ms AND RDY 이벤트"의 2단 게이트로 추가 제한된다. "tick 자체는 유지, 그 안의 I2C 접근 부분만 게이팅"이 이미 현재 구조와 정확히 들어맞는다.

> [!NOTE]
> 참고로 `tdc_touch.c`는 시간 기준 2종을 별도로 쓴다 — 100ms 폴링 게이트는 `ci_timer_get_tick()`(CFX 구동), 부팅 auto-ATI 타임아웃(`s_mclr_done_tick`, 153·187행)은 `tdc_timer_get_t3_tick()`(TIMER3 구동, §4.3 참조)을 쓴다. 서로 다른 카운터이므로 혼동 주의.

### 4.3 func_sleep()의 ULP tick — 루프 본문은 터치 전용이나, ISR 자체는 LED와 결합

`func_sleep()`의 ULP 루프(1228~1249행) **본문**을 직접 재확인한 결과, `read_status`(1233행)·디버그 로그(1236행)·ATI 에러 복구(1237행)·notouch 게이트/재부팅(1239~1246행)·상태 로그(1248행) — BLE·LED·ISD 호출이 0건이다(`bleCommunication`/`isd_interface` 문자열 자체가 이 루프 안에 없음).

**다만 본 노드가 추가로 확인한 사실**: 이 100ms 웨이크업을 발생시키는 하드웨어(TIMER3)의 **인터럽트 핸들러 자체**(`TIMER_3_IRQHandler`, `Gen1_5/common/ci_timer.c:19~29`)는 다음과 같다:

```c
19:  void TIMER_3_IRQHandler(void)
20:  {
21:      /* normal 모드: LED · 터치 공유 카운터 + LED arbiter 전담. ... */
24:      g_tdc_timer_t3_tick++;
25:      led_arbiter_tick();
```

코드 주석이 이 카운터를 **"LED·터치 공유 카운터"**라고 직접 명시한다. `func_sleep()` 진입 시 `ci_timer_init_prescaled(TDC_TOUCH_ULP_TIMER_PRESCALE, ...)`(1224행)로 이 TIMER3을 100ms로 재설정하므로, 절전 중에도 이 ISR은 100ms마다 그대로 실행되며 `led_arbiter_tick()`도 매번 함께 호출된다 — **"루프 본문 로직"과 "ISR 자체"를 구분해야 한다.** 본문(main.c 1228~1249행, C레벨)은 터치 전용이지만, 그 본문을 깨우는 타이머 인터럽트 자체는 LED arbiter와 여전히 결합돼 있다.

따라서 "func_sleep()의 tick은 100% 터치 전용이라 자유롭게 재설계 가능"이라는 단순화는 **정정한다**: I2C 접근 부분(본문)만 게이팅하는 것은 여전히 아무 문제 없이 가능하지만(§3 위치_2), 만약 tick의 **주기 자체**(TIMER3 prescale/timeout 값)를 바꾸는 재설계까지 고려한다면 `led_arbiter_tick()` 호출 빈도도 함께 바뀌는 부수효과가 있다는 것을 인지해야 한다. 절전 진입 직전 `turnOffLED()`(1202행)가 호출돼 시각적 영향은 제한적일 가능성이 높으나, `led_arbiter_tick()` 내부 동작(페이드 큐 처리 등)에 실질적 영향이 있는지는 **[미확인, 본 노드 범위 밖]**이다.

### 4.4 fake_func_sleep()만의 예외 — BLE와 동일 게이트를 공유하나 순차적이라 분리 가능

`fake_func_sleep()`의 ULP 루프(889~1028행)만 유일하게, 동일 t3-tick 게이트(894행) 안에서 `bleCommunication(dummy_isd)`(899행)와 `read_status`(909행)가 함께 실행된다. 다만 두 호출은 순차적으로 나열된 별개 문장이며(899행이 908~909행보다 먼저 끝남), BLE 호출을 건드리지 않고 그 뒤 터치 read 앞에만 국소 게이트(§3 위치_3)를 추가하는 것은 여전히 가능하다. §2.3에서 확인했듯 이 경로는 디버그 커맨드로만 진입하는 비활성 기본 경로이므로 실질 비중은 낮다.

---

## 5. 참고 — 다운스트림(후보_6 등)이 주의할 구조적 분기점 (본 노드 범위 밖, 관찰만 기록)

§3 위치_1에 게이트를 넣을 때, "RDY 이벤트 없음"으로 판정된 사이클에 대해 코드 구조상 두 가지 서로 다른 구현이 가능함을 확인했다 — 어느 쪽을 고르느냐는 본 노드 범위를 넘는 설계 문제이므로 관찰만 남긴다.

- **분기_1**: `tdc_touch_process()` 전체를 조기반환(기존 298행과 동일한 `return false` 패턴 재사용) — 이 경우 346~348행의 `tdc_touch_logic_step()` 호출 자체가 스킵되어, FSM은 그 사이클의 `now_ms`(306행)를 전혀 받지 못한다.
- **분기_2**: I2C read(303~338행)만 건너뛰고 `tdc_touch_logic_step()`(348행)은 여전히 호출하되 `in.pressed`/`in.ati_error`/`in.ati_active`를 직전 값으로 유지(캐시)해서 넘기는 방식 — FSM은 계속 매 100ms마다 스텝하지만 입력값이 갱신되지 않는 사이클이 생긴다.

15번 문서 질문_1(롱터치 레벨 상태 재확인 가능성)이 정확히 이 두 분기 중 어느 쪽이 채택되느냐에 따라 결론이 달라질 수 있는 지점으로 보인다 — 판단은 후보_6·검증_7 소관.

---

## 6. 리스크·미확인 목록

| 항목 | 상태 |
|---|---|
| 외곽 iterationFlag tick(CFX 구동)의 정확한 주기 | **[미확인]** — `_ci_timer_elapsed_1msec_counter`라는 변수명이 ~1ms를 시사하나 실측 확인은 아님 |
| fake_func_sleep()의 정확한 호출 주기 | **[미확인]** — 선행 G2 문서와 동일 게이트, T3 tick 1ms 해상도만 확인 |
| RDY 이벤트 플래그의 구체적 구현 방식(ISR·비트필드 등) | **[본 노드 범위 밖]** — C4(07번 문서)가 `ci_dio.c` 2슬롯 재사용안을 이미 제시, 본 노드는 "어디서 검사할까"만 확정 |
| 분기_1 vs 분기_2가 INV-CTRL-1~3 및 롱터치 판정에 미치는 영향 | **[본 노드 범위 밖]** — §5는 구조적 관찰만, 영향 판단은 후보_6·검증_7 소관 |
| func_sleep() tick 주기 변경 시 `led_arbiter_tick()` 빈도 변화의 실질 영향 | **[미확인, 신규 발견]** — §4.3, 절전 중 LED가 꺼져있어(`turnOffLED()`) 시각적 영향은 제한적일 가능성이 높으나 확정 아님 |
| main.c:668(disable_iteration 호출부)의 정확한 트리거 조건 | **[미확인]** — grep으로 존재만 확인, 주변 문맥 미조사(본 결론에 영향 없다고 판단해 범위 밖으로 뺌) |

---

## 핵심 결론 (3~5줄)

1. `tdc_touch.c`/`main.c`의 3개 폴링 경로(노말 FSM·`func_sleep`·`fake_func_sleep`) 전부 "경과시간 게이트 통과 → `read_status` 무조건 호출 → `read_ok` 시에만 `read_debug`"라는 동일 골격이며, 현재 어떤 경로도 RDY 플래그로 게이팅하지 않는다 — 새 RDY 게이트는 항상 "추가"되는 것이지 "대체"가 아니다.
2. RDY 게이트를 끼울 정확한 위치는 `tdc_touch.c:301`(300행 경과시간 게이트와 302행 read 사이, 노말 FSM)과 `main.c:1232`(1231행과 1233행 사이, 실사용 절전)이며, 둘 다 현재 공백 줄이라 삽입이 구조적으로 깔끔하다.
3. BLE 등 비-터치 로직은 100ms tick이 아니라 CFX 구동의 별개의 더 빠른 외곽 tick(`iterationFlag`)에 얹혀 있다 — 이 tick은 제거 불가하지만, `tdc_touch.c`의 100ms 게이트는 이미 그 외곽 tick과 무관하게 격리된 사설 게이트이므로 **외곽 tick을 그대로 둔 채 터치 I2C 부분만 게이팅하는 것이 곧바로 가능**하다.
4. `func_sleep()`의 ULP 루프 본문은 터치 전용(BLE·ISD 호출 0건)이라 I2C 게이팅은 자유롭지만, 그 100ms 웨이크업을 만드는 TIMER3 인터럽트 핸들러 자체는 `led_arbiter_tick()`과 결합돼 있다는 것을 본 노드가 새로 확인했다 — "게이팅"과 "tick 주기 자체의 재설계"는 이 경로에서 영향 범위가 다르다.
5. RDY 이벤트 플래그 자체는 현재 코드베이스에 0건(신규 구현 전제, 본 노드가 독립 grep으로 재확인) — 본 문서는 "어디에 꽂을까"만 확정했고, "무엇을 꽂을까"(ISR·플래그 구현)는 C4·후보_6 소관이다.

---

**주요 근거 파일 경로**:
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/systemControl/tdc_touch.c` (전체, 특히 273~421행)
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_iqs323.c` (전체, 특히 128~230행, 347~398행)
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_time.h`
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_config.h`
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/main.c` (90~130행, 278~520행, 700~1255행)
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/systemControl/driver_timmer.c`
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Gen1_5/common/ci_timer.c` (전체)
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Cortex-M3-src/BleCommunication/tdc_remote_general_debug.c` (169·174행)
- `/mnt/e/Claude/projects/Sound1/src/2__cm3/Gen1_5/common/ci_dio.c` (104~168행, RDY 신규 플래그 부재 확인용 대조)
