# Sound1 유닛 시험 코드

이 폴더는 현재 소프트웨어 유닛 검증 기준서의 시험을 실행/분석하기 위한 도구다. 실제 장치의 합격 보고서가 아니다. 소스에 추가된 시험 경계는 `TDC_UNIT_TEST` 빌드에만 들어간다. 과거 SND1 시험 파일은 그대로 보존하며 현재 시험 번호와 섞어 집계하지 않는다.

## 먼저 확인할 사항

시스템 시험 `SW-FW-SYS-Test-ID-7`의 별도 RTT 명령은 [전하량 계산/오류 LED 시험](../system/README.md)을 사용한다. 이 명령은 `TDC_SYS_TEST_7_ENABLE`로 분리되어 있으며 아래 유닛 콘솔과 정지 조건/실행 경로가 다르다.

개별 함수 호출보다 먼저 [유닛 시험 일괄 실행 매뉴얼](../../docs/참고/인허가/사운드1/DHF/04_설계%20검증/SW/펌웨어/유닛%20시험%20일괄%20실행%20매뉴얼.md)을 본다. 시험 환경을 한 번 준비한 뒤 RTT GUI에서 `ut auto 1`로 CM3/CFX, `ut hear 1`로 HEAR, 별도 BOOT 이미지에서 `ut boot 1`로 실행한다. 실제 입력은 `ut observe 그룹 단계번호`로 관련 상태를 함께 보관한다. `collect_rtt.py`는 저장 로그를 유닛별로 나누며, END/수집 검사를 제품 합격으로 취급하지 않는다.

- 시험 지그만 사용한다. 착용 중인 제품에서 실행하지 않는다.
- 정상 CM3 반복 처리, CFX 오디오/PCM 처리, 공유 자료 갱신과 해당 ISR을 멈춰 시험 코드가 RAM을 독점하도록 한다. 백업/복원은 다른 코어의 동시 쓰기를 막아 주지 않는다.
- 현재 빌드의 기호와 메모리 맵을 사용한다. 이전 빌드의 주소를 다시 사용하지 않는다.
- 계산 시험은 RAM에서 입력을 고정한다. 파일/드라이버 시험의 호스트 경계는 임시 파일과 모의 주변장치다. 실제 저장소 쓰기, 리셋, 자극 송신을 자동으로 실행하지 않는다.
- HEAR 시험은 HEAR 코드/창/FIFO가 준비되고 완료 ISR이 동작해야 한다. 정상 처리만 정지하고 필요한 완료 ISR은 유지한다. 준비를 확인하지 않고 `hardware_ready=1`을 넣지 않는다.

## PC 검사와 시험 이미지 만들기

저장소 루트에서 Python으로 다음 명령을 실행한다. `TDC_E8300_SDK` 환경 변수를 생략하면 설치된 Ezairo 8300 SDK 경로를 사용한다. 호스트 GCC도 필요하다.

```text
python tests/unit/run_suite.py
python tests/unit/run_suite.py --sdk
```

첫 명령은 로그/전달, 실제 계산 함수 및 격리된 파일/드라이버 시험을 검사한다. 두 번째는 정상/시험 빌드의 전체 링크도 검사한다. 결과와 전체 출력은 `tmp/unit-tests/suite`, 시험 이미지는 `tmp/unit-tests/target`에 저장한다. Eclipse의 기존 Debug 산출물은 덮어쓰지 않고 장치에 다운로드하지 않는다.

CFX 시험 링크는 `+Wbridge,+s`를 사용한다. 이 옵션이 없으면 호출하지 않는 디버거용 시험 함수가 제거될 수 있다. CM3/BOOT는 시험 전용 참조를 링크의 `KEEP`으로 유지한다. 정상 빌드는 매크로가 없어서 해당 참조가 없다. `--normal` 빌드를 제품 시험 이미지와 혼동하지 않는다.

회귀 검사기의 종료 코드 0은 검사기가 예상대로 동작했다는 뜻이다. 기록된 기준 불일치까지 제품 합격이라는 뜻은 아니다. 실패를 숨기도록 기댓값을 바꾸지 않는다.

### 제품 소스를 보존하는 작업

2026-10-02 추가 보완에서는 제품 소스/헤더/링커/프로젝트 설정 273개의 시작 해시에 어셈블리/CFX 링커 6개를 보충해 279개를 비교한다. 보충한 파일은 Git 변경이 없음을 확인했고 기존 기준도 보존했다. 기존 사용자 수정은 시작 시점 내용으로 보존한다. 시험 전용 디렉터리만 변경을 허용하며, 제품 파일이 추가/삭제/수정되면 검사가 실패한다. IDE 임시 파일과 무시된 빌드 생성물은 비교 대상이 아니다.

```text
python tests/unit/run_suite.py --sdk --preserve-source tmp/unit-tests/source-preservation/before-complete.json
```

새로운 시험 전용 작업에서 기준을 만들 때는 `check_source_preservation.py <새 기준 파일> --capture`를 먼저 실행한다. 기존 기준 파일을 덮어쓰지 않는다. 제품 파일이 달라졌을 때 검사기는 파일을 되돌리지 않고 차이만 출력한다.

감사기록의 보충 PC 시험은 `python tests/unit/check_event_log.py`다. 실제 `ci_event_log.c`의 함수 본문과 자료형을 읽고 파일/시간 경계만 RAM 입력으로 대신한다. 생성/읽기/순환 저장, 파일 오류와 짧은 읽기/쓰기를 검사한다. `tmp/unit-tests/event-log/checks.json`에 실제값/기대값을 남긴다. 시간 정보의 신뢰성, 외부 접근 권한과 실제 저장장치 보존은 이 시험의 범위 밖이다.

제품 원본을 바꾸지 않고 확장한 경로 중 다음은 **PC 전용**이다. 타깃의 같은 번호를 호출하면 이 입력까지 자동으로 실행된다고 해석하지 않는다.

| 실행 | 보완 범위 | 결과 |
|---|---|---|
| `python tests/unit/tdc_ut_cfx_io_check.py` | 실제 맵 변경/계산 완료와 CIS/NofM 연산 연결, 밴드 수 경계 | `tmp/unit-tests/cfx-io/checks.json` |
| `python tests/unit/tdc_ut_io_cm3_build.py --check-known-findings` | 터치 초기화, 구형 PMIC, 맵 저장/초기화, 리모콘/매핑 명령 | `tmp/unit-tests/cm3-io/checks.json` |
| `python tests/unit/tdc_ut_cm3_extra_run.py` | 내부기/맵 선택, 전원/FPGA 준비, 연결 확인, 단극/양극 설정, 계산 호출자 | `tmp/unit-tests/cm3-extra/checks.json` |

`--check-known-findings`는 알려진 실패의 위치와 값을 대조하는 옵션이다. 생략하면 CM3 I/O의 현재 계약 불일치 때문에 종료 코드 1이 반환된다. PC 추가 상태 시험은 조건마다 새 프로세스에서 실행하므로 내부 정적 변수의 초기 상태를 분리한다. 이것은 장치 RAM 백업/복원 API를 추가했다는 뜻이 아니다.

## 한 유닛씩 실행하고 출력하기

시험 함수는 시작 시 자동으로 호출하지 않는다. 시험용 디버거에서 명시적으로 호출한다. 한 번의 함수 호출이 현재 유닛의 입력/기대값을 준비하고 실제 함수를 실행한다. 완료한 배열은 RTT 전송이 끝날 때까지 보존한다.

| 범위 | 실행 함수 | 결과 배열/상태 | 다음 실행 전 |
|---|---|---|---|
| CFX 계산 | `tdc_ut_cfx_run_unit(unit)` | `g_tdc_ut_cfx_vector_*` 및 해당 SDK 계산의 `g_tdc_ut_cfx_numeric_*` | 두 배열 배출 후 `tdc_ut_cfx_vectors_release()` |
| CFX I/O | `tdc_ut_cfx_io_run_unit(unit)` | `g_tdc_ut_cfx_io_*` | `tdc_ut_cfx_io_release()` |
| CM3 계산/제어 | `tdc_ut_cm3_vectors_run_unit(unit)` | `g_tdc_ut_cm3_vector_*` | `tdc_ut_cm3_vectors_release()` |
| CM3 I/O | `tdc_ut_io_cm3_run_unit(unit)` | `g_tdc_ut_io_cm3_*` | `tdc_ut_io_cm3_release()` |
| BOOT | `tdc_ut_boot_run_unit(unit)` | `g_tdc_ut_boot_*` | `tdc_ut_boot_release()` |

`unit`은 현재 기준서의 코어별 유닛 번호다. 각 헤더의 반환값을 확인한다. 완료, 부분 실행, 미지원, 이전 결과 대기, 안전 조건 위반과 배열 넘침을 구분한다. `done=1`이나 실행 함수의 반환값만으로 합격을 판단하지 않는다.

CM3에서 계산/관찰 배열을 출력할 때는 다음 순서다.

```c
/* CM3 계산 시험 예. 반환값과 overflow를 먼저 검사한다. */
tdc_ut_cm3_vectors_run_unit(27);
tdc_ut_report_begin("CM3", g_tdc_ut_cm3_vector_actual,
    tdc_ut_cm3_vectors_expected(), g_tdc_ut_cm3_vector_count,
    TDC_UT_CM3_VECTOR_CAPACITY, g_tdc_ut_cm3_vector_done,
    g_tdc_ut_cm3_vector_overflow);
/* 전경에서 반복 호출: 0=진행/재시도, 1=전체 배출, -1=잘못된 자료. */
tdc_ut_report_poll();
/* poll이 1을 반환한 뒤에만 다음 시험을 허용한다. */
tdc_ut_cm3_vectors_release();
```

위 코드는 호출 순서를 보여 주는 예다. 한 번의 `poll()` 호출만으로 완료되는 코드는 아니다. 버퍼가 차면 이전 줄을 보존하므로 다음 전경 호출에서 재시도한다. ISR에서 출력하거나 버퍼가 빌 때까지 무한 대기하지 않는다.

CFX 배열은 CM3에서 다음과 같이 읽는다.

```c
tdc_ut_report_cfx_begin(actual_word, expected_word, count, capacity, done, overflow);
/* HEAR 결과의 같은 형식은 HEAR 접두어로 출력한다. */
tdc_ut_report_cfx_core_begin("HEAR", actual_word, expected_word,
    count, capacity, done, overflow);
tdc_ut_report_poll();
```

`actual_word/expected_word`는 CFX 디버거/링커 맵의 XMEM **워드 주소**다. CM3 바이트 주소나 주소에 이미 4를 곱한 값이 아니다. 전달기는 XRAM 범위를 확인하고 24비트 음수를 부호 복원한다. CFX 결과를 만들던 실행은 멈춰 있어야 하고 전송 중 새 시험을 시작하면 안 된다.

CFX-023/031/032는 중간값을 고정한 분기 시험과 실제 SDK 로그/거듭제곱/나눗셈 경로를 따로 실행한다. SDK 경로의 원본 계산값은 `g_tdc_ut_cfx_numeric_actual`에 `[실제값, 사전 하한, 사전 상한]` 순서로 남는다. `vector` 배열의 1은 이 구간에 들어갔다는 판정이지 원본 계산값이 아니다.

```c
/* 먼저 vector 배열의 구간 판정 결과를 모두 배출한다. */
tdc_ut_report_cfx_begin(actual_word, expected_word, vector_count,
    vector_capacity, done, overflow);
/* poll()이 1이면, numeric_count>0인 경우 다음 관찰 기록을 배출한다. */
tdc_ut_report_cfx_observed_begin("CFX", numeric_word, numeric_count,
    numeric_capacity, done, overflow);
/* 다시 poll()이 1일 때까지 읽은 뒤 CFX 쪽 release를 호출한다. */
```

`numeric_word`도 현재 빌드의 XMEM 워드 주소다. 헤더가 있는 여러 계산 조건을 그대로 읽으며 `OBSERVED`로만 출력한다. 구간 판정 기록과 원본 계산값 기록을 함께 보존한다. 기준값은 독립 수식/SDK 정밀도/정수 절삭으로 실행 전에 정하며 측정 결과에 맞추어 넓히지 않는다. PC의 수학 대체 함수가 일치해도 실제 CFX 수치 정확도는 장치에서 이 배열을 읽어 확인해야 한다.

BOOT는 `tdc_ut_boot_report_begin()`을 호출한 후 `tdc_ut_boot_report_poll()`로 배출한다. 정상 부트 진행 중에 강제로 유닛을 바꾸지 않는다. 타깃에서 실행하지 않는 파일 시험은 PC의 격리된 fixture로 확인한다.

BOOT의 RAM 파일 시험은 SDK의 큰 `FIL` 객체를 스택에 두지 않는다. 시험 전용 `.tdc_ut_boot_workspace`를 기존 RTT 제어 블록 뒤의 BRAM에 배치하고 매 사용 전 초기화한다. 제공된 시험 링커 구성을 함께 사용해야 한다. 다른 코어가 해당 BRAM을 사용하는 상태에서 실행하지 않는다. 파일 입력은 RAM이며 실제 파일/NVM 쓰기나 코어 점프를 하지 않는다. 실제 SDK CRC를 사용하는 13번은 CRC 장치도 전경에서 독점해야 한다.

관찰만 하는 배열은 `tdc_ut_report_begin(..., expected=NULL, ...)`로 전달한다. CM3 계산 runner의 `tdc_ut_cm3_vectors_expected()`는 관찰일 때 NULL을 반환한다. CM3 I/O는 `g_tdc_ut_io_cm3_observed`가 1이면 expected 대신 NULL을 넘긴다. 헤더 없는 CFX/HEAR 캡처는 `tdc_ut_report_cfx_values_begin(core, unit, case, word, count, done)`로 출력한다. 이 경로는 **OBSERVED**만 기록하고 PASS를 만들지 않는다.

CM3-029의 DAC 실패 상태는 별도 `tdc_ut_cm3_vectors_observe_calculation_failure()`로 준비한다. case 91/92는 이전 완료 플래그가 각각 0/1이다. 실제 함수의 false 반환, 공유 앞부분 갱신, 복사 시도와 완료/출력 허용 상태를 기록한다. 실패한 지역 T/C 배열 읽기만 시험 경계에서 생략하므로, 남은 T/C 값을 제품이 보존했다고 판단하지 않는다. `tdc_ut_cm3_vectors_expected()`가 NULL인 것을 확인해 관찰 기록으로 배출한다. 실패 후 값/출력 처리 정책은 설계 판단이 남아 있다.

손상된 기록/명시적 중단은 `tdc_ut_report_cancel()` 또는 BOOT의 cancel로 중단하되 시험 완료로 처리하지 않는다. 이미 줄의 일부가 전송됐다면 CRLF가 남는다. **취소한 뒤에도 poll이 1을 반환할 때까지 배출한 후** 새 begin/release를 호출한다. 이 동안 새 begin은 거부한다. 취소된 벡터에 합격 문구를 추가하지 않는다.

## HEAR 실행

`tdc_ut_hear_start(unit, vector, hardware_ready)` 다음에 `tdc_ut_hear_poll()`을 호출한다. CMD0은 고정된 16개 입력의 절대값/최대값이다. CMD1의 vector 1~5는 512점의 0 입력/bin 32/bin 64/DC/Nyquist 입력이다. 창 계수가 사전 입력과 다르면 시작을 거부한다. 완료 전에는 메모리를 복원하거나 새 시험을 시작하지 않는다.

HEAR-001의 MaxElement는 절대값 배열의 첫 값을 최대값으로 덮어쓴다. vAbsolute 중간 배열은 microcode/수식 분석이고 실제 완료 뒤의 배열은 최종값 비교다. 두 시점의 결과를 섞지 않는다.

HEAR-002의 판정 배열은 FFT 9개 bin의 두 성분과 vMag 256개 전부를 독립 DFT 기준과 비교한다. FFT 전체 512단어는 `g_tdc_ut_hear_fft`, vMag 전체 256개는 `g_tdc_ut_hear_vmag`, 실제 BFP 지수는 `g_tdc_ut_hear_block_exp`에서 읽는다. 판정 기록 배출 후 `tdc_ut_report_cfx_values_begin("HEAR",2,80,fft_word,512,done)`과 case 81의 vMag 256개, case 82의 지수 1개를 각각 끝까지 배출한다. 원본 배열을 읽기 전에 release하지 않는다.

FFT의 원시값에 `2^E`, vMag에 `2^(E+1)`을 곱한 값으로 비교한다. 사전 오차는 FFT 성분 `64+ceil(abs(기준)/1000)`, vMag `64+ceil(기준/100)`이며 SDK 제조사 보장값이 아닌 기술 검토 기준이다. 실기 결과와 함께 타당성을 확인해야 한다. 최대 bin만 일치한 결과를 전량 크기 정확도로 확대하지 않는다. 타임아웃 뒤 늦게 완료되어도 타임아웃 기록은 실패로 남긴다. 호스트에서는 HEAR를 대신 계산해 완료로 꾸미지 않는다.

## 로그 읽기

형식 예이며 실제 장치에서 수집한 로그가 아니다.

```text
[UT-CM3-027] C1 A[0:1]=1,24
[UT-CM3-027] C1 PASS N=2 F=0
[UT-CFX-015] C1 I19 A=0 E=1
[UT-CFX-015] C1 FAIL N=20 F=1
```

- `C1`: 벡터/시험 조건 1. `A`: 실제 실행 결과. `E`: 사전 기대값. `I`: 다른 값의 배열 인덱스.
- `A[0:1]`: 포함 범위 0부터 1까지. 다음 줄의 범위로 이어 모든 데이터를 기록한다.
- `N`: 해당 벡터의 비교 값 수. `F`: 다른 값 수. PASS는 그 벡터의 일치만 뜻한다. 부분 시험이나 다른 벡터까지 전체 유닛 합격으로 확대하지 않는다.
- ASCII와 CRLF를 포함해 줄당 최대 60바이트다. ANSI 색상 코드를 넣지 않는다. RTT가 일부만 받거나 0을 반환하면 남은 바이트를 재시도한다.
- 캡처 파일에 빠진 범위/완료 줄이 있으면 전달 완료 증거가 부족하다. 로그가 화면으로 표시된 사실은 RTT 버퍼에 기록된 사실과 별도로 확인한다.

## 코드 확인/장치 확인이 남는 범위

분석 방법 A는 해당 입력의 조건 분기, 호출 순서, 인자와 상태 변경을 소스에서 검토한다. 번호만 등록하고 합격 로그를 출력하는 함수를 만들지 않는다. 육안 LED, 전압/파형, 실제 NVM 유지 및 QCC 내부 시험은 계산 배열로 대체하지 않는다.

코어별 coverage JSON과 [시험 코드 구현 현황](../../docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력/시험%20코드%20구현%20현황.md)에서 실행/부분/관찰/분석 범위를 확인한다. 다른 코어 또는 정상 루프를 정지하지 못한 상태에서 자동 계산 시험을 진행하지 않는다. 오실로스코프/사진이 필요한 항목은 실제 증거를 추가한 뒤 판단한다.

CM3 4/6/30은 기본 RAM 입력이 연결돼 있다. 4는 대기/장치 초기화 경계를 대신 받아 실제 기동 함수의 준비/허용 순서를 보는 부분 시험이다. 실제 장치 초기화와 CFX 조회는 추가 확인한다. 6은 실제 맵 변경 함수에 동일 맵/새 맵을 요청하며 제품 파일 대신 RAM 경계를 쓴다. 30은 실제 내부기 상태 설정/플래그 해제 함수로 A 유지/B 변경을 확인한다. 사용자 observer gateway를 별도로 지정하면 그 경로는 OBSERVED로만 기록한다.

CM3-006의 동일 맵 재적재와 CM3-030의 동일 상태 변경 플래그는 불일치 기록을 유지한다. 후자는 기준서가 같은 상태에서 플래그 값을 명시하지 않아 해석 검토가 필요하다. CM3 I/O의 수동 관찰은 기준 입력을 별도로 실행한 전/후 결과를 캡처하는 용도다. PC 벡터가 있는 파일 시험도 실제 제품 저장소 시험을 대신하지 않는다.

## 현재 준비 범위와 잔여

PC 고정 입력은 90개 ID(CFX 29/CM3 45/BOOT 16)다. 부분 입력과 비활성 구형 보드 분기를 포함한 집계다. 현재 115개 번호는 모두 실행/분석/관찰에 연결돼 있지만 115개 전체 시험이 완료된 것은 아니다. 실제 장치 시험은 0회이며 사용자 지시로 보류 중이다.

- CFX-023/031/032 실제 SDK 경로와 HEAR의 크기/스케일 비교는 타깃 실행 결과가 필요하다. PC 수학 대체 함수나 독립 DFT만으로 장치 합격을 만들지 않는다.
- CM3의 새 PC 입력은 실기 자동 실행 경로가 아니다. 출력/전원/저장/리셋을 안전하게 격리하지 못한 타깃 경로를 자동으로 실행하지 않는다. BOOT-002/007/017도 PC 경로와 타깃 미지원을 구분한다.
- CM3-028/029 전하량/계산 완료, CFX-033 작은 밴드 패킷, CM3-042/043 저장 응답, 046/049/051 명령 처리, BOOT-007/011 및 감사기록의 실패 반환을 기록으로 보존한다. CM3-039는 비활성 구형 보드 코드다. CFX-015/CM3-006/030은 설계 의도 확인이 필요하다. 제품 수정으로 합격에 맞추지 않았다.
- 정상/시험 SDK 링크, 헤더/링커 입력 동일성, 시험 심볼 분리, 정적 RAM/스택 영역과 선택한 ARM 함수의 단일 스택 프레임은 `run_suite.py --sdk`에서 검사한다. 전체 호출 깊이/IRQ/라이브러리 및 실제 스택 최댓값은 별도로 확인한다. BOOT 시험 이미지의 힙 여유가 작으므로 링크 통과를 모든 실제 파일/동적 할당 경로의 안전성으로 확대하지 않는다.
