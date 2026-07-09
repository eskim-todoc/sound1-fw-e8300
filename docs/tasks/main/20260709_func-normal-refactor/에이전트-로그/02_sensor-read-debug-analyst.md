---
name: 센서읽기·디버그블록 분석 — func_normal 423~473행
purpose: func_normal() 분해 계획 수립을 위한 담당 구역(센서 읽기 5종 + fake_0x34 QCC 대체 디버그 블록) 책임·상태수명·결합도 분석
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: stable
tags: [main, func_normal, qcc_batt_timeout, fake_0x34, refactor]
---

# 센서읽기·디버그블록 분석 (main.c 423~473행 담당)

**TL;DR**: 5종 센서 읽기(441~452행)는 전부 부작용 없는 순수 getter이나 `tdc_touch_process()`만 예외(I2C 폴링+내부 FSM 전진, 부작용 있음). `#if 1` 블록(456~485행)은 상시 컴파일(매크로 아닌 하드코딩 1)이라 사실상 상용 코드이며, 내부 `static fake_0x34`/`fake_0x34_done`은 **func_normal 호출을 넘어(절전 재진입 후에도) 유지**되는 진짜 static이라 그대로 함수로 추출해도 안전하다. 단 `qcc_batt_timeout`(비-static 지역변수, 679행에서 소비, 923행 근방 사용처는 실재하지 않음 — 함수는 742행에서 종료)은 "한번 true면 그 반복 내내 true 유지"하는 sticky 플래그라, 추출 함수가 매 호출 bool을 반환하는 방식으로 바꾸면 `qcc_batt_timeout = f()`처럼 직접 대입해선 안 되고 `if (f()) qcc_batt_timeout = true;` 형태로 OR-누적해야 동작이 보존된다. 5개 반환값(mcuErrorCode/usbConnectorState/ledPattern/batteryLevel/powerButtonPushed)은 상호 의존 없이 병렬 취득 가능 — 구조체로 묶어 단일 함수로 추출 권장.

## 1. 각 읽기 호출의 부작용 여부

| 호출 | 실제 구현 | 부작용 | 비고 |
|---|---|---|---|
| `readErrorCode()` | `error.c:149` `return errorCode;` | 없음 (순수 getter) | 모듈 전역 `errorCode` 단순 반환 |
| `snd_charger_get_state()` | `batteryNPowerControl.c:90` `return cfx_cm3_sharedMemoryAll.chargerState;` | 없음 (순수 getter) | CFX 공유메모리 값 반환 |
| `geteLED_OutputPattern()` | `LedOutput.c:403` `return LedOutputPattern;` | 없음 (순수 getter) | 모듈 static 반환 |
| `snd_batt_get_level()` | `batteryNPowerControl.c:47` `snd_batt_get_percent()` 값을 구간 매핑 | 없음 (순수 변환, 내부에서 percent getter만 호출) | percent → `EN__BATTERY_LEVEL` enum 매핑 |
| `snd_batt_get_state()` (fake_0x34 블록 내부에서 사용) | `batteryNPowerControl.c:27` `return s_snd_batt_state;` | 없음 (순수 getter) | |
| `tdc_touch_process()` | `tdc_touch.c:273` | **있음** — ① `ci_timer_get_tick()` 기반 폴링 게이트(`s_poll_tick_old` static 갱신), ② `tdc_touch_iqs323_read_status()` I2C 실제 하드웨어 리드, ③ 내부 init/터치 FSM(`s_init_state` 등) 전진, ④ 디버그 프린트/롱터치 액션 디스패치 | 5개 읽기 중 유일하게 순수하지 않음 — 타이밍(폴링 간격) 의존적이라 호출 위치·빈도를 바꾸면 동작이 달라질 수 있음 |

**결론**: `readErrorCode/snd_charger_get_state/geteLED_OutputPattern/snd_batt_get_level`은 서로 순서 무관·부작용 없는 조회이므로 자유롭게 재배치·구조체 취득 가능. `tdc_touch_process()`는 매 iteration마다 정확히 1회, 현재 위치(다른 4개 읽기 이후, `systemControl()` 호출 이전)에서 호출되는 시점을 그대로 유지해야 한다 — 추출 시에도 "5개 읽기를 한 함수로 묶되 내부 호출 순서는 원본 그대로" 원칙을 지켜야 함.

## 2. `fake_0x34` / `fake_0x34_done` 생명주기

- 선언 위치: `main.c:458-459`, `func_normal()`의 `while(1)` 루프 내부 `#if 1` 블록 안의 **중첩 블록 스코프**에 `static`으로 선언.
- C 언어 의미론상 static 지역변수는 **선언 위치의 블록 진입 여부와 무관하게 프로그램 수명 전체 동안 값을 유지**한다. 즉 이 두 변수는 `func_normal()` 한 번의 호출 내 반복(iteration)뿐 아니라, **`func_normal()`이 리턴(systemOff→break→func_sleep 후 재진입)되어도 값이 보존**된다 (main.c:323-327의 바깥 `while(1) { func_normal(); func_sleep(); }` 참조).
- 정확한 상태 전이:
  - `fake_0x34_done == 0` 인 동안만 타임아웃 감지 로직이 활성.
  - `fake_0x34 == 0`이면 최초 진입 시각을 `ci_timer_get_tick()`으로 래치.
  - 3000ms 경과 시 `fake_0x34_done = 1` + `qcc_batt_timeout = true` (타임아웃 경로).
  - 그 전에 `snd_batt_get_state() != EN__SND_BATT_STATE_RESET`(QCC가 실제 0x34 배터리 정보를 보내온 경우)이면 `fake_0x34_done = 1`만 세팅하고 `qcc_batt_timeout`은 건드리지 않음(정상 경로, 디버그 로그만 출력).
  - 한 번 `fake_0x34_done = 1`이 되면 **프로그램이 재부팅되지 않는 한 이 블록은 이후 다시는 아무 것도 하지 않는다** — 절전 후 재진입에도 재감지되지 않음(코드 주석 352행이 명시한 의도와 일치).
- `qcc_batt_timeout`은 `main.c:353`에서 **매 `func_normal()` 호출마다 `false`로 재선언**되는 비-static 지역변수 — 절전에서 깨어나 재진입할 때마다 리셋됨. 471행에서 한 번 `true`로 세팅된 후에는 **같은 `func_normal()` 호출(같은 절전 사이클) 내에서는 그 값을 되돌리는 코드가 없음** — 679~691행(같은 while(1) 루프, iteration 게이트 바깥)에서 `if (qcc_batt_timeout)` 로 소비되어, 최초 1회 `led_request(LED_SRC_POWER, LED_ST_POWER_OFF)` 발동 후, burst 완료 시 `systemState.systemOff = true` 세팅 → 아래쪽 절전 진입 분기(701행)에서 `break`로 `while(1)` 탈출 → `func_sleep()` 진입.
- **923행 근방 사용처는 실재하지 않음**: `func_normal()`은 742행 `return 0;`으로 종료되며, `grep -n "qcc_batt_timeout" main.c` 결과 사용처는 353(선언)·471(set)·679(소비) 세 곳뿐. 지시문의 "923행" 참조는 이 파일 기준으로는 해당 없음(다른 파일/버전 라인 넘버 혼선으로 추정) — 종합 문서 작성 시 이 점 유의 필요.
- 요약하면 `qcc_batt_timeout`은 **"이번 절전 사이클(한 번의 func_normal 호출) 동안 계속 살아있어야 하는" sticky bool**이며, 함수 스코프 전체(353행 선언~691행 마지막 소비)에 걸쳐 생존해야 한다. 절전 재진입 시 리셋되는 것이 의도된 동작(무한 재절전 방지, 주석 350-352행 참조).

## 3. `#if 1` 블록의 컴파일 조건 및 추출 가능성

- `#if 1`은 매크로가 아니라 **상수 1**이 그대로 조건에 박혀 있음 — 어떤 빌드 설정으로도 이 블록을 끌 방법이 없다. 주석은 "QCC 대체용 디버깅 코드"라 부르지만 실질적으로는 **상시 컴파일·상시 실행되는 상용 경로**다(디버깅 코드라는 이름이 오해를 유발하는 하드코딩 사례 — 은수님이 지적한 "하드코딩 느낌"에 정확히 해당).
- 별도 함수로 추출 가능. 순수 함수화 조건 충족: 참조하는 것은 `ci_timer_get_tick()`(부작용 없는 tick getter), `snd_batt_get_state()`(부작용 없는 getter), `tdc_timer_get_t3_tick()`(디버그 프린트 전용, 부작용 없음), `ci_printw/ci_printi`(로그 출력, 상태에 영향 없음) 뿐 — 함수 스코프 밖 상태를 읽거나 쓰지 않는다.
- **주의**: `static fake_0x34`/`fake_0x34_done`을 추출된 함수의 로컬 static으로 그대로 옮기면 앞서 2절에서 설명한 "프로그램 수명 전체 유지" 의미론이 **그대로 보존**된다(스코프 위치가 어디든 static 지역변수 수명은 동일하므로 1:1 동치). 추출 자체는 안전.
- **함정(behavior-preserving 관점에서 가장 중요한 발견)**: 추출 함수가 "이번 호출에서 새로 타임아웃을 감지했는가"를 bool로 반환하는 형태(`bool tdc_qcc_batt_check_timeout(void)`)로 설계하면, `fake_0x34_done`이 이미 1인 이후의 모든 호출에서는 함수가 `false`를 반환한다. 이때 호출부에서 `qcc_batt_timeout = tdc_qcc_batt_check_timeout();` 처럼 **직접 대입**하면, 최초 타임아웃 감지 다음 iteration부터 `qcc_batt_timeout`이 `false`로 덮어써져 679행의 파워오프 시퀀스가 중단되는 **회귀 버그**가 발생한다. 원본 코드는 `qcc_batt_timeout = true;`만 있고 `false`로 되돌리는 대입이 전혀 없으므로(353행 함수 진입 시 1회 초기화 이후), 반드시 `if (tdc_qcc_batt_check_timeout()) { qcc_batt_timeout = true; }` 형태의 **OR-누적(sticky) 패턴**으로 호출해야 동작이 보존된다. 계획.md 단계에서 반드시 이 패턴을 명시할 것.
- 인자: 없음(내부적으로 전부 조회). 반환: `bool`(이번 호출에서 새로 타임아웃 감지했는지) — 기존 로그 문자열(`[FAKE_0x34] ...`)은 함수 내부에 그대로 유지.

## 4. 나머지 센서 읽기(441~452행) 추출 방안

- 5개 반환값(`mcuErrorCode`, `usbConnectorState`, `ledPattern`, `batteryLevel`, `powerButtonPushed`) 모두 이후 `systemControl()` 호출(492행)의 인자로 쓰일 뿐, 서로 간 의존관계는 없음(순서 무관, 4개는 병렬 취득 가능한 순수 getter + `tdc_touch_process()` 1개는 부작용 있으나 다른 4개와 상호 의존 없음).
- 반환값이 5개라 개별 out-parameter로 뽑기보다 **구조체로 묶어 단일 함수로 추출**하는 편이 호출부를 훨씬 깔끔하게 만든다. 예:

```c
typedef struct
{
    ST__ERROR_CODE     mcuErrorCode;
    ST__USB_CONNECTOR  usbConnectorState;
    EN__LED_PATTERN    ledPattern;
    EN__BATTERY_LEVEL  batteryLevel;
    bool               powerButtonPushed;
} tdc_sensor_snapshot_t;

static tdc_sensor_snapshot_t tdc_read_sensor_snapshot(void)
{
    tdc_sensor_snapshot_t s;
    s.mcuErrorCode       = readErrorCode();
    s.usbConnectorState  = snd_charger_get_state();
    s.ledPattern         = geteLED_OutputPattern();
    s.batteryLevel       = snd_batt_get_level();
    s.powerButtonPushed  = tdc_touch_process();  // 부작용 있음 — 순서 유지 필수
    return s;
}
```

- 호출부는 `tdc_sensor_snapshot_t snap = tdc_read_sensor_snapshot();` 한 줄 + `snap.xxx`로 `systemControl()` 인자 전달. 신규 심볼이므로 프로젝트 네이밍 규칙에 따라 `tdc_` 접두어 부여(루트 지침 §7).
- 구조체 반환은 값 복사(각 필드가 매 iteration 새로 채워짐)라 기존 지역변수 대입과 동일한 값 의미론 유지 — 동작 변화 없음.
- 위 `tdc_qcc_batt_check_timeout()`과 조합 시 423~473행 전체가 다음 2줄로 축약 가능:
  ```c
  tdc_sensor_snapshot_t snap = tdc_read_sensor_snapshot();
  if (tdc_qcc_batt_check_timeout()) { qcc_batt_timeout = true; }
  ```

## 부록: 지시문 범위 표기와 실제 코드 라인 불일치

- 지시문의 "POWER-ON 트레이스 폴링"에 해당하는 코드가 423~473행 범위 내에 없음. `main.c` 전체에서 "POWER-ON"/"PowerOn" 문자열은 발견되지 않았고, 가장 근접한 후보는 362~371행의 `cfx_cm3_sharedMemoryAll.is_CFX_started == 1` busy-wait("[INFO] CFX STARTED" 출력)이나, 이는 `func_normal()` 최초 진입 시 1회만 도는 부팅 대기 루프이지 iteration마다 반복되는 423~473행 블록과는 무관함(00_오케스트레이터-입력.md 기준 01번 담당 구역 320-421행에 해당). 종합 시 참고.
- iteration 게이트 자체(`iterationFlag`, `main.c:108,110,117,439`)는 CFX 타이머 ISR이 `enable_iteration()`/`disable_iteration()`으로 set/reset하는 module-static bool — 439~668행 전체 블록을 감싸는 "이번 tick에 1회만 실행" 게이트이며, 423~473행 담당 구역은 이 게이트의 **내부**에 있다(게이트 자체를 별도 추출할 필요는 없고, 추출 함수들은 게이트 안에서 호출되는 형태 유지).
