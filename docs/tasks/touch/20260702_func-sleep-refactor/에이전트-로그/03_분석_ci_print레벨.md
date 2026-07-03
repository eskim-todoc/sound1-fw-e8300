---
name: 분석_ci_print레벨 — func_sleep() 로그 레벨 재분류
purpose: ci_print 11건 전수를 printw(최대 에러)/printd(그 외) 2단으로 재분류하고 최소화·문구 통일안 제시
type: tasks/에이전트-로그
applies_to: [Sound1]
maturity: stable
tags: [touch, sleep, refactor, ci_print, 상태:진행]
---

**TL;DR**: 11건 중 `printw` 잔류는 2건뿐(칩 리셋을 유발하는 최종 이벤트: `ATI ERROR -> REBOOT`, `touch detected -> REBOOT`). 나머지 9건은 전부 `printd`. 핵심 쟁점 2건 모두 "재부팅(칩 리셋) 여부"를 기준으로 뒤집힘: `ATI ERROR -> RECOVER`(재시도)는 printw→printd 강등, `touch detected -> REBOOT`는 printi→printw 승격.

## 재분류 기준

00번 문서의 요구사항 "최대 에러 상태만 printw"를 문자 그대로 적용하려면 판정 축이 하나 필요하다. 채택 기준: **이 라인이 실행되면 `func_sleep()` 자신의 제어 흐름이 끝나고 `SYS_WATCHDOG_RESET()`으로 칩이 강제 리셋되는가**. 이 기준만이 "최대"라는 표현에 걸맞은 유일하고 objective한 경계선이다. 재시도·자동복구·정상 진행 알림은 아무리 문구가 `ERROR`/`timeout`을 포함해도 함수가 계속 살아있으므로 배제한다.

## 재분류 표 (main.c 라인은 uncommitted 현재 상태 기준)

| main.c 라인 | 메시지 | 현재 | 재분류 | 근거 |
|---|---|---|---|---|
| 1042 | "CM3 IS PREPARING TO ENTER SLEEP MODE" | printi | printd | 정상 진입 알림, 리셋 없음 |
| 1044 | "FIRST OF ALL, TRY TO POWER OFF FPGA BACKTRL H/W" | printi | **삭제** | `write_FPGA_reset()` 호출 직전 의도만 서술(WHAT), 바로 다음 줄(1050)이 결과를 찍으므로 정보 중복 |
| 1050 | "FPGA reset : %s" (success/failed) | printi | printd | `ret`가 `failed`여도 `func_sleep()`은 그대로 진행(비차단 fire-and-forget), 리셋 유발 안 함 — success/failed 두 분기 모두 동일 레벨 |
| 1066 | "CFX : sleep" | printi | printd | `enter_ULP_mode_Command_CM3_to_CFX==0` 확인용 정상 진행 알림 |
| 1120 | "[T] LTA=..." | printd | printd (유지) | 이미 요구사항 부합, `#if TDC_TOUCH_DEBUG_PRINT_ENABLE` 게이트 그대로 |
| 1131 | "ATI ERROR -> REBOOT (recover in normal)" | printw | **printw 유지** | `ati_error_reboot_cnt > 5` 분기, 직후 `SYS_WATCHDOG_RESET()`으로 칩 리셋 — 유일한 진짜 최종 에러 |
| 1142 | "ATI ERROR -> RECOVER (in sleep)" | printw | **printd로 강등** | `tdc_touch_iqs323_re_ati()` 호출 후 카운트만 증가, 함수 계속 실행. 00번 문서가 이미 "재시도 중, 최종 아님"이라 명시 — 재시도는 정상 회복 경로이지 에러 최댓값이 아님 |
| 1165 | "notouch confirmed -> RESEED, gate open" | printi | printd | 게이트 정상 해제, 에러 아님 |
| 1179 | "notouch timeout 10s -> forced RESEED" | printw | **printd로 강등** | 10초간 확정 못 한 상태이나 `tdc_touch_iqs323_reseed()` 한 줄로 자동 탈출·리셋 없음. 표에도 "자동 복구(비치명)"로 이미 기재됨 — 리셋 기준에 못 미침 |
| 1189 | "touch detected -> REBOOT" | printi | **printw로 승격** | 1131과 동일 클래스(직후 LED 전환 + `SYS_WATCHDOG_RESET()`으로 칩 리셋). "재부팅 직전"이 printi에 머무는 것은 기준과 모순 — 이번 재분류의 핵심 수정 포인트 |
| 1210 | "ULP STATE: %s -> %s" | printv | printd | 목표 최종 상태(요구사항 §목표)에 `printv`는 존재하지 않음 — 상세 추적 성격은 유지하되 레벨만 printd로 흡수 |

## 최종 집계

- `printw`: **2건** — 1131("ATI ERROR -> REBOOT"), 1189("touch detected -> REBOOT"). 둘 다 "칩 리셋 직전"이라는 동일 시맨틱.
- `printd`: **8건**(1044 삭제 반영 시) 또는 9건(삭제 보류 시) — 계측·정상 진행·자동 복구·상세 추적 전부 포함.
- `printe`: 0건 (기존과 동일, 이번에도 미사용).

## 삭제·통일안

1. **삭제**: 1044 "FIRST OF ALL..." — 1050의 결과 로그가 즉시 뒤따르므로 순수 중복. 삭제해도 정보 손실 없음(로직 변경 없음, 로그 문구 제거일 뿐).
2. **문구 통일**: 1131/1142/1179/1189 네 줄 모두 `[TOUCH] SLEEP: ` 접두어는 이미 일치하나, 선행 `\r\n` 유무가 들쭉날쭉(1131·1142·1189엔 있고 1179엔 없음) — 통일 시 가독성 개선되나 이는 로그 "겉모습"일 뿐 동작에 영향 없으므로 계획 단계(11번 문서)에서 채택 여부 결정 권장.
3. **레벨과 별개로 유지할 것**: `#if (TDC_TOUCH_DEBUG_PRINT_ENABLE)` 게이트(1120)와 `#if 1`(CFX 대기 루프, 1066 포함)은 이번 로그 레벨 재분류와 무관한 기존 컴파일 스위치이므로 손대지 않는다(01번 분석 소관).

## 핵심 쟁점 결론

00번 문서가 지목한 두 쟁점은 **동일한 잣대(칩 리셋 유발 여부)로 정반대 방향 수정**이 필요하다: "touch detected -> REBOOT"는 최종 리셋 이벤트인데도 printi에 머물러 있어 **승격**해야 하고, "ATI ERROR -> RECOVER"는 최종이 아닌 재시도인데도 printw였던 것을 **강등**해야 한다. 두 판단 모두 "리셋 직전인가"라는 단일 기준에서 자연스럽게 도출되므로 별도 예외 규칙 없이 일관되게 적용 가능하다.
