---
name: 에이전트-11-결과
purpose: 약 절전 루프 뚜껑 열림 감지 & SPI 안전성 분석 결과 요약
type: agent-log
maturity: draft
tags: [cradle, SPI, 뚜껑열림, isd_state]
---

# 에이전트 11 결과 요약

**TL;DR**: nRF가 SPI 마스터, CM3가 슬레이브. nRF가 실제로 꺼지지 않더라도 QCC 셧다운이면 SPI로 올 패킷 없음. isd_state 더미값 가능. 뚜껑 열림 감지는 GPIO 폴링 필수. 재부팅 전 20ms 로그 드레인 권장.

## SPI 동작 분석

| 항목 | 판정 |
|---|---|
| CM3 SPI 역할 | 슬레이브 (마스터는 nRF) |
| QCC 셧다운 후 SPI 패킷 수신 | **불가** (QCC가 보내지 않음) |
| nRF 실제 OFF 여부 | 꺼지지 않음 (공함수) |
| bleCommunication() 약 절전 호출 시 실효 | 미미 (패킷 없음) |

## isd_state 안전성

- `{en__isdStatus_NA, false}` 더미값 전달 가능 (내부 검증 없음)
- conneded_ISD = false → ISD 미연결 처리

## 뚜껑 열림 감지 권장

```c
// DIO_PIN_INDEX_for_CarryingCaseCoverOpen GPIO 직접 읽기
// 홀센서 HIGH(1) = 뚜껑 열림 (df_Opened)
if (Sys_GPIO_Read(DIO_PIN_INDEX_for_CarryingCaseCoverOpen) == 1)
{
    delay_ms(20);  // 로그 드레인
    SYS_WATCHDOG_RESET();
}
```

## 재부팅 전 정리

- 20ms 딜레이 (RTT 로그 드레인 — ci_ble_control_boot.c:114 패턴 참조)
- QCC 이미 셧다운 상태이므로 별도 QCC 정리 불필요
