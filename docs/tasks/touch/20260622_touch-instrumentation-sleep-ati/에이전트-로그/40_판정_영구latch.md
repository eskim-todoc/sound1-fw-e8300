---
name: 판정-영구latch-수렴
purpose: i2c드라이버 분석(33)과 영구화 메커니즘 분석(34)을 39_판정과 수렴하여 확률적 트리거·영구 latch 메커니즘 단일 확정, 해법 표 평가, 로직 분석기 측정 항목 제시
type: 판정
maturity: stable
tags: [touch, iqs323, rdy, force_window_open, latch, root-cause, logic-analyzer, 0xff]
---

# read_debug 영구 latch — 최종 수렴 판정

> **TL;DR**: 확률적 트리거는 read_debug의 force_window_open이 RDY High 상태에서 0xFF I2C 트랜잭션을 발행하여 IC가 무응답하는 것이며, 이 조건에 착지하는 타이밍이 부팅마다 다른 위상 경쟁에 의해 결정된다. 영구 latch는 read_status가 한 window를 서비스·닫은 직후 read_debug가 구조적으로 RDY High에서 0xFF를 반복 발행하는 결정론적 패턴이다. 드라이버 상태 오염·IC read pointer 오염은 없다. 권고 해법: **(가) 0x10+0x13 4윈도우 통합 combined_read** — 트리거와 latch를 모두 구조적으로 제거한다.

---

## 1. 확정된 사실 기반

| 항목 | 확정 근거 |
|---|---|
| IC 컨버전은 통신과 무관, 자기 주기로 계속 진행 | 데이터시트 §8.8: 마스터 미응답해도 reference value 최신 유지 |
| 정상 환경에서 force_window_open 45ms 내 RDY Low 보장 | IC Streaming LP rate=100ms → 정상 window 주기 ≤100ms, 컨버전 주기 ~0.5~2ms |
| read_status 4회 실측 모두 매 폴링 성공 | `READ 0x10 WINDOW OPEN FAIL` 로그 0건 |
| read_debug만 실패, 비대칭 구조 | `FORCE WINDOW OPEN TIMEOUT + DEBUG READ FAIL (addr)` 경로 |
| force_window_open의 0xFF는 §8.13 Force Comm 아님 | §8.13 Force Comm = SCL 없이 SDA 4회 토글. 현재 코드는 START+0x44+0xFF+STOP I2C 트랜잭션 |
| Stop Bit Disable=0(기본) 환경에서 0xFF의 역할 미정의 | §8.9: 0xFF end-comm은 Stop Bit Disable=1 전용. 0=기본에서 IC 동작 비명시 |
| 드라이버 자체 영구 latch 경로 없음 | 33_분석 확인: 모든 실패 경로 → Error → init_I2c() → Idle 복귀. WritingDone 명시 호출 구제 |

---

## 2. 확률적 트리거 — 수렴

### 2.1 단일 메커니즘

**read_debug의 force_window_open(tdc_touch_iqs323.c:353)이 RDY High 상태에서 진입하여 0xFF I2C 트랜잭션을 발행하고, IC가 45ms 내 RDY Low를 구동하지 않는다.**

세부 경로:
1. `read_status` 완료(read_register 내 wait_window_closed:213) → RDY High 확인
2. `tdc_touch.c:155` read_debug 즉시 호출
3. `force_window_open:139` — `Sys_GPIO_Read(RDY) == WIN_OPEN(0)`가 거짓(RDY High=1) → 0xFF i2c_write 발행
4. `force_window_open:151~160` 45ms 폴링 루프 → RDY Low 미도달 → TIMEOUT, return false
5. `tdc_touch_iqs323.c:353` `||` short-circuit → `i2c_write(&reg, 1)` 미실행
6. `tdc_touch_iqs323.c:355` → `ci_printw("[TOUCH] DEBUG READ FAIL (addr)")`, return false

### 2.2 트리거의 확률성 설명

**왜 27회, 64회 등 들쭉날쭉한가?**

force_window_open 진입 시 RDY 핀 상태는 두 가지다:
- **RDY Low(WIN_OPEN=0)**: read_status의 STOP이 window를 닫은 직후 IC가 빠르게 다음 window를 열었을 때. force_window_open:139~141이 `return true`(0xFF 미발행). read_debug 성공.
- **RDY High(WIN_CLOSED=1)**: IC가 아직 다음 window를 열지 않은 상태. 0xFF 발행 → 45ms timeout.

LP report rate=100ms 환경에서 read_status의 STOP 직후 IC가 다음 window를 여는 시점은 "다음 LP 사이클 완료 시점"이다. 이 시점까지 남은 시간은 `t_remaining = T_lp - (t_elapsed_in_current_cycle)` 이며:

- `t_remaining < 수십 µs`: STOP 직후 즉시 또는 수십 µs 이내에 RDY Low 도달 → force_window_open:139에서 이미 Low 감지 → 성공
- `t_remaining > 수십 µs ~ 100ms`: force_window_open:143에서 0xFF 발행 시점에 RDY High → timeout

부팅마다 `t_elapsed_in_current_cycle` 초기값이 달라지므로(메인 루프 지터 + IC 내부 주기 위상 비동기) 처음 몇 십 회 동안 성공/실패 패턴이 다르다. 성공하는 동안은 IC가 "우연히" force_window_open 진입 시 이미 RDY Low를 구동한 상태이고, 0xFF 발행 없이 통과한다.

**불확실 여부**: 정확한 착지 조건(몇 회에서 전환되는가)은 코드/데이터시트만으로 예측 불가. IC 내부 LP 주기의 위상 안정화 시점과 메인루프 위상의 상대 관계에 의존한다. 27회/64회의 차이는 부팅마다 초기 위상이 달라서 발생하는 것이며, 2차/3차의 무발생은 해당 부팅에서 이 위상 관계가 100회 이상 "운 좋게" RDY Low 구간에 착지했을 가능성이 높다.

> [!NOTE]
> 2차/3차 무발생을 "근본적으로 다른 경로"로 해석할 필요는 없다. 동일 메커니즘에서 위상이 좋으면 100회 이상 연속 성공도 가능하다(force_window_open이 0xFF 없이 통과하는 조건이 지속). 이는 확률적 과정이다.

---

## 3. 영구 latch 메커니즘 — 수렴

### 3.1 단일 메커니즘

최초 0xFF TIMEOUT 발생 후, 다음 폴링부터:

1. **read_status**: IC가 LP 100ms 주기로 자연 RDY Low를 구동 → force_window_open:139가 이미 Low를 감지, 0xFF 미발행, 즉시 통과 → 0x10 addr write → wait_window_closed → data read → wait_window_closed(RDY High) → 성공
2. **read_debug(수십 µs 이내 즉시 호출)**: read_status가 window를 서비스·닫은(STOP) 직후 → RDY High → 0xFF 발행 → 45ms timeout

이 두 단계가 poll interval이 바뀌지 않는 한 매 폴링 결정론적으로 반복된다.

**왜 탈출이 불가능한가?**

- read_debug 실패 경로(line 355~356)에서 복구 동작이 없다. wait_window_closed 미호출, init_I2c 미호출.
- 드라이버 상태는 0xFF i2c_write가 WritingDone→setI2cDriverStatusIdle 경로로 Idle 복귀. 드라이버 자체는 정상.
- 다음 폴링에서 read_status는 IC 자연 window를 서비스하여 성공 → 이 성공이 역설적으로 read_debug에 "RDY High 상태"를 다시 넘긴다.
- IC는 정상(read_status 성공)이므로 IC 리셋 등의 복구 트리거가 발생하지 않는다.

**드라이버 상태 오염 여부**: 33_분석 결론 재확인 — 해당 없음. 0xFF i2c_write는 STOP까지 정상 완료 후 WritingDone → setI2cDriverStatusIdle → Idle. timeout 경로에서 드라이버 상태는 항상 Idle 유지.

**IC read pointer 오염 여부**: addr write(i2c_write(&reg,1))가 short-circuit으로 실행되지 않으므로 IC read pointer를 변경하지 않는다. read_status는 매 폴링 0x10으로 addr write → IC read pointer는 정상.

---

## 4. 코드/데이터시트만으로 확정 불가한 부분

| 불확실 항목 | 이유 |
|---|---|
| 0xFF I2C 트랜잭션이 IC 내부에서 어떻게 처리되는가 | 데이터시트 §8.9: 0xFF end-comm은 Stop Bit Disable=1 전용. Stop Bit Disable=0 상태에서 0xFF 수신 시 IC 내부 동작 미명시. IC가 이를 무시하는지, 내부 상태를 변경하는지 알 수 없다 |
| 0xFF 후 IC가 왜 RDY Low를 45ms 내에 구동하지 않는가 | IC가 0xFF를 "end-comm command"로 인식해 현재 window를 닫고 다음 window 준비를 지연시키는지, 단순히 undefined → no-op → 다음 LP 주기까지 window 없음인지 구분 불가 |
| 2차/3차 부팅의 100회+ 무발생 원인 | 동일 메커니즘에서 위상이 좋아 100회 이상 성공했을 수도 있고, 해당 부팅에서 IC LP 주기 안정화 패턴이 달랐을 수도 있다. 재현성 부족 |
| 0xFF 발행 후 IC가 "완전 latch"(이후 모든 window 억제) vs "단일 timeout"(다음 window는 정상)인지 | read_status가 계속 성공하므로 IC는 정상 window를 제공함. 즉 0xFF가 "IC 완전 latch"를 유발하지는 않고, read_debug의 타이밍 문제가 매 폴링 반복되는 것. 그러나 0xFF가 IC 내부에 누적적 부작용을 미치는지는 미확인 |

---

## 5. 로직 분석기 확인 항목

아래는 코드/데이터시트로 확정 불가한 부분을 실기에서 확인하기 위한 측정 항목이다.

| 측정_번호 | 캡처 대상 | 기대 관측 (가설) | 판정 의미 |
|---|---|---|---|
| 측정_1 | RDY + SCL + SDA — read_status 성공 직후 ~ read_debug 첫 force_window_open 구간 | read_status STOP 직후 수십 µs 이내에 RDY가 High로 복귀. read_debug force_window_open에서 SCL+SDA의 START+0xFF+STOP 트랜잭션 확인. STOP 후 45ms 내 RDY Low 미도달 확인 | 0xFF 트랜잭션이 실제로 버스에 나가는지, RDY 응답이 없는 타이밍을 실기 확인 |
| 측정_2 | RDY — read_status STOP 직후 ~ 다음 RDY Low 엣지까지 간격 측정 | IC LP rate=100ms 설정 → 일반적으로 0~100ms 이내에 RDY Low. 정상 부팅에서는 일부 폴링에서 STOP 직후 ~0.5~2ms 내 즉각 Low도 관찰 가능 | read_debug force_window_open 진입 시 RDY Low인 경우가 존재하는지 확인(성공 구간의 설명) |
| 측정_3 | RDY + SCL + SDA — 0xFF 발행 전후 전체 트랜잭션 캡처 | 0xFF 발행 후 IC가 NACK/ACK 중 무엇을 반환하는지. STOP 후 RDY 핀 변화 관찰 | 0xFF ACK 여부로 IC가 슬레이브 주소를 인식했는지 판별. NACK이면 IC가 통신 거부 상태임 |
| 측정_4 | 영구 latch 진입 후 — read_status의 RDY Low 엣지 타이밍과 read_debug 0xFF 발행 타이밍 비교 | read_status STOP → RDY High(수십 µs~수 ms) → read_debug 0xFF → 45ms 대기 → timeout → read_status (IC 다음 자연 RDY Low). IC는 정상적으로 100ms마다 RDY Low를 계속 구동 | IC가 0xFF에도 불구하고 정상 LP 주기를 유지하는지, 아니면 주기가 변조되는지 확인 |
| 측정_5 | RDY + SCL + SDA — 성공 구간(1~27회 또는 1~64회)에서 read_debug force_window_open 진입 시 RDY 상태 | 일부 폴링에서 read_debug 진입 시 이미 RDY Low(IC가 빠르게 다음 window를 열었음). force_window_open:139가 0xFF 없이 즉시 return true | 성공 구간의 설명 근거 확인. 성공 = RDY Low 진입, 실패 = RDY High 진입 + 0xFF |

---

## 6. 해법 평가 표

| 해법 | 확률적 트리거 제거 | 영구 latch 제거 | 부작용 | 미규명 원인 안전성 |
|---|---|---|---|---|
| **(가) 0x10+0x13 4윈도우 통합 combined_read** | **완전 제거** — read_debug 별도 호출 없음, RDY High 진입 경로 소멸 | **완전 제거** — 트리거 조건 자체가 사라짐 | 윈도우 2개 추가(4→6 per poll). wait_window_closed 실패 시 동작 확인 필요 | **안전** — latch 원인이 미규명이어도 read_debug 호출 경로 자체가 제거되므로 미규명 원인의 영향을 받지 않음 |
| **(나) 계측 저빈도 (N tick마다 1회)** | **미제거** — 호출 시 동일 조건 발생 | **미제거** — N tick마다 latch 재진입 가능 | 계측 빈도 감소 | **불안전** — latch가 더 느리게 발생할 뿐 |
| **(다) read_debug 실패 시 init_I2c·윈도우 정리 추가** | **미제거** — 0xFF 발행 자체는 그대로 | **불확실** — init_I2c 후 IC 상태가 정상화될 수 있으나, 다음 폴링에서 동일 조건 재진입 | 복구 루프 추가로 코드 복잡도 증가. latch 탈출 조건이 명확하지 않음 | **불안전** — 트리거 원인이 남아 있고 복구 조건이 IC 동작에 의존 |
| **(라) 계측 제거 (디버그 블록 OFF)** | **완전 제거** — read_debug 호출 없음 | **완전 제거** — 트리거 조건 소멸 | 디버그 계측 불가. 튜닝 데이터 없음 | **안전** — 가장 단순한 제거. 단, 디버그 목적 달성 불가 |

### 미규명 원인 안전성 추가 설명

영구 latch의 근본 원인("0xFF I2C 트랜잭션이 IC 내부에서 어떻게 처리되는가")이 데이터시트로 완전히 확정되지 않는다 해도:

- **(가)**는 read_debug 호출 경로 자체를 제거하므로, IC 내부 동작의 미규명 부분에 의존하지 않는다. 원인이 무엇이든 트리거 조건이 없어진다.
- **(다)**는 IC 내부 상태 복구를 가정해야 하므로, 미규명 부분에 의존적이다.
- **(나)**는 확률적 트리거를 낮출 뿐 제거하지 못한다.

따라서 **미규명 원인이 있더라도 (가)가 유일하게 완전히 안전한 해법**이다.

---

## 7. 최종 권고

### 권고: (가) 통합 combined_read 4윈도우 구조

`read_debug`를 폐지하고 `tdc_touch_iqs323_read_combined` 함수로 통합한다.

**구현 시퀀스**:
```
윈도우_1: force_window_open → i2c_write(0x10) → wait_window_closed   [addr]
윈도우_2: force_window_open → i2c_read(buf, 2)  → wait_window_closed   [0x10 Status 2B]
윈도우_3: force_window_open → i2c_write(0x13)  → wait_window_closed   [addr]
윈도우_4: force_window_open → i2c_read(buf, 4)  → wait_window_closed   [0x13+0x14 4B]
```

모든 force_window_open이 IC의 자연 RDY Low를 기다리며 진입. 0xFF 발행은 force_window_open 내부 로직에 따라 RDY Low 상태일 때는 발행 안 됨(즉시 return true). 각 윈도우가 이전 STOP 후 IC 자연 window를 기다리므로 "STOP 직후 RDY High 상태에서 강제 진입" 경로가 존재하지 않는다.

**수정 범위**:
- `tdc_touch_iqs323.c`: `read_debug` → `read_combined` (status + debug 통합)
- `tdc_touch_iqs323.h`: 구조체 확장 또는 status/debug 통합 구조체
- `tdc_touch.c:144~167`: 단일 `read_combined` 호출로 통합

**FSM 입력(`tdc_touch_in_t`) 변경 없음** — 회귀 0.

**근거**:
1. 확률적 트리거와 영구 latch를 **모두 구조적으로 제거**한다
2. 드라이버 상태 오염·IC read pointer 오염이 없음이 33_분석에서 확인되었으므로 combined_read로의 전환에 숨겨진 위험 없음
3. 미규명 원인(0xFF IC 내부 처리)에 의존하지 않는다
4. 계측(LTA/Counts) 데이터를 유지하면서 문제를 제거한다

**위험**:
- IQS323 auto-increment: 0x10→0x11→0x12→0x13 순서이므로 0x10에서 6바이트 단순 read는 0x11·0x12를 사이에 포함한다. addr 개별 지정 방식(윈도우_1/2에서 0x10, 윈도우_3/4에서 0x13)이 안전하며 이 구조로 구현한다.
- 윈도우 수 2→4 증가 → 폴링당 통신 부하 약 2배. 폴링 게이트(TDC_TOUCH_POLL_INTERVAL_MS)가 충분하다면 문제없음.
- `wait_window_closed` 반환값 미사용 패턴이 기존 코드에서도 동일하게 유지됨 — 이 부분은 별도 정리 이슈이며 combined_read 자체의 위험은 아님.

---

## 8. 두 분석의 수렴 결론

| 분석 | 트리거 메커니즘 설명 | 영구화 메커니즘 설명 | 수렴 후 결론 |
|---|---|---|---|
| 39_판정 | ATI 과도기 이후 Streaming 리듬 안정화 → 21회 기준 | read_status 성공 후 read_debug 구조적 RDY High 진입 반복 | 부분 정확. 21회 설명은 실측 27/64회와 불일치 |
| 34_분석 | LP 주기와 메인루프 위상의 부팅마다 다른 초기 관계 | 0xFF가 IC next window를 abort → 100ms 내 RDY Low 미도달 반복 | 트리거의 확률성(27/64/무발생)을 더 잘 설명. 영구화 메커니즘은 39와 동일 |
| **수렴** | **위상 경쟁이 트리거의 확률성을 설명. ATI 과도기는 일부 기여** | **read_status STOP 직후 read_debug가 구조적으로 RDY High 진입 + 0xFF 반복 발행** | **메커니즘 단일. 드라이버 latch 없음. 0xFF 오용이 핵심** |
