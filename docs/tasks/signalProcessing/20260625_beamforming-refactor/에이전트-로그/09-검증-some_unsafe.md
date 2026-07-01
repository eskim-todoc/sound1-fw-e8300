---
name: 리팩토링 검증 09
purpose: some_unsafe
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify]
---

# [검증 09] some_unsafe

## 근거
실파일 Read 결과 두 항목이 behavior-preserving이 아님을 확인했습니다.

**[unsafe #1] `[loop/high]` audioMixer.c:59-74 — 동작보존 위험 감별가 항목 (`보존=false` 명시)**
실코드(audioMixer.c L59-74) 확인 결과 i=0..14는 `p_delay_buf0[i+1]`을 참조하고, i=15만 `p_delay_buf1[0]`을 참조합니다. 동작보존 위험 감별가가 이미 `보존=false`로 명시했음에도 불구하고, 루프·복사 효율 분석가와 헤더·스코프·데드코드 분석가, 인벤토리 종합 분석가가 동일 코드(L59)에 대해 `보존=true`인 split-loop 안을 제시하고 있습니다. split 자체(i=0..14 루프 + i=15 별도 문장)는 수학적으로 올바르지만, CFX chess 컴파일러의 `chess_loop_range` DSP HW 루프 스케줄러가 완전 언롤된 현재 코드와 다른 사이클 타이밍을 생성할 수 있습니다. 더 심각하게는, 나이브한 단일 루프(i=0..15)로 구현하면 `p_delay_buf0[16]`에 OOB 접근이 발생합니다. 복수의 분석가가 상충되는 보존 판정을 내렸으므로 unsafe로 간주해야 합니다.

**[unsafe #2] `[loop/high]` main.c:158 — 인벤토리 종합 분석가 항목 (`보존=false` 명시)**
실코드(main.c L158-165) 확인 결과 `lib_g_i2s_buffer_in_pos`는 0↔1 ping-pong 방식입니다. 인벤토리 종합 분석가가 제안한 `^= 1` XOR 토글은 동일 동작이지만, lib_i2s.c에서 동일 변수를 modulo-8(I2S_BUFFER_FULL_READY_CNT=8) 방식으로 증가시키는 경로와 혼용 가능성이 있어 `보존=false`로 명시됨. 인벤토리 자체가 보존 불가 판정을 내린 항목.

**[주의] `[loop/medium]` main.c:324 mic 통합 루프 — 루프·복사 효율 분석가**
`(mic==0) ? HCT_A0_0 : HCT_A0_1` 삼항식을 chess_loop_range 내부에서 사용하면 CFX chess 컴파일러의 XMEM 포인터 aliasing 처리가 분리된 두 루프와 달라질 수 있습니다. 인벤토리 자체도 "컴파일러 동작 검증 필수"로 표시했으며 `보존=true`라고 했지만 CFX DSP 특화 환경에서는 출력 동일성이 보장되지 않습니다.

나머지 항목(상수화, 주석 추가, 데드코드 제거, naming rename 등)은 실코드 대조 결과 제안값이 원 리터럴과 동일하고 런타임 경로에 영향을 주지 않아 behavior-preserving이 맞습니다.

## flagged
- [loop/high] audioMixer.c:59-74 (동작보존 위험 감별가): 보존=false 명시 항목. split-loop 방식이 수학적으로 올바르더라도 CFX chess_loop_range DSP HW 루프 스케줄러가 완전 언롤된 현재 코드와 다른 사이클 타이밍을 생성할 수 있음. 나이브 단일 루프 구현 시 p_delay_buf0[16] OOB 접근 위험. 루프·복사 효율 분석가·헤더·스코프 분석가·인벤토리 종합 분석가가 같은 라인에 보존=true로 상충 판정을 내려 혼선이 있음 — 동작보존 위험 감별가의 보존=false 판정이 올바름.
- [loop/high] main.c:158 (인벤토리 종합 분석가): 보존=false 명시 항목. lib_g_i2s_buffer_in_pos ^= 1 XOR 토글이 0↔1 ping-pong 자체는 동일하지만 lib_i2s.c의 modulo-8 증가 경로와 혼용 시 동작 불일치 위험.
- [loop/medium] main.c:324 mic 통합 루프 (루프·복사 효율 분석가): 보존=true로 주장하나 (mic==0) ? HCT_A0_0 : HCT_A0_1 삼항식을 chess_loop_range 내 XMEM 포인터로 사용할 때 CFX chess 컴파일러의 포인터 aliasing 처리가 원래 두 독립 루프와 달라질 수 있음. 출력 값 동일성이 CFX DSP 환경에서 보장되지 않음.
