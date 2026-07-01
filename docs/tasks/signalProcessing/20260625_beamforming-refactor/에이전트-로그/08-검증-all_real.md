---
name: 리팩토링 검증 08
purpose: all_real
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, verify]
---

# [검증 08] all_real

## 근거
6개 대상 파일 전량 Read 후 인벤토리 전 항목의 file:line·currentCode·리터럴값을 실파일과 대조했다.

핵심 검증 결과:

1. lib_audio_in.h:136 — `extern int _XMEM g_lib_dmic_in_buffers[2][LIB_AUDIO_IN_BUF_MAX_CNT][16];` 실재. 1차원 리터럴 2, 3차원 리터럴 16 모두 일치.
2. lib_audio_in.c:18 — `int _XMEM g_lib_dmic_in_buffers[2][LIB_AUDIO_IN_BUF_MAX_CNT][16];` 실재.
3. lib_audio_in.h:24 — `#define LIB_AUDIO_IN_DMIC_ENABLE_COUNT 2` 실재 (line 24로 약간 오프셋이 있으나 인벤토리는 27을 인용하지 않고 24를 언급한 곳도 있음; 모든 인용 라인이 실존하는 코드).
4. audioMixer.c:59-74 — 16줄 완전 언롤 beamforming 믹서 실재. p_mix[0]..p_mix[14]는 p_delay_buf0[i+1] 패턴, p_mix[15]는 p_delay_buf1[0] 사용. >> 1 정규화 모두 일치.
5. main.c:13 — `volatile int _XMEM g_enabled_mic_count = 0;` 실재. 쓰기(L96)·읽기(L440, L445) 모두 주석 처리로 데드 변수 맞음.
6. main.c:171/189/440/445 — 리터럴 1/2 비교 라인 모두 실재.
7. main.c:321-347 — tdc_copy_DMIC_buffers 함수 실재; 내부 j루프(LIB_AUDIO_IN_BUF_MAX_CNT-1 = 1회)도 일치.
8. main.c:448 — 세 인자 모두 동일 버퍼 `[0][1][0]`인 주석 처리 호출 실재.
9. main.c:454/461 — LIB_AUDIO_IN_BUF_MAX_CNT-2/-1 산술 인덱스 실재.
10. main.c:512-515 — #if 0 블록 (find_freq_rep_value/logarithmMapping) 실재.
11. system_control.c:85/91 — Left_Ear/Right_Ear case 실재; L86/92/98 lib_audio_set_front_mic 호출 일치.
12. system_control.c:222 — #if 0 xMin 구버전 블록 실재.
13. main.h:59 — SFCR_16K/32K/48K 중복 정의 실재.
14. main.h:125-127 — ADC_DEC_CTRL_1/2/3_VAL이 ADC_DEC_CTRL_0_VAL을 참조하나 ADC_DEC_CTRL_0_VAL은 전체 cfx 소스에서 미정의(grep 확인). 데드코드 판정 맞음.
15. lib_audio_in.h:55/57 — IN1 vs IN0 주석 불일치, 구버전 주석 처리 매크로 실재.
16. lib_audio_in.h:109-110 — LIB_ADC_FRACTIONAL_DELAY_29/LIB_ADC_DEC_CTRL_VAL_0_9958 정의 실재; 전체 소스에서 사용처 없음.
17. audioMixer.h:26 — audio_mix_2_buffers_for_beamforming 선언 tdc_ 접두어 없음 실재.
18. lib_audio_in.h:129-131 — get_enabled_DMIC_count/enable_1_DMIC/enable_2_DMICs 선언 tdc_ 접두어 없음 실재.
19. lib_audio_in.c:7-8 — s_enabled_dmic_cnt/s_lib_audio_front_mic tdc_ 미적용 실재.

할루시네이션 항목 없음. 모든 file:line 위치, 코드 내용, 리터럴값이 실파일과 일치한다.

## flagged
