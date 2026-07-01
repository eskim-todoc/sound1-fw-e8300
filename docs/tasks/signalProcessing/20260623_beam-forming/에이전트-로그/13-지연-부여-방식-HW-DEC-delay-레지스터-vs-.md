---
name: 토론 페르소나 13
purpose: 지연 부여 방식: HW DEC delay 레지스터 vs SW 버퍼 시프트 — 어느 쪽이 우 토론 수렴
type: tasks
maturity: experimental
tags: [beamforming, agent-log, discuss]
---

# [단계2·토론 13] 지연 부여 방식: HW DEC delay 레지스터 vs SW 버퍼 시프트 — 어느 쪽이 우월한가

## 찬
HW DEC delay 레지스터 우월론 — (1) 해상도 0.26µs(1/240 샘플)로 SW 정수 샘플(62.5µs) 대비 240배 정밀, (2) 목표 0.933 샘플을 DELAY_INTEGER=7+FRAC=14로 양자화 오차 0.024µs 이내 달성(사실D, HW p.450-451), (3) HW 고유 스큐 0.125 샘플(사실A·B)을 동일 레지스터로 통합 보정하여 ch1에 INT=6+FRAC=14 단일 설정으로 완결, (4) decimation 단에서 정렬이 완료되므로 CFX 파이프라인 추가 레이턴시·링 버퍼 관리 불필요.

## 반
SW 버퍼 시프트 필요론 — (1) 착용 기하가 앞=Right인 경우 필요 지연 1.058 샘플이 DELAY_INTEGER 최대 7/8=0.875를 초과하여 HW 단독 커버 불가(사실G), (2) 동적 빔 스티어링 시 HW 레지스터 write 후 decimation flush latency가 데이터시트 미명시로 예측 불가, (3) 사실C(HW p.219)의 "fractional delay 변경이 채널 간 동기화에 영향" NOTE가 채널별 독립 적용의 부작용 가능성을 제기, (4) SW 버퍼는 HW 장비 없이 RTT/printf 디버깅으로 지연 정확도 검증 가능.

## 수렴 결론
현재 보드 구성(Left=앞, 필요 지연 0.808 샘플 — 현재 상태 정리 및 복기.md "DMIC2[1]==DMIC1[0]" 확정)에서 HW DEC delay 레지스터가 명확히 우월하다. SW 버퍼 필요론의 세 반론은 모두 현재 보드에 비적용이다: (1) 착용 기하 역전 시나리오는 현재 확정된 Left=앞에 해당 없음, (2) flush latency 문제는 고정 end-fire 단방향 운용에서 발생하지 않음, (3) 사실C 부작용은 채널별 독립 레지스터라는 사실D(HW p.450-451)에 의해 기각. SW 버퍼는 앞=Right로 방향이 역전되어 필요 지연이 1.058 샘플로 HW 한계 초과 시에만 보완책으로 유효. 실행 권고: lib_audio_in.h에 ch1 전용 매크로(DELAY_INTEGER=6, DELAY_FRACTIONAL=14)를 신규 정의하고 2-DMIC 분기의 SYS_SET_ADC_DEC_CTRL(AUDIO, 1, ...) 호출 값을 ch1 전용으로 교체. ch2는 LIB_ADC_DEC_CTRL_VAL(delay=0) 유지. 잔여 불확실성: 사실C의 채널 간 동기화 영향 범위, DMIC 채널에 대한 레지스터 적용 확인(사실E "ADC와 동일" 주장의 레지스터 주소 레벨 검증), QCC DMIC2 공유 충돌 해소 프로토콜.

## 실행 인사이트


