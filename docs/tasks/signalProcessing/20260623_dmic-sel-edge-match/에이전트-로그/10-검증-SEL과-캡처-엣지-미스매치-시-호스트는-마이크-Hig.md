---
name: SEL엣지 검증 10
purpose: 적대적 검증 - SEL과 캡처 엣지 미스매치 시 호스트는 마이크 High-Z 구간을 샘플
type: tasks
maturity: experimental
tags: [dmic, sel-pin, agent-log, verify]
---

# [검증 10] SEL과 캡처 엣지 미스매치 시 호스트는 마이크 High-Z 구간을 샘플하여 데이터가 손상된다

**판정**: `CONFIRMED`

## 근거
데이터시트 3개 레이어를 직접 확인하여 3개 반증 각도를 모두 시도했으나 모두 실패했다.

전제_1(High-Z 구조): SPH0641LM4H-1 §6 p.7 타이밍도 원문에 SELECT=VDD 마이크는 falling 직후 t_DZ(3~16 ns) 후 "High Z" 레이블이 명시되고, SELECT=GND 마이크는 rising 직후 t_DZ 후 "High Z"가 명시된다. 각 마이크의 비담당 반주기에 High-Z 구간이 타이밍 파라미터 수치(t_DZ Min 3 ns, Max 16 ns)와 함께 명기되어 있어 구조적 사실로 확정된다.

전제_2(E8300 캡처 엣지): HW Reference §18.2 p.563 텍스트 원문 — "the low data channel will represent the data sent when the DMIC clock is low … The low data channel is captured on the rising DMIC clock edge, while the high data channel is captured on the falling DMIC clock edge." Figure 32(p.564)에서도 비담당 반주기에 'Z' 표기가 명시된다. _RE = rising 캡처, _FE = falling 캡처 대응이 텍스트·타이밍도 모두에서 확인된다.

반증 각도_A(t_DZ 완료 전 캡처로 직전값 유지 가능성): t_DZ Min 3 ns 이므로 캡처 엣지와 High-Z 전환이 겹치는 메타안정 구간이 존재할 수 있으나, 이는 "손상 없음"이 아니라 메타안정·직전값 잔류라는 다른 형태의 손상이다. 반증 실패.

반증 각도_B(E8300 내부 풀다운으로 안정 레벨 제공): DATA 핀 내부 종단 명시가 없으며, 설령 있더라도 모든 비트가 동일 논리 레벨로 고정되는 것은 DC 오프셋 손상이다. 반증 실패.

반증 각도_C(t_HOLD 구간 내 캡처 가능성): 미스매치 조합(SEL=GND + FE)에서 falling 엣지 시점에 Mic Low DATA는 이미 rising 이후 t_DZ 완료로 High-Z 상태를 유지 중이며, t_HOLD는 반대 마이크(Mic High)의 파라미터여서 관련 없다. 반증 실패.

단, 손상 양상의 구체적 형태(플로팅·직전값 잔류·메타안정 중 어느 쪽이 지배적인가)는 t_DZ 완료 여부·라인 기생 용량·외부 종단저항 유무에 의존하며 데이터시트만으로는 확정 불가다. "손상 발생" 자체는 SPH0641 §6 타이밍도 + E8300 §18.2의 구조적 정의로 확정된다.

## 데이터시트 근거
SPH0641LM4H-1 Rev.A §6 p.7 타이밍도: SELECT=VDD 마이크 — falling 엣지 후 t_DZ(Min 3 ns, Max 16 ns) 후 "High Z" 명시; SELECT=GND 마이크 — rising 엣지 후 t_DZ 후 "High Z" 명시. §4 p.4 Microphone Interface Specifications: t_DZ Min 3 ns / Max 16 ns 수치 확정. §5 p.7 표: Mic(High)/SELECT=VDD → "Asserts DATA on Rising Clock Edge" / "Latch DATA on Falling Clock Edge"; Mic(Low)/SELECT=GND → "Asserts DATA on Falling Clock Edge" / "Latch DATA on Rising Clock Edge". E8300 HW Reference §18.2 p.563: "the low data channel is captured on the rising DMIC clock edge, while the high data channel is captured on the falling DMIC clock edge." Figure 32 p.564: DMIC low/high data channel 각 비담당 반주기에 'Z' 표기. lib_audio_in.h:80: DMIC2_DATA_FE | DMIC1_DATA_RE (현재 펌웨어 설정, 반증 대상 아닌 컨텍스트).

