---
name: 01 — 사용처 census (배터리 레벨 enum)
purpose: snd_batt_get_level()/EN__BATTERY_LEVEL/en__batteryPower_*/batteryLevelStatus 등 배터리 레벨 enum 관련 심볼의 전 트리 사용처 census, 실사용/전달/dead 분류
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: experimental
tags: [main, systemControl, battery, EN__BATTERY_LEVEL, snd_batt_get_level, census, dead-code]
persona: 01-usage-census-analyst
---

# 01. 사용처 census — `EN__BATTERY_LEVEL` / `snd_batt_get_level()` 계열

**TL;DR**: `snd_batt_get_level()`의 호출처는 전 트리에서 `main.c:577` **단 1곳**이다(grep으로 0건 추가 확인). `EN__BATTERY_LEVEL` 값을 실제로 분기·계산에 소비하는 코드는 `systemControl.c:251~253`(<40%)·`:324`(<20%) **2곳뿐**이며, main은 조회 후 그대로 전달만 한다(전달-only). `batteryLevelStatus`/`prev_batteryLevelStatus`/`batteryBoundary`는 write-only 고아이며, 그 유일한 쓰기 함수 `calculationBatteryBoundary()`의 유일한 호출자 `ci_battery_init()`조차 `initialize.c:362`에서 **주석 처리**되어 있어 현재 빌드에서는 그 쓰기조차 실행되지 않는다(이중 dead). 은수님 가설("systemControl 외 사용처 없음")은 **참**으로 확정.

## 1. 조사 방법

전 트리(`E:\workspace\projects\sound1-fw-e8300`, `src/`+`docs/` 포함) 대상으로 심볼별 `Grep` 전수 검색을 수행하고, 매치마다 파일을 직접 `Read`하여 정의/선언/실사용 소비/전달만/dead로 분류했다. `docs/` 매치는 선행 작업 로그(주로 `20260714_battery-led-refactor`)의 인용이므로 census 표에서는 `src/` 매치만 근거로 채택했다.

## 2. 심볼별 census 표

### 2.1 `snd_batt_get_level()`

| 파일:라인 | 분류 | 비고 |
|---|---|---|
| `batteryNPowerControl.c:47~88` | 정의 | `snd_batt_get_percent()`를 20%p 그리드로 7단계 매핑. `static`/이력 변수 없음(margin-free 순수 함수) |
| `batteryNPowerControl.h:63` | 선언 | 프로토타입 |
| `main.c:577` | **유일 호출처(실사용)** | `batteryLevel = snd_batt_get_level();` |

**호출자 0건 추가 확인**: `snd_batt_get_level` 전 트리 grep 결과, `src/` 내 매치는 정의(.c:47)·선언(.h:63)·호출(main.c:577) **3건**뿐이다. `docs/` 내 매치는 전부 이전 작업(`20260714_battery-led-refactor`, `20260709_func-normal-refactor`) 분석 로그의 인용이지 실제 코드가 아니다. → **다른 호출 0건, main.c:577 유일 확정**.

### 2.2 `EN__BATTERY_LEVEL` (타입)

| 파일:라인 | 분류 | 비고 |
|---|---|---|
| `batteryNPowerControl.h:17~26` | 정의(typedef enum) | 7값: `en__batteryPower_0per/0btw20/20btw40/40btw60/60btw80/80btw100/100per` |
| `batteryNPowerControl.h:63` | 선언(반환형) | `snd_batt_get_level()` 프로토타입 |
| `batteryNPowerControl.c:47` | 정의(반환형) | 함수 정의부 |
| `batteryNPowerControl.c:221` | **전역 변수 선언(타입 사용)** | `volatile EN__BATTERY_LEVEL batteryLevelStatus = en__batteryPower_100per;` — write-only(§3) |
| `batteryNPowerControl.c:222` | **전역 변수 선언(타입 사용)** | `volatile EN__BATTERY_LEVEL prev_batteryLevelStatus = ...` — write-only(§3) |
| `main.c:463` | 지역 변수 선언(타입 사용) | `func_normal()` 내 `batteryLevel` 선언 |
| `main.h:67` | **함수 선언(타입 사용) — dead** | `debugging_for_monitoring(...)` 파라미터 타입. 이 함수는 **정의도 호출도 전 트리에 0건**(§4) — 순수 vestigial 선언 |
| `systemControl.c:125` | 함수 파라미터(타입 사용) | `systemControl()` 정의부 시그니처 |
| `systemControl.h:29` | 함수 파라미터(타입 사용) | `systemControl()` 선언부 시그니처 |

**실소비처(값을 읽어 분기)는 systemControl.c 2곳뿐** — §2.3에서 근거 제시. 그 외 모든 등장은 "타입으로서 선언/전달"이며 값 자체를 읽어 판단하는 곳이 아니다.

### 2.3 `en__batteryPower_*` (7개 열거값) — 실소비처 정밀 확정

| 파일:라인 | 분류 | 비고 |
|---|---|---|
| `batteryNPowerControl.h:19~25` | 정의 | 값 목록 |
| `batteryNPowerControl.c:56,61,66,71,76,81,86` | **생산(return)** | `snd_batt_get_level()` 내부, percent→enum 매핑 결과 반환 |
| `batteryNPowerControl.c:221~222` | 초기값 대입(write) | 전역 초기화 |
| `batteryNPowerControl.c:230~231` | **write-only** | `calculationBatteryBoundary()` 진입 시 두 전역을 `en__batteryPower_100per`로 리셋 — 그 이후 아무도 읽지 않음(§3) |
| `systemControl.c:251` | **실소비(비교/분기)** | `batteryLevel == en__batteryPower_0per` |
| `systemControl.c:252` | **실소비(비교/분기)** | `batteryLevel == en__batteryPower_0btw20` |
| `systemControl.c:253` | **실소비(비교/분기)** | `batteryLevel == en__batteryPower_20btw40` — 251~253 OR 결합 → `veryLowBattery`(<40% 저전력) |
| `systemControl.c:324` | **실소비(비교/분기)** | `batteryLevel <= en__batteryPower_0btw20` (enum 순서값 비교, <20% 상당) → `conneded_ISD` 시 10분 주기 저배터리 자극 알림(`stimulationTrigger`) 트리거 |

**결론**: `en__batteryPower_*` 값을 읽어 실제로 분기·계산하는 곳은 전 트리에서 **`systemControl.c:251, 252, 253, 324` 4줄(논리적으로 2개 판정: <40%, <20%)뿐**이다. `batteryNPowerControl.c:56~86`은 "생산"(반환)이지 "소비"가 아니며, `:221/222/230/231`은 아무도 읽지 않는 write-only다.

### 2.4 `batteryLevel` (변수/파라미터명)

| 파일:라인 | 분류 | 비고 |
|---|---|---|
| `main.c:463` | 선언 | `func_normal()` 지역변수 |
| `main.c:577` | write | `snd_batt_get_level()` 결과 대입 |
| `main.c:621` | **전달만(pass-through)** | `systemControl()` 4번째 인자로 전달 — main.c 자신은 이 값을 한 번도 비교·분기하지 않음 |
| `systemControl.c:125` / `systemControl.h:29` | 파라미터 선언 | — |
| `systemControl.c:251~253, 324` | **실사용(비교)** | §2.3과 동일 지점 |
| `main.h:67` | 파라미터명(dead 선언 내) | `debugging_for_monitoring()` — §4에서 dead 확정 |

main.c 내에서 `batteryLevel`을 사용하는 문장은 선언·대입(577)·전달(621) 3곳뿐이며, **비교·분기는 0건**이다. LED 배터리 표시 블록(main.c:673, 676 부근)은 별도로 `snd_batt_get_percent()`를 직접 재조회해 사용하므로 `batteryLevel`(enum)과는 무관하다.

### 2.5 `batteryLevelStatus` / `prev_batteryLevelStatus` — 고아 사슬 확정

| 파일:라인 | 분류 | 비고 |
|---|---|---|
| `batteryNPowerControl.c:221` | 정의(전역, 초기화) | `= en__batteryPower_100per` |
| `batteryNPowerControl.c:222` | 정의(전역, 초기화) | `= en__batteryPower_100per` |
| `batteryNPowerControl.c:230` | write | `calculationBatteryBoundary()` 내부, 매 호출 100per로 리셋 |
| `batteryNPowerControl.c:231` | write | 상동 |
| (전 트리 어디에도 없음) | **read 0건** | 두 변수 모두 대입 이후 어디서도 읽히지 않음(grep 재확인: 매치는 위 4줄뿐) |

**write 자체도 현재 미실행**: 이 두 변수를 쓰는 유일한 함수 `calculationBatteryBoundary()`(`batteryNPowerControl.c:224~326`)의 유일한 호출자는 `ci_battery_init()`(`ci_battery.c:59`)이고, 그 `ci_battery_init()`의 유일한 호출자는 `initialize.c:362`에서 **`// ci_battery_init();  // 배터리 측정을 위한 초기화`로 주석 처리**되어 있다. 즉 현재 빌드 경로에서는:

```
initialize.c:362  // ci_battery_init();  ← 주석 처리(비실행)
      └─(비실행)→ ci_battery.c:59 calculationBatteryBoundary()
                        └─(비실행)→ batteryLevelStatus/prev_batteryLevelStatus/batteryBoundary write
```

`batteryLevelStatus`/`prev_batteryLevelStatus`는 **write-only일 뿐 아니라 그 write조차 호출 사슬 최상단에서 주석 처리되어 실행되지 않는 이중 dead 상태**다. 요구사항.md가 명시한 "직전 작업(`updateBatteryLevel` 삭제)으로 write-only 고아가 됨" 가설과 정합하며, 실측으로 한 단계 더 강한 결론(호출 자체가 비활성)을 확인했다.

### 2.6 `calculationBatteryBoundary` / `batteryBoundary`

| 파일:라인 | 분류 | 비고 |
|---|---|---|
| `batteryNPowerControl.h:34` | 선언 | — |
| `batteryNPowerControl.c:224~326` | 정의 | LSAD 보정값 기반 충전/방전별 전압 경계 12개 필드 계산(`ci_printv` 디버그 로그 12줄 포함) |
| `ci_battery.c:59` | 호출(유일) | `ci_battery_init()` 내부 |
| `initialize.c:362` | **호출 자체가 주석 처리** | `// ci_battery_init();` |
| `batteryNPowerControl.c:217` | `batteryBoundary` 전역 정의 | `ST__SYSTEM_BATTERY_BOUNDARY batteryBoundary;` |
| `batteryNPowerControl.c:244~325` (12줄) | write-only | 각 경계값 대입 |
| (전 트리 어디에도 없음) | **read 0건** | `batteryBoundary.` 전 트리 grep 매치가 이 파일 내부(정의+write 12건)뿐 — 다른 파일에서 참조 0건 |

`ST__BATTERY_BOUNDARY`/`ST__SYSTEM_BATTERY_BOUNDARY` 구조체 타입도 `batteryNPowerControl.c` 내부(:199~215)에서만 쓰인다. 이 전체 계통(LSAD 자체 전압 측정 기반 경계 계산)은 QCC가 percent를 직접 공급하는 현 구조(`snd_batt_set_percent()` via BLE 0x34) 이전의 legacy 경로로, `updateBatteryLevel()`(이미 삭제됨, 이력: `20260714_battery-led-refactor`)이 소비하던 것이었다. 그 소비자가 사라진 뒤 생산자(`calculationBatteryBoundary`)와 그 호출(`ci_battery_init`)까지 이미 주석 처리되어 있어, 이 계통 전체가 **호출조차 실행되지 않는 완전한 dead 사슬**이다.

### 2.7 참고 — 혼동 주의(별개 심볼)

- `cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3` (`cfx_cm3_sharedMemory.h:151` 등): 이름은 유사하나 **완전히 별개의 legacy CFX 공유메모리 int 필드**(LSAD raw 샘플 차, `EN__BATTERY_LEVEL`과 무관). `shared_memory.h:141`에 "CM3에서 제어한 이후로 미사용됨" 주석 존재 — 이 역시 별도 dead 후보이나 본 census 범위(`EN__BATTERY_LEVEL` 계열) 밖이라 결론에서 제외.
- `snd_batt_get_percent()`/`snd_batt_get_state()`: `EN__BATTERY_LEVEL`이 아닌 `int`/`EN__SND_BATT_STATE`를 다루는 **별도 활성 API**. 아래 §3(BLE)에서 다루듯 이쪽이 실제로 많이 쓰인다 — `EN__BATTERY_LEVEL`이 죽어 있다고 해서 배터리 정보 전체가 죽은 게 아님을 분명히 한다.

## 3. `snd_batt_get_percent()` / `snd_batt_get_state()` 실사용처 (대조군 — enum과 무관하되 정밀 확인 요청 반영)

전 트리 census 정확도를 위해 형제 API도 함께 표로 남긴다(둘 다 **활성**, enum 아님):

| 심볼 | 파일:라인 | 분류 |
|---|---|---|
| `snd_batt_get_percent()` | `batteryNPowerControl.c:37~40` | 정의 |
| | `batteryNPowerControl.c:51` | 실사용(내부, `snd_batt_get_level()` 매핑용) |
| | `main.c:673, 676` | 실사용(LED 배터리 히스테리시스, percent 직접 비교) |
| | `ble_communication.c:100` | **실사용 — BLE 0x33 응답 송신값**(`Tx_dataBuff`에 raw percent 그대로 적재, `:112`) |
| | `tdc_ui_command.c:460` | 실사용(UI 디버그 출력) |
| `snd_batt_get_state()` | `batteryNPowerControl.c:27~30` | 정의 |
| | `main.c:600, 678` | 실사용(RESET 상태 게이트) |
| | `ble_communication.c:98` | 실사용(0x33 응답 시 RESET 여부로 0xFF 대체 판단) |

**BLE 관련 핵심 확인(census 부수 소득)**: E8300→앱 배터리 송신 경로(`ble_communication.c:91~117`, CMD `0x33`)는 `snd_batt_get_percent()`가 반환하는 **raw int percent를 그대로** `Tx_dataBuff`에 적재해 보낸다(`:100, 112`). `EN__BATTERY_LEVEL`/`en__batteryPower_*`는 이 송신 경로 어디에도 등장하지 않는다 — BLE 패킷은 7단계 enum 형태가 아니라 percent 형태로 나간다.

## 4. `debugging_for_monitoring` — 부수 발견 (dead 선언)

`main.h:67`에 `EN__BATTERY_LEVEL batteryLevel`을 파라미터로 받는 `debugging_for_monitoring(...)` 함수 선언이 있으나, 전 트리 grep 결과 **정의도 호출도 0건**이다(매치가 `main.h:67` 단 1건). 링크 시점에 실제로 존재하지 않는 함수를 선언만 해둔 상태로, `EN__BATTERY_LEVEL` 관점에서는 "타입을 사용하는 선언"이지만 함수 자체가 유령이라 실사용 판정에서 완전히 배제해야 한다. enum 제거 시 이 선언도 정리 대상에 포함해야 한다(§5).

## 5. 결론

### 5.1 가설 검증

> 은수님 가설: "`EN__BATTERY_LEVEL`은 `systemControl()` 외에는 사용되는 곳이 없다."

**참(확정)**. 전 트리 실측 근거:
1. `snd_batt_get_level()` 호출처 = `main.c:577` 유일(0건 추가 발견).
2. `EN__BATTERY_LEVEL`/`en__batteryPower_*` 값을 읽어 분기하는 곳 = `systemControl.c:251~253`(<40%)·`:324`(<20%) 뿐. main은 조회(577)→전달(621)만 하고 자신은 비교하지 않는다.
3. `batteryLevelStatus`/`prev_batteryLevelStatus`/`batteryBoundary`는 write-only이며, 그 write 경로(`calculationBatteryBoundary`←`ci_battery_init`←`initialize.c:362`)조차 주석 처리되어 실행되지 않는다 — 완전 dead.
4. `main.h:67`의 `debugging_for_monitoring` 선언은 정의·호출 모두 없는 유령 함수 — dead 선언.
5. BLE 송신(`ble_communication.c` 0x33)은 `EN__BATTERY_LEVEL`이 아닌 raw percent(`snd_batt_get_percent()`)를 사용 — enum은 BLE 경로에도 관여하지 않는다.

### 5.2 enum 제거 시 손대야 할 심볼·파일 목록 (범위 참고용 — 계획은 은수님 승인 후)

| 구분 | 심볼/코드 | 파일 |
|---|---|---|
| 삭제 후보(함수) | `snd_batt_get_level()` | `batteryNPowerControl.c:47~88`, `batteryNPowerControl.h:63` |
| 삭제 후보(타입) | `EN__BATTERY_LEVEL`, `en__batteryPower_*` | `batteryNPowerControl.h:17~26` |
| 삭제 후보(dead 전역) | `batteryLevelStatus`, `prev_batteryLevelStatus` | `batteryNPowerControl.c:221~222, 230~231` |
| 삭제 후보(dead 전역+함수+호출) | `batteryBoundary`, `ST__BATTERY_BOUNDARY`, `ST__SYSTEM_BATTERY_BOUNDARY`, `calculationBatteryBoundary()` | `batteryNPowerControl.c:199~326`, `.h:34`, `ci_battery.c:28~60`(`ci_battery_init` 자체는 CI_LASD 초기화 등 다른 책임도 있어 함수 전체 삭제는 별도 판단 필요), `initialize.c:362`(이미 주석) |
| 삭제 후보(dead 선언) | `debugging_for_monitoring(...)` | `main.h:67` |
| 시그니처 변경 필요 | `systemControl()` 파라미터 `EN__BATTERY_LEVEL batteryLevel` → `int` percent (질문_3·4는 다른 페르소나 담당) | `systemControl.c:122~128, 251~253, 324`, `systemControl.h:26~32` |
| 호출부 변경 필요 | `batteryLevel = snd_batt_get_level();` → `snd_batt_get_percent()` 전달 | `main.c:463, 577, 621` |

## 자체 검토

### 지침 준수 여부
| 항목 | 확인 |
|---|---|
| 읽기 전용, `src/` 미수정 | 준수(Read/Grep만 사용) |
| `파일:라인` 근거 표기 | 전 항목 준수 |
| 전 트리 grep(0건 입증 포함) | `snd_batt_get_level`, `batteryLevelStatus` 등 read-0건 주장에 grep 재확인 근거 첨부 |

### 논리적 정합성
| 점검 | 결과 |
|---|---|
| 요구사항 질문_1(전 트리 사용처) 대응 | §2 전체 |
| snd_batt_get_level 유일 호출처 확정(질문 명시 요구) | §2.1 |
| EN__BATTERY_LEVEL 실소비처 확정(systemControl.c:251~253/324 외 여부) | §2.3 — 그 외 0건 확인 |
| 고아 사슬(batteryLevelStatus/calculationBatteryBoundary/batteryBoundary) | §2.5~2.6 — write 자체도 비실행인 것까지 확인(요구사항 대비 추가 발견) |
| 가설 판정 | §5.1 참(확정) |

### 미해결/타 페르소나 이관 사항
| 이슈 | 처리 |
|---|---|
| BLE 패킷 배터리 전송 심층 분석(수신 0x34/응답 0x33 전체 흐름, 앱-QCC 프로토콜 스펙) | 이 로그 §3에서 부수 확인(percent 형태 확정)만 하고, 전담 페르소나(BLE 패킷 조사) 결과와 교차검증 필요 |
| `systemControl()` enum→percent 대체 시 경계값 등가성(20/40 포함·배제) | 범위 외(다른 페르소나: percent 대체 설계) |
| `debugging_for_monitoring` 삭제가 `main.h` 다른 선언에 미치는 영향 | 계획 단계에서 확인 필요(본 census에서는 dead임만 확정) |
