---
name: cm3 백텔 캘리브레이션 값 이력
purpose: FPGA backterConfiguration_resetValue 튜닝 이력과 현장실패 개체 기록을 코드에서 문서로 이관 보존
type: 참고
applies_to: [Sound1]
tags: [cm3, fpga, backtel, calibration, 이력, 현장실패]
---

# FPGA 백텔 캘리브레이션 값 이력

**TL;DR**: `board/FPGA_ver2_7_0.h` 의 `backterConfiguration_resetValue` 튜닝 이력이다. 현재 채택값은 **`0x2D`(11번)**. 2차 리팩토링(2026-07-23)에서 헤더의 주석 이력을 코드에서 제거하며 이 문서로 보존했다. **동작에는 영향이 없다** — 채택값 `0x2D` 는 그대로 유지되고, 주석으로만 남아 있던 후보값·현장실패 기록만 이관했다.

## 배경

`FPGA_ver2_7_0.h` 의 `FPGA_WRITABLE_REGISTER_RESET_VALUE` enum 에서 백텔(backtel) 리셋 설정값을 튜닝한 이력이 `#if 0` / 주석 형태로 코드에 누적돼 있었다. 리팩토링 원칙상 죽은 코드·주석은 정리하되, **현장실패 개체번호 같은 이력은 소실되면 안 되는 정보**라 문서로 옮긴다.

## 원본 (제거 시점)

`board/FPGA_ver2_7_0.h:116-131`

```c
    stimulation_PhaseDuration_resetValue = 0,
#if 0
            backterConfiguration_resetValue=0x31,           // 백텔 안들어옴 (32, 39, 59, 119, 120, 121), 백텔 값오류(190) 4월 4주차
#else

    // backterConfiguration_resetValue=0x39, //14
    // backterConfiguration_resetValue=0x35, //13
    // backterConfiguration_resetValue=0x31, //12
    backterConfiguration_resetValue = 0x2D, // 11
    // backterConfiguration_resetValue=0x29, //10        //  백텔 안들어옴 (32, 39, 59, 119, 120, 121), 백텔 값오류(190)
    // backterConfiguration_resetValue=0x25, //9
    // backterConfiguration_resetValue=0x21, //8
    // backterConfiguration_resetValue=0x1D, //7
    // backterConfiguration_resetValue=0x19, //6

#endif
```

## 튜닝 후보값 표

| 라벨 | 값 | 채택 여부 |
|---|---|---|
| 14 | `0x39` | 후보 |
| 13 | `0x35` | 후보 |
| 12 | `0x31` | 후보 (`#if 0` 쪽 원래 값) |
| **11** | **`0x2D`** | **현재 채택** |
| 10 | `0x29` | 후보 |
| 9 | `0x25` | 후보 |
| 8 | `0x21` | 후보 |
| 7 | `0x1D` | 후보 |
| 6 | `0x19` | 후보 |

## 현장실패 기록 (원 주석 그대로)

> 백텔 안들어옴 (32, 39, 59, 119, 120, 121), 백텔 값오류(190) 4월 4주차

- **백텔 미수신 개체번호**: 32 · 39 · 59 · 119 · 120 · 121
- **백텔 값오류 개체번호**: 190
- 시점: 4월 4주차

## 제거 후 코드

리팩토링 후 `FPGA_ver2_7_0.h` 는 채택값만 남긴다.

```c
    stimulation_PhaseDuration_resetValue = 0,
    backterConfiguration_resetValue      = 0x2D,  // 튜닝 이력: docs/참고/cm3-백텔-캘리브레이션-이력.md
    fpga_IO_MUX_Configuration_resetValue   = 0,
```
