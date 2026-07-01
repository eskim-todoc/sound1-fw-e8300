---
name: 리팩토링 인벤토리 04
purpose: 네이밍·tdc_-컨벤션-분석가
type: tasks
maturity: experimental
tags: [beamforming, refactor, agent-log, analysis]
---

# [분석 04] 네이밍·tdc_ 컨벤션 분석가 (할루시네이션 0, 동작 보존 우선)

## 결론
6개 대상 파일을 Read 도구로 전량 확인 후 실재하는 file:line만 근거로 작성. 총 5개 항목 발견: ① enable_1_DMIC / enable_2_DMICs / get_enabled_DMIC_count — tdc_ 접두어 누락 신규 심볼, ② audio_mix_2_buffers_for_beamforming — tdc_ 접두어 누락, ③ tdc_copy_DMIC_buffers — 이미 tdc_ 적용 완료(정상), ④ g_enabled_mic_count — 실질적 데드 변수(코드 내 유일 실참조 2개가 주석 처리된 상태), ⑤ s_enabled_dmic_cnt vs g_enabled_mic_count — 같은 의미를 두 변수로 중복 관리하며 하나가 완전히 사장됨. 기존 심볼 일괄 rename은 범위 외.

## 인벤토리 항목
- **[naming/low]** `src/1__cfx/lib_cfx/lib_audio_in.h:130` (보존=True)
  - 현재: void enable_1_DMIC(void);
  - 제안: void tdc_enable_1_DMIC(void);  — 정의(lib_audio_in.c:173), 호출처(main.c:173, main.c:191, system_control.c:72, system_control.c:113) 전부 동시 rename
  - 동등성: 심볼 rename만이며 DIO 레지스터 조작 로직 무변경
- **[naming/low]** `src/1__cfx/lib_cfx/lib_audio_in.h:131` (보존=True)
  - 현재: void enable_2_DMICs(void);
  - 제안: void tdc_enable_2_DMICs(void);  — 정의(lib_audio_in.c:180), 호출처(main.c:97, main.c:191, system_control.c:76) 전부 동시 rename
  - 동등성: 심볼 rename만이며 DIO 레지스터 조작 로직 무변경
- **[naming/low]** `src/1__cfx/lib_cfx/lib_audio_in.h:129` (보존=True)
  - 현재: int  get_enabled_DMIC_count(void);
  - 제안: int tdc_get_enabled_DMIC_count(void);  — 정의(lib_audio_in.c:168), 호출처(main.c:171, main.c:189, main.c:440, main.c:445) 전부 동시 rename
  - 동등성: 심볼 rename만이며 s_enabled_dmic_cnt 반환 로직 무변경
- **[naming/low]** `src/1__cfx/systemControl/audioMixer.h:26` (보존=True)
  - 현재: void audio_mix_2_buffers_for_beamforming(int _XMEM *p_delay_buf0, int _XMEM *p_delay_buf1, int _XMEM *p_no_delay_buf0);
  - 제안: void tdc_audio_mix_beamforming(int _XMEM *p_delay_buf0, int _XMEM *p_delay_buf1, int _XMEM *p_no_delay_buf0);  — 정의(audioMixer.c:55), 호출처(main.c:454, main.c:461) 및 주석 내 비활성 호출(main.c:448, main.c:453, main.c:460) 전부 동시 rename. 함수명 단축도 함께 제안(
  - 동등성: 심볼 rename만이며 p_mix[i] 산출 로직(no_delay>>5 + delay_buf0[i+1]>>5)>>1 및 p_mix[15] = delay_buf1[0] 수식 무변경
- **[deadcode/low]** `src/1__cfx/systemControl/main.c:13` (보존=True)
  - 현재: volatile int _XMEM g_enabled_mic_count = 0;
  - 제안: 변수 삭제 또는 extern 선언 제거. 근거: ① 유일한 쓰기 참조 main.c:96 은 '// g_enabled_mic_count = 2;' 로 주석 처리됨. ② 유일한 읽기 참조 main.c:440·445 는 '/*g_enabled_mic_count == 1*/' '/*g_enabled_mic_count == 2*/' 로 주석 처리됨. ③ 실 실행 경로에서 읽히거나 쓰이는 위치가 0개. ④ 의미상으로는 s_enabled_
  - 동등성: 실 실행 경로에서 이 변수를 읽거나 쓰는 코드가 0개이므로 삭제해도 런타임 동작 불변
