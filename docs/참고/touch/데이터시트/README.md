---
name: iqs323-datasheet-index
purpose: IQS323 데이터시트 원문 정리(README + 8파일) 색인 + 빠른 참조표
type: 데이터시트
maturity: stable
tags: [touch, iqs323, datasheet, index, quick-reference]
---

# IQS323 데이터시트 원문 정리 (색인)

> **TL;DR**: Azoteq IQS323 데이터시트(v1.11, 2025-08, 68p)의 **원문 1차 정리물(SSOT)**. 펌웨어 작업 시 PDF 재독 없이 필요한 파일만 참조. 본 README는 8개 파일 색인 + 빠른 참조표(주소·공식·전류·기본값). 개념 해설·Sound1 적용은 [`../레퍼런스/`](../레퍼런스/) 2차 문서 참조.

> [!NOTE]
> 본 폴더는 데이터시트 **원문 충실 정리**다. 해설·비유·Sound1 코드 적용은 [`../레퍼런스/`](../레퍼런스/)(2차), 트러블슈팅은 [`../이슈해결/`](../이슈해결/). 원본 PDF: [`../iqs323_datasheet.pdf`](../iqs323_datasheet.pdf).

---

## 파일 색인

| 파일 | 데이터시트 구간 | 원문 p. | 내용 |
|---|---|---|---|
| [01_개요·전기·타이밍](01_개요·전기·타이밍.md) | §1~4 | 1~14 | 개요·블록도 / 핀맵 3종·신호·레퍼런스 회로 / 절대최대·권장동작·ESD·소비전류 / Reset·MCLR·I²C·Start-Up 타이밍 |
| [02_proxfusion동작](02_proxfusion동작.md) | §5 | 15~20 | 4전력모드 / Count·LTA·Reseed / Filter Beta·Fast Filter Band / Prox·Touch Threshold·Invert·Dual Direction / ATI·Re-ATI·ATI Error / Sensor Setup·Wav Pattern·CalCap |
| [03_하드웨어설정](03_하드웨어설정.md) | §6 | 21~23 | Inactive Rxs / Prox Control(0v5·Cs Size·S/H Bias) / Engine Bias / Dead Time / Charge Transfer Frequency / Reset 3종 |
| [04_부가기능·UI](04_부가기능·UI.md) | §7 | 24~29 | OutA / Slider·제스처 / Reference UI / Release UI / Movement UI / Watchdog |
| [05_i2c인터페이스](05_i2c인터페이스.md) | §8 | 30~34 | I²C 사양·주소·Memory Map / RDY·Comm Window·Timeout / Streaming vs Event Mode / Force Comm / Program Flow |
| [06_레지스터레퍼런스](06_레지스터레퍼런스.md) ★ | §9 + 부록 A | 35~37, 49~64 | **전체 레지스터 맵** — 주소표(기본값) + 비트필드 상세(A.1~A.37) |
| [07_오더링·패키지](07_오더링·패키지.md) | §10~11 | 38~48 | Order Code·Top Marking / 패키지 outline·footprint(WLCSP11·DFN12·QFN20) / Tape&Reel |
| [08_개정이력·이슈](08_개정이력·이슈.md) | 부록 B·C | 65~68 | Revision History(v1.1~v1.11) / **Known Issues**(I²C Settings·I²C Lock Up) / Contact |

---

## 빠른 참조표

### 디바이스 핵심

| 항목 | 값 |
|---|---|
| 구성 | 3ch Self-Cap / 3ch Mutual-Cap / 2ch Inductive ProxFusion® |
| CPU / 클럭 | 14 MHz (f_OSC 13.23~14~14.77 MHz) |
| 전원 | VDD 1.71~3.6 V, VREG nom 1.53 V |
| I²C | Fast-mode-plus ≤1 MHz, 8-bit 주소·16-bit word·**little endian** |
| I²C 주소 | 001/A01 = **0x44** · 002 = **0x58** (debug = LSB invert) |
| Hardware ID (0xE1) | 0xF003 = IQS3dd · 0xF004 = IQS3ed |
| 패키지 | WLCSP11(1.48×1.08) · DFN12(3×3) · QFN20(3×3) |
| 패드 직렬저항 | **470 Ω** (EMI/ESD) · RDY/MCLR cap **1 nF** |

### 핵심 공식 ([02](02_proxfusion동작.md)·[04](04_부가기능·UI.md))

| 항목 | 공식 |
|---|---|
| Prox 진입 (non-inv) | `(LTA − Counts) > Prox Threshold` |
| Touch 진입 / 이탈 | `(LTA − Counts) > Touch Threshold` / `> (Touch Threshold − Touch Hysteresis)` |
| Dual Direction | `Counts > (LTA + Threshold)` 또는 `Counts < (LTA − Threshold)` |
| Damping factor | `Beta / 256` |
| ATI Target | `(div·mult 적용 후 Counts) × (ATI Resolution Factor / 16)` |
| Re-ATI Boundary | `ATI Target ± ATI Band` (Small 1/16 · Large 1/8) |
| Release UI reseed | `(Counts − Activation LTA) > (Delta Snapshot × Release Delta % / 128)` |
| Touch Threshold 값 | `(Threshold × LTA) / 256` |
| Follower Weight | `Weight / 4096` |
| Event/Movement Timeout | `value × 512 ms` |

### 소비 전류 (Event mode, [01](01_개요·전기·타이밍.md) §3.4)

| 모드 | Self-cap(3ch) | Mutual(2ch) | Inductive(1coil) |
|---|---|---|---|
| Normal (1.8V) | 125 µA @16ms | 171 µA @16ms | 128 µA @10ms |
| Low Power | 37.0 µA @60ms | 50.0 µA @60ms | 11.0 µA @80ms |
| Ultra Low Power | 4.00 µA @160ms | 9.00 µA @160ms | 6.50 µA @200ms |
| Halt | 2.00 µA @3000ms | — | — |

### 주요 레지스터 기본값 ([06](06_레지스터레퍼런스.md))

| 레지스터 | 주소(S0/S1/S2) | 기본값 |
|---|---|---|
| Sensor Setup | 0x30/40/50 | 0x0101 |
| Conversion Frequency Setup | 0x31/41/51 | 0x057F |
| Prox Control | 0x32/42/52 | 0x1290 |
| Prox Input and Control | 0x33/43/53 | 0x01CF |
| Pattern Definitions | 0x34/44/54 | 0x030A |
| ATI Setup | 0x36/46/56 | 0x040C |
| ATI Base | 0x37/47/57 | 0x0064 |
| I²C Transaction Timeout | 0xD1 | 0x00C8 (200ms) |

### Wav Pattern (Table 5.2) · PXS Mode (A.7/A.8)

| 측정 타입 | Wav Pattern 0 / 1 | PXS Mode |
|---|---|---|
| Self Capacitance | 0x03 / 0x00 | 0x10 |
| Mutual Capacitance | 0x0E / 0x00 | 0x13 |
| Inductive | 0x0B / 0x00 | 0x3D |
| Current Measurement | — | 0x1D |

(모든 경우 Wav Pattern Select = 0x00)

### ⚠️ Known Issues ([08](08_개정이력·이슈.md))

- **I²C Settings Register** (00x v1.3↓): bit clear 불가 → reset 후 재기록.
- **I²C Lock Up** (00x v1.3↓, A0x v1.4↓): 전 byte 상수 반환 → 매 통신 끝 존재하지 않는 register read, `0xEE` 아니면 hard reset.
