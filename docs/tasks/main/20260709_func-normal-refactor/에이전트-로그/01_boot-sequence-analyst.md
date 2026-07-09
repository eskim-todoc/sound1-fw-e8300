---
name: 01_boot-sequence-analyst
purpose: func_normal() 진입부(CFX 시작 대기 + Initialize() + ISD1 정보 RTT 출력 블록)의 실행 빈도·추출 가능성·상태 결합도 분석
type: tasks/에이전트-로그
applies_to: [projects/Sound1]
maturity: stable
tags: [main.c, func_normal, refactor, boot-sequence, isd-info, watchdog-reset]
---

## TL;DR

func_normal() 진입부(CFX 시작 대기 while(1) + Initialize() + ISD1 정보 RTT 출력, 현재 파일 기준 라인 362~435)는 "매 iteration마다"가 아니라 **매 func_normal() 호출(= 매 부팅/리부팅 사이클)마다 정확히 1회** 실행된다. 결정적으로, func_sleep()은 정상 반환하지 않고 항상 `SYS_WATCHDOG_RESET()`(칩 리셋)으로 종료되므로(main.c:1158), main()의 `while(1){ func_normal(); func_sleep(); }`(main.c:323~327)는 실질적으로 **func_normal()을 부팅당 1회만** 실행한다 — "절전 복귀 후 재진입"은 존재하지 않고 매번 완전 리부팅이다. ISD1 정보 출력 블록(387~435)은 전역 `g_ci_filesystem_ptr_entire_map` 하나만 참조하며 func_normal()의 지역변수와 전혀 결합되어 있지 않아 파라미터 없는 함수로 즉시 추출 가능하다.

## 1. 실행 빈도 — "매 호출마다"인가 "1회성"인가

### 결론: 매 func_normal() 호출마다 실행되지만, func_normal() 자체가 부팅당 1회만 호출된다

- `main()`의 최상위 루프: `main.c:323~327`
  ```c
  while (1)
  {
      func_normal();
      func_sleep();
  }
  ```
- `func_sleep()` (`main.c:1177~1265`)은 ULP 진입 후 `while(1) // ULP loop`(`main.c:1241`)에 영구히 머무르며, 유일한 탈출 경로는 `tdc_touch_sleep_handle_touch_reboot()`(`main.c:1138~1166`) 내부의 `SYS_WATCHDOG_RESET()`(`main.c:1158`)이다. 함수 끝의 `return 0;`(`main.c:1264`)에는 "도달 불가 — 컴파일러 만족용" 주석이 명시되어 있어, **func_sleep()은 정상적으로 반환하는 경로가 없다**.
- 즉 워치독 리셋 = MCU 하드웨어 리셋 → 리셋 벡터부터 재시작 → `main()` 재실행 → `func_normal()`이 다시 호출된다. 이는 **"같은 실행 컨텍스트 내 재진입"이 아니라 완전히 새로운 부팅**이다.
- 결과적으로 진입부(362~435)가 실행되는 시점은 오직 "전원 인가 최초 부팅" 또는 "절전 중 터치로 인한 워치독 리셋 후 재부팅" 두 가지뿐이며, 두 경우 모두 **완전히 새로운 스택/전역 초기화 상태에서 정확히 1회** 실행된다. func_normal() 내부에 "이미 실행했음"을 표시하는 static 가드 변수는 없고, 애초에 필요하지도 않다(리부팅마다 스택과 전역 초기화 코드 전체가 새로 실행되므로 중복 실행 우려 자체가 성립하지 않음).
- 참고: 은수님 지적처럼 "절전 복귀 후 재진입"이라는 멘탈 모델은 이 펌웨어 아키텍처와 다르다. 절전 해제는 "복귀"가 아니라 "리부팅"이다. 리팩토링 계획 시 이 사실을 명확히 반영해야 함수 분리 후에도 동일한 재부팅 기반 흐름을 보존할 수 있다(behavior-preserving 핵심 전제).

## 2. ISD 정보 출력 블록(387~435) 별도 함수 추출 가능성

### 결론: 파라미터 없는 함수로 즉시 추출 가능 (부작용 없음, 순수 읽기+RTT 출력)

블록 전체(`main.c:387~435`, 자체 스코프 `{ ... }`)가 참조하는 것:

- **전역 변수 1개만 참조**: `g_ci_filesystem_ptr_entire_map` (`ST__CFX_CM3_SharedMemory_entireMap *` 전역 포인터, `Gen1_5/FS/ci_map.c`에서 동일 패턴으로 매핑 데이터 접근에 상시 사용되는 기존 전역 — 이 프로젝트에서 이미 표준적으로 쓰이는 접근 방식).
- 블록 내부에서 선언하는 지역 포인터 6개(`p_isd_info`, `p_isd_1_info_name`, `p_isd_1_passkey`, `p_isd_1_location`, `p_isd_1_year`, `p_isd_1_month_model`, `p_isd_1_serial`)는 모두 위 전역에서 파생된 것이며 블록 밖에서 정의되지 않는다.
- **미사용 변수 발견**: `p_isd_info`(`main.c:388`)는 선언만 되고 이후 실제로 참조되지 않는다(사용되는 것은 나머지 5개 + 그 아래 파생 포인터들). 추출 시 이 죽은 선언을 제거할지(동작 불변 원칙상 컴파일 결과에 영향 없는 단순 삭제이므로 안전) 계획 단계에서 결정 필요.
- func_normal()의 최상위 지역변수(`batteryLevel`, `ledPattern`, `systemState`, `isd_state`, `usbConnectorState`, `BLE_communicationState`, `mcuErrorCode`, `powerButtonPushed`, `conneded_ISD`, `mappingConnection`, `qcc_batt_timeout`, `qcc_timeout_poweroff_started`) 중 **어느 것도 이 블록에서 읽거나 쓰지 않는다**.
- 부작용은 오직 RTT 출력(`ci_printw`/`ci_printv`/`ci_printi`, 전역 RTT 채널 — 이미 `SEGGER_RTT_Init()`으로 부팅 초반 초기화됨, main.c:313)뿐이며 리턴값도 없다.

→ **시그니처: `static void tdc_isd_log_boot_info(void);`** (파라미터 없음, 반환값 없음). `main.c` 내 `static` 헬퍼로 두거나, ISD 매핑 접근 로직이 이미 모여 있는 `Gen1_5/FS/ci_map.c` 쪽으로 옮기는 것도 고려 가능하나, 이 블록은 "부팅 로그 출력"이라는 UI/진단 성격이 강해 main.c에 두는 편이 자연스럽다.

## 3. 이후 iteration 루프(437~739)와의 상태 공유 여부

### 결론: 진입부(362~435)는 이후 루프와 지역변수를 통한 상태 공유가 없다

- 진입부(362~435)가 건드리는 것은 오직 전역/공유메모리뿐이다: `cfx_cm3_sharedMemoryAll.is_CFX_started`(읽기, 362), `cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 1`(쓰기, 381), `Initialize()` 호출(내부에서 다수의 전역/주변장치 초기화 수행 — 이 서브에이전트 분석 범위 밖), `g_ci_filesystem_ptr_entire_map`(읽기, ISD 출력용).
- func_normal()의 지역변수 중 이 구간(362~435) 내에서 값이 설정되는 것은 없다. 지역변수 초기화는 모두 그 이전(336~359, 이 담당 구역보다 앞)에서 끝나 있고, 그다음 실질적으로 값이 쓰이는 곳은 437행 이후의 iteration 루프(예: `mcuErrorCode = readErrorCode();` 등, 441행부터)이다.
- 따라서 함수 추출 시 진입부와 ISD 출력 블록은 **출력 파라미터나 반환값을 통해 iteration 루프로 상태를 넘길 필요가 없다** — 완전히 "실행하고 끝"인 순차 단계로, 이 구역만 놓고 보면 함수 분리의 결합도 리스크가 가장 낮은 부분이다.
- 단, `Initialize()` 자체가 이후 루프에서 쓰이는 전역 상태(배터리/ISD/터치 등 서브시스템 초기값)를 설정할 가능성이 높다 — 이는 `Initialize()` 내부 로직에 대한 별도 분석이 필요하며 본 담당 구역(진입부 텍스트 자체)의 범위를 벗어난다. 계획 단계에서 "Initialize() 내부"를 다루는 다른 담당 구역 산출물과 교차 확인 권장.

## 4. 함수 추출 제안

| 대상 | 제안 함수명 | 시그니처 | 비고 |
|---|---|---|---|
| ISD1 정보 RTT 로그 출력 (387~435) | `tdc_isd_log_boot_info` | `static void tdc_isd_log_boot_info(void);` | 파라미터·반환값 없음. 전역 `g_ci_filesystem_ptr_entire_map` 직접 참조. 미사용 `p_isd_info` 선언은 추출 시 정리 권장(behavior 불변, 사경고 제거) |
| CFX 시작 대기 while(1) (362~371) | `tdc_cfx_wait_started` | `static void tdc_cfx_wait_started(void);` | 전역 `cfx_cm3_sharedMemoryAll.is_CFX_started`만 참조하는 busy-wait. 추출 시 `__NOP()` 및 `ci_printd` 로그 그대로 보존 |

두 헬퍼 모두 func_normal() 진입부에서 순서대로 호출(`tdc_cfx_wait_started(); Initialize(); ...; tdc_isd_log_boot_info();`)하면 동작 동일성이 보존된다. `Initialize()` 호출 자체는 이미 이름 있는 외부 함수이므로 추가 추출 대상이 아니다.

## 핸드오프

- 이 구역(362~435)은 iteration 루프(437~)와 지역변수 결합이 없어 분리 리스크가 낮음 — 계획 단계에서 우선 추출 후보로 권장.
- `Initialize()` 내부가 이후 루프에서 참조하는 전역 상태를 어떻게 설정하는지는 별도 분석 필요(본 담당 구역 범위 밖).
- 라인 번호 참고: 본 분석은 현재 워킹트리 기준 실측 라인(func_normal 시작 334, 진입부 362~435)이며, 작업 지시서상 "320~421"은 근사치(과거 버전 대비 드리프트로 추정)이므로 종합 문서 작성 시 실측 라인 번호로 정정 필요.
