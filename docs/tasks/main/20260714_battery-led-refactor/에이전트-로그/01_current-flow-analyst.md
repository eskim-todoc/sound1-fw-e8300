---
name: current-flow-analyst
purpose: func_normal() 배터리→LED 블록(542~578)·매핑 블록(620~654)의 현재 동작을 작업트리 실측 기준으로 빈틈없이 해부 — 상태 전이표·마진의 코드적 정체·순서 의존·static 수명 확정
type: agent-log
maturity: experimental
tags: [main.c, battery, LED, hysteresis, func_normal, static-state, refactor]
---

# 현재 배터리→LED 데이터 흐름 해부 (작업트리 실측 기준)

> [!NOTE]
> **TL;DR**: 작업트리 `main.c`는 배터리 임계값이 커밋본(40/41/65/64)이 아니라 **10/12/80/78**로 미커밋 되돌림 상태다(git diff 확인). 이 값으로 sticky band는 폭 2%p({10,11}, {78,79})가 되어 커밋본(폭 1%p)보다 넓다. "변화 마진"의 정체는 진입≠이탈 임계 이원화 + `prev_batt_st`(또는 `s_map_low_active`) 참조 조건분기 그 자체이며, 제거 시 if-else 체인에서 정확히 2개 분기(sticky 조건)가 사라진다. 실행 순서(배터리→ISD→매핑)는 `batt_st`/`isd_conn` 데이터 의존으로 강제되며 마진 제거와 무관하게 보존해야 한다.

## 0. 라인 안내 및 미커밋 상태 확인

`Read` 도구로 직접 확인한 작업트리 실측 라인(2026-07-14 현재):
- 블록 전체: `main.c:538~655`
- 배터리 하위 블록: `542~578`
- ISD 하위 블록: `580~618`
- 매핑 하위 블록: `620~654`

`git diff -- src/2__cm3/Cortex-M3-src/main.c` 결과(전체 diff, 이 파일에서 유일한 hunk):

```diff
-                else if (pct < 40 /*10*/)
+                else if (pct < 10)
                     batt_st = LED_ST_BATT_CRITICAL;
-                else if (pct < 41 /*12*/ && prev_batt_st == LED_ST_BATT_CRITICAL)
+                else if (pct < 12 && prev_batt_st == LED_ST_BATT_CRITICAL)
                     batt_st = LED_ST_BATT_CRITICAL;
-                else if (pct >= 65 /*80*/)
+                else if (pct >= 80)
                     batt_st = LED_ST_BATT_READY;
-                else if (pct >= 64 /*78*/ && prev_batt_st == LED_ST_BATT_READY)
+                else if (pct >= 78 && prev_batt_st == LED_ST_BATT_READY)
                     batt_st = LED_ST_BATT_READY;
```

**확인 사항**:
- 변경 범위는 배터리 블록 4개 리터럴뿐이다. 매핑 블록(`s_map_low_active`, pct 20/22, `main.c:628,630`)은 diff에 없음 — 작업트리·커밋본 동일.
- 커밋본은 "현재값 40/41/65/64 + 주석으로 옛값 /*10*/ /*12*/ /*80*/ /*78*/ 보존" 형태였으나, 작업트리는 그 **주석 처리된 옛값을 실제 코드로 승격**하고 40/41/65/64는 완전히 삭제했다(주석 흔적 없음). 즉 단순 값 스와프가 아니라 "이력 주석 → 활성 코드"로의 리버트다.
- 선행 `20260709_func-normal-refactor/에이전트-로그/04_...` 문서는 **커밋본(40/41/65/64) 기준**으로 sticky band 폭을 "1%p로 좁아짐"이라 서술했는데, 이는 그 시점 워킹트리(당시도 unstaged 상태였던 것으로 보임) 기준이지 현재 작업트리와는 다르다. 아래 §1.1에서 폭 재계산 결과 **현재 작업트리는 폭 2%p**로, 04 문서가 "좁아짐"이라 평가한 것과 반대 방향이다. **이하 전이표는 작업트리 실측(10/12/80/78) 기준으로 다시 계산한 것이며, 04 문서의 수치 결론(폭 1%p)은 현재 상태에 그대로 적용하면 안 된다.**

## 1. 배터리 블록 (main.c:542~578)

### 1.1 `pct` 획득 경로

```c
548 #ifdef ENABLE_UI_CMD
549     bool ovr_batt_active = tdc_ui_command_override_battery_active();
550     int  pct             = ovr_batt_active ? (int) tdc_ui_command_override_battery_percent() : snd_batt_get_percent();
551 #else
552     bool ovr_batt_active = false;
553     int  pct             = snd_batt_get_percent();
554 #endif
```

- `ENABLE_UI_CMD`는 `src/2__cm3/processorDirective.h:90`에서 `#define`으로 **항상 활성**이다(현재 빌드에서 `#else` 분기는 죽은 코드가 아니라 단지 미사용). 즉 548~550행이 실제 실행 경로.
- `snd_batt_get_percent()`(`batteryNPowerControl.c:37~40`)는 파일 전역 `volatile int s_snd_batt_percent`(`batteryNPowerControl.c:18`, 초기값 0)의 단순 getter다. 이 값은 QCC가 0x34(Power info)를 보낼 때 `snd_batt_set_percent()`(같은 파일 42~45행)로 채워지는 구조로 추정된다(setter 호출부 자체는 이번 검색 범위 밖 — QCC 통신 핸들러 쪽, **확인 아님·추정**). E8300 측에는 이 getter/setter 경로에 별도 평활화·마진 로직이 없다 — 요구사항 문서의 "QCC가 이미 측정·필터링" 전제와 부합한다.
- 오버라이드 시(`ovr_batt_active==true`) `pct`는 UI 커맨드 값으로 **완전히 대체**된다. 이 `pct`는 뒤이어 매핑 블록(628/630행)도 그대로 재사용하므로, 배터리 오버라이드가 활성화되면 매핑 블록의 "배터리 LOW" 판정에도 영향을 준다(§3 참조).

### 1.2 `EN__SND_BATT_STATE_RESET` 처리

```c
558  if (!ovr_batt_active && snd_batt_get_state() == EN__SND_BATT_STATE_RESET)
559  {
560      batt_st = LED_ST_IDLE;
561  }
```

- `snd_batt_get_state()`(`batteryNPowerControl.c:27~30`)가 `EN__SND_BATT_STATE_RESET`(부팅 후 QCC 0x34 미수신 상태, 초기값 `batteryNPowerControl.c:19`)이면, **오버라이드가 비활성일 때만** 배터리 판정을 건너뛰고 `LED_ST_IDLE`을 강제한다. 주석(542~547행)이 명시하듯, 이 게이트가 없으면 `pct==0`이 그대로 `pct<10`(CRITICAL) 조건에 걸려 부팅 초기 노란색 점멸이 잠깐 발생하는 결함을 막는다.
- 오버라이드가 활성이면 이 RESET 게이트를 **우회**한다 — 즉 UI 강제 테스트 중에는 실제 QCC 수신 여부와 무관하게 오버라이드 pct로 배터리 상태를 계산한다.

### 1.3 히스테리시스 if-else 체인과 상태 전이표 (작업트리 실측 10/12/80/78)

```c
555  static led_state_t prev_batt_st = LED_ST_IDLE;
...
563  else if (pct < 10)
564      batt_st = LED_ST_BATT_CRITICAL;
565  else if (pct < 12 && prev_batt_st == LED_ST_BATT_CRITICAL)
566      batt_st = LED_ST_BATT_CRITICAL;
567  else if (pct >= 80)
568      batt_st = LED_ST_BATT_READY;
569  else if (pct >= 78 && prev_batt_st == LED_ST_BATT_READY)
570      batt_st = LED_ST_BATT_READY;
571  else
572      batt_st = LED_ST_BATT_MID;
574  prev_batt_st = batt_st;
```

`else if` 체인이므로 각 조건은 앞선 조건이 모두 거짓일 때만 평가된다. pct 구간별로 실제 도달 가능한 분기를 전개하면:

| pct 구간 | prev_batt_st 무관 여부 | 결과 `batt_st` | 근거 라인 |
|---|---|---|---|
| RESET 상태(`!ovr_batt_active`) | 무관 | `LED_ST_IDLE` | 558~561 |
| `pct < 10` | 무관 (항상) | `LED_ST_BATT_CRITICAL` | 563~564 |
| `10 ≤ pct < 12` | `prev==CRITICAL`이면 유지 | `LED_ST_BATT_CRITICAL` (sticky) | 565~566 |
| `10 ≤ pct < 12` | `prev!=CRITICAL`이면 | `LED_ST_BATT_MID` (565 거짓 → 567/569도 거짓 → 571 도달) | 571~572 |
| `12 ≤ pct < 78` | 무관 | `LED_ST_BATT_MID` | 571~572 |
| `78 ≤ pct < 80` | `prev==READY`이면 유지 | `LED_ST_BATT_READY` (sticky) | 569~570 |
| `78 ≤ pct < 80` | `prev!=READY`이면 | `LED_ST_BATT_MID` | 571~572 |
| `pct ≥ 80` | 무관 (항상) | `LED_ST_BATT_READY` | 567~568 |

**진입/이탈/sticky band 요약**:
- CRITICAL 진입: `pct<10`로 하강 통과 시. CRITICAL 이탈: `pct≥12`로 상승 통과 시. sticky band는 `pct∈{10,11}`(정수 pct 기준 폭 2%p) — 이 구간에서는 "직전이 CRITICAL이었는가"만이 유지 여부를 결정.
- READY 진입: `pct≥80`로 상승 통과 시. READY 이탈: `pct<78`로 하강 통과 시. sticky band는 `pct∈{78,79}`(폭 2%p).
- MID는 그 외 전부 — CRITICAL/READY 어느 쪽도 아니고 sticky 조건도 불성립한 경우의 기본값(571 `else`).
- `prev_batt_st`는 **매 iteration 시작 시점의 직전 확정 상태**를 읽어 sticky 여부만 결정하고(565·569행), 그 iteration이 끝나면 574행에서 이번에 확정된 `batt_st`로 즉시 갱신된다 — 오버라이드로 `led_request()` 송출 자체가 억제되더라도(576행 가드) 574행의 저장은 항상 실행된다(가드 범위가 578행 `led_request()` 호출 한 줄에만 걸려 있고 574행은 가드 밖).

### 1.4 "변화 마진(히스테리시스)"의 코드적 정체

마진은 두 가지 장치의 조합으로 구현되어 있다:
1. **진입 임계 ≠ 이탈 임계의 이원화** — CRITICAL 진입 `10`, 이탈 `12`; READY 진입 `80`, 이탈 `78`. 단일 임계값이라면 진입=이탈이어야 하는데 여기선 2%p씩 벌려놨다.
2. **`prev_batt_st` 참조 조건분기(565, 569행)** — 벌어진 구간(sticky band) 안에서 "이전에 CRITICAL/READY였는가"를 검사해 유지시키는 로직. 이 두 줄이 없으면 sticky band는 자동으로 MID에 흡수된다.

**마진을 제거하면 정확히 사라지는 것**:
- 565행(`else if (pct < 12 && prev_batt_st == LED_ST_BATT_CRITICAL) ...`) 통째로 삭제.
- 569행(`else if (pct >= 78 && prev_batt_st == LED_ST_BATT_READY) ...`) 통째로 삭제.
- 남는 체인은 `RESET→IDLE / pct<X→CRITICAL / pct≥Y→READY / else→MID`의 4분기(단일 임계값 X, Y 필요 — 요구사항 이슈_2, 이번 페르소나 범위 밖 결정 사항)로 단순화된다.
- `prev_batt_st`(555행 static)는 이 블록 내부에서 sticky 판정 외 다른 용도가 없다(§5에서 참조 범위 재확인) — 두 sticky 분기가 사라지면 **읽는 곳이 없어지므로** `prev_batt_st` 변수 자체도 삭제 가능해진다. 단, 574행 "매 iteration 갱신" 자체는 다른 블록이 참조하지 않으므로(§5) 변수 삭제 시 574행도 함께 제거하면 된다. `batt_st`의 반환/전달(ISD 블록이 필요, §4)은 마진 제거와 무관하게 그대로 유지해야 한다.
- 단일 임계값이 되면 마진 폭(현재 sticky {10,11}·{78,79})은 물리적으로 사라지고, pct가 경계값을 정수 단위로 오르내릴 때마다 매 iteration LED 상태가 즉시 바뀐다(디바운스 없음) — 이것이 정확히 사용자가 요청한 "변화 마진 제거"의 동작 변화다.

## 2. ISD 블록 (main.c:580~618) — 순서 의존 확인용 재확인

```c
591  if (isd_conn)
593      if (readLED_indicatorOnOff() == 1)
595          led_request(LED_SRC_ISD, LED_ST_IN_USE);
598      else
599          led_request(LED_SRC_ISD, LED_ST_NONE);
606  else  // 내부기 미 연결
609      led_request(LED_SRC_ISD, LED_ST_NONE);
610      led_request(LED_SRC_BATTERY, batt_st);
616  led_set_isd_conn_state(isd_conn);
```

- `isd_conn==false`(ISD 미연결)일 때 609~610행에서 **배터리 블록의 산출물 `batt_st`를 다시 `led_request(LED_SRC_BATTERY, batt_st)`로 재송출**한다. 이는 배터리 블록이 **반드시 먼저 실행**되어 `batt_st`가 확정돼 있어야 함을 뜻한다 — 순서를 바꾸면 컴파일조차 안 되거나(변수 미정의) 이전 iteration의 낡은 `batt_st`를 참조하는 버그가 된다.
- `led_set_isd_conn_state(isd_conn)`(616행)은 `LedOutput.c:87`의 `s_isd_conn`(volatile int)을 갱신한다. 이 값은 `LedOutput.c:709` `compute_best_state()`가 소비하며, `led_arbiter_tick()`(`LedOutput.c:725`, Timer 3 ISR에서 구동)이 매 tick 이를 참조한다(실측 확인, grep으로 정의 위치 재검증 완료). 613~615행 주석대로 `s_req[LED_SRC_ISD]` 확정(591~610행) **직후**에 `s_isd_conn`을 갱신함으로써 ISR이 두 상태 사이에 끼어들어 1-tick IN_USE 잔상이 뜨는 race를 줄인다. 이 순서(led_request 확정 → led_set_isd_conn_state)는 **마진 제거와 무관하게 절대 보존** 대상이다(이번 작업 범위外_2, 확인만).

## 3. 매핑 블록 (main.c:620~654)

```c
620  /* Mapping (SS4.4) — 배터리 레벨(LOW 임계 20%) × ISD 연결 여부 4종 분기.
621   * LOW 진입 pct ≤ 20, 해제 pct ≥ 22 (±2% 히스테리시스). */
627  static bool s_map_low_active = false;
628  if (pct <= 20)
629      s_map_low_active = true;
630  else if (pct >= 22)
631      s_map_low_active = false;
632      /* pct == 21 구간은 직전 상태 유지 */
```

- `pct`는 **배터리 블록에서 이미 계산된 동일 변수를 재사용**한다(별도 조회 없음 — 동일 enclosing 스코프 `{ }` 538~655행 내부이므로 C 변수 스코프상 그대로 공유). 오버라이드(`ovr_batt_active`)가 활성이면 매핑 블록의 LOW 판정도 오버라이드된 pct를 쓴다는 뜻.
- `s_map_low_active`는 배터리 블록과 **동형 마진 구조**이나 구현 스타일이 다르다: 배터리는 "진입+sticky 조건"을 매 분기 반복 기술, 매핑은 **불리언 래치를 먼저 확정**한 뒤 그 래치로 LED 상태를 계산하는 2단계 구조(628~631행에서 래치 확정 → 637~648행에서 소비). 진입 `pct≤20`, 이탈 `pct≥22`, sticky band는 `pct==21` 단 1개 값(폭 1%p) — 632행 주석이 "직전 상태 유지"라 명시.
- **4분기**(636~654행): `map_conn`(매핑 앱 연결 여부) × `isd_conn`(ISD 블록 산출물) × `s_map_low_active`(배터리 LOW 래치)의 조합.

| map_conn | s_map_low_active | isd_conn | 결과 LED 상태 | 라인 |
|---|---|---|---|---|
| false | – | – | `LED_ST_NONE` | 651~652 |
| true | true(LOW) | true | `LED_ST_MAPPING_ISD_BATT_LOW` (보라 점멸 100/900ms) | 642 |
| true | true(LOW) | false | `LED_ST_MAPPING_NO_ISD_BATT_LOW` (보라 지속) | 642 |
| true | false(READY) | true | `LED_ST_MAPPING_ISD_BATT_READY` (파랑 점멸 200/800ms) | 646 |
| true | false(READY) | false | `LED_ST_MAPPING_NO_ISD_BATT_READY` (파랑 지속) | 646 |

(색상·점멸 패턴 주석은 `LedOutput.h:73~76`에서 확인.)

- 마진 제거 시: 628~631행이 `bool low = (pct <= T);` 단일 조건(임계 T 미정, 이슈_2)으로 축소되고, `s_map_low_active` static 변수 자체도 배터리 블록의 `prev_batt_st`와 동일 논리로 삭제 가능(다른 블록 미참조, §5).

## 4. 순서 의존 종합 (배터리 → ISD → 매핑)

| 데이터 | 산출 블록 | 소비 블록 | 의존 성격 |
|---|---|---|---|
| `pct` | 배터리 블록(549~550, 1회 계산) | 배터리 자신 + 매핑 블록(628, 630) | 값 공유(재계산 아님) |
| `batt_st` | 배터리 블록(556~574) | ISD 블록(610행, ISD 미연결 시 재송출) | **배터리가 ISD보다 먼저 실행돼야** 함 |
| `isd_conn`(오버라이드 반영 확정값) | ISD 블록(582/584) | 매핑 블록(642, 646) | **ISD가 매핑보다 먼저 실행돼야** 함 |

이 순서(배터리→ISD→매핑, 코드 상 538~655행의 물리적 순서와 동일)를 어기면: (a) ISD 블록이 아직 계산 안 된 `batt_st`를 참조(컴파일 에러 또는 스코프 밖 참조), (b) 매핑 블록이 이전 iteration의 낡은 `isd_conn`으로 LED 색상을 잘못 고른다. **이 순서 제약은 마진 제거와 독립적**이며, 함수로 재캡슐화하더라도 호출 순서(배터리 함수 → ISD 함수(batt_st 인자) → 매핑 함수(isd_conn 인자))로 반드시 보존해야 한다.

## 5. static 상태 수명 (`prev_batt_st`, `s_map_low_active`)

- 선언 위치: `prev_batt_st`는 `main.c:555`, `s_map_low_active`는 `main.c:627` — 둘 다 `func_normal()` 내부의 **두 번째 while(1)**(437~739행, iteration 루프) 안, 그 안에서도 LED 블록(538~655) 스코프 내부에 있다.
- `main.c` 전체에서 두 이름을 재검색(grep)한 결과 **매치는 이 파일 1개**뿐이었다 — 즉 다른 소스 파일은 물론 `main.c` 내 다른 함수도 이 두 변수를 참조하지 않는다. 순수하게 이 블록 자기 자신(다음 iteration)을 위한 지역 상태다.
- 함수 호출 구조: `main()`(추정 함수, 323~327행에서 실측)이 `while(1){ func_normal(); func_sleep(); }`로 `func_normal()`을 반복 호출한다(325~326행 실측 확인). `func_normal()`은 호출될 때마다 새 스택 프레임에서 시작하지만, C 표준상 함수-지역 `static` 변수는 **그 초기화 구문이 프로그램 실행 중 처음 도달했을 때 단 1회만** 값을 설정하고, 이후로는 함수가 반환(741행)하고 다시 호출되어도 이전 값을 그대로 들고 있다. 따라서 `prev_batt_st`·`s_map_low_active`는:
  1. `func_normal()` 내부의 iteration 루프(437~739)를 도는 동안 매 iteration 값이 갱신되며 유지되고,
  2. `func_normal()`이 `break`(726행)로 빠져나가 `func_sleep()`으로 절전에 들어갔다가, 깨어나서 `func_normal()`이 **다시 호출**되어도 초기화되지 않고 절전 직전의 마지막 값을 그대로 유지한다.
- 실질적 의미: 절전에서 깨어난 직후 새 iteration에서 QCC가 아직 신선한 pct를 보내지 않았더라도(RESET 상태) 558행 게이트가 `LED_ST_IDLE`을 강제하므로 `prev_batt_st`의 stale 값이 즉시 영향을 주지는 않는다. 다만 RESET을 벗어난 첫 iteration에서 pct가 마침 sticky band(10~11 또는 78~79)에 걸리면, **절전 이전 세션의 마지막 배터리 상태**가 그대로 sticky 판정에 쓰인다 — 즉 마진 로직의 "기억"은 전원 재부팅이 아닌 절전/재개 경계도 넘어간다(확인: static 변수 수명 자체는 확실, "절전 직후 sticky band 진입 시나리오"의 실제 발생 빈도는 QCC 타이밍에 달려 있어 추정).
- 마진 제거로 두 static 변수를 완전히 삭제하면 이 "세션을 넘나드는 잔존 상태" 자체가 사라진다 — 이는 마진 제거의 부수 효과로서 계획 문서에 명시할 가치가 있다(behavior 변경이 이 지점에서도 발생).
- 참고로 `snd_batt_get_state()`/`snd_batt_get_percent()`가 읽는 `s_snd_batt_state`/`s_snd_batt_percent`(`batteryNPowerControl.c:18~19`)는 이 블록과 무관한 **별도의 파일-스코프 volatile 전역**이며, LED 블록의 `static` 지역 변수와는 수명 관리 주체가 다르다(QCC 통신 핸들러가 갱신 주체로 추정 — 확인 아님).

## 자체 검토

### 지침 준수
- 읽기 전용 분석만 수행, `src/` 무수정 확인(Edit/Write 도구를 코드 파일에 사용하지 않음).
- 모든 정량적 주장에 `파일:라인` 근거 제시, 추측 항목은 "추정"·"확인 아님"으로 명시.

### 논리적 정합성
| 점검 | 결과 |
|---|---|
| 작업트리 실측값(10/12/80/78)과 커밋본(40/41/65/64) 차이를 git diff로 직접 검증했는가 | 완료 — §0 |
| 선행 04 문서의 "sticky band 폭 1%p" 결론이 현재 상태에 그대로 적용되는지 검증했는가 | 완료 — 현재는 폭 2%p로 재계산, 04 문서와 다름을 명시 |
| `prev_batt_st`/`s_map_low_active`의 참조 범위(main.c 유일)를 grep으로 재확인했는가 | 완료 — §5 |
| 순서 의존(배터리→ISD→매핑)의 근거를 데이터 흐름으로 제시했는가 | 완료 — §4 |
| `led_set_isd_conn_state()` 순서 보존 주장을 LedOutput.c 실제 코드로 재검증했는가 | 완료 — §2 (`LedOutput.c:87,709,725` 확인) |

### 미해결 이슈 (승인 게이트 소관, 이 문서는 옵션 제시만)
- 마진 제거 후 단일 임계값을 무엇으로 할지(요구사항 이슈_2)는 이 페르소나의 결정 범위 밖 — 옵션 비교는 `분석.md`/`계획.md`에서 종합.
- `snd_batt_set_percent()` 호출부(QCC 핸들러 쪽)는 이번 검색 범위 밖이라 미확인 — 필요 시 별도 확인 권장.
