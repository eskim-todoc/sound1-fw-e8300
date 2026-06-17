---
name: 에이전트-07-결과
purpose: FPGA/nRF/QCC/PMIC 제어 함수 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, FPGA, nRF, QCC, PMIC, 저전력]
---

# 에이전트 07 결과 요약

**TL;DR**: 주요 함수 시그니처 확인. QCC 셧다운에 외부 2.2초 딜레이 필수. nRF 실제 꺼지지 않음(공함수). PMIC 실질 제어 불가(1.5세대 V_LINK_ON). CFX 전원은 OnOff_3V_PMIC와 독립.

## 핵심 함수 시그니처

| 함수 | 파일:라인 | 특이사항 |
|---|---|---|
| `write_FPGA_reset()` | isd_interface_FPGA.c:647 | `bool` 반환, I2C 비블로킹 |
| `ResetNRF()` | initialize.c:74 | NOP만, GPIO 주석 처리 |
| `NRF_Off_Command()` | systemControl.c:37 | 공함수, GPIO 주석 처리 |
| `snd_qcc_set_mode(SHUTDOWN)` | snd_qcc.c:57 | GPIO DIO24 LOW, **함수 내 딜레이 없음** |
| `DIO_PIN_INDEX_for_FPGA_SLEEP` | Board_OTE_ver1_5.h | DIO4(rev≤1.2) 또는 DIO26 |
| `OnOff_3V_PMIC_CM3_to_CFX(false)` | cfx_cm3_sharedMemory.c:162 | 공유메모리=2, **실질 효과 없음** |

## QCC 셧다운 딜레이 근거

```c
// ci_ble_control_boot.c:117-123
snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);
for (volatile int i = 0; i < 2200; i++) {
    SYS_WATCHDOG_REFRESH();
    delay_ms(1);  // 2.2초 대기
}
```

## CFX 전원 독립성

- 1.5세대: RF PMIC 5V는 FPGA V_LINK_ON 바이패스로 CM3 제어 불가 (항상 활성)
- `OnOff_3V_PMIC_CM3_to_CFX(false)` 코드에 "사실상 아무 의미 없다 (by 김은수, 2026.02.20)" 주석
- CFX 전원: `enter_ULP_mode_Command_CM3_to_CFX` 공유메모리 신호로만 제어 가능
