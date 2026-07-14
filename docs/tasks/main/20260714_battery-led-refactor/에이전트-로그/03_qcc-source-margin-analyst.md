---
name: QCC 배터리 소스 신뢰성 + 마진 제거 안전성 분석
purpose: "배터리 percent는 QCC가 이미 측정·필터링하므로 E8300 쪽 히스테리시스 마진이 불필요하다"는 은수님 주장을 코드 근거로 검증 — 주입 경로·QCC 필터링 여부·마진 제거 안전성·level 재사용성·RESET/타임아웃 상호작용 판정
type: agent-log
maturity: experimental
tags: [main, battery, LED, hysteresis, qcc, margin, ble, 0x34, agent-log]
---

# 페르소나_03 — QCC 배터리 소스 신뢰성 + 마진 제거 안전성 분석

**TL;DR**: QCC → E8300 percent 주입 경로(`ble_communication.c:130`)는 **가공 없는 단순 대입**(`s_snd_batt_percent = battery_level`)이며, E8300 쪽에는 스무딩·클램프·유효성 검사가 전혀 없다. QCC가 자기 쪽에서 이미 필터링했는지는 **코드상 확인 불가**(QCC 펌웨어는 이 저장소 밖). 다만 히스테리시스가 "관찰된 채터링 버그의 수정"이 아니라 **최초 설계 시점의 방어적 관례**(commit `3c2d3a7`)로 추가됐다는 이력, 그리고 이미 프로덕션에 마진 없이 동작 중인 소비처(`systemControl.c:251-255` veryLowBattery, `snd_batt_get_level()` 자체)가 존재한다는 사실은 "제거해도 안전하다" 쪽에 무게를 싣는다. 최종 평결은 **조건부 안전 — 실측 게이트 필요**로 정리했다.

## 1. 주입 경로 (확인 완료)

### 1.1 호출 체인

```
QCC (BLE 0x34, Power info)
  → SPI RX → bleCommunication() [ble_communication.c:227]
      SPI_CMMM_FETCH 케이스 (SPI 패킷 도착 시에만, 이벤트 구동 — 고정 주기 폴링 아님)
      → fetch_readDataForBleSetting() [ble_communication.c:27, 341에서 호출]
          → bleSettingPacket.data[0..2]에 페이로드 복사 (헤더 제외)
      → setting_nrf_ble_adv_info() [ble_communication.c:46, 417에서 호출]
          → EN__SND_BT_CMD_SYSTEM_INFO_POWER 분기 (ble_communication.c:119-165)
              battery_level = bleSettingPacket.data[0]
              snd_batt_set_percent(battery_level)   // ble_communication.c:130
              snd_batt_set_state(DISCHARGING|CHARGING|RESET)  // charger_connected 값에 따라 분기
```

### 1.2 `snd_batt_set_percent()` 정의 (batteryNPowerControl.c:42-45)

```c
void snd_batt_set_percent(int percent)
{
    s_snd_batt_percent = percent;
}
```

**가공 없는 단순 대입**. 범위 클램프(0~100), NaN/이상치 방어, 이동평균·저역통과 등 어떤 필터도 E8300 쪽에는 없다. `int battery_level = bleSettingPacket.data[0]`이 SPI 페이로드 1바이트를 그대로 읽은 값이므로, QCC가 몇 %를 보내든 그 값이 그대로 `snd_batt_get_percent()`에 반영된다.

### 1.3 갱신 주기 · 해상도

- **주기**: 이벤트 구동(`SPI_CMMM_FETCH` 상태가 될 때만 `fetch_readDataForBleSetting()` 실행, `ble_communication.c:250`). QCC가 몇 ms/초 간격으로 0x34를 보내는지는 **이 저장소에 정의가 없음 — 코드상 확인 불가**(QCC 펌웨어 소관, 요구사항 §5.2 범위외_3과 일치).
- **해상도**: `bleSettingPacket.data[0]`은 SPI 페이로드 1바이트를 `int`로 읽은 값 — **1% 단위 정수**로 보임(0~100 범위를 가정한 필드 설계, `ble_communication.c:105`의 `0xFF` 특수값 — "배터리 정보 미수신" 센티널 — 사용에서 유추). 0.x% 단위 소수 해상도 근거는 없음.
- **RESET 상태 시 0xFF 센티널**: `setting_nrf_ble_adv_info()`의 0x33(Battery, 레거시) 분기(`ble_communication.c:98-107`)에서 `snd_batt_get_state() == RESET`이면 `0xFF`를 응답하는 코드가 있으나, 이건 0x33(구식, "사실상 쓸모없어진 명령" 주석 `ble_communication.c:97`) 응답 값이지 0x34 수신 percent 자체의 센티널은 아니다. 0x34로 실제 수신되는 `battery_level` 값 자체에 대한 유효성 검사(0~100 범위 밖 값 거부 등)는 **없음**.

## 2. QCC 값이 이미 필터링됐는가 — 코드상 확인 불가 (근거 있는 정황 판단)

**단정 불가.** QCC 펌웨어 소스는 이 저장소에 없다(요구사항 §5.2 범위외_3, "QCC 측 배터리 측정·필터링 로직은 QCC 펌웨어 소관"). E8300 쪽에서 관찰 가능한 간접 정황은 다음과 같다.

| 정황 | 근거 | 해석 |
|---|---|---|
| E8300 쪽엔 스무딩 없음 (raw pass-through) | `batteryNPowerControl.c:42-45` | QCC가 안 걸러주면 E8300에서 걸러줄 곳이 전혀 없다 — QCC 신뢰가 전제 조건 |
| 값 필드가 1% 정수 해상도로 설계됨 (0~100, 0xFF 센티널) | `ble_communication.c:105, 130` | 정수 양자화 자체가 ±0.5% 미만의 미세 노이즈를 흡수하는 1차 방어선은 됨. 다만 "경계 근처에서 39↔40 플리핑"은 정수 양자화만으로는 막히지 않음 |
| E8300 쪽 히스테리시스가 "관찰된 QCC 노이즈 버그 수정"이 아니라 **최초 설계 시 선제적으로 추가**됨 | `git show 3c2d3a7`(2026-04-15, "LED 운용 방식 재구조화") 최초 도입 diff에 `/* Battery (SS4.5) -- hysteresis +/-2% */`가 이미 포함 — 이후 이 마진을 트리거한 버그 리포트/이슈 문서를 docs 전수 검색(`hysteresis`, `채터링`, `깜빡임`, `flicker`, `chatter`)했으나 **매칭 0건**(이번 파일 제외) | 마진은 "QCC 데이터가 실제로 튀는 걸 봐서 추가"가 아니라, CFX 자체측정 시절 `updateBatteryLevel()`(ADC 노이즈 대응, §4 참조)의 설계 관성을 QCC 경로에도 관례적으로 이식한 것으로 보임 — **QCC 노이즈가 실재한다는 직접 증거는 아님** |
| 이미 마진 없이 프로덕션 동작 중인 소비처 존재 | `systemControl.c:251-255`(`veryLowBattery`), `snd_batt_get_level()` 자체(§4) | percent를 단일 임계로 직접 쓰는 경로가 이미 있고 별도 디바운스 이슈 보고가 없음 — 단, §3에서 논하듯 이 소비처들은 "깜빡임이 안 보이는" 특성(비가시적/단조 소비) 때문일 수 있어 반증력이 제한적 |

**결론**: QCC가 자체적으로 스무딩/디바운스를 하는지는 **확인 불가**로 정직하게 남긴다. 다만 "E8300 마진이 실측된 QCC 노이즈에 대응해 만들어졌다"는 근거도 없다 — 오히려 방어적 관례로 보인다. 이 두 사실을 조합하면 마진 제거는 "검증된 안전"이 아니라 "반증되지 않은 위험을 안고 가는 결정"이다.

## 3. 마진 제거 안전성 평결 — 조건부 안전, 실측 게이트 필요

### 3.1 두 갈래 판단 기준 적용

- QCC percent가 안정적/양자화 → 마진 중복, 제거 안전 **(직접 증거 없음, §2)**
- QCC percent가 노이즈 있음 → 제거 시 경계 채터링 재발 위험 **(반증 못함, §2)**

코드만으로는 어느 쪽도 확정할 수 없다. 다만 아래 비대칭성이 있다.

### 3.2 소비처별 "마진 없음"의 위험도가 다르다 — 이게 핵심 논점

| 소비처 | 마진 유무 | 관찰 가능성 | 채터링 시 사용자 체감 |
|---|---|---|---|
| `systemControl.c:251-255` veryLowBattery → 파워오프 | 없음(단일 판정, 다만 20%p 폭 저전력 밴드 자체가 넓어 경계 재통과 빈도 낮음) | **낮음** — 파워오프는 편도(1회성) 동작, 껐다 켰다를 반복 관찰할 방법이 없음(방전 중 파워오프되면 그걸로 끝) | 체감 불가 — 챠터링이 있어도 "보이지 않는다" |
| `snd_batt_get_level()` (§4) | 없음(단일 임계 7단계) | **낮음** — level 자체를 직접 표시하는 UI 없음, systemControl 내부에서만 소비 | 체감 불가 |
| `main.c:542-578` 배터리 LED (`batt_st`) | 있음(±2%p, 상수는 40/41·65/64 WIP → 10/12·80/78 예정) | **높음** — LED는 사용자가 지속적으로 육안 관찰하는 출력 | **채터링 = 즉시 눈에 보이는 결함**(색 반짝임) |
| `main.c:620-654` 매핑 LED (`s_map_low_active`) | 있음(±2%p, 20/22) | **높음** — 동일하게 육안 관찰 대상 | 동일 |

**따라서 "이미 마진 없이 동작 중"이라는 사실(§2 표 마지막 행)은 안전성의 직접 증거가 아니다.** veryLowBattery·level 소비처는 마진이 없어도 문제가 드러나지 않는 구조(비가시적·편도성)이기 때문이지, QCC percent가 안정적이어서가 아닐 수 있다. 반대로 LED는 사용자가 매 프레임 보는 출력이라 마진 제거의 리스크가 고스란히 노출된다. 이 비대칭을 무시하고 "level/veryLowBattery도 마진 없이 잘 동작하니 LED도 안전하다"고 일반화하면 **논리적 비약**이다.

### 3.3 평결

**조건부 안전.** 코드 근거만으로 "QCC가 이미 필터링했다"를 확정할 수 없으므로, 마진 완전 제거를 무조건 안전하다고 보증할 수 없다. 그러나:

- 사용자(은수님)의 판단 근거인 "QCC가 측정을 알아서 잘 한다"는 전제는 요구사항 §5.2 범위외_3에서 이미 **전제로 못 박혀** 있고, 이는 이번 작업의 위임 범위 밖(QCC 펌웨어 검증은 여기서 할 수 없음)이다.
- 마진 제거가 실제로 문제가 되면 **증상이 명확하고(LED 깜빡임), 되돌리기 쉬운 변경**(단일 임계값 상수 하나만 복원하면 원복)이라 리스크가 낮다.
- 따라서 "일단 제거하고, 아래 §3.4 실측 게이트로 사후 확인" 방식이 합리적 절충안이다.

### 3.4 실측 게이트 (불확실성 해소 방법)

무엇을 측정하면 확정되는지:

1. **QCC percent 로그 채집**: `ble_communication.c:130` 진입 시점마다 `ci_printd`로 `(tick, battery_level)` 페어를 로깅(임시 계측, 배터리가 임계 근처(예: 10%, 80%)를 지나는 완속 방전/충전 구간에서 수 분간). 연속된 값이 39/40/39/40처럼 **경계에서 반복 교차**하면 QCC가 필터링을 안 한다는 직접 증거 — 마진 유지 필요. 값이 단조 감소/증가만 하고 역행이 없으면 QCC가 이미 안정화했다는 증거 — 마진 제거 확정.
2. **0x34 수신 간격 실측**: 위 로그의 tick 차이로 QCC의 실제 송신 주기 확인(현재 §1.3에서 확인 불가로 남긴 항목). 주기가 매우 짧다면(예: 100ms 이하) 노이즈 관찰 빈도가 높아지므로 마진 필요성이 커짐.
3. **경계 통과 시 LED 육안 관찰(파일럿)**: 마진 제거 버전을 임시 빌드해 실제 배터리로 10%/80% 경계를 천천히 통과시키며 LED가 깜빡이는지 육안 확인 — 가장 직접적인 최종 검증.

## 4. `snd_batt_get_level()` 재사용성 — 이미 마진 없는 단일 임계 매핑

`batteryNPowerControl.c:47-88` 정의를 보면 `snd_batt_get_level()`은 **호출마다 `snd_batt_get_percent()`를 읽어 그 자리에서 7단계 enum으로 매핑하는 순수 함수**이며, `prev_*` 같은 상태 기억이 전혀 없다(구조체 정의에 static 변수 없음, 매 호출 독립적 판정).

```c
EN__BATTERY_LEVEL snd_batt_get_level(void)
{
    int percent = snd_batt_get_percent();
    if (100 <= percent) return en__batteryPower_100per;
    else if ((80 <= percent) && (percent < 100)) return en__batteryPower_80btw100;
    ... // 이하 20%p 폭 단일 임계, 히스테리시스 없음
}
```

이는 레거시 `updateBatteryLevel()`(batteryNPowerControl.c:336-685, ADC raw 값 `readBatteryLevel_FromCFX()` 기반, `prev_batteryLevelStatus` 비교로 "노이즈 추세 반전 방지" — 명시적 히스테리시스 로직 포함)과 **완전히 다른 설계**다. 레거시는 CFX 자체 측정값(ADC raw, 노이즈 큼)을 다뤘기 때문에 마진이 필요했고, 현재 `snd_batt_get_level()`은 QCC가 이미 준 percent를 그냥 재양자화만 한다 — **애초에 "QCC가 필터링해준다"는 전제를 이미 코드로 실천 중인 함수**다.

**재사용성 판단**: level(7단계, 20%p 폭)은 percent(1%p 해상도)보다 밴드가 훨씬 넓다(20%p vs 현재 LED 임계 10~12%p, 78~80%p 근방 폭 좁음). LED 블록이 percent 대신 level을 쓰면:
- 장점: 20%p 폭 자체가 자연 데드존 역할을 해 별도 히스테리시스 없이도 채터링 확률이 percent 직접 사용보다 낮아짐. `snd_batt_get_level()` 재사용 시 신규 함수가 QCC 원시값을 다시 파싱할 필요 없이 기존 조회 함수 그대로 호출 가능 — 캡슐화 이점.
- 단점: LED 3단계(CRITICAL/MID/READY)와 level 7단계 경계가 정확히 안 맞음(현재 LED 임계는 10%·80%, level 경계는 20%·40%·60%·80%) — level 그대로 쓰면 LED 임계가 20%/80%로 강제로 바뀌는 **사양 변경**이 추가로 발생. 이슈_2(단일 임계값을 무엇으로 둘지)와 얽힘.

**결론**: level 재사용은 "20%p 폭이 사실상 마진 역할을 대신한다"는 점에서 매력적이지만, 그러면 LED 임계값 자체가 20/40/60/80 고정으로 못박히므로 계획 단계에서 이슈_2·이슈_4와 함께 옵션으로 명시 제시할 것을 권장(이번 문서는 판단 재료 제공까지, 확정은 계획 승인 단계).

## 5. RESET/타임아웃 상호작용

### 5.1 `EN__SND_BATT_STATE_RESET` 게이트는 마진과 무관 · 독립적으로 유지 필요

`main.c:558-562`:
```c
if (!ovr_batt_active && snd_batt_get_state() == EN__SND_BATT_STATE_RESET)
{
    batt_st = LED_ST_IDLE;   // pct 판정을 아예 건너뜀
}
```

이 게이트는 pct 값과 무관하게 **상태(state) 필드**만 검사한다. 마진(히스테리시스)을 제거해 단일 임계 매핑으로 바꾸더라도 이 RESET 분기는 **그대로 유지해야 한다** — 이유: `initialize.c:481-482`에서 부팅 시 `snd_batt_set_percent(0)`으로 초기화되므로, RESET 게이트가 없으면 QCC 0x34 수신 전까지 pct=0이 그대로 "CRITICAL(저전력)" 조건에 걸려 부팅 초기 노란 LED가 잠깐 켜지는 회귀가 재발한다(이 문제 자체가 과거 Rev.5에서 RESET 가드를 추가한 이유, commit `9409f9f` "배터리 RESET 가드 추가"). **마진 제거와 RESET 가드 유지는 서로 독립적인 두 결정**이며, 계획서에는 "마진만 제거, RESET 가드는 보존"을 명시해야 한다.

### 5.2 `fake_0x34` 타임아웃 블록(main.c:456-485)과의 관계도 독립적

- 타임아웃 판정 조건은 `snd_batt_get_state() != EN__SND_BATT_STATE_RESET`(main.c:474) — **percent 값을 전혀 보지 않는다.** 3000ms(main.c:468) 동안 state가 RESET에 머무르면 `qcc_batt_timeout = true`로 파워오프 절차(main.c:679-691, 별도 전용 블록, "방법 E" — `docs/tasks/power/20260609_qcc-batt-timeout-sleep/분석.md §4`)를 태운다.
- 이 로직은 pct 히스테리시스 유무와 **완전히 독립**이다. 마진을 제거해도 타임아웃 판정식·파워오프 경로는 코드 한 줄도 건드릴 필요가 없다.
- 다만 §5.1의 RESET 가드가 사라지면(잘못 손대면) `qcc_batt_timeout` 발생 전 짧은 구간에 pct=0이 그대로 LED CRITICAL로 새기 때문에, RESET 가드 보존이 여기서도 다시 한번 전제 조건이 된다.

### 5.3 결론

RESET 가드·타임아웃 블록은 마진(히스테리시스) 제거 작업의 **범위 밖 불변 영역**으로 취급해야 한다. 계획 단계에서 신규 함수(`tdc_led_request_battery` 류)를 설계할 때 이 RESET 조기 반환 분기를 함수 시그니처/내부 로직에서 빠뜨리지 않도록 체크리스트에 추가할 것을 권장한다(선행 분석 `docs/tasks/main/20260709_func-normal-refactor/에이전트-로그/04_led-request-logic-diagnostician.md` §6 체크리스트에 이미 유사 항목 있음 — 마진 제거 버전에도 동일하게 적용).

## 자체 검토

### 지침 준수 여부
| 항목 | 준수 |
|---|---|
| 코드(`src/`) 무수정, 읽기 전용 분석 | 예 — Read/Grep/Bash(git log·diff만) 사용, Edit/Write는 본 문서에만 |
| 추측·확인 엄격 구분 | 예 — §2·§3에 "코드상 확인 불가" 명시 라벨링, QCC 필터링 여부를 단정하지 않음 |
| 파일:라인 근거 | 예 — 전 절에 `파일:라인` 표기 |
| 실측 게이트 제안(불확실 시) | 예 — §3.4 |

### 논리적 정합성
- §3.2에서 "이미 마진 없는 소비처가 있다"는 사실을 안전성의 직접 증거로 과대 해석하지 않도록 비가시성/편도성 논거로 반박 — 사용자 주장에 아부하지 않고 반증 가능성을 명시했다.
- §2의 "히스테리시스가 방어적 관례로 도입됐다"는 주장은 `git show 3c2d3a7` diff 확인 + docs 전수 grep(0건)으로 뒷받침됨. 다만 이는 **부재 증명(negative evidence)**의 한계가 있음 — "버그 리포트 문서가 없다"가 "버그가 없었다"를 완전히 보증하진 않는다는 점을 여기 명시해 둔다.

### 미해결 이슈
- QCC 자체 필터링 여부·0x34 송신 주기는 이 저장소 범위에서 확정 불가 — §3.4 실측 게이트로 승인 게이트 이후(구현 전 또는 구현 후 검증 단계에서) 별도 계측 세션 필요.
- level 재사용 시 LED 임계값이 20/40/60/80으로 강제 이동하는 사양 변경 폭은 계획 단계에서 이슈_2·이슈_4와 함께 은수님 확인 필요(본 문서는 옵션 제시까지).
