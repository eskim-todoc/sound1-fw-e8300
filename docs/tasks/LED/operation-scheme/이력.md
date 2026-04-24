# LED 운용 방식 Rev.5 처리 결과

작성자: 김은수
작성일: 2026-04-15
상태: 구현 1차 완료. 실기 검증 대기.
브랜치: `claude_led-rev5-impl`
관련 분석서: [`docs/[분석] LED 운용 방식.md`](./%5B%EB%B6%84%EC%84%9D%5D%20LED%20%EC%9A%B4%EC%9A%A9%20%EB%B0%A9%EC%8B%9D.md)

> **표기 안내 (전체 문서 공통)**
> - 점멸 패턴은 모두 `ON X ms / OFF Y ms` 형식으로 표기. 한 주기 = `X + Y` ms.
>   예) `ON 1100ms / OFF 1100ms` → 켜진 시간 1100ms, 꺼진 시간 1100ms, 한 주기 2200ms.
> - 점멸이 없으면 `지속 ON`.
> - 코드 디스크립터 `(on_ms, period_ms)` 와의 환산: `period_ms = on_ms + off_ms`.
>   예) `(on_ms=1100, period_ms=2200)` ↔ `ON 1100ms / OFF 1100ms`.

---

## 1. 배경 — 실기 관찰

보드 전원 투입 시 육안으로 관찰된 동작 순서:

1. **부팅 직후**: 노란색 LED 짧게 점등
2. **POWER ON LED**: SKYBLUE ON 80ms / OFF 220ms × 5회 버스트
3. **정상 운용**: 녹색 지속 ON (배터리 테스트용 90% 오버라이드 반영)

스펙상 1번 "노란색 잠깐 점등"은 정의되어 있지 않음 → 부팅 시퀀스 상의 의도치 않은 LED 출력.

---

## 2. 원인 분석

### 2.1 타임라인

| # | 시점 | 코드 위치 | 상태 |
|---|---|---|---|
| 1 | 리셋 직후 | — | GPIO Hi-Z, LED off |
| 2 | `Initialize()` 진입 | [`initialize.c:604-605`](../src/2__cm3/Cortex-M3-src/systemControl/initialize.c#L604) | `snd_batt_set_state(RESET)` + `snd_batt_set_percent(0)` |
| 3 | 메인 루프 1st iter | [`main.c:528`](../src/2__cm3/Cortex-M3-src/main.c#L528) (수정 전) | `pct = 0 < 10` → **`LED_ST_BATT_CRITICAL` 요청** |
| 4 | Arbiter 결정 | [`LedOutput.c`](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c) | BATT_CRITICAL (prio 60)이 유일 요청자 → **승자 선출** |
| 5 | 패턴 출력 | [`LedOutput.c:23`](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L23) | ORANGE ON 1100ms / OFF 1100ms 점멸 → **노란색 LED 점등** 🟡 |
| 6 | ~500ms 후 fake 배터리 주입 | [`main.c:455-457`](../src/2__cm3/Cortex-M3-src/main.c#L455) | `pct=90`, `state=DISCHARGING`, `charger=DISCONNECTED` |
| 7 | POWER_ON 요청 | [`systemControl.c:204`](../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c#L204) | LED_ST_POWER_ON (prio 95) → SKYBLUE 버스트 |
| 8 | 버스트 종료 후 | [`LedOutput.c:257-266`](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c#L257) | 자가 해제, BATT_READY(GREEN) 표시 |

### 2.2 근본 원인

- `Initialize()`에서 배터리 정보를 **"레벨 0, 상태 RESET"** 으로 초기화.
- 메인 루프는 배터리 LED 요청을 **매 iteration 무조건 수행**.
- 요청 로직은 `pct` 값만 보고 판정하므로, `pct = 0`이면 실제 배터리 정보 미수신 상태임에도 불구하고 `BATT_CRITICAL`로 해석.
- QCC의 `0x34(Power info)` 프로토콜 수신 전까지(약 500ms)는 이 잘못된 판정이 Arbiter에 반영됨.
- 하드웨어적으로 노란색은 RGB(ACTIVEHIGH)에서 R+G 동시 점등으로 구현 → 사용자 눈에 "노란색"으로 보임.

### 2.3 추가로 발견된 Rev.5 불일치

기존 코드의 패턴 디스크립터가 Rev.5 스펙과 어긋나 있음:

| 상태 | 기존 코드 | Rev.5 스펙 |
|---|---|---|
| `LED_ST_BATT_CRITICAL` | ORANGE **ON 180ms / OFF 180ms** 점멸 | ORANGE **ON 1100ms / OFF 1100ms** 점멸 |
| `LED_ST_BATT_MID`      | ORANGE **ON 1100ms / OFF 1100ms** 점멸 | ORANGE **지속 ON** |
| `LED_ST_MAPPING_NO_ISD` | BLUE **ON 300ms / OFF 300ms** 점멸 | BLUE **ON 1100ms / OFF 1100ms** 점멸 |
| `LED_ST_OTA_EZAIRO`    | GREEN **ON 1100ms / OFF 1100ms** 점멸 | GREEN **ON 180ms / OFF 180ms** 점멸 |
| 네이밍 | `LED_ST_READY` | `LED_ST_BATT_READY` (배터리 시리즈 네이밍 일관성) |

---

## 3. 해결 방법

### 3.1 배터리 RESET 상태 가드 추가 (핵심)

**파일**: [`src/2__cm3/Cortex-M3-src/main.c:514-547`](../src/2__cm3/Cortex-M3-src/main.c#L514) (배터리 LED 요청 블록)

```c
if (!ovr_batt_active && snd_batt_get_state() == EN__SND_BATT_STATE_RESET)
{
    /* 배터리 정보 미수신: 판정 보류 */
    batt_st = LED_ST_IDLE;
}
else if (pct < 10)                                              batt_st = LED_ST_BATT_CRITICAL;
else if (pct < 12 && prev_batt_st == LED_ST_BATT_CRITICAL)      batt_st = LED_ST_BATT_CRITICAL;
else if (pct >= 80)                                             batt_st = LED_ST_BATT_READY;
else if (pct >= 78 && prev_batt_st == LED_ST_BATT_READY)        batt_st = LED_ST_BATT_READY;
else                                                            batt_st = LED_ST_BATT_MID;
```

**동작**:
- `snd_batt_get_state() == EN__SND_BATT_STATE_RESET` 일 때 → `LED_ST_IDLE` 요청 (prio 10, BLACK 지속 OFF)
- Arbiter에서 다른 요청자(POWER_ON, ERROR 등)가 있으면 자동 선점됨
- QCC가 `0x34` 수신 후 `snd_batt_set_state(DISCHARGING 또는 CHARGING)`으로 상태 전환하면 이 가드가 자동 해제되고 기존 `pct` 판정 로직이 동작

**UI 오버라이드 예외**: `ui_sys_ovr_batt_active()`가 true이면 테스트 의도를 존중해 가드를 건너뜀. (`ui_led_cmd`로 실기 검증 시 RESET 상태에서도 강제 표시 가능)

### 3.2 Rev.5 패턴 4건 정정

**파일**: [`src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c`](../src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c) 의 `k_led_patterns[]` 테이블.

각 행에 사람이 읽기 좋은 ON/OFF 표기를 인라인 주석으로 같이 적어 넣음 (코드 디스크립터 값과 시간 의미를 한눈에 대조 가능):

```c
[LED_ST_BATT_READY]     = { en__LED_GREEN,   0,    0,    0 },  // 녹색 지속 ON       (변경: LED_ST_READY → LED_ST_BATT_READY)
[LED_ST_IN_USE]         = { en__LED_WHITE,   0,    0,    0 },  // 흰색 지속 ON
[LED_ST_BATT_MID]       = { en__LED_ORANGE,  0,    0,    0 },  // 노랑 지속 ON       (변경: ON 1100ms / OFF 1100ms 점멸 → 지속 ON)
[LED_ST_BATT_CRITICAL]  = { en__LED_ORANGE,  1100, 2200, 0 },  // 노랑 ON 1100ms / OFF 1100ms 점멸 (변경: ON 180ms / OFF 180ms → ON 1100ms / OFF 1100ms)

[LED_ST_MAPPING_NO_ISD] = { en__LED_BLUE,    1100, 2200, 0 },  // 파랑 ON 1100ms / OFF 1100ms 점멸 (변경: ON 300ms / OFF 300ms → ON 1100ms / OFF 1100ms)
[LED_ST_MAPPING_ISD]    = { en__LED_BLUE,    0,    0,    0 },  // 파랑 지속 ON

[LED_ST_PAIR]           = { en__LED_BLUE,    180,  360,  0 },  // 파랑 ON 180ms  / OFF 180ms 점멸
[LED_ST_OTA_QCC]        = { en__LED_GREEN,   1100, 2200, 0 },  // 녹색 ON 1100ms / OFF 1100ms 점멸
[LED_ST_OTA_EZAIRO]     = { en__LED_GREEN,   180,  360,  0 },  // 녹색 ON 180ms  / OFF 180ms 점멸 (변경: ON 1100ms / OFF 1100ms → ON 180ms / OFF 180ms, QCC와 시각 구분)
```

### 3.3 `LED_ST_READY` → `LED_ST_BATT_READY` 리네임

배터리 시리즈 네이밍 일관성(`BATT_READY` / `BATT_MID` / `BATT_CRITICAL`) 확보. 리네임 대상 전부 업데이트:

| 파일 | 위치 | 변경 |
|---|---|---|
| `LedOutput.h` | enum `led_state_t` | `LED_ST_READY` → `LED_ST_BATT_READY` |
| `LedOutput.c` | 패턴 디스크립터 테이블 | 동일 |
| `LedOutput.c` | `led_prio_of()` switch | 동일 |
| `main.c` | 배터리 판정 4개 라인 | 동일 |
| `ui_led_cmd.c` | `k_state_map` 엔트리 | `"READY"` → `"BATT_READY"` (UI 커맨드 키워드도 변경) |

### 3.4 표기 안내·주석 정정

`LedOutput.h`:
- 구조체 `led_pattern_desc_t`에 ON/OFF ↔ (on_ms, period_ms) 환산 안내 주석 추가.
- enum `led_state_t` 항목 주석을 모두 `색 ON Xms / OFF Yms 점멸` 또는 `색 지속 ON` 형식으로 통일.

`LedOutput.c`:
- 패턴 디스크립터 테이블 상단에 ON/OFF 표기 안내 코멘트 추가.
- 각 행에 사람이 읽기 좋은 형식의 인라인 주석 추가 (위 §3.2 참조).

`Rev.5 분석 문서`:
- 문서 상단에 표기 안내 추가 (`ON Xms / OFF Yms` 통일).
- 본문의 모든 시간 표기를 같은 형식으로 통일.
- §1.5의 stale 수치(`MAPPING_NO_ISD = 300ms ON / OFF`)를 §변경점·§3.4와 일치하도록 정정 (`ON 1100ms / OFF 1100ms`).

---

## 4. 수정 파일 목록

| 파일 | 변경 요약 |
|---|---|
| `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.h` | enum 리네임, 구조체 환산 안내 주석, enum 주석 ON/OFF 표기 통일 |
| `src/2__cm3/Cortex-M3-src/systemControl/LedOutput.c` | 패턴 디스크립터 4건 수정, 우선순위 switch case 리네임, 테이블 ON/OFF 인라인 주석 |
| `src/2__cm3/Cortex-M3-src/main.c` | 배터리 RESET 가드 추가, `LED_ST_READY` → `LED_ST_BATT_READY` |
| `src/2__cm3/Gen1_5/ui/ui_led_cmd.c` | `"READY"` 커맨드 키워드 → `"BATT_READY"` |
| `docs/[분석] LED 운용 방식 분석 및 수정 방향 Rev.5 by 김은수.md` | 표기 통일 + §1.5 stale 수치 정정 (의미 변경 없음, MAPPING_NO_ISD §1.5만 §변경점 반영) |
| `docs/[이력] LED 운용 방식 Rev.5 처리 결과 by 김은수.md` | 본 문서 (신규) |

---

## 5. 기대 동작 (수정 후)

### 5.1 부팅 타임라인

| # | 시점 | LED |
|---|---|---|
| 1 | 리셋~`Initialize()` 완료 | GPIO LOW → **off** (변경 없음) |
| 2 | 메인 루프 진입 (배터리 상태 RESET) | `LED_SRC_BATTERY = LED_ST_IDLE` → 다른 요청자 없으면 **off** |
| 3 | fake 0x34 주입(~500ms 후) | `pct=90`, `state=DISCHARGING` → `LED_ST_BATT_READY` 요청 시작 |
| 4 | `systemControl()`이 POWER_ON 요청 | prio 95 승자 → SKYBLUE 버스트 × 5 |
| 5 | 버스트 종료 후 | BATT_READY(prio 30) 승자 → **녹색 지속 ON** 🟢 |

**핵심 차이**: #2 단계에서 더 이상 노란색이 나타나지 않음.

### 5.2 실기 차이

- 노란색 LED의 초기 점등 현상 제거
- `BATT_MID` 조건(10~80%): 기존 ON 1100ms / OFF 1100ms 점멸 → **지속 ON**
- `BATT_CRITICAL` 조건(<10%): 기존 ON 180ms / OFF 180ms 점멸 → **ON 1100ms / OFF 1100ms 점멸**
- `MAPPING_NO_ISD`: 기존 ON 300ms / OFF 300ms 점멸 → **ON 1100ms / OFF 1100ms 점멸**
- `OTA_EZAIRO`: 기존 ON 1100ms / OFF 1100ms 점멸 → **ON 180ms / OFF 180ms 점멸** (QCC ON 1100ms / OFF 1100ms와 시각 구분)

---

## 6. 검증 계획

### 6.1 회귀 테스트 (Rev.5 §7.2 기반)

| # | 시나리오 | 기대 결과 |
|---|---|---|
| a | 정상 부팅 → 배터리 90% | off → SKYBLUE 버스트 → GREEN 지속 ON. **노란색 없음** |
| b | 배터리 9% + ISD 연결 | ORANGE ON 1100ms / OFF 1100ms 점멸 (BATT_CRITICAL) |
| c | 배터리 30% | ORANGE 지속 ON (BATT_MID) |
| d | 배터리 85% | GREEN 지속 ON (BATT_READY) |
| e | Mapping 연결 + ISD 미연결 | BLUE ON 1100ms / OFF 1100ms 점멸 |
| f | OTA_EZAIRO | GREEN ON 180ms / OFF 180ms 점멸 |
| g | OTA_QCC | GREEN ON 1100ms / OFF 1100ms 점멸 (f와 구분) |
| h | 배터리 정보 수신 지연 (>500ms) | 지연 구간 동안 **off 유지**, 수신 후 정상 표시 |

### 6.2 단위 검증

현재 `tests/` 폴더 비어 있음. Rev.5 §7.1 기반 단위 테스트는 후속 작업에서 추가 예정.

### 6.3 실기 검증 시 확인 포인트

- 오실로스코프로 각 패턴 ON 시간 / 한 주기 오차 ±5% 이내
- UI 커맨드 `led state BATT_READY` 로 기존 `READY` 대비 리네임 정상 동작 확인
- 배터리 상태 RESET → DISCHARGING 전환 순간 LED 전환 지연 없음

---

## 7. 잔여 과제

1. **fake 0x34 디버깅 코드 정리**: [`main.c:439-461`](../src/2__cm3/Cortex-M3-src/main.c#L439)의 `#if 1` 블록은 QCC 대체용 디버깅 코드. 실제 QCC 통신 안정화 후 제거 필요.
2. **초기화 단계 에러 → POWER_ON 미요청 게이트**: Rev.5 §1.1, §2 이슈 #8에 따른 구현 필요 (본 PR 범위 외).
3. **단위 테스트 셋 작성**: `tests/led/` 구성 (Rev.5 §7.1).
4. **사용자 LED off 플래그 동작 확인**: BATT_CRITICAL / MAPPING / BATT_READY 억제, ERROR / POWER_* 예외 처리가 현 Arbiter에서 이미 구현되어 있음을 실기로 재확인 필요.

---

## 8. 요약

- **문제**: 부팅 초기 ~500ms 동안 노란색 LED가 잠깐 점등.
- **원인**: `Initialize()`가 배터리 percent를 0으로 초기화하는데, 메인 루프의 배터리 LED 요청 로직이 `pct=0`을 `BATT_CRITICAL`(노랑 점멸)로 판정. QCC 정보 수신 전까지 이 오판정이 Arbiter에 반영.
- **해결**: 배터리 상태가 `EN__SND_BATT_STATE_RESET`일 때는 `pct` 판정을 건너뛰고 `LED_ST_IDLE`을 요청하도록 가드 추가.
- **부수 작업**: Rev.5 스펙과 어긋나 있던 패턴 4건 정정(BATT_CRITICAL, BATT_MID, MAPPING_NO_ISD, OTA_EZAIRO) 및 `LED_ST_READY` → `LED_ST_BATT_READY` 리네임. 코드·문서 시간 표기를 모두 `ON Xms / OFF Yms` 형식으로 통일.
- **영향 파일**: 4개 소스 + 1개 분석 문서(표기 통일) + 1개 신규 문서.
- **다음 단계**: 실기에서 §6.1 시나리오 a~h 검증 후 upstream PR 고려.
