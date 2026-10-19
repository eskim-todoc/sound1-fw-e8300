# SYS-012 DIO35 처리 시간 측정

CFX의 PCM FIFO 인터럽트부터 `pcmDataOut()` 완료까지 DIO35를 High로 유지한다. 안정된 Live Stimulation에서 **High 폭의 최대값과 펄스의 누락 여부**를 확인한다. 코드/PC 검증은 끝났지만 실제 오실로스코프 측정 결과는 아직 없다.

## 켜고 끄기

두 프로젝트에서 `TDC_SYSTEM_TEST_DIO35_ENABLE`을 각각 설정한다. 처음 추가한 값은 모두 `0`이며 SYS-012 측정에는 양쪽 모두 `1`이 필요하다. 2026-10-16 작업 시작 시 CM3/CFX 모두 `1`인 사용자 설정을 확인하고 보존했다. `TDC_UNIT_TEST`와 관계없이 사용할 수 있으며 컴파일 옵션 `-DTDC_SYSTEM_TEST_DIO35_ENABLE=1`로도 선택할 수 있다. 다른 시험과 함께 진행할 때는 [통합 사용방법](README.md)을 참고한다.

| CM3 | CFX | 동작 |
|---|---|---|
| 0 | 0 | 기존 동작. DIO35 초기화/표시 코드 없음 |
| 1 | 1 | SYS-012. CM3가 출력 Low로 초기화, 이후 CFX가 High/Low 표시 |
| 1 | 0 | CM3가 출력 Low로 초기화만 함. 향후 CM3 시험에서 사용 가능 |
| 0 | 1 | 사용하지 않음. 출력 모드 초기화가 없으므로 측정 불가 |

- CM3 설정: [processorDirective.h](../../src/2__cm3/processorDirective.h)
- CFX 설정: [tdc_sys_test_config.h](../../src/1__cfx/test/tdc_sys_test_config.h)

**현재 보드의 DIO35는 GPIO 출력으로 사용할 수 있는 미사용 핀/테스트 패드다**(은수님 확인, 2026-10-14). 핀 연결 확인 대기는 해소했다. DIO35 자체는 EXTCLK 겸용이므로 다른 보드나 회로 변경 시에는 연결을 다시 확인한다. 고임피던스 프로브와 보드 GND를 사용하고 GPIO에 외부 전압을 인가하지 않는다. 시험은 착용 제품이 아닌 시험용 외부기/내부기 지그에서 한다. 이 확인은 실제 파형이나 1ms 충족을 측정한 결과는 아니다.

SYS-012 측정 중에는 CM3가 같은 핀을 토글하면 안 된다. 향후 CM3 시험에 사용할 때는 CFX 설정을 0으로 바꾸고 해당 CM3 시험 구간만 제어한다. 두 이미지의 상수는 독립적이므로 한쪽 변경만으로 다른 쪽이 자동 갱신되지 않는다. 시험 뒤에는 둘 다 0으로 빌드하고 재부팅한다. 매크로를 0으로 다시 빌드하는 것만으로 실행 중인 이전 이미지의 핀 설정이 되돌아가지는 않는다.

## 어디서 올라가고 내려가는가

| 위치 | 동작/의미 |
|---|---|
| CM3 `ci_dio_configure_normal()` 마지막 | QCC 핀 초기화 뒤 출력 래치를 Low로 내리고, DIO35를 GPIO 출력으로 전환. 초기 High 펄스를 피하기 위한 순서 |
| CFX `FIFO_4_ISR()` 첫 처리 | High. PCM 출력 FIFO의 다음 데이터 준비 요청 |
| CFX `pcmDataOut()` 반환 직전 | Low. 해당 모드의 계산/PCM FIFO 기록과 후속 링크 확인 완료 |

High 구간에는 입력 FIFO 준비 대기, 입력 혼합/빔포밍, AGC와 HEAR 연산, 자극 계산/PCM FIFO 기록 및 링크 상태 확인이 포함된다. 순수 CFX CPU 연산 시간은 아니다. 하드웨어 IRQ 발생부터 ISR의 High 쓰기까지 걸린 시간, Low 뒤의 함수 복귀와 실제 PCM 직렬 전송/내부기 출력 지연은 포함하지 않는다. 표시 자체의 소량 실행 시간도 추가된다.

## 오실로스코프에서 확인

1. 두 코어의 설정을 1로 빌드한 시험 이미지를 적용한다. SYS-007 명령이나 유닛 시험 실행기를 함께 실행하지 않는다. SYS-007을 한 번 실행한 상태는 NOP를 유지하므로 정상 Live 측정 전에 재부팅한다.
2. 내부기 지그 연결과 Live Stimulation 전환이 끝난 뒤 DIO35의 반복 파형을 관찰한다. 초기 연결, 모드 전환, 충전 및 절전 전후 파형은 Live 결과에 포함하지 않는다.
3. 상승 에지로 트리거하고 양의 펄스 폭과 상승 에지 사이 간격을 확인한다. 여러 블록을 관찰하여 평균만 아니라 최대 High 폭을 남긴다. 이번 요청의 확인값은 `1 ms 이하`다. 별도 시험 문서의 허용오차를 이 코드가 자동 적용하거나 통과 판정하지 않는다.
4. 상승 간격이 정상 블록 주기인 약 1ms인지, 누락/비정상 긴 High가 없는지도 확인한다. 같은 핀의 파형과 실제 측정 화면을 시험 근거로 저장한다.

모든 PCM 모드에서 같은 두 지점을 표시한다. 따라서 펄스가 나온다는 사실만으로 Live 모드라고 판단할 수 없다. 처리 중 다음 FIFO4가 발생하면 이미 High여서 새 상승 에지가 보이지 않는다. 직전 처리의 Low가 뒤 요청의 High를 지울 수도 있다. 짧게 보이는 펄스만으로 모든 블록의 마감 충족을 증명하지 않고, 누락/비정상 주기가 보이면 별도의 IRQ/완료 횟수 또는 PCM 파형 분석이 필요하다. 이 구현에는 초과/누락 카운터를 추가하지 않았다.

절전 진입이나 처리 중단 시 `pcmDataOut()`에 도달하지 않으면 High가 남을 수 있다. 측정 실패를 감추지 않도록 임의 timeout이나 강제 Low를 추가하지 않았다. 절전 소비전류 시험에는 DIO35 시험 기능을 끈다.

## PC에서 다시 확인

아래 빌드는 Eclipse Debug 출력이나 장치 펌웨어를 건드리지 않는다. 결과는 `tmp/unit-tests/target`에 생성된다. 빌드 도구는 소스 헤더를 수정하지 않고 OFF/ON을 명시한다.

```powershell
python tests/unit/build_target_tests.py CM3 --normal
python tests/unit/build_target_tests.py CFX --normal
python tests/unit/build_target_tests.py CM3 --normal --system-test-dio35
python tests/unit/build_target_tests.py CFX --normal --system-test-dio35
python tests/unit/build_target_tests.py CM3 --normal --system-test-7 --system-test-dio35
python tests/system/check_dio35.py --before "docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/qa/dio35-system-test-20261014/before"
```

검사기는 실제 세 함수의 본문을 호스트 관찰 함수에 연결해 Low 선행 초기화, 6개 PCM 분기의 High/Low 순서, 반복 호출과 OFF 보존을 검사한다. 실제 SDK 산출물의 입력/출력 해시, CFX SET/CLR 명령 및 CM3 초기화 순서도 확인한다. 하드웨어 GPIO/클럭/IRQ를 재현하거나 신호처리 시간을 측정하는 시험은 아니다.
