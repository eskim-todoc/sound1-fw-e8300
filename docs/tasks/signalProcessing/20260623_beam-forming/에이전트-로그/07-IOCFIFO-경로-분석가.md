---
name: 분석 페르소나 07
purpose: IOC/FIFO 경로 분석가 — E8300 입력 패스 채널 타이밍 전문 분석 결과
type: tasks
maturity: experimental
tags: [beamforming, agent-log, analysis]
---

# [단계1·분석 07] IOC/FIFO 경로 분석가 — E8300 입력 패스 채널 타이밍 전문

**신뢰도**: high

## 결론
IN1(DMIC1/ch1)→FA0_0, IN2(DMIC2/ch2)→FA0_1 경로는 IOC single-source 구성이며 double-access가 정상 적용돼 있다. 단, decimation 시분할 구조상 FA0_0(ch1)이 FA0_1(ch2)보다 1/8 샘플 늦게 FIFO에 쓰이는 고유 비대칭 스큐가 존재한다. FIFO 인터럽트는 채널별 독립(CFX INT 0 vs INT 1)으로 각 FIFO가 block_size(16샘플) 충전 시 개별 발화하므로, 동일 스큐가 인터럽트 타이밍 차이로도 발현된다.

## 발견(근거)
- **IOC 매핑: IN1→FA0_0(MIC0), IN2→FA0_1(MIC1) — 각각 single-source 1:1 구성**
  - 근거: lib_audio_in.h:50: `LIB_IOC_ADC_CFG_VAL = IOC_INPUT_CFG_IN1_FA0_0 | IOC_INPUT_CFG_IN2_FA0_1`. IN0/IN3은 NONE. 각 FIFO에 단일 소스만 연결됨. (lib_audio_in.h 라인 50 / HW §14 IOC 설명)
- **FA0_0, FA0_1 모두 double-access 활성화 — single-source이므로 정상(필수 조건 충족)**
  - 근거: lib_audio_in.h:60: `LIB_IOC_FIFO_ACCESS_VALUE = FIFO_A0_0_DBL_ACC_EN | FIFO_A0_1_DBL_ACC_EN | ...`. 코드 주석: '단일 소스/목적지면 double-access 권장, 복수 소스면 반드시 비활성화'. FA0_0/FA0_1은 각각 IN1/IN2 단일 소스이므로 double-access 활성화가 올바른 설정. (lib_audio_in.h 라인 55-60)
- **FIFO 인터럽트: FA0_0→CFX INT 0, FA0_1→CFX INT 1 — 독립 발화, 동시 보장 없음**
  - 근거: lib_audio_in.c:49-50: `SYS_FIFO_CFXINTCONFIG(0, FIFO_INT_A0_0)` (MIC0), `SYS_FIFO_CFXINTCONFIG(1, FIFO_INT_A0_1)` (MIC1). 각 FIFO의 block_size=16샘플(microcode.h:87,93: HCT_FIFO_A0_0_BLOCK_SIZE=16, HCT_FIFO_A0_1_BLOCK_SIZE=16). 각각 독립적으로 16샘플 충전 시 발화. (lib_audio_in.c 라인 49-50 / microcode.h 라인 87, 93)
- **채널 간 1/8 샘플 고유 스큐: ch2(FA0_1=DMIC2/Left)가 ch1(FA0_0=DMIC1/Right)보다 1/8 샘플 먼저 FIFO에 도착**
  - 근거: HW p.222(사실A): 'ADC0·ADC2 데이터가 ADC1·ADC3보다 1/8 sample period ahead'. HW p.445(사실B): 'time multiplexed into two pairs (channels 0,1; channels 2,3) resulting in longer processing delays for channels 1,3'. 현재 매핑: ch2=DMIC2(Left)→짝수 쌍(먼저), ch1=DMIC1(Right)→홀수 쌍(늦게). (HW p.222, p.445 §14.4)
- **스큐의 FIFO 인터럽트 타이밍 영향: FA0_1 인터럽트(MIC1/Left)가 FA0_0 인터럽트(MIC0/Right)보다 1/8 샘플(≈7.8µs) 빠르게 발화**
  - 근거: FIFO 인터럽트는 각 채널이 block_size만큼 채워질 때 발화. ch2(FA0_1)가 1/8 샘플 먼저 도착하므로 FA0_1의 block_size(16) 충전이 FA0_0보다 1/8 샘플 먼저 완료됨. 1/8샘플@16kHz = (1/16000)/8 = 7.8125µs. 두 인터럽트 사이에 약 7.8µs 간격 발생. 추가로 block_size=16샘플로 동일하므로 long-term 레이트는 같고 위상만 7.8µs 어긋남. (microcode.h 라인 87, 93 / HW p.222, p.445)
- **FIFO 단에서 추가적인 채널 간 정렬 기구는 없음 — 스큐는 FIFO를 통과해 CFX DSP에 그대로 전달됨**
  - 근거: FA0_0, FA0_1은 독립 FIFO(메모리 오프셋 0/32, 각 32깊이). FIFO 자체는 버퍼 역할만 하며 채널 간 타임스탬프 비교나 정렬 로직 없음. HW 문서에 FIFO-level cross-channel sync 기능 언급 없음. 1/8 샘플 스큐는 CFX까지 보존됨. (microcode.h 라인 83-93 / lib_audio_in.c 라인 186-188)
- **현재 펌웨어의 DEC delay=0(양 채널 공통)이므로 HW 스큐 보정 미적용 — 스큐가 그대로 노출된 상태**
  - 근거: lib_audio_in.h:96-98: `LIB_SAMPLE_FRACTIONAL_DELAY 0`, `ADC_INTEGER_DELAY_0`으로 `LIB_ADC_DEC_CTRL_VAL` 구성. 1개-DMIC 활성 기준 ch1에만 적용(lib_audio_in.c:111). 2-DMIC 시 ch1/ch2 모두 동일 매크로 적용(라인 128-129)으로 상대 보정 없음. (lib_audio_in.h 라인 96-98 / lib_audio_in.c 라인 111, 128-129)
- **DMIC2(FE)는 DMIC1(RE) 대비 클럭 반주기 추가 스큐를 가짐 — 단 오버샘플링 클럭 기준이므로 출력 샘플 대비 크기는 미검증**
  - 근거: HW §18.2 p.563(사실E): 'DMIC*_DATA_RE=rising edge, _FE=falling edge 캡처'. 공유 클럭(DIO22) 반주기=1/(2×3.84MHz)≈130ns. 16kHz 샘플주기(62.5µs) 대비 0.002샘플 미만. 단 CIC pre-decimation 내부 처리에서 이 반주기가 증폭되는지 여부는 데이터시트에 명시 없음. (HW §18.2 p.563 / lib_audio_in.h 라인 80)
- **double-access 활성화가 FA0_4(PCM out)에는 적용되지 않음 — 단 MIC 경로와 무관**
  - 근거: lib_audio_in.h:60: `LIB_IOC_FIFO_ACCESS_VALUE`에 `FIFO_A0_4_DBL_ACC_EN` 없음. 코드 주석 '(double-access mode not enabled for PCM out)'. FA0_0/FA0_1에는 정상 적용됨. (lib_audio_in.h 라인 55-60)

## 미해결 질문
- DMIC2 FE 캡처의 반주기 스큐(≈130ns)가 CIC/decimation 내부 처리를 거치면서 얼마나 증폭되는지 — HW §18.2가 명시적으로 정량화하지 않음. RE/FE를 동일하게 통일해야 하는지 판단 불가.
- FA0_0/FA0_1 각 FIFO 인터럽트가 CFX DSP 내부에서 어떻게 소비되는지 — 두 인터럽트가 별도 핸들러인지 동일 핸들러인지, 그리고 DSP가 두 블록을 같은 프레임으로 묶어 처리하는지 소스에서 확인되지 않음 (lib_cfx/signalProcessing 내 호출 경로 미확인).
- decimation 절대 group delay가 ch1과 ch2에서 동일한지 — 같은 BAND_SELECT(0K_8K), 같은 SFCR이면 상쇄되어 상대 지연만 남을 것으로 추정되나, 데이터시트에서 ch1/ch2 절대 group delay 수치 명시 페이지 미확인.
- 2-DMIC 활성화 시 QCC와 DMIC2 공유 시 10ms 대기 중 FA0_1 FIFO 상태 — 버퍼 오버플로 또는 stale 데이터 잔류 가능성.
