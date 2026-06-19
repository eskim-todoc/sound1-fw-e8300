---
name: adversary-assumption-v2
purpose: 명제 G~J 및 에이전트 10~14 결론의 가정 공격 — 스트리밍 200ms 의존·명제_J 허점·force_window_open 불필요 반례
type: 에이전트-로그
maturity: stable
tags: [touch, iqs323, adversarial, streaming, lta, ati-band, counts-saturation, force-window-open, 명제_G, 명제_H, 명제_J]
---

# 15 adversarial 반증가 — 명제 G~J 가정 공격 v2

> **TL;DR**: ① 스트리밍 윈도우 "200ms 의존"은 전력·노이즈 미확인 측면에서 위험한 전제이며, 폴링 방식 유지 결론은 Event Mode 비교 없이 내려졌다. ② 명제_J "FIXED LTA delta 충분"은 LTA가 ATI Band를 크게 이탈할 때 counts 분해능 저하·포화(Max Counts 한계)·threshold 실질 무효화로 붕괴하는 경로가 존재한다. ③ "force_window_open 불필요" 결론은 Event Mode + 인터럽트 구조를 가정하지 않은 채 내려졌으며 현 폴링 구조에서의 반례가 있다.

---

## 1. 공격 대상 명제·결론 목록

| 공격 대상 | 에이전트 결론 | 출처 |
|---|---|---|
| **명제_G**: force_window_open 비용 "무시할 수 없음" | 0.1~45ms 대기, 비용 크다 | 10_rdy-window-timing.md §4 |
| **명제_H-B**: force_window_open 불필요 → 거짓 | 현 폴링 구조에서 삭제 불가 | 11_streaming-window.md §4 |
| **명제_H-A**: 통신 윈도우 200ms 개방 → 거짓 | 정상 t_Low ≈ 0.5ms | 11_streaming-window.md §3 |
| **명제_J**: ATI Disabled + LTA IIR만으로 터치 성립 → 참 | ATI 독립, threshold 유효 | 12_ati-lta-convergence.md §2.5 |
| **종합 결론**: 현 구조(폴링 + force) 유지 권고 | 스트리밍 모드 설정 코드 없음 | 13_code-verify.md §3 |

---

## 2. 공격 ① — 스트리밍 윈도우 200ms "의존" 결론의 전력·노이즈 미확인 위험

### 2.1 에이전트 결론의 논리 구조

11_streaming-window.md §4.2는 "스트리밍 모드에서 200ms 폴링을 쓰는 이상 force_window_open은 구조적 필수 요소"라고 판정했다. 이 결론은 **현 폴링 구조를 고정 전제로 두고 force_window_open을 필수화**한다.

그러나 이 논리 구조 자체가 "스트리밍 + 폴링" 조합을 기정사실로 받아들이는 함정이다.

### 2.2 스트리밍 모드 자체의 미검증 비용

데이터시트 §8.11.1 확정 사실(14_datasheet-verify.md §2): 스트리밍 모드는 **이벤트 유무에 관계없이** report rate마다 RDY를 LOW로 assert한다.

Report Rate = 0(기본값, 즉시 토글) + NP 전력 모드 조건에서:
- IQS323은 측정 완료마다 즉시 RDY를 토글한다.
- 측정 주기가 수ms라면 초당 수십~수백 회 RDY 토글 발생. [추정]
- **RDY 핀은 open-drain(§8.6)**이며 풀업 저항을 통해 전류가 흐른다. 폴링 미접속 상태에서도 RDY가 LOW로 유지되는 구간마다 풀업 전류가 소모된다.
- 스트리밍 모드 유지가 배터리 기기(Sound1, 인공와우 외부기)에서 유발하는 **추가 전류 소모가 정량화된 근거가 어디에도 없다.**

데이터시트 §8.11.2 노트 원문(14_datasheet-verify.md §2): "Streaming은 디버깅·개발 단계에서 임시 활용, **제품 펌웨어는 Event Mode 권장**."

에이전트 결론은 이 권장을 인용했지만(11_streaming-window.md §5.2), 스트리밍 모드 유지 비용 대비 Event Mode 전환 비용을 **정량 비교하지 않은 채** "현 구조 유지"를 권고했다.

### 2.3 노이즈 환경에서 RDY 오인식 위험 [추정]

스트리밍 모드에서 RDY가 수ms마다 토글되면, ESD·전자기 간섭(인공와우 외부기는 RF 주변 환경)에서 RDY 라인의 글리치가 **정상 토글과 구별 불가능**해질 수 있다. [추정]

force_window_open은 RDY LOW를 윈도우 열림으로 판단해 I2C를 시작한다(13_code-verify.md §1). 스트리밍이 아닌 방식(Event Mode + 인터럽트)이었다면 글리치를 소프트웨어에서 필터링할 여지가 있지만, 현 busy-wait 폴링 방식은 글리치 구별 메커니즘이 없다.

> [!IMPORTANT]
> 이 노이즈 취약성은 실측이 없어 [추정]이다. 그러나 "스트리밍 + 폴링 유지" 결론이 이 위험을 검토한 흔적이 없다.

### 2.4 반증 요약

| 에이전트 결론 | 반증 | 심각도 |
|---|---|---|
| 스트리밍 + 폴링 구조에서 force_window_open 필수 | 전제 자체(스트리밍 유지)의 전력 비용 미정량화 | 중 |
| Event Mode 전환 시 force 불필요 언급 | Event Mode가 전력·노이즈 측면에서 우월한지 정량 비교 없음 | 중 |
| 데이터시트 §8.11.2 "제품 = Event Mode 권장" 인용 | 권장 사유(전력·신뢰성)를 따르지 않는 선택의 비용이 미검증 | 높음 |

---

## 3. 공격 ② — 명제_J "FIXED LTA delta 충분"의 허점 — counts 분해능·포화·threshold 실질 무효화

### 3.1 명제_J 결론의 핵심 가정

12_ati-lta-convergence.md §2.2 핵심 논거:

> "ATI Disabled에서 noTouch Counts = 고정 MULT/COMP 결과값. RESEED 후 LTA ≈ noTouch Counts. 터치 시 delta = LTA - 터치Counts → threshold 비교 유효."

이 논거는 **"noTouch Counts가 ATI Target 부근의 합리적 범위에 있다"**는 암묵적 전제에 의존한다. 그러나 LTA가 ATI Band를 크게 이탈하는 시나리오에서 이 전제가 붕괴한다.

### 3.2 공격 경로 A — counts 절대값 범위와 분해능 저하

IQS323의 counts는 내부 ADC 또는 적산 방식으로 출력되는 정수값이며, 최댓값(Max Counts)이 존재한다. 데이터시트는 ATI 알고리즘의 목적을 "기기 간 편차를 흡수해 counts를 ATI Target으로 정규화"로 명시한다(12_ati-lta-convergence.md §1.1, DS §5.9).

**환경 변화(온도·습도·ESD 후 특성 변화)로 noTouch counts가 ATI Band를 크게 이탈하는 시나리오:**

```
시나리오:
  ATI Target = 400, Touch Threshold = 156
  환경 drift → noTouch Counts 상승 → LTA 추적 → LTA = 800

  문제_A: LTA = 800이면 터치 시 Counts 감소가 150 이하여야
          (LTA - 터치Counts) > 156 → 터치Counts < 644
          실제 터치 delta가 기기 물리 특성상 150 이하인지
          검증된 근거가 없다. [추정]

  문제_B: noTouch Counts = 800이면 Max Counts에 근접할 수 있다.
          counts 포화(saturation) 시 터치에 의한 감소 폭 자체가
          물리적으로 제한된다. 포화 근처에서는 delta가 작아진다.
```

12_ati-lta-convergence.md §2.3는 "LTA가 drift된 noTouch counts를 계속 추적하므로 delta 성립"이라 했으나, **counts의 절대값이 측정 범위 상한에 근접하면 LTA 추적 자체는 성립하더라도 delta의 절대값이 threshold보다 작아진다.**

이 경로에서 threshold는 **형식상 유효하지만 실질적으로 무효화**된다.

### 3.3 공격 경로 B — LTA IIR 추적 지연과 "가짜 delta" 구간

14_datasheet-verify.md §4: LTA IIR 수식: `LTA_new = LTA_old + (Counts - LTA_old) × Beta/256`

Beta 최대값 = 15 → alpha = 15/256 ≈ 5.9%. 환경이 급격히 변화하면:
- counts가 급상승(온도·습도 급변)할 때 LTA는 5.9%씩 천천히 추적
- 수렴 전 "가짜 delta" 구간 발생:
  - Counts가 급상승하면 LTA < Counts → (LTA - Counts) < 0 → 터치 미인식(정상처럼 보이나)
  - 반대로 Counts가 급감하면 LTA > Counts → delta 양수 → **환경 변화가 터치로 오인**될 수 있다 [추정]

12_ati-lta-convergence.md §2.5 "급격 변화 시 추적 지연으로 일시 오동작 가능 [추정]"이라고 표기했으나, **이 "일시"가 인공와우 외부기 실사용 환경에서 얼마인지 정량화되지 않았다.**

### 3.4 공격 경로 C — ATI Band 이탈 시 MULT/COMP 고정의 누적 문제

ATI Full이면 Re-ATI가 MULT/COMP를 재조정해 counts를 ATI Target(400)으로 복귀시킨다. ATI Disabled(현 펌웨어)에서는 이 재조정이 없으므로:

```
초기: noTouch Counts = 400 (RESEED로 LTA=400)
환경 drift: noTouch Counts → 600
  → LTA 추적: LTA → 600
  → 터치 Counts = 390(예) → delta = 600-390 = 210 > 156 → 터치 인식 (아직 유효)

추가 drift: noTouch Counts → 900
  → 접근 Max Counts 한계 [추정]
  → 터치 시 Counts 감소 폭 제한: 예를 들어 Counts = 870
  → delta = 900-870 = 30 < 156 → 터치 미인식
```

이 경로는 LTA 추적이 완벽하게 동작하더라도 **counts 포화 근처에서 threshold가 실질 무효화**된다.

12_ati-lta-convergence.md는 이 Max Counts 한계 경로를 언급하지 않았다. 명제_J "참" 판정의 주요 누락이다.

### 3.5 반증 요약

| 명제_J 결론 | 반증 경로 | 심각도 |
|---|---|---|
| LTA 추적으로 delta 보존 → threshold 유효 | counts 포화(Max Counts 근접) 시 delta 자체가 threshold보다 작아짐 | 높음 |
| 환경 drift에도 LTA IIR이 따라감 | LTA 추적 지연 구간에서 가짜 delta → 오인식 [추정] | 중 |
| Re-ATI 스킵이 판정식 바꾸지 않음 | MULT/COMP 고정으로 counts 절대값이 비정상 범위로 drift 가능 | 높음 |
| ATI Disabled 현 펌웨어에서 실제 동작 중 | 실측 데이터 없음 — 동작 중=문제 없음 등호가 아님 | 중 |

> [!IMPORTANT]
> 명제_J "참" 판정의 핵심 조건이 "LTA가 합리적 counts 범위 안에 머무는 한"이라는 묵시적 전제다. 이 전제가 필드에서 항상 성립하는지 실측 없이는 알 수 없다.

---

## 4. 공격 ③ — force_window_open "불필요" 결론의 반례

### 4.1 에이전트 11_streaming-window의 결론 재확인

11_streaming-window.md §4 "명제_H(2-2)-B 거짓: force_window_open은 현 폴링 구조에서 삭제 불가하다."

이 판정 자체는 옳다. 그러나 은수님 원본 질문(00-v2, 의문 2-2): "스트리밍 모드에서 약 200ms 동안 통신 윈도우가 열려 있다면, 굳이 force로 열 필요가 있나?"의 근저 가정은 다르다.

### 4.2 "force_window_open 불필요" 반례 — Event Mode 전환 시

에이전트 결론은 "현 폴링 구조에서 force 필수"를 증명했지만, 이것이 "어떤 구조에서도 force가 필수"는 아니다.

**반례**: Event Mode + falling-edge 인터럽트 구조

```
Event Mode(§8.11.2):
  - 이벤트 발생 시에만 RDY assert
  - MCU가 RDY falling-edge 인터럽트로 즉시 응답
  - 응답 시점에 RDY는 이미 LOW(window 열림)
  → force_window_open() 첫 줄 GPIO 체크에서 즉시 true 반환
  → 0xFF Force Comm 불필요 → 대기(0.1~45ms) 불필요
```

13_code-verify.md §1이 확정한 force_window_open 구조에서 "이미 열려 있으면 즉시 true 반환"(L147~149)이 바로 이 경로다. 즉 **force_window_open 함수 자체는 Event Mode + 인터럽트 구조에서도 호환되며, 그 경우 실제 Force Comm(0xFF)을 전송하지 않는다.**

에이전트 11의 결론 "force_window_open 삭제 불가"는 현 폴링 방식 유지를 전제한 것이다. 구조 전환(Event Mode + 인터럽트) 시 force_window_open 함수는 남겨도 무해하지만 0xFF 전송 비용은 0이 된다. 이것이 은수님 질문의 실질적 의미였을 수 있다.

### 4.3 "200ms 의존 결론"이 놓친 것

11_streaming-window.md §3.2가 확정한 "t_Low ≈ 0.4~0.5ms (정상 서비스)" 데이터는 실제로 이렇게 해석해야 한다:

- 스트리밍 모드에서 t_Low(윈도우 개방 구간)는 실제 트랜잭션 시간뿐이다.
- 200ms 폴링에서 force_window_open의 비용(0.1~45ms)이 매 사이클 발생한다(10_rdy-window-timing.md §3.2).
- read_register 1회 = force_window_open 2회 + wait_close 2회 = **최악 130ms, 정상 수~20ms** (13_code-verify.md §4).
- 즉 200ms 폴링 주기 중 최대 65%가 윈도우 대기에 소비될 수 있다. [추정]

이 비용을 "무시할 수 없음"이라 판정한 10_rdy-window-timing.md §4는 정확하다. 그러나 이 비용의 **원인이 스트리밍 모드 자체가 아니라 "스트리밍 + 폴링 불일치"**임을 강조하지 않았다. Event Mode + 인터럽트로 전환하면 이 비용이 거의 0이 된다.

### 4.4 반증 요약

| 에이전트 결론 | 반례 | 심각도 |
|---|---|---|
| force_window_open 삭제 불가 (현 구조에서) | Event Mode + 인터럽트 전환 시 Force Comm(0xFF) 비용이 0이 됨 | 낮음(현 구조 판정 자체는 맞음) |
| 스트리밍 + 폴링 구조 기정사실화 | 데이터시트 §8.11.2가 Event Mode를 제품 권장으로 명시 | 높음 |
| 윈도우 비용 "무시 불가" 결론 | 비용의 원인(폴링 불일치)과 해법(구조 전환)을 분리하지 않음 | 중 |

---

## 5. 누락 대안 — 에이전트 10~14가 검토하지 않은 것

| 누락 항목 | 검토됐어야 할 내용 | 영향 |
|---|---|---|
| **Event Mode + 인터럽트의 정량 비용** | GPIO 인터럽트 핀 사용 여부, CM3 NVIC 설정 가능성 | force_window_open 0xFF 비용 원천 제거 가능성 |
| **Max Counts 값** | IQS323 counts 최대 출력값, 포화 조건 | 명제_J의 "threshold 유효" 전제 검증 |
| **Beta별 LTA 수렴 시간 정량** | Beta=1~15에 따른 샘플 수 → ms 변환 | 급격 환경 변화 시 오동작 구간 길이 |
| **실운용 noTouch counts 범위 실측** | RTT로 noTouch 상태 counts 장기 모니터링 | 명제_J 필드 유효성 확인 |
| **스트리밍 모드 전류 소모 실측** | RDY 풀업 + IQS323 활성 소비 전류 × report rate | 배터리 기기에서 모드 선택 근거 |

---

## 6. 종합 반증표

| 공격 번호 | 대상 | 핵심 반증 | 심각도 |
|---|---|---|---|
| **공격_1** | 스트리밍 200ms 의존 결론 | 전력 비용 미정량, 노이즈 취약성 미검토, Event Mode 정량 비교 없음 | 높음 |
| **공격_2-A** | 명제_J counts 포화 | Max Counts 근접 시 delta < threshold → 실질 무효화 | 높음 |
| **공격_2-B** | 명제_J LTA 추적 지연 | Beta 스무딩으로 급변 구간 오인식 [추정] | 중 |
| **공격_2-C** | 명제_J MULT/COMP 고정 누적 | 장기 drift로 counts 비정상 범위 이탈, threshold 공동화 가능 | 높음 |
| **공격_3** | force_window_open 불필요 반례 | Event Mode + 인터럽트 시 0xFF 비용 = 0, 구조 전환이 근본 해법 | 중 |

---

## 7. 실측 요청 — 반증 해소를 위한 최소 측정

| 실측 ID | 항목 | 방법 | 목적 |
|---|---|---|---|
| 실측_V1 | noTouch Counts 장기 범위 | RTT 로그, 상온·고온·다습 조건 24시간 | 명제_J counts 포화 경로 현실성 확인 |
| 실측_V2 | Max Counts 값 | 데이터시트 정밀 확인 또는 채널 강제 차폐 후 포화 측정 | 포화 margin 정량 |
| 실측_V3 | 스트리밍 모드 전류 소모 | 전류 프로브, NP 전력 모드 + Report Rate=0 조건 | Event Mode 전환 전력 절감 정량 |
| 실측_V4 | Event Mode + 인터럽트 전환 가능성 | CM3 GPIO 인터럽트 핀 여유 확인 (회로도) | force_window_open 구조 전환 타당성 |

---

## 참조 근거

| 문서 | 참조 항목 |
|---|---|
| 10_rdy-window-timing.md §3.2, §4 | force_window_open 비용 0.1~45ms, "무시 불가" 판정 |
| 11_streaming-window.md §3, §4, §5 | t_Low 0.5ms, force 불필요 = 거짓, Event Mode 권장 |
| 12_ati-lta-convergence.md §2.2, §2.3, §2.5 | 명제_J 참 판정, LTA 추적 가정, 급변 [추정] 표기 |
| 13_code-verify.md §1, §4 | force_window_open 구조 확정, 130ms 상한 |
| 14_datasheet-verify.md §2, §4 | 스트리밍 §8.11.1·§8.11.2, LTA IIR §5.6, 터치 판정 §5.7 |
| DS §8.11.2 노트 | "제품 펌웨어 = Event Mode 권장" |
| DS §5.9, §5.10 | ATI 정규화 목적, Re-ATI 조건 |
