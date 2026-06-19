---
name: azd004-index
purpose: AZD004 Azoteq Sensing Technology Introduction 원문 정리 색인
type: 데이터시트
maturity: stable
tags: [touch, azd004, azoteq, sensing, index, reference]
---

# AZD004 Azoteq Sensing Technology Introduction (색인)

> **TL;DR**: Azoteq 앱노트 **AZD004**(Sensing Technology Introduction, v1.1, 2025-03, 33p)의 원문 충실 정리(SSOT). 전하 전달 센싱 원리·센싱 기술 4종·신호 조정(ATI)·필터링·선형화·디바운스·전력·EMC·I²C를 6개 md로 분할. 펌웨어/설계 시 PDF 재독 없이 참조. 원본: [`../azd004_azoteq_sensing_v1.1.pdf`](../azd004_azoteq_sensing_v1.1.pdf).

> [!NOTE]
> 본 폴더는 앱노트 **원문 충실 정리**다. IQS323 전용 데이터시트는 [`../데이터시트/`](../데이터시트/), Sound1 제어 적용 관점은 [`../적용가이드/`](../적용가이드/). 모든 수치·표는 할루시네이션 검증(원문 전수 대조, 적출 0건)을 거쳤다.

---

## 파일 색인

| 파일 | 원문 구간 | 내용 |
|---|---|---|
| [01_개요·전하전달](01_개요·전하전달.md) | §1~2 | Introduction / Charge Transfer Method(Cs 충방전·counts 정의·f_cx 영향) |
| [02_센싱기술](02_센싱기술.md) | §3 | Self-Cap / Mutual-Cap / Resonant Inductive / Hall Effect — 원리·counts 방향·응용 |
| [03_신호조정·ATI](03_신호조정·ATI.md) | §4 | Resolution(Multipliers) / Offset(Compensation) / ATI Overview·Stages / 응용별 ATI |
| [04_필터링·LTA](04_필터링·LTA.md) | §5 | IIR("Beta") 필터 / Counts("AC") 필터 / Long Term Average(LTA) |
| [05_선형화·디바운스·이력](05_선형화·디바운스·이력.md) | §6~7 | Counts 선형화·Base/Target·민감도 / Debounce·Hysteresis |
| [06_전력·기생·부품·EMC·I2C](06_전력·기생·부품·EMC·I2C.md) | §8~12 | Power Modes / Parasitic Cap / Component Choices(Derating) / EMC / I²C Communication |

---

## 빠른 참조

| 항목 | 값/내용 | 파일 |
|---|---|---|
| counts 방향 (Self-Cap) | 접근/접촉 → C 증가 → **counts 감소** | [02](02_센싱기술.md) |
| counts 방향 (Mutual-Cap) | 접촉 → **counts 증가** (부동 도전체 시 Cm 증가 예외) | [02](02_센싱기술.md) |
| ATI 목적 | Resolution(Multiplier) + Offset(Compensation) 자동 튜닝 | [03](03_신호조정·ATI.md) |
| Sensitivity | ∝ Target / Base | [03](03_신호조정·ATI.md) |
| LTA Beta | β=6~10 (Counts 필터 β=0~4보다 느림) | [04](04_필터링·LTA.md) |
| 선형화 식 | `Target² / Raw` (예: 3276750/Raw) | [05](05_선형화·디바운스·이력.md) |
| 전력 모드 주기 | NP 10~50ms · LP 50~200ms · ULP 100~500ms | [06](06_전력·기생·부품·EMC·I2C.md) |
| VREG 디레이팅 | 패키지·전압별 용량 감소(1206 충분 / 0402 4µF 미만) | [06](06_전력·기생·부품·EMC·I2C.md) |
