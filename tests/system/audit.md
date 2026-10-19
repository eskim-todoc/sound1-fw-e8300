# SYS-050/051 감사기록 표시 명령

2026-05-29의 고정 입력으로 감사기록의 표시 형태를 확인한다. 실제 연결/해제나 파일 손상을 일으키지 않고, NVM 감사파일/장치 시간/현재 Bluetooth 주소를 변경하지 않는다. 각 행의 `[TEST]`는 RAM 시험 데이터라는 뜻이다. 자동 기록 기능의 합격 근거로 사용하지 않는다.

## 실행

1. CM3 [processorDirective.h](../../src/2__cm3/processorDirective.h)의 `TDC_SYS_TEST_AUDIT_ENABLE`을 `1`로 바꿔 빌드한다. 최초 추가 값은 `0`이며 현재 작업본의 사용자 설정은 [통합 사용방법](README.md)과 실제 헤더에서 확인한다. 기존 `ENABLE_UI_CMD`를 사용하며 CFX 설정이나 유닛 시험 설정은 필요하지 않다.
2. SEGGER RTT Viewer Terminal 0에서 아래 명령을 한 줄씩 입력한다. `END`를 확인한 뒤 다음 명령을 입력한다.

```text
--sys50
--sys51
```

`--sys50 show`, `--sys51 show`도 같다. 출력 도중 취소는 해당 번호의 `--sys50 stop` 또는 `--sys51 stop`이다. RTT 용량이 부족하면 행 전체가 다음 poll로 보류된다. stop도 출력 가능한 시점에 `STOPPED`를 표시하며 취소한 나머지 행은 출력하지 않는다.

SYS-007 실행 후 HOLD 상태이면 먼저 재부팅한다. SYS-023이 동작 중이면 종료를 기다린다. 이 명령은 정상 전원 제어를 정지하지 않으며, 시험 후에는 새 상수를 `0`으로 되돌린다.

## 출력 내용

50번은 시험 주소 `02:24:A0:00:00:50`의 10:00:00 연결과 10:00:05 해제를 차례로 출력한다. 51번은 10:01:00의 무결성 오류 이벤트를 출력한다. 아래 내용은 PC에서 실제 출력 코드를 실행해 확인한 고정 시험 벡터이며 과거 장치에서 수집한 기록이 아니다.

```text
[SYS-050][TEST] RECORDS=2 STORAGE=RAM
[SYS-050][TEST] <0> TIME {26-05-29-10-00-00}
[SYS-050][TEST] <0> ADDR {02:24:A0:00:00:50}
[SYS-050][TEST] <0> EVENT {00000040} CONNECTED
[SYS-050][TEST] <1> TIME {26-05-29-10-00-05}
[SYS-050][TEST] <1> ADDR {02:24:A0:00:00:50}
[SYS-050][TEST] <1> EVENT {00000032} DISCONNECTED
[SYS-050][TEST] END

[SYS-051][TEST] RECORDS=1 STORAGE=RAM
[SYS-051][TEST] <0> TIME {26-05-29-10-01-00}
[SYS-051][TEST] <0> ADDR {00:00:00:00:00:00}
[SYS-051][TEST] <0> EVENT {000000F0} INTEGRITY_ERROR
[SYS-051][TEST] END
```

TIME은 기존 형식과 같은 `년-월-일-시-분-초`이며 `26`은 2026년이다. ADDR은 기존 바이트 순서를 유지한다. 51번의 0 주소는 연결 상대가 없는 시험 조건이다. 파일명이나 손상 위치는 기존 16바이트 감사기록의 필드가 아니므로 실제 저장 정보처럼 추가하지 않았다.

기존 `--dump_log`와 같은 TIME/ADDR/EVENT 필드를 쓰되 한 번의 RTT 출력이 길어지지 않도록 세 줄로 나누고 같은 기록 번호를 붙였다. 각 행은 CRLF 포함 60바이트 이하, 현재 가장 긴 행은 54바이트다.

## 실제 자동 기록과 구분할 사항

2026-10-15 소스 확인 기준:

- [연결 처리](../../src/2__cm3/Cortex-M3-src/BleCommunication/remoteControl.c)는 시간/주소를 받은 후 `0x40`을 기록한다.
- [해제 처리](../../src/2__cm3/Cortex-M3-src/BleCommunication/ble_communication.c)의 `0x32` 쓰기 호출은 주석 상태다.
- `0xF0`의 실제 쓰기 호출은 [UI 수동 명령](../../src/2__cm3/Gen1_5/ui/tdc_ui_command.c)의 `--write_integrity_err`에서만 확인했다. 손상 감지 후 자동 저장 경로는 확인되지 않았다.

따라서 `--sys50/51`은 표시 확인용이다. 기존 `--write_integrity_err`는 실제 감사파일을 변경하는 별개 명령이므로 이번 사용 절차에 넣지 않는다. 현재 두 시험의 자동 기록 요구와 소스의 차이는 그대로 남아 있다. 무선 연결, CRC 검사, 저장 유지와 재기동 검증을 이번 출력으로 대신하지 않는다.

## PC 검사

```powershell
python tests/system/check_sys_audit.py
python tests/unit/build_target_tests.py CM3 --normal
python tests/unit/build_target_tests.py CM3 --normal --system-test-audit
python tests/unit/build_target_tests.py CM3 --normal --system-test-audit --system-test-7 --system-test-23 --system-test-dio35
python tests/system/verify_sys_audit_builds.py --before 'docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/qa/sys50-51-command-20261015/before'
```

실제 UI 파서와 표시 코드를 PC에서 실행한다. 제품 파일 쓰기/주소 변경 함수는 호출되면 시험을 실패시키는 경계로 대체한다. 이 검사는 보드 다운로드나 제품 시스템 시험 합격을 뜻하지 않는다.
