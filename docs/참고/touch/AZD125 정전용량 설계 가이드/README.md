---
name: azd125-index
purpose: AZD125 Capacitive Sensing Design Guide 원문 정리 색인
type: 데이터시트
maturity: stable
tags: [touch, azd125, capacitive, design-guide, layout, index]
---

# AZD125 Capacitive Sensing Design Guide (색인)

> **TL;DR**: Azoteq 앱노트 **AZD125**(Capacitive Sensing Design Guide — 버튼·슬라이더·휠, v1.2, 2025-03, 41p)의 원문 충실 정리(SSOT). 설계 시작·기구/레이아웃 베스트 프랙티스·근접/접지·터치 버튼·슬라이더/휠·온도/방수/노이즈를 6개 md로 분할. PCB 설계·전극 치수·환경 보정 참조. 원본: [`../azd125-capacitive_sensing_design_guide_v1.2.pdf`](../azd125-capacitive_sensing_design_guide_v1.2.pdf).

> [!NOTE]
> 본 폴더는 앱노트 **원문 충실 정리**다. 센싱 일반 원리는 [`../AZD004 센싱 일반 가이드/`](../AZD004%20센싱%20일반%20가이드/), Sound1 적용 관점은 [`../적용가이드/`](../적용가이드/). 모든 수치·표는 할루시네이션 검증(원문 전수 대조, 적출 0건)을 거쳤다.

---

## 파일 색인

| 파일 | 원문 구간 | 내용 |
|---|---|---|
| [01_개요·설계시작](01_개요·설계시작.md) | §1~2 | Introduction / Starting a New Design |
| [02_베스트프랙티스·레이아웃](02_베스트프랙티스·레이아웃.md) | §3 | Mechanics(오버레이·접착·유전율) / Common Layout Considerations(트레이스·이격·LED) |
| [03_근접센싱·접지효과](03_근접센싱·접지효과.md) | §4~5 | Proximity 설계 / Grounding(Battery·Well-Grounded·Device·Portable 감도 개선) |
| [04_터치버튼](04_터치버튼.md) | §6 | Self-Capacitive / Mutual Capacitive 터치 버튼 설계 치수·형상 |
| [05_슬라이더·휠](05_슬라이더·휠.md) | §7 | Self / Mutual Capacitive 슬라이더·휠 전극 치수표 |
| [06_온도·방수·노이즈](06_온도·방수·노이즈.md) | §8~10 | Temperature(Follow UI) / Water Immunity·Humidity / Noise(SNR) |

---

## 빠른 참조

| 항목 | 값/내용 | 파일 |
|---|---|---|
| 민감도 기준 | 약 10 fF/count | [02](02_베스트프랙티스·레이아웃.md) |
| 유전율(표 3.1) | Air 1.0 / Glass 7.6~8.0 / FR-4 5.2 / Nylon 3.2 등 | [02](02_베스트프랙티스·레이아웃.md) |
| 오버레이 두께 | 0.8~10 mm (권장 0.8~3 mm) | [02](02_베스트프랙티스·레이아웃.md) |
| 근접 결합 용량 | C1 < 10 pF, C2 > 100 pF | [03](03_근접센싱·접지효과.md) |
| 휴대형 감도 개선 | 8개 방법 | [03](03_근접센싱·접지효과.md) |
| 슬라이더 치수 (표 7.1/7.2) | Pitch·Gap·Element 수 권장값 | [05](05_슬라이더·휠.md) |
| 온도 드리프트 / Follow UI | Follow Weight 식, ATI 비활성 절차 | [06](06_온도·방수·노이즈.md) |
| SNR 기준 | 비핵심 ≥3, 핵심 ≥5 | [06](06_온도·방수·노이즈.md) |
