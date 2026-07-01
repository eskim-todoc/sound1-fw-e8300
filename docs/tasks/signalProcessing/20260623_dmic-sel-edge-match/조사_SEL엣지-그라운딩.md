---
name: SEL-엣지 정합 그라운딩
purpose: SPH0641 마이크 SELECT/엣지와 E8300 DMIC 캡처 엣지의 사실 기반 대조
type: tasks
maturity: experimental
tags: [dmic, sel-pin, clock-edge, pdm, sph0641, grounding]
---

# SEL ↔ 캡처 엣지 그라운딩 (사실 기반)

**TL;DR**: SPH0641 마이크는 **SEL=GND → 데이터 LOW 구간 유효(falling에 assert)**, **SEL=VDD → 데이터 HIGH 구간 유효(rising에 assert)**. E8300은 **"low data channel(클럭 LOW 구간 데이터) = rising 캡처(`_RE`)"**, **"high data channel(클럭 HIGH 구간 데이터) = falling 캡처(`_FE`)"**. 따라서 **올바른 조합: SEL=GND↔`_RE`, SEL=VDD↔`_FE`**. 현재 펌웨어는 DMIC1=`_RE`·DMIC2=`_FE` → **U7 SEL=GND, U9 SEL=VDD 강제 요구**.

> [!NOTE]
> 근거: `docs/참고/e8300/knowles ... .pdf`(**SPH0641LM4H-1**, Rev.A 2014) · `Ezairo 8300 Hardware Reference.pdf` §18.2 · cfx `lib_audio_in.h/.c`.

---

## 1. 마이크 — Knowles SPH0641LM4H-1 (PDM)

- 단일 비트 **PDM 출력**, sigma-delta. **Dual Multiplexed Channels 지원**(스테레오: 2개가 DATA 라인 공유, 한쪽 HIGH 구간·한쪽 LOW 구간 출력).
- 핀: 1=DATA(PDM O), **2=SELECT(Lo/Hi(L/R) Select, Digital I, 내부 풀다운 — floating 금지)**, 3=GND, 4=CLOCK, 5=VDD.

### SELECT ↔ 엣지 표 (데이터시트 §5 INTERFACE CIRCUIT, p.7)

| Microphone | SELECT | Asserts DATA On | Latch DATA On (호스트) | 데이터 유효 구간 |
|---|---|---|---|---|
| **Mic (High)** | **VDD** | **Rising** Clock Edge | Falling Clock Edge | 클럭 **HIGH** 구간 |
| **Mic (Low)** | **GND** | **Falling** Clock Edge | Rising Clock Edge | 클럭 **LOW** 구간 |

- 타이밍도(§6, p.7): SELECT=VDD면 rising 직후 DATA(VOH/VOL) 구동 후 High-Z; SELECT=GND면 falling 직후 DATA 구동 후 High-Z. 즉 **각 마이크는 자신의 반주기에만 라인을 구동하고 나머지 반주기는 High-Z**(멀티플렉싱 전제).
- SELECT 내부 풀다운 → **미스트랩(floating/GND) 시 기본 = Low(GND) = LOW 구간 유효.**

## 2. E8300 DMIC 캡처 엣지 (HW §18.2, p.563)

- 정의: **"low data channel = 클럭이 LOW일 때 보낸 데이터, high data channel = 클럭이 HIGH일 때 보낸 데이터"**.
- 캡처: **"low data channel은 rising DMIC 클럭 엣지에 캡처, high data channel은 falling DMIC 클럭 엣지에 캡처."**
- AUDIO_MUX 설정값(HW 레지스터):
  - `DMIC*_DATA_RE` = "Data is provided from DMIC (**rising-edge**)" (0x2).
  - `DMIC*_DATA_FE` = "Data is provided from DMIC (**falling-edge**)" (0x3).
- 스테레오(1 pad 공유) 시: low=rising 캡처, high=falling 캡처.

## 3. 대조 → 올바른 조합표 (핵심 결론)

| 마이크 SEL | 유효 구간 | = E8300 채널 유형 | **올바른 E8300 설정** |
|---|---|---|---|
| **GND (Low)** | 클럭 LOW | low data channel | **`DMIC*_DATA_RE` (rising)** |
| **VDD (High)** | 클럭 HIGH | high data channel | **`DMIC*_DATA_FE` (falling)** |

- **즉 SEL=GND ↔ `_RE`, SEL=VDD ↔ `_FE`가 유일하게 올바른 조합.**
- **미스매치 시**: 마이크가 라인을 구동하지 않는 반주기(High-Z)에 호스트가 샘플 → **High-Z/플로팅 또는 직전 값**을 캡처 → 데이터 손상(노이즈·DC·간헐 오류).

## 4. 현재 펌웨어 매핑 (cfx `lib_audio_in.h:80`)

`LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC = (ADC3_OUT | DMIC2_DATA_FE | DMIC1_DATA_RE | ADC0_OUT)`

| 마이크 | 채널 | 펌웨어 캡처 엣지 | **요구되는 SEL 스트랩** |
|---|---|---|---|
| **U7 DMIC1 (Right front)** | ch1 | `DMIC1_DATA_RE` (rising) | **SEL = GND** (또는 기본 풀다운) |
| **U9 DMIC2 (Left front)** | ch2 | `DMIC2_DATA_FE` (falling) | **SEL = VDD** (능동 스트랩 필수) |

- **U7**: RE 캡처 → SEL=GND 필요. 내부 풀다운 기본값과 일치 → floating이어도 우연히 정합 가능(단 floating 금지 권고).
- **U9**: FE 캡처 → **SEL=VDD를 회로에서 능동 스트랩해야만 정합.** 만약 U9 SEL이 GND/floating이면 → 마이크는 LOW 구간 출력인데 펌웨어는 falling 캡처 → **미스매치(High-Z 캡처).**

## 5. 빔포밍 연계 (포인트_3 해소 입력)

- 본 보드는 **DATA 라인 분리**(U7=DIO23, U9=DIO17) + 클럭 공유(DIO22). 즉 **멀티플렉싱(1 DATA 라인 공유)이 아님** → 두 마이크가 같은 반주기에 출력해도 라인 충돌 없음.
- 따라서 **두 마이크를 동일 SEL(예: 둘 다 GND) + 동일 엣지(둘 다 `_RE`)로 통일 가능** → RE/FE 반주기 스큐(0.13µs) 원천 제거. 단 이는 **양 마이크 SEL 스트랩이 동일해야 성립**(회로 의존).
- 현재 RE/FE 분리는 "자유 선택"이 아니라 **각 마이크의 SEL 스트랩에 의해 강제된 설정**일 가능성이 큼(U7=GND, U9=VDD 추정).

## 6. 미해결 → 분석/검증 입력

- **미해결_1**: 실제 보드 U7·U9의 SEL 스트랩(VDD/GND/floating) — 회로도 확인 필요. 펌웨어 정합성의 최종 판정 전제. → **해소 (2026-06-23 회로도): U7 SEL=GND, U9 SEL=VDD → 현재 펌웨어 두 채널 정합 ✅. 부품은 SPH8690LM4H-1(데이터시트 SPH0641과 부품번호 상이, SEL 규약 동일 가정).**
- **미해결_2**: U9가 원래 QCC 담당이었으므로 SEL 스트랩이 QCC 인터페이스 기준으로 결정됐을 가능성 — E8300 캡처와의 정합 재확인.
- **미해결_3**: both-RE 통일이 빔포밍에 유리하나, SEL 재스트랩(HW 변경) 필요성·비용.
