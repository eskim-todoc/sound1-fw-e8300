---
name: 빔포밍구현 검증 14
purpose: 적대적 검증
type: tasks
maturity: experimental
tags: [beamforming, impl-review, agent-log, verify]
---

# [검증 14] SW 1샘플 + HW FRAC=6(0.025) = 1.025샘플이 22mm/343m·s 이상값(1.026샘플)에 정합한다

**판정**: `CONFIRMED`

## 근거
주장의 세 연결 고리를 모두 코드·데이터시트로 직접 검증했다.

**고리 1 — FRAC=6 = 0.025샘플**
`lib_audio_in.h:112-113`에서 `LIB_ADC_FRACTIONAL_DELAY_6 = (6 << AUDIO_ADC_DEC_CTRL_DELAY_FRACTIONAL_Pos)`, `LIB_ADC_DEC_CTRL_VAL_0_0250 = ... | ADC_INTEGER_DELAY_0 | LIB_ADC_FRACTIONAL_DELAY_6 | ...`로 INT=0, FRAC=6임이 명시된다. 분모 240은 데이터시트 그라운딩 문서(`조사_데이터시트-그라운딩.md`)에서 SFCR = round(3.84MHz/8/16kHz)-1 = 29, 분모 = 8×(29+1) = 240으로 확정된다. 따라서 FRAC=6 → 6/240 = 0.025000샘플이 수식적으로 정확하다. `LIB_ADC_DEC_CTRL_VAL_0_9958`(INT=7, FRAC=29) = 7/8+29/240 = 0.99583… 역시 매크로 이름과 일치해 분모 240 전제를 교차검증한다.

**고리 2 — SW 1샘플 구현**
`audioMixer.c:59-74`에서 `p_mix[i] = ... p_delay_buf0[i+1] ...` (i=0..14), `p_mix[15] = ... p_delay_buf1[0] ...`로 지연 채널을 인덱스 i+1로 읽는다. 이는 블록 내 1샘플 오프셋이다. 이 해석의 전제(index 0=최신)는 분석 종합 문서에서 `tdc_copy_DMIC_buffers`의 버퍼 롤링 방향으로 검증된 것으로 기재되어 있으며, 코드 리뷰 전반에서 동일하게 채택된 가정이다.

**고리 3 — 합산 1.025 vs 타깃 1.02624**
0.022m ÷ 343m/s × 16000Hz = 1.026239샘플. 반올림 시 1.026으로 주장과 일치. 1.025 vs 1.026239의 절대 오차 = 0.001239샘플로, HW 1스텝(1/240 = 0.004167샘플)의 0.30배에 불과하다. 즉 오차가 레지스터 최소 조정 단위 미만이다.

**실제 코드 반영 확인**
`system_control.c:88`(`SYS_SET_ADC_DEC_CTRL(AUDIO, 2, LIB_ADC_DEC_CTRL_VAL_0_0250)`)와 `:93`(`ch0`에 동일 매크로)에서 맵변경 이벤트 시 실제로 적용됨이 확인된다.

**잔여 불확실성(주장 자체를 흔들지 않음)**
블록 내 인덱스 방향(index 0=최신)은 RTT 실측으로 최종 확정이 권고되지만, 분석 전체가 이 가정을 일관되게 채택하고 있어 주장의 산술 정합성에는 영향 없다. 또한 HW frac는 맵변경 이벤트에서만 적용되어 init 직후 구간에는 0.025샘플이 미적용되지만, 이는 주장(수치 정합)의 범위 밖이다.

## 추가 근거
- `src/1__cfx/lib_cfx/lib_audio_in.h:112-113`: `LIB_ADC_FRACTIONAL_DELAY_6 = (6 << ...)`, `LIB_ADC_DEC_CTRL_VAL_0_0250 = ... | ADC_INTEGER_DELAY_0 | LIB_ADC_FRACTIONAL_DELAY_6 | ...` — INT=0, FRAC=6 명시
- `docs/tasks/signalProcessing/20260623_beam-forming/조사_데이터시트-그라운딩.md`: SFCR=29, 분모=8×30=240, 1스텝=1/240샘플, "DELAY_FRACTIONAL valid = 0~SFCR(=29)" (데이터시트 HW p.29693 인용)
- `src/1__cfx/systemControl/audioMixer.c:59-74`: `p_delay_buf0[i+1]` (i=0..14), `p_delay_buf1[0]` (i=15) — 블록 내 1샘플 인덱스 오프셋으로 SW 1샘플 지연 구현
- `src/1__cfx/systemControl/system_control.c:88,93`: `SYS_SET_ADC_DEC_CTRL(AUDIO, 2, LIB_ADC_DEC_CTRL_VAL_0_0250)` / `SYS_SET_ADC_DEC_CTRL(AUDIO, 0, LIB_ADC_DEC_CTRL_VAL_0_0250)` — 실제 mapChange에서 호출됨
- 산술: 6/240=0.025000샘플; 0.022/343×16000=1.026239샘플; 오차=0.001239샘플 < 1/240스텝(0.004167샘플)

