---
name: 02 과거 updateBatteryLevel() 해부 + 부활 타당성
purpose: 레거시 updateBatteryLevel()의 구조·"추세 반전 방지 마진"·입력 소스를 해부하고, QCC 기반 현 구조에서의 직접 부활 가능성과 포팅 가치를 판단
type: agent-log
maturity: experimental
tags: [main, battery, LED, refactor, updateBatteryLevel, legacy, dead-code, qcc]
---

# 02. 과거 `updateBatteryLevel()` 해부 + 부활 타당성

**TL;DR**: `updateBatteryLevel()`(`batteryNPowerControl.c:336`)은 자체 LSAD ADC 전압(`readBatteryLevel_FromCFX()`)을 충전/방전 별도 보정 경계값과 비교해 7단계 `EN__BATTERY_LEVEL`로 이산화하고, 충전 방향에 따라 단방향으로만 갱신되는 래칫(ratchet) 로직으로 "노이즈로 인한 추세 반전"을 막는다. 이 래칫이 곧 은수님이 말한 "배터리 변화 마진"의 정체다. 그러나 입력 소스가 QCC 이전 시대의 E8300 자체 전압측정이라 **직접 부활은 부적합**하다 — 다행히 "7단계 enum 이산화" 구조는 이미 `snd_batt_get_level()`로 포팅되어 있고, "함수 캡슐화" 정신만 신규 `tdc_` 함수로 마저 되살리면 된다. `updateBatteryLevel()` 자체는 현재 호출자가 전무하고 그 부작용(`battery_percentage`)도 다른 함수(`readBatteryPercentage()`)가 우회해버려 **완전한 dead code**로 확인된다.

## 1. 구조 해부

### 1.1 함수 개요

`EN__BATTERY_LEVEL updateBatteryLevel(int chargingState, EN__LED_PATTERN ledPattern)` — `batteryNPowerControl.c:336`. 호출 지점은 `main.c:448` 단 한 곳이며 현재 주석 처리:

```c
// batteryLevel      = updateBatteryLevel(usbConnectorState.chargerConnectorPluggedIn, ledPattern);
```
(`main.c:448`, 448행 바로 아래 451행에서 현재는 `snd_batt_get_level()`로 대체)

### 1.2 `updatecounter` 주기 throttle

`static volatile int updatecounter = 0;`(`batteryNPowerControl.c:338`)가 0일 때만 본문이 실행된다(`batteryNPowerControl.c:343`). 충전 분기(`df_Connected`)는 처리 후 `updatecounter = 100;`으로 명시적으로 리셋한다(`batteryNPowerControl.c:540`). 방전 분기(`df_Disconnected`)는 내부에서 리셋하지 않지만, 함수 말미의 공통 코드(`batteryNPowerControl.c:677~682`)가 `updatecounter--` 후 음수면 100으로 되감아, **어느 분기든 결과적으로 100회 주기 throttle**이 걸린다. 즉 메인 루프 약 100회당 1회만 실제 재계산하고 나머지는 이전 `batteryLevelStatus`를 그대로 반환한다.

### 1.3 `df_Connected`/`df_Disconnected` 분기 — 비대칭 구조

- `df_Disconnected`(방전, `batteryNPowerControl.c:345~446`): **`ledPattern > en__LED_POWER_On`** 게이트를 통과해야만 `readBatteryLevel_FromCFX()`를 읽고 레벨을 재계산한다(`batteryNPowerControl.c:347~349`). 게이트를 통과하지 못하면 이 호출 주기 동안 아무 것도 갱신하지 않는다.
- `df_Connected`(충전, `batteryNPowerControl.c:447~541`): 게이트 없이 항상 `readBatteryLevel_FromCFX()`를 읽고 재계산하며, 이 분기에서만 `updatecounter = 100`을 명시적으로 설정한다(`batteryNPowerControl.c:540`).

두 분기가 대칭이 아니다 — 방전 시에만 LED 패턴 게이트가 걸리고, throttle 리셋 타이밍도 분기마다 다르다. 레거시 코드 자체의 구조적 비대칭(의도인지 누락인지 확인 불가)으로, 신규 설계에서 그대로 따라갈 필요는 없다.

### 1.4 `isFirstBatteryCeckDone` 최초 진입 처리

`static volatile bool isFirstBatteryCeckDone = false;`(`batteryNPowerControl.c:339`). 최초 1회는 `prev_batteryLevelStatus`와의 비교(추세 반전 방지 마진, §2 참조) 없이 경계값만으로 직접 `batteryLevelStatus`를 확정한다(방전: `batteryNPowerControl.c:410~444`, 충전: `batteryNPowerControl.c:506~538`). 이후부터는 마진 로직이 적용된다. 부트 직후 "이전 추세"가 없는 상태에서 마진을 걸면 무한정 초기값(`en__batteryPower_100per`)에 갇히는 것을 막기 위한 부트스트랩 처리다.

### 1.5 7단계 `EN__BATTERY_LEVEL` 매핑 + 충전/방전 별도 boundary

`EN__BATTERY_LEVEL`(`batteryNPowerControl.h:17~26`)은 `en__batteryPower_0per=0` ~ `en__batteryPower_100per=6`의 7단계 enum이다. 경계값은 전압 기준으로 미리 계산되어 `batteryBoundary`(`ST__SYSTEM_BATTERY_BOUNDARY`, `batteryNPowerControl.c:210~217`) 구조체에 `dischargingBatterBoundary`/`chargingBatterBoundary` 두 세트로 나뉘어 저장된다. `calculationBatteryBoundary()`(`batteryNPowerControl.c:224~326`)가 보드 캘리브레이션 값(`readBatteryCalibrationValue()`)을 이용해 방전 시 경계 전압(0%=3.36V ~ 100%=4.096V, `batteryNPowerControl.c:185~190`)과 충전 시 경계 전압(0%=3.50V ~ 100%=4.12V, `batteryNPowerControl.c:192~197`)을 각각 ADC 카운트로 환산한다. 충전 중에는 배터리 자체 내부저항으로 인해 동일 SOC에서도 단자전압이 더 높게 측정되므로, 방전용보다 높은 전압 경계를 별도로 두는 것 — 즉 **충전 유무에 따라 전압→퍼센트 환산 테이블 자체가 다르다.**

### 1.6 `en__LED_POWER_On` 게이트의 의미

`EN__LED_PATTERN`(`LedOutput.h:29~39`)은 `en__LED_NA=0 ... en__LED_POWER_On=6, en__LED_POWER_Off=7`의 순서를 가진 레거시 enum이며, 현재도 "게이트 호환용으로 유지"(`LedOutput.h:26` 주석)되고 있다. `ledPattern > en__LED_POWER_On`은 enum 값이 7(`en__LED_POWER_Off`)일 때만 참이 된다. 즉 **방전 분기의 배터리 레벨 재계산은 LED 패턴이 "POWER_Off" 상태일 때만 수행**되고, 에러 패턴들(`en__LED_Map_Error` 등, 1~5)이나 `en__LED_POWER_On`(6) 상태에서는 건너뛴다. `LedOutput.c:323~336`의 `led_state_to_enum()`이 신규 상태머신(`LED_ST_POWER_ON`/`LED_ST_POWER_OFF` 등)을 이 레거시 enum으로 사상해 이 게이트와의 호환성을 지금도 유지하고 있다. 다만 이 게이트가 "부팅 시퀀스가 끝난 이후에만 배터리 표시를 갱신"이라는 의도인지, 단순히 오래된 매직넘버 비교인지는 코드만으로는 확정할 수 없다 — 신규 설계에서 그대로 채용할 근거는 약하다(추측 표기).

## 2. "추세 반전 방지 마진"의 정확한 동작

방전 분기(`batteryNPowerControl.c:351~409`)와 충전 분기(`batteryNPowerControl.c:451~505`)는 구조가 대칭이다. 각 경계 구간마다 다음 패턴이 반복된다(방전 예시, `batteryNPowerControl.c:361~368`):

```c
else if ((devidedBatteryLevel < boundary_100per) && (devidedBatteryLevel >= boundary_80per))
{
    // 감소하였다가 경계 값에서 노이즈로 인하여 배터리 추세가 변경되면 안된다.
    if (prev_batteryLevelStatus > en__batteryPower_80btw100)
    {
        batteryLevelStatus = en__batteryPower_80btw100;
    }
}
```

즉 **측정값이 특정 구간에 들어와도, `prev_batteryLevelStatus`(직전 확정 레벨)가 그 구간보다 "높을 때"(방전 기준)만 하위 레벨로 내려가는 것을 허용**한다. `prev_batteryLevelStatus <= candidate` 이면(이미 그 레벨이거나 더 낮으면) 아무 것도 하지 않는다 — 결과적으로 **방전 중에는 `batteryLevelStatus`가 enum 순서상 절대 증가하지 않고, 충전 중에는 절대 감소하지 않는 단방향 래칫**이 된다. 충전 분기는 부등호 방향만 반대(`prev_batteryLevelStatus < candidate`, `batteryNPowerControl.c:456` 등)로 대칭이다.

흥미로운 부수 효과: 방전 분기의 최상위 구간(`en__batteryPower_100per`, `batteryNPowerControl.c:353~360`)의 조건은 `prev_batteryLevelStatus > en__batteryPower_100per`인데, `en__batteryPower_100per`가 enum의 최댓값(6)이므로 이 조건은 **항상 거짓**이다 — 방전 중에는 100% 티어로 절대 복귀하지 않는다. 대칭적으로 충전 분기의 최하위 구간(`en__batteryPower_0per`, `batteryNPowerControl.c:501~504`, 이 구간은 예외적으로 `prev_batteryLevelStatus` 비교 없이 항상 대입 — `batteryNPowerControl.c:503`이 무조건 실행)도 확인 필요하나, 코드 상 0% 구간은 비교 없이 바로 대입하도록 되어 있어 완전 대칭은 아니다(경계 코너케이스, 버그라기보다 "가장 바깥쪽 티어는 그 방향으로 더 갈 곳이 없다"는 논리의 자연스러운 귀결로 판단됨).

**결론**: 이 래칫이 정확히 은수님이 말한 "배터리 레벨의 변화 마진"이다. 요구사항.md가 지목한 `main.c` 현재의 진입/이탈 이원 임계값(sticky band) 히스테리시스와는 구현 형태가 다르지만(고정 이중 임계값 vs. 이전 상태 비교 기반 단방향 래칫), **"경계 부근 노이즈로 인한 채터링·추세 역전 방지"라는 목적은 동일한 개념**이다. 요구사항.md 목표_3(QCC가 이미 측정·필터링하므로 마진 불필요)이 겨냥하는 대상과 정확히 일치한다 — 다만 레거시는 래칫 방식, 현재 `main.c`는 sticky-band 방식이라는 구현 차이는 계획 문서에서 구분해 서술할 가치가 있다.

## 3. 입력 소스 대조 — 왜 직접 부활이 부적합한가

`readBatteryLevel_FromCFX()`(`cfx_cm3_sharedMemory.c:191~195`)는:

```c
int readBatteryLevel_FromCFX(void)
{
    ci_battery_update();
    return cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3;
}
```

`ci_battery_update()`(`ci_battery.c:73~79`)는 LSAD(Low-Speed ADC) 채널 차분값을 계산한다 — `LSAD->DATA_TRIM_SAT_CH[0] - LSAD->DATA_TRIM_SAT_CH[1]` (DIO23_INPUT − VSSA), 즉 **E8300 보드 자체의 아날로그 배터리 전압 측정 회로**를 원시 ADC 카운트로 읽는 것이다. 이 값은 `calculationBatteryBoundary()`가 보드별 캘리브레이션(`readBatteryCalibrationValue()`, 4V 인가 시 측정값)으로 환산한 전압 경계값(§1.5)과 비교된다.

반면 현 구조는 `snd_batt_set_percent()`(BLE 0x34 수신 경로로 QCC가 주입, `batteryNPowerControl.c:42~45`)를 통해 **QCC가 이미 측정·필터링한 percent**를 받아 `snd_batt_get_percent()`/`snd_batt_get_level()`로 조회만 한다(`main.c:451` 주석 "직접 측정하지 않고, QCC에서 배터리 정보 받으면 업데이트 됨").

직접 부활이 부적합한 근거:

1. **측정 채널 자체가 다르다.** `readBatteryLevel_FromCFX()`는 E8300 LSAD 전압, QCC 경로는 QCC가 측정한 percent — 단위·해상도·보정 방식이 호환되지 않는다. `updateBatteryLevel()`을 그대로 살리려면 `readBatteryLevel_FromCFX()` 호출부를 QCC percent 기반으로 전면 교체해야 하는데, 그 순간 함수 몸체(전압 경계 비교 로직)는 의미를 잃는다.
2. **CFX 공유메모리 필드 자체가 "미사용" 확정.** 선행 분석(`docs\tasks\signalProcessing\20260514_cfx-cm3-analysis\첨부_분석_CFX-CM3 공유메모리 시퀀스 및 타이밍 Rev.1.md:219`, `첨부_분석_CFX 프로젝트 구조 및 시퀀스 Rev.0.md:192`)이 `systemShare.batteryLevel_CfX_to_CM3`를 "CM3에서 제어한 이후로 미사용"으로 이미 지목했다(`src\1__cfx\environment\shared_memory.h:141` 주석과 일치). CFX 쪽 소스도 이 필드를 배터리 판정에 쓰지 않는다는 뜻으로, E8300 자체 측정 경로 전체가 QCC 도입 이후 조직적으로 방치된 레거시임을 뒷받침한다.
3. **중복 판정 위험.** QCC가 이미 배터리를 측정·필터링해 percent를 내려주는데 E8300이 LSAD로 별도 측정해 병행 판정하면, 두 값이 불일치할 때 어느 쪽을 신뢰할지 문제(설계 복잡도만 증가, 요구사항.md 범위외_3과 충돌 — QCC 측 측정을 신뢰한다는 전제).
4. **곁가지 의존성.** 헤더가 `driver_MAX17262.h`(연료게이지 IC 드라이버)를 포함하는 등(`batteryNPowerControl.h:11`) 레거시 하드웨어 의존이 얽혀 있어, 되살리면 하드웨어 존재 여부까지 재검증해야 한다(현재 보드 리비전에 해당 회로가 실장되어 있는지는 이번 분석 범위 밖 — 확인 필요 사항으로 남김).

## 4. 부활/포팅 판단 — 무엇을 신규 `tdc_` 함수로 가져갈 가치가 있는가

| 레거시 요소 | 부활/포팅 가치 | 판단 근거 |
|---|---|---|
| (a) 함수 캡슐화(입력→레벨 판정→출력을 한 함수에 모음) | **가치 있음 — 포팅 권장** | `func_normal()`의 인라인 if-else 체인을 걷어내려는 목표_1과 정확히 일치. 레거시가 가졌던 "한 곳에 모은다"는 구조적 장점만 계승하고 내부 구현(전압 경계·래칫)은 QCC percent 입력에 맞게 새로 작성해야 함. |
| (b) 레벨→상태(퍼센트 버킷) 매핑을 단일 `switch`로 모음(`batteryNPowerControl.c:543~580`) | **부분 가치 — 이미 유사 구조가 존재** | 개념은 좋으나, "percent → 7단계 enum" 변환은 이미 `snd_batt_get_level()`(`batteryNPowerControl.c:47~88`)로 포팅되어 있다. 신규 함수는 이 기존 함수를 재사용하거나, LED 상태로 이어지는 다음 단계(레벨/퍼센트→LED 색상·점멸 패턴)만 새로 캡슐화하면 된다. |
| (c) `EN__BATTERY_LEVEL` 7단계 enum 이산화 | **이미 포팅 완료 — 추가 작업 불필요** | `snd_batt_get_level()`이 동일 enum·동일 7단계 경계(0/20/40/60/80/100)를 percent 기준으로 재현하고 있다(`batteryNPowerControl.c:54~87`). 은수님이 "적용하지 못해 아쉽다"고 한 부분 중 이 조각은 사실 **이미 되살아나 있다** — 계획 문서에서 이 사실을 명시해 중복 작업을 방지해야 함. |
| (d) `updatecounter` 주기 throttle | **포팅 비권장** | LED 갱신 주기 제어는 이미 `func_normal()`의 메인 루프 주기·LED arbiter(디바운스/우선순위) 쪽에서 다뤄질 사안(01/04 페르소나 소관)이며, 레거시처럼 함수 내부에 정적 카운터를 또 두면 상태가 이중화된다. |
| (e) `isFirstBatteryCeckDone` 부트스트랩 처리 | **개념만 참고 가능** | "최초 1회는 마진 없이 확정값 사용"이라는 아이디어 자체는 QCC 데이터가 아직 도달하지 않은 부팅 초기 구간에 여전히 유효할 수 있으나, 목표_3에서 마진 자체를 없애기로 했으므로 이 항목의 존재 이유(마진 우회)도 함께 사라진다. |
| (f) 충전/방전 별도 경계 테이블(`batteryBoundary` 이원화) | **포팅 대상 아님** | 전압 기반 보정이 QCC percent 체계에는 불필요. 다만 "충전 중 표시 정책이 방전 중과 다를 수 있다"는 상위 개념(예: 충전 중엔 UI 표시를 다르게)은 04(재캡슐화 설계) 페르소나가 LED 패턴 차원에서 별도로 검토할 사안. |
| (g) "추세 반전 방지" 래칫(§2) | **명시적으로 포팅 금지** | 요구사항.md 목표_3이 정확히 이 개념(마진)의 제거를 요청. QCC가 이미 필터링한 값을 다시 래칫으로 억제하면 목표에 정면으로 반한다. |
| (h) `en__LED_POWER_On` 류 LED 패턴 게이트(§1.6) | **포팅 비권장(근거 불충분)** | 의도가 불명확한 매직넘버성 비교이며, 현재 LED arbiter 구조(`LED_SRC` 기반)와 결이 다르다. 04 페르소나가 arbiter 결합을 설계할 때 이 게이트를 그대로 채용하지 말고 필요하면 명시적 조건(예: 부팅 완료 플래그)으로 재정의할 것을 제안. |

**요약**: 은수님이 아쉬워한 "구조"의 핵심 — ① 함수 캡슐화, ② enum 기반 이산화 — 중 ②는 이미 살아있고, ①만 신규로 되살리면 된다. "마진"은 오히려 이번 요구사항에서 제거 대상이므로 포팅해선 안 된다.

## 5. 죽은 코드 판정

`updateBatteryLevel` 전 트리 grep 결과 — 정의(`batteryNPowerControl.c:336`)와 선언(`batteryNPowerControl.h:35`)을 제외하면 유일한 참조는 `main.c:448`의 **주석 처리된 호출문**뿐이다. 다른 호출자는 전무.

내부 전역 상태(`batteryBoundary`, `batteryLevelStatus`, `prev_batteryLevelStatus`, `battery_percentage`)를 파일 전체 grep한 결과도 `batteryNPowerControl.c` 자기 자신 안에서만 쓰인다(`files_with_matches` 결과 1개 파일). 특히 `battery_percentage`의 유일한 외부 접근자였던 `readBatteryPercentage()`(`batteryNPowerControl.c:330~334`)조차 이미:

```c
int readBatteryPercentage(void)
{
    // return battery_percentage;
    return s_snd_batt_percent;
}
```

로 패치되어 있어(`batteryNPowerControl.c:332`가 주석 처리, 333행이 QCC 값으로 우회), `updateBatteryLevel()`이 계산한 결과는 **호출되더라도 아무 데서도 관측되지 않는다.** `readBatteryPercentage()`는 여전히 `mappingControl.c:2076`, `isd_interface_mapping_Live.c:460`, `remoteControl.c:966`에서 살아있게 호출되지만, 이들은 모두 QCC 경로(`s_snd_batt_percent`)를 읽을 뿐 레거시 배터리 판정과 무관하다.

`calculationBatteryBoundary()`는 `ci_battery_init()`(`ci_battery.c:59`)에서 지금도 호출되어 `batteryBoundary`를 채우지만, 그 결과를 읽는 곳은 죽어있는 `updateBatteryLevel()`뿐이므로 **부수효과 없는 낭비성 호출**이다. `readBatteryLevel_FromCFX()`도 호출자가 `updateBatteryLevel()`의 두 지점(`batteryNPowerControl.c:349, 449`)뿐이라 마찬가지다.

**결론**: `updateBatteryLevel()`은 (1) 호출자 없음(주석 처리), (2) 부작용 관측자 없음(우회됨), (3) 입력 소스도 CFX 측에서 "미사용"으로 이미 확정된 필드 — 3중으로 완전한 dead code다. 삭제 후보로 타당하나, 요구사항.md 범위외_4에 따라 **최종 삭제 여부는 계획 문서에서 제안만 하고 은수님 승인을 받아야 한다.** 삭제 시 함께 정리될 수 있는 것: `calculationBatteryBoundary()`, `batteryBoundary`/`batteryLevelStatus`/`prev_batteryLevelStatus`/`battery_percentage` 전역, `readBatteryLevel_FromCFX()`·`readBatteryCalibrationValue()`(단, 후자는 다른 소비자 유무 재확인 필요 — 이번 grep은 `updateBatteryLevel()` 스코프 중심이라 `readBatteryCalibrationValue()`의 전체 소비자 목록까지는 확증하지 않았음, 계획 단계에서 재확인 권장).

## 자체 검토

### 지침 준수 여부
| 항목 | 준수 |
|---|---|
| 읽기 전용(코드 미수정) | ✅ — Read/Grep만 사용, `src/` 변경 없음 |
| 근거 `파일:라인` 명시 | ✅ — 전 항목에 구체 경로·행 번호 표기 |
| 추측과 확인 구분 | ✅ — §1.6 LED 게이트 의도, §3의 하드웨어 실장 여부, §5의 `readBatteryCalibrationValue()` 전체 소비자는 "확인 필요/추측"으로 명시 |
| 로그 직접 Write, frontmatter+TL;DR | ✅ |

### 논리적 정합성
| 점검 | 결과 |
|---|---|
| "추세 반전 방지 마진" = 은수님의 "변화 마진" 동일 개념 확인 | ✅ (§2) — 단, 구현 형태(래칫 vs sticky-band)는 다름을 명시해 01 페르소나 결과와 대조 필요 |
| 직접 부활 불가 근거 3가지 이상 제시 | ✅ (§3) |
| 포팅 가치 항목별 판단 근거 제시 | ✅ (§4 표) |
| dead code 판정에 grep 전 트리 근거 | ✅ (§5) |

### 미해결 이슈
| 이슈 | 처리 |
|---|---|
| `readBatteryCalibrationValue()`의 전체 소비자 목록 미확증 | 계획 단계 또는 06(반증 검증) 재확인 권장 |
| E8300 현재 보드 리비전에 LSAD 배터리 측정 회로가 실제 실장되어 있는지 | 하드웨어 확인 필요, 이번 분석 범위 밖(추측 아님, 미확인으로 명시) |
| `en__LED_POWER_On` 게이트의 원 설계 의도 | 코드만으로 확정 불가 — 은수님 확인 필요 시 승인 게이트에서 질문 후보 |
