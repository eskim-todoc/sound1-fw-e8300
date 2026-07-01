---
name: SEL엣지 분석 01
purpose: PDM 전기 인터페이스 분석가
type: tasks
maturity: experimental
tags: [dmic, sel-pin, agent-log, analysis]
---

# [분석 01] PDM 전기 인터페이스 분석가

**신뢰도**: high

## 결론
SPH0641은 SELECT 핀으로 어느 반주기에 DATA를 구동할지 결정하는 전기적 멀티플렉싱 구조를 갖는다. SELECT=GND면 falling 엣지 직후 클럭 LOW 구간에 DATA를 구동(VOH/VOL)하고, SELECT=VDD면 rising 엣지 직후 클럭 HIGH 구간에 DATA를 구동한다. 나머지 반주기는 High-Z(출력 disable)로 라인을 떠나므로, 호스트는 반드시 마이크가 구동하는 반주기에 맞는 클럭 엣지에 샘플해야 유효 데이터를 얻는다.

## 발견(근거)
- **SELECT=VDD 마이크(Mic High)는 rising 클럭 엣지 직후 DATA를 assert하며, 클럭 HIGH 구간 동안 DATA(VOH/VOL)를 유지한다.**
  - 근거: 데이터시트 §5 p.7 표: 'Mic(High), SELECT=VDD: Asserts DATA on Rising Clock Edge'. 타이밍도 §6 p.7: rising 직후 DATA 구동 시작. (SPH0641LM4H-1 데이터시트 §5·§6, p.7)
- **SELECT=GND 마이크(Mic Low)는 falling 클럭 엣지 직후 DATA를 assert하며, 클럭 LOW 구간 동안 DATA를 유지한다.**
  - 근거: 데이터시트 §5 p.7 표: 'Mic(Low), SELECT=GND: Asserts DATA on Falling Clock Edge'. 호스트는 rising에 latch. (SPH0641LM4H-1 데이터시트 §5·§6, p.7)
- **각 마이크는 자신이 담당하는 반주기(HIGH 또는 LOW 구간)에만 DATA 라인을 구동하고, 나머지 반주기에는 High-Z(출력 트라이스테이트)로 전환한다.**
  - 근거: 타이밍도 §6 p.7: SELECT=VDD면 HIGH 구간 구동·LOW 구간 High-Z; SELECT=GND면 LOW 구간 구동·HIGH 구간 High-Z. Dual Multiplexed Channels 지원 명세와 일치. (SPH0641LM4H-1 데이터시트 §6, p.7)
- **High-Z 반주기에 호스트가 샘플하면 라인은 플로팅(또는 직전 드라이브의 기생 잔류 전압)을 캡처하여 데이터가 손상된다.**
  - 근거: 각 마이크는 반주기에만 DATA를 구동하므로, 반대 반주기에는 정의된 로직 레벨이 없다. 호스트가 High-Z 구간에 샘플하면 유효 PDM 비트를 얻지 못한다(미스매치 증상: 노이즈·DC·간헐 오류). (SPH0641LM4H-1 데이터시트 §6, p.7 (구조적 논리 추론))
- **1-DATA선 공유 시 SELECT=VDD(High-Z during LOW)와 SELECT=GND(High-Z during HIGH) 두 마이크가 서로 다른 반주기를 담당해 충돌 없이 멀티플렉싱된다.**
  - 근거: Dual Multiplexed Channels 지원 명세: 한 DATA 패드를 두 마이크가 교번 구동. 각각 자기 반주기에만 VOH/VOL을 내고 나머지 반주기 High-Z → 와이어드 멀티플렉싱 가능. (SPH0641LM4H-1 데이터시트 §5·§6, p.7)
- **SELECT 핀 내부 풀다운이 있으므로 floating 또는 GND 연결 시 기본 동작은 SELECT=GND(Low 구간 유효, rising 캡처)다.**
  - 근거: 데이터시트 핀 설명: 'SELECT: Lo/Hi (L/R) Select, Digital input, 내부 풀다운 — floating 금지'. floating=GND와 동일 동작(내부 풀다운). (SPH0641LM4H-1 데이터시트 §5, p.7 핀 설명)
- **본 보드(U7=DIO23, U9=DIO17)는 DATA 라인이 분리돼 있어 물리적 멀티플렉싱 구조가 아니므로, 두 마이크를 동일 SEL(예: 둘 다 GND) + 동일 캡처 엣지(둘 다 _RE)로 통일하면 반주기 스큐(0.13 µs)를 원천 제거할 수 있다.**
  - 근거: 보드 구성: U7=DIO23, U9=DIO17, CLK=DIO22 공유. DATA 라인 분리이므로 각 마이크가 동일 반주기에 구동해도 충돌 없음. 멀티플렉싱 전제가 없으므로 SEL 통일이 가능. (조사_SEL엣지-그라운딩.md §5 (보드 구성 사실))

## 미해결 질문
- 실제 보드 U7(DMIC1)·U9(DMIC2)의 SELECT 스트랩(VDD/GND/floating) — 회로도 확인 전까지 펌웨어 정합 판정 불완전. 특히 U9 SELECT가 VDD인지 GND인지 미확인.
- U9는 원래 QCC가 담당했으므로 SELECT 스트랩이 QCC 인터페이스 기준으로 결정됐을 가능성 — E8300 FE 캡처와의 실제 정합 여부 미확인.
- both-RE 통일을 위한 SEL 재스트랩이 HW 변경을 요구하는지 여부 — 현재 SEL이 능동 VDD 스트랩인 경우 PCB 수정 필요.
