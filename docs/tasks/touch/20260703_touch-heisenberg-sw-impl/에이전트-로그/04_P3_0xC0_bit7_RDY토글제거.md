---
name: 04-P3-0xC0-bit7-RDY토글제거
purpose: P3 레버(0xC0 bit7 Interface Selection Streaming→Events) 구현-레디 스펙 — write 사이트 5곳 원자적 반영, force_window_open/wait_window 프로토콜 급소 검토, C2 불변식 회귀 체크, fake_func_sleep 경로 반영 확인
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, p3, interface-selection, event-mode, 0xc0, rdy-window, invariant-check, fake-func-sleep, implementation-spec]
---

# 04 · P3 — 0xC0 bit7 Interface Selection (Streaming → Events)

**TL;DR**: 0xC0 bit7(Streaming→Events)로 자가 RDY토글을 제거한다. write 6곳 중 5곳(318·447·453·465·471)에 원자적 반영, 237(ack_reset)은 제외한다. 급소는 force_window_open() 조기반환 소멸에 따른 0xFF 의존도 상승과 func_sleep() 롱터치 카운터의 read-fail 즉시리셋 상호작용이다. L1 공유로 fake_func_sleep()에도 자동 반영된다.

---

## 0. 사이트 수 독립 재확인

원 근거 문서 간 사이트 수가 어긋난 이력이 있다(S3="4곳"→S8="5곳"→V8 코드확정="6곳, ack_reset 제외 시 5곳"). 본 노드가 인용에 의존하지 않고 재실행:

```
grep -n "write_register(REG_SYSTEM_CONTROL" tdc_touch_iqs323.c   → 237,318,447,453,465,471 (6곳, 유일)
grep -rn "0xC0|SYSTEM_CONTROL" src/2__cm3/Cortex-M3-src/ (제외: iqs323.c) → 3건, 전부 무관
  (main.c: g_cm3_manu_reserved[0xC0] 배열크기 / ble: OTA 커맨드 상수 0xC0 / systemControl.h: 헤더가드명)
```

**확립**: IQS323 0xC0 레지스터 write는 전 소스트리에서 `tdc_touch_iqs323.c` 6곳뿐. 본 레버는 이 중 **5곳**을 수정한다.

## 1. 변경 대상 — 파일:라인 / before→after

파일: `src/2__cm3/Cortex-M3-src/systemControl/tdc_touch_iqs323.c`

| # | 라인 | 함수 | 호출 특성 | before(lsb,msb) | after(lsb,msb) |
|---|---|---|---|---|---|
| 1 | :237 | `ack_reset()` | 부팅 1회, POR 직후 최초 write | 0x01,0x00 | **변경 없음**(§2) |
| 2 | :318 | `beta_power_settings()` | 부팅 1회, bit7 최초 확립("base") | 0x50,0x07 | **0xD0,0x07** |
| 3 | :447 | `apply_settings()` 부팅 Re-ATI | 부팅 1회 | 0x54,0x07 | **0xD4,0x07** |
| 4 | :453 | `apply_settings()` 부팅 RESEED | 부팅 1회 | 0x58,0x07 | **0xD8,0x07** |
| 5 | :465 | `tdc_touch_iqs323_re_ati()` | **런타임 반복**(노말·절전·fake 공유) | 0x54,0x07 | **0xD4,0x07** |
| 6 | :471 | `tdc_touch_iqs323_reseed()` | **런타임 반복**(노말·절전·fake 공유) | 0x58,0x07 | **0xD8,0x07** |

값 계산: `0x50|0x80=0xD0`, `0x54|0x80=0xD4`, `0x58|0x80=0xD8` — bit7(0x80)만 OR. Power Mode(bits[6:4]=101 Automatic No ULP)·MSB CH timeout(0x07, CH0~2 disable)는 불변(데이터시트 A.30, `06_레지스터레퍼런스.md:505-520`).

예시 diff(사이트 2, :318):
```c
- ok &= write_register(REG_SYSTEM_CONTROL, 0x50, 0x07);
+ ok &= write_register(REG_SYSTEM_CONTROL, 0xD0, 0x07); /* bit7 Interface Selection=Events 추가 */
```
사이트 3~6도 동일 패턴(값만 0xD4/0xD8로 대체, msb 0x07 불변).

## 2. 237(ack_reset)을 제외하는 판단 근거

1. POR 직후 최초 write 시점 레지스터는 이미 0xC0 POR 기본값(`0x0000`, bit7=0 포함, `06_레지스터레퍼런스.md:119`) — `0x01`(bit0만 set)을 쓰는 것은 bit7을 "0 유지"이지 "0으로 되돌림"(회귀)이 아니다.
2. 이 시점은 `sensor_setup()` 이전이라 CH0 측정 자체가 미설정 — 자가토글 제거 실익이 없는 구간(하이젠베르크 실패 메커니즘 무대는 700ms~2.4s 지속 런타임, `01_C1_실패메커니즘.md`§2).
3. `02_C2_기존기능인벤토리.md`§10 INV-X-1 정의 자체가 "모든 **런타임** write"만 대상 — ack_reset은 정의상 이미 제외 대상(V8 검증 §3 동일 결론).
4. 노출 구간 상한: site#2(:318)가 site#1 이후 수 개 write(confirm_reset·sensor_setup·touch_settings·prox·events_enable)만 지나면 바로 bit7=1로 전환 — 위험 노출 시간이 극히 짧다.

**결정**: 237은 `0x01,0x00` 유지. [가설-강: boot 구간 무해성] — §7 실측 게이트에 등재.

## 3. 적용 시퀀스·조건

1. **원자적 반영 필수**: 사이트 2~6은 반드시 **동일 커밋**으로 수정. 하나라도 누락되면 그 write 실행 순간 bit7이 조용히 0으로 되돌아간다 — INV-X-1이 경고하는 "트리거만 단독 write" 패턴의 **bit7 확장판**(`10_S8_조합시스템전략.md`§3 X-① 동일 지적).
2. **데이터시트 순서 제약 준수 확인**: §8.12(`05_i2c인터페이스.md:365`) "event mode 진입을 위해 Reset Event bit가 set이 아니어야 함". 현 시퀀스는 `ack_reset()`→`confirm_reset()`(Reset Event bit clear 확인, :246-263)→…→site#2 순서라 **이미 준수**(confirm_reset 통과 후에만 bit7=1 write에 도달) — 재배치 불요, 확인만 필요.
3. **Events Enable(0xD3) 선행 확인**: `events_enable()`(:300-304, 0xD3=0x52=ati_error|ati_event|touch_event)이 `beta_power_settings()`(site#2) **이전**에 이미 실행(:427→:431 순서) — Events 진입 시점에 활성 이벤트 집합이 이미 유효. 순서 변경 불요.
4. 런타임 사이트(5·6)는 순서 문제 없음 — bit7은 부팅 시 이미 영구 확립, 매 호출이 동일 값을 재확인(idempotent write).

## 4. 급소 — force_window_open/wait_window_closed 프로토콜 영향

### 4.1 사라지는 것 — IC 자가토글
Streaming(현재): Report Rate(0xC1) 미설정(POR 0x0000) 상태에서 IC는 "측정 사이클 종료마다 무조건" RDY를 토글(`참고/touch/개선/RDY 윈도우 타이밍 분석/3_종합결론.md` — 실측 참고 상한 16ms, Sound1 단일채널 정확값은 [미확인]). Events(변경 후): 활성 이벤트(ati_error·ati_event·touch_event) 미발생 구간엔 RDY 침묵(§8.11.2, `05_i2c인터페이스.md:306-360`) — 이 상시 자가토글이 이벤트 무발생 구간에서 제거되는 것이 P3의 목표 효과다.

### 4.2 늘어나는 것 — 0xFF 강제개방 의존도
`force_window_open()`(:145-175)은 `if(RDY==OPEN) return true;`(:149-152) 조기반환을 갖는다. Streaming에서는 IC의 고빈도 자가토글 덕에 SW 폴링(100ms) 순간 RDY가 우연히 이미 열려있어 0xFF 발행 없이 반환되는 경우가 존재할 수 있다([가설-강], 정확한 비율은 미실측). Events 전환 후 이벤트 무발생 구간(대부분의 유휴 폴링)에서는 이 조기반환 경로가 **사실상 소멸** — 매 폴링 0xFF 강제개방+최대 45ms 대기(`TDC_TOUCH_IQS323_MAX_WAIT_OPEN_MS`, iqs323.h:32)에 의존한다. 이 45ms는 데이터시트 Force Comm t_wait 상한(0.1~45ms, §8.13 `05_i2c인터페이스.md:398`)과 **동일 경계값**이라 여유가 이미 0에 가깝다(`05_S3...md`§9 각주³ 동일 지적). [미확인]: 현 코드의 0xFF write가 §8.13 Figure 8.2의 "무클럭 SDA 토글" 정확히 그대로인지, 아니면 일반 I2C write(클럭 포함)로 유사 효과만 내는지는 확인 못함 — 어느 쪽이든 Streaming에서 이미 동작 중이므로 동작성 자체는 문제없으나, bit7 전환이 이 메커니즘의 응답지연을 바꾸는지는 실측 필요.

### 4.3 [본 노드 자체발견] 롱터치 재부팅 카운터의 read-fail 상호작용
`tdc_touch_sleep_handle_touch_reboot()`(main.c:1125-1153, **`func_sleep()`이 사용** — 실사용 절전 경로)의 가드는 `if (ok && state==TOUCH){cnt++...} else {cnt=0;}`(:1127-1152) — **read 실패(force_window_open 타임아웃 포함, ok=false)는 즉시 카운터를 0으로 리셋**한다(hold 아님, 즉시 리셋). §4.2의 0xFF 의존도 상승으로 45ms 타임아웃 빈도가 늘면, **의도된 2.4초 롱터치(절전→기상 UX)가 24연속 폴링 중 단 1회 실패로도 무한정 재시작**될 위험이 있다 — "오탐→재부팅"을 줄이려다 "정상 기상 제스처"를 더 취약하게 만드는 트레이드오프. 대조적으로 `tdc_touch_sleep_handle_ati_error()`(:1062-1092)는 `ok` 조건이 두 분기 모두에 걸려 있어 실패 시 카운터가 **동결**(리셋도 증가도 없음)만 된다 — 두 워치독의 read-fail 민감도가 다르다는 점을 구현 시 인지해야 한다. `fake_func_sleep()` 자체는 이 재부팅이 이미 no-op(:1011 주석)라 **현재 관찰 증상과는 무관**하지만, `func_sleep()`(실사용)엔 살아있어 배포 전 확인이 필요하다.

### 4.4 읽기 의미론은 불변
이벤트-인에이블(0xD3) 미포함 필드(Power·Slider·Prox event)가 있어도, 창이 열리면(강제든 이벤트든) System Status(0x10) 전체 비트가 항상 최신값으로 읽힌다 — 판정 로직이 소비하는 pressed/prox/ati_error/ati_active 4필드의 **읽기 결과 자체는 bit7과 무관**(§8.12 흐름상 "이벤트가 창을 여는 이유"만 다를 뿐, 읽는 내용은 항상 현재 레지스터 전체). `read_status()`/`read_debug()` 파싱 로직(iqs323.c:347-398) 변경 불요.

## 5. C2 불변식별 회귀 체크

| 불변식 | 판정 | 근거 |
|---|---|---|
| ① Full ATI | 무손상 | 0x36 write 없음, ATI 게이트·경로 미접촉 |
| ② 최초 터치해제 게이트 | 무손상~경미주의 | read 실패 시 notouch_cnt=0 리셋 패턴(main.c:1111 유사)이지 오확정 아님 — 게이트 개방이 지연될 가능성만(§4.2 실측 후 재확인) |
| ③ ATI 에러 Re-ATI | 무손상 | §4.3 — ati_error 카운터는 read 실패 시 동결(리셋도 증가도 없음), 5회 상한 로직 자체 불변 |
| ④ 터치판정(CH0) | 무손상 | System Status 비트 매핑·판정 공식 미접촉(§4.4) |
| ⑤ 롱터치(2400ms) | **주의(본 노드 핵심 발견)** | §4.3 — `func_sleep()` 실사용 기상 제스처가 read 실패 누적 시 지연·재시작 가능. 배포 전 실측 필수 |
| ⑥ 절전 진입/기상 시퀀스 | 무손상 | `func_sleep()`/`fake_func_sleep()` 코드 자체는 무편집, `apply_settings()` 결과값만 상속(간접 영향은 ⑤에 포착) |
| ⑦ 공통 폴링 경로(L1) | **주의(최다 접점)** | force_window_open/wait_window_closed 자체(:128-175)는 무편집이나 그 **호출 성공률 특성**이 바뀜(§4.1-4.2) — 노말·절전·fake 3경로 동시 영향(INV-⑦-1 원칙대로) |
| ⑧ 단일채널 | 무손상 | 0x40/0x50 미접촉 |
| X-① 0xC0 비트보존 | **주의(확장 적용)** | INV-X-1이 "Power Mode·CH timeout"에 "Interface Selection(bit7)"까지 확장됨 — §1 표 5곳 전수 반영·§3 원자적 배포로 대응 |
| X-② READ_FAIL hold | 무손상 | hold 카운트(20폴링) 로직 자체 미접촉. ⑤가 지적하는 것은 이 hold와 **별개**인 ULP 전용 즉시-리셋 로직(main.c, 다른 파일·다른 메커니즘) |
| X-③ 디버그계측 무해성 | 무손상 | read_debug() 로직·호출부 미접촉, bit7 변경은 read_status와 동일하게 영향(FSM 무입력이라 무해) |

## 6. fake_func_sleep() 경로 반영

P3는 `tdc_touch_iqs323.c`(L1)만 수정하고 `main.c`는 무편집이다. `fake_func_sleep()`(:810-1031)은 `tdc_touch_iqs323_read_status()`(:909)·`_re_ati()`(:960)·`_reseed()`(:983,996)를 **`func_sleep()`과 동일한 공유 함수**로 호출한다(grep 결과 두 함수 모두 동일 심볼 사용, 별도 구현 아님) — 따라서 bit7 변경은 **코드 수정 없이 자동으로 fake_func_sleep() 경로에도 적용**된다(`02_C2...`§11·INV-⑦-1의 "L1 공유" 이점 그대로 적용). §4.3의 롱터치 카운터 상호작용은 `fake_func_sleep()` 자체 로직(:1002-1019, 재부팅 자체가 no-op)엔 영향이 없으나 — **동일 read-fail 즉시리셋 패턴이 :1017 `touch_cnt=0`에도 동일하게 존재**하므로, 이 no-op이 향후 원복될 경우 §4.3 리스크가 fake_func_sleep()에도 그대로 재현됨을 기록해 둔다.

## 7. 리스크 · 실측 게이트 (우선순위순)

| 순위 | 항목 | 상태 | 게이트 |
|---|---|---|---|
| 1 | §4.3 `func_sleep()` 롱터치 카운터 read-fail 즉시리셋 상호작용 | [가설-강, 본 노드 자체발견] | 배포 후 RTT로 force_window_open 실패율(지속터치 24폴링 구간) 계측, 필요 시 hold 방식 보강 검토(P3 범위 밖, S6 영역과 협조) |
| 2 | 0xFF 강제개방 빈도 증가·45ms 마진 | [가설-강, S3/S8 계승] | Streaming 대비 Events에서 force_window_open 소요시간 분포 비교(RTT 타임스탬프 또는 오실로스코프) |
| 3 | 237 제외 판단의 boot 구간 무해성 | [가설-강] | 배포 후 부팅 시퀀스 ATI/터치 이상 유무 로그 확인 |
| 4 | IC 자가토글 실제 저감폭(T_주기 실측값) | [미확인] | `RDY 윈도우 타이밍 분석/3_종합결론.md` 측정_1~3 그대로 승계 — Events 전후 오실로스코프 비교 권고 |
| 5 | DIO16(RDY)-CRX0 실제 PCB 결합도 | [미확인] | 스키매틱/레이아웃 확인 필요(S3·S8 동일 미확인 이월) |

## 핵심 결론

1. 6곳 중 5곳(318·447·453·465·471)에 bit7(0x80)을 원자적으로 OR — 값은 0x50→0xD0, 0x54→0xD4(×2), 0x58→0xD8(×2). 237(ack_reset)은 POR 직후 무의미 구간이라 의도적으로 제외한다.
2. 데이터시트 순서 제약(Reset Event bit clear 후 Event mode 진입, Events Enable 선행 설정)은 현 시퀀스가 이미 만족 — 추가 재배치 불요.
3. 급소는 force_window_open()의 "이미 열림" 조기반환 소멸에 따른 0xFF 의존도 상승이며, 이것이 `func_sleep()`의 롱터치 재부팅 카운터(read 실패 시 즉시 0 리셋)와 만나 **의도된 기상 제스처를 약화시킬 수 있다**는 것이 본 노드의 핵심 신규 발견(§4.3) — C2 불변식 ⑤·⑦에 "주의" 등급을 부여한 사유다.
4. `main.c` 무편집으로 fake_func_sleep()에 자동 반영(L1 공유). 8대 불변식 명시적 위반은 0건이나 ⑤·⑦·X-①은 실측 전 "무손상" 단언이 불가하다 — **조건부 구현-레디**.

## 리스크/불확실성

| 항목 | 상태 |
|---|---|
| Streaming 대비 Events의 실제 t_wait(force comm 응답지연) 차이 | [미확인] — 데이터시트 미명시, §7 순위 2 |
| §4.3 리스크의 실제 발생 확률(재현 빈도) | [미확인] — 메커니즘은 확립, 크기는 실측 필요 |
| Report Rate(0xC1, R5 레버) 병행 적용 시 상호작용 | Event 진입 시 0xC1은 무효화됨([가설-강], §8.11.1 "Report Rate는 Streaming 전용" 문언 기반) — R5·P3 순서 무관, 상충 없음 |
| 237 제외가 실제로 안전한지 | [가설-강] — §7 순위 3 |
| 현 0xFF write가 §8.13 Figure 8.2의 무클럭 SDA 토글과 동일 메커니즘인지 | [미확인] — §4.2 각주 |
