---
name: iqs323-ds-02-proxfusion동작
purpose: IQS323 데이터시트 §5 ProxFusion Module 원문 정리 — 전력모드·Count·LTA·Filter·Threshold·ATI·Sensor Setup
type: 데이터시트
maturity: stable
tags: [touch, iqs323, datasheet, proxfusion, ati, lta, threshold, sensor-setup]
---

# IQS323 데이터시트 — 02 ProxFusion® Module (§5)

> **TL;DR**: IQS323 핵심 센싱 동작. 4전력모드(NP/LP/ULP/Halt), Count(정전·인덕턴스 반비례)·LTA·Reseed, Filter Beta·Fast Filter Band, Prox/Touch Threshold 공식·Invert·Dual Direction, ATI·Re-ATI·ATI Error, Sensor Setup(PXS Mode·Wav Pattern·CalCap) 원문 정리. 원문 p.15~20.

> [!NOTE]
> 레지스터 비트 상세는 [`06_레지스터레퍼런스.md`](06_레지스터레퍼런스.md), 하드웨어 측 설정(Conversion Freq·Dead Time 등)은 [`03_하드웨어설정.md`](03_하드웨어설정.md).

IQS323은 특허 기술로 센서 데이터를 측정·처리하는 단일 ProxFusion® 모듈을 포함한다.

---

## 5.1 Channel Options

Self-capacitive, mutual-capacitive, reference tracking, inductive 설계 모두 가능. 응용 배경은 application note 참조:

- **AZD004**: Azoteq Sensing Technologies
- **AZD125**: Capacitive Sensing Design Guide
- **AZD115**: Inductive Design Layout Guide

> [!NOTE]
> **개념 해설: 측정 방식 4종**
>
> IQS323이 지원하는 4가지 측정 원리. 방식에 따라 핀 배선, threshold 판정 방향(Invert), 권장 파형 패턴이 달라진다.
>
> | 방식 | 측정 대상 | 주요 특징 | Sound1 사용 |
> |---|---|---|---|
> | **Self-capacitive** | 전극↔GND 간 정전용량 | 단일 핀 Tx·Rx 겸용 | ✓ 사용 |
> | **Mutual-capacitive** | Tx·Rx 두 전극 간 결합 정전용량 | Tx·Rx 핀 분리 | — |
> | **Inductive** | 코일의 인덕턴스 변화 | 금속 물체 감지, 방수 버튼 | — |
> | **Reference tracking** | 환경 드리프트 보상용 기준 채널 | 온도·습도 공통 보정 | — |
>
> ---
>
> **Self-capacitive**
>
> 한 핀(CRx0/CTx0)이 전압 인가(Tx)와 전하 측정(Rx)을 동시에 담당한다. 측정 대상은 전극 패드와 접지(GND) 사이에 형성되는 정전용량이다.
>
> ```
> 노터치: C_total = C_parasitic        ← 기생 정전용량만 존재
> 터치  : C_total = C_parasitic + C_finger  ← 손가락 추가 용량 합산
> ```
>
> 총 정전용량이 커지면 counts가 감소한다(§5.4 참고). **==터치 시 counts 감소==** 방향이므로 기본 판정 공식 `(LTA − Counts) > Threshold`가 추가 설정 없이 동작한다.
>
> ---
>
> **Mutual-capacitive**
>
> CTx 핀에서 교류 파형을 송신하고 CRx 핀에서 두 전극 간 결합 정전용량(Cm)을 수신·측정한다. 손가락(접지된 도체)이 접근하면 Tx→Rx 전기력선 일부가 손가락으로 빠져나가 Cm이 감소한다.
>
> Cm 감소 → 한 번의 전달에서 이전되는 전하 Q = Cm·V 감소 → 기준 커패시터 Cs를 채우는 데 더 많은 반복 필요 → **==counts 증가==**
>
> 터치 시 counts가 증가하므로 기본 판정 공식이 반대로 동작한다. **==Invert bit 설정 필수==** (§5.7 참고).
>
> ---
>
> **Inductive**
>
> 코일 전극을 사용한다. 금속 물체가 접근하면 와전류(eddy current)가 발생해 코일의 유효 인덕턴스가 변화하고, 이 변화를 counts로 읽는다. 터치 센서 용도보다는 비접촉 금속 감지나 방수 케이스 관통 버튼에 활용한다.
>
> ---
>
> **Reference tracking**
>
> 사용자 터치가 닿지 않되 동일한 환경(온도·습도)에 노출된 위치에 기준 채널을 배치한다. 이 채널의 LTA 변화량을 주 채널 LTA에서 실시간으로 차감해 공통 드리프트를 제거한다. 장시간 착용 감지 등 환경 안정성이 중요한 응용에 사용한다.

## 5.2 Low Power Options

IQS323은 **4개 전력 모드** 제공:

| 모드 | 설명 |
|---|---|
| **Normal power (NP)** | 일반 동작 |
| **Low power (LP)** | 보통 NP보다 느린 rate |
| **Ultra-low power (ULP)** | 최적화 펌웨어 설정. **단일 채널에서 deep sleep로부터 급속 wake-up**(예: 분산 근접 이벤트, 접근 사용자 즉시 응답). 타 센서 채널은 소비 최적화 위해 더 느린 rate로 샘플 |
| **Halt** | conversion·processing 없는 deep sleep |

- NP/LP/ULP는 AZD004 참조.
- **Halt 탈출**: force communications request(§8.13) 필요 + 다음 통신 윈도에서 power mode 변경. 최저 소비 위해 `Halt Mode Report Rate`에 `3000`(ms) write 권장.
- 현재 활성 전력 모드는 `System Status`에 보고.
- **"power mode timeout"보다 긴 채널 time-out** → 채널 timeout이 reset되고 power mode가 한 단계 낮춰짐. 전력 모드 변경은 채널 touch/prox 상태에 영향 없음(장기간 트리거 추적 + 소비 최적 양립).

> [!NOTE]
> **§5.2 Low Power Options — 회로 원리**
>
> 4개 모드의 소비 전류 차이는 **충전 전달(Charge Transfer) 사이클을 얼마나 자주 실행하느냐**에서 비롯된다.
>
> 한 번의 conversion(측정)을 위해 IC는 CTx 핀 드라이버를 활성화하고, 내부 비교기(comparator)를 켜고, Cs를 충전·방전하는 일련의 아날로그 블록을 구동한다. 이 블록들이 동작하는 시간 비율(duty cycle)이 평균 소비 전류를 직접 결정한다.
>
> | 모드 | 동작 원리 | 소비 전류 목안 |
> |---|---|---|
> | **NP** | 매 Report Rate마다 모든 채널 전체 conversion | 수백 µA 수준 |
> | **LP** | Report Rate 간격이 NP보다 길어 아날로그 블록 off 시간 증가 | NP 대비 수십 배 절감 |
> | **ULP** | **하나의 wake-up 채널만 짧은 주기로 샘플, 나머지 채널은 훨씬 긴 주기로 샘플** | 단일 자릿수 µA 가능 |
> | **Halt** | CTx 드라이버·비교기·클록 모두 off, I²C 슬레이브만 간헐 동작 | 최저 (수 µA 이하) |
>
> **ULP에서 단일 채널 wake-up이 가능한 이유**
>
> IQS323은 ULP 진입 시 지정된 채널(예: CH0 = distributed proximity 채널)만 짧은 주기로 계속 샘플링하고, 나머지 채널은 매우 긴 주기(데이터시트 원문: "more slowly"로 명시)로 저속 샘플링을 유지한다. Wake-up 채널은 전극 면적을 넓게 잡아 손 접근 시 발생하는 수 pF 수준의 정전용량 변화를 넓은 범위에서 검출할 수 있다. 이 채널에서 prox 이벤트가 발생하면 IC는 NP로 즉시 복귀해 전체 채널 고속 샘플링을 재개한다. "빠른 파수꾼 1명 + 느린 보조원들"의 계층적 감시 구조다.
>
> **Halt의 탈출 비용**
>
> Halt에서는 RDY 핀이 고정(폴링 불가)되어 정상 I²C 통신 윈도가 열리지 않는다. Force Communications 요청(§8.13)은 SDA 라인을 특정 패턴으로 구동해 IC를 깨우는 하드웨어 트릭이다. 이 탈출 비용이 있으므로 Halt는 사용자가 장시간 부재할 때만 진입하며, `Halt Mode Report Rate = 3000ms`를 설정해 I²C 슬레이브가 깨어 있는 최소 주기를 3초로 늘린다.

> [!NOTE]
> **§5.2 Low Power Options — SW 구현 관점**
>
> 전력 모드 전환은 `System Control` 레지스터의 `Power Mode` 필드로 제어한다. Automatic으로 설정하면 IC가 NP→LP→ULP로 단계 강하하며, touch/prox 이벤트 발생 시 자동으로 NP로 복귀한다. Halt는 RDY 핀이 고정되어 통상 I²C 윈도가 열리지 않으므로 `Force Communications` 요청(§8.13)으로 강제 윈도를 열어야 모드 변경 명령 전달이 가능하다. Halt 사용 시 초기화 시퀀스에서 `Halt Mode Report Rate = 3000ms`를 명시적으로 기록해 소비 전류를 최소화한다. 현재 활성 모드는 `System Status`의 `Current Power Mode` 필드에서 이벤트 처리마다 확인한다.

## 5.3 Power Mode Selection

`System Control`의 `Power Mode` 필드로 선택.

- **'Automatic'**: `Power Mode Timeout`이 지정한 시간 동안 상호작용 미감지 시 더 효율적 모드로 stepped. `Power Mode Timeout`='0x00'이면 power mode 미강하.
- **'Automatic No ULP'**: 'Automatic'과 동일하나 **ULP에는 절대 진입하지 않음**.
- 'Automatic'/'Automatic No ULP' 동안 이벤트 트리거 시 현재 모드 무관하게 **NP로 복귀**, 이후 자동 전환 재개.

> [!NOTE]
> **§5.3 Power Mode Selection — 회로 원리**
>
> **Automatic 모드의 stepped 전환 조건**
>
> IC 내부에는 타이머가 있다. 마지막 prox/touch 이벤트로부터 `Power Mode Timeout` 시간이 경과할 때마다 NP→LP→ULP 순서로 한 단계씩 강하한다. "stepped"란 한 번에 NP→ULP로 건너뛰는 것이 아니라 단계적으로 강하한다는 의미다. 각 단계에서 아날로그 블록의 duty cycle이 낮아지므로 소비 전류가 계단식으로 줄어든다.
>
> **이벤트 시 NP로 즉시 복귀하는 이유**
>
> 터치 이벤트가 감지되면 이후 단기간 내에 추가 인터랙션(연속 버튼 조작, 슬라이더 드래그 등)이 일어날 가능성이 높다. 이 시점에 LP나 ULP에 머무르면 샘플링 주기가 길어 다음 이벤트를 늦게 감지한다. NP로 즉시 복귀해 고속 샘플링을 재개하면 연속 인터랙션을 실시간에 가깝게 처리할 수 있다. 즉 "이벤트 → NP 복귀 → Timeout 경과 → 단계 강하"가 반복되는 적응형 전력 관리 루프다.
>
> **Automatic No ULP의 존재 이유**
>
> Channel Timeout(§5.8)을 함께 쓸 경우 데이터시트가 ULP 모드 사용을 명시적으로 금지한다. 이 조합에서는 Automatic No ULP를 강제 선택해 ULP를 차단한다.

> [!NOTE]
> **§5.3 Power Mode Selection — SW 구현 관점**
>
> `Power Mode Timeout = 0x00`은 모드 강하를 완전히 막는다 — 디버깅 중 항상 NP를 유지하고 싶을 때 임시로 활용한다. Channel Timeout(§5.8)을 함께 사용하는 경우 ULP 진입이 충돌을 일으키므로 반드시 `Automatic No ULP`를 선택해야 한다. 수동 고정(Manual) 설정은 테스트 목적 외에 제품 펌웨어에서는 피하고, Automatic 계열에서 이벤트 복귀 흐름을 신뢰하는 것이 안전하다. 초기화 완료 시 `Power Mode` 필드의 실제 기록값을 `System Status`로 검증하는 assert를 두면 설정 누락을 조기에 잡을 수 있다.

## 5.4 Count Value

각 채널 측정은 **counts 값**(raw 신호)을 반환. **Count는 capacitance·inductance에 반비례**하며 모든 다른 출력이 이로부터 파생. `Filtered Counts` register에 보고.

> [!NOTE]
> **개념 해설: Counts가 정전용량에 반비례하는 이유**
>
> IQS323은 **충전 전달(Charge Transfer)** 방식으로 정전용량을 숫자로 변환한다. 동작 원리는 아래와 같다.
>
> **충전 전달 사이클 (1회)**
> 1. 측정 전극(Cx)에 전압을 인가해 전하를 축적한다.
> 2. Cx의 전하를 내부 기준 커패시터(Cs)로 전달한다.
> 3. Cs 전압이 임계값에 도달하면 Cs를 방전하고 **1 카운트를 기록**한다.
> 4. 이 과정을 반복한다. 최종 Counts = 임계값에 도달하기까지 소요된 총 사이클 수.
>
> ```
> ┌─────────────────────────────────────────┐
> │    충전 전달 사이클 (1회)               │
> │                                         │
> │  ① Cx에 전압 인가 → 전하 축적          │
> │         │                               │
> │         ▼                               │
> │  ② Cx 전하를 Cs로 전달                 │
> │         │                               │
> │         ▼                               │
> │  ③ Cs 전압 ≥ 임계값?                   │
> │     YES → Cs 방전 + 1카운트 ↑ → ①     │
> │     NO  → 계속 전달 (②반복)            │
> └─────────────────────────────────────────┘
>
> 측정 시간 고정 → 완료된 사이클 수 = Counts
> ```
>
> **왜 정전용량이 클수록 counts가 적어지는가**
>
> 전하량 공식 Q = C·V에서, Cx가 클수록 한 번의 전달에서 Cs로 이동하는 전하량(Q)이 많아진다. 즉 Cs가 임계값에 더 빨리 도달하므로 더 적은 횟수로 완료된다.
>
> ```
> 측정 시간 (고정)
> │────────────────────────────│
>
> 노터치 (C 작음):
> ──┬──┬──┬──┬──┬──  사이클 5회 → Counts = 5 (높음)
>
> 터치   (C 큼):
> ──┬───────┬──────  사이클 2회 → Counts = 2 (낮음)
>
> Counts ∝ 1/C
> ```
>
> ```
> 노터치 (C 작음): 1회 전달 전하 少 → Cs 천천히 채워짐 → 사이클 많이 필요 → Counts 높음
> 터치   (C 큼):   1회 전달 전하 多 → Cs 빠르게 채워짐 → 사이클 적게 필요 → Counts 낮음
> ```
>
> Counts ∝ 1/C — 이것이 "capacitance에 반비례"의 의미다.
>
> Inductive 측정에서 인덕턴스(L)도 마찬가지 반비례 관계를 가진다.
>
> ```
>       Counts
>       ↑
>   500 ┤ ·············  ← 노터치 LTA (기준선)
>   400 ┤                   (C 작음 → 사이클 많음)
>   300 ┤ ─────────────  ← 터치 중 Counts
>       │                   (C 큼 → 사이클 적음)
>       └──────────────→ 시간
> ```

> [!NOTE]
> **Counts 정의 명확화와 흔한 오개념**
>
> **카운트의 정확한 정의**
>
> ```
> 카운트 = Cs가 임계값에 도달하기까지 필요한 전달 횟수
>
> 노터치: ──┬──┬──┬──┬──┬──  (10번 전달 후 임계값 도달) → Counts = 10
> 터치  : ──┬─────┬─────      ( 4번 전달 후 임계값 도달) → Counts =  4
> ```
>
> Cs가 임계값에 도달하면 1카운트 기록 후 Cs가 방전되며, 이후 다음 측정 주기가 시작된다.
>
> **Cx가 크면 카운트가 작아지는 두 가지 경로**
>
> ```
> Cx가 크다 (터치)
>     │
>     ├─① 저장 전하 Q = Cx × VDD가 많음
>     │       → 사이클당 Cs 이동 전하량 많음
>     │       → Cs 임계값 도달까지 전달 횟수 적음
>     │       → 카운트 작음  ↓
>     │
>     └─② 충전에 시간이 오래 걸림 (τ = R × Cx)
>             → 1 사이클 시간 증가
>             → 고정 측정 시간 내 총 사이클 수 감소
>             → 카운트 작음  ↓
>
> 두 효과 모두 같은 방향 → Counts ∝ 1/Cx (반비례)
> ```
>
> 두 경로는 인과관계가 아니라 "Cx가 크다"는 같은 원인에서 나오는 독립적인 결과다.
>
> **흔한 오개념: "Cs가 빨리 차면 카운트가 커야 하는 것 아닌가?"**
>
> ```
> ❌ 오개념: 카운트 = Cs 방전 횟수 (고정 시간 내 얼마나 자주 방전됐나)
>    Cx 큼 → Cs 빨리 참 → 방전 자주 발생 → 카운트 많음?
>
> ✅ 실제:   카운트 = Cs 임계값 도달까지 전달 횟수
>    Cx 큼 → Cs 빨리 참 → 전달을 적게 해도 도달 → 카운트 작음
> ```
>
> "방전이 자주 일어난다"와 "방전에 도달하기까지 전달 횟수가 적다"는 같은 사실을 다른 방향에서 표현한 것이다. IQS323이 보고하는 Counts는 후자(전달 횟수)다.
>
> **Cs와 Vref 개념 정리**
>
> | 항목 | 정의 | 비고 |
> |---|---|---|
> | **Cs** | IC 내부 기준 커패시터 | `Prox Control` 레지스터 `Cs Size` 필드(bit12)로 40pF/80pF 선택 |
> | **Cx** | 외부 터치 전극 정전용량 | 기생 C + 터치 시 손가락 C |
> | **Vref** | IC 내부 비교기 기준 전압 | 데이터시트 **비공개** — AZD004 앱노트 참조 |
>
> **Cs Size가 민감도를 조절하는 원리 (V = Q/C)**
>
> ```
> Cs = 40pF (작음): 이동 전하 Q → 전압 상승 빠름 → Vref 빨리 도달 → 카운트 적음 (민감)
> Cs = 80pF (큼):   이동 전하 Q → 전압 상승 느림 → Vref 늦게 도달 → 카운트 많음 (둔감)
> ```
>
> Cs Size는 임계값이 아닌 **커패시터 물리 크기** 설정이다. V = Q/C에서 C가 크면 같은 전하량으로 전압 상승이 작아져 Vref 도달에 더 많은 전달 횟수가 필요해진다.
>
> CRx0~2 앞단의 외부 캡(예: 100pF)은 Cs가 아닌 **VREG 디커플링 캡** — 측정 경로와 무관.

### 5.4.1 Linearise Counts

`Sensor Setup`의 `Linearise Counts` bit set 시 counts를 보고 전 linearise. 이 옵션 set 시 **counts가 inverted**되므로 `Invert` bit를 적절히 set해 채널 로직 정합. **Release UI(§7.4) 사용 시 특히 권장**.

> [!NOTE]
> **개념 해설: 왜 선형화가 필요하고 왜 Invert가 필요한가**
>
> **선형화가 필요한 이유**
>
> §5.4에서 확인했듯 Counts ∝ 1/C 관계, 즉 C와 쌍곡선(반비례) 관계다. 실제 터치 세기(정전용량 변화량)에 비례하는 선형 값이 필요한 경우 이 쌍곡선 관계가 불편하다.
>
> 슬라이더를 예로 들면: 채널별 delta 비율로 손가락 위치를 내삽(centroid interpolation)할 때, 값이 쌍곡선이면 양 끝과 중간에서 기울기가 달라 위치 오차가 발생한다. 선형화하면 기울기가 균일해져 정밀한 위치 계산이 가능하다.
>
> **선형화 수식** (AZD004 §6.1):
> ```
> Linearised Counts = ATI_Target² / Raw Counts
> ```
> 역수 연산이므로 결과는 C에 **비례**한다 — 터치 강도가 셀수록 값이 커진다.
>
> **왜 Invert bit가 함께 필요한가**
>
> 선형화 전: 터치 시 Counts 감소 (C↑ → 1/C↓)
> 선형화 후: 터치 시 Linearised Counts 증가 (1/C의 역수 → C에 비례 → C↑면 값↑)
>
> 값의 방향이 반전된다. 기본 판정 공식 `(LTA − Counts) > Threshold`는 Counts가 감소할 때 양수 delta가 만들어지도록 설계되어 있으므로, 방향이 뒤집히면 Invert bit로 판정 방향도 함께 뒤집어야 한다.
>
> Sound1은 선형화를 사용하지 않는다.

### 5.4.2 Max Counts

각 채널은 `Max Counts`(`Prox Control`) 한계 미만으로 제한. ATI 설정·하드웨어가 한계 초과 count를 유발하면 conversion 정지, max value가 read됨(에러 조건에서 stuck 방지). 정상 동작 예상 max counts 위의 가장 작은 max counts 설정 선택 권장. `Linearise Counts` set 시 linearisation이 max counts 적용 **후** 일어나므로 max counts 초과 값이 보고될 수 있음.

## 5.5 Reference Value / Long-Term Average (LTA)

사용자 상호작용은 측정 count를 기준값 **LTA(Long Term Average)** 와 비교해 감지. LTA는 환경 변화 추적 위해 천천히 갱신되며 **touch·proximity 이벤트 중에는 frozen**. 채널 LTA는 `Channel X LTA` register에 보고.

> [!NOTE]
> **개념 해설: LTA의 역할과 동작**
>
> LTA는 "아무것도 건드리지 않은 상태"의 counts 기준선이다. 터치 감지는 `Delta = LTA − Counts`를 threshold와 비교하는 방식으로 이루어진다.
>
> LTA가 필요한 이유: 온도·습도·먼지 등 환경 변화로 noTouch 상태의 counts 값이 천천히 바뀐다. 고정된 기준값을 쓰면 환경 변화에 따라 false touch 또는 미감지가 발생한다. LTA는 이런 느린 drift를 자동으로 추적해 항상 현재 환경의 "기준선"을 유지한다.
>
> **LTA 갱신 방식** (IIR 필터 — §5.6 참고):
> ```
> LTA_new = LTA_old + (Counts − LTA_old) × (Beta / 256)
> ```
> 매 측정마다 현재 Counts 쪽으로 아주 조금씩 이동한다. 환경이 천천히 변하면 LTA도 따라가지만, 갑작스러운 변화(터치)는 반영하지 않는다.
>
> ```
>   Counts/LTA
>   ↑
>   │    노터치 구간         터치 구간     노터치 복귀
>   │                    ┌──────────┐
> 500┤ ·····▽·····▽·····  │          │  ····▲····
>   │      ↓ LTA 천천히   │ LTA 동결 │      ↑ LTA
>   │      추적           │ (갱신 중단)│      재개
>   │ ─────────────────   │          │ ──────────
> 300┤                    │ Counts   │
>   │                    └──────────┘
>   └────────────────────────────────→ 시간
>
>   ▽ = LTA가 조금씩 Counts 방향으로 이동 (IIR)
>   터치 중 LTA ≠ Counts → Delta 유지 → 터치 상태 유지
> ```
>
> **터치 중에 LTA가 동결되는 이유**
>
> 터치 상태에서 LTA가 계속 갱신되면 터치된 counts(낮은 값)를 기준선으로 학습해버린다. 그러면 터치를 떼어도 Delta가 threshold 미만이 되어 touch 해제를 인식하지 못한다. 이를 막기 위해 터치·근접 이벤트 중에는 LTA 갱신을 멈춘다.

### 5.5.1 Reseed

LTA의 수동 reseed가 필요한 상황이 있을 수 있음. Reseed는 최신 측정 counts를 취해 LTA를 그 값으로 seed → 외부 환경 최신 조건에 LTA 일치. `System Control`의 `Reseed` bit set으로 명령, 완료 시 **자동 clear**.

## 5.6 Filter Betas

raw 입력(counts·LTA)에 **IIR(Infinite Impulse Response) 필터** 적용. Damping 옵션은 `Counts Filter Betas`, `LTA Filter Betas`, `LTA Fast Filter Betas` register에 정의.

$$\text{Damping factor} = \frac{\text{Beta}}{256}$$

- **NP filter betas**: `Current Power Mode`(`System Status`)='Normal Power'일 때 사용.
- **LP filter betas**: 'Low Power'·'Ultra Low Power'일 때 사용.
- **Fast Filter Band**: fast beta 필터 사용 시점 결정. counts가 LTA로부터 **sensing 반대 방향으로 Fast Filter Band 이상** drift하면 fast filtering 적용. counts↔LTA 차이가 fast filter band 미만이 되면 normal filter로 복귀.

> [!NOTE]
> **개념 해설: IIR 필터와 Beta 의미**
>
> **IIR 필터란**
>
> IIR(Infinite Impulse Response)는 과거 출력값을 다시 입력으로 사용하는 재귀 필터다. 이 맥락에서는 1차 저역통과 필터(Low-Pass Filter)로 동작하며, 갑작스러운 노이즈를 흡수하고 천천히 변하는 신호만 통과시킨다.
>
> 소프트웨어 관점에서 보면 "지수이동평균(EMA, Exponential Moving Average)"과 동일하다:
> ```c
> // alpha = Beta / 256
> filtered = filtered + (raw - filtered) * alpha;
> ```
>
> **Damping factor (= Beta / 256) 의미**
>
> `alpha = Beta / 256` 이 한 번의 측정에서 새 값이 이전 필터링 값에 얼마나 반영되는지를 결정한다.
>
> | Beta | alpha (= Beta/256) | 현재 입력 가중 | 특성 |
> |---|---|---|---|
> | 1 | ≈ 0.4% | 0.4% 반영, 99.6% 과거 유지 | 매우 느린 추적, 강한 스무딩 |
> | 4 | ≈ 1.6% | 1.6% 반영, 98.4% 과거 유지 | 느린 추적 |
> | 8 | ≈ 3.1% | 3.1% 반영 | 중간 |
> | 15 | ≈ 5.9% | 5.9% 반영 | 상대적으로 빠른 추적 |
>
> Beta가 클수록 현재 측정값이 더 많이 반영되어 빠르게 변화를 추적한다. Beta가 작을수록 과거 값의 가중이 커져 노이즈에 강건하지만 반응이 느리다.
>
> Beta 레지스터는 4비트(0~15)이므로 실제 가용 alpha는 최대 약 5.9%다. 즉 항상 강한 스무딩이 적용된다.
>
> **NP/LP를 각각 설정하는 이유**
>
> NP는 샘플링 주기가 짧아 많은 측정값이 들어오므로 동일한 Beta라도 추적 속도가 LP보다 빠르다. 전력 모드별로 Beta를 따로 설정해 모드가 바뀌어도 필터 동작이 일관되게 유지되도록 한다.

## 5.7 Proximity and Touch Thresholds

각 채널은 독립 설정 가능한 prox·touch threshold 보유. 채널의 counts·LTA와 함께 prox/touch 상태 판정. 상태 진입 시 `System Status`의 `CHx Prox`·`CHx Touch` bit가 set되어 이탈까지 유지.

**Non-inverted 로직 + dual direction disabled 기준:**

- **Proximity 진입**: `Prox Debounce Enter`(`Prox Settings`)가 지정한 연속 샘플 수보다 많이 아래 만족 시
$$(LTA - Counts) > \text{Prox Threshold}$$
- **Proximity 이탈**: `Prox Debounce Exit` 지정 연속 샘플 수보다 많이 위 조건 미충족 시. `Prox Threshold`는 `Prox Settings`.
- **Touch 진입**:
$$(LTA - Counts) > \text{Touch Threshold}$$
- **Touch 이탈**:
$$(LTA - Counts) > (\text{Touch Threshold} - \text{Touch Hysteresis})$$
  `Touch Threshold`·`Touch Hysteresis`는 `Touch Settings`.

**Invert** (`Sensor Setup`): 위 로직 반전. **mutual-capacitance·inductance 센싱 시 사용자 상호작용으로 counts가 증가**(self-cap은 감소)하므로 필요.

**Dual Direction** (`Sensor Setup`): set 시 양방향 적용 — 채널이 prox/touch 상태가 되는 조건:
$$\text{Counts} > (LTA + \text{Threshold}) \quad \text{또는} \quad \text{Counts} < (LTA - \text{Threshold})$$

> [!NOTE]
> **§5.7 Debounce Enter/Exit — 회로 원리**
>
> **Debounce가 없으면 왜 false touch가 발생하는가**
>
> 정전용량 측정 시스템은 전극 주변의 전기장을 감지하므로, 기계적 진동·전원 노이즈·RF 간섭·정전기 방전(ESD) 등이 수 나노초~수 밀리초 단위의 순간적인 counts 변화를 일으킬 수 있다. 이 스파이크가 threshold를 단 1회 초과하는 것만으로도 터치 이벤트가 발생해 오동작으로 이어진다.
>
> **연속 샘플 수 누적 방식**
>
> Debounce Enter 값을 N으로 설정하면 threshold 조건이 **N+1회 연속**으로 충족되어야 비로소 상태가 진입한다. 내부 카운터가 매 샘플마다 증가하고, 조건이 한 번이라도 미충족되면 **카운터가 0으로 리셋**된다. 이 리셋 조건이 핵심이다.
>
> ```
> 샘플 시계열 예 (N=2, 즉 3회 연속 필요):
>
> 샘플:   1      2      3      4      5
> delta:  [노이즈] [노이즈] [터치]  [터치]  [터치]
> 조건:   초과    미달    초과    초과    초과
> 카운터: 1      0      1      2      3 → 진입
>                ↑ 리셋
> ```
>
> ```
> 노이즈 스파이크 (Debounce Enter=2):
>
> 샘플:  1    2    3    4    5
> 조건:  ✓    ✗    ✓    ✓    ✓
> 카운터: 1    0    1    2    3 → 터치 진입
>         └→리셋  └→ 연속 누적 →┘
>
> 노이즈만인 경우:
>
> 샘플:  1    2    3    4    5
> 조건:  ✓    ✗    ✓    ✗    ✓
> 카운터: 1    0    1    0    1  → 진입 없음
>         └→리셋    └→리셋
> ```
>
> 노이즈 스파이크는 연속성이 없어 카운터가 누적되지 않는다. 실제 손가락 터치는 수십 ms 이상 지속되므로 여러 샘플에 걸쳐 연속 조건을 충족한다.
>
> **Enter vs Exit Debounce를 분리하는 이유**
>
> 진입 Debounce가 크면 응답 지연이 늘어나므로 작게 설정하고, 이탈 Debounce는 조금 크게 설정해 손가락을 빠르게 떼는 순간의 경계 진동(bouncing)을 흡수한다. 두 값을 독립 설정할 수 있어 응답성과 안정성을 별도 조율할 수 있다.
>
> **샘플링 주기와의 관계**
>
> NP에서 Report Rate = 20ms(50 Hz)이면, Enter=2(3회 연속)는 최소 60ms의 응답 지연을 의미한다. LP에서 100ms 주기라면 동일 Enter=2가 300ms 지연으로 늘어난다. 전력 모드에 따라 체감 응답성이 달라지므로 설계 시 고려 필요.

> [!NOTE]
> **§5.7 Debounce — SW 구현 관점**
>
> Debounce는 `Prox Settings` 레지스터의 `Prox Debounce Enter` / `Prox Debounce Exit` 필드와 `Touch Settings`의 대응 필드에서 설정한다 (각 4비트, 0~15). Enter=N이면 N+1회 연속 조건 충족이 필요하므로 응답 지연이 샘플 주기 × (N+1)이 된다. 예: 50 Hz(20ms 주기) 기준 Enter=0→최소 20ms, Enter=1→40ms, Enter=2→60ms. Exit 값이 너무 높으면 터치를 뗀 뒤에도 상태가 과도하게 유지되어 연속 터치가 묶인다. 단순 버튼 감지 용도라면 Enter=2~3, Exit=1~2가 출발점으로 적합하다. 레지스터 기록 위치는 ATI 완료 후, 본격 측정 전 초기화 시퀀스 말미다.

> [!NOTE]
> **개념 해설: Invert와 Dual Direction**
>
> **Invert bit가 필요한 이유**
>
> 기본 판정 공식 `(LTA − Counts) > Threshold`는 counts가 LTA 아래로 내려갈 때, 즉 **counts 감소** 시 양수 delta가 만들어지도록 설계되어 있다. Self-cap 터치(counts 감소)에 최적화된 공식이다.
>
> Mutual-cap과 Inductive에서는 터치 시 counts가 **증가**한다(§5.1 참고). 이 경우:
> ```
> Delta = LTA − Counts
> 터치 → Counts↑ → Delta < 0 → threshold 조건 미충족 → 터치 미감지
> ```
> Invert bit를 설정하면 판정 방향이 반전되어 counts 증가 시에도 올바르게 터치를 감지한다.
>
> 정리하면:
> | 방식 | 터치 시 Counts | Invert bit |
> |---|---|---|
> | Self-cap | 감소 (LTA−Counts > 0) | 0 (비활성) |
> | Mutual-cap | 증가 (LTA−Counts < 0) | 1 (활성) |
> | Inductive | 증가 또는 감소 (구현 의존) | 맞춰 설정 |
>
> **Dual Direction의 사용 사례**
>
> 한 채널이 양방향 변화를 모두 감지해야 할 때 사용한다. 예: 금속 물체가 접근할 때와 멀어질 때 모두 이벤트가 필요한 경우, 또는 기준값 위아래로 변화하는 차동 측정 구성.

## 5.8 Channel Timeouts

채널이 `Event Timeouts` register가 지정한 시간보다 오래 prox/touch 상태면 reseed되어 상태 이탈. event timeout은 모든 채널에 적용되며 `System Control`의 `CHx Timeout Disable` bit로 채널별 비활성 가능.

> [!WARNING]
> 채널 prox/touch timeout 사용 시 **ULP 모드 금지**. 자동 전력 전환은 'Automatic No ULP'로 설정. (원문 각주)

> [!NOTE]
> **§5.8 Channel Timeouts — 회로 원리**
>
> **터치 상태가 오래 유지될 때 왜 문제가 생기는가**
>
> LTA는 터치/prox 이벤트 중에 동결된다(§5.5). 손가락이 계속 올라가 있으면 LTA 갱신이 수십 초~수 분간 멈춘다. 이 사이 온도·습도 등 환경 요인으로 "진짜 noTouch counts" 기준이 서서히 바뀌지만 LTA는 그 변화를 반영하지 못한다. 손가락을 떼었을 때 현재 counts가 동결된 LTA와 여전히 크게 차이나면 touch 이탈 조건 `(LTA − Counts) < (Touch Threshold − Hysteresis)`이 충족되지 않아 **stuck touch(유령 터치)** 상태가 된다.
>
> 또 다른 문제는 **물체가 전극 위에 올려진 경우**다. 예: 가방 안 기기에서 어떤 물체가 전극을 지속적으로 누르고 있으면, LTA 동결 + 터치 상태 유지가 무한 지속되어 IC가 사용자 인터랙션에 응답하지 않는 데드락에 빠진다.
>
> **Timeout 후 Reseed 메커니즘**
>
> `Event Timeouts`로 지정된 시간이 경과하면 IC는 두 가지 동작을 동시에 수행한다:
> 1. **Reseed** — 현재 counts 값을 그대로 LTA에 기록해 "지금 이 상태가 noTouch 기준"으로 재정의한다.
> 2. **Touch/Prox 상태 강제 해제** — CHx Prox·CHx Touch bit를 clear한다.
>
> Reseed 후 LTA는 현재 counts에 일치하므로 Delta = 0이 되어, 물체가 남아 있더라도 threshold를 넘지 않는다. 사용자가 실제로 새로 터치할 때 비로소 다시 Delta가 쌓여 이벤트가 발생한다. 이것은 환경 baseline 강제 재동기화다.
>
> **Timeout 값 선택 기준**
>
> 정상 최대 터치 지속 시간(예: 롱프레스 3초)보다 충분히 길고, 비정상 stuck(예: 10초 이상) 전에 Reseed가 일어나도록 설정한다. 너무 짧으면 긴 롱프레스가 중단되고, 너무 길면 stuck 복구가 늦다.

> [!NOTE]
> **§5.8 Channel Timeouts — SW 구현 관점**
>
> `Event Timeouts` 레지스터에 Prox/Touch 각각의 최대 지속 시간을 설정한다. 해당 시간 초과 시 해당 채널은 강제 Reseed 후 상태가 해제되므로, 물체가 전극 위에 계속 올려진 채로 stuck되는 상황을 방지하는 watchdog 역할을 한다. 채널별 비활성화는 `System Control`의 `CHx Timeout Disable` bit를 set한다. **ULP 금지 규칙**: 데이터시트 원문은 "채널 prox/touch timeout 사용 시 ULP 모드 금지"를 명시한다(§5.2 각주). Channel Timeout을 활성화한 펌웨어는 §5.3에서 반드시 `Automatic No ULP`를 선택해야 한다.

## 5.9 Automatic Tuning Implementation (ATI)

ATI는 외부 부품 변경 없이 광범위 센싱 전극 정전·인덕턴스에서 최적 성능을 제공하는 ProxFusion® 기술. ATI 알고리즘이 각 채널의 divider·multiplier·compensation을 선택.

- `ATI Setup`의 `ATI Mode`='Full'일 때, ATI 알고리즘이 `ATI Base`를 입력으로 `Coarse/Fine Fractional Divider`·`Coarse/Fine Fractional Multiplier`(`ATI Multipliers and Dividers`)를 set. coarse 먼저, fine 나중. 낮은 base value → 감도↑.
- 각 채널 `Compensation Value`·`Compensation Divider`(`Compensation`)는 `ATI Setup`의 `ATI Resolution Factor`로 set. 높은 resolution factor → 일반적으로 감도↑.
- ATI 트리거 시 먼저 divider·multiplier로 counts를 `ATI Base`에 최대한 근접시키고, 그 다음 `Compensation Value`·`Compensation Divider`로 ATI Target에 근접:

$$\text{ATI Target} = (\text{divider·multiplier 적용 후 Counts}) \times \frac{\text{ATI Resolution Factor}}{16}$$

- 설계 시 일부/전체 divider·multiplier 고정을 원하면 `ATI Mode`를 'ATI from Fine Fractional Divider', 'ATI from Compensation Divider', 'Compensation Only'로 설정.
- **conversion frequency > 2 MHz**: `Compensation Value` 최소화 + `Compensation Divider` 최대화(또는 둘 다 '0'). 둘 다 '0'은 `ATI Resolution Factor`='16' + ATI enable, 또는 ATI disable + `Compensation Value`·`Compensation Divider`='0'으로 달성.
- **`ATI Mode`='Full' 권장**(divider·multiplier·compensation 자동 선택). ATI 알고리즘은 짧은 시간에 실행되어 사용자가 인지 못 함. 변수 선택 상세는 AZD004 §4.4.

> [!NOTE]
> **§5.9 ATI Mode 5가지 옵션 — 회로 원리**
>
> ATI 보정은 **MULT 단계**(Coarse/Fine Fractional Divider·Multiplier)와 **COMP 단계**(Compensation Value·Divider)의 2단계로 이루어진다. 5가지 모드는 이 두 단계 중 어느 것을 알고리즘이 자동으로 결정하고 어느 것을 수동 고정값으로 쓰느냐의 조합이다.
>
> | ATI Mode | MULT 조정 | COMP 조정 | 언제 사용하는가 |
> |---|---|---|---|
> | **Full** | 알고리즘 자동 | 알고리즘 자동 | 기본 권장. 전극 기생 C 편차가 큰 양산 환경 |
> | **ATI from Fine Fractional Divider** | Coarse MULT는 고정, Fine MULT만 자동 + COMP 자동 | 알고리즘 자동 | Coarse 배율을 수동으로 확정하고 나머지만 ATI에 맡길 때 |
> | **ATI from Compensation Divider** | MULT 전체 고정 (수동값 유지) | Comp Divider만 자동, Comp Value 고정 | MULT가 이미 최적화된 상태에서 COMP 정밀 조정만 필요할 때 |
> | **Compensation Only** | MULT 전체 고정 | COMP Value·Divider 모두 자동 | MULT를 절대 건드리지 않고 COMP로만 Target 맞출 때 |
> | **Disabled** | 고정 (레지스터 기록값 그대로) | 고정 (레지스터 기록값 그대로) | ATI를 완전히 비활성. 수동 캘리브레이션·RESEED로 대체 |
>
> **각 옵션의 물리적 의미**
>
> - **MULT (Multiplier/Divider)**: 내부 전하 분배 비율을 변경한다. 한 사이클에서 Cs로 전달되는 전하 비율을 조절해 같은 전극 용량에서 더 많거나 적은 사이클이 필요하게 만든다. 이를 통해 counts의 절대 범위를 ATI Base에 맞추는 **거친 배율 조정**이다.
>
> - **COMP (Compensation Value·Divider)**: IC 내부의 작은 커패시터 뱅크(몇 pF 단위)를 전극과 병렬로 연결하거나 해제한다. 이 내부 커패시터가 기생 정전용량 일부를 상쇄(offset subtraction)해 전달 전하를 미세하게 줄임으로써 counts를 ATI Target까지 **정밀하게 조정**한다.
>
> **Full이 권장되는 이유, Disabled가 사용되는 이유**
>
> Full 모드에서는 양산 편차를 IC가 자동 흡수하므로 모든 기기에서 동일한 감도를 보장한다. 그러나 ATI 실행 중 I²C 응답 지연이 발생할 수 있다(Sound1에서 확인된 문제). Disabled 모드는 이 문제를 피하기 위해 사전에 최적화한 MULT/COMP 값을 직접 기록하고 Reseed로 LTA를 동기화하는 방식이다. 대신 기기 간 기생 C 편차를 수동으로 보정하거나 허용 오차 범위 내에 있음을 확인해야 한다.

> [!NOTE]
> **§5.9 ATI Mode 선택 — SW 구현 관점**
>
> `ATI Mode = Full`이 기본 권장이다 — MULT와 COMP를 알고리즘이 모두 결정하므로 보드 간 기생 정전용량 편차를 자동 흡수한다. `Compensation Only`는 MULT를 이미 수동으로 고정했고 COMP만 미세 조정이 필요할 때 유용하다. Sound1처럼 `ATI Mode = Disabled`로 고정값을 쓰는 경우, 초기화 시 MULT/COMP 값을 레지스터에 직접 기록한 뒤 `Reseed` bit를 set해 LTA를 현재 counts에 맞추는 흐름으로 ATI를 대체한다. ATI Error(§5.11) 발생 시 자동 재시도가 없으므로, `ATI Error` bit를 초기화 시퀀스에서 폴링하고 set 시 `Re-ATI` bit를 수동 트리거하는 에러 핸들러를 반드시 포함해야 한다.

> [!NOTE]
> **개념 해설: ATI가 필요한 이유와 2단계 보정 과정**
>
> **ATI가 필요한 이유 — 기생 정전용량 편차**
>
> 같은 회로도로 만든 PCB라도 납땜 상태, PCB 소재, 배선 길이 등 제조 편차로 전극 주변 기생 정전용량이 기기마다 조금씩 다르다.
>
> ```
> 기기 A: noTouch counts = 900
> 기기 B: noTouch counts = 600
> 기기 C: noTouch counts = 300
> ```
>
> Threshold를 200으로 설정하면 기기 A(900)는 700 이하에서 감지, 기기 C(300)는 100 이하에서 감지한다. 실제 필요한 delta 크기가 같더라도 기기마다 counts 절대값이 달라 동일 threshold로 동일 감도를 낼 수 없다.
>
> ATI는 각 채널의 noTouch counts를 특정 목표값(ATI Target)으로 정규화해 이 문제를 해결한다.
>
> ```
> ATI 적용 전 (기기 편차):          ATI 적용 후 (정규화):
>
>   Counts                             Counts
>   ↑                                  ↑
> 900┤ 기기A ──────                 400┤ 기기A ──
> 600┤ 기기B ──────      →  →  →    400┤ 기기B ──
> 300┤ 기기C ──────                 400┤ 기기C ──
>   └────────→ 시간                    └────────→ 시간
>
>   threshold=200 적용 시                threshold=200 일괄 적용
>   기기마다 감도 다름                   모든 기기 동일 감도
> ```
>
> **2단계 ATI 보정 과정**
>
> 단계 1 — **MULT 조정** (거친 보정, ATI Base 목표):
> Coarse/Fine Fractional Multiplier·Divider(배율)를 조정해 counts를 ATI Base에 맞춘다. 기어 변속처럼 넓은 범위를 큰 단계로 조정한다.
>
> 단계 2 — **COMP 조정** (정밀 보정, ATI Target 목표):
> Compensation Value·Divider(내부 커패시터 뱅크)로 기생 정전용량 일부를 상쇄해 counts를 ATI Target까지 세밀하게 맞춘다.
>
> ```
> ┌──────────────────────────────────────┐
> │ ATI 2단계 수렴 흐름                   │
> │                                      │
> │  시작: 원시 noTouch Counts (편차 큼) │
> │                │                     │
> │                ▼                     │
> │  ┌─────────────────────────┐         │
> │  │ 단계 1: MULT 조정        │         │
> │  │ Coarse/Fine Divider·    │         │
> │  │ Multiplier 자동 선택     │         │
> │  │ 목표: ATI Base 근접      │         │
> │  └────────────┬────────────┘         │
> │               │ (거친 보정 완료)      │
> │               ▼                      │
> │  ┌─────────────────────────┐         │
> │  │ 단계 2: COMP 조정        │         │
> │  │ Compensation Value·     │         │
> │  │ Divider 자동 선택        │         │
> │  │ 목표: ATI Target 근접    │         │
> │  └────────────┬────────────┘         │
> │               │ (정밀 보정 완료)      │
> │               ▼                      │
> │  결과: Counts ≈ ATI Target           │
> └──────────────────────────────────────┘
>
> ATI Target = ATI Base × (ATI Resolution Factor / 16)
> Sound1: 100 × (64/16) = 400 counts
> ```
>
> ATI 완료 후 모든 기기의 noTouch counts가 ATI Target(예: 400) 근처로 정규화된다. 이제 동일한 threshold 설정이 모든 기기에서 동일한 감도로 동작한다.
>
> **Sensitivity ∝ ATI Target / ATI Base**: ATI Base를 낮추면 Target도 낮아지거나 배율이 커져 counts 해상도가 높아지고 감도가 향상된다.

## 5.10 Automatic Re-ATI

IQS323은 채널이 설계 시 동작 범위를 벗어났음을 자동 감지해 re-ATI를 자동 트리거. re-ATI 발생 시 `System Status`의 `ATI Event` bit set, master가 I²C로 read하면 clear.

re-ATI는 채널 LTA가 `ATI Band`(ATI Target 중심) 밖으로 drift할 때 실행. `ATI Band`는 `ATI Setup`에서 모든 채널 공통 설정:

$$\text{Re-ATI Boundary} = \text{ATI Target} \pm \text{ATI Band}$$

**예시**: ATI Target=800, ATI Band=1/8 → band = ⅛ × 800 = 100 counts → ATI는 `LTA > 900` 또는 `LTA < 700`일 때 실행.

> [!NOTE]
> **개념 해설: Re-ATI 발동 조건과 의미**
>
> ATI는 부팅 시 1회 수행하는 캘리브레이션이다. 그러나 전원이 켜진 채로 시간이 지나면 환경 변화나 마모로 인해 noTouch counts가 ATI Target에서 멀어질 수 있다. 이 경우 LTA가 ATI Band 경계를 벗어나면 IC가 자동으로 ATI를 다시 실행한다.
>
> 즉 Re-ATI = "런타임 중 자동 재캘리브레이션". 사람이 개입하지 않아도 장기간 안정적인 감도를 유지할 수 있다.
>
> Sound1은 ATI Mode = Disabled(autoATI 비활성)로 고정 MULT/COMP를 사용한다. 초기화 시 RESEED로 LTA를 재설정하는 방식으로 대응한다. 이유는 Full Mode에서 autoATI 발동 시 I²C 무응답 사례가 있었기 때문이다.

## 5.11 ATI Error

ATI 알고리즘 실행 후 에러 검사. 어느 채널이든 ATI 완료 시점에 아래가 참이면 `System Status`의 `ATI Error` bit set:

- ATI 알고리즘 완료 시 **Counts가 Re-ATI Boundary 밖**

> [!IMPORTANT]
> **ATI Error 발생 시 re-ATI가 자동 트리거되지 않는다.** master가 `System Control`의 `Re-ATI` bit set으로 수동 트리거해야 함. `Re-ATI` bit는 IQS323이 자동 clear.

## 5.12 Sensor Setup

### 5.12.1 Self / Mutual Capacitance / Inductive Measurements

- 사용 채널은 채널의 `Sensor Setup`에서 `Enable Channel` bit set.
- 측정 수행: `Prox Control`의 `PXS Mode`로 측정 타입 선택 + `Prox Input and Control`(CRx)·`Sensor Setup`(CTx)에서 올바른 Rx·Tx 선택. **self-capacitive는 동일 CRx와 CTx 선택** (예: 전극이 CRx0/CTx0이면 `Prox Input and Control`의 `CRx0` bit + `Sensor Setup`의 `CTx0` bit set).
- Reference UI 미사용 시 `Channel Setup`의 `Channel Mode`='Independent'.
- 모든 측정 타입은 적절한 conversion frequency 선택 필요(§6.5).
- **Inductive 측정**: `Prox Input and Control`의 `Dead Time Enable` bit clear(dead time 비활성) + `Sensor Setup`의 `FOSC Tx Frequency` 옵션 enable + `Conversion Frequency Setup`의 `Conversion Frequency Period`='0' 권장.

> [!NOTE]
> **§5.12.1 Self-cap PXS Mode 설정 — SW 구현 관점**
>
> Self-cap 채널 초기화 시 `Prox Input and Control`의 `CRx` bit와 `Sensor Setup`의 `CTx` bit를 **동일 핀 번호**로 함께 set해야 IC가 self-cap 모드로 인식한다. 둘 중 하나만 set하거나 서로 다른 핀을 지정하면 mutual-cap으로 해석되어 counts 방향이 반전되어 터치 미감지로 이어진다. 초기화 순서는 ① `PXS Mode` 필드 설정 → ② CRx/CTx 동일 핀 기록 → ③ Wav Pattern 0을 self-cap 권장값 `0x03`으로 기록, `Wav Pattern Select = 0x00` 설정 순이다. 이 세 레지스터(Prox Control, Prox Input and Control, Sensor Setup)는 ATI 트리거 전에 모두 확정해야 ATI가 올바른 모드로 수행된다.

> [!NOTE]
> **개념 해설: Self-cap 핀 설정 방법**
>
> Self-cap에서 "동일 CRx와 CTx 선택"의 의미:
>
> Self-cap은 한 핀이 Tx·Rx를 겸용한다. IQS323에서 핀이 CRx0/CTx0 공용이라면:
> - `Prox Input and Control`의 **CRx0 bit** = 1 (수신 핀으로 이 핀 사용)
> - `Sensor Setup`의 **CTx0 bit** = 1 (송신 핀으로 이 핀 사용)
>
> 두 bit를 모두 같은 핀 번호로 설정하면 IC가 self-cap 방식으로 동작한다.
>
> Mutual-cap은 CRx와 CTx를 서로 다른 핀으로 설정한다. 예: CRx0 수신, CTx1 송신.

**Wav Pattern**: `Pattern Definitions`의 `Wav Pattern 0`·`Wav Pattern 1`이 CTx 핀 출력 파형 정의. `Pattern Selection and Engine Bias Current`의 `Wav Pattern Select`가 각 CTx에 Wav0/Wav1 중 무엇을 출력할지 선택('0'→Wav Pattern 0, '1'→Wav Pattern 1).

**Table 5.1 — Wav Pattern Select 비트 매핑**

| Bit3 | Bit2 | Bit1 | Bit0 |
|---|---|---|---|
| TxA | CTx2 | CTx1 | CTx0 |

**Table 5.2 — Recommended Pattern Values** (모든 경우 `Wav Pattern Select`='0x00')

| Measurement Type | Wav Pattern 0 | Wav Pattern 1 |
|---|---|---|
| Self Capacitance | 0x03 | 0x00 |
| Mutual Capacitance | 0x0E | 0x00 |
| Inductive | 0x0B | 0x00 |

> [!NOTE]
> **§5.12.1 Wav Pattern — 회로 원리**
>
> Wav Pattern 레지스터는 4비트 값으로, 각 비트가 **충전 전달 사이클의 각 단계에서 해당 CTx 핀을 High로 구동할지 여부**를 나타낸다. IQS323의 충전 전달 엔진은 한 번의 측정 conversion 동안 여러 단계(phase)를 순환하며 CTx 핀을 교번 구동한다. 각 비트가 "이 phase에서 이 핀을 Active(High)로 하라"는 마스크 역할을 한다.
>
> **비트 해석 (4비트 = 4단계)**
>
> ```
> Wav Pattern 비트:  Bit3  Bit2  Bit1  Bit0
>                   (P3)  (P2)  (P1)  (P0)
> ```
>
> - **0x03 = 0b0000_0011** (Self Capacitance):
>   P0·P1 단계에서 CTx 구동. 두 연속 단계에서 양방향(+/−) 구동 후 Rx에서 전하를 읽는다. Self-cap은 Tx=Rx 동일 핀이므로 짧은 구동 후 같은 핀에서 전하를 측정하는 구조다. 두 단계만 활성화해 구동 에너지를 최소화한다.
>
> - **0x0E = 0b0000_1110** (Mutual Capacitance):
>   P1·P2·P3 단계에서 CTx 구동. 세 단계에 걸쳐 Tx를 구동하면서 별도 Rx 핀에서 결합 정전용량을 통해 전달된 전하를 읽는다. Self-cap은 Tx=Rx가 동일 핀이어서 짧은 구동으로 충분하지만, Mutual-cap은 CTx와 CRx가 물리적으로 분리된 핀이므로 Tx→Rx 전기력선을 충분히 형성하기 위해 더 많은 구동 단계가 필요하다(데이터시트 Table 5.2 권장값).
>
> - **0x0B = 0b0000_1011** (Inductive):
>   P0·P1·P3 단계에서 CTx 구동. 인덕티브 코일은 전류 변화에 대한 반응(L·dI/dt)으로 와전류를 유도한다. 비연속 단계 패턴(P0·P1·P3, P2는 off)은 코일에 공진 주파수에 맞는 교번 전류를 흘리는 구조다. FOSC Tx Frequency 옵션과 함께 동작해 코일 공진 주파수 부근에서 최대 감도를 얻는다.
>
> **Wav Pattern Select = 0x00의 의미**
>
> Wav Pattern Select의 각 비트가 '0'이면 해당 CTx 핀이 Wav Pattern 0을 출력하고, '1'이면 Wav Pattern 1을 출력한다. 모든 채널에 동일한 Wav Pattern 0을 적용할 때 0x00으로 설정한다. 멀티채널에서 일부 채널만 다른 패턴이 필요할 때 해당 비트를 '1'로 설정해 Wav Pattern 1을 개별 지정한다.

### 5.12.2 Temperature / Current Measurement

IQS323은 외부 온도·외부 전류 측정 가능(정확도 낮음). 일부 fringe case에 유용. 대부분의 경우 `Prox Input and Control`의 `Internal Reference` bit clear로 **비활성** 권장.

> [!NOTE]
> **§5.12.2 Temperature/Current — SW 구현 관점**
>
> `Prox Input and Control`의 `Internal Reference` bit를 **clear(0)** 하는 것이 기본 초기화 설정이다. 이 bit가 set 상태이면 IC 내부 기준 전압 회로가 활성화되어 인접 채널의 정전용량 측정에 노이즈가 추가되고, 측정 정확도 자체도 낮아 실용적 가치가 없다. 초기화 체크리스트에 `Internal Reference = 0` 확인 항목을 포함하고, 초기화 완료 후 레지스터 덤프 검증 시 이 bit가 0임을 명시적으로 assert한다.

### 5.12.3 Calibration Capacitor

IQS323 내부 calibration capacitor(CalCap). ProxFusion® 모듈 입력에 연결되어 conversion의 load로 사용 — 보통 디버깅·특성화용. 미사용 시:

- `Pattern Definitions`의 `Calibration Capacitor` field = '0 pF'
- `Sensor Setup`의 `CalCap Rx`·`CalCap Tx` bit clear
- `Prox Input and Control`의 `Calibration Capacitor Select` clear

> [!NOTE]
> **§5.12.3 CalCap — 회로 원리 및 디버깅 시나리오**
>
> CalCap은 IC 내부에 탑재된 **정밀 알려진 값(known capacitance)의 커패시터**다. 일반 측정에서는 외부 전극의 기생 정전용량 + 손가락 용량이 결합되어 counts를 만들어낸다. CalCap을 활성화하면 외부 전극 대신 이 내부 커패시터가 측정 경로에 연결되어, 외부 요인을 완전히 배제한 상태에서 IC 자체의 변환 회로(charge transfer engine)만 동작하게 된다.
>
> **구체적인 디버깅 시나리오**
>
> 시나리오_1 — **IC 동작 검증 (전극 배선 의심)**
> counts가 비정상이거나 0이 나올 때, 원인이 IC 내부 문제인지 외부 전극/배선 문제인지 구분해야 한다. CalCap을 활성화해 내부 커패시터로 측정하면:
> - counts가 정상 → IC는 살아있음, 외부 전극·배선 문제
> - counts가 여전히 이상 → IC 자체 문제 또는 전원·레지스터 설정 오류
>
> 시나리오_2 — **ATI 보정 범위 검증**
> ATI Base·Target·Resolution Factor 설정이 적절한지 확인할 때, 외부 전극의 기생 C 편차 없이 고정된 CalCap 값으로 ATI를 실행하면 이론 counts 예측값과 실제 counts를 비교할 수 있다. 편차가 크면 레지스터 설정 오류임을 확인할 수 있다.
>
> 시나리오_3 — **Compensation 설정 검증**
> COMP Value·Divider 값을 수동으로 조정하는 Disabled 모드에서, CalCap을 알려진 값으로 연결하고 counts 변화를 관찰해 COMP 1 LSB당 실제 counts 변화량을 측정할 수 있다. 이 수치가 COMP 설정의 분해능(resolution) 기준이 된다.
>
> 시나리오_4 — **노이즈 바닥(noise floor) 측정**
> 외부 전극에 노이즈가 없는 이상적인 CalCap 환경에서 counts의 peak-to-peak 변동을 측정한다. 이 값이 시스템 내부 노이즈 바닥이다. 외부 전극 연결 후 noise가 이보다 현저히 크면 PCB 레이아웃·쉴딩 문제로 판단한다.
>
> **미사용 시 반드시 명시적으로 비활성화**
>
> CalCap이 활성 상태로 남아 있으면 내부 커패시터가 측정 경로에 더해져 counts가 낮게 나타난다. 일반 터치 측정에서 예상 counts보다 낮게 나오는 이상 증상이 발생한다. 초기화 시퀀스에서 3개 레지스터(`Calibration Capacitor = 0 pF`, `CalCap Rx/Tx clear`, `Calibration Capacitor Select clear`)를 명시적으로 기록하는 것이 필수다.
