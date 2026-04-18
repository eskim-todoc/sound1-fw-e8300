# [분석] CFX 프로젝트 구조 및 시퀀스 Rev.0

> 대상 : `src/1__cfx/` 트리 전부  
> 목적 : CFX 코어의 전반적인 구조, CM3와의 공유 메모리 핸드셰이크, 그리고 오디오 입력 → 신호처리 → PCM 자극 출력까지의 실행 시퀀스를 빠짐없이 정리한다.  
> 작성 관점 : 레거시 E7150/BTE 시리즈 코드를 E8300 / OTE 1.5 Gen 플랫폼으로 옮기면서 "지금은 안 쓰지만 지우지 않은" 필드·함수·#if 블록이 다수 남아 있고, 이로 인해 시퀀스가 뒤엉켜 있다. 가능한 범위에서 [첨언]으로 수정 제안도 함께 기록한다.

---

## 0. 요약 (한 장으로 본 구조)

CFX는 **Onsemi E8300 플랫폼의 오디오/신호처리 전용 코어**이며, Cortex-M3 (CM3) 코어와 **PRAM5의 공유 메모리** 한 장을 통해서만 상호작용한다. CFX는 자체 RAM과 HEAR 하드웨어 가속기(FFT, vMag, abs/max)를 갖고 있고, 마이크·I2S 입력 → AGC → FFT → 주파수 그룹 대표값 → 로그 매핑 → CIS/nOFm 자극 스트리밍 → PCM 출력이라는 파이프라인을 **1 msec 주기의 PCM FRAME / FIFO interrupt** 위에서 돌린다.

```
                        ┌──────────────────── CM3 (Cortex-M3) ────────────────────┐
                        │ 파일시스템, EEPROM, UI, 볼륨/맵 계산, backtel 해석, LED, BLE │
                        └──────────────────────────┬──────────────────────────────┘
                                                   │
                                   [ShareMem : 0x28000~ in CFX / PRAM5 base in CM3]
                                                   │
 ┌───── CFX ────────────────────────────────────────┴────────────────────────────────┐
 │   main() → while(1) {                                                              │
 │       Addr_SharedMem->is_CFX_started = 1;                                          │
 │       if (is_enabled_CFX_iteration == 1) { normal(); standby(); }                  │
 │   }                                                                                │
 │                                                                                    │
 │   [DMIC0/1, ADC] → FIFO_A0_0 / A0_1 ──┐                                             │
 │   [I2S in]       → FIFO_A0_5  ────────┼→ audio_mix_* → HEAR_AUDIO_MIX              │
 │                                        │                                            │
 │   HEAR FC0 : abs + max    →  AGC gain (attack/release, attenuation/rotation/amplify)│
 │   → m_agc_output_buffer → FIFO_A1_0 (FFT in)                                        │
 │   HEAR FC1 : Win-DFT + vMag →  HEAR_ADDR_VMAG_OUTPUT                                │
 │   → find_freq_rep_value (pass-bin → max amplitude per 주파수 그룹)                  │
 │   → logarithmMapping (A·x^p + B, C/T clamp, mute)                                   │
 │   → stimulationStrategy (CIS 또는 nOFm Phase0/1) → FIFO_A0_4 (PCM out)              │
 │                                                                                    │
 │   check_link_connection_state : 300 ms 주기 백텔(ISD 연결 확인) 4스텝                │
 │                                                                                    │
 └────────────────────────────────────────────────────────────────────────────────────┘
```

본문은 위 흐름을 **① 파일 레이아웃, ② 부팅·상태 머신, ③ 공유 메모리 카탈로그, ④ CM3↔CFX 핸드셰이크 시퀀스, ⑤ 신호처리 파이프라인 상세, ⑥ PCM 출력 모드 상세, ⑦ 인터럽트/ISR 지도, ⑧ 레거시 마이그레이션에서 꼬인 부분 및 수정 제안** 순서로 쪼개어 기술한다.

---

## 1. 파일 레이아웃

```
1__cfx/
├── app_LCF.bcf                   CFX 링커 구성 (메모리 맵)
├── .cproject / .project          Eclipse
├── asm/
│   ├── asm_startup.S             리셋 → main 호출
│   └── asm_interrupt.S           IVT (HEAR, FIFO, CM3 인터럽트 벡터)
├── OTE_1_5_gen/
│   ├── OTE_1_5_gen_FS_MEM.h      CFX가 보는 FS 메모리 주소 매크로
│   └── OTE_1_5_gen_UART.h        FS_MEM_UART 디버그 버퍼 (state + flag[32] + buffer[512])
├── environment/
│   ├── custom_types.h            xmem_int/iomem_int 타입
│   └── shared_memory.h           ★ CM3-CFX 공유 메모리 전체 정의 (Addr_SharedMem)
├── systemControl/
│   ├── main.c / main.h           main, normal_init, normal_loop, standby, I2S_handle, HEAR/PCM LiveStimulation 핸들러
│   ├── system_control.c / .h     LB_Normal_PowerMode, 맵 변경·볼륨 변경 이벤트 처리, 맵 데이터 복사
│   ├── interrupt_service_routine.c / .h    g_interrupt_flags, FIFO/HEAR/CM3 ISR
│   ├── audioMixer.c / .h         audio_mix_* (HEAR_ADDR_AUDIO_MIX 갱신)
├── signalProcessing/
│   ├── definitionsForAlgorithm.h 상수·선택 플래그 (AGC 계수 종류, ISD 체크 방식, FFT size, 채널 수 등)
│   ├── agc.c / .h                AGC 계수 테이블, audio_agc(), attack/release
│   ├── FrequencyAnalysis.c / .h  update_FFT_inputData, find_freq_rep_value, read_FFT_PassBin_index
│   ├── nonlinearMapping.c / .h   logarithmMapping, calculate_x_power_p, A/B 계수 계산
│   └── stimulationStrategy.c / .h CIS / nOFm 자극 스트리밍 (PCM FIFO 쓰기)
├── internalDevice/
│   ├── driver_PCM.c / .h         pcmDataOut, 6개 PCM 모드(fn_PcmBitStream_Mode_*), fn_reset_PCM
│   └── driver_PCM_liveStimulation.c / .h    check_link_connection_state, Enable/Disable_backtelCircuit, frame_N_perChannel
├── lib_cfx/
│   ├── lib_PCM.c / .h            PCM0(NPP) 마스터 초기화
│   ├── lib_audio_in.c / .h       FIFO/ADC/DMIC 설정, configure_audio_path_all
│   ├── lib_audio_mixer.c / .h    lib_audio_loopback, lib_loopback_AGC_out (DAC 출력용)
│   ├── lib_i2s.c / .h            PCM1(I2S 슬레이브), 4단계 fade-in/out, 버퍼 링 관리
│   └── lib_i2s_bridge.c / .h     build_bridge_C1_Q816_cfx (Hermite C1 브릿지, 현재 main.c에서 호출 안 됨)
└── hear/
    ├── microcode.h / .c          HEAR Configuration Tool 생성물 (FIFO·FC 주소·엔트리)
```

---

## 2. 부팅 / 상태 머신

### 2.1 최상위 루프

`systemControl/main.c:13` — `main()` 는 다음과 같이 **두 단계 핸드셰이크 후에야** 신호처리 루프로 들어간다.

```
main()
├─ SYSTEM priorities / MEM_ARBITER 설정
├─ SYS_CFX_INIT_LOOP_STACK
├─ set_saturation_mode(1)
├─ app_configure_interrupts(CFX_INT_NORMAL)
├─ g_standby_checker = 0
└─ while (1) {
       Addr_SharedMem->is_CFX_started = 1;         ← (A) "CFX는 살아있다"
       if (is_enabled_CFX_iteration == 1) {        ← (B) CM3 준비 완료 신호
           normal();    // normal_init() + normal_loop()
           standby();
       }
       SYS_CFX_NOP();
   }
```

핸드셰이크 의미 :

- **(A) `is_CFX_started`** : CM3는 이 값을 보고 "CFX 코어가 main에 도달했다"를 판단. 이 이후에야 CM3는 자신의 초기화(파일시스템, EEPROM, LED 등)를 진행한다.
- **(B) `is_enabled_CFX_iteration`** : CM3 초기화가 끝나면 CM3가 이 값을 1로 쓴다. CFX는 그 전까지 인터럽트 구성만 한 채 idle loop를 돌며 NOP.

[첨언 ①] 두 플래그 모두 공유 메모리 루트(`ST__CFX_CM3_SharedMemory_ALL`)의 **bare int**로 선언되어 있어 순서 보장(memory barrier)은 하드웨어의 단일 접근성에만 의존한다. 보통 이런 "런/홀드" 신호는 `volatile` + single-writer 규칙만 지키면 큰 문제는 없지만, 구조체 다른 필드 변경과 함께 복합적으로 해석되면 race가 생길 수 있다. 현재는 "CFX가 쓰는 필드 / CM3가 쓰는 필드"가 비교적 잘 분리되어 있어 실제 문제는 보이지 않는다.

### 2.2 normal → standby 전이

`normal_loop()` 의 while 조건은 `systemControl/main.c:128`:

```c
while (Addr_SharedMem->systemShare.enter_ULP_mode_Command_CM3_to_CFX != 1) { ... }
```

- CM3가 ULP(Ultra Low Power) 진입을 결정하면 이 값을 1로 쓰고, CFX는 한 iteration 끝에 루프를 탈출한다.
- `standby()` 는 ADC/DMIC/DAC/PCM/I2S/IOC/FIFO/HEAR 모두를 OFF 시키고 `CFX_INT_STANDBY` 인터럽트 세트(WATCHDOG + CM3_1 = wake up 만)로 바꾼 뒤 `g_standby_checker = 1`. 그리고 내부 while 에서 `g_interrupt_flags.wake_up` 을 대기.
- wake up 이 들어오면 `break` → 바깥 `while(1)` 로 복귀 → `normal()` 재진입.

[첨언 ②] `g_standby_checker` 는 `main.c:39` 에서 초기화 후 `standby()` 진입 시 1로만 쓰일 뿐, CFX 내부 어디서도 읽히지 않는다. CM3가 공유 메모리로 이 값을 받지도 않는다(해당 변수는 XMEM 로컬). → **dead variable 후보**. 디버거에서 확인용이었을 가능성이 크므로 **제거하거나, 공유 메모리의 `systemShare` 에 실제 Standby 상태 필드로 옮기는 것이 올바르다.**

### 2.3 normal_init 이 만드는 하드웨어 상태

```
SYS_HEAR_PAUSE + RESET
→ app_configure_interrupts(CFX_INT_NORMAL)
→ HEAR FC enable + priority 설정
→ SYS_HEAR_START
→ configure_audio_path_all   : FIFO 초기화/클리어, ADC/DMIC/DAC/PCM/IOC route
→ lib_init_PCM(DIO34, DIO5, DIO6)   : PCM0 master (NPP/FPGA 쪽)
→ lib_init_I2S(DIO25, DIO19, DIO14, DIO29)  : PCM1 slave (QCC 오디오 인입)
→ enable_DMIC
→ lib_enable_I2S
→ lib_enable_PCM
→ DAC GF x2 / OUTPUT CTRL OD1 enable (OD0 disable)
→ fn_reset_PCM()              : preamble state = FILL_ZERO
→ is_pcm_specific_command_reading / pcm_specific_command_read_index = 0
```

이때 인터럽트 활성화 대상은 다음과 같이 두 벌을 유지한다(`interrupt_service_routine.c:34`).

| 모드 | EBL 설정 | 의미 |
|---|---|---|
| `CFX_INT_NORMAL` | EBL_0 : WATCHDOG / EBL_7 : FIFO_0~6 / EBL_8 : HEAR_0,1 / EBL_10 : CM3_0 | 신호처리 풀 가동 |
| `CFX_INT_STANDBY` | EBL_0 : WATCHDOG / EBL_10 : CM3_1 | wake-up 만 |

[첨언 ③] `app_configure_interrupts(CFX_INT_NORMAL)` 이 **main 에서 한 번**, **normal_init 에서 또 한 번** 호출된다. 후자가 덮어써서 기능상 동일하지만, 의미적으로는 main 의 첫 호출이 불필요하다. `is_enabled_CFX_iteration` 을 폴링하는 동안 인터럽트를 받을 이유가 없으므로 **main 시작부에서는 필요 최소 인터럽트만 설정하거나, 아예 disable 상태로 두고 normal_init 단일 진입점에서만 enable 하는 것이 안전**하다.

---

## 3. 공유 메모리 카탈로그 (`environment/shared_memory.h`)

루트 구조체는 `ST__CFX_CM3_SharedMemory_ALL` 이고, CFX 측에서는 주소 고정으로 매크로가 해결된다.

```c
#define Addr_SharedMem \
    ((volatile ST__CFX_CM3_SharedMemory_ALL chess_storage(IOMEM) *) 0x28000)
```

주요 필드를 "누가 쓰는가 / 누가 읽는가" 관점으로 정리하면 다음과 같다.

### 3.1 상태 / 런 플래그

| 필드 | Writer | Reader | 설명 |
|---|---|---|---|
| `is_CFX_started` | CFX | CM3 | CFX가 main 에 진입했음을 알림. |
| `is_enabled_CFX_iteration` | CM3 | CFX | 1이 되어야 normal 루프 진입. |
| `CFX_EEPROM_data_is_Loaded` | CM3 | CFX(대기성) | EEPROM 로드 여부 (사용 지점은 현재 파일에서 직접 참조 없음 → CM3 측 통지). |
| `CFX_ErrorCode` | CFX | CM3 | CFX 오류 코드(현재 CFX 코드에서 명시적 set 지점은 없음, 필드만 존재). |
| `CM3_status` | CM3 | CFX | CM3 상태 코드(직접 참조 미확인). |
| `CM3_tempValue1/2` | 양쪽 | 양쪽 | 디버깅 전용 임시 값. `#if 0` 된 AGC 디버그에서 사용. |
| `systemShare.system_opMode` | – | – | **미사용 (주석으로 "지울 것, 지우면 공유메모리 주소도 변경해야 함")** |
| `systemShare.powerButton_pushed_CFX_to_CM3` | CFX | CM3 | 파워 버튼 이벤트 (현재 CFX 코드 경로에서 set 지점 미발견). |
| `systemShare.batteryLevel_CfX_to_CM3` | – | – | **미사용 (주석: CM3에서 제어한 이후로 미사용)** |
| `systemShare.consumptionPowerControl_Command_CM3_to_CFX` | CM3 | CFX | 사용 지점 미확인. |
| `systemShare.enter_ULP_mode_Command_CM3_to_CFX` | CM3 | CFX | standby 진입 트리거. standby() 진입 시 CFX가 직접 0으로 클리어. |

[첨언 ④] `system_opMode`, `batteryLevel_CfX_to_CM3`, `isMappingProgramConneted` 같이 **"미사용이지만 지우지 않은 필드"** 가 shared memory 중간에 있는 것이 현 구조의 가장 큰 리스크이다. 이유는 주석에도 쓰여 있듯이 **필드를 제거하면 그 뒤 모든 필드의 오프셋이 바뀌고, CM3 쪽 접근 주소도 동시에 고쳐야 하기 때문**이다. 현재 CFX는 struct pointer 로 접근(오프셋 자동 계산)하지만, CM3 측 레거시 경로가 절대 주소로 접근하고 있다면 동시 반영이 필수다. → **shared_memory.h 정리 PR을 만들 때 반드시 CM3 측 대응 PR과 원자적으로 진행**해야 한다.

### 3.2 맵 / 볼륨 / 사용자 설정

| 필드 | 역할 |
|---|---|
| `connected_ISD_num` (0, 1~4) | CM3가 내부기 연결 번호를 기록. CFX의 `LB_Normal_PowerMode` 의 분기 기준. |
| `connected_ISD_Map_info` | 현재 연결된 ISD의 맵 스탬프 + 사용가능 인덱스 + 각 맵 날짜. |
| `cfx_ISD_info[MAX_NUM_USER=4]` | ISD 본체 정보(시리얼/L-R/사용자명/패스키). |
| `userSettingValue` | mapNum, stimulVolume, audioVolume, indicatorLED_OnOff, indicatorStimul_OnOff, teleCoil_OnOff, Ble_Onff. |
| `userSettingValueLoadedFlag` | CFX ↔ CM3 간 "사용자 설정 정보 복사 완료" 핸드셰이크. |
| `currentMapData` | 현재 활성 맵(자극 전략, 펄스폭, 주파수밴드 수, T/C/xMin/xMax, CIS_FreqBandOrder 등). |
| `calculatedStimulPara_byCM3` | **CM3가 계산**한 채널별 T/C 255, frameNumPerChannel, transferableFrameNum. |
| `calculatedStimulationIndcator_byCM3` | 자극 알림 지시(카운팅으로 ON/OFF 80ms×3회 반복, indicatorStimulLevel_255). |
| `currentOutputStimulLevel_255[32]` | **CFX가 출력 중인** 전극별 자극 레벨을 CM3에 공유(이퀄라이저용). |
| `mapChangeFlag` | 아래 3개 플래그로 구성 (§4.1 시퀀스). |
| `ReadWriteCommand_ForFlash` | 맵 Read/Write/Erase/Recover 명령 및 타깃 인덱스. |
| `repositoryForReadWriteMapData` | 맵 읽기/쓰기용 저장소. |

### 3.3 PCM / I2C / USB / 배터리 / EEPROM

| 필드 | 역할 |
|---|---|
| `cfx_PCM_interface.PCM_mode` | 현재 PCM 출력 모드(0~5, 99). CM3가 지배적으로 세팅, CFX가 읽어 분기. |
| `cfx_PCM_interface.PCM_mode_next` | SpecificCommand 수행 후 자동 복귀할 다음 모드. |
| `cfx_PCM_interface.PCM_liveStimulationTemplate[24]` | 현재 사용 없음(필드만 유지). |
| `cfx_PCM_interface.PCM_specificBuffer[24]` | SpecificCommand 모드에서 CFX가 그대로 PCM FIFO로 흘려 보낼 24-word 버퍼. |
| `cfx_PCM_interface.Reset_PCM` | CM3가 1로 세팅 → CFX가 `fn_reset_PCM()` 이후 0 으로 클리어. |
| `cfx_PCM_interface.conneded_ISDCheckPCM_state` | 백텔 체크 상태 머신(0/1/2). 오타(`connected` 아닌 `conneded`) **그대로 유지**. |
| `cfx_i2c_interface` | CM3 ↔ CFX I2C 프록시(현 CFX 코드 경로에서 사용 미확인, CM3 전용 API 사용 추정). |
| `chargerState.chargerConnectorPluggedIn/carryingCasePluggedIn/carryingCaseCoverOpen` | USB/케이스 상태. normal_loop 에서 `chargerConnectorPluggedIn` 검사 후 LiveStimulation 아닌 경로의 pcmDataOut 을 차단한다. |
| `backtelControlRegister` | 백텔 회로 제어 레지스터 이미지. `Enable/Disable_backtelCircuit` 이 현재 값을 읽어 bit0만 바꿔 다시 쓴다. |
| `batteryCalibrationValue` | 배터리 보정 값. |
| `earpieceDetecion` | 이어피스 감지 결과. |
| `maxAudioInput` | CFX가 직전 16샘플의 최대 입력 절대값을 공유(디버깅/이퀄라이저). |
| `is_CFX_started`, `is_enabled_CFX_iteration` | §3.1 과 동일. |

### 3.4 최근 추가된 플래그 (레거시와 구분되는 명시적 최신 영역)

주석에 **"일시 / 작성 / 내용"** 이 함께 들어간 영역이 있고, 이것만 봐도 "마이그레이션 중 추가"된 필드임을 알 수 있다.

```c
/* 2026-01-20 김은수 : 자극 묵음 기능 활성/비활성화 제어 */
int is_enabled_mute_stimulation_under_t_level;
int mute_stimulation_t_level_offset;

/* 2026-02-24 김은수 : PCM SpecificCommand 읽기 경쟁 상태 표시 */
int is_pcm_specific_command_reading;
int pcm_specific_command_read_index;
```

- **묵음 기능**: 구조체 플래그 `is_enabled_mute_stimulation_under_t_level` 의 의미 규약이 `nonlinearMapping.c:129` 와 `stimulationStrategy.c:393` 에서 **서로 다르다**(자세한 내용은 §8.6에서 다룬다).
- **SpecificCommand 경쟁 방지**: CFX가 `fn_PcmBitStream_Mode_SepcificCommand()` 수행 중엔 1, 종료 시 0 으로 플래그. CM3는 이 값을 폴링하다가 0일 때만 specificBuffer 에 쓰도록 약속되어 있다.

---

## 4. CM3 ↔ CFX 핸드셰이크 시퀀스

### 4.1 맵(프로그램) 변경 시퀀스

정상 경로는 다음과 같다. (타임라인 왼쪽이 이른 순서)

```
    CM3                                                CFX
─────────────────────────────────────────────────────────────────────────
(1) currentMapData.* 에 새 맵 데이터 복사           │
(2) mapChangeFlag.cm3Command_mapChange = 1 ────────▶│
                                                    │(3) LB_Normal_PowerMode
                                                    │    → Normal_PowerMode_isdConnected
                                                    │    → Normal_PowerMode_event_mapChange
                                                    │       ├ fn_PcmBitStream_Mode_NopStandby (선 NOP)
                                                    │       ├ copy_MappingData[_without_mappingDate]
                                                    │       ├ nOFm 사용시 LastStimulus_BandIndex=0, Phase=0
                                                    │       ├ prepare_pcmStimulationPacketHeader
                                                    │       ├ read_FFT_PassBin_index
                                                    │       ├ mapChangeFlag.cfx_Reloaded_MapdataFlag = 1 ▶
                                                    │       └ mapChangeFlag.cm3Command_mapChange = 0
(4) cfx_Reloaded_MapdataFlag=1 감지 후               │
    calculatedStimulPara_byCM3 계산 (T/C 255,       │
    frameNumPerChannel, transferableFrameNum)       │
(5) cm3_audioParameterCalculationDone_Flag = 1 ────▶│
                                                    │(6) Normal_PowerMode_event_audioParameterCalculation
                                                    │    ├ read_transferableChannelNum_fromCM3
                                                    │    ├ read_C_T_Level_fromCM3
                                                    │    ├ VMAG 출력 버퍼 0 클리어
                                                    │    ├ m_previous_stimulation_volume = -1 (강제 재계산 유도)
                                                    │    ├ m_previous_audio_volume       = -1
                                                    │    └ cm3_audioParameterCalculationDone_Flag = 0
(7) (다음 iter) volume != prev 이므로 재계산         │
                                                    │    → calculate_newStimulation_maxLevel
                                                    │    → calculate_logaritmMapping_coeff_with_audioVolume
                                                    │    → m_audio_volume = userSettingValue.audioVolume - 1
```

특수 경로 : **매핑 App의 Live 모드** 에서는 `userSettingValue.mapNum < 0` 이 설정되어 있고, 이 경우에는 `copy_MappingData_without_mappingDate()` 가 호출되어 매핑 날짜는 보존된다. 또한 `logarithmMapping()` 내부의 자극 알림 경로에서도 `mapNum < 0` 분기가 추가되어 **채널 번호를 매 프레임 공유 메모리에서 새로 읽게 되어 있다**.

[첨언 ⑤] `mapChangeFlag` 3개 플래그(map 변경 요청 / CFX 리로드 완료 / CM3 파라미터 계산 완료)는 **사실상 세 단계 양방향 request-ack** 이다. 레거시가 2단계였던 흔적(`cfx_Reloaded_MapdataFlag`)과 E8300 에서 볼륨 연산을 CM3가 담당하게 된 3단계 확장(`cm3_audioParameterCalculationDone_Flag`) 이 함께 있다. 각 플래그는 **반대편이 한 번 읽으면 리세터가 된다**는 규약을 가정하지만, 구조체 주석에 이 규약이 전혀 문서화 돼 있지 않다. → 새 엔지니어가 이 영역을 건드리다가 "한 번만 0으로 돌릴 것"을 잊고 CM3/CFX 양쪽에서 clear 하면 핸드셰이크가 영구히 깨질 수 있다. **반드시 shared_memory.h 주석에 Owner/Writer 규약을 명시해야 한다.**

### 4.2 볼륨 변경 시퀀스

현재 활성 경로(`system_control.c:192` 의 `#else` 블록 = xMin 기능 추가된 코드) 는 다음이다.

```
userSettingValue.stimulVolume != m_previous_stimulation_volume
            or
userSettingValue.audioVolume  != m_previous_audio_volume
            │
            ▼
Normal_PowerMode_event_both_stimulation_audio_volumeChange()
   ├ fn_PcmBitStream_Mode_NopStandby        (선 NOP 로 자극 정지)
   ├ m_previous_stimulation_volume 갱신
   ├ m_previous_audio_volume       갱신
   ├ m_audio_volume = audioVolume - 1
   ├ calculate_newStimulation_maxLevel(stimulVolume)
   │        → g_max_limit_stimulus_amplitude_C_level[] 갱신 (0.7/0.8/0.9/1.0 비율)
   └ calculate_logaritmMapping_coeff_with_audioVolume()
            → xMin 에 오디오 볼륨 선형 게인 적용 후 A/B 계수 재계산
```

[첨언 ⑥] **dead code 위험**:

- `#if 0` 블록에 있는 기존 함수 `Normal_PowerMode_event_stimulationVolumeChange()` 는 **함수 본체가 아직 살아 있지만 호출되지 않는다**(system_control.h 에 prototype만 유지). 마찬가지로 `calculate_logaritmMapping_coeff()`(audio volume 미적용 버전)도 호출되지 않는다. 링커 LTO 가 꺼져 있으면 바이너리에 그대로 남는다.
- `m_audio_volume` 은 현재 "ISD 연결 + 볼륨 변경 발생" 경로에서만 갱신된다. ISD가 해제되었다가 다시 연결될 때, 첫 iteration 은 `_both_` 함수가 두 볼륨이 같다고 판단하면 재계산을 건너뛰고 stale 값이 유지될 수 있다. 그러나 맵 변경 이벤트에서 `m_previous_* = -1` 로 강제하므로 **맵이 한 번이라도 다시 로드되는 한** 결과는 맞다.
- **권장**: `Normal_PowerMode_event_stimulationVolumeChange()` 와 비활성 `#if 0` 블록, 그리고 `calculate_logaritmMapping_coeff()` 를 **완전히 삭제**하든지, 또는 두 경로를 단일 `calculate_logaritmMapping_coeff_with_audioVolume()` 로 합치고 함수명을 중립적으로 리네임 (`calculate_logarithmMapping_coeff`).

### 4.3 ISD 연결 / 해제 시퀀스

```
m_current_isd = Addr_SharedMem->connected_ISD_num

if (m_current_isd != 0):
    if (m_previous_isd != m_current_isd):
        userSettingValueLoadedFlag = 1   // CFX 가 ISD 변경 확인했음을 CM3에 알림
        m_previous_isd = m_current_isd
    Normal_PowerMode_event_mapChange()
    Normal_PowerMode_event_audioParameterCalculation()
    Normal_PowerMode_event_both_stimulation_audio_volumeChange()
else: /* 해제 */
    if (isMappingProgramConneted != 1):
        m_link_connection_check_counter = -300   // 백텔 타이머 초기화
        conneded_ISDCheckPCM_state = BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation
    if (m_previous_isd != 0):                    // 해제되는 첫 시점
        if (isMappingProgramConneted != 1):
            m_prev_agc_10db_gain_Q8_16 = 0       // attack/release 게인 초기화
            userSettingValueLoadedFlag = 0
    m_previous_isd = 0
```

[첨언 ⑦] `isMappingProgramConneted` 는 shared_memory 주석에 **"사용안함 지울 것"** 으로 표시되어 있지만, **실제 코드에서는 위 `Normal_PowerMode_isdDisonnected()` 안에서 비교 연산으로 2번 사용된다**. 지우려면 이 코드 경로를 먼저 없애야 한다.

또한 함수명 **`Normal_PowerMode_isdDisonnected`** (typo : Disonnected) 가 남아 있다. 코드 영향은 없지만 공개 API 네이밍 일관성을 위해 `Normal_PowerMode_isdDisconnected` 로 정정 권장.

### 4.4 SpecificCommand 경쟁 상태 시퀀스 (2026-02-24 추가)

```
CM3                                              CFX
───────────────────────────────────────────────────────────────
(1) PCM_specificBuffer[] 에 커맨드 써야 함        │
(2) is_pcm_specific_command_reading == 0 확인     │
(3) PCM_specificBuffer[] 기입                     │
(4) PCM_mode_next 설정                            │
(5) PCM_mode = SepcificCommand ─────────────────▶│
                                                  │(6) pcmDataOut → fn_PcmBitStream_Mode_SepcificCommand
                                                  │    ├ is_pcm_specific_command_reading = 1
                                                  │    ├ for i in [0..23]:
                                                  │    │     FIFO_PCM_WRITING[-i] = PCM_specificBuffer[i]
                                                  │    │     pcm_specific_command_read_index = i
                                                  │    ├ PCM_mode = PCM_mode_next
                                                  │    └ is_pcm_specific_command_reading = 0
```

- CM3는 읽기(현재 `read_index`) 값을 모니터링하여 "CFX가 버퍼를 어디까지 소비했는지" 확인 가능.
- CFX는 이 모드에서 버퍼 전체를 한 번에 소비(24 word) 후 즉시 다음 모드로 전환.

[첨언 ⑧] 이 경쟁 방지 코드는 **CM3 측 협조 없이는 의미가 없다**. CM3에서 `is_pcm_specific_command_reading == 0` 을 반드시 확인한 뒤에만 `PCM_specificBuffer[]` 에 쓰도록 되어 있어야 하고, CM3 코드를 반드시 같이 검토해야 한다. CM3 측 구현이 없는 상태에서 플래그만 세팅되면 쓸데없는 공유 메모리 트래픽이 된다.

### 4.5 백텔(ISD 연결 확인) 시퀀스

`check_link_connection_state()` 는 `fn_PcmBitStream_Mode_LiveStimulation()` 의 두 번째 작업이다. 1 msec 마다 `m_link_connection_check_counter` 를 증가시키다가 **특정 tick** 에서만 스텝 함수를 수행하는 상태 머신이다.

```
counter  │  동작
─────────┼────────────────────────────────────────────────────────────
0        │  Enable_backtelCircuit()
         │   - backtelControlRegister |= 1  (공유 메모리에 갱신)
         │   - PCM FIFO 첫 word : pcm_Mold_BacktelConfiguration
         │   - 나머지 : pcm_Mold_NopStandby
         │   - conneded_ISDCheckPCM_state = BackelCircuitEnabled
1        │  backtelPcmOut()
         │   - g_pcmFrameNum_per_channel 에 따라 frame_{1|2|3|4}_perChannel
         │     첫 word : pcm_Mold_connectionCheck_ReadISDPower
         │     나머지 : pcm_Mold_NopBacktel (+ Standby 혼합)
2        │  Disable_backtelCircuit()
         │   - backtelControlRegister &= 0xFE
         │   - PCM FIFO 첫 word : pcm_Mold_BacktelConfiguration
         │   - 나머지 : pcm_Mold_NopStandby
4        │  shareConnectionCheckFired_DiableBacktel()
         │   - g_pcmFrameNum_per_channel > 12 일 땐 Cleared 상태로 돌려 생략
         │   - 그 외엔 BackelCircuitDisabled_readPcmFired 로 CM3 알림
others   │  LB_increaseCounter()  : counter++ (300 msec 돌고 0으로 복귀)
```

즉 300 msec 마다 4 msec 정도의 backtel cycle 을 한 번 태우고, CM3는 `conneded_ISDCheckPCM_state = 2` 를 감지해서 FPGA FIFO 값을 읽는다. 읽고 나면 **CM3가 그 필드를 다시 0으로 돌려놓아야 한다**.

[첨언 ⑨] `#ifndef DisalbedBackTel` 가드가 `driver_PCM.c:82` 에 있지만 `DisalbedBackTel` 는 **오탈자** (Disabled→Disalbed) 이면서 **어디서도 정의되지 않는다** → 항상 active. "기능을 잠깐 끄고 싶을 때"의 탈출구로 만든 것으로 보이는데 이 상태로는 무용하다. 오타 수정 후 CMake/ICF 에서 정의 가능한 실제 플래그로 만들거나 단순히 제거.

---

## 5. 신호처리 파이프라인 상세

### 5.1 데이터 경로 전체

```
[DMIC0 (EZ)   ADC1] ─▶ FIFO_A0_0 (HCT_A0_0) ─┐
[DMIC1 (QCC)  ADC2] ─▶ FIFO_A0_1 (HCT_A0_1) ─┤
                                              │
[I2S in        ADC]  ─▶ FIFO_A0_5 (HCT_A0_5) ─┤
                                              │
                                   audioMixer │ (AUDIO_INPUT_RSHIFT=5)
                                              ▼
                              HEAR_ADDR_AUDIO_MIX   (16 샘플)
                                              │
                          ┌──────────────────┘
                          ▼
                HEAR FC0 : agc_preprocessing  (abs + max)
                          │    → HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE
                          ▼  (HEAR_0_ISR ⇒ function_chain0 = 1)
                CFX audio_agc()
                   ├ calculate_agc_gain(max)     (noise / rotation / attenuation / amplify)
                   ├ audio_agc_attack_release   (Q8.16 IIR)
                   └ apply_agc_gain  →  m_agc_output_buffer[16]
                          │
                          ▼
                update_FFT_inputData     (FIFO_A1_0 에 top→bottom 복사)
                          │
                          ▼
                HEAR FC1 : fft_vmag     (Win-DFT + vMag)
                          │    → HEAR_ADDR_VMAG_OUTPUT  (256 bin)
                          ▼  (HEAR_1_ISR ⇒ function_chain1 = 1)
                find_freq_rep_value
                   ├ g_pass_bin_index[256]  (FS에서 로드된 pass-bin → 32 그룹 매핑)
                   ├ g_freq_rep_values[32]  (각 그룹의 max magnitude)
                   └ g_freq_rep_values_scaled[32] = >> 4  (게인 합산분 복원)
                          │
                          ▼
                logarithmMapping
                   y = (A · x^p + B) >> 8
                   clamp to [T_level, C_level]   or  mute if y < T + offset
                   → g_pcm_amplitude_level[32]  (전극별 0~255)
                   → Addr_SharedMem->currentOutputStimulLevel_255[i] = y
                          │
                          ▼
                stimulationStrategy
                   ├ CIS   : 전 주파수 밴드를 FreqBandOrder 따라 PCM 패킷 생성
                   │         addr_transferableChannelNum_per_1msec vs freqBandNum 비교
                   └ nOFm  : Phase0 에서 상위 16개 선별 + 인덱스 정렬 + 인접도 재배열
                             Phase0 상위 8 전송 / Phase1 하위 8 전송
                          │
                          ▼
                FIFO_A0_4 (HCT_A0_4)  →  PCM master (NPP → FPGA → ISD forward)
```

### 5.2 오디오 입력 믹싱

`audioMixer.c` / `audio_mix_internal_mic_only()` — 내부 DMIC 한 채널만 사용할 때는 `HCT_A0_0` 을 `>> 5` 해서 `HEAR_ADDR_AUDIO_MIX` 에 복사.

`main.c:273` `audio_mix_2_buffers((int _XMEM *) &HCT_A0_0[0], p_buffer);` — I2S 스트리밍이 살아있으면 `p_buffer`(I2S) 와 `HCT_A0_0`(DMIC) 를 1:1 합산 후 절반으로 나눠(`>> 1`) 믹싱.

[첨언 ⑩] `normal_loop` 의 조건문은 `(g_interrupt_flags.pcm_out == 1) && (g_interrupt_flags.mic0 == 1)` 이고 **`mic1` 은 주석처리** 되어 있다. 즉 QCC(DMIC2) 마이크는 드라이버는 활성화되지만 동기화 플래그는 무시된다. 이건 "I2S가 살아날지 알 수 없으니 DMIC0 + PCM out 동기만 본다"는 설명이 `main.c:130` 에 있고, **의도는 맞지만 mic1 인터럽트는 남아서 소모되고 있다**. 필요 없다면 `SYS_FIFO_CFXINTCONFIG(1, ...)` 쪽을 NONE 으로 바꿔 인터럽트 부하를 줄이는 것이 올바르다.

### 5.3 AGC

`signalProcessing/agc.c` 의 3-zone 구조:

- **Noise region** : `agc_input_db < m_noise_gate_db` (= -117.5 dB @ Q8.16).  기울기 2.0, y-intercept 는 audio volume (`m_audio_volume`) 별 테이블.
- **Attenuation region** : `m_rotation_point_db < agc_input_db` (= -94.75 dB @ Q8.16).  기울기·y-intercept 모두 audio volume 테이블.
- **Amplify region** : 그 사이.  기울기 1.0 (단위 게인), y-intercept 는 volume 테이블.

Attack/Release IIR:

```
prev <= target  → release (coeff 0.01)
prev >  target  → attack  (coeff 0.95)
```

[첨언 ⑪] 세 영역 진입 조건의 **부등호 방향이 어색한 부분**이 있다. `m_rotation_point_db_Q8_16 < agc_input_db_Q8_16` 를 "크면 attenuation" 으로 쓰고 있는데 주석과 실제 의미를 대조하면:

- `agc_input_db < noise_gate_db` (예: -130 dB) → noise region ✓
- `rotation_point_db < agc_input_db` (예: -80 dB) → attenuation (overload) ✓
- 그 외 ( noise_gate ≤ agc_input ≤ rotation_point ) → amplify ✓

이 부분은 정상이다. 다만 **Noise region slope 2.0과 Attenuation slope 테이블의 단위(부호/scale)** 가 `>> 20`, `>> 16` 으로 서로 다르게 정렬되어 있어서, 코드만 읽어서는 그 이유를 추적하기 어렵다. 주석에 "잡음 영역 ybias 는 Q12.12, 나머지는 Q8.16" 이라고 쓰여 있는 대로 **각 영역의 Q-format 이 실제로 다르기 때문에 정렬 shift 가 다른 것**이다. 유지보수자는 Q-format 변화를 반드시 확인해야 하며, 이 부분은 주석 보강만으로도 큰 품질 향상이 가능하다.

### 5.4 FFT 입력 준비와 Pass Bin 매핑

`update_FFT_inputData()` — `m_agc_output_buffer` 의 16 샘플을 **역순으로** `D_FIFO_A1_0->ACCESS` 에 push. (hardware FFT FIFO 의 pop 순서가 top→bottom 이기 때문)

`find_freq_rep_value()` — `g_pass_bin_index[256]` 에 **각 pass bin 이 32 개 주파수 그룹 중 어디에 속하는지** 가 저장되어 있고(-1 은 미사용), 각 그룹의 **최대 magnitude** 를 뽑아 `g_freq_rep_values[32]` 에 저장한다. 이후 `>> RIGHT_SHIFT_MAX_MAG_FREQ_SCALE(=4)` 로 스케일 보정.

[첨언 ⑫] `g_pass_bin_index[]` 초기값은 0. `read_FFT_PassBin_index()` 는 `addr_MapProgramData_FrequencyAnalysisBandNumbers == *ADDR_NUM_OF_FREQ_BAND_IN_FS_MEM` 일 때만 복사한다. 즉 **첫 부팅 시 FS 메모리에 아직 pass bin 데이터가 안 올라왔거나, 맵의 밴드 수와 FS의 밴드 수가 다르면** pass bin 은 전부 0 → `find_freq_rep_value()` 가 모든 magnitude 를 index 0 에만 쌓는다. 이 상태에서 정상 자극처럼 보이지는 않겠지만 **채널 0에 큰 자극이 한 방에 쏠릴 수 있음**. 방어 로직 추가를 권장 (예: `read_FFT_PassBin_index()` 실패 시 모든 bin 을 -1 로 세팅해서 모든 그룹을 0 으로 고정).

### 5.5 로그 매핑 (Nonlinear Mapping)

수식: `y = A · x^p + B`, 클램프 to `[T, C_max_by_volume]`, 또는 뮤트.

주요 변수/함수:

- `calculate_x_power_p(x)` : `x^0.15` (p=0.15) 를 Q8.16 로 리턴. `cfx_pwr_to_dB_asm` / `cfx_dB_to_pwr_asm` 사용.
- `calculate_newStimulation_maxLevel(stimulVolume)` : 볼륨 1~4 에 0.7 / 0.8 / 0.9 / 1.0 비율 곱해 `g_max_limit_stimulus_amplitude_C_level[32]` 갱신.
- `calculate_logaritmMapping_coeff_with_audioVolume()` : xMin 에 오디오 볼륨 선형 게인을 미리 곱한 뒤 A/B 재계산. 디버깅을 위해 `FS_MEM_UART->buffer[]` 의 `[0..31]` xMin, `[32..63]` gained xMin, `[64..95]` xMax, `[96..127]` A, `[128..159]` B, `[160..191]` C, `[192..223]` T 를 채운다(UART flag 1/2 에 따라).

[첨언 ⑬] `calculate_logaritmMapping_coeff()` 와 `calculate_logaritmMapping_coeff_with_audioVolume()` 의 실제 차이는 **xMin 대신 gained xMin 사용 한 줄뿐**이다. Branch 하나로 합쳐 `void calculate_logarithmMapping_coeff(bool apply_audio_volume)` 형태로 리팩터링 가능. 중복 코드가 수백 줄이라 버그 발생 시 동기화 실패 위험이 크다.

### 5.6 뮤트 로직 (v2 : CM3 제어)

```c
is_enabled = Addr_SharedMem->is_enabled_mute_stimulation_under_t_level;

if (is_enabled == 1) {               // 묵음 활성
    if (y < T + offset) y = 0;
} else if (is_enabled == 2) {        // 묵음 비활성
    if (y < T) y = T;                // 최소 T 유지
} else {                             // 정의되지 않음
    y = 0;                           // 안전하게 자극 OFF
}
```

[첨언 ⑭] **큰 문제 후보** : 이 3-value enum 은 `stimulationStrategy.c:393` 의 **CIS 자극 스트리밍** 에서도 참조되는데, 그쪽의 의미 규약이 다르다.

```c
// stimulationStrategy_CIS()
if (is_enabled == 1) {   // 묵음 활성
    if (amp == 0) → ISD_register forward check 패킷
    else           → 자극 패킷
} else {                 // 1이 아니면 어떤 값이든 -> 자극 패킷
    → 자극 패킷
}
```

즉 **`nonlinearMapping.c` 에서는 `== 2` 가 "비활성", `!= 1 && != 2` 는 전부 "자극 OFF"** 인 반면, **`stimulationStrategy.c` 에서는 `== 1` 이 아니면 전부 "묵음 기능 미적용(자극 패킷 그대로 전송)"** 이다. 그런데 nonlinearMapping 이 y=0 으로 만들어 놓으면 stimulationStrategy 의 `amp == 0` 분기도 함께 트리거 되어 ISD forward check 가 들어가 사실상 동작은 맞게 보일 수 있다. 하지만 상태 정의가 두 파일에서 엇갈려 **"정의되지 않은 상태"에서 자극 레벨 y=0 인 채 CIS 는 자극 패킷을 그대로 내보내는 이중 해석 버그**가 존재한다. → **CIS 쪽도 `!= 1 && != 2` 를 별도 분기로 처리하거나, 둘을 enum 으로 정의 (`en__mute_disabled`, `en__mute_enabled`, `en__mute_undefined`) 해 한 곳에서 해석하도록 통일**이 필요하다.

### 5.7 자극 지시 (Indicator Stimulus)

`logarithmMapping()` 마지막부:

```
if (calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 == 1) {
    if (userSettingValue.mapNum < 0) {   // 매핑 App Live 중 실시간 채널 변경
        addr_MapProgramData_indicatorStimulCannel_index
            = currentMapData.stimulationIndicatorChannelNum;
    }
    g_pcm_amplitude_level[indicator_index - 1]
        = calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255;
}
```

[첨언 ⑮] `addr_MapProgramData_indicatorStimulCannel_index` 초기값 0. 이 경우 `index - 1 = -1` 로 음수 인덱스 접근이 가능하다. CM3 측에서 indicator 기능 활성화 전에 반드시 채널 번호(1~32)를 유효하게 설정해 놓는다는 가정이 있는데, **CFX 코드에서 `indicator_index >= 1` 가드가 없다**. 방어적으로 `if (idx >= 1 && idx <= 32)` 가드 추가 권장.

### 5.8 자극 전략 : CIS

핵심 루프 (`stimulationStrategy.c:350`):

```
for i in [0..addr_MapProgramData_FrequencyAnalysisBandNumbers):
    freqBandOrder = addr_MapProgramData_CIS_FreqBandOrder[i] - 1
    electrodIndex = addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1
    electrodeMap  = addr_electrodeMap[electrodIndex] << electrodIndexPositionAtPCM_Mold  (10)
    stimulusLevel = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold (2)
    addr_stimulationTempBuff[i] = electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header
```

PCM 패킷 워드(20 bit)의 비트 배치 (`internalDevice/driver_PCM.h`):

```
bit 19 18 17 16 15  14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    0  1  0  0 P    e  e  e  e  e   s  s  s  s  s  s  s  s  0  0
    │       │ │     └── electrodeMap (5 bit) ──┘  stimulusLevel (8 bit)
    └ Mold_Stimulation(0x40000)  └ firstPulsePhase
```

전송 타이밍 분기는 `addr_MapProgramData_FrequencyAnalysisBandNumbers` 와 `g_transferableChannelNum_per_1msec`(CM3가 계산, 펄스폭 기반) 관계에 따라 두 가지:

- **밴드 수 < 전송 가능 채널 수** : 밴드 다 보내고 남는 슬롯은 NopStandby 로 채움. (rate-limit)
- **밴드 수 ≥ 전송 가능 채널 수** : 현재 `addr_transferred_index` 부터 전송, 전체 밴드를 몇 ms 나눠서 라운드로빈.

### 5.9 자극 전략 : nOFm (n of m)

로직:

```
(1) 32 밴드 중 상위 16 선택 (heap 대신 최소값 교체 방식)
(2) 선택된 16개 인덱스를 "프레즌스 맵"으로 오름차순 정렬
(3) 인접도 재배열 (0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15) 패턴
(4) "마지막 자극 밴드"로부터 가장 먼 인덱스부터 원형 진행
(5) Phase 0 : 상위 8개 전송 (stimulationTempBuff[0..7])
(6) Phase 1 : 하위 8개 전송 (stimulationTempBuff[8..15])
```

Phase 0 에서만 재계산하고 Phase 1 은 그대로 가져다 쓴다 (2 ms 주기 합성).

[첨언 ⑯] **버그 후보** : `stimulationStrategy_nOFm()` 내부 (`stimulationStrategy.c:186`):

```c
for (int i = 0; i < df_MaxNum_nOFm; i++)
{
    int idx = g_nOFm_adjacent_BandIndex[i];

    if (i >= 0 && idx < df_MaxNumOfElectrode)   // ←★
    {
        present[idx] = 1;
    }
}
```

- `i` 는 루프 인덱스이므로 `i >= 0` 은 항상 true. 의도한 가드는 아마 **`idx >= 0`** 였을 것이다.
- 초기값으로 `g_nOFm_adjacent_BandIndex[]` 는 모두 -1 로 채워지고, 상위 16 선별 후에도 사용 가능한 밴드 수가 16 미만이면 -1 이 남는다. `present[-1] = 1` 은 C 언어로는 **배열 범위 밖 쓰기**. 스택 인접 변수를 오염시킬 수 있다.
- 현재 상황에서는 대부분의 맵이 밴드 수 ≥ 16 이어서 -1 이 남지 않아 우연히 안전하지만, **밴드 수 1~15 인 환자 맵에서는 언제든 터질 수 있다**.
- **수정 권장** : `if (idx >= 0 && idx < df_MaxNumOfElectrode)` 로 바꾼다.

또한 nOFm Phase0/Phase1 구조는 **Phase0 계산 → 1 ms 뒤 Phase1 전송** 사이에 `g_pcm_amplitude_level[]` 이 바뀔 수 있다(다음 프레임 FFT/매핑이 Phase1 전송 직전에 들어갈 경우). Phase1 은 Phase0 당시의 스냅샷 `addr_stimulationTempBuff[8..15]` 만 쓰므로 **의도대로 일관된 16 자극** 이 보장되지만, 대신 **최대 2 ms 지연** 을 감수한다. 레거시 문서/주석에 이 의도가 기록되어 있지 않으면 리뷰어가 오판할 수 있다.

### 5.10 PCM 출력 구성

`fn_PcmBitStream_Mode_LiveStimulation()` = `stimulationStrategy()` + `check_link_connection_state()`.

PCM FIFO (`FIFO_A0_4`, `HCT_A0_4`, 64 word) 는 **double-access 미적용** 이며 `HEAR_PCM_FIFO_BLK_SIZE = 24`. 즉 한 번의 매핑 라운드에서 24 word(= 1 msec) 를 소비한다.

```
HEAR_ADDR_FIFO_PCM_WRITING = HEAR_ADDR_FIFO_PCM + 23    (top 주소)
writing 방향  : top → bottom (포인터 -- 로 이동)
```

### 5.11 I2S 입력 처리

`lib_i2s.c` 의 상태 머신:

```
LIB_I2S_STATE_DISABLED   ─(FLAG=active, interrupt_cnt 변화)─▶ LIB_I2S_STATE_ENABLED
LIB_I2S_STATE_ENABLED    ─(FLAG=inactive)─▶                 LIB_I2S_STATE_DISABLED
LIB_I2S_STATE_ENABLED    ─(인터럽트 10회 연속 없음)─▶         LIB_I2S_STATE_DISABLED
```

버퍼 링 : `lib_g_i2s_buffers[I2S_BUFFER_FULL_READY_CNT = 8][16]` (in_pos / out_pos).
Fade-in/out : `I2S_BUFFER_UNDERRUN_CNT = 4` 기준 4 × 16 계수 테이블 4세트.

`main.c::I2S_handle()` 흐름:

```
lib_i2s_copy_data_from_fifo(A0_5)
switch (lib_g_i2s_buffer_state):
    READY :
        p = I2S_get_buffer_with_fade_in_process()
        audio_mix_2_buffers(HCT_A0_0, p)
        lib_audio_loopback(HCT_A0_3, p)   // DAC1로 I2S 출력
        copy_cnt--
        if (copy_cnt == UNDERRUN_CNT) state=UNDERRUN, sub_state=FADE_OUT_STEP_0
    UNDERRUN :
        p = I2S_get_buffer_with_fade_out_process()
        audio_mix_2_buffers(HCT_A0_0, p)
        lib_audio_loopback(HCT_A0_3, p)
        if (sub_state != FADE_OUT_END) copy_cnt--
lib_i2s_copy_data_from_fifo(A0_5)   // 두 번째 호출
```

[첨언 ⑰] 두 번 `lib_i2s_copy_data_from_fifo()` 를 호출하는 이유는 **인터럽트 타이밍 누락 보완** 으로 추정된다. FIFO 블록 당 16 샘플 미리 받기 + 후속 16 샘플 드레인. 내부 `disable_interrupts()` / `enable_interrupts()` 로 race 방어를 하는데, **CFX의 interrupt gate 전역 상태를 드라이버 함수 안에서 바꾸는 패턴** 은 main loop 의 `SYS_WAIT_FOR_INTERRUPT` 타이밍과 상호작용할 수 있다. 현재 normal_loop 은 처리 끝에만 WFI 를 호출하므로 문제 없어 보이지만, 나중에 low-latency 개선을 시도할 때 반드시 점검할 포인트이다.

[첨언 ⑱] **디버그 오염** : `main.c:262`, `main.c:269-271`, `main.c:289`, `main.c:296-298` 에서 **`Addr_SharedMem->currentOutputStimulLevel_255[30]`, `[31]`, `[0..15]`** 에 I2S 버퍼 내용과 상태값을 덮어쓴다. 이 배열은 본래 **전극 32개의 현재 자극 레벨** 을 CM3에 알리는 목적이며, CM3 측 이퀄라이저 UI 에서 사용한다. 현 구현에서는 I2S 디버그가 활성화되어 있는 한 **CM3 이퀄라이저에 I2S raw 값이 자극 값으로 표시되는 상태**다. → 출시용 빌드에서는 반드시 `#if DEBUG_I2S` 등으로 감싸야 한다.

---

## 6. PCM 출력 모드 상세 (`driver_PCM.c`)

| 모드 | 값 | 용도 | 핵심 동작 |
|---|---|---|---|
| FillZero | 0 | 초기 상태 | FIFO 24 word 를 0으로 |
| Preamble | 1 | 자극 전 시작 시퀀스 | FILL_ZERO/TOGGLE 상태로 번갈아 출력. TOGGLE 후엔 **PCM_mode = NopStandby 로 자동 전환** |
| LiveStimulation | 2 | 정상 동작 | `stimulationStrategy()` + `check_link_connection_state()` |
| SepcificCommand (typo) | 3 | CM3 직접 PCM 주입 | `PCM_specificBuffer[24]` → FIFO. 종료 시 `PCM_mode = PCM_mode_next` |
| NopStandby | 4 | 자극 정지 | `pcm_Mold_NopStandby` 24개 |
| NopBacktel | 5 | 백텔 NOP | `pcm_Mold_NopBacktel` 24개 |
| notApplicable | 99 | 예약 | – |

[첨언 ⑲] 모드 명 `PcmBitStream_Mode_SepcificCommand` 는 **"Specific" 오타**. public API 이므로 정정 시 CM3 쪽도 같이 수정 필요 → 일회성 마이그레이션 PR 권장.

[첨언 ⑳] `fn_PcmBitStream_Mode_Preamble()` 종료 시 `PCM_mode = NopStandby` 로 **CFX가 공유 메모리를 덮어쓴다**. CM3도 동시에 이 필드를 쓸 수 있으므로 **원자적으로 안전한 필드 선정이 아니다**. 실제 문제 사례가 발생하면 이 필드를 `volatile int` + 단일 writer 로 강제할 수 없으므로, **Preamble 종료 → NopStandby 로의 상태 전이는 CM3가 수행하도록 역할을 재분배** 하는 게 이상적이다.

---

## 7. 인터럽트 / ISR 지도

`interrupt_service_routine.c` 의 ISR 들은 **단일 플래그 비트만 세팅** 하는 최소 구현이다.

| ISR | 비트 | 의미 |
|---|---|---|
| `HEAR_0_ISR` | `function_chain0` | FC0 (AGC preprocessing) 종료 |
| `HEAR_1_ISR` | `function_chain1` | FC1 (FFT + vMag) 종료 |
| `FIFO_0_ISR` | `mic0` | DMIC0 (EZ) 블록 준비 |
| `FIFO_1_ISR` | `mic1` | DMIC1 (QCC) 블록 준비  → **현재 사용 안 됨** |
| `FIFO_2_ISR` | `dac0` | DAC0 FIFO 준비        → 사용 미확인 |
| `FIFO_3_ISR` | `dac1` | DAC1 FIFO 준비        → 사용 미확인 |
| `FIFO_4_ISR` | `pcm_out` | PCM 출력 FIFO 준비 (1 msec) |
| `FIFO_5_ISR` | (전용 `lib_g_i2s_interrupt_flag`, `_cnt`) | I2S 입력 블록 준비 |
| `FIFO_6_ISR` | `i2s_out` | I2S 출력 FIFO 준비    → 현재 사용 안 됨 |
| `CM3_0_ISR` | `read_isd_info` | ISD info 읽기 요청 이벤트 |
| `CM3_1_ISR` | `wake_up` | standby 탈출용 |

[첨언 ㉑] 다음 플래그들은 **선언돼 있지만 본 코드에서 참조 안 됨** : `read_isd_info`, `dac0`, `dac1`, `i2s_in`(struct 멤버 자체도 `i2s_in` 이 있는데 `i2s_in` 플래그는 한 번도 `1` 로 세팅 안 됨 — 전용 `lib_g_i2s_interrupt_flag` 사용). 정리 시 구조체 멤버 축소 + 해당 FIFO 인터럽트 enable 비트 해제를 함께 해야 한다.

[첨언 ㉒] `HEAR_LiveStimulation_Mode()` 는 FC0 종료 시 진입하여 내부에서 **do-while 로 FC1 완료를 busy-wait** 한다(`main.c:356-363`). 그 사이 다른 FIFO ISR 가 발생하면 플래그만 세팅되고 처리는 지연된다. CFX 전체 타이밍 버짓(1 msec) 을 초과하지 않도록 AGC+FFT+vMag+mapping+stimulation 이 **< 1 msec** 으로 묶여야 하며, 주석에 기재된 시간(FFT+vMag 216 μs, find_freq 113 μs, logaritmMapping 76 μs) 합계 약 400 μs 이후에도 상당한 여유가 있다.

---

## 8. 레거시 마이그레이션에서 꼬인 부분 · 수정 제안 종합

앞 절에서 산발적으로 언급한 이슈를 **우선순위** 로 재정리한다.

### 8.1 구조적 (높음)

1. **[공유 메모리 미사용 필드 잔존]** `system_opMode`, `batteryLevel_CfX_to_CM3`, `isMappingProgramConneted` 등이 shared_memory.h 중간에 남아 있음. 제거하려면 CFX/CM3 양쪽의 접근 주소를 동시에 수정하는 단일 PR 필요. **각 필드에 Owner/Reader 주석 추가 후 미사용 필드는 순차적으로 제거** 하는 정리 PR 권장.
2. **[mute 기능 정의 불일치]** `is_enabled_mute_stimulation_under_t_level` 의 해석이 `nonlinearMapping.c` 와 `stimulationStrategy.c` 에서 다르다. **enum 화 + 한 곳에서 해석** 으로 일원화.
3. **[nOFm present[idx] 범위 버그]** `stimulationStrategy.c:186` `i >= 0` → `idx >= 0` 로 수정. 밴드 수 < 16 인 맵에서 메모리 오염 가능.
4. **[pass bin 초기화 방어 부족]** `g_pass_bin_index[]` 가 FS 와 밴드 수 불일치 시 기본 0 으로 남는다. `read_FFT_PassBin_index()` 실패 경로에서 -1 채움 + 에러 코드 세팅 권장.
5. **[indicator 채널 인덱스 음수]** `addr_MapProgramData_indicatorStimulCannel_index - 1` 의 `0 - 1 = -1` 접근 가능. 가드 추가.

### 8.2 사용하지 않는 코드 (중간)

6. **[dead function]** `Normal_PowerMode_event_stimulationVolumeChange()`, `calculate_logaritmMapping_coeff()` 호출 없음 (xMin 기능 활성화 이후).
7. **[dead variable]** `g_standby_checker`, `g_break_point`, `addr_stimulationData[]`, `g_interrupt_flags.{read_isd_info, dac0, dac1, i2s_in, i2s_out, mic1}`, `lib_g_i2s_buffer_prev1/prev2`, `lib_g_i2s_last_output_val`, `lib_g_i2s_click_occurred` 중 일부.
8. **[dead macro/branch]** `#ifndef DisalbedBackTel` (오타 + 정의되지 않음), `#if 0` 블록들 (AGC 디버그, stimulationStrategy nOFm 크기 순 테스트 입력, logarithm 디버그 buffer 400~499).
9. **[미사용 I2S Bridge]** `lib_i2s_bridge.c` 의 `build_bridge_C1_Q816_cfx()` 는 I2S fade 구현의 대안(Hermite C1 보간) 으로 보이는데 현재 `main.c` 에서 호출하지 않는다. "수준 도입 시도 후 폐기" 의 흔적.

### 8.3 디버깅 잔재 (중간)

10. **[이퀄라이저 공유 배열 오염]** `I2S_handle()` 이 `Addr_SharedMem->currentOutputStimulLevel_255[0..15, 30, 31]` 에 I2S 상태값 덮어씀. 빌드 플래그로 가드.
11. **[FS_MEM_UART 디버그 항상 활성화]** `calculate_logaritmMapping_coeff_with_audioVolume()` 내 UART 디버그 복사 코드가 `#if 1` 로 활성. 출시 빌드에서 제거.

### 8.4 오타 / 네이밍 (낮음)

12. `conneded_ISDCheckPCM_state` → `connected_ISDCheckPCM_state`
13. `PcmBitStream_Mode_SepcificCommand` → `PcmBitStream_Mode_SpecificCommand`
14. `indicatorStimulOutput_OnOff_Coltroled_byCM3` → `..._Controlled_...`
15. `Normal_PowerMode_isdDisonnected` → `Normal_PowerMode_isdDisconnected`
16. `audio_input_x_mim` → `audio_input_x_min`
17. `Ble_Onff` → `Ble_OnOff`
18. `BackelCircuit…` → `BacktelCircuit…` (전체 파일에 산재)

네이밍 정정은 **CM3 공유 구조체와 필드명이 일치해야 하므로 단일 PR** 에서 양쪽을 동시에 바꿔야 한다. 컴파일러가 필드명 미스매치를 잡아주므로 안전성은 높다.

### 8.5 주석/문서화 (낮음)

19. 공유 메모리 각 필드에 **Writer / Reader** 명시.
20. `mapChangeFlag` 3플래그 **request-ack 시퀀스 다이어그램** 을 공유 헤더에 주석으로 삽입.
21. AGC 3-zone Q-format 정렬 이유를 `agc.c` 상단 주석으로 통합.
22. `stimulationStrategy_nOFm` 인접도 재배열 인덱스 의미를 주석으로 설명.

### 8.6 "실제 문제가 될 가능성이 높은 순서" 정리

| 순위 | 대상 | 증상 시나리오 |
|---|---|---|
| ★★★ | nOFm `present[idx]` 범위 미검사 | 밴드 수 < 16 맵 로딩 시 메모리 오염 |
| ★★★ | mute enum 해석 불일치 | "정의되지 않은 상태" 에서 CIS 가 무자극 아닌 자극 패킷을 송출 |
| ★★☆ | indicator 채널 0 → index -1 | 포인터 뒤로 접근 시 스택/XMEM 오염 |
| ★★☆ | shared memory 미사용 필드 제거 실패 | 일부 제거만 진행되면 CM3와 오프셋 어긋남, 전체 공유 통신 파괴 |
| ★★☆ | `Preamble → NopStandby` CFX write | CM3 동시 write 시 PCM 모드 일관성 깨짐 |
| ★☆☆ | `currentOutputStimulLevel_255[30/31]` 디버그 오염 | 리모콘/매핑 앱에서 자극 시각화 오류 |
| ★☆☆ | mic1 인터럽트 enable + 플래그 무시 | 전력/CPU 소모 약간 증가 |

---

## 9. 유지보수를 위한 체크리스트

새로 팀에 합류하거나 장기간 이탈 후 복귀할 때 다음 순서로 확인 권장:

1. **공유 메모리 버전 일치** : `environment/shared_memory.h` 와 CM3 측 동일 헤더의 **sizeof / offset** 이 동일한지.
2. **FS 메모리 초기화** : 부팅 직후 `g_pass_bin_index[]` 가 정상 로드되는지. 로그매핑 A/B 계수가 0이 아닌지.
3. **PCM 모드 전이** : Preamble 이후 NopStandby → LiveStimulation 까지 1회 이상 전환됐는지.
4. **백텔 주기** : `conneded_ISDCheckPCM_state` 가 0 → 1 → 2 순으로 300 msec 주기로 돌고 있는지.
5. **nOFm 범위** : 현재 환자 맵의 사용 밴드 수(`FrequencyAnalysisBandNumbers`) 가 16 이상인지 (미만이면 §8.1.3 버그 확인 필수).
6. **묵음 상태** : `is_enabled_mute_stimulation_under_t_level` 가 1 또는 2 로만 세팅되는지 (0/기타 값 들어오면 CIS 에서 미동작).
7. **이퀄라이저 UI** : CM3 이퀄라이저 표시값에 I2S raw 값이 섞이지 않는지 (디버그 오염 확인).

---

## 10. 부록 : 주요 상수 요약

| 상수 | 값 | 위치 |
|---|---|---|
| `df_inputADC_DataBuffLength` | 16 | definitionsForAlgorithm.h |
| `df_MaxNumOfElectrode` | 32 | 동 |
| `df_MaxNum_nOFm` | 16 | 동 |
| `FFT_SIZE` / `HALF_FFT_SIZE` | 512 / 256 | 동 |
| `df_MaxNumTransferableChannel` | 24 | 동 |
| `df_connectionCheckPeriod_ms` | 300 | 동 |
| `AUDIO_INPUT_RSHIFT` | 5 | 동 |
| `RIGHT_SHIFT_MAX_MAG_FREQ_SCALE` | 4 (window 모드) | 동 |
| `NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY` | 5 | 동 |
| `df_maxAudioForLogarithm` / `df_minAudioForLogarithm` | 21892 / 8 | 동 |
| `T_levelOffset` | 2 | 동 |
| `ISD_registerAddr_forwardPath_check_Data` | 0x50783 | 동 |
| `I2S_BUFFER_UNDERRUN_CNT` | 4 | lib_i2s.h |
| `I2S_BUFFER_FULL_READY_CNT` | 8 | 동 |
| `LIB_I2S_DATA_BUF_LEN` | 16 | 동 |
| `Addr_SharedMem 베이스` | 0x28000 (IOMEM) | shared_memory.h |
| `MAX_NUM_USER` / `MAX_NUM_MAP` | 4 / 4 | definitionsForAlgorithm.h |

---

## 11. 작성자 주

본 문서는 **src/1__cfx/** 하위 전 파일을 한 차례 정독하며 추출한 정적 분석 결과이다. 동적 동작(실제 FPGA 응답, backtel 성공률, 실제 PCM 타이밍)은 현장 계측이 추가로 필요하다. 특히 §8 에서 ★★★ 로 표시한 항목은 **환자 맵이 밴드 수 < 16 이거나 mute 기능이 enum 에 없는 값으로 세팅되는 순간 실제 오동작**이 재현될 수 있으므로, 출시 전 회귀 테스트 항목으로 반드시 포함시키길 권장한다.

변경 이력은 본 문서 Rev.1 이후 추가한다.
