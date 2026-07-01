---
name: SEL엣지 검증 08
purpose: 적대적 검증 - SEL=GND↔DMIC*_DATA_RE, SEL=VDD↔DMIC*_DAT
type: tasks
maturity: experimental
tags: [dmic, sel-pin, agent-log, verify]
---

# [검증 08] SEL=GND↔DMIC*_DATA_RE, SEL=VDD↔DMIC*_DATA_FE가 유일하게 올바른 조합이다

**판정**: `CONFIRMED`

## 근거
두 데이터시트 원문을 직접 확인했다.

SPH0641LM4H-1 §5 표(p.7): SELECT=GND → "Asserts DATA on Falling Clock Edge" (LOW 구간 유효, 호스트는 Rising에 latch), SELECT=VDD → "Asserts DATA on Rising Clock Edge" (HIGH 구간 유효, 호스트는 Falling에 latch). §6 타이밍도(p.7): 각 마이크는 자기 반주기에만 DATA(VOH/VOL)를 구동하고 나머지 반주기는 High-Z.

E8300 HW Reference §18.2(p.563): "the low data channel will represent the data sent when the DMIC clock is low, and the high data channel will represent the data sent when the DMIC clock is high." + "The low data channel is captured on the rising DMIC clock edge, while the high data channel is captured on the falling DMIC clock edge." Figure 32(p.564) 타이밍도가 이를 시각적으로 확인.

논리 체인: SEL=GND → LOW 구간 유효 → E8300 low data channel → DMIC*_DATA_RE(rising) 캡처. SEL=VDD → HIGH 구간 유효 → E8300 high data channel → DMIC*_DATA_FE(falling) 캡처. 역방향 조합은 마이크가 High-Z인 반주기에 캡처하므로 구조적으로 데이터 손상이 발생한다.

반증 후보 세 가지 검토: (1) E8300 §18.2의 "mono 신호는 rising 또는 falling 어느 쪽이든 캡처 가능"은 E8300 측 설정 자유도이며, SEL 핀과의 물리적 정합 요건을 면제하지 않는다. (2) DATA 선 분리 구성에서 both-RE 통일도 올바른 조합이라는 주장은 SEL도 함께 GND로 바꾼 경우이므로 주장의 대응 규칙(SEL=GND↔RE) 범주 안에 있어 반증이 아니다. (3) setup time 경계에서 우연한 유효 캡처 가능성은 간헐적·비결정적 동작으로 "올바른 조합" 지위를 부여할 수 없다.

미해결 사항(U7·U9 실제 SEL 스트랩, U9 SEL=VDD 능동 스트랩 존재 여부)은 현재 펌웨어와 회로의 정합 여부에 관한 미지수이지, 대응 규칙 자체의 정확성에 영향을 주지 않는다.

## 데이터시트 근거
SPH0641LM4H-1 Rev.A §5 Interface Circuit 표(p.7): SELECT=GND → "Asserts DATA On: Falling Clock Edge / Latch DATA On: Rising Clock Edge"; SELECT=VDD → "Asserts DATA On: Rising Clock Edge / Latch DATA On: Falling Clock Edge". §6 Timing Diagram(p.7): SELECT=VDD면 rising 직후 DATA 구동 후 High-Z; SELECT=GND면 falling 직후 DATA 구동 후 High-Z. SELECT 핀 설명(p.8): "This pin is internally pulled low but should not be left floating." — E8300 HW Reference §18.2(p.563): "the low data channel will represent the data sent when the DMIC clock is low, and the high data channel will represent the data sent when the DMIC clock is high. The low data channel is captured on the rising DMIC clock edge, while the high data channel is captured on the falling DMIC clock edge." Figure 32(p.564): DMIC low data channel이 rising 직후 'Low data 1' 캡처, DMIC high data channel이 falling 직후 'High data 0' 캡처 — cfx lib_audio_in.h:80: LIB_DMIC_AUDIO_MUX_CFG_INPUT_CH_SRC = (ADC3_OUT | DMIC2_DATA_FE | DMIC1_DATA_RE | ADC0_OUT)

