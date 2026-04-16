# UI 명령어 사용법

RTT 콘솔에서 펌웨어 동작을 테스트하는 CLI. `ENABLE_UI_CMD` 매크로 활성 시 사용 가능.

## 기본 규칙

- 모든 명령어는 `--`로 시작한다 (예: `--led show`)
- 대소문자 구분 없음 (`--led show` = `--LED SHOW`)
- 빈 줄(Enter만)을 입력하면 `--led show`가 자동 실행된다
- `--help`로 전체 명령어 목록 확인

---

## LED 상태 확인 / 제어

### 현재 상태 보기

```
> --led show
--- LED Arbiter state ---
  [power]   = none
  [error]   = none
  [ble]     = none
  [mapping] = none
  [battery] = ready
  [isd]     = in_use
  burst_in_progress = 0
  user_led_off = 0
-------------------------
```

### 소스별 상태 직접 설정 (override)

```
--led req <소스> <상태>
```

각 소스에 사용할 수 있는 상태는 아래 표 참조.

```
> --led req battery critical
OK: battery <- critical (override)

> --led req error fpga
OK: error <- fpga (override)
```

`--led req`로 설정하면 해당 소스에 **override**가 걸린다. override가 걸린 소스는 main loop이 자동으로 덮어쓰지 않으므로, 원하는 LED 패턴을 유지하면서 확인할 수 있다. `--led show`에서 `(override)` 표시로 구분된다.

### 소스별 상태 해제

```
> --led clr battery
OK: battery cleared
```

해당 소스의 override를 해제하고 상태를 `none`으로 되돌린다. 이후 main loop이 다시 자동으로 상태를 갱신한다.

---

## 소스별 사용 가능한 상태

`--led req <소스> <상태>` 명령어에서 사용하는 값 목록. 모든 소스 공통으로 `none`(요청 없음) 사용 가능.

### power (전원)

| 상태 | 설명 |
|---|---|
| `on` | 전원 켜짐 |
| `off` | 전원 꺼짐 |

```
> --led req power on
```

### error (에러 표시)

| 상태 | 설명 |
|---|---|
| `map` | 매핑 에러 |
| `mcu` | MCU 에러 |
| `accel` | 가속도센서 에러 |
| `fpga` | FPGA 에러 |
| `pmic` | PMIC 에러 |

```
> --led req error fpga
```

### ble (BLE 표시)

| 상태 | 설명 |
|---|---|
| `pair` | BLE 페어링 |
| `ota_qcc` | QCC OTA 진행 중 |
| `ota_ezairo` | Ezairo OTA 진행 중 |

```
> --led req ble pair
```

### mapping (매핑 연결)

| 상태 | 설명 |
|---|---|
| `no_isd` | 매핑 연결됨, ISD 미연결 |
| `with_isd` | 매핑 연결됨, ISD 연결됨 |

```
> --led req mapping with_isd
```

### battery (배터리)

| 상태 | 설명 |
|---|---|
| `ready` | 충분 (80% 이상) |
| `mid` | 보통 |
| `critical` | 부족 (10% 미만) |

```
> --led req battery critical
```

### isd (자극장치)

| 상태 | 설명 |
|---|---|
| `in_use` | 사용 중 |

```
> --led req isd in_use
```

---

## LED 추가 제어

### 사용자 LED 표시기 on/off

```
> --led user off
OK: user LED off

> --led user on
OK: user LED on
```

### 페어링 LED 주입

```
> --led pair
OK: PAIR latch injected
```

BLE 소스에 pair 상태를 바로 주입하는 단축 명령어.

### Power burst 확인 / 해제

```
> --led burst on       (현재 burst 상태 확인)
burst_in_progress = 1

> --led burst off      (burst 강제 해제)
OK: power burst force-cleared
```

---

## Override 명령어 (시스템 상태 강제 지정)

main 루프의 판정 로직에서 실제 값 대신 override 값을 사용하게 한다. LED 패턴 테스트에 활용.

### 배터리 잔량 override

```
> --batt 5
OK: battery override = 5%

> --batt show
  real  = 85%
  ovr   = 5% (active)
```

배터리 퍼센트를 고정하여 배터리 레벨별 LED 동작을 테스트한다.

### ISD 연결 override

```
> --isd on
OK: ISD override = on

> --isd off
OK: ISD override = off
```

### 매핑 연결 override

```
> --map on
OK: MAP override = on

> --map off
OK: MAP override = off
```

### 에러 주입 / 해제

```
> --err fpga
OK: FPGA error injected

> --err clr
OK: all error flags cleared
```

사용 가능한 에러 타입: `data_logging`, `fpga`, `acc`, `pmic`, `mcu`, `map`

---

## 기타 명령어

| 명령어 | 설명 |
|---|---|
| `--vol <1~10>` | 오디오 볼륨 변경 |
| `--init_all_map` | 모든 매핑 데이터 삭제 |
| `--dump_log` | 이벤트 로그 출력 |
| `--write_integrity_err` | Integrity error 로그 기록 |

---

## 테스트 시나리오 예시

### 배터리 부족 → 위험 LED 확인 (override 방식)

```
> --led req battery critical
OK: battery <- critical (override)
> --led show
  ...
  [battery] = critical (override)
```

또는 입력값 override 방식 (`--batt`는 배터리 퍼센트를 바꿔서 main loop이 자연스럽게 LED 상태를 계산):

```
> --batt 5
> --led show
  ...
  [battery] = critical
```

### 매핑 + ISD 연결 상태 LED 확인

```
> --map on
> --isd on
> --led show
  ...
  [mapping] = with_isd
  [isd]     = in_use
```

### 에러 LED 확인 후 해제

```
> --err fpga
> --led show
  [error] = fpga
> --err clr
> --led show
  [error] = none
```

### 전원 on/off 패턴 확인

```
> --led req power on
> --led show
  [power] = on
> --led req power off
> --led show
  [power] = off
```
