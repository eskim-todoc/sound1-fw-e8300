---
name: 모듈화 분석 06
purpose: HW결합·CFX제약-분석가
type: tasks
maturity: experimental
tags: [modularize, agent-log, analysis]
---

# [분석 06] HW결합·CFX제약 분석가

## 결론
대상 6개 파일에서 HEAR_ADDR_AUDIO_MIX·HCT_A0_x FIFO·SYS_SET_ADC_DEC_CTRL·chess_loop_range·_XMEM 전역에 결합된 14개 심볼/함수 군을 식별함. audioMixer.c 전 함수, tdc_copy_DMIC_buffers, lib_init_audio_in_path_common, lib_i2s_copy_data_from_fifo, Normal_PowerMode_event_mapChange 내 ADC_DEC_CTRL 기록 구간은 HW·CFX 아키텍처 직접 결합이므로 통합/모듈 수준 온타깃 검증 대상. 순수 유닛으로 분리 가능한 경계 5개를 제안함: DAS 산술 커널, Q24 곱/클램프 커널, front-mic 상태 게터/세터, I2S 스트리밍 상태 조회, DMIC 활성 수 조회.

## 유닛
- **das_beamform_kernel** [unit-test] pure=True @ `audioMixer.c:113-128 (추출 전: audio_mix_2_buffers_for_beamforming 내 산술 블록)`
  - delay-and-sum 산술만 수행. p_no_delay[i]·p_delay0[i+1](블록 내)·p_delay1[0](블록 경계 i=15) 각각 >>AUDIO_INPUT_RSHIFT 후 합산, >>AUDIO_MIX_NORMALIZE_RSHIFT(=1). HEAR_ADDR_AUDIO_MIX 미참조 — 출력 포인터를 인수로 받으면 순수. [현재 audioM
  - dep: []
- **q24_fade_kernel** [unit-test] pure=True @ `lib_i2s.c:351-362 (fade-in), lib_i2s.c:403-414 (fade-out)`
  - 16샘플 배열에 계수(coeff[16])를 곱하고 Q24 범위로 클램프. `t = ((long)src[15-i] * coeff[i]) >> 16; clamp(Q24_MAX, Q24_MIN)`. HW·전역 미참조, 포인터+계수만 받으면 순수. [lib_i2s.c:351-362, 403-414에서 추출 제안]
  - dep: []
- **front_mic_state** [logic-explanation] pure=False @ `lib_audio_in.c:159-167`
  - lib_audio_set_front_mic(int type) / lib_audio_get_front_mic(void) — 전역 s_lib_audio_front_mic 읽기/쓰기만. HW 레지스터 없음. LIB_AUDIO_FRONT_MIC_NONE/LEFT/RIGHT 세 값 분기 로직.
  - dep: []
- **dmic_count_query** [logic-explanation] pure=False @ `lib_audio_in.c:169-172`
  - get_enabled_DMIC_count() — 전역 s_enabled_dmic_cnt 반환. HW 없음. 단순 getter. enable_1/2_DMIC()는 DIO 레지스터 기록 포함이므로 별도 HW 결합 유닛.
  - dep: []
- **i2s_streaming_state_query** [logic-explanation] pure=False @ `lib_i2s.c:174-182`
  - I2S_isStreaming() — `return lib_g_i2s_state == LIB_I2S_STATE_ENABLED`. tdc_i2s_set_streaming_state(int state) — 전역 lib_g_i2s_state 쓰기. HW 없음.
  - dep: []
- **hw_audio_path_init** [integration-test] pure=False @ `lib_audio_in.c:21-157`
  - lib_init_audio_in_path_common(): SYS_SET_ADC_SAMPLE_FREQ_CFG·SYS_SET_ADC_CTRL(×4)·SYS_SET_OUTPUT_CTRL·SYS_IOC_INPUTCONFIG·SYS_IOC_PCMINPUTCONFIG·SYS_IOC_OUTPUTCONFIG·HCT_FIFO_Configure·SYS_FIFO_AUTOMU
  - dep: []
- **hw_dmic_clock_enable** [integration-test] pure=False @ `lib_audio_in.c:174-216`
  - enable_1_DMIC()/enable_2_DMICs(): s_enabled_dmic_cnt 설정 + Sys_DIO_Config(DIO22/DIO10) DIO_MODE_ADCCLK 기록. disable_DMIC(): 동일 핀 DIO_MODE_DISABLE. DIO 레지스터 직접 접근 — 통합 검증 대상.
  - dep: []
- **hw_fifo_clear** [integration-test] pure=False @ `lib_audio_in.c:218-249`
  - clear_FIFO(volatile int _XMEM *p_fifo, int len): chess_loop_range(1, HEAR_LARGEST_FIFO_SIZE) 루프로 XMEM 직접 0 기록. clear_FIFO_all(): HCT_A0_0~6·HCT_A1_0 전체 클리어. disable_FIFO_all(): Sys_FIFO_Configure 8회 호
  - dep: []
- **hw_decimation_delay_set** [integration-test] pure=False @ `src/1__cfx/systemControl/system_control.c:85-103`
  - Normal_PowerMode_event_mapChange() 내 Left_Ear/Right_Ear 분기(system_control.c:85-103): SYS_SET_ADC_DEC_CTRL(AUDIO, 0, ...) · SYS_SET_ADC_DEC_CTRL(AUDIO, 2, ...) 로 ch0(EZ)·ch2(QCC) decimation filter 소수점 
  - dep: ['front_mic_state']
- **tdc_copy_DMIC_buffers** [integration-test] pure=False @ `src/1__cfx/systemControl/main.c:326-353`
  - 매 블록마다 HCT_A0_0(FIFO MIC0=DMIC1/EZ)·HCT_A0_1(FIFO MIC1=DMIC2/QCC) 직접 읽어 g_lib_dmic_in_buffers[mic][block][i] 링버퍼로 shift-복사. HCT_A0_x FIFO 직접 접근 + _XMEM 전역 쓰기 + chess_loop_range CFX 전용 — 통합 검증 대상. shif
  - dep: ['hw_fifo_clear']
- **audio_mix_to_hear_mem** [integration-test] pure=False @ `src/1__cfx/systemControl/audioMixer.c:7-129`
  - audioMixer.c 5개 함수 전체(audio_mix_internal_mic_only·audio_mix_external_mic_only·audio_mix_1_buffer·audio_mix_2_buffers·audio_mix_2_buffers_for_beamforming): 출력 목적지가 `(int _XMEM *) HEAR_ADDR_AUDIO_MIX` 고
  - dep: ['das_beamform_kernel']
- **hw_i2s_init** [integration-test] pure=False @ `lib_i2s.c:86-161`
  - lib_init_I2S(): SYS_IOC_INPUTCONFIG·OUTPUTCONFIG·PCMINPUTCONFIG(LIB_I2S_IOC)·LIB_I2S->CTRL·LIB_I2S->STATUS 직접 기록. LIB_I2S_DIOConfig(): DIO->SRC_PCM[diff] 기록, SYS_DIO_CONFIG 호출. Sys_PCM_Config 호출. lib_
  - dep: []
- **i2s_fifo_capture** [integration-test] pure=False @ `lib_i2s.c:263-312`
  - lib_i2s_copy_data_from_fifo(int _XMEM *p_fifo): 인터럽트 비활성화 후 lib_g_i2s_interrupt_flag 체크, HCT_A0_5(호출부 main.c:145·359·414에서 바인딩)에서 lib_g_i2s_buffers[in_pos]로 16샘플 복사, in_pos 갱신, buffer_copy_cnt 증가, buf
  - dep: ['i2s_streaming_state_query']
- **normal_loop_i2s_dmic_dispatch** [integration-test] pure=False @ `src/1__cfx/systemControl/main.c:126-238`
  - normal_loop() 메인 루프(main.c:126-238): Sys_GPIO_Read(I2S_FLAG_DIO_NUM) HW GPIO 직접 읽기, lib_g_i2s_interrupt_flag·g_interrupt_flags·Addr_SharedMem 전역 복합 참조, DMIC 수 분기(enable_1/2_DMIC), PCM_LiveStimulation_
  - dep: ['tdc_copy_DMIC_buffers', 'audio_mix_to_hear_mem', 'i2s_fifo_capture', 'i2s_streaming_state_query', 'dmic_count_query']

## 모듈
- **DMIC_capture_module**: units=['hw_audio_path_init', 'hw_dmic_clock_enable', 'hw_fifo_clear', 'tdc_copy_DMIC_buffers', 'front_mic_state', 'dmic_count_query'] dep=[]
- **beamforming_module**: units=['das_beamform_kernel', 'audio_mix_to_hear_mem', 'hw_decimation_delay_set'] dep=['DMIC_capture_module']
- **i2s_module**: units=['hw_i2s_init', 'i2s_fifo_capture', 'i2s_streaming_state_query', 'q24_fade_kernel'] dep=[]
- **audio_dispatch_module**: units=['normal_loop_i2s_dmic_dispatch'] dep=['DMIC_capture_module', 'beamforming_module', 'i2s_module']

## 노트
- HEAR_ADDR_AUDIO_MIX 결합: audioMixer.c L9·21·34·45·111 — 5개 함수 모두 `(int _XMEM *) HEAR_ADDR_AUDIO_MIX` 직접 캐스팅. 출력 목적지가 HEAR HW 고정 주소이므로 이 파일 전체는 순수 유닛 불가. 분리 방법: 믹서 함수 시그니처에 출력 포인터 인수 추가 후 호출부에서 HEAR_ADDR_AUDIO_MIX 바인딩 → das_beamform_kernel 순수 추출 가능.
- HCT_A0_x FIFO 결합: MIC0=HCT_A0_0(main.c:338, audioMixer.c:10, main.c:377·405), MIC1=HCT_A0_1(main.c:351), I2S_in=HCT_A0_5(main.c:145·359·414, lib_i2s.c:263), DAC1=HCT_A0_3(main.c:378·406·498), 나머지 HCT_A0_2/4/6·HCT_A1_0(lib_audio_in.c:229-248). 이 심볼들이 나타나는 함수는 모두 통합/모듈 수준 검증 대상.
- SYS_SET_ADC_DEC_CTRL 결합: system_control.c:88-103 — Left 귀=ch2(QCC)에 FRAC=6(LIB_ADC_DEC_CTRL_VAL_0_0250), Right 귀=ch0(EZ)에 FRAC=6 설정. 빔포밍 HW 소수점 지연(0.025샘플)의 귀 방향 분기 전체가 HW 레지스터 직접 기록이므로 통합 검증 필수. lib_audio_in.c:142-143의 configure_audio_path_all() 내 초기값(SYS_SET_ADC_DEC_CTRL(0, LIB_ADC_DEC_CTRL_VAL) / (2, ...))도 동일.
- chess_loop_range CFX 전용: audioMixer.c:13·25·37·47(beamforming 전개된 lines), main.c:330·333·343·346, lib_audio_in.c:221, lib_i2s.c:289·348·399 — CFX(Tensilica) 컴파일러 전용 루프 힌트. 비CFX 환경(호스트 단위 테스트)에서 빌드하려면 #define chess_loop_range(a,b) /* noop */ 처리 필요. 순수 유닛 추출 시 이 매크로도 추상화 경계에 포함시킬 것.
- g_lib_dmic_in_buffers _XMEM 전역: lib_audio_in.c:19 선언(extern), main.c:335·338·348·351 기록, main.c:428·447·457·462·467 읽기. _XMEM=CFX 전용 메모리 공간. 비CFX 빌드 시 #define _XMEM /* empty */ 처리로 호스트 메모리 대체 가능 — das_beamform_kernel 추출 시 포인터 인수화로 해결.
- 순수 유닛 경계 제안 — das_beamform_kernel: `void das_beamform_kernel(int *p_no, int *p_del0, int *p_del1, int *p_out)` 형태. p_out[0..14] = (p_no[i]>>RSHIFT + p_del0[i+1]>>RSHIFT)>>1, p_out[15] = (p_no[15]>>RSHIFT + p_del1[0]>>RSHIFT)>>1. 계수 AUDIO_INPUT_RSHIFT·AUDIO_MIX_NORMALIZE_RSHIFT 상수만 의존, HW 없음. 테스트 벡터: 정수 배열 주입 → 출력 배열 기댓값 검증.
- 순수 유닛 경계 제안 — q24_fade_kernel: `void q24_fade_kernel(int *p_src, const int *p_coeff, int len)` — `t=(long)p_src[len-1-i]*p_coeff[i]>>16; clamp; p_src[len-1-i]=(int)t`. lib_i2s.c:351-362와 403-414 중복 로직 단일화. 테스트 벡터: Q24_MAX 경계값·정수 0·음수 입력으로 클램프 동작 검증.
