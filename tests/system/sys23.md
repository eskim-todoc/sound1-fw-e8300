# SYS-023 배터리 저전력 알람 자극 시험

`--sys23 start`는 **알람 판단에만 잔량 19%를 넣어 기존 알람 자극 한 회차를 요청**한다. 시험용 19%는 전원 종료 판단에 전달하지 않으므로 이 입력 때문에 절전으로 들어가지 않는다. 실제 잔량과 LED/무선 조회 값은 그대로다.

현재 제품 코드는 40% 미만에서 종료를 시작하므로 실제 19%를 그대로 넣으면 알람보다 종료가 먼저다. 이번 명령은 그 제품 정책을 고치지 않고 알람 경로만 시험한다. 실제 잔량이 낮아지거나 충전/오류/전원 종료 조건이 생기면 시험 입력을 해제하고 정상 제어를 우선한다. **시스템 전체가 실제 19%에서 정상 동작한다는 검증으로 사용하지 않는다.**

## 준비와 실행

실제 자극을 요청하는 명령이다. 착용 제품이나 이식 내부기가 아닌 **시험용 내부기 지그**에서만 실행한다.

1. CM3 [processorDirective.h](../../src/2__cm3/processorDirective.h)의 `TDC_SYS_TEST_23_ENABLE`을 `1`로 빌드한다. 최초 추가 값은 `0`이며, 현재 작업본의 사용자 설정은 [통합 사용방법](README.md)과 실제 헤더에서 확인한다. `ENABLE_UI_CMD`는 필요하고 `TDC_UNIT_TEST`는 필요하지 않다. CFX에는 이번 시험을 위한 별도 상수가 없다.
2. 실제 배터리 잔량 40~100%, 충전기 미연결 상태에서 정상 기동한다. 내부기를 연결해 Live 자극이 가능한 상태로 만들고 매핑 앱은 분리한다. 기존 오류나 전원 LED 패턴이 없어야 한다.
3. 자극 알림 사용 설정이 ON인지 확인한다. 알람의 채널/크기는 현재 맵을 그대로 쓰며 명령이 임의 변경하지 않는다. 첫 관찰에는 기본 맵 1처럼 알람 채널을 스코프로 찾기 쉬운 조건을 사용한다. 알람 채널은 1~32, 계산된 알람 코드값은 1~255여야 한다.
4. SEGGER RTT Viewer의 Terminal 0에서 아래 명령을 한 줄씩 입력한다. 로그가 끝나기 전에 다음 명령을 넣으면 `BUSY`가 나올 수 있다.

```text
--sys23 status
--sys23 start
```

정상 알람 함수가 약 80 ms ON/OFF를 3회 처리하고 한 회차가 끝나면 시험 입력이 자동 해제된다. 5초가 지나도 끝나지 않으면 다음 정상 루프에서 시험 입력을 해제한다. 이 시간은 전경 실행에 의존하며 하드웨어 차단 타이머가 아니다.

| 명령 | 동작 |
|---|---|
| `--sys23 start` | 알람 판단용 19% 주입, 한 회차 관찰 시작 |
| `--sys23 status` | IDLE/ARMED/OBSERVING 상태, 실제 잔량, 알람 판단 잔량, 알림 설정, PCM 모드 조회 |
| `--sys23 stop` | 시험 입력 즉시 해제. 이미 진행 중인 알람은 기존 함수에서 끝날 수 있음 |

반복 시험은 `DONE`과 로그 출력이 끝난 뒤 `start`를 다시 입력한다. 중간에 `stop`했다면 기존 알람이 끝난 뒤 다시 시작한다. `stop`은 실제 자극의 긴급 정지나 이미 송신된 패킷의 취소를 보장하지 않는다.

SYS-007을 실행한 장치는 NOP 유지 상태이므로 먼저 재부팅한다. SYS-023 실행 중에는 맵 변경/오류 주입 등 다른 UI 명령이 차단된다. 기존 유닛 시험 콘솔을 동시에 실행하지 않는다. 시험 후에는 `TDC_SYS_TEST_23_ENABLE=0`으로 빌드한다.

## 로그에서 확인하는 것

아래는 구현된 형식의 **예상 로그 일부**이며 실제 장치 수집 로그가 아니다. 실제 잔량과 맵 채널/코드값은 별도 행에서 현재 값을 출력한다. 각 시험 로그는 CRLF를 포함해 최대 60바이트다. RTT 버퍼가 부족하면 행 전체를 보류해 다음 poll에서 재시도한다.

```text
[SYS-023] START SOURCE=TEST_ALARM_INPUT
[SYS-023] LOW_BATTERY_TRIGGER=1
[SYS-023] CM3_ALARM_GATE=1
[SYS-023] CM3_ALARM_GATE=0
[SYS-023] CM3_ALARM_GATE=1
[SYS-023] CM3_ALARM_GATE=0
[SYS-023] CM3_ALARM_GATE=1
[SYS-023] CM3_ALARM_GATE=0
[SYS-023] DONE: CM3 alarm ended; input restored
```

`LOW_BATTERY_TRIGGER=1`은 기존 `systemControl`이 알람을 요청했다는 뜻이다. `CM3_ALARM_GATE`는 기존 알람 함수가 CFX 공유 출력 플래그를 바꾼 관찰값이다. `DONE`은 CM3 알림 처리가 끝났다는 뜻이지 전극 출력이나 시스템 시험의 합격 판정이 아니다. 스코프에서 현재 맵의 알람 채널, 전류와 반복 파형을 실제로 확인한다. 일반 음성 자극은 계속될 수 있으므로 무음 등 기준서의 입력 조건을 맞춘다.

시작 조건이 맞지 않으면 `REJECT`, 실행 도중 조건이 바뀌면 `ABORT`, 5초가 지나면 `TIMEOUT`과 이유를 출력한다. 명령 자체는 맵/NVM/실제 배터리 상태를 저장하거나 변경하지 않는다. 기존 `--batt 19`는 LED 표시용이며 이 명령의 대체가 아니다.

## 구현과 PC 검증

- [시험 명령/관찰 코드](../../src/2__cm3/Cortex-M3-src/test/tdc_sys_test_23.c)
- [알람 입력 적용](../../src/2__cm3/Cortex-M3-src/systemControl/systemControl.c), [정상 전경 연결](../../src/2__cm3/Cortex-M3-src/main.c)
- [기존 알람 함수](../../src/2__cm3/Cortex-M3-src/internalDevice/indicatorByStimul.c)

저장소 루트에서 아래 명령으로 PC 검사/격리된 SDK 빌드를 수행한다. 보드에 다운로드하거나 자극하지 않는다.

```powershell
python tests/system/check_sys23.py
python tests/unit/build_target_tests.py CM3 --normal
python tests/unit/build_target_tests.py CM3 --normal --system-test-23
python tests/unit/build_target_tests.py CM3 --normal --system-test-7 --system-test-23 --system-test-dio35
python tests/unit/build_target_tests.py CM3 --system-test-7 --system-test-23
python tests/system/verify_sys23_builds.py --before 'docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/qa/sys23-command-20261014/before'
```

호스트 검사는 실제 명령 파서, 전원/알람 판단, 알림 함수를 실행하고 RTT/시간/장치 경계만 대체한다. 실제 자극 파형, 전류, 시간과 장치에서의 절전 미진입은 실기 확인 대상으로 남는다.
