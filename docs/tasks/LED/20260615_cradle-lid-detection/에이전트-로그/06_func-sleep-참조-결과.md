---
name: 에이전트-06-결과
purpose: func_sleep() 구현 참조 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, func_sleep, 저전력]
---

# 에이전트 06 결과 요약

**TL;DR**: func_sleep() main.c:797~954. 실행 순서: FPGA리셋→CFX ULP신호→nRF→NRF_Off→QCC셧다운→FPGA슬립→PMIC끄기→LED끄기→CFX ULP대기→클럭감소→ULP루프. CFX는 enter_ULP_mode 공유메모리 신호로 제어.

## 핵심 발견

- `func_sleep()`: `main.c:797~954`
- 실행 순서 (약 절전과 차이점 표시):
  1. `write_FPGA_reset()` ← 적용
  2. `enter_ULP_mode_Command_CM3_to_CFX = 1` ← **약 절전 시 생략**
  3. `ResetNRF()` ← 적용 (NOP만)
  4. `NRF_Off_Command()` ← 적용 (공함수)
  5. `snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN)` ← 적용 (**2.2초 딜레이 필요**)
  6. `Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP)` ← 적용
  7. `OnOff_3V_PMIC_CM3_to_CFX(false)` ← 적용 (실효 미미)
  8. `turnOffLED()` ← 적용
  9. CFX ULP 대기 루프 ← **약 절전 시 생략**
  10. `ci_power_sleep()` (클럭 감소) ← **약 절전 시 생략** (bleCommunication 루프 때문)
  11. ULP 터치 폴링 루프
- `nRF`, `NRF_Off_Command` 모두 GPIO 주석 처리 — 실제 nRF 미꺼짐
- CFX는 공유메모리 신호로 독립 제어
