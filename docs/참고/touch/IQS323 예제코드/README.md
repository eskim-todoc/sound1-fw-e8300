---
name: iqs323-example-code-index
purpose: IQS323 Arduino 예제 코드·가이드 정리 색인
type: 데이터시트
maturity: stable
tags: [touch, iqs323, arduino, example-code, driver, index]
---

# IQS323 Arduino 예제 코드 정리 (색인)

> **TL;DR**: Azoteq 공식 **IQS323 Arduino 예제 코드**(드라이버 라이브러리 + 메인 스케치 + EV-Kit 설정 헤더 3종 + Arduino 가이드 PDF v1.5.2)의 정리(SSOT). 드라이버 API·레지스터 초기화 시퀀스·EV-Kit별 설정값을 4개 md로 분할. 현 펌웨어 드라이버 대조·init 시퀀스 참조용. 원본: [`../iqs323-example-code/`](../iqs323-example-code/).

> [!NOTE]
> 본 폴더는 예제 코드·가이드 **원문 충실 정리**다. IQS323 데이터시트는 [`../데이터시트/`](../데이터시트/), Sound1 자체 드라이버는 `src/2__cm3/.../tdc_drv_iqs323.*`. 할루시네이션 검증(원문 대조) 완료 — 01·02 적출 0, 03·04 경미 항목 정정 반영.

---

## 파일 색인

| 파일 | 대상 소스 | 내용 |
|---|---|---|
| [01_개요·파일구조](01_개요·파일구조.md) | `.ino` · README · 트리 | 메인 스케치 setup/loop 흐름, 폴더·파일 구조, EV-Kit 선택 매크로 |
| [02_드라이버_API](02_드라이버_API.md) | `IQS323.cpp/.h` | public 메서드(begin/init/run/queueValueUpdates 등)·enum·메모리맵 구조체·비트 매크로 |
| [03_EV_KIT설정·레지스터](03_EV_KIT설정·레지스터.md) | EV_KIT 헤더 3종 + `inc/IQS323_addresses.h` | 3개 EV-Kit 초기화 레지스터 값 비교 + 레지스터 주소 상수 |
| [04_Arduino가이드](04_Arduino가이드.md) | `iqs323_arduino_guide-v1.5.2.pdf` | 셋업·배선·SparkFun Pro Micro 설정·시리얼 출력·동작 흐름도 |

---

## 빠른 참조

| 항목 | 값/내용 | 파일 |
|---|---|---|
| I²C 주소 (예제 기본) | 0x44 | [02](02_드라이버_API.md)·[03](03_EV_KIT설정·레지스터.md) |
| 핵심 메서드 | `begin` · `init` · `run` · `queueValueUpdates`(18바이트) · `force_I2C`(0xFF/0x00) | [02](02_드라이버_API.md) |
| init 흐름 | POR → Product 확인 → Reset 상태 → Settings write → Re-ATI → ATI 완료 대기 → Read | [04](04_Arduino가이드.md) |
| EV-Kit 선택 | `IQS323_EV_KIT` 매크로(기본 0=Inductive Options) | [01](01_개요·파일구조.md) |

> [!WARNING]
> **EV-Kit 모델 표기 원문 간 상이**: Inductive Options EV-Kit가 코드(`.ino`/헤더)에는 **AZP1212A4**, Arduino 가이드 PDF에는 **AZP1212A3**으로 적혀 있다. 각 md는 자기 출처에 충실(코드 기준 A4, 가이드 기준 A3)하며 상호 참조 노트를 달았다. 실물 리비전은 보유 보드로 확인.
