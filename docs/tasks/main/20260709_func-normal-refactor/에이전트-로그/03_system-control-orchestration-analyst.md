---
name: 시스템제어 오케스트레이션 분석 — func_normal 475-536행
purpose: func_normal() 리팩토링을 위한 systemControl/isd_interface/bleCommunication 호출 체인의 순서 의존성·루프 캐리 변수·분해 경계 분석
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: stable
tags: [main, func_normal, systemControl, isd_interface, bleCommunication, refactor]
---

# 시스템제어 오케스트레이션 분석 (main.c:475~536)

**TL;DR**: 담당 구역(475~536행)의 6개 호출(systemControl→update_mapNum→isd_interface→bleCommunication→stimulation_IndicatorOut→PMIC on/off, 이어서 모드 플래그 분기)은 `isd_state`·`BLE_communicationState`가 **루프 캐리 변수**(이번 iteration 앞부분은 직전 iteration 값을 그대로 사용, 후반부에 갱신)라서 서로 강하게 얽혀 있어 하나의 "서브시스템 상태 갱신" 함수로 묶어야 하고, 그 뒤에 오는 모드 플래그 분기(526~536행)만 `BLE_communicationState.mappingConnection` 단일 bool 입력만 필요해 별도 함수로 분리 가능하다. 분리 시 두 함수 모두 **호출 순서를 텍스트 그대로 유지**해야 하며, `update_mapNum()`은 데이터 의존이 없어 보여도 위치를 옮기면 CFX 공유메모리를 통해 동작이 달라진다.

## 1. 호출 체인과 순서 의존성

담당 구역의 실행 순서(A→G)와 각 호출이 실제로 소비/생산하는 값:

| 단계 | 코드 위치 | 호출 | 소비(입력) | 생산(출력) |
|---|---|---|---|---|
| A | 492~499행 | `systemState = systemControl(ledPattern, mcuErrorCode, usbConnectorState, batteryLevel, powerButtonPushed, isd_state.conneded_ISD, BLE_communicationState.mappingConnection)` | `ledPattern/mcuErrorCode/usbConnectorState/batteryLevel/powerButtonPushed`(이번 iteration 441~452행에서 갓 읽은 값) + `isd_state`·`BLE_communicationState`(**직전 iteration에서 넘어온 값**, 아직 이번 iteration 값으로 안 바뀜) | `systemState`(이번 iteration 값) |
| B | 504행 | `update_mapNum()` | 파라미터 없음. 내부적으로 `isUserSettingValueLoaded_CFX()`/`readProgramMapNum()`/`readConnected_ISD_usableMapIndex()` 등 CFX 공유메모리 read + 자체 `static prev_userSettingValueLoadedFlag` | CFX 공유메모리(`changeProgramMapNum()` 경유) — `systemState`/`isd_state`/`BLE_communicationState`와 직접 연결 없음 |
| C | 506~509행 | `isd_state = isd_interface(systemState.enable_ISD, BLE_communicationState.mappingConnection, BLE_communicationState.isdControlCommand)` | `systemState.enable_ISD`(A에서 갓 생산, fresh) + `BLE_communicationState`(**아직 D에서 안 바뀐, 직전 iteration 값**) | `isd_state`(이번 iteration 값으로 덮어씀) |
| D | 515행 | `BLE_communicationState = bleCommunication(isd_state)` | `isd_state`(C에서 갓 생산, fresh) | `BLE_communicationState`(이번 iteration 값으로 덮어씀) |
| E | 517~520행 | `stimulation_IndicatorOut(readStimulIndicator_OnOff(), systemState.StimulationIndicatorTriggerLowPower, BLE_communicationState.StimulationIndicatorTrigger)` | `systemState`(A) + `BLE_communicationState`(D, fresh) + CFX 공유메모리 read | 없음(자체 캡슐화된 static으로 자극 인디케이터 파형 제어) |
| F | 522~523행 | `OnOff_3V_PMIC_CM3_to_CFX(systemState.enablePMIC)` | `systemState`(A)만 — C/D와 무관 | CFX 공유메모리 write (주석상 1.5세대 HW에서는 사실상 no-op) |
| G | 526~536행 | `if (BLE_communicationState.mappingConnection) { changeSystemModeFlag(en__mappingMode); shareMappingProgramConnection(true); } else { changeSystemModeFlag(en__normalMode); shareMappingProgramConnection(false); }` | `BLE_communicationState.mappingConnection`(D, fresh) 단일 bool | CFX 공유메모리 write, 반환값 없음 |

의존 그래프: **A → C → D → {E, G}**, F는 A에만 의존(C/D 무관하지만 코드 순서상 F가 D 뒤에 있음 — 재배치해도 데이터상 안전하나 사이드이펙트 순서 보존 원칙상 그대로 둘 것을 권고).

> [!IMPORTANT]
> **가장 중요한 함정**: `isd_state`와 `BLE_communicationState`는 **루프 캐리 변수**다. Step A/C에서 읽는 값은 "이번 iteration에 새로 계산된 값"이 아니라 "직전 iteration의 Step C/D가 만든 값"이다. 이 두 변수가 이번 iteration 값으로 덮어써지는 시점은 각각 C(506행)·D(515행)이며, 그 이전(A·C의 소비 시점)에는 반드시 stale(직전 iteration) 값이어야 한다. 함수 분해 시 이 값을 새 함수의 **in/out 파라미터**(포인터 또는 반환 구조체)로 넘기되, 새 함수 내부에서 static으로 재선언하거나 진입 시 리셋하면 안 된다 — 호출자(`func_normal()`의 루프)가 이 값을 다음 iteration까지 그대로 보관해야 한다.
>
> 반면 `systemState`는 루프 캐리가 아니다 — Step A에서 매 iteration 전부 새로 계산되고, 다음 iteration의 systemControl() 호출 인자로 재사용되는 필드가 하나도 없다(인자로 쓰이는 건 `isd_state.conneded_ISD`·`BLE_communicationState.mappingConnection`뿐). 따라서 새 함수의 순수 출력(반환값)으로만 다뤄도 안전하다.

> [!WARNING]
> `update_mapNum()`(Step B)은 `systemState`/`isd_state`/`BLE_communicationState` 어느 것도 인자로 받지 않지만, **위치 의존성이 있다.** 주석(120~144행)에 따르면 이 함수는 `isd_interface()`가 ISD 연결을 인식해 CFX 공유메모리에 사용자 설정값을 로드한 "이후"에 그 결과(`isUserSettingValueLoaded_CFX()`)를 확인하는 용도다. 그런데 실제 실행 순서는 B(504행)가 C(506행, `isd_interface()` 호출) **이전**에 온다 — 즉 이번 구현은 의도적으로(또는 우연히) "직전 iteration까지의 ISD 연결 결과"만 보고 이번 iteration에 새로 연결된 결과는 다음 iteration에야 반영되는 1-iteration 지연 구조다. 리팩토링 시 B를 C 뒤로 옮기면(데이터 의존이 없어 보여 "정리"하고 싶어질 수 있음) 이 지연이 없어져 **동작이 바뀐다** — behavior-preserving 원칙 위반. 두 함수로 쪼개더라도 B는 반드시 새 함수 내부에서 A와 C 사이의 위치를 유지해야 한다.

## 2. 하나의 함수로 묶을지, 여러 함수로 나눌지

- **A~F(Step F까지)는 하나의 함수로 묶는 것이 안전하고 자연스럽다.** C가 A의 출력을, D가 C의 출력을, E가 A+D의 출력을 필요로 하는 선형 체인이라 인위적으로 쪼개면 두 함수 사이에 `systemState`/`isd_state`(중간값)를 그대로 다시 넘겨야 하는 불필요한 인터페이스만 늘어난다. 이름 예: `tdc_update_subsystem_states(...)` — "서브시스템(ISD/BLE/PMIC/자극인디케이터) 상태 갱신"에 해당.
- **G(모드 플래그 분기, 526~536행)는 별도 함수로 분리 가능하고, 은수님이 제안한 "모드 플래그 갱신" 구분과 정확히 일치한다.** G의 유일한 입력은 `BLE_communicationState.mappingConnection`(bool 하나)이고, G의 출력(CFX 공유메모리 mode flag)은 A~F 체인의 어떤 값도 되돌려 소비하지 않는다(순수 말단 side-effect). 이름 예: `tdc_update_system_mode_flag(bool mappingConnection)`.
- 호출부는 `result = tdc_update_subsystem_states(...); tdc_update_system_mode_flag(result.BLE_communicationState.mappingConnection);` 형태로, **현재 텍스트 순서를 그대로 보존**하면 동작 불변성이 유지된다.

## 3. 입력 표 (이 블록이 요구하는 값)

| 변수 | 출처 | 신선도 |
|---|---|---|
| `ledPattern` | 441~452행 블록의 `geteLED_OutputPattern()` | 이번 iteration 값(fresh) |
| `mcuErrorCode` | `readErrorCode()` | fresh |
| `usbConnectorState` | `snd_charger_get_state()` | fresh |
| `batteryLevel` | `snd_batt_get_level()` | fresh |
| `powerButtonPushed` | `tdc_touch_process()` | fresh |
| `isd_state`(`.conneded_ISD`) | 직전 iteration의 Step C 결과 (루프 캐리) | **stale** — Step A에서만 소비 |
| `BLE_communicationState`(`.mappingConnection`, `.isdControlCommand`) | 직전 iteration의 Step D 결과 (루프 캐리) | **stale** — Step A(`.mappingConnection`)·Step C(둘 다)에서 소비 |
| `readStimulIndicator_OnOff()` | CFX 공유메모리(`userSettingValue.indicatorStimul_OnOff`) | 전역, 항상 최신 |
| (Step B 내부) `isUserSettingValueLoaded_CFX()` 등 | CFX 공유메모리, CFX가 비동기 갱신 | 전역, 1-iteration 지연 관계(§1 경고 참조) |

## 4. 출력 표 (다음 LED 요청 블록 등이 필요로 하는 값)

| 변수 | 소비처 | 소비 시점 |
|---|---|---|
| `systemState.enable_ISD` | 블록 내부 Step C | 같은 iteration, 즉시 |
| `systemState.enablePMIC` | 블록 내부 Step F | 같은 iteration, 즉시 |
| `systemState.StimulationIndicatorTriggerLowPower` | 블록 내부 Step E | 같은 iteration, 즉시 |
| `systemState.BLE_Off` / `.cradleLidClosed` / `.systemOff` | 범위 밖 — `NRF_On_OFF()`(660행), 크래들 뚜껑 체크(694행), sleep 진입 체크(701행) | 같은 iteration, 블록 뒤 |
| `isd_state.conneded_ISD` / `.isd_controlState` | 블록 내부 Step D(`bleCommunication` 인자 전체) + 범위 밖 LED 블록(582/584행 `isd_conn`) + `NRF_On_OFF()`(660행) + 크래들 체크(694행) | 같은 iteration(즉시~후반) |
| `isd_state`(전체) | **다음 iteration Step A**(`.conneded_ISD`) | 다음 iteration |
| `BLE_communicationState.mappingConnection` | 블록 내부 Step G + 범위 밖 LED 블록(623/625행 `map_conn`) + `NRF_On_OFF()` + sleep-guard(708행) | 같은 iteration, 블록 안팎 |
| `BLE_communicationState.StimulationIndicatorTrigger` | 블록 내부 Step E | 같은 iteration, 즉시 |
| `BLE_communicationState.BLE_Off_Command` | 범위 밖 `NRF_On_OFF()`(660행) | 같은 iteration, 블록 뒤 |
| `BLE_communicationState`(전체, `.mappingConnection`+`.isdControlCommand`) | **다음 iteration Step A·Step C** | 다음 iteration |

## 5. 인접 구역(node 02/04)에 대한 핸드오프 메모

- 입력 5종(`ledPattern`/`mcuErrorCode`/`usbConnectorState`/`batteryLevel`/`powerButtonPushed`)은 node 02 담당 구역(423~473행)에서 이번 iteration에 갓 읽은 순수 입력이며, 우리 구역 안에서는 절대 재대입되지 않는다 — 함수 분해 시 그대로 파라미터로 전달하면 됨.
- node 04(LED 요청 로직, 526~643행 — 실제로는 538행부터)는 `systemState`를 전혀 참조하지 않고 `isd_state.conneded_ISD`·`BLE_communicationState.mappingConnection`만 필요로 한다(582/584/623/625행). `systemState`는 그 뒤 `NRF_On_OFF()`·sleep 전이 로직(node 05 담당, 645행~)에서만 쓰인다 — LED 블록과 시스템제어 블록을 분리할 때 `systemState`를 LED 함수에 넘길 필요가 없다는 뜻.
