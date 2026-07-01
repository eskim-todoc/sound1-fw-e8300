---
name: SEL엣지 검증 09
purpose: 적대적 검증 - 현재 펌웨어의 DMIC2=DMIC2_DATA_FE 설정은 U9 마이크 S
type: tasks
maturity: experimental
tags: [dmic, sel-pin, agent-log, verify]
---

# [검증 09] 현재 펌웨어의 DMIC2=DMIC2_DATA_FE 설정은 U9 마이크 SEL=VDD 능동 스트랩을 강제 요구한다

**판정**: `CONFIRMED`

## 근거

주장을 반증하려면 "DMIC2_DATA_FE 설정임에도 SEL=GND/floating으로 정합이 성립한다"를 보여야 한다. 이를 시도했으나 실패했다.

[반증 시도 1] SEL=GND + FE 조합이 동작할 수 있는가?
SPH0641 데이터시트 §5 p.7 명세: SEL=GND → "Asserts DATA on Falling Clock Edge" = 클럭 LOW 구간에만 DATA 유효, HIGH 구간은 High-Z. E8300 HW Reference §18.2 p.563: DMIC*_DATA_FE = falling-edge 캡처 = 클럭 HIGH→LOW 전이 직전에 샘플. 즉 E8300이 HIGH 구간 끝(falling)을 캡처하는데, SEL=GND 마이크는 HIGH 구간에 High-Z이다. 유효 데이터 없는 구간을 캡처 → 반증 불가. 조합 자체가 데이터시트로 부정된다.

[반증 시도 2] SELECT 내부 풀다운이 있으므로 GND 기본이고, 설계자가 FE를 쓴다면 SEL=VDD가 이미 보드에 존재할 것이다. 따라서 "강제 요구"가 아니라 "이미 충족된 조건"이 아닌가?
이는 반증이 아니다. 주장은 "강제 요구한다"는 필요 조건 관계를 서술한 것이다. SEL=VDD가 이미 보드에 존재한다면 주장이 참이면서 동시에 충족된 상태일 뿐이다. "강제 요구"의 논리적 의미(DMIC2_DATA_FE → SEL=VDD 필수)는 회로도 확인 여부와 무관하게 성립한다.

[반증 시도 3] U9가 QCC 담당이었으므로 QCC 기준으로 SEL이 GND로 결정됐을 수 있고, 그러면 현재 FE 설정이 미스매치이지 "강제 요구"가 아닐 수 있다.
이 경우 주장은 더욱 강화된다. SEL=GND인데 FE 설정이면 미스매치(데이터 손상)이고, 이를 해소하려면 SEL=VDD 능동 스트랩이 필요하다 — 즉 "강제 요구"가 더 명확해진다.

[코드 확인] lib_audio_in.h:80: `DMIC2_DATA_FE` 실제 설정 확정. lib_audio_in.c:104: U9=DIO10(CLK)+DIO17(DATA), "QCC" 주석. SEL 핀을 소프트웨어로 제어하는 코드 없음 → SEL은 하드웨어 스트랩 전용.

[결론] DMIC2_DATA_FE 설정과 SEL=VDD 필요 조건의 관계는 데이터시트 §5·§6 + E8300 HW Reference §18.2 인용으로 논리적 필연이 확립되어 있다. 이 관계를 깨는 근거가 없다. 회로도가 없어서 "현재 보드에 SEL=VDD가 실제로 스트랩되어 있는가"는 미확인이지만, 그것은 주장("강제 요구")을 반증하는 것이 아니라 강제 요구가 "충족되어 있는가"를 묻는 별개 질문이다. 주장 자체는 confirmed.


## 데이터시트 근거

1. 코드 근거: /mnt/e/Claude/projects/Sound1/src/1__cfx/lib_cfx/lib_audio_in.h:80 — `LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC = (ADC3_OUT | DMIC2_DATA_FE | DMIC1_DATA_RE | ADC0_OUT)`. DMIC2=_FE 설정 실재 확인.

2. 데이터시트 §5 p.7 (SPH0641LM4H-1): SELECT=VDD → "Asserts DATA on Rising Clock Edge" = 클럭 HIGH 구간에만 DATA 유효(나머지 High-Z). SELECT=GND → "Asserts DATA on Falling Clock Edge" = 클럭 LOW 구간에만 DATA 유효(나머지 High-Z). SELECT 핀 내부 풀다운 명시 → floating/GND 기본값 = Low(GND) = LOW 구간 유효.

3. E8300 HW Reference §18.2 p.563: "high data channel(클럭 HIGH 구간 데이터)은 falling 엣지에 캡처" = DMIC*_DATA_FE(0x3). "low data channel(클럭 LOW 구간 데이터)은 rising 엣지에 캡처" = DMIC*_DATA_RE(0x2).

4. 대조: DMIC2_DATA_FE = E8300이 HIGH 구간 데이터를 falling으로 캡처. 이를 위해 마이크가 HIGH 구간에 DATA(VOH/VOL)를 구동해야 함. SPH0641에서 HIGH 구간 구동은 SEL=VDD일 때만 성립(§5 p.7). SEL=GND 또는 floating이면 내부 풀다운에 의해 LOW 구간만 구동, HIGH 구간은 High-Z → E8300이 High-Z 상태를 캡처 → 데이터 손상.

5. lib_audio_in.c:104 주석: "U9 (DMIC2) : DMIC_CLK2 (DIO10), DMIC_OUT2 (DIO17) : QCC" — SEL 핀 제어 로직 없음. SEL 스트랩은 보드 하드웨어에서만 결정.

6. /mnt/e/Claude/projects/Sound1/docs/tasks/signalProcessing/20260623_dmic-sel-edge-match/조사_SEL엣지-그라운딩.md §4: "U9: FE 캡처 → SEL=VDD를 회로에서 능동 스트랩해야만 정합. 만약 U9 SEL이 GND/floating이면 → 마이크는 LOW 구간 출력인데 펌웨어는 falling 캡처 → 미스매치(High-Z 캡처)."

미확인 사항: U9 SEL의 실제 보드 스트랩값(회로도 미제출). 그러나 이것은 "강제 요구가 충족되어 있는가"의 문제이지 "강제 요구가 존재하는가"를 반증하지 않는다.


