---
name: 03 — systemControl percent 직접사용 재설계
purpose: systemControl() 의 EN__BATTERY_LEVEL 파라미터를 배터리 percent 직접 비교로 대체 시 동작 등가성 검증, 저전력/ISD 가드 부수효과 분석, 재설계 옵션(A/B) 제시
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: experimental
tags: [main, systemControl, battery, EN__BATTERY_LEVEL, percent, redesign, equivalence]
persona: 03-systemcontrol-redesign-analyst
---

# 03. `systemControl()` percent 직접사용 재설계 분석

**TL;DR**: `systemControl.c:251~253`(<40% 저전력)·`:324`(<20% ISD 가드)의 enum 비교를 `battery_percent < 40` / `battery_percent < 20` 로 대체하면 **percent 0~100 전 구간에서 동작이 100% 동일**하다(경계표 §2 검증, 발산 0건). 251~253은 저전력 시 강제 종료 시퀀스(LED POWER_OFF → `enablePMIC=false`+`systemOff=true`)를 트리거하고, 324는 ISD 연결 시 10분 주기 저배터리 자극 알림(`stimulationTrigger`)을 트리거하는데, 두 부수효과 모두 percent 대체로 그대로 보존된다. 옵션 A(파라미터만 `int battery_percent`로 교체, enum·getter·고아 전역 존치)는 3개 파일 약 15줄 diff의 최소 변경이며, 옵션 B(A + `snd_batt_get_level()`/`EN__BATTERY_LEVEL`/고아 전역/`debugging_for_monitoring` 선언까지 삭제)는 01 census가 이미 "타 소비처 0건, 고아 전역 write조차 비실행"을 확정했으므로 추가 위험이 거의 없다 — **A+B를 한 번에 하는 것을 추천**. RESET(percent=0) 오판정 우려는 systemControl()에는 해당 없음을 제어흐름으로 확인(§5).

## 1. 현재 코드 확인

### 1.1 시그니처 — `systemControl.h:26~32`, `systemControl.c:122~128`

```c
ST__SYSTEM_STATE systemControl(EN__LED_PATTERN   current_led_pattern,
                               ST__ERROR_CODE    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               EN__BATTERY_LEVEL batteryLevel,
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected);
```

### 1.2 소비 지점 1 — `systemControl.c:250~256` (저전력 판정)

```c
// 배터리 방전 상태 확인 (전기기계적안정성 시험을 위해 저전력 범위 변경)
if ((batteryLevel == en__batteryPower_0per)         // 원래 0per만 저전력 인데
    || (batteryLevel == en__batteryPower_0btw20)    // 0~20per 랑
    || (batteryLevel == en__batteryPower_20btw40))  // 20~40per 도 저전력으로 처리 즉, 40per 미만이면 저전력
{
    veryLowBattery = true;
}
else
{
    veryLowBattery = false;
}
```

원 주석대로 원래는 0%만 저전력이었으나 "전기기계적안정성 시험"을 위해 <40%로 확장된 이력이 있음 — **임계값(40) 자체는 범위외(요구사항 §5.2 범위외_2), 값 유지가 원칙**.

### 1.3 소비 지점 2 — `systemControl.c:324` (ISD 저배터리 자극 알림)

```c
if (conneded_ISD && batteryLevel <= en__batteryPower_0btw20)
{
    if (lowBatteryIndicatorCounter == 0)
    {
        stimulationTrigger         = true;
        lowBatteryIndicatorCounter = df_lowbatteryIndicationPeriod_ms;
    }
    lowBatteryIndicatorCounter--;
}
```

`en__batteryPower_0per`(=0) / `en__batteryPower_0btw20`(=1) 두 값에 대해 `<=` 비교 — enum 정의 순서(`batteryNPowerControl.h:17~26`)에 의존한 값 비교(순수 동등비교가 아님)이므로, 이 자체가 이미 "enum을 숫자처럼 쓰는" 코드다. percent 대체가 오히려 이 암묵적 의존을 명시적으로 만든다.

### 1.4 `snd_batt_get_level()` 경계 — `batteryNPowerControl.c:47~88`

| percent 구간 | 반환 enum |
|---|---|
| `100 <= percent` | `en__batteryPower_100per` |
| `80 <= percent < 100` | `en__batteryPower_80btw100` |
| `60 <= percent < 80` | `en__batteryPower_60btw80` |
| `40 <= percent < 60` | `en__batteryPower_40btw60` |
| `20 <= percent < 40` | `en__batteryPower_20btw40` |
| `0 < percent < 20` | `en__batteryPower_0btw20` |
| else(`percent <= 0`, 음수 포함) | `en__batteryPower_0per` |

## 2. percent 등가 검증

### 2.1 `:251~253` ≡ `battery_percent < 40`

| percent | enum 버킷 | 251~253 결과(OR) | `percent<40` | 일치 |
|---|---|---|---|---|
| 음수(이상값) | 0per | true | true | ✅ |
| 0 | 0per | true | true | ✅ |
| 1, 19 | 0btw20 | true | true | ✅ |
| 20, 21, 39 | 20btw40 | true | true | ✅ |
| **40**(경계) | 40btw60 | **false** | **false** | ✅ |
| 41, 59 | 40btw60 | false | false | ✅ |
| 60~99, 100 | 60btw80/80btw100/100per | false | false | ✅ |

**결론**: `en__batteryPower_0per ∪ 0btw20 ∪ 20btw40` 은 정확히 `percent < 40` 과 동치. 경계 40은 **불포함**(40%는 저전력 아님) — 양쪽 동일. 발산 0건.

### 2.2 `:324` ≡ `battery_percent < 20`

| percent | enum 버킷(순서값) | `batteryLevel <= 0btw20`(≤1) | `percent<20` | 일치 |
|---|---|---|---|---|
| 0 | 0per(0) | true | true | ✅ |
| 1~19 | 0btw20(1) | true | true | ✅ |
| **20**(경계) | 20btw40(2) | **false**(2≤1 아님) | **false** | ✅ |
| 21~39 | 20btw40(2) | false | false | ✅ |
| 40+ | 그 이상 | false | false | ✅ |

**결론**: `batteryLevel <= en__batteryPower_0btw20` 은 정확히 `percent < 20` 과 동치. 경계 20은 **불포함**. 발산 0건.

### 2.3 종합

두 대체 모두 percent 0~100(및 음수 이상값 포함) 전 구간에서 **동작 100% 동일**. `snd_batt_get_level()`의 버킷 경계가 정확히 20%p 그리드(margin-free)이고, `systemControl()`의 두 판정이 그 버킷들의 합집합/순서비교로 정확히 20/40 컷과 일치하도록 설계되어 있었기 때문 — 우연이 아니라 애초에 enum 자체가 20%p 그리드로 만들어져 있어서 percent 직접 비교와 수학적으로 동형(isomorphic)이다.

## 3. 저전력/ISD 가드 로직의 부수효과 — percent 대체 후에도 보존되는가

### 3.1 저전력 판정(`:250~315`)이 트리거하는 것

`veryLowBattery == true` (또는 `powerButtonPushed`)가 되면:
1. (최초 1회, `:265~284`) `systemStatus.Led_Pattern = en__LED_POWER_Off`, `led_request(LED_SRC_POWER, LED_ST_POWER_OFF)`, `systemStatus.enable_ISD = false`, `isPowerOffEnabled = true` — **전원 종료 LED 패턴 시작**.
2. (이후 매 tick, `:287~315`) POWER_OFF LED burst가 끝나면(`!tdc_led_is_burst_pending()`) `systemStatus.enablePMIC = false`, `systemStatus.systemOff = true` — **실제 시스템 전원 차단**(PMIC 비활성 + systemOff 플래그, main 루프가 이를 보고 절전/종료 진입).
3. `!veryLowBattery && conneded_ISD` 이면 진행 중이던 power-off 시퀀스를 취소(`:304~311`, 내부기 부착 중 오탐 탈출 경로).

즉 <40% 저전력은 단순 경고가 아니라 **강제 전원 종료**를 유발한다(전기기계적안정성 시험 목적 확장 임계값). percent 대체는 이 판정을 §2.1 표대로 값 단위까지 동일하게 재현하므로 부수효과 전부 보존.

### 3.2 ISD 가드(`:324`)가 트리거하는 것

`conneded_ISD && percent<20` && (POWER_OFF 시퀀스 밖, `isPowerOffEnabled==false`, `:316~357` else 분기)이면 10분(`df_lowbatteryIndicationPeriod_ms`) 주기로 `stimulationTrigger = true` — 함수 말미(`:407`) `systemStatus.StimulationIndicatorTriggerLowPower = stimulationTrigger;` 로 반환되어, 내부기(ISD)에 저배터리 자극(햅틱/알림) 신호를 보내는 근거가 된다. §2.2 검증대로 percent 대체가 이 트리거 조건을 정확히 재현.

## 4. 재설계 옵션

### 옵션 A — 최소 변경(시그니처만 percent로)

**변경 파일 3개, 약 15줄**:

`systemControl.h:29`
```c
// EN__BATTERY_LEVEL batteryLevel  →
int battery_percent,
```

`systemControl.c:125` (+ 251~253, 324)
```c
ST__SYSTEM_STATE systemControl(EN__LED_PATTERN   current_led_pattern,
                               ST__ERROR_CODE    mcuErrorCode,
                               ST__USB_CONNECTOR chargerState,
                               int               battery_percent,   // was EN__BATTERY_LEVEL batteryLevel
                               bool              powerButtonPushed,
                               bool              conneded_ISD,
                               bool              mappingConnected)
{
    ...
    // :251~253 대체
    if (battery_percent < 40)   // 원래 0per만 저전력 인데 40per 미만까지 저전력으로 처리(전기기계적안정성 시험, 값 유지)
    {
        veryLowBattery = true;
    }
    else
    {
        veryLowBattery = false;
    }
    ...
    // :324 대체
    if (conneded_ISD && (battery_percent < 20))
    {
        ...
    }
```

`main.c:463, 577, 621`
```c
int  batteryPercent;               // was: EN__BATTERY_LEVEL batteryLevel;
...
batteryPercent = snd_batt_get_percent();   // was: snd_batt_get_level();
...
systemState = systemControl(ledPattern, mcuErrorCode, usbConnectorState,
                            batteryPercent,   // was: batteryLevel
                            powerButtonPushed, isd_state.conneded_ISD,
                            BLE_communicationState.mappingConnection);
```

- **위험**: 매우 낮음 — §2 등가표로 동작 불변 증명됨. 호출부는 전 트리에서 `main.c:618` **1곳뿐**(`isd_interface.c`의 매치는 주석 인용일 뿐 호출 아님, grep 재확인).
- **한계**: `snd_batt_get_level()`/`EN__BATTERY_LEVEL`/고아 전역(`batteryLevelStatus` 등)은 그대로 남음 — 다만 01 census 기준으로 이들은 옵션 A 적용 즉시 **소비처 0건의 완전 dead 코드**가 된다(치우지 않고 자리만 옮기는 셈).

### 옵션 B — 전체 정리(A + enum·getter·고아 전역·유령 선언 삭제)

옵션 A에 더해 삭제:

| 대상 | 위치 | 01 census 근거 |
|---|---|---|
| `snd_batt_get_level()` 정의 | `batteryNPowerControl.c:47~88` | 유일 호출처(main.c:577)가 옵션 A로 제거됨 → 소비처 0 |
| `snd_batt_get_level()` 선언 | `batteryNPowerControl.h:63` | 상동 |
| `EN__BATTERY_LEVEL` typedef + `en__batteryPower_*` 7값 | `batteryNPowerControl.h:17~26` | 실소비처가 `systemControl.c:251~253/324` 뿐이었고 그마저 percent로 대체됨 |
| 고아 전역 `batteryLevelStatus`/`prev_batteryLevelStatus` (선언+초기화+리셋) | `batteryNPowerControl.c:221~222, 230~231` | write-only, 그 write 경로(`calculationBatteryBoundary`←`ci_battery_init`)조차 `initialize.c:362`에서 주석 처리되어 **비실행** |
| 유령 선언 `debugging_for_monitoring(...)` | `main.h:67` | 정의·호출 전 트리 0건. **enum 삭제 시 이 줄이 남아있으면 컴파일 에러** — 필수 동반 삭제 |

**주의(연쇄)**: `batteryLevelStatus`/`prev_batteryLevelStatus`의 write는 `calculationBatteryBoundary()` 내부(`:230~231`)에 있으므로, 이 두 전역만 지우려면 그 함수 내부의 해당 2줄만 제거하면 되고 함수 자체(LSAD 전압 경계 계산, `batteryBoundary`/`ST__BATTERY_BOUNDARY` 계통)는 별개의 더 넓은 dead 코드(01 census §2.6, `ci_battery_init()` 호출 자체가 주석 처리)이므로 **본 정리 범위 밖**으로 유지 가능 — 단, 은수님이 원하면 그 계통까지 한 번에 정리하는 것도 검토 가능(별도 판단 필요, 01 로그 §5.2 참고).

- **위험**: 01 census가 "systemControl() 외 실사용 0건 + BLE는 percent 그대로 사용 + 고아 전역 write 자체도 비실행"까지 확정했으므로, 옵션 A 대비 **추가 위험이 거의 없음**. 다만 `main.h:67` 삭제를 빠뜨리면 컴파일 실패하므로 체크리스트화 필요.
- **이점**: enum·getter·고아 전역이라는 "죽었지만 남아있는" 코드 3종을 실제로 제거 — 향후 독자가 `EN__BATTERY_LEVEL`을 보고 "어딘가 쓰이나?"를 다시 조사하는 비용을 없앰. 요구사항의 "사용하지 않을 시 어떻게 개선하면 되는지" 질문에 가장 직접적으로 답하는 옵션.

### 4.3 트레이드오프 요약

| | 옵션 A | 옵션 B |
|---|---|---|
| Diff 규모 | 3파일, ~15줄 | 6파일(A의 3 + batteryNPowerControl.c/.h, main.h), ~70줄(대부분 삭제) |
| 위험 | 매우 낮음(등가 증명됨) | 낮음(01 census로 타 소비처 배제 확정) — 단 `main.h:67` 동반 삭제 필수 |
| 정리 완결성 | 낮음 — enum 등 3종이 완전 dead로 남음 | 높음 — dead 코드 실제 제거 |
| 되돌리기 용이성 | 매우 쉬움(순수 시그니처 치환) | A보다는 조금 더 큼(삭제라 git revert는 가능하나 diff가 더 넓음) |

**추천**: 01 census가 이미 "다른 소비처 없음"을 실측(0건 grep)으로 확정했고, 고아 전역의 write 경로조차 비실행임이 밝혀졌으므로, **A만 먼저 하고 B를 나중에 미루는 것의 실익이 적다** — 같은 PR에서 A+B를 함께 적용하는 것을 추천. 다만 최종 삭제 여부·범위·머지는 요구사항 §6에 따라 은수님 승인 사항.

## 5. 보존/주의 사항

### 5.1 파라미터명

기존 시그니처는 `current_led_pattern`(snake), `mcuErrorCode`/`chargerState`/`batteryLevel`/`powerButtonPushed`/`mappingConnected`(camel), `conneded_ISD`(snake, 오타 포함) 이 혼재된 스타일이다. `tdc_` 접두는 불필요(요구사항 명시)하고, 형제 API인 `snd_batt_set_percent(int percent)`(`batteryNPowerControl.c:42`)·`tdc_led_request_battery(int pct, ...)`(`main.c:360`)가 이미 "percent/pct"를 파라미터명으로 쓰는 선례가 있으므로 **`int battery_percent`**(시그니처) / 호출부 지역변수는 **`batteryPercent`**(main.c 기존 camelCase 지역변수 관례 유지, 또는 `battery_percent`로 통일 — 어느 쪽이든 가능, 필요 시 리뷰어 취향에 맞춤)를 추천. 핵심은 `batteryLevel`이라는 이름을 유지한 채 타입만 int로 바꾸면 "레벨(enum)"이라는 의미가 코드에 남아 오해를 유발하므로, **이름도 percent로 바꾸는 것을 권장**(요구사항 §5.1 범위 내).

### 5.2 RESET(percent=0) 저전력 오판정 위험 — systemControl()에는 해당 없음 (검증됨)

`main.c`의 배터리 LED 판정(`tdc_led_request_battery`, `main.c:360~389`)은 `batt_is_reset_state`(= `snd_batt_get_state() == EN__SND_BATT_STATE_RESET`)를 **명시적으로 최우선 분기**하여 부팅 초기 percent=0(RESET, 0x34 미수신)이 CRITICAL로 새는 것을 막는다(`main.c:670` 주석, Rev.5 근거) — 이는 이 LED 판정이 `chargerState`와 무관하게 **매 tick 무조건 실행**되기 때문에 필요한 가드다.

반면 `systemControl()`의 두 소비 지점(`:251~253`, `:324`)은 `chargerState.chargerConnectorPluggedIn == df_Disconnected` 분기(`:202`) **내부에만** 존재한다. 부팅 후 QCC의 첫 0x34 패킷이 오기 전까지 `chargerState`는 `df_Defalut`로 고정되어 있고(`initialize.c:503` `snd_charger_set_state(EN__SND_CHARGER_STATE_RESET)` → `chargerConnectorPluggedIn = df_Defalut`), 이때는 `:157~160` 분기(`systemStatus.Led_Pattern = en__LED_NA;`만 수행)로 빠져 저전력/ISD 가드 블록 자체가 **아예 실행되지 않는다**(`main.c:613~616` 주석과 일치). 그리고 최초 0x34 패킷 처리(`ble_communication.c:130` `snd_batt_set_percent()` → `:135~152` `snd_charger_set_state()`)는 **같은 함수 호출 내에서 percent와 charger 상태를 함께 갱신**하므로, `chargerState`가 `df_Disconnected`로 바뀌는 시점에는 이미 실제 배터리 percent도 갱신되어 있다 — RESET 기본값 0과 "충전기 미연결 확정" 상태가 동시에 성립하는 창(window)이 구조적으로 없다.

**결론**: percent=0이 실측 0%인지 RESET-기본값인지 모호한 문제는 `systemControl()`의 저전력/ISD 가드에는 **애초에 적용되지 않는다**(enum 방식이든 percent 방식이든 동일) — 이는 LED 판정 코드와 달리 `chargerState` 게이팅이 자연 방벽 역할을 하기 때문이다. 따라서 percent 직접 비교로 바꿔도 **신규 위험 없음**, 그리고 **기존에도 위험이 없었던** 지점이다(LED 코드에 있던 것과 혼동 주의).

### 5.3 호출부 영향 범위

`systemControl(` 전 트리 grep 결과 매치 파일은 `main.c`, `systemControl.c`(정의), `systemControl.h`(선언), `isd_interface.c` 4개이나 `isd_interface.c:258`은 `"main 함수의 systemControl() 함수에서..."`라는 **주석**일 뿐 실제 호출이 아니다(확인됨). 따라서 시그니처 변경의 실제 영향 범위는:
- 정의: `systemControl.c:122~128`
- 선언: `systemControl.h:26~32`
- 호출: `main.c:618~625` (인자 전달, 4번째 인자)
- 호출 인자 준비: `main.c:463`(지역변수 선언), `main.c:577`(대입)

**딱 1개 호출부**(main.c) 뿐이며, 다른 어떤 파일도 이 시그니처 변경으로 컴파일 영향을 받지 않는다(옵션 B에서 `main.h:67`은 시그니처 변경이 아니라 enum 타입 자체 삭제로 인한 영향이므로 별개 항목).

## 자체 검토

### 지침 준수 여부
| 항목 | 확인 |
|---|---|
| 읽기 전용, `src/` 미수정 | 준수(Read/Grep만 사용, 코드 변경 없음) |
| `파일:라인` 근거 표기 | 전 항목 표기 |
| 등가표(경계 포함/배제 명시) | §2.1, §2.2 — 40/20 양쪽 모두 표로 검증 |
| 옵션 A/B 트레이드오프 명시 | §4.3 |

### 논리적 정합성
| 점검 | 결과 |
|---|---|
| percent<40/percent<20 대체가 :251~253/:324와 100% 동치인가(질문_3) | ✅ §2 — 발산 0건, 40/20 경계 불포함 양쪽 동일 |
| 저전력/ISD 가드 부수효과(질문_2 성격, 실제로는 배경 이해) 파악 | ✅ §3 — 강제 전원종료(POWER_OFF LED→PMIC off+systemOff)·10분 주기 자극 알림 확인 |
| RESET(percent=0) 오판정 위험 | ✅ §5.2 — systemControl()은 chargerState 게이팅으로 구조적 면제, LED 코드와 다름을 구분 |
| 호출부 1곳 확인 | ✅ §5.3 — isd_interface.c는 주석, 실제 호출 아님 |
| 01 census와 교차 정합 | ✅ §4.2 — 삭제 대상 표에 01 로그 근거 인용, 모순 없음 |

### 미해결/타 페르소나 이관 사항
| 이슈 | 처리 |
|---|---|
| BLE 패킷 배터리 송신 경로가 percent 방식임(enum 무관) 확인 | 01 census §3에서 이미 확인(`ble_communication.c:100,112`), 본 노드는 그 결론을 전제로 사용. 전담 BLE 노드(02) 결과와 재교차 권장 |
| `calculationBatteryBoundary`/`batteryBoundary`/`ST__BATTERY_BOUNDARY` 계통 전체 삭제 여부 | 본 정리(고아 전역 2개)와 별개의 더 넓은 dead 코드 — 은수님 별도 판단 필요(01 로그 §5.2 참고) |
| 옵션 A/B 중 최종 선택, 계획·구현 착수 | 위임 밖(요구사항 §6) — 은수님 승인 후 |
