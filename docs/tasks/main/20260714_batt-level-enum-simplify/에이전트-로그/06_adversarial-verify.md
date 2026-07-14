---
name: 06 — 반증 검증 (배터리 레벨 enum 단순화)
purpose: 01 census · 02 BLE 조사 · 03 재설계 분석 3건을 회의적으로 재검증 — 인용 진위·percent 등가성·소비처 누락·컴파일 의존성·고아 전역 삭제 안전성·부수효과 보존을 실제 코드 대조로 반증 시도
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: experimental
tags: [main, systemControl, battery, EN__BATTERY_LEVEL, adversarial-verify, cross-check]
persona: 06-adversarial-verify
---

# 06. 반증 검증 — 배터리 레벨 enum 단순화 분석 (01~03 대상)

**TL;DR**: 01·02·03이 인용한 `파일:라인`·값·로직을 전부 실제 코드(`systemControl.c`, `batteryNPowerControl.c/.h`, `main.c/.h`, `ble_communication.c`, `remoteControl.c`, `mappingControl.c`, `isd_interface_mapping_Live.c`, `ci_battery.c`, `initialize.c`)와 대조했다. **할루시네이션(사실과 다른 인용) 0건**. percent 등가성(`251~253≡<40`, `324≡<20`)은 전 구간(음수·0·경계값·100 초과 포함) 재계산으로 **확정(confirmed)** — 반증 시도 실패, 발산 없음. `EN__BATTERY_LEVEL`/`en__batteryPower_*`/`snd_batt_get_level` 소비처는 전 트리(src 전체, docs 제외) 독립 grep 결과 **정확히 6개 파일**(`systemControl.c/.h`, `batteryNPowerControl.c/.h`, `main.c/.h`)로 01의 census와 100% 일치 — 누락 소비처 0건. 컴파일 의존성(타입 참조 선언) 총 **10곳**을 직접 열거·확인했고 01·03이 이미 전부 포착했다(신규 발견 없음). 고아 전역(`batteryLevelStatus`/`prev_batteryLevelStatus`) read 0건, 그 write 경로(`calculationBatteryBoundary`←`ci_battery_init`←`initialize.c:362` 주석)도 실측 재확인했다. 부수효과(강제 전원종료·10분 주기 자극알림, `df_lowbatteryIndicationPeriod_ms=600000`) 보존 주장도 실제 로직과 일치. **경미한 지적 2건**(하할루시네이션 아님, 정확도 개선 여지)만 발견 — §7 참조. 전반적으로 01~03은 **매우 신뢰도 높은 실측 기반 문서**로 판정한다.

## 1. 검증 방법

- 01/02/03/요구사항.md 전문 Read
- 교차 소스 전부 Read: `systemControl.c`(전체, 411줄) / `.h`, `batteryNPowerControl.c`(전체, 483줄) / `.h`, `main.c`(440~690 구간), `main.h`(전체), `ble_communication.c`(80~250 구간), `remoteControl.c`(955~995 구간), `mappingControl.c`(2050~2095 구간), `isd_interface_mapping_Live.c`(430~475 구간), `ci_battery.c`(전체), `initialize.c`(360~510 구간)
- 독립 Grep(01/02가 쓴 명령을 그대로 신뢰하지 않고 이 노드가 직접 재실행): `EN__BATTERY_LEVEL`, `en__batteryPower_`, `snd_batt_get_level`, `debugging_for_monitoring`, `batteryLevelStatus`, `calculationBatteryBoundary`, `ci_battery_init`, `readBatteryLevel_FromCFX`, `batteryLevel_CfX_to_CM3`, `readBatteryPercentage`, `systemControl\(`, `snd_charger_set_state\(`, `readBatteryLevel\b`
- 범위: `src/` 전체(전 폴더) + 프로젝트 루트(docs 제외) — 01/02가 검색한 하위 폴더보다 넓게 잡아 "다른 폴더 누락" 가능성을 직접 배제

## 2. 인용 진위 검증 (파일:라인·값 대조)

| 인용처 | 인용 내용 | 실제 코드 대조 | 판정 |
|---|---|---|---|
| 01 §2.1 | `snd_batt_get_level()` 정의 `batteryNPowerControl.c:47~88` | 실제 47행 함수 시작, 88행 종료(닫는 중괄호) — 정확 | confirmed |
| 01 §2.2 | `EN__BATTERY_LEVEL` typedef `batteryNPowerControl.h:17~26` | 실제 17행 `typedef enum`, 26행 `} EN__BATTERY_LEVEL;` — 정확 | confirmed |
| 01 §2.2 | 고아 전역 `batteryNPowerControl.c:221~222` | 실제 221~222행 `volatile EN__BATTERY_LEVEL batteryLevelStatus/prev_batteryLevelStatus = en__batteryPower_100per;` — 정확 | confirmed |
| 01 §2.2 | `main.h:67` `debugging_for_monitoring` 선언, dead | 실제 67행 정확히 일치, 전 트리 grep 재확인 결과 정의·호출 0건(§4에서 재확인) | confirmed |
| 01 §2.3 | `systemControl.c:251,252,253` 3줄 OR, `:324` | 실제 251~253행 정확히 일치(`en__batteryPower_0per`/`0btw20`/`20btw40`), 324행 `batteryLevel <= en__batteryPower_0btw20` 정확 | confirmed |
| 01 §2.5 | `calculationBatteryBoundary()`의 write `.c:230~231`, 유일 호출자 `ci_battery_init()`, 그 호출이 `initialize.c:362`에서 주석 처리 | 실측: `ci_battery.c:59` 내부에서 호출(그 함수가 `ci_battery_init()` 본체, 28~60행), `initialize.c:362` `// ci_battery_init();` 주석 확인 | confirmed |
| 02 §2 | `ble_communication.c:118~165`, 0x34 수신, `snd_batt_set_percent(battery_level)` at `:130` | 실제 119~165행, 130행 정확히 `snd_batt_set_percent(battery_level);` | confirmed |
| 02 §3.1 | `ble_communication.c:90~117`, 0x33 응답, `batt_percent=snd_batt_get_percent()` at `:100`, `Tx_dataBuff[...]=batt_percent` at `:112` | 실제 91행 `else if`, 100행, 112행 정확 일치 | confirmed |
| 02 §3.2 | `remoteControl.c:964~989`, 0x43, `readBatteryPercentage()` at `:966`, 클램프 `:968~975` | 실제 964~989행 정확 일치(case 964, value=966, 클램프 968~975) | confirmed |
| 02 §3.3 | `mappingControl.c:2060~2092`, 0x67, `readBatteryPercentage()` at `:2076` | 실제 2060~2092행 정확 일치, 2076행 정확 | confirmed |
| 02 §3.4 | `isd_interface_mapping_Live.c:438~471`, 죽은 주석 `readBatteryLevel()` at `:459`(2번째 인용에선 "459" 명시), `readBatteryPercentage()` at `:460` | 실제 파일 경로는 `src/2__cm3/Cortex-M3-src/internalDevice/isd_interface_mapping_Live.c`(BleCommunication 폴더 아님, §7-2 참조) — 438~471행, 459행 죽은 주석, 460행 실제 호출 **줄 번호·코드 내용은 정확** | confirmed (경로 라벨만 부정확, §7-2) |
| 02 §5 | `cfx_cm3_sharedMemory.c:191~194` `readBatteryLevel_FromCFX()`, 호출처 0건 | 실제 191~195행 정확(단, 함수 내부에서 `ci_battery_update()`를 호출 — 02가 언급 안 한 세부사항이나 결론에 영향 없음, §5 참조) | confirmed |
| 03 §1.1 | 시그니처 `systemControl.h:26~32`, `systemControl.c:122~128` | 실제 정확히 일치 | confirmed |
| 03 §1.4 | `snd_batt_get_level()` 경계표(100/80~99/60~79/40~59/20~39/1~19/else) | 실제 코드 54~87행과 정확 일치(경계 비교 연산자까지 일치) | confirmed |
| 03 §5.2 | `initialize.c:503` 부팅 시 `snd_charger_set_state(EN__SND_CHARGER_STATE_RESET)`, 0x34 핸들러 내 percent·charger가 "같은 함수 호출 내에서 함께 갱신" | 실제 확인: `snd_charger_set_state` 호출처는 전 트리에서 `initialize.c:503`(RESET, 부팅)과 `ble_communication.c:138/143/149`(0x34 핸들러 내부, percent 대입(:130)과 같은 분기 블록) **2곳뿐** — "동시 갱신, 창구 없음" 주장 구조적으로 확인 | confirmed |

**인용 진위 종합**: 01/02/03이 인용한 파일:라인·코드 내용·값 중 **사실과 다른 것은 0건**. 모든 인용이 실제 코드와 정확히 일치했다.

## 3. percent 등가성 반증 시도 — 실패 (등가 확정)

`snd_batt_get_level()` 경계(실측, `batteryNPowerControl.c:54~87`):

| percent 구간 | enum |
|---|---|
| `100<=percent` | `100per` |
| `80<=percent<100` | `80btw100` |
| `60<=percent<80` | `60btw80` |
| `40<=percent<60` | `40btw60` |
| `20<=percent<40` | `20btw40` |
| `0<percent<20` | `0btw20` |
| else(`percent<=0`, 음수 포함) | `0per` |

### 3.1 `251~253` ≡ `percent<40` — 재계산 결과

`veryLowBattery = (batteryLevel==0per) || (batteryLevel==0btw20) || (batteryLevel==20btw40)`.

집합 결합: `{percent<=0} ∪ {0<percent<20} ∪ {20<=percent<40}` = `(-∞,0] ∪ (0,20) ∪ [20,40)` = `(-∞,40)` = `percent<40`. 이 세 구간은 서로 빈틈·중복 없이 이어 붙는다(0 경계, 20 경계 모두 한쪽 집합에만 속함). **경계 재계산으로 전 구간 동치 확정.**

pct=0,19,20,39,40,100 개별 대입 재확인:

| pct | enum | 251~253 (OR) | `pct<40` | 일치 |
|---|---|---|---|---|
| 0 | 0per | true | true | 일치 |
| 19 | 0btw20 | true | true | 일치 |
| 20 | 20btw40 | true | true | 일치 |
| 39 | 20btw40 | true | true | 일치 |
| 40 | 40btw60 | false | false | 일치 |
| 100 | 100per | false | false | 일치 |
| 음수 | 0per | true | true | 일치 |
| 255(raw byte 상한) | 100per | false | false | 일치 |

**발산 0건. `pct==0`(0per)이 `<40` 쪽에 포함되는지 특별히 확인** — 포함됨(0per는 `percent<=0` 구간이므로 `0<40`과 일치).

### 3.2 `324` ≡ `percent<20` — 재계산 결과

`batteryLevel <= en__batteryPower_0btw20`은 enum 선언 순서(C 표준: 명시적 값 없는 enumerator는 이전값+1)에 의한 정수 비교다. `en__batteryPower_0per=0`, `en__batteryPower_0btw20=1`(다른 값 명시 없음, `.h:19~20` 확인) → `<=1`은 `{0per, 0btw20}` = `percent<20`.

| pct | enum(순서값) | `<=1` | `pct<20` | 일치 |
|---|---|---|---|---|
| 0 | 0per(0) | true | true | 일치 |
| 19 | 0btw20(1) | true | true | 일치 |
| 20 | 20btw40(2) | false | false | 일치 |
| 39 | 20btw40(2) | false | false | 일치 |
| 40 이상 | 그 이상(3~6) | false | false | 일치 |

**발산 0건. `pct==0`이 `<20` 쪽에도 정확히 포함됨을 재확인.**

### 3.3 반증 시도 결론

두 등가식 모두 **경계값·음수·100 초과값을 포함한 전 구간에서 반례를 찾지 못했다** — 03의 등가 주장은 **confirmed**. enum 자체가 20%p 그리드로 설계되어 있고 `systemControl()`의 두 판정이 정확히 그 그리드의 부분합/순서비교이기 때문에 우연이 아닌 구조적 동형(isomorphic) 관계라는 03의 설명도 코드 구조(§1.4 경계표)로 뒷받침된다.

## 4. 소비처 누락 반증 시도 — 실패 (누락 없음 확정)

01·02의 "systemControl 외 소비처 없음", "BLE enum 관여 0"을 깨기 위해 **이 노드가 직접, 더 넓은 범위로** grep을 재실행했다(01/02의 grep 결과를 신뢰하지 않고 독립 재현):

```
grep "EN__BATTERY_LEVEL|en__batteryPower_|snd_batt_get_level" src/**  → 6개 파일
  systemControl.c, systemControl.h,
  batteryNPowerControl.c, batteryNPowerControl.h,
  main.c, main.h
grep 동일 패턴, 프로젝트 루트 전체(docs 제외) → 동일 6개 파일 (추가 0건)
```

`0__bootloader`, `1__cfx`, `3__hear`, `4__eeprom`, `5__calibration`, `tests/` 등 01/02가 명시적으로 훑지 않은 폴더까지 포함한 전수 검색에서도 **추가 소비처 0건**. 01의 census 표(§2.2~§2.3)와 **완전히 일치**하며, 누락을 발견하지 못했다 — 반증 시도 실패.

BLE 쪽도 마찬가지로 `ble_communication.c/.h`, `remoteControl.c/.h`, `mappingControl.c/.h`, `isd_interface_mapping_Live.c`(경로는 `internalDevice/`, §7-2) 전부 재확인했고 `EN__BATTERY_LEVEL`/`en__batteryPower_*`/`snd_batt_get_level` 매치 0건, `readBatteryPercentage`/`snd_batt_get_percent` 경유 percent 전송 4경로(0x33/0x43/0x67/0x66-서브7)만 확인 — 02의 결론과 100% 일치.

## 5. 컴파일 의존성 전수 — 타입 참조 선언 10곳

`EN__BATTERY_LEVEL`을 파라미터·반환·변수 타입으로 참조하는 **모든** 선언(enum 삭제 시 반드시 수정/삭제해야 컴파일 성공)을 이 노드가 직접 grep+Read로 재열거했다:

| # | 위치 | 종류 | enum 삭제 시 필요 조치 |
|---|---|---|---|
| 1 | `batteryNPowerControl.h:17~26` | typedef 정의 자체 | 삭제 |
| 2 | `batteryNPowerControl.h:63` | `snd_batt_get_level()` 선언(반환형) | 삭제(함수 자체 제거) |
| 3 | `batteryNPowerControl.c:47` | `snd_batt_get_level()` 정의(반환형) | 삭제 |
| 4 | `batteryNPowerControl.c:221` | `batteryLevelStatus` 전역(타입) | 삭제 또는 타입 변경 |
| 5 | `batteryNPowerControl.c:222` | `prev_batteryLevelStatus` 전역(타입) | 삭제 또는 타입 변경 |
| 6 | `batteryNPowerControl.c:230~231` | `calculationBatteryBoundary()` 내부, 위 두 전역에 대입(값 자체는 `en__batteryPower_100per`) | 전역 삭제 시 이 2줄도 반드시 제거(안 하면 컴파일 에러) |
| 7 | `main.c:463` | `func_normal()` 지역변수 선언 | `int`로 변경 |
| 8 | `main.h:67` | `debugging_for_monitoring(...)` 파라미터(정의·호출 0건, 유령) | 삭제(필수 — 안 지우면 컴파일 에러) |
| 9 | `systemControl.c:125` | `systemControl()` 정의 파라미터 | `int`로 변경 |
| 10 | `systemControl.h:29` | `systemControl()` 선언 파라미터 | `int`로 변경 |

**검증 결과**: 이 노드가 독립적으로 찾은 10곳은 **01(§2.2, §5.2)과 03(§4 옵션 B 표)이 합쳐서 이미 전부 포착한 목록과 정확히 일치**한다 — 신규 발견(01·03이 놓친 선언) **0건**. `main.h:67`(#8)이 실제로 `EN__BATTERY_LEVEL batteryLevel`을 파라미터로 참조함을 코드로 재확인했고(정의·호출 grep 재확인 결과도 0건, 유령 함수 맞음), 03의 "이 선언 삭제 누락 시 컴파일 에러" 경고도 타당하다(파라미터 타입이 사라진 typedef를 참조하므로).

## 6. 고아 전역 삭제 안전성 — 재확인

- `batteryLevelStatus`/`prev_batteryLevelStatus` 전 트리 grep(이 노드 독립 실행): 매치 **4건**(`.c:221,222,230,231`) — write만 존재, **read 0건** 재확인. 01의 주장과 일치.
- `calculationBatteryBoundary()`의 유일 호출자는 `ci_battery_init()`(`ci_battery.c:28~60`, 59행에서 호출) — **함수 전체를 Read로 직접 확인**했고, LSAD 교정/샘플링 초기화 후 마지막에 `calculationBatteryBoundary()`를 호출하는 구조가 맞다.
- `ci_battery_init()`의 유일 호출처는 `initialize.c:362`이며 실제로 `// ci_battery_init();  // 배터리 측정을 위한 초기화`로 **주석 처리**되어 있음을 직접 확인 — 01의 "write 경로 자체가 비실행"이라는 강한 주장 confirmed.
- 부수 발견(01·02 미언급, 이 노드가 추가 확인): `ci_battery_update()`(별개 함수, `batteryLevel_CfX_to_CM3` int 필드에 LSAD raw 값을 쓰는 함수, `EN__BATTERY_LEVEL`과 무관)의 호출부가 `initialize.c:495`에도 있으나, 그 블록 전체가 `#if 0`(483~500행)로 **컴파일 자체에서 배제**되어 있어 이중으로 죽어있다. 또 `readBatteryLevel_FromCFX()`(`cfx_cm3_sharedMemory.c:191~195`) 내부에서도 `ci_battery_update()`를 호출하지만, 이 함수 자체가 호출처 0건(죽은 함수)이므로 영향 없음. **이 발견은 EN__BATTERY_LEVEL 삭제 안전성 판단에 영향을 주지 않는다** — 별도 계통(int 기반 CFX 레거시)이며 01·02도 이를 "무관한 별개 dead code"로 이미 분리해뒀다(01 §2.7, 02 §5).
- `batteryBoundary`/`ST__BATTERY_BOUNDARY`/`ST__SYSTEM_BATTERY_BOUNDARY`: 전 트리 grep 재확인 결과 `batteryNPowerControl.c` 내부(정의+write 12건+구조체정의)만 매치, **read 0건**(다른 파일 참조 없음) — 01의 주장 confirmed. 단, 이 계통은 `EN__BATTERY_LEVEL` 타입을 쓰지 않으므로(전부 `int` 필드) enum 삭제와 독립적인 별개 정리 대상이라는 01·03의 구분도 맞다.

## 7. 부수효과 보존 검증

`systemControl.c` 전체(122~410행)를 직접 Read하여 03의 주장을 재확인:

- **251~253 (veryLowBattery)**: true가 되면 263행 `if (veryLowBattery || powerButtonPushed)` → `isPowerOffEnabled==false`일 때 1회 `systemStatus.Led_Pattern=en__LED_POWER_Off`, `led_request(LED_SRC_POWER, LED_ST_POWER_OFF)`, `systemStatus.enable_ISD=false`, `isPowerOffEnabled=true`(277~281행) — 03의 "전원 종료 LED 패턴 시작" 정확. 이후 매 tick(287행 `if(isPowerOffEnabled)`) burst 종료 시(`!tdc_led_is_burst_pending()`) `systemStatus.enablePMIC=false`, `systemStatus.systemOff=true`(296~299행) — "실제 시스템 전원 차단" 정확. `!veryLowBattery && conneded_ISD`면 `isPowerOffEnabled=false`로 취소(305~310행) — 정확.
- **324 (ISD 저배터리 자극알림)**: `conneded_ISD && batteryLevel<=0btw20`이고 `isPowerOffEnabled==false`(316행 else 분기 내부에만 존재)일 때 `lowBatteryIndicatorCounter==0`이면 `stimulationTrigger=true`, 카운터를 `df_lowbatteryIndicationPeriod_ms`로 리셋(326~330행) — 이 상수 실측값 `600000`(ms, `definitionsForAlgorithm.h` 3개소에서 확인) = **정확히 10분**, 03의 "10분 주기" 주장 confirmed. `systemStatus.StimulationIndicatorTriggerLowPower = stimulationTrigger;`(407행, 함수 반환 직전) — 03의 "내부기에 저배터리 자극 신호 전달 근거" 설명과 일치.
- percent 대체(§3에서 confirmed된 등가) 시 이 두 트리거 조건의 참/거짓이 전 구간에서 동일하게 재현되므로, 부수효과(강제 전원종료·10분 주기 자극알림) 모두 **보존된다**는 03의 결론이 코드 실측으로 뒷받침된다.

## 8. 할루시네이션 집계

| 구분 | 건수 |
|---|---|
| 파일:라인 인용이 실제 코드와 다른 경우 | **0건** |
| 인용된 코드 내용(조건식·값·연산자)이 실제와 다른 경우 | **0건** |
| 존재하지 않는 함수/변수를 실존하는 것처럼 서술 | **0건** |
| 논리적 결론(등가성·소비처 없음·부수효과)이 실제 코드와 배치되는 경우 | **0건** |
| 경미한 부정확(할루시네이션 아님, §9 참조) | 2건 |

**최종 판정: 01·02·03에서 코드 사실에 반하는 할루시네이션은 발견되지 않았다.**

## 9. 반드시 고쳐야 할 지적 (경미)

이 2건은 오류라기보다 "설명의 정밀도" 문제이며, 계획·구현 단계에 실질적 영향은 없다. 다만 문서 정확성을 위해 계획 문서 작성 시 반영 권장.

1. **02 §3.4의 폴더 스코프 라벨링 부정확**: 02는 §1에서 조사 범위를 "`BleCommunication/` 전체"로 명시했으나, §3.4에서 분석한 `isd_interface_mapping_Live.c`는 실제로는 `src/2__cm3/Cortex-M3-src/internalDevice/` 폴더에 있다(`BleCommunication/`이 아님, 파일시스템 확인 완료 — 동명 파일은 이 경로 1개뿐). 인용된 라인·코드 내용 자체는 100% 정확하지만, "BLE 폴더 전체"라는 범위 서술과 실제 분석 파일 위치가 어긋난다. 계획 단계에서 "BLE 관련 파일" 목록을 만들 때 `internalDevice/isd_interface_mapping_Live.c`도 명시적으로 포함해야 함(내용상 이미 다뤄졌으므로 재분석은 불필요, 표기만 정정).
2. **01 §3의 `main.c:673, 676` 성격 서술이 다소 느슨함**: 01은 이 두 줄을 "실사용(LED 배터리 히스테리시스, percent 직접 비교)"로 설명했으나, 실제로는 두 줄 다 `snd_batt_get_percent()`(또는 UI 오버라이드 값)를 `pct` 지역변수에 대입하는 코드일 뿐이고, 실제 비교·히스테리시스 로직은 그 다음 줄(679행) `tdc_led_request_battery(pct, ovr_batt_active, batt_is_reset_state)` 함수 내부에서 일어난다. "percent 직접 비교"라는 표현이 673/676행 자체에서 비교가 일어나는 것으로 오독될 수 있다. `EN__BATTERY_LEVEL`과 무관한 대조군 설명이라 이번 enum 삭제 결론에는 영향 없음.

## 10. 종합 판정

| 항목 | 판정 |
|---|---|
| 01 census 전반 | **confirmed** — 인용·수치·결론 전부 실측 일치 |
| 02 BLE 조사 전반 | **confirmed** — 인용·수치·결론 전부 실측 일치(폴더 라벨 1건 경미 지적, §9-1) |
| 03 재설계 분석 전반 | **confirmed** — 등가성·부수효과·컴파일 의존성·RESET 무영향 주장 전부 실측 일치 |
| percent 등가성(<40, <20) | **confirmed** — 전 구간(경계·음수·100 초과 포함) 반례 없음 |
| 소비처 누락 여부 | **없음(confirmed)** — 독립 재grep으로도 6개 파일 외 추가 소비처 0건 |
| 컴파일 의존성(타입 참조 선언) | **10곳, 완전 포착 confirmed** — 01·03이 이미 전부 인지, 신규 누락 0건 |
| 고아 전역 삭제 안전성 | **confirmed** — read 0건, write 경로 자체가 주석 처리로 비실행 |
| 부수효과 보존 | **confirmed** — 강제 전원종료·10분 주기 자극알림 로직 실측 일치 |
| uncertain 판정 항목 | 없음 |
| refuted 판정 항목 | 없음 |

## 자체 검토

### 지침 준수 여부
| 항목 | 확인 |
|---|---|
| 읽기 전용, `src/` 미수정 | 준수 — Read/Grep/Bash(조회만) 사용, Edit/Write는 본 문서에만 |
| 01~03·요구사항.md 전문 Read | 준수 |
| 교차 검증 소스 전부 Read(코드) | 준수 — systemControl.c/.h, batteryNPowerControl.c/.h, main.c/.h, ble_communication.c, remoteControl.c, mappingControl.c, isd_interface_mapping_Live.c, ci_battery.c, initialize.c |
| 회의적 반증 우선(그냥 재확인 아님) | 준수 — 경계값 전수 재계산, 독립 grep 재실행(01/02 결과 신뢰 없이 재현), 더 넓은 범위(전 폴더) 검색으로 누락 가능성 직접 배제 |
| 판정 confirmed/refuted/uncertain 명시 | 준수 — §10 종합 판정표 |
| 할루시네이션 집계 | 준수 — §8 |

### 논리적 정합성
| 점검 | 결과 |
|---|---|
| 인용 진위(질문 1) | §2 — 0건 불일치 |
| percent 등가 반박 시도(질문 2) | §3 — 반박 실패, 등가 확정 |
| 소비처 누락 반박 시도(질문 3) | §4 — 반박 실패, 누락 없음 확정 |
| 컴파일 의존성 전수(질문 4) | §5 — 10곳, 01·03과 완전 일치 |
| 고아 전역 삭제 안전성(질문 5) | §6 — read 0건, write 경로 비실행 재확인 + 부수 발견(ci_battery_update `#if 0`) 기록 |
| 부수효과 보존(질문 6) | §7 — 강제 전원종료·10분 주기 자극알림 로직 실측 대조 |

### 미해결/후속 이관 사항
| 이슈 | 처리 |
|---|---|
| §9의 경미 지적 2건(폴더 라벨, 표현 정밀도) | 계획 문서 작성 시 반영 권장, 재분석 불필요 |
| `calculationBatteryBoundary`/`batteryBoundary`/`ST__BATTERY_BOUNDARY` 계통(EN__BATTERY_LEVEL과 무관한 별도 dead code) 전체 삭제 여부 | 이번 검증 범위 밖 — 01·03이 이미 "은수님 별도 판단 필요"로 분리해둔 사항, 이 노드도 동일 결론 |
