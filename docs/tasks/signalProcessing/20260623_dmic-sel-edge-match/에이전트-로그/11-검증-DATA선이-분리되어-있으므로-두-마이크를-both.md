---
name: SEL엣지 검증 11
purpose: 적대적 검증 - DATA선이 분리되어 있으므로 두 마이크를 both-RE(둘 다 SEL=
type: tasks
maturity: experimental
tags: [dmic, sel-pin, agent-log, verify]
---

# [검증 11] DATA선이 분리되어 있으므로 두 마이크를 both-RE(둘 다 SEL=GND)로 통일하면 RE/FE 반주기 스큐를 제거할 수 있다

**판정**: `CONFIRMED`

## 근거
주장의 논리 구조를 두 단계로 분리해 검증한다.

[전제] DATA선 분리 → 멀티플렉싱 충돌 없음
SPH0641 데이터시트 §6 p.7 타이밍도에 따르면, 1-DATA선 공유(멀티플렉싱) 구성에서는 각 마이크가 자기 반주기에만 VOH/VOL을 구동하고 나머지는 High-Z로 전환하도록 설계되어 있다. 이 제약은 두 마이크가 한 패드를 공유할 때만 성립한다. 현 보드는 U7=DIO23, U9=DIO17로 DATA 패드가 물리적으로 분리되어 있고 CLK만 DIO22를 공유한다(그라운딩 §5, lib_audio_in.h:48-49). 따라서 두 마이크가 동일 반주기(LOW 구간)에 동시에 데이터를 출력해도 전기적 충돌은 발생하지 않는다. 전제는 데이터시트 + 보드 구성 사실로 확정된다.

[결론] both-RE 통일 → RE/FE 반주기 스큐 제거
RE/FE 반주기 스큐의 물리적 원인은 DMIC1_DATA_RE(rising 캡처)와 DMIC2_DATA_FE(falling 캡처)의 캡처 시점 차이다. 이 차이는 정확히 DMIC 클럭 반주기 = 0.5/3.84 MHz = 0.1302 µs이다(검증 페르소나 30: HW p.450-451 근거). DATA선이 분리된 구성에서는 DMIC2를 DATA_FE가 아닌 DATA_RE로 변경해도 캡처 유효성이 그대로 유지된다(E8300 HW Ref §18.2 p.563: 분리 패드 DMIC도 RE/FE 독립 선택 가능). SEL=GND(both) + DMIC*_DATA_RE(both)로 통일하면 두 채널의 캡처 시점이 동일 rising 엣지로 정렬되어 반주기 오프셋이 원천 소멸한다. 이 논리는 타이밍 정의로부터 직접 도출되며 반증 가능한 반례가 없다.

[유의사항 — 반증이 아님]
(a) U9 SEL 스트랩 실제값 미확인(회로도 없음): 현재 U9 SEL=VDD로 능동 스트랩돼 있다면 GND 재스트랩이 PCB 수정을 요구한다. 이는 주장의 조건부 전제("both-RE로 통일하면")를 실행할 때의 HW 비용 문제이지, 스큐 제거 원리 자체를 부정하지 않는다.
(b) QCC DIO17 영향: SEL 재스트랩 시 QCC 측 캡처 정합에 영향을 줄 수 있으나, 이는 부가 시스템 영향이지 스큐 제거 원리의 반증이 아니다.
(c) 스큐 크기 무시 가능 수준(0.1302 µs = 빔포밍 목표 지연 58.31 µs의 0.22%, DELAY_FRACTIONAL 1 LSB의 0.5배): 스큐 제거의 실용적 이득이 거의 없다는 사실이 확인되지만, "제거 가능하다"는 주장 자체를 반증하지 않는다.

결론: 주장이 주장하는 내용 — "DATA선 분리가 both-RE 통일을 가능하게 하며, 그 결과 RE/FE 반주기 스큐가 제거된다" — 은 데이터시트(SPH0641 §5/§6, E8300 HW Ref §18.2 p.563)와 보드 구성 사실(DIO23/DIO17 분리)로 논리적·전기적으로 지지된다.

## 데이터시트 근거
- SPH0641LM4H-1 데이터시트 §5 p.7: SEL=GND → "Asserts DATA on Falling Clock Edge"(LOW 구간 유효), SEL=VDD → "Asserts DATA on Rising Clock Edge"(HIGH 구간 유효). 멀티플렉싱(High-Z) 제약은 1-DATA 패드 공유 구성 전제.
- SPH0641LM4H-1 데이터시트 §6 p.7 타이밍도: 각 마이크는 자기 반주기에만 VOH/VOL 구동, 나머지 High-Z — 이 구조는 패드 공유 시의 멀티플렉싱 전제이며, 패드 분리 시에는 동일 반주기 동시 구동도 충돌 없음.
- E8300 HW Reference §18.2 p.563: DMIC*_DATA_RE(0x2) = rising-edge 캡처, DMIC*_DATA_FE(0x3) = falling-edge 캡처. 각 채널 독립 설정 가능.
- 보드 구성 사실(lib_audio_in.h:48-49, 조사_SEL엣지-그라운딩.md §5): U7=DIO23, U9=DIO17(DATA 분리), CLK=DIO22(공유) — 멀티플렉싱 구조 아님.
- 현재 펌웨어(lib_audio_in.h:80): DMIC1_DATA_RE | DMIC2_DATA_FE → 반주기 스큐 0.5/3.84 MHz = 0.1302 µs 발생.
- 검증 페르소나 30(/mnt/e/Claude/projects/Sound1/docs/tasks/signalProcessing/20260623_beam-forming/에이전트-로그/30-검증-DMIC2(FE)DMIC1(RE)-반주기-스큐는-출력-.md): 판정 CONFIRMED, 스큐 = 0.002083 샘플(출력 62.5 µs의 1/480).
- 토론 페르소나 16 수렴 결론: "pad 분리 구성에서 DMIC2를 RE로 설정해도 데이터 캡처에 문제 없다" — both-RE 통일의 HW 허용성 확인.

