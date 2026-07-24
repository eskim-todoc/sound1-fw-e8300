# A. 미사용 매크로 · 상수 · 타입

**총 150건** (제거 권고 141건 · 보류 권고 9건)

담당 범위: 매크로·상수(enum 값 포함)·타입(typedef/struct)만. 함수·변수, UART 게이트 항목, `FS_MEM_UART`, 모듈 통째 제거 5건(`tdc_dfu_ota`·`tdc_sys_earpiece`·`tdc_isd_map_test_stim`·`tdc_pwr_lsad`·`tdc_hal_i2c_cfx`), 불가침 항목은 제외했다(근거는 §비고).

모든 라인 번호는 로그 인용이 아니라 이번 조사에서 `Read`/`Grep`으로 실제 소스를 직접 열어 확인한 값이다. 특히 `board/99_eeprom_address.h`는 로그(노드10)가 "약 40종 전량 미사용"이라 보고했으나, 재검증 결과 4개는 실제로 외부에서 쓰이고 있어 목록에서 제외했다(§비고 1번 참고).

---

## `board/processorDirective.h`

- [x] `DebuggingMode_3_3V_PMIC_controlled_by_CFX` — `processorDirective.h:27` · 바깥 `#if 0`(24행)로 정의 자체가 죽어있고 전역 `#ifdef` 소비처 0건 · **안전**
- [x] `DebuggingMode_Only_CFX_code` — `processorDirective.h:31` · 상동 · **안전**
- [x] `DebuggingMode_telecoilTestInput` — `processorDirective.h:35` · 상동 · **안전**
- [x] `DebuggingMode_AGCTestInput` — `processorDirective.h:39` · 상동 · **안전**
- [x] `DebuggingMode_AGC_TestMaxInput` — `processorDirective.h:43` · 상동 · **안전**
- [x] `DebuggingMode_FFT_TestInput` — `processorDirective.h:47` · 상동 · **안전**
- [x] `DebuggingMode_Vmag_TestInput` — `processorDirective.h:51` · 상동 · **안전**
- [x] `DebuggingMode_FreqAnal_TestInput` — `processorDirective.h:55,63`(동일 이름 중복 정의 2곳) · 상동 · **안전**
- [x] `DebuggingMode_StimulVolumeTest` — `processorDirective.h:59` · 상동 · **안전**
- [x] `DebuggingMode_Test_generatingPCM` — `processorDirective.h:67` · 상동 · **안전**
- [x] `DebuggingMode_Test_PCM_dataOut` — `processorDirective.h:71` · 상동 · **안전**
- [x] `DebuggingMode_LED_Pins_as_CFX_TestPoint` — `processorDirective.h:75` · 상동 · **안전**
- [x] `DebuggingMode_Only_CM3_code` — `processorDirective.h:79` · 상동 · **안전**
- [x] `AudioInputSignal_Tx_usingUART` — `processorDirective.h:119` · 자체 `#if 0`(117행)로 이미 비활성 + 전역 `#ifdef` 소비처 0건(오디오 신호를 UART로 송신하는 CM3측 구현 자체가 없음) · **안전**
- [x] `CFX_UART_USING_ISR` — `processorDirective.h:123` · CM3 소스 전역 미참조. 이름상 CFX(1__cfx) 측 UART ISR 설정용으로 추정되나 이 파일이 CFX와 공유/동기화되는 사본인지 CM3 조사 범위로는 미확인(노드01 판단보류) · **주의**
- [x] `CFX_UART_BAUD_RATE` — `processorDirective.h:126` · 동일 사유(CFX UART 통신 속도 설정값 추정, CM3 자체는 미참조) · **주의**
- [x] `CFX_UART_RX_ENABLE` — `processorDirective.h:136` · 동일 사유 · **주의**
- [x] `UART_bufferLength_forAudio` — `processorDirective.h:137` · 동일 사유 · **주의**

## `cfx_link/tdc_shm_addr.h`

- [x] `tdc_shm_addr.h` 오프셋 매크로 81개 일괄(`Addr_SharedMem_*`/`addr_SharedMem_*`/`Addr_sharedMem_*`, 27~127행 — 한 구조체의 오프셋 세트라 묶음 1항목으로 처리) — `tdc_shm_addr.h:27-127` · 이번 조사에서 재검증(grep) 결과 이 81개 매크로 자체를 직접 참조하는 코드는 CM3 전역에 0건(주소 계산에 실제 쓰이는 것은 `CM3_DataMemoryBaseAddr`·`CFX_AccessAddrForCM3DataMem`·`StartAddressCM3_sharedVarialbe`·`BaseAddr_CM3CFX_SharedVariable` 4개뿐, 이 81개는 그 4개로부터 파생된 후속 계산값). 다만 `cfx_cm3_sharedMemoryAll` 구조체 필드 오프셋을 1워드 단위로 전부 나열한 **공유 ABI 문서** 역할을 하고 있고, 노드10 조사에서 1__cfx·5__calibration 프로젝트가 동일 레이아웃을 각자 별도 파일로 들고 있다가 이미 3파(01-20/02-24/07-20)에 걸쳐 drift가 발생한 사실이 확인됨 — 이 파일이 그 drift를 검증할 유일한 참고 자료일 수 있어 삭제 시 대조 수단을 잃을 위험이 있음 · **주의** (은수님 판단 필요 — 삭제보다 "미사용 확인 주석 추가 후 존치" 또는 별도 ABI 문서로 이관을 권장)

## `board/99_eeprom_address.h`

파일 전체가 정의하는 매크로 중 4개(`df_24bitWordLength_ISD_info`·`_UserSettingValues`·`_mappingDate`·`_ProgramData`)는 노드10 보고와 달리 실제로 `fs/tdc_fs.c`·`cfx_link/tdc_cfx_eeprom_read.c`·`cfx_link/tdc_shm_addr.h`에서 쓰이고 있어 이번 목록에서 제외했다(§비고 1번). 아래는 그 4개를 제외하고 이번 조사에서 전량 재검증(grep)해 외부 참조 0건을 확인한 63개다. 이 파일은 EEPROM 물리 레이아웃을 기술하는 유일한 문서일 가능성이 있어(노드10 지적), 코드에서 매크로만 정리하더라도 레이아웃 자체는 별도 문서로 보존하는 것을 권장한다.

- [x] `AT_WREN` — `99_eeprom_address.h:8` · SPI EEPROM Write-Enable 옵코드 상수, 전역 미참조 · **안전**
- [x] `df_eepromLastData_Adrress` — `99_eeprom_address.h:10` · EEPROM 끝 주소 상수, 파일 내부 계산에만 연쇄 참조, 외부 소비처 0건 · **안전**
- [x] `df_eepromAddrressByteLengthFor24bitWord` — `99_eeprom_address.h:11` · 정의 외 어디에도(파일 내부 포함) 참조 없음 · **안전**
- [x] `df_MaxNumberOfUsableElectrode` — `99_eeprom_address.h:12` · 정의 외 참조 0건(실사용 전극수 매크로는 별도의 `df_MaxNumOfElectrode`) · **안전**
- [x] `df_24bitWordLength_FFT_PassBin_index` — `99_eeprom_address.h:102` · 파일 내부에서만 연쇄 참조, 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_FFT_PassBin_index` — `99_eeprom_address.h:103` · 상동 · **안전**
- [x] `df_24bitWordLength_Half_FFT_Size` — `99_eeprom_address.h:104` · 상동 · **안전**
- [x] `df_ByteLength_Half_FFT_Size` — `99_eeprom_address.h:105` · 정의 외 참조 0건(파일 내부 포함) · **안전**
- [x] `df_24bitWordLength_FFT_windowCoeff` — `99_eeprom_address.h:110` · 파일 내부에서만 연쇄 참조, 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_FFT_windowCoeff` — `99_eeprom_address.h:111` · 상동 · **안전**
- [x] `df_ByteLength_ISD_info` — `99_eeprom_address.h:117` · 짝인 `df_24bitWordLength_ISD_info`는 외부에서 쓰이나(§비고 1) 이 바이트-길이 값 자체는 파일 내부(233-236행)에서만 참조, 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_UserSettingValues` — `99_eeprom_address.h:122` · 짝인 워드-길이 값은 외부 사용되나 이 값 자체는 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_MapStemp` — `99_eeprom_address.h:125` · 파일 내부에서만 연쇄 참조, 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_MapStemp` — `99_eeprom_address.h:126` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_mappingDate` — `99_eeprom_address.h:132` · 짝인 워드-길이 값은 외부 사용되나 이 값 자체는 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_stimulationStrategy` — `99_eeprom_address.h:135` · 정의 외 참조 0건(파일 내부 포함) · **안전**
- [x] `df_ByteLength_stimulationStrategy` — `99_eeprom_address.h:136` · 상동 · **안전**
- [x] `df_24bitWordLength_firstPulsePhase` — `99_eeprom_address.h:139` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_firstPulsePhase` — `99_eeprom_address.h:140` · 정의 외 참조 0건. 부수 발견: 여는 괄호 1개·닫는 괄호 2개로 괄호 불균형 상태(참조 시 컴파일 에러 유발) · **안전**
- [x] `df_24bitWordLength_stimulationMode` — `99_eeprom_address.h:142` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_stimulationMode` — `99_eeprom_address.h:143` · 정의 외 참조 0건, 동일한 괄호 불균형 버그 보유 · **안전**
- [x] `df_24bitWordLength_pulsePhaseWidth` — `99_eeprom_address.h:145` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_pulsePhaseWidth` — `99_eeprom_address.h:146` · 정의 외 참조 0건, 괄호 불균형 버그 보유 · **안전**
- [x] `df_24bitWordLength_NumberOfFrequencyBand` — `99_eeprom_address.h:149` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_NumberOfFrequencyBand` — `99_eeprom_address.h:150` · 정의 외 참조 0건, 괄호 불균형 버그 보유 · **안전**
- [x] `df_24bitWordLength_ToneSignalOutputElectrodeIndex` — `99_eeprom_address.h:153` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_ToneSignalOutputElectrodeIndex` — `99_eeprom_address.h:154` · 정의 외 참조 0건, 괄호 불균형 버그 보유 · **안전**
- [x] `df_24bitWordLength_ToneSignalOutputLevel` — `99_eeprom_address.h:157` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_ToneSignalOutputLevel` — `99_eeprom_address.h:158` · 정의 외 참조 0건, 괄호 불균형 버그 보유 · **안전**
- [x] `df_24bitWordLength_StimulusChannelAssignedElectrodIndex` — `99_eeprom_address.h:162` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_StimulusChannelAssignedElectrodIndex` — `99_eeprom_address.h:163` · 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_ReferenceChannelAssignedElectrodIndex` — `99_eeprom_address.h:167` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_ReferenceChannelAssignedElectrodIndex` — `99_eeprom_address.h:168` · 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_CIS_FreqBandOrder` — `99_eeprom_address.h:172` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_CIS_FreqBandOrder` — `99_eeprom_address.h:173` · 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_T_Level_uA` — `99_eeprom_address.h:177` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_T_Level_uA` — `99_eeprom_address.h:178` · 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_C_Level_uA` — `99_eeprom_address.h:182` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_C_Level_uA` — `99_eeprom_address.h:183` · 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_xMinLevel` — `99_eeprom_address.h:187` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_xMinLevel` — `99_eeprom_address.h:188` · 정의 외 참조 0건 · **안전**
- [x] `df_24bitWordLength_xMaxLevel` — `99_eeprom_address.h:192` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_xMaxLevel` — `99_eeprom_address.h:193` · 정의 외 참조 0건 · **안전**
- [x] `df_ByteLength_ProgramData` — `99_eeprom_address.h:198` · 짝인 워드-길이 값은 외부 사용되나(§비고 1) 이 값 자체는 파일 내부(202행)에서만 참조, 외부 소비처 0건 · **안전**
- [x] `df_24bitWordLength_isd_DB` — `99_eeprom_address.h:202` · 파일 내부에서만 연쇄 참조(203행), 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_isd_DB` — `99_eeprom_address.h:203` · 파일 내부에서만 연쇄 참조(226-229행), 외부 소비처 0건 · **안전**
- [x] `df_24bitWordLength_TestVector_AGC_input` — `99_eeprom_address.h:206` · 파일 내부에서만 연쇄 참조, 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_TestVector_AGC_input` — `99_eeprom_address.h:207` · `#ifdef TestVector_AGC_input_Using`(241행, 정의처 없음 — 상시 미정의) 죽은 분기 안에서만 연쇄 참조 · **안전**
- [x] `df_24bitWordLength_TestVector_FFT_input` — `99_eeprom_address.h:209` · 파일 내부에서만 연쇄 참조, 외부 소비처 0건 · **안전**
- [x] `df_ByteLength_TestVector_FFT_input` — `99_eeprom_address.h:210` · 파일 내부에서만 연쇄 참조(244·248행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_FFT_PassBin_index` — `99_eeprom_address.h:217` · EEPROM 절대주소 계산용, 파일 내부에서만 연쇄 참조(222행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_FFT_windowCoeff` — `99_eeprom_address.h:222` · 파일 내부에서만 연쇄 참조(226·243·248행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_ISD_DB_4` — `99_eeprom_address.h:226` · 파일 내부에서만 연쇄 참조(227·236행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_ISD_DB_3` — `99_eeprom_address.h:227` · 파일 내부에서만 연쇄 참조(228·235행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_ISD_DB_2` — `99_eeprom_address.h:228` · 파일 내부에서만 연쇄 참조(229·234행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_ISD_DB_1` — `99_eeprom_address.h:229` · 파일 내부에서만 연쇄 참조(233·239행), 외부 소비처 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_User_1_SettingValues` — `99_eeprom_address.h:233` · 정의 외 참조 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_User_2_SettingValues` — `99_eeprom_address.h:234` · 정의 외 참조 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_User_3_SettingValues` — `99_eeprom_address.h:235` · 정의 외 참조 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_User_4_SettingValues` — `99_eeprom_address.h:236` · 정의 외 참조 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_BatteryCalibraionData` — `99_eeprom_address.h:239` · 정의 외 참조 0건 · **안전**
- [x] `df_EERPROM_BaseAddr_AGC_input_TestVector` — `99_eeprom_address.h:243` · `#ifdef TestVector_AGC_input_Using`(상시 미정의) 죽은 분기 안에서만 정의·참조 · **안전**
- [x] `df_EERPROM_BaseAddr_FFT_input_TestVector` — `99_eeprom_address.h:244,248` · 동일 이름이 `#ifdef`(244행, 죽음)/`#else`(248행, 컴파일되는 쪽) 양쪽에 정의되어 있으나 둘 다 정의 외 참조 0건 · **안전**

## `dfu/tdc_dfu_ble_ota.h`

노드06이 "31개"로 집계한 클러스터를 이번 조사에서 실제로 세어보니 32개였다(수치 차이는 노드06의 어림 집계로 추정, §비고 2). 전부 `tdc_dfu_ble_ota.c`가 매직넘버(9, 13, `p_packet[4]` 등)로 직접 처리하고 있어 정의만 있고 참조가 없다.

- [ ] `RECV_PKT_SIZE_OTA_WRITE` — `tdc_dfu_ble_ota.h:69` · 정의 외 참조 0건, 실제 파싱은 매직넘버 사용 · **안전**
- [ ] `RECV_PKT_SIZE_OTA_READ` — `tdc_dfu_ble_ota.h:70` · 상동 · **안전**
- [ ] `RECV_PKT_SIZE_OTA_SIZE` — `tdc_dfu_ble_ota.h:71` · 상동 · **안전**
- [ ] `RESP_PKT_SIZE_OTA_WRITE` — `tdc_dfu_ble_ota.h:77` · 정의 외 참조 0건. 실제 write 응답은 9바이트를 보내 이 매크로(9)와 값은 같지만 코드가 참조하지 않음 · **안전**
- [ ] `RESP_PKT_SIZE_OTA_READ` — `tdc_dfu_ble_ota.h:78` · 정의 외 참조 0건, 실제 read 응답 크기(9바이트)와 값(8)이 어긋나 있어 애초에 쓰인 적 없는 것으로 추정 · **안전**
- [ ] `RESP_PKT_SIZE_OTA_SIZE` — `tdc_dfu_ble_ota.h:79` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_HEADER` — `tdc_dfu_ble_ota.h:85` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_DATA_INDEX_0` — `tdc_dfu_ble_ota.h:86` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_DATA_INDEX_1` — `tdc_dfu_ble_ota.h:87` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_DATA_INDEX_2` — `tdc_dfu_ble_ota.h:88` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_SLOT_NUM` — `tdc_dfu_ble_ota.h:89` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_FILE_TYPE_0` — `tdc_dfu_ble_ota.h:90` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_FILE_TYPE_1` — `tdc_dfu_ble_ota.h:91` · 정의 외 참조 0건 · **안전**
- [ ] `RECV_PKT_IDX_OTA_OPTION` — `tdc_dfu_ble_ota.h:92` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_HEADER` — `tdc_dfu_ble_ota.h:98` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_DATA_INDEX_0` — `tdc_dfu_ble_ota.h:99` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_DATA_INDEX_1` — `tdc_dfu_ble_ota.h:100` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_DATA_INDEX_2` — `tdc_dfu_ble_ota.h:101` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_SLOT_NUM` — `tdc_dfu_ble_ota.h:102` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_FILE_TYPE_0` — `tdc_dfu_ble_ota.h:103` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_FILE_TYPE_1` — `tdc_dfu_ble_ota.h:104` · 정의 외 참조 0건 · **안전**
- [ ] `RESP_PKT_IDX_OTA_OPTION` — `tdc_dfu_ble_ota.h:105` · 정의 외 참조 0건 · **안전**
- [ ] `ST__OTA_CONTROL_PACKET` (구조체 타입) — `tdc_dfu_ble_ota.h:115-119` · 이 타입으로 선언된 변수/인스턴스가 코드베이스에 0건 · **안전**
- [ ] `OTA_RETRY_MAX` — `tdc_dfu_ble_ota.h:121` · 정의 외 참조 0건 · **안전**
- [ ] `CI_OTA_STATE_FALL_BACK` — `tdc_dfu_ble_ota.h:125` · 정의 외 참조 0건(`CI_OTA_STATE_E` enum 값) · **안전**
- [ ] `CI_OTA_STATE_OTA_UPDATE` — `tdc_dfu_ble_ota.h:126` · 정의 외 참조 0건 · **안전**
- [ ] `CI_OTA_UPDATE_STATE_IDLE` — `tdc_dfu_ble_ota.h:131` · 정의 외 참조 0건(`CI_OTA_UPDATE_STATE_E` enum 값) · **안전**
- [ ] `CI_OTA_UPDATE_STATE_BOOT_TRY` — `tdc_dfu_ble_ota.h:132` · 정의 외 참조 0건 · **안전**
- [ ] `CI_OTA_UPDATE_STATE_BOOT_OK` — `tdc_dfu_ble_ota.h:133` · 정의 외 참조 0건 · **안전**
- [ ] `CI_OTA_UPDATE_STATE_BOOT_CONFIRM` — `tdc_dfu_ble_ota.h:134` · 정의 외 참조 0건 · **안전**
- [ ] `CI_OTA_RESULT_SUCCESS` — `tdc_dfu_ble_ota.h:139` · 정의 외 참조 0건(`CI_OTA_RESULT_E` enum 값) · **안전**
- [ ] `CI_OTA_RESULT_FAIL` — `tdc_dfu_ble_ota.h:140` · 정의 외 참조 0건 · **안전**

## `dfu/tdc_dfu_ble_boot.h`

- [ ] `RECV_PKT_SIZE_BOOT_INFO` — `tdc_dfu_ble_boot.h:28` · 정의 외 참조 0건. 실제 수신 파싱은 `p_packet[...]` 원시 인덱스로 처리, 대응 `RESP_PKT_SIZE_BOOT_INFO`는 실사용(대조군) · **안전**
- [ ] `RECV_PKT_SIZE_BOOT_SELECT` — `tdc_dfu_ble_boot.h:29` · 상동 · **안전**

## `ble/tdc_ble_protocol.h`

- [x] `en__BLE_COMM_COMMAND_connecteLogDate` — `tdc_ble_protocol.h:35` · 값 0x31, `tdc_ble_communication_step()` 커맨드 라우팅 어디에도 이 범위 매칭이 없어 정의 외 참조 0건 · **안전**
- [x] `en__BLE_COMM_COMMAND_REPlY_ERROR_BLE` — `tdc_ble_protocol.h:37` · 값 0xF0, 동일 값을 갖는 `en__ERROR_Response`(`tdc_ble_protocol.h:23`)가 `sys/tdc_sys_error.c:164`에서 실사용 중이라 이름만 다른 죽은 중복 · **안전**
- [x] `PayloadSize_ExternalDeviceInfo_Size_byte` — `tdc_ble_protocol.h:122` · 정의 외 참조 0건(인근 `Max_ImpedanceReturnDataSize`는 실사용, 대조군) · **안전**

## `hal/tdc_hal_dio.h`

DIO CFG 매크로 10종 중 `OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT`(30행, `tdc_hal_dio.c:73`에서 실사용) 1개만 생존하고 나머지 9개는 사장이다. NORMAL 4종은 미참조 또는 주석 속에만 있고, LP 5종은 `tdc_hal_dio_configure_sleep()`의 `#if 0`(`tdc_hal_dio.c:84-105`, 직접 확인) 안에서만 참조된다.

- [x] `OTE_1_5_GEN_DIO_CFG_NORMAL_EARPIECE_DET_N` — `tdc_hal_dio.h:26` · 전역 참조 0건(주석 포함 전무) · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_NORMAL_CHG_DET_N` — `tdc_hal_dio.h:27` · 유일한 참조가 `tdc_hal_dio.c:70`의 주석 처리된 줄뿐 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_NORMAL_CASE_DET` — `tdc_hal_dio.h:28` · 전역 참조 0건 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_NORMAL_CASE_OPEN` — `tdc_hal_dio.h:29` · 전역 참조 0건 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_LP_EARPIECE_DET_N` — `tdc_hal_dio.h:33` · 유일한 참조(`tdc_hal_dio.c:86`)가 `#if 0`(84-105행) 안 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_LP_CHG_DET_N` — `tdc_hal_dio.h:34` · 유일한 참조(`tdc_hal_dio.c:89`)가 `#if 0` 안 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_LP_CASE_DET` — `tdc_hal_dio.h:35` · 유일한 참조(`tdc_hal_dio.c:92`)가 `#if 0` 안 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_LP_CASE_OPEN` — `tdc_hal_dio.h:36` · 유일한 참조(`tdc_hal_dio.c:95`)가 `#if 0` 안 · **안전**
- [x] `OTE_1_5_GEN_DIO_CFG_LP_ACCEL_INT` — `tdc_hal_dio.h:37` · 유일한 참조(`tdc_hal_dio.c:98`)가 `#if 0` 안 · **안전**

## `drv/tdc_drv_isl9122.h`

- [x] `TDC_DRV_ISL9122_REG_INTFLAG_MASK` — `tdc_drv_isl9122.h:11` · 유일한 참조(`tdc_drv_isl9122.c:100`)가 `tdc_drv_isl9122_reset()`의 `#if 0`(92-101행) 안 · **안전**
- [x] `TDC_DRV_PMIC_DEFAULTVALUE_INTFLAG_MAS` — `tdc_drv_isl9122.h:14` · 유일한 참조(`.c:96`)가 동일 `#if 0` 안 · **안전**
- [x] `TDC_DRV_PMIC_BITPOSITION_OC_FAULT_MODE` — `tdc_drv_isl9122.h:19` · 유일한 참조(`.c:96,98`)가 동일 `#if 0` 안 · **안전**
- [x] `TDC_DRV_PMIC_BITLENGTH_OC_FAULT_MODE` — `tdc_drv_isl9122.h:20` · 유일한 참조(`.c:96`)가 동일 `#if 0` 안 · **안전**
- [x] `TDC_DRV_PMIC_BITPOSITION_TYPE` — `tdc_drv_isl9122.h:16` · 죽은 코드 안에서조차 참조 0건(완전 고아) · **안전**
- [x] `TDC_DRV_PMIC_BITLENGTH__TYPE` — `tdc_drv_isl9122.h:17` · 상동 · **안전**

## `pwr/tdc_pwr_clock.h` · `tdc_pwr_clock.c`

- [x] `tdc_pwr_clock_backup_t` (구조체 타입) — `tdc_pwr_clock.h:33-45` · 이 타입으로 선언된 변수가 코드베이스에 0건 · **안전**
- [x] `bootloader_boot_information` (구조체 타입) — `tdc_pwr_clock.h:52-95` · 이 타입으로 선언된 변수가 코드베이스에 0건(부트로더와 오프셋을 공유하는 실제 활성 타입은 `dfu/tdc_dfu_sdk_boot.h`의 `tdc_boot_status_t`로 별개) · **안전**
- [x] `TDC_PWR_CLOCK_NORMAL` — `tdc_pwr_clock.h:27` · 정의 외 참조 0건 · **안전**
- [x] `TDC_PWR_CLOCK_STANDBY` — `tdc_pwr_clock.h:28` · 정의 외 참조 0건 · **안전**
- [x] `TDC_PWR_CLOCK_PREHEAT` — `tdc_pwr_clock.h:29` · 정의 외 참조 0건 · **안전**
- [x] `NVM_BOOT_INFO_OFFSET` — `tdc_pwr_clock.c:7` · 정의 외 참조 0건(짝인 무호출 함수 `bootloader_CRC_calc`·미참조 배열 `s_boot_info`용) · **안전**
- [x] `NVM_BOOT_INFO_OFFSET_OCTETS` — `tdc_pwr_clock.c:8` · 정의 외 참조 0건 · **안전**
- [x] `NVM_BOOT_INFO_SIZE` — `tdc_pwr_clock.c:10` · 유일한 참조(`.c:16`)가 미참조 배열 `s_boot_info[NVM_BOOT_INFO_SIZE]` 선언용 · **안전**
- [x] `NVM_BOOT_INFO_SIZE_OCTETS` — `tdc_pwr_clock.c:11` · 정의 외 참조 0건 · **안전**

## `main.h`

- [x] `OTE_1_5_GEN_TEST_WITHOUT_CFX` — `main.h:37` · 정의 후 `#if` 등 어디에도 참조되지 않음 · **안전**
- [ ] `DEV_FW_VER_BETA` — `main.h:29` · `devFwVer_type = DEV_FW_VER_RELEASE`(`main.c:119`)만 실사용, 이 값은 참조 0건. 다만 개발자가 손으로 골라 쓰는 빌드 라벨 상수라 "사장"과 "미사용 옵션"의 경계가 애매함 · **주의**
- [ ] `DEV_FW_VER_RND` — `main.h:31` · 상동 · **주의**
- [ ] `DEV_FW_VER_HW_TEST` — `main.h:32` · 상동 · **주의**
- [ ] `DEV_FW_VER_SW_TEST` — `main.h:33` · 상동 · **주의**

## `fs/tdc_fs.h`

- [x] `SND_FATFS_MOUNT_OPTION` — `tdc_fs.h:42` · 정의 외 참조 0건(형제 매크로 `TDC_FS_MOUNT_OPTION`이 실사용, 대조군) · **안전**
- [x] `TDC_FS_LOGICAL_DRIVE_NUM_FOR_BOOT_STATUS` — `tdc_fs.h:34` · 정의 외 참조 0건(형제 매크로 `TDC_FS_LOGICAL_DRIVE_NUM`이 실사용, 대조군) · **안전**

---

## 비고

1. **노드10 보고 정정(중요)**: `board/99_eeprom_address.h`를 "매크로 전량 미사용(약 40종)"이라 보고했으나, 이번 조사에서 파일이 정의하는 매크로 전체를 이름별로 재검증(grep)한 결과 `df_24bitWordLength_ISD_info`(`fs/tdc_fs.c:898`, `cfx_link/tdc_shm_addr.h:64,117`) · `df_24bitWordLength_UserSettingValues`(`tdc_shm_addr.h:118`) · `df_24bitWordLength_mappingDate`(`cfx_link/tdc_cfx_eeprom_read.c:49`) · `df_24bitWordLength_ProgramData`(`tdc_shm_addr.h:120`) 4개는 실제로 파일 외부에서 쓰이고 있음을 확인했다. 이 4개는 목록에서 제외했고, 이 목록의 나머지 63개는 개별적으로 전량 재검증을 거쳤다. 이번 과제 지침이 예고한 "그렙 함정"이 이번엔 반대 방향(로그가 "미사용"이라 했으나 실은 사용 중)으로 나타난 사례라 별도로 기록한다.
2. **노드06 수치 불일치**: `dfu/tdc_dfu_ble_ota.h` 클러스터를 노드06은 "31개"로 집계했으나 실제로 헤더를 열어 세어보니 32개였다. 근거는 못 찾았고 노드06의 어림 집계로 추정한다(수치 차이 자체가 내용에 영향 없음 — 32개 전부 이번 목록에 개별 반영함).
3. **범위 밖 제외 확인**: 아래는 이번 목록에 넣지 않았다.
   - `hal/tdc_hal_i2c_cfx.h`의 `TDC_HAL_I2C_CFX_CMD_WRITE`/`_CMD_READ`, `hal/tdc_hal_i2c_state.h`의 매크로 11종(`TDC_HAL_I2C_ERR_*` 4·`TDC_HAL_I2C_STATE_TX/RX_*` 6·`TDC_HAL_I2C_WAITING_TIME_MS` 1) — 전부 지침 4항이 지정한 모듈 통째 제거 대상 `tdc_hal_i2c_cfx`에 종속된 매크로라 그 모듈 제거 섹션에서 함께 처리하는 것이 맞다고 판단해 제외했다. `TDC_HAL_I2C_STATE_IDLE`(`tdc_hal_i2c_state.h:13`)만 유일하게 이 파일에서 "쓰이는" 매크로이나, 그 소비처(`tdc_hal_i2c_cfx.c:45,75`) 자체가 이 죽은 모듈 안이라 함께 정리 대상이다.
   - `sys/tdc_sys_error.h:136`의 `FPGA_OR_ISD_ErrorFlag`(노드03) — 구조체 필드 미사용 사례이나 매크로·타입·enum이 아니라 구조체 "필드"라 이 목록의 범위(매크로/상수/타입/enum) 밖으로 판단했다. 함수·변수 섹션 담당 쪽에서 다뤄야 할 항목일 수 있어 교차 확인을 권장한다.
   - `board/processorDirective.h`의 `EEPROM_LSK_Error`(91-93행)·`disable_Tx_PowerControl`(95-97행), `conneded_ISDCheck_byForwardPath`(99-103행 `#else`, 정의 자체가 없음) — 이들은 "정의됐지만 소비처가 없는 매크로"가 아니라 "설정매크로가 상시 참/거짓이라 다른 코드의 분기가 죽는" 사례(소비처가 실제 존재, `isd/tdc_isd_init_fpga.c` 등)라 죽은 전처리 블록 섹션 소관으로 보고 제외했다.
   - `board/DIO_PIN_Config.h`의 `UART_DIO_PIN_CFG`, `hal/tdc_hal_uart.h` 전체 매크로/타입(`TDC_HAL_UART_*`·`FS_MEM_UART_BUF_LEN`·`DebugMode_t`·`debug_agc_t` 등), `board/Board_OTE_ver1_5.h`의 UART용 DIO 정의 — 전부 지침 4항 "UART 관련 전 항목(별도 게이트)"에 해당해 제외했다. 단, `board/processorDirective.h`의 오디오-경유-UART 매크로 5종(`AudioInputSignal_Tx_usingUART`·`CFX_UART_USING_ISR`·`CFX_UART_BAUD_RATE`·`CFX_UART_RX_ENABLE`·`UART_bufferLength_forAudio`)은 과제 담당 상세에서 명시적으로 이 목록에 포함하라고 지정해 위 본문에 반영했다.
   - `ble/tdc_ble_mapping.c`의 `en__mapping_testStimulation`(0x90) 처리 분기(`#ifndef RELEASE`)와 `RELEASE` 매크로 자체 — `RELEASE`는 소비처가 있는(상시 참) 설정 매크로라 "미사용 매크로"가 아니라 "죽은 분기" 사례이며, 06번·09번 로그가 지적한 대로 `isd/tdc_isd_map_test_stim.c` 생존 여부와 얽혀 있어 이 목록(A) 범위 밖으로 판단했다.
   - `board/FPGA_ver2_7_0.h`의 백텔 캘리브레이션 실험 이력 값(`backterConfiguration_resetValue` 관련, 09번 로그)과 `board/electrodeMapping.c`의 `#if 1`/`#else` 보드 분기 — 매크로 정의 자체의 제거가 아니라 코드 분기/데이터 값 문제이며, 특히 electrodeMapping은 10번 로그가 "명백히 사용 중"으로 재판정했으므로 애초에 제외 대상이다.
4. **불가침 확인**: 이번 목록의 어떤 항목도 `cfx_cm3_sharedMemoryAll` 구조체 자체, `*_IRQHandler`, SDK `Sys_*`/`SYS_*`, `isd/tdc_isd_map_*` 5대 FSM, `lib/` 원본 API를 건드리지 않는다. `tdc_shm_addr.h`의 81개 오프셋 매크로는 `cfx_cm3_sharedMemoryAll`의 필드 배치를 계산하는 보조 매크로일 뿐 그 구조체 정의 자체가 아니므로 불가침 대상이 아니라고 판단했으나, ABI 문서 역할 때문에 위험도를 신중히 매겨 보류 권고로 두었다(위 그룹 항목 참고).
5. **판단이 갈리는 항목**: `main.h`의 `DEV_FW_VER_*` 4종과 `processorDirective.h`의 `CFX_UART_*`/`UART_bufferLength_forAudio` 4종은 원 로그가 이미 "판단 보류"로 남긴 항목이라 그대로 보류 권고([ ])로 표시했다. `tdc_shm_addr.h` 81개 그룹도 동일하게 보류 권고로 두었다 — 코드 관점에서는 삭제해도 안전(컴파일·동작 영향 없음)하지만, 문서·ABI 가치 때문에 최종 판단은 은수님 몫으로 남긴다.
