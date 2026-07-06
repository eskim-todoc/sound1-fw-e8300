---
name: 06-P5-FSM라이드스루
purpose: fake_func_sleep() 전용 재부팅 카운터(ATI에러·터치) 문턱 확장을 구현-레디 스펙으로 — 파일·라인·매크로 값·적용 시퀀스·C2 불변식 회귀 체크·S6-4형 시간원 함정 회피 근거 포함
type: tasks/에이전트-로그
maturity: experimental
tags: [touch, iqs323, heisenberg, fake-func-sleep, watchdog-threshold, ride-through, counter-based, sw-only, implementation-ready]
---

# 06 · P5 — FSM 라이드스루 (fake_func_sleep 재부팅 문턱 확장)

**TL;DR**: `fake_func_sleep()` 전용 재부팅 카운터(ATI에러 임계 5→50, 터치 임계 24→240, 둘 다 [실측 게이트])를 확장하는 구현-레디 스펙. `func_sleep()` 완전 무변경, 신규 0xC0 write 0건, ms-타임스탬프 대신 순수 정수 카운터로 설계해 CFX standby 시간원 정지(S6-4형 함정) 위험을 원천 회피한다. 근본해결이 아닌 안전망임을 명시.

---

## 0. 표기 원칙

01_C1과 동일: **[확립]** 코드/데이터시트 직접 확인, **[가설-강]** 코드+데이터시트 논리적 도출, **[미확인]** 실측 필요.

---

## 1. 스코프 — F0 게이팅의 실체 (신규 런타임 분기 0줄)

- F0 정의: `tdc_get_fake_op_mode() == 1`(main.c:89~97, 기존 getter/setter, 부작용 없음 [확립, 16_V6_S6검증.md §3]).
- `fake_func_sleep()`은 `func_normal()` 루프에서 F0==1일 때만 호출된다(main.c:730 `if (tdc_get_fake_op_mode() == 1) fake_func_sleep();`) — 그 함수에 진입해 있다는 사실 자체가 F0 참. 따라서 본 레버는 새 `if(F0)` 분기를 추가하지 않고 **fake_func_sleep() 로컬 상수를 func_sleep()과 다르게 두는 것만**으로 F0-게이팅을 달성한다(08_S6_FSM판정강건화.md §2와 동일 원칙, 다이어그램도 해당 문서 참조).
- 이미 존재하는 별도 탈출구(손대지 않음): main.c:901~906 `if (tdc_get_fake_op_mode() == 0) { ...; SYS_WATCHDOG_RESET(); }` — 계측 세션 종료(F0가 0으로 복귀) 감지 시 그 폴링에서 즉시 정상 리부팅. 은수님이 계측을 끝내면 본 레버의 확장 문턱과 무관하게 항상 즉시 빠져나올 수 있다 — N을 넉넉히 잡아도 "무한 대기"가 되지 않는 근거.
- `func_sleep()`(main.c:1164~1252)과 5개 static 헬퍼(1035~1163)는 **한 줄도 수정하지 않는다**.

---

## 2. 대상 — 현재 코드 (before)

| 카운터 | 위치(파일:라인) | 현재 임계 | 비교 구조 | 실제 트리거 | 현재 도달가능성 |
|---|---|---|---|---|---|
| `ati_error_reboot_cnt` | 선언 882 · 판정 947~965 · 임계 **949** | 리터럴 `5` | `if (5 < cnt) reboot; else {re_ati(); cnt++;}` (체크가 증분보다 먼저) | 7번째 연속 감지(cnt=6일 때) ≈700ms | 라이브 — `SYS_WATCHDOG_RESET()` 953행 실행됨 |
| `touch_cnt` | 선언 880 · 판정 1002~1019 · 임계 **1007** | `TDC_TOUCH_ULP_REBOOT_TOUCH_CNT`(=24, tdc_touch_time.h:48-49 func_sleep과 공유 매크로) | `cnt++; if (cnt>=24) reboot;` (증분이 체크보다 먼저) | 24번째 연속 샘플 ≈2400ms | **죽은 코드** — 1011행 `SYS_WATCHDOG_RESET()` 주석 처리(§6-3 확인) |

---

## 3. 변경 사양 (before → after)

### 3.1 신규 매크로 — `tdc_touch_time.h`

```c
/* ====================== (C) fake_func_sleep 전용 — F0 라이드스루 확장 문턱 ======================
 * 안전망(세이프티넷)이며 근본해결 아님. func_sleep() 대응 값(5 / TDC_TOUCH_ULP_REBOOT_TOUCH_CNT=24)은 무변경.
 * ms-타임스탬프 아닌 순수 카운터(§6 근거) — 매 tick 정수 증분만, now_ms 비교 없음. */
#define TDC_TOUCH_FAKE_ATI_ERROR_RETRY_CNT  50   /* [실측 게이트] baseline 5의 10배. 실제 리부팅은 52번째 연속 감지(§2 +2 구조) ≈5.2s */
#define TDC_TOUCH_FAKE_REBOOT_TOUCH_CNT     240  /* [실측 게이트] baseline 24의 10배. 240번째 연속 샘플 ≈24.0s */
```

### 3.2 적용 사이트 — `main.c` 2줄 치환

| 파일:라인 | before | after |
|---|---|---|
| main.c:949 | `if (5 < ati_error_reboot_cnt)` | `if (TDC_TOUCH_FAKE_ATI_ERROR_RETRY_CNT < ati_error_reboot_cnt)` |
| main.c:1007 | `if (touch_cnt >= TDC_TOUCH_ULP_REBOOT_TOUCH_CNT)` | `if (touch_cnt >= TDC_TOUCH_FAKE_REBOOT_TOUCH_CNT)` |

**레지스터 변경 0건.** `tdc_touch_iqs323_re_ati()`(iqs323.c:462~465, `write_register(0xC0, 0x54, 0x07)`)를 그대로 재사용 — 재시도 "횟수"만 늘 뿐 개별 write 조합은 불변, INV-X-1 자동 보존.

---

## 4. 적용 시퀀스·조건

1. `tdc_touch_time.h`에 §3.1 매크로 2개 추가(func_sleep 쪽 매크로·리터럴은 그대로 둠).
2. `main.c:949`, `main.c:1007` 두 줄만 치환. 조건문 골격·else 분기·호출 순서는 전부 불변(§1 F0 자동성 — 신규 if 없음).
3. 빌드 확인 → RTT로 "ATI ERROR -> RECOVER" 반복 로그 개수가 새 임계까지 늘어나는지, 정상 복귀 시 `ati_error_reboot_cnt=0` 리셋(966~972행, 무변경)이 그대로 동작하는지 확인.
4. (결정 보류, 본 레버 스코프 밖) `main.c:1011` `SYS_WATCHDOG_RESET()` 주석 복원 여부 — §6-3 open question.

---

## 5. fake_func_sleep 경로 반영

본 레버는 정의상 `fake_func_sleep()` 로컬 변수·리터럴만 대상이다 — 재현 경로 자체가 곧 적용 대상이라 "고쳤는데 재현 경로엔 반영 안 됨" 위험이 원천적으로 없다. `func_sleep()`은 대응 리터럴(main.c:1066 `5`)·매크로(main.c:1130 `TDC_TOUCH_ULP_REBOOT_TOUCH_CNT`)를 그대로 유지해 실사용(정상 절전) 회귀 표면이 0이다.

---

## 6. 카운터 채택 근거 — S6-4형 시간원 함정 회피 (ms-타임스탬프 미사용)

1. **위험의 실체** [확립, `16_V6_S6검증.md` §2.3]: Normal FSM 쿨다운이 쓰는 `now_ms`는 `ci_timer_get_tick()`(`g_ci_timer_main_tick`) — 이 카운터는 `CFX_0_IRQHandler`/`FIFO_5_IRQHandler`에서만 증가(driver_timmer.c, "CFX 인터럽트 전담"). CFX가 ULP standby로 전환되면(`enter_ULP_mode_Command_CM3_to_CFX=1` — `fake_func_sleep()`도 main.c:826에서 동일 커맨드 발행) 이 시간원이 멎을 개연성이 높다[가설-강, 실측 미확인].
2. **함정 시나리오**: 만약 본 레버가 "절대 ms 비교"(`now_ms >= threshold_ms`)로 설계됐다면, standby 중 그 비교가 영구 고정돼 "재시도 자체가 멈추는" 회귀가 발생할 수 있었다 — S6-4가 원안(ms-타임스탬프 이식)에서 겪은 것과 동일 함정.
3. **본 레버의 회피**: `ati_error_reboot_cnt`·`touch_cnt` 둘 다 **매 폴링 정수 증분 + 정수 비교**뿐이며 `now_ms`류를 전혀 참조하지 않는다(§3.2 치환도 비교 상수만 바꿀 뿐 비교 대상 변수·구조는 원본 그대로) — V6가 권고한 "안전한 대안"(폴링-반복횟수 카운터)과 동일 계열.
4. `fake_func_sleep()`의 tick 원천은 `tdc_timer_get_t3_tick()`(TIMER3 ISR, CM3 로컬, initialize.c:272 "1ms 주기")이지 `ci_timer_get_tick()`이 아니다 — CFX standby와 무관[확립]. 단, fake_func_sleep 루프 본체가 실제로 "약 100ms당 1회" 처리되는지(BLE/SPI 인터럽트가 여전히 살아있는 환경에서 t3_tick 게이트의 실질 주기)는 SYS_WAIT_FOR_INTERRUPT 매크로 정의를 본 조사에서 찾지 못해 **[미확인 — 실측 게이트, §8-2]**로 이월한다.

---

## 7. C2 불변식별 회귀 체크

| 불변식 | 판정 | 근거 |
|---|---|---|
| INV-① Full ATI | 무손상(§상세) | 0x36·경로_1~3 미접촉(`tdc_touch_logic.c`는 별 파일, fake_func_sleep 미호출[확립]) |
| INV-② 최초터치해제 | 무손상 | `sleep_ignore`/`notouch_cnt` 게이트(974~1001행)는 독립 if-분기, 본 치환과 상호배제 없음 |
| INV-③ ATI에러 Re-ATI | 주의(기존 비대칭 확대) | 노말의 "TOUCH 중 금지" 게이트는 원래도 절전에 없음(02_C2 INV-③-1) — 본 레버는 그 폭을 F0 중에만 더 벌릴 뿐 골격 불변 |
| INV-④ 터치판정 | 무관 | System Status 비트·0x62 임계 미접촉 |
| INV-⑤ 롱터치 | 무손상(§상세) | |
| INV-⑥ 절전 진입/기상 | 무손상 | 진입 시퀀스(810~868행) 미접촉, "기상=리부팅뿐" 골격 유지(N 도달 시 여전히 리부팅) |
| INV-⑦ 공통폴링 | 무손상 | L1(`read_status`) 함수·45/20ms 타임아웃 미접촉 |
| INV-⑧ 단일채널 | 무관 | — |
| INV-X-1 (0xC0 비트보존) | 무손상 | 신규 0xC0 write 사이트 0개, 기존 `re_ati()` 재사용(§3.2) |
| INV-X-2 (READ_FAIL) | 무관 | hold 로직 미접촉 |
| INV-X-3 (계측 무해) | 무손상(참고 §8-5) | 판정 미입력 성격 불변, 단 세션이 길어지는 만큼 `read_debug()` 총 호출 수는 비례 증가 |

### INV-① 상세 (지시 확인)
Full ATI(0x36) 모드·MULT/COMP write·경로_1~3(`tdc_touch_logic.c`)은 `fake_func_sleep()`이 아예 호출하지 않는 코드다[확립]. 본 레버가 만지는 경로_4는 "복구 실패 시 포기하고 리부팅"까지의 유예 시간만 늘린다 — `tdc_touch_iqs323_re_ati()` 자체·트리거 조합(0x54,0x07)·호출 조건(`ati_error && !ati_active`)은 1바이트도 안 바뀐다. 재시도 횟수가 5→50으로 늘어 IC가 스스로 Band([350,450]) 안으로 복귀할 기회가 오히려 늘어난다 — 드리프트 회피 기능이 약화되는 경로가 없다.

### INV-⑤ 상세 (지시 확인)
노말→절전 롱터치(`tdc_touch_logic.c:172~190`, 2400ms 1회 래치)는 `fake_func_sleep()`과 별개 파일·미호출[확립]. 절전→노말(웨이크) 방향의 진짜 대칭 구현은 `func_sleep()`의 `tdc_touch_sleep_handle_touch_reboot()`(main.c:1125~1153, `TDC_TOUCH_ULP_REBOOT_TOUCH_CNT` 그대로 유지) — 본 레버는 `fake_func_sleep()` 로컬 `touch_cnt`만 건드리므로 이 대칭에 전혀 참여하지 않는다. 유일한 영향은 "계측 세션 중(F0=true)에 한해" 웨이크 방향이 느려지는 의도적 예외(08_S6 각주²·16_V6 §2.2 동의 재확인)이며, 대상 자체가 현재 사문화(1011행 주석)라 즉시 효과는 0이다.

---

## 8. 리스크 · 실측 게이트

1. **N값 확정 전** [실측 게이트, 최우선] — 50/240은 baseline×10의 잠정 제안. P0(실측 프로토콜) 산출물의 RTT 로그로 (a) ati_error 연속지속 실측 분포, (b) 통상 계측 세션 길이를 확보 후 재산정 권고.
2. **fake_func_sleep 루프 주기가 정말 ~100ms/count인지** [미확인, 신규 플래그] — t3_tick 자체(TIMER3, CM3 로컬)는 CFX standby와 무관함이 확립됐으나, BLE/SPI 인터럽트가 살아있는 fake_func_sleep 환경에서 `if (last_t3_tick < tdc_timer_get_t3_tick())` 게이트가 정확히 func_sleep()의 100ms HW 타이머 주기와 동일하게 동작하는지는 SYS_WAIT_FOR_INTERRUPT 실제 웨이크 우선순위까지 재확인하지 못했다. 실제 주기가 다르면 §3.1의 ms 환산(≈5.2s/≈24.0s)만 재조정하면 되고, 카운트 자체(50/240)의 안전성 논리(순수 정수 비교)에는 영향 없다.
3. **S6-2 발동 전제** (16_V6 §5 재확인) — `touch_cnt` 확장은 main.c:1011의 `SYS_WATCHDOG_RESET()` 주석이 복원돼야 실효한다. 복원 여부·사유(커밋 메시지 미기재, 08_S6 §1)는 은수님 확인 필요 — 본 레버 스코프 밖 결정 사항으로 열어둔다.
4. **근본해결 아님**(지시 명시) — 본 레버는 하이젠베르크 애그레서(SPI/SYSCLK/QCC/I2C 등)를 물리적으로 전혀 저감하지 않는다[16_V6 §1 동의]. 오염이 세션 내내 거의 100% duty(체계적)라면 N을 아무리 늘려도 "리부팅을 늦출" 뿐 복구를 보장하지 않는다 — S1~S5(레지스터/클럭 레버)와 반드시 병행. 단, `fake_func_sleep()`은 `I2C_MASTER_PRESCALE_240`(128kHz)을 써 C1의 H1(SCL 5.12MHz 위반) 가설은 이 경로에 한해 반박됐다(16_V6 §1) — H2(read_debug 중복 트랜잭션)·SPI/BLE 코히런트 가능성은 여전히 미확인.
5. 확장된 재시도 구간 동안 `read_debug()`(`TDC_TOUCH_DEBUG_PRINT_ENABLE` 기본 1, 01_C1 §3.2)도 그만큼 더 오래 반복 호출된다 — 신규 애그레서 추가는 아니며(빈도·조건 불변, 세션이 길어지는 만큼 총 호출 수만 비례 증가) H2가 사실이면 P1(관측 탈침습)과의 병행 적용이 중요해진다.

---

## 핵심 결론

1. `fake_func_sleep()`의 재부팅 카운터 2종(ati_error 임계 리터럴 `5`@main.c:949, touch 임계 `TDC_TOUCH_ULP_REBOOT_TOUCH_CNT`@main.c:1007)을 fake_func_sleep 전용 신규 매크로(제안 50/240, ×10, [실측 게이트])로 교체 — 딱 2줄 치환 + 헤더 매크로 2개 추가로 완결.
2. `func_sleep()`·5개 static 헬퍼는 한 줄도 안 건드림 — 실사용 절전 경로 회귀 표면 0.
3. ms-타임스탬프를 쓰지 않고 기존 정수 카운터 패턴을 그대로 유지해, 16_V6가 S6-4에서 발견한 "CFX standby 시간원 정지 → 쿨다운 영구고정" 함정을 원천 회피한다.
4. INV-①(경로_4 재시도 기회 증가로 드리프트 회피 오히려 강화)·INV-⑤(별개 함수·현재 사문화라 무손상) 모두 무손상 확인. INV-③만 "기존 비대칭 확대"로 의식적 예외 처리.
5. 근본해결이 아닌 안전망 — S1~S5 병행이 전제이며, N값·fake_func_sleep 실제 tick 주기 둘 다 실측 게이트로 이월한다.
