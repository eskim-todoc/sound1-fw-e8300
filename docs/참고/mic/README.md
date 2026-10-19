---
name: 마이크 데이터시트
purpose: Sound1·Sullivan 1.5 마이크 원문 데이터시트 보관과 분석 문서 안내
type: reference/부품
applies_to: [Sound1]
tags: [마이크, dmic, pdm, knowles, syntiant, 데이터시트]
---

# 마이크 데이터시트

**TL;DR**: 두 제품의 PDM 디지털 마이크 원문이다. **분석·비교·환산표는 [`../오디오 입력 체인/`](../오디오%20입력%20체인/README.md) 에 있다** — 여기에 중복해 두지 않는다.

## 원문

| 제품 | 품번 | 제조 | 파일 |
|---|---|---|---|
| **Sound1** | **SPH0641LM4H-1** | Knowles Electronics (Rev D, 2018) | [`sound1__SPH0641LM4H-1.pdf`](sound1__SPH0641LM4H-1.pdf) |
| **Sullivan 1.5** | **SPK0641HT4H-1** | Syntiant Corp. (Rev D-1, 2024) | [`sullivan1.5__SPK0641HT4H-1.pdf`](sullivan1.5__SPK0641HT4H-1.pdf) |

> [!CAUTION]
> **두 PDF 모두 스펙 표가 이미지다.** 텍스트 레이어에 표 헤더만 있고 셀 값이 없어 **자동 추출로는 수치가 나오지 않는다.** 페이지를 렌더해 눈으로 읽어야 한다.
>
> ```python
> import pymupdf
> d = pymupdf.open('sound1__SPH0641LM4H-1.pdf')
> d[2].get_pixmap(dpi=190).save('p3.png')   # 3쪽 = 음향 스펙
> ```
>
> 스펙 표 위치: **Sound1 은 2~3쪽**(2쪽 일반 · 3쪽 Normal/저전력), **Sullivan 은 2~3쪽**(2쪽 일반+Normal · 3쪽 저전력/슬립).

## 분석 문서

| 문서 | 내용 |
|---|---|
| [**오디오 입력 체인**](../오디오%20입력%20체인/README.md) | **여기부터 읽는다** — 전체 인덱스와 한 장 요약 |
| [01 마이크 스펙 상세](../오디오%20입력%20체인/01_마이크%20스펙%20상세.md) | 두 마이크 전 항목 비교 · 교체 가능성 |
| [03 dB SPL - 코드 환산표](../오디오%20입력%20체인/03_dBSPL-코드%20환산표.md) | 음압 ↔ 24비트 코드 |
| [07 미확정 사항](../오디오%20입력%20체인/07_미확정%20사항과%20측정%20절차.md) | **실무 적용 전에 읽을 것** |

## 30초 요약

| | Sound1 | Sullivan 1.5 |
|---|---|---|
| 감도 | **−26 dBFS** | **−26 dBFS** |
| AOP | **120 dB SPL** | **120 dB SPL** |
| SNR | 64.3 dB(A) | 64.5 dB(A) |
| **PSRR** | 55 dBV/FS | **70 dBV/FS** |

**음향 성능은 사실상 같고, 차이는 전원 잡음 내성뿐이다(Sullivan 15 dB 우세).** 감도가 같아 **양쪽 다 `0x7FFFFF` = 120 dB SPL** 이다.
