---
name: SEL엣지 분석 03
purpose: SEL↔엣지 정합 매핑 분석가
type: tasks
maturity: experimental
tags: [dmic, sel-pin, agent-log, analysis]
---

# [분석 03] SEL↔엣지 정합 매핑 분석가

**신뢰도**: high

## 결론
마이크 SELECT 핀이 어느 반주기에 DATA를 구동하는지와 E8300이 어느 엣지에 그 채널을 캡처하는지를 맞춰야 한다. SEL=GND → low data channel → rising-edge 캡처(RE), SEL=VDD → high data channel → falling-edge 캡처(FE)가 유일하게 유효한 조합이다. 역방향 조합은 마이크가 High-Z인 반주기에 호스트가 샘플하므로 반드시 데이터가 손상된다.

## 발견(근거)
- **SEL=GND이면 마이크는 클럭 LOW 구간에만 DATA를 구동한다(나머지는 High-Z). 호스트가 이 데이터를 받으려면 Low→High 전이, 즉 rising 엣지에 latch해야 한다.**
  - 근거: 데이터시트 §5 p.7: SELECT=GND → 'Asserts DATA on Falling Clock Edge' = 클럭 LOW 구간 유효. E8300 HW Ref §18.2 p.563: 'low data channel은 rising DMIC 클럭 엣지에 캡처'. AUDIO_MUX: DMIC*_DATA_RE(0x2) = rising-edge. (마이크 DS §5 p.7 / E8300 HW Ref §18.2 p.563)
- **SEL=VDD이면 마이크는 클럭 HIGH 구간에만 DATA를 구동한다(나머지는 High-Z). 호스트가 이 데이터를 받으려면 High→Low 전이, 즉 falling 엣지에 latch해야 한다.**
  - 근거: 데이터시트 §5 p.7: SELECT=VDD → 'Asserts DATA on Rising Clock Edge' = 클럭 HIGH 구간 유효. E8300 HW Ref §18.2 p.563: 'high data channel은 falling 엣지에 캡처'. AUDIO_MUX: DMIC*_DATA_FE(0x3) = falling-edge. (마이크 DS §5 p.7 / E8300 HW Ref §18.2 p.563)
- **역방향 조합(SEL=GND+FE, 또는 SEL=VDD+RE)은 마이크가 High-Z인 반주기에 캡처하므로 반드시 데이터 손상이 발생한다.**
  - 근거: 데이터시트 §6 p.7 타이밍도: 각 마이크는 자기 반주기에만 VOH/VOL을 구동하고, 나머지 반주기는 High-Z. High-Z 구간 캡처 시 플로팅 또는 직전 값 래치. (마이크 DS §6 p.7)
- **현재 펌웨어 DMIC1(U7, Right front)은 DATA_RE(rising) 설정이므로, 보드 SEL 스트랩은 GND여야 정합된다. SELECT의 내부 풀다운이 있으므로 floating 상태에서도 GND와 동일하게 동작하여 DMIC1은 사실상 자동 정합될 가능성이 높다.**
  - 근거: cfx lib_audio_in.h:80: DMIC1_DATA_RE. 마이크 DS §5 p.7: 내부 풀다운 → floating 기본 = Low(GND) = LOW 구간 유효. (lib_audio_in.h line 80 / 마이크 DS §5 p.7)
- **현재 펌웨어 DMIC2(U9, Left front)는 DATA_FE(falling) 설정이므로, 보드 SEL 스트랩은 반드시 VDD여야 정합된다. GND 또는 floating이면 미스매치이다.**
  - 근거: cfx lib_audio_in.h:80: DMIC2_DATA_FE. 마이크 DS §5 p.7: SEL=VDD만 HIGH 구간 유효. 내부 풀다운이 있으므로 floating이면 GND로 동작 → 미스매치. (lib_audio_in.h line 80 / 마이크 DS §5 p.7)
- **U7·U9의 DATA 선이 분리(DIO23/DIO17)되어 있으므로 양 마이크를 동일 SEL+동일 엣지로 통일해도 전기적 충돌은 없다. 그러나 현재 펌웨어는 DMIC1=RE·DMIC2=FE로 비대칭이므로, 보드 SEL 스트랩이 채널별로 달라야 소프트웨어와 정합된다.**
  - 근거: 그라운딩 사실: DATA 선 분리 확인(U7=DIO23, U9=DIO17), CLK 공유(DIO22). 1:1 대응이므로 멀티플렉싱 충돌 없음. (보드 구성 사실 섹션)

## 미해결 질문
- U7(DMIC1) SEL 스트랩의 실제 회로도 연결(GND/VDD/floating) 확인 필요. 내부 풀다운 때문에 floating도 동작하지만, 의도적 GND 스트랩인지 확인해야 설계 의도 판단 가능.
- U9(DMIC2) SEL 스트랩의 실제 회로도 연결 확인 필요. U9가 원래 QCC 담당이었으므로 SEL이 QCC 요구사항(L/R 채널 선택 기준)에 따라 결정됐을 가능성이 있으며, 그 기준이 E8300 FE 설정과 일치하는지 검증 필요.
- 두 채널을 both-RE(SEL 모두 GND) 또는 both-FE(SEL 모두 VDD)로 통일하는 방향으로 리팩토링할 경우, 펌웨어 AUDIO_MUX 설정과 보드 SEL 스트랩을 동시에 변경해야 하는데 그 필요성 및 우선순위는 별도 판단 필요.
