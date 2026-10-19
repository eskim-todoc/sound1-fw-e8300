# 시스템 시험 명령 사용방법

이 문서는 SYS-007, 012, 023, 050, 051, 054, 073, 077, 080의 준비/명령/확인값/종료 방법을 한곳에 모은다. 시험용 외부기와 내부기 지그에서만 사용한다. 착용 제품이나 이식 내부기를 연결한 상태에서 오류 주입 명령을 실행하지 않는다. 시험 코드 실행 결과와 제품의 시스템 시험 합격은 구분한다.

## 1. 빌드 설정과 RTT 연결

CM3 설정은 [processorDirective.h](../../src/2__cm3/processorDirective.h)에 있다. 필요한 상수를 `1`로 바꿔 CM3를 빌드한다. `ENABLE_UI_CMD`는 켜고, 기존 유닛 시험의 `TDC_UNIT_TEST`는 켤 필요가 없다. 시험이 끝난 출하/일반 사용 빌드에서는 아래 상수를 모두 `0`으로 한다.

| 시험 | 켜는 상수 | 추가 조건 |
|---|---|---|
| 7 전하량/오류 LED | `TDC_SYS_TEST_7_ENABLE` | 내부기/매핑 분리 |
| 12 처리 시간 | `TDC_SYSTEM_TEST_DIO35_ENABLE` | CM3와 [CFX 설정](../../src/1__cfx/test/tdc_sys_test_config.h) 모두 1 |
| 23 저전력 알람 | `TDC_SYS_TEST_23_ENABLE` | 시험용 내부기 Live 연결 |
| 50/51 감사기록 표시 | `TDC_SYS_TEST_AUDIT_ENABLE` | RAM 표시 전용 |
| 54 이미지 CRC | `TDC_SYS_TEST_54_ENABLE` | 내부기/매핑 분리 |
| 73 감사 저장 실패 | `TDC_SYS_TEST_73_ENABLE` | 내부기/매핑 분리 |
| 77 암복호화 | `TDC_SYS_TEST_77_ENABLE` | 내부기/매핑 분리 |
| 80 설정 복원 | `TDC_SYS_TEST_80_ENABLE` | 내부기/매핑 분리. 시험 전용 파일 사용 |

2026-10-16 SYS80 변경 작업에서는 은수님이 켜 둔 SYS-007/023/AUDIT, CM3/CFX DIO35와 SYS-054/073/077/080의 `1`을 유지했다. 현재 파일의 상수 값을 확인하고 빌드하며, 이 설명의 날짜만 보고 활성 여부를 판단하지 않는다.

SEGGER RTT Viewer로 CM3에 연결하고 Terminal 0에서 명령 한 줄을 입력한 뒤 Enter를 누른다. 기동 직후에는 전원 LED 패턴이 끝날 때까지 기다린다. 명령마다 로그가 끝난 뒤 다음 명령을 입력한다. `BUSY`는 이전 로그를 내보내는 중이라는 뜻이다.

각 시스템 시험 로그는 `[SYS-번호]`로 시작하며 한 행은 CRLF 포함 60바이트 이하다. 벡터는 `IN`, `ENC`, `OUT` 다음의 바이트 위치를 보고 이어 읽는다. RTT 공간이 부족하면 행 전체를 보류한다. 기존 제품 디버그 메시지 전체를 이 형식으로 바꾼 것은 아니다.

## 2. 한 번에 시험할 때의 순서

1. 정상 기동 상태에서 `--sys50`, `--sys51`로 표시를 확인한다.
2. 시험용 내부기를 연결해 Live에서 SYS-012 DIO35를 측정하고 `--sys23 start`로 알람을 확인한다. 알람이 끝나면 내부기와 매핑 앱을 분리한다.
3. `--sys77 run`과 SYS-054 명령으로 암복호화/CRC를 확인한다. 이때부터 정상 맵/절전 처리를 멈춘 **NOP HOLD** 상태다. 54/73/77/80 명령끼리는 사용할 수 있지만 Live로 자동 복귀하지 않는다.
4. SYS-080은 `arm → check → clear` 순서로 진행한다. 시험 사이에 재부팅할 필요가 없으며 실제 사용자 설정은 변경하지 않는다.
5. SYS-073은 한 오류 사례를 보고 재부팅한 뒤 다음 사례를 실행한다. 오류가 남아 있으면 새 오류 시험은 거절한다.
6. SYS-007을 실행하고 LED를 확인한다. 끝나면 재부팅한다. 다른 시험과 순서를 바꾸었다면 HOLD 상태에서 Live 시험을 시작하지 않도록 먼저 재부팅한다.

HOLD는 일반 전경 처리를 멈추고 현재/다음 PCM을 NOP로 요청한 상태다. CFX/Timer 인터럽트와 워치독은 계속 동작한다. 이미 전송된 자극을 취소하거나 물리적 무자극을 보증하는 기능이 아니므로 내부기 분리 조건을 지킨다. 보드에 다운로드하거나 재부팅하는 동작은 PC 검증 스크립트가 자동으로 수행하지 않는다.

| 공통 응답 | 해야 할 일 |
|---|---|
| `REJECT: disconnect ISD and mapping` | 실제 내부기와 매핑 연결을 끊는다. `--isd off`로 우회하지 않는다. |
| `REJECT: existing error` | 오류 원인을 확인하고 재부팅한다. 결과를 보기 위해 오류를 임의로 덮어쓰지 않는다. |
| `REJECT: LED override or power pattern` | 전원 패턴 종료와 강제 LED 설정 해제를 확인한다. |
| `INPUT ERROR` | 이 문서의 명령/인자 수를 확인한다. HOLD 진입 후 인자 오류를 확인할 수도 있으므로 `status`로 상태를 확인한다. |
| `REBOOT_REQUIRED` / `FS_UNCERTAIN` | 파일 닫기/저장 결과가 확실하지 않아 후속 파일 명령을 중단한 상태다. 로그를 보존하고 재부팅한다. 80번 백업은 삭제하지 않는다. |
| `LOG_FORMAT_ERROR` / `LOG_OVERFLOW` | 결과가 불완전하므로 해당 출력으로 판정하지 않는다. |

54/73/77/80의 `--sys번호 status`는 HOLD/PCM/감사 오류/LED 요청값을 보여준다. `--sys번호 stop`은 남은 로그만 취소한다. **실행을 마친 파일 변경이나 오류를 되돌리지 않으며 HOLD도 해제하지 않는다.** SYS80 시험 파일은 `--sys80 clear`로 정리한다. `restore`는 이전 버전의 실제 설정 손상 시험을 실행하여 백업이 남은 경우에만 사용한다.

## 3. SYS-012: 처리 시간 측정

CM3/CFX의 DIO35 상수를 모두 1로 빌드한다. 스코프 프로브는 DIO35, 기준은 보드 GND에 연결한다. Live Stimulation이 안정된 뒤 High 폭의 최대값과 반복 주기/누락을 확인한다. 별도 RTT 시작 명령은 없다.

High는 CFX `FIFO_4_ISR()` 진입, Low는 `pcmDataOut()` 반환 직전이다. 입력 준비 대기/연산/출력 FIFO 기록 등이 포함되며 실제 내부기 전극의 자극 시간은 아니다. 현재 요청 기준은 1ms 이하의 처리 시간 관찰이고, 코드가 허용오차나 합격을 자동 판정하지 않는다. 두 코어가 동시에 핀을 토글하지 않는다. [상세 조건과 한계](dio35.md)를 함께 참고한다.

## 4. SYS-023: 배터리 19% 알람 자극

실제 잔량 40~100%, 충전기 미연결, 매핑 미연결, 알림 ON, 시험용 내부기 Live 연결 상태에서 실행한다.

```text
--sys23 status
--sys23 start
```

알람 판단에만 19%를 넣는다. 실제 잔량/종료 판단은 바꾸지 않는다. `LOW_BATTERY_TRIGGER=1`, `CM3_ALARM_GATE`의 ON/OFF 3회, `DONE`을 확인하고 스코프로 현재 맵의 알람 자극을 확인한다. 약 80ms ON/OFF이며 종료/중단/5초 제한 때 시험 입력을 해제한다. 중단은 `--sys23 stop`이다. 시험값 때문에 절전되지 않지만 실제 저전력/충전/오류 보호는 유지된다. 현재 제품의 실제 19% 동작을 검증한 결과와는 다르다. [상세 사용법](sys23.md).

## 5. SYS-050/051: 감사기록 표시

```text
--sys50
--sys51
```

50번은 2026-05-29 10:00:00 연결/10:00:05 해제와 가상 주소 `02:24:A0:00:00:50`, 51번은 10:01:00 무결성 오류를 표시한다. `END` 뒤 다음 명령을 입력한다. `show`를 붙여도 같고 `stop`으로 출력을 취소한다.

매 행 `[TEST]`가 붙은 **RAM 표시 시험**이다. 실제 파일 생성/저장/연결 해제를 실행하지 않는다. 실제 해제 기록 호출과 무결성 오류 자동 기록의 기존 미비는 이 표시로 해소되지 않는다. [전체 출력 형식](audit.md).

## 6. SYS-054: 펌웨어 이미지 CRC

```text
--sys54 check 1 0
--sys54 header 1 0
--sys54 data 1 0
```

끝의 숫자는 `슬롯 번호 / APP 파일 번호`다. 위 예는 부트 드라이브 `0:/1/APP000.FEZ`를 읽는다. 슬롯은 1~4, 0은 공장 이미지 루트이며 APP 번호는 0~2다. 다른 슬롯/파일을 시험하려면 해당 숫자로 바꾼다. `OPEN` 오류가 나면 그 경로에 파일이 있는지와 실제 설치 구성을 확인한다.

| 명령 | 실행 내용 | 확인할 출력 |
|---|---|---|
| `check` | 헤더/섹션의 CRC와 파일 길이 검사 | `HEADER_RESIDUE=0000`, `CRC_CHECK=MATCH`, `CLOSE=0`, `UNMOUNT=0` |
| `header` | 먼저 원본 검사, 읽은 헤더의 RAM 한 비트만 변경 | `BASELINE=MATCH` 뒤 0이 아닌 헤더 잔여값, `CRC_CHECK=REJECT` |
| `data` | 먼저 원본 검사, 일반 데이터 섹션의 RAM 한 비트만 변경 | `RAM_FLIP=SEC...`, `CRC_BAD`, `CRC_CHECK=REJECT` |

파일 자체는 변경하지 않으며 메모리 적재/점프/재부팅도 하지 않는다. `BOOT=NOT_RUN`이므로 실제 손상 이미지의 기동 거부 증거는 별도로 필요하다. 이 CM3 검사기의 짧은 읽기/섹션 누락 거절을 기존 부트로더의 보호 동작으로 해석하지 않는다.

현재 명령은 **비암호화 FEZ**를 지원한다. 암호화 파일/섹션은 미지원으로 표시하고 통과시키지 않는다. VOLATILE 섹션은 부트로더가 CRC 오류를 무시하는 기존 예외가 있으므로 `PARTIAL`로 표시한다. 이런 경우 54번 전체 합격으로 기록하지 않는다. 키를 출력하거나 실행 이미지 자체를 손상시키는 명령은 제공하지 않는다.

## 7. SYS-073: 감사기록 생성/저장 실패

가장 먼저 확인하기 쉬운 저장 실패는 다음과 같다.

```text
--sys73 write open
--sys73 status
```

실제 `ci_event_log_write()`를 호출하되, 파일 입출력만 시험 전용 RAM 저장소로 연결한다. 원래 감사파일/감사 RAM/장치 시각/Bluetooth 주소는 변경하지 않는다. `RET=-1`, `ERROR=10/1`, `FAULT_REACHED=1`, `OBS=PRODUCT_ERROR`를 확인한다. 해당 오류가 기존 오류 표시 경로로 전달되어 빨간 LED 점멸 요청이 유지된다. 현재 패턴은 180ms ON/180ms OFF다. 실제 LED는 눈으로 확인하고 사진이 필요하면 직접 촬영한다.

| 명령 형식 | 사용할 오류 이름 |
|---|---|
| `--sys73 init 오류이름` | `open`, `read`, `short-read`, `write`, `short-write`, `sync`, `close` |
| `--sys73 write 오류이름` | 위와 같음 |
| `--sys73 led` | 저장 오류 코드를 직접 넣어 빨간 LED 표시만 확인 |

`open`은 파일 열기 실패, `read/write`는 읽기/쓰기 실패, `short-read/short-write`는 요청보다 적은 바이트 처리, `sync`는 저장 동기화 실패, `close`는 닫기 실패다. 한 사례에서 오류가 남으면 재부팅한 뒤 다음 사례를 실행한다.

- `CHECK=MATCH`: 해당 주입 지점에 도달했고 반환값/오류 코드가 비교 조건과 맞음.
- `CHECK=GAP`: 오류를 주입했지만 현재 제품 코드가 실패/오류 상태를 기대대로 전달하지 않음. 실제 불일치로 남긴다.
- `CHECK=NOT_REACHED`: 그 실행 경로에 해당 파일 연산이 없음. 예를 들어 기존 write 경로는 sync를 호출하지 않는다.
- `OBS=NO_PRODUCT_ERROR`: 제품 함수가 오류 코드를 등록하지 않았으므로 시험 코드가 대신 빨간 LED를 켜서 합격처럼 만들지 않는다.
- `--sys73 led`의 `SRC=EXPLICIT_LED_ERROR`는 직접 오류를 넣은 표시 시험이다. 실제 저장 실패 검출과는 별개다.

HOLD는 시험을 위해 미리 진입한 정지 상태다. `PRODUCT_STOP=NOT_PROVEN`은 실제 제품의 저장 실패 후 정지 정책까지 입증한 것은 아니라는 뜻이다. 현재 init/read/write/sync/close의 일부 오류 처리 공백은 그대로 관찰한다.

## 8. SYS-077: 원문/암호문/복원문 비교

```text
--sys77 run
```

가상 제조번호 7701과 사용자 이름 `SYS77_TEST`를 가진 132바이트 ISD 정보를 만든다. 실제 저장 래퍼의 CRC와 AES-128 ECB를 사용해 `1:/SYS77.TST`에 기록한 뒤 파일의 암호문 144바이트를 읽고, 실제 읽기 래퍼로 복호화/CRC를 확인한다.

`IN`은 원문 132바이트, `ENC`는 CRC/패딩까지 포함한 저장 암호문 144바이트, `OUT`은 복원문 132바이트다. 각 행의 숫자는 0부터 시작하는 바이트 위치이며 16바이트씩 이어진다. `IN`과 `OUT`의 모든 바이트가 같아야 한다. `WRITE=0`, `READ_CRC=0`, `SAME=1`, `CIPHER_CHANGED=1`, `FILE_ROUNDTRIP=MATCH`, `TEST_FILE_REMOVE=0`을 함께 확인한다.

쓰기의 `WR_SYNC N=1 RES=0`, `WR_CLOSE N=1 RES=0`과 읽기의 `RD_CLOSE N=1 RES=0`은 실제 파일 래퍼 안에서 관찰한 동기화/닫기 결과다. `RD_SYNC=NOT_RUN`은 읽기 경로에서 동기화를 호출하지 않았다는 뜻이다. 래퍼 반환이 0이어도 이 관찰에 실패하면 왕복 성공으로 처리하지 않고 `NOT_COMPLETE`로 중단한다. 닫기/동기화 실패 시 파일을 삭제하지 않으며 재부팅 전 다른 저장 시험도 거절한다.

실제 사용자 정보와 키는 출력하지 않는다. 시험 파일이 이미 있으면 덮어쓰지 않고 거절한다. 중간 실패로 파일이 남으면 파일 내용과 오류를 확인한 후 관리자가 시험 파일임을 확인하여 정리한다. 명령이 다른 기존 파일을 지우지는 않는다. 재부팅 후 저장 유지 시험은 이 왕복 검사와 별개다.

## 9. SYS-080: 시험 파일의 CRC 오류 검출과 공장 기본값 복원

시험 전용 `1:/SYS80.TST`를 정상 CRC와 함께 저장한 뒤 데이터만 손상시킨다. `check`가 저장 CRC와 계산 CRC의 불일치를 확인하고, 같은 시험 파일을 공장 기본값으로 복구한다. **재부팅 없이 함수 안에서 시험하며, 실제 사용자 설정/맵/게인은 바꾸지 않는다.** 내부기와 매핑을 분리하고 시험용 외부기에서 실행한다.

### 9.1 정상 파일 생성과 손상

```text
--sys80 arm
```

1. 사전값 `[2,2,5,2,2,1,2]`의 CRC를 실제 `ci_crc_ccitt_calc`로 계산한다.
2. 설정 28바이트, 저장 CRC 4바이트, 시험 파일 식별 표식 8바이트를 새 파일에 저장한다. 기존 파일이 있으면 덮어쓰지 않는다.
3. 저장 바이트와 실제 제품 읽기 함수의 CRC 검사 성공을 확인한다.
4. 파일의 첫 바이트만 바꿔 맵 번호를 2에서 3으로 만든다. 저장 CRC는 바꾸지 않는다.
5. 바이트 변경과 CRC 불일치를 확인하고 `ARMED`를 출력한다. 로그가 끝나면 재부팅하지 않고 `check`를 입력한다.

`test.create fr=8`은 파일이 이미 있다는 뜻이다. 이전 시험 파일이면 아래 `clear` 후 다시 시작한다. `ARM FAILED`나 I/O 오류는 준비 실패이므로 복구 시험 통과로 판단하지 않는다.

### 9.2 CRC 비교와 기본값 복구

```text
--sys80 check
```

`check`는 파일을 새로 읽어 저장 CRC와 현재 데이터로 계산한 CRC를 출력한다. 실제 제품 읽기 함수가 CRC 오류를 반환하는지도 확인한 뒤, 아래 기본값을 대입하고 CRC를 다시 계산해 저장한다. 저장 후 파일을 다시 열어 설정값과 CRC를 검증해야 `CHECK=PASS`가 나온다.

| 로그 항목 | 설정 | 손상 파일의 값 | 복구 기본값 |
|---|---|---:|---:|
| MAP | 맵 번호 | 3 (정상 저장 시 2) | 1 |
| STIM_VOL | 자극 볼륨 | 2 | 4 |
| AUDIO_VOL | 오디오 볼륨 | 5 | 1 |
| LED | LED 알림 | 2 (OFF) | 1 (ON) |
| STIM_ALERT | 자극 알림 | 2 (OFF) | 1 (ON) |
| TELECOIL | 텔레코일 | 1 (ON) | 2 (OFF) |
| BLE | BLE 설정 | 2 (OFF) | 1 (ON) |

기본값의 근거는 [ci_map.c](../../src/2__cm3/Gen1_5/FS/ci_map.c)의 `ci_map_init_map_data` 사용자 설정 초기화 부분이다. 실제 초기화 함수를 호출하여 제품 설정을 바꾸는 것이 아니라 그 기본값을 시험 파일에 사용한다. 호스트 검사는 두 기본값 정의가 같은지도 대조한다.

현재 시험 입력에서 정상 저장 CRC는 `84F6`, 첫 바이트를 바꾼 뒤 계산 CRC는 `A062`, 기본값 복구 후 CRC는 `33F7`이다. 아래는 PC에서 실제 시험 함수를 실행하여 확인한 예상 출력이며, 보드에서 수집한 로그는 아니다. 보드 시험에서는 실제 출력과 비교한다.

```text
[SYS-080] FILE=1:/SYS80.TST
[SYS-080] CRC STORED=84F6 CALC=A062
[SYS-080] READ check rc=-1 expect=-1
[SYS-080] CRC=MISMATCH; FACTORY_RESET=START
[SYS-080] MAP:3->1
[SYS-080] STIM_VOL:2->4
[SYS-080] AUDIO_VOL:5->1
[SYS-080] LED:2->1
[SYS-080] STIM_ALERT:2->1
[SYS-080] TELECOIL:1->2
[SYS-080] BLE:2->1
[SYS-080] RESET CRC STORED=33F7 CALC=33F7
[SYS-080] READ reset rc=0 expect=0
[SYS-080] DEFAULTS=MATCH; SAVE=VERIFIED
[SYS-080] CHECK=PASS; test file defaults restored
```

- `CRC=MISMATCH; FACTORY_RESET=START`: 파일 전체를 읽었으며 두 CRC가 달라 기본값 복구를 시작함.
- `MAP:3->1` 등: 손상 파일 값에서 기본값으로 바꾸는 항목별 정보. 아직 저장 성공을 뜻하지 않음.
- `RESET CRC STORED=... CALC=...`: 복구 후 다시 읽은 저장 CRC와 계산 CRC. 둘이 같아야 함.
- `DEFAULTS=MATCH; SAVE=VERIFIED`, `CHECK=PASS`: 시험 파일의 기본값 저장과 재확인 완료.
- `CRC=MATCH; CHECK=NO_RESET`: CRC가 일치하여 파일을 그대로 유지함. 복구 후 다시 `check`하면 이 결과가 나옴.
- `CHECK=FAIL`, `IO ...`, `REJECT ...`: 파일 접근/길이/식별/검증 실패. CRC 오류로 단정해 초기화하지 않음.

이 PASS는 **시험 파일의 CRC 검출/복구 함수 시험** 결과다. 실제 부팅 경로, 전원 차단 후 저장 유지, CFX 맵/알림/BLE 적용이나 제품 전체 공장초기화까지 시험한 결과는 아니다.

### 9.3 정리와 반복 시험

```text
--sys80 clear
--sys80 arm
--sys80 check
```

`clear`는 이 명령으로 만든 시험 파일만 지운다. 다른 이름의 파일, 식별할 수 없는 기존 파일과 과거 백업은 삭제하지 않는다. 생성 도중 실패한 파일도 같은 부팅에서 직접 만든 파일로 확인되면 정리할 수 있다. 재부팅 뒤 식별 표식까지 손상된 파일은 자동 삭제하지 않으므로 시험 파일인지 먼저 확인해야 한다.

닫기 오류가 나면 후속 파일 명령을 막는다. 로그를 보존하고 재부팅한 뒤 상태를 확인한다. 정상 시험의 `arm/check/clear` 사이에는 재부팅이 필요 없지만, 모든 시험을 마치고 일반 동작으로 돌아갈 때는 공통 NOP HOLD 해제를 위해 재부팅한다.

### 9.4 이전 버전의 백업이 남은 경우에만 원복

```text
--sys80 restore
```

이전 버전의 sys80으로 실제 `1:/ISD1_USER_SETTING`을 손상시켜 `1:/SYS80.BAK`가 남은 경우에만 사용한다. **이 호환 명령은 실제 사용자 설정을 쓴다.** 백업의 길이/검사값과 원본 CRC를 확인하고 전체 바이트 원복/재확인이 성공해야 백업을 삭제한다. `RESTORED` 뒤 재부팅하여 원래 설정을 적용한다. 복원 실패 시 백업은 남긴다. 새 시험 전용 파일 방식에서는 `restore`를 실행할 필요가 없다.

## 10. SYS-007: 전하량 계산/오류 LED 시험

CFX 처리 시간의 오실로스코프 관찰은 [SYS-012 DIO35 사용법](dio35.md)을 참고한다. 아래 SYS-007 전하량/LED 명령과는 별도 시험이다.

배터리 19%의 알람 자극 요청은 [SYS-023 사용법](sys23.md)을 참고한다. SYS-007 실행 후에는 NOP 유지 상태이므로 SYS-023 전에 재부팅한다.

고정 날짜/가상 주소의 감사기록 표시 명령은 [SYS-050/051 사용법](audit.md)을 참고한다. 실제 감사파일과 자동 기록 기능은 변경하지 않는다.

`--sys7 t=100 c=300 w=251`을 입력하면 C 전류와 한 상의 펄스폭으로 전하량을 계산한다. 75,000 pC보다 크면 초과량 경고와 빨간 LED 점멸을 요청하고, 이하면 `CHARGE_CHECK=PASS`를 출력한다. 이 예에서는 75,300 pC이므로 300 pC 초과 경고가 나온다.

현재 제품의 전하량 보호 문제는 수정하지 않았다. **새 명령의 PASS는 시험 코드의 전하량 비교 결과이며, 정상 맵의 보호 기능이나 시스템 시험 전체가 통과했다는 뜻은 아니다.** 시험 코드가 초과를 판단한 뒤 기존 오류 처리/LED 표시를 연결하여 확인하는 범위다. 실제 제품 계산 결과를 확인하는 기존 `charge`/`dac` 명령도 그대로 남아 있다.

### 10.1 준비와 켜기

1. 시험용 외부기만 사용하고 착용 제품/이식 내부기를 연결하지 않는다. 내부기와 매핑 앱을 분리한다. 시험 중 다시 연결하지 않는다.
2. [processorDirective.h](../../src/2__cm3/processorDirective.h)의 `TDC_SYS_TEST_7_ENABLE`을 `1`로 빌드하거나 컴파일 옵션에 `-DTDC_SYS_TEST_7_ENABLE=1`을 넣는다. 2026-10-13 현재 작업본에서 사용자가 켜 둔 `1`은 유지했다. 시험 종료 후에는 `0`으로 빌드한다. `ENABLE_UI_CMD`가 필요하며 `TDC_UNIT_TEST`는 필수가 아니다.
3. 시험 빌드를 적용한 외부기가 정상 모드로 기동한 다음 SEGGER RTT Viewer의 Terminal 0에서 명령을 입력한다. 전원 ON 패턴이 끝날 때까지 기다린다. 기존 유닛 시험 전용 콘솔을 동시에 실행하지 않는다.
4. `--sys7 status`로 응답을 확인하고 아래 명령을 한 번씩 실행한다. 출력이 끝나기 전에 다음 시험을 입력하면 `BUSY`로 거절한다. 시험 중에는 입력 에코/프롬프트가 로그 앞에 섞이지 않게 생략된다.

명령은 시작할 때 실제 내부기 연결, 매핑 연결, 기존 오류, 오류 LED 강제 제어와 진행 중인 전원 패턴을 확인한다. 조건이 맞지 않으면 `REJECT`로 이유를 출력한다. `--isd off` 같은 표시용 명령으로 연결 조건을 우회할 수 없다.

### 10.2 명령과 확인할 결과

#### T/C/펄스폭을 직접 입력하는 시험

T/C는 uA, w는 한 상의 펄스폭 us이며 정수만 입력한다. 키 순서는 자유지만 t/c/w를 각각 한 번씩 넣어야 한다. 입력 범위는 `0 <= t <= c <= 65535`, `1 <= c,w <= 65535`다. 이 범위는 콘솔 계산용이며 실제 맵이나 DAC의 출력 가능 범위를 뜻하지 않는다. 부호, 소수점, 단위 문자, 6자리 이상의 숫자, 누락/중복 키는 거절한다. 128바이트 입력 버퍼를 넘는 행이나 중간 NUL 문자가 있는 행도 전체를 폐기한다.

| 입력 명령 | 계산 전하량 | 메시지와 LED |
|---|---:|---|
| `--sys7 t=100 c=300 w=249` | 74,700 pC | `CHARGE_CHECK=PASS`, 새 오류 점멸 없음 |
| `--sys7 t=100 c=300 w=250` | 75,000 pC | 한도와 같으므로 `CHARGE_CHECK=PASS`, 새 오류 점멸 없음 |
| `--sys7 t=100 c=300 w=251` | 75,300 pC | `CHARGE_CHECK=WARN EXCESS=300 pC`, 빨간 점멸 5초 요청 |

전하량은 `C x w`이며 채널 수나 양상의 수를 곱하지 않는다. T는 C 이하인지 확인하며, 검사에는 최대 전류인 C를 쓴다. T/C 값을 실제 맵이나 자극 출력에 적용하지 않는다. `C=2500, w=25`도 전하량만 보면 62,500 pC로 PASS이지만, DAC 범위까지 적합하다는 뜻은 아니다. 그 구분을 위해 로그에 `SCOPE=charge only; DAC/map not checked`를 출력한다.

경고 뒤 다음 입력을 시험하려면 5초 종료 안내를 기다리거나 `--sys7 stop`을 입력한 뒤 로그 배출을 기다린다. 관찰 중에는 새 시험을 거절한다. 입력 오류는 `INPUT ERROR`, 시험 전 연결/오류 조건 미충족은 `REJECT`로 구분한다.

#### 기존 계산 확인/표시/상태 명령

| 명령 | 입력/동작 | 현재 코드에서 확인할 내용 |
|---|---|---|
| `--sys7 charge` | 32채널, T=100 uA, C=300 uA, 한 상의 펄스폭 251 us | 75,300 pC > 75,000 pC. 현재는 계산 성공/오류 없음이므로 `MISMATCH`. 빨간 LED를 강제로 켜지 않음 |
| `--sys7 dac` | 32채널, T=100 uA, C=2500 uA, 25 us | DAC 범위 초과로 계산 실패. `REJECT`와 계산 기준 `MATCH`. LED 시험/시스템 전체 통과가 아님 |
| `--sys7 led` | 전하량 초과 오류 코드 직접 주입 | `SOURCE=INJECTED`. 기존 오류 처리로 빨간 점멸 요청. 5초 동안 실제 빨간 점멸을 관찰 |
| `--sys7 status` | 현재 상태 조회 | `HOLD`는 시험 정지 상태, `OBS`는 오류 관찰 중 여부. 각각 1이면 해당 상태임 |
| `--sys7 stop` | 시험이 만든 오류 표시 중단 | RTT 버퍼가 막혀도 즉시 처리. 로그만 기존 출력 뒤에 보냄. 자극 동작은 재개하지 않음 |

시험에 진입하면 일반 맵/ISD 제어와 자동 절전 처리를 멈추고 현재/다음 PCM 모드를 NOP로 유지한다. 직접 입력은 지역 변수의 전하량만 계산한다. 기존 `charge`/`dac`는 전용 RAM 배열을 사용하고 계산 뒤 DAC 설정을 복원한다. 시험 T/C 값은 공유 맵/CFX 출력 배열/NVM/내부기 설정에 적용하지 않는다. Timer 3와 CFX 인터럽트, 워치독은 계속 동작한다.

NOP 요청은 이미 전송된 패킷을 취소하거나 물리적인 무자극을 확인하는 기능이 아니다. 그래서 내부기를 분리한 상태가 필수다. 시험 중 일반 `--prog`, `--err`, `--gating` 등의 명령은 차단하며 `--sys7`, `--help`만 받는다. 제품의 일반 전원/매핑 동작을 함께 시험하는 모드가 아니다.

입력값 초과와 `led`는 현재 코드의 빨강 180 ms ON/180 ms OFF 요청을 사용한다. 실제 색상/점멸 확인은 시험자가 한다. 5초가 지나거나 `stop`을 받으면 시험이 소유한 오류만 제거한다. 중간에 다른 오류가 발생하면 그 오류를 지우지 않는다. RTT 연결이 끊겨도 Timer 3가 계속 동작하면 5초 종료는 진행된다.

**종료 후에도 NOP 정지는 유지된다. 정상 사용으로 돌아가려면 외부기를 재부팅한다.** 맵/자극을 자동 재개하지 않으며, 디버거로 CPU를 멈추면 5초 타이머도 정지할 수 있다. 시험이 끝나면 매크로를 `0`으로 되돌린 정상 빌드를 사용한다.

### 10.3 로그 읽기

아래는 현재 소스를 PC에서 실행해 확인한 출력 형식이며 실제 외부기에서 수집한 로그가 아니다.

`--sys7 t=100 c=300 w=251`:

```text
[SYS-007] CASE=input SOURCE=TEST_CHARGE
[SYS-007] IN T=100 C=300 uA W=251 us
[SYS-007] CHARGE=75300 LIMIT=75000 pC
[SYS-007] SCOPE=charge only; DAC/map not checked
[SYS-007] CHARGE_CHECK=WARN EXCESS=300 pC
[SYS-007] LED_REQ=13 RED_MAP=13
[SYS-007] VISUAL=CHECK_REQUIRED HOLD_MS=5000
[SYS-007] NOP hold; reboot to resume; no system PASS
```

`--sys7 t=100 c=300 w=250`:

```text
[SYS-007] CASE=input SOURCE=TEST_CHARGE
[SYS-007] IN T=100 C=300 uA W=250 us
[SYS-007] CHARGE=75000 LIMIT=75000 pC
[SYS-007] SCOPE=charge only; DAC/map not checked
[SYS-007] CHARGE_CHECK=PASS
[SYS-007] LED_REQ=0 RED_MAP=13
[SYS-007] NOP hold; reboot to resume; no system PASS
```

정상 제품 계산의 기존 불일치를 확인하는 `--sys7 charge`:

```text
[SYS-007] CASE=charge SOURCE=ACTUAL_CALC
[SYS-007] IN T=100 C=300 uA PW=251 us N=32
[SYS-007] CHARGE=75300 LIMIT=75000 pC
[SYS-007] CHARGE_GUARD=DISABLED
[SYS-007] CALC=ACCEPT EXPECT=REJECT
[SYS-007] DATA_ERR=0 EXPECT=3
[SYS-007] CHECK=MISMATCH; calculation only
[SYS-007] LED_REQ=0 RED_MAP=13
[SYS-007] NOP hold; reboot to resume; no system PASS
```

- `SOURCE=TEST_CHARGE`는 시험 명령에 추가한 전하량 비교다. `CALC`는 기존 `charge`/`dac`에서 호출한 제품 계산 반환값이며 `ACCEPT`는 성공 반환, `REJECT`는 실패 반환이다.
- `DATA_ERR=0`은 데이터 처리 오류 없음, `3`은 전하량 초과 오류다.
- `LED_REQ=0`은 오류 LED 요청 없음이다. 현재 `RED_MAP=13`과 같은 값이면 기존 맵 오류의 빨간 점멸 요청이다. GPIO/LED를 실측했다는 의미는 아니다.
- `CHECK`는 해당 계산의 반환값/오류 코드와 사전 기준 비교만 나타낸다. 시스템 시험 전체의 합격 표시가 아니다.
- `VISUAL=CHECK_REQUIRED`는 시험자가 직접 LED를 봐야 한다는 뜻이다. 빨간 LED 표시를 독립 확인하려면 `--sys7 led`를 실행하고, 보고서에도 오류를 주입한 시험임을 구분한다.

시험 로그 한 줄은 CRLF 포함 60바이트 이내다. RTT 공간이 부족하면 행 전체를 보관하고 다음 poll에서 재시도한다. PC 검사에서 현재 명령의 최장 줄은 54바이트다. 기존 UI 도움말/제품 디버그 로그 전체를 60바이트로 바꾼 것은 아니다.

### 10.4 SYS-007 개발자 검사

저장소 루트에서 실행한다. SDK 빌드는 Eclipse Debug 산출물을 건드리거나 보드에 다운로드하지 않는다.

```powershell
python tests/system/check_sys7.py
python tests/unit/build_target_tests.py CM3 --normal
python tests/unit/build_target_tests.py CM3 --normal --system-test-7
python tests/unit/build_target_tests.py CM3 --system-test-7
```

두 번째 명령은 SYS-007 OFF, 세 번째는 SYS-007 ON, 네 번째는 기존 유닛 시험과 SYS-007을 함께 켠 빌드다. 이 검사 도구는 헤더의 현재 값과 관계없이 컴파일 옵션으로 0/1을 명시하며 헤더를 쓰지 않는다. 생성 위치는 `tmp/unit-tests/target/CM3/normal`, `normal/system7`, `test/system7`이다.

호스트 검사는 실제 계산/오류/LED 요청 함수와 실제 UI의 입력 분리/명령 호출 경로를 사용한다. RTT 송수신과 시간을 PC 입력으로 대신하며 다른 UI의 파일/설정 함수가 호출되면 실패하게 한다. 현재의 비활성 전하량 검사와, PC에서만 검사 매크로를 켰을 때의 기존 성공 반환 문제도 별도로 재현한다. 제품 설정은 변경하지 않는다. 입력 파싱/최대 곱/한도 경계, 연결/기존 오류/전원 패턴 거절, DAC 상태/공유 자료 보존, 전송 지연, 중복 명령, tick 순환, 중단/5초 종료/재실행/다른 오류 보존과 긴 행/NUL/CRLF 처리를 검사한다.

검사 결과는 `tmp/system-tests/sys7/checks.json`에 남는다. SDK 빌드와 PC 검사는 실제 RTT 접속, 전송 타이밍, GPIO의 색상/점멸 및 물리적 무자극을 증명하지 않는다. 실기 시험 결과와 사진은 별도로 확보해야 한다. 이번 구현의 판단/검증 기록은 [설계 출력 작업 기록](../../docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/작업.md)에 있다.

## 11. 저장/무결성 명령의 개발자 검사

다음 명령은 PC의 RAM 파일 경계에서 실제 시험 함수/제품 함수를 실행한다. 외부기에 연결하거나 장치 파일을 변경하지 않는다. SDK와 호스트 GCC가 필요하다.

```powershell
python tests/system/check_sys_storage.py
python tests/system/check_sys_storage.py --optimization O2
python tests/system/check_sys73.py
python tests/system/check_sys80.py
python tests/system/check_sys7.py
python tests/system/check_sys23.py
python tests/system/check_sys_audit.py
```

SDK 빌드는 모든 시스템 시험 OFF, 새 시험 각각 ON, 기존 시험과 모두 ON을 나눠 확인한다. 한 빌드를 실행 중인 동안 같은 출력 폴더의 빌드를 다시 실행하지 않는다.

```powershell
python tests/unit/build_target_tests.py CM3 --normal
python tests/unit/build_target_tests.py CM3 --normal --system-test-54
python tests/unit/build_target_tests.py CM3 --normal --system-test-73
python tests/unit/build_target_tests.py CM3 --normal --system-test-77
python tests/unit/build_target_tests.py CM3 --normal --system-test-80
python tests/unit/build_target_tests.py CM3 --normal --system-test-7 --system-test-23 --system-test-audit --system-test-dio35 --system-test-54 --system-test-73 --system-test-77 --system-test-80
python tests/system/verify_sys_storage_builds.py --before 'docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/qa/sys-storage-20261016/before'
```

현재 소스/헤더와 검사 기록의 해시가 달라지면 다시 검사한다. 결과는 `tmp/system-tests/storage`, `sys73`, `sys80`에 기록한다. `HOST_PASS`나 빌드 성공은 검사 도구가 약속한 동작과 일치한다는 의미다. 73번 제품의 `GAP`, 54번의 미지원/부분 검사와 실기 미실행 상태를 시스템 시험 합격으로 바꾸지 않는다.
