# B. 죽은 전처리 블록 · #if 0 · 영구 거짓 분기

**총 53건** (제거 권고 34건 · 보류 권고 19건)

> 근거: 주력 로그 `에이전트-로그/09_죽은전처리-블록.md`(55건 원보고) + 보조 `03`·`04`·`05`·`06`·`08` 교차확인. 범위 규칙(UART 전체·FS_MEM_UART·모듈 통째 제거 5건·isd 5대 FSM 불가침)에 따라 일부는 제외하고 `TDC_HAL_I2C_USING_ISR`(노드03/08)·`DisalbedBackTel`(노드05) 관련 실제 죽은 분기를 소스에서 직접 `Grep`/`Read`로 확인해 추가했다. 라인 번호는 전량 현재 소스(`Read`/`Grep`)로 재확인했다.

## `board/processorDirective.h`

- <s>`DebuggingMode_*` 매크로 13종 — `processorDirective.h:24-82`</s> → **[제거목록-A-매크로.md](제거목록-A-매크로.md) 에서 개별 13항목으로 다룹니다.** 중복이라 이 목록에서 뺐습니다 (여기서 체크하지 마세요)
- [x] `EEPROM_LSK_Error` / `disable_Tx_PowerControl` 정의 스텁 — `processorDirective.h:91-97`(각 3줄) · 둘 다 `#if 0` 안 정의라 영구 미정의. `disable_Tx_PowerControl`은 `!defined()`로 쓰여 존치 로직은 그대로 살아있으므로 정의 스텁만 제거해도 동작 불변 · **주의**

## `hal/tdc_hal_i2c.c`

- [x] `TDC_HAL_I2C_USING_ISR` 상시정의로 죽은 `#ifndef`/유령함수 3곳 — `tdc_hal_i2c.c:164-165,231-237,295-301`(합 16줄) · `processorDirective.h:155`와 `tdc_hal_i2c.h:9` 양쪽에서 무조건(가드 없이) 정의되어, `#else` 분기의 `tdc_hal_i2c_comm()`(정의될 수 없는 유령함수)과 `i2c_state_WritingDone`/`ReadingDone` 폴링 처리 2곳이 영구 사장 · **주의**

## `isd/tdc_isd.c`

- [x] `conneded_ISDCheck_byForwardPath` 죽은 분기 — `tdc_isd.c:304-323`(20줄) · `processorDirective.h:99-103` 구조상 `byForwardPath`가 영구 미정의라 `#if defined(...)` 분기 상시거짓, `#elif ...byISDPower`만 채택 · **주의**
- [x] 무조건 backtel 호출 dead-alt — `tdc_isd.c:224-244`(21줄) · OTA 링크체크 예외처리가 있는 `#else`(살아있음)로 대체된 구버전 무조건호출 · **위험**
- [x] 디버그 레지스터 읽기 dead 3곳 — `tdc_isd.c:466-473,557-562,669-675`(합 20줄) · 백텔 카운터 0 에러 시 레지스터 값만 읽고 버리는 `#if 0` 디버그 코드, 3곳 동일 패턴 · **위험**
- [x] `DisalbedBackTel` 미정의로 죽은 `#else` 분기 2곳 — `tdc_isd.c:525-529,712-716`(합 10줄) · 오탈자 매크로가 트리 전체에서 `#define`된 적이 없어(노드05) `#ifndef DisalbedBackTel` 분기만 상시 채택, `#else`(연결확인 카운터 클리어 대안 로직) 영구 사장 · **주의**

## `isd/tdc_isd_fpga.c` · `isd/tdc_isd_fpga.h`

- [x] `tdc_isd_fpga_get_written_value` / `tdc_isd_fpga_update_written_value` 전체 — `tdc_isd_fpga.c:34-107`, `tdc_isd_fpga.h:34-37` · 함수 정의·선언 전체가 `#if 0` 안이고, 유일한 호출부 2곳(`isd_map_ecap.c:540,630`)도 각각 다른 `#if 0` 안이라 실질 호출 0건 · **주의**

## `isd/tdc_isd_init.c`

- [x] `EEPROM_LSK_Error` 분기 — `tdc_isd_init.c:204-223`(20줄) · `processorDirective.h:91-93`에서 영구 미정의라 LSK/PPSK 레지스터 설정 코드 상시 스킵 · **위험**
- [x] ID 바이트순서 dead-alt / `vtg_Lock_state` 강제override — `tdc_isd_init.c:471-476`(6줄), `:926-928`(3줄) · 내부기 ID 역순 조합(채택) vs 다른 조합식(사장), 전압락 상태 강제 0 오버라이드 완전 사장 · **위험**

## `isd/tdc_isd_init_fpga.c`

- [x] `EEPROM_LSK_Error` 분기 — `tdc_isd_init_fpga.c:554-573`(20줄) · 위와 동일 매크로 영구 미정의로 LSK/PPSK 레지스터 설정 스킵 · **위험**
- [x] 디버그 debounce 카운터 — `tdc_isd_init_fpga.c:648-652`(5줄) · 백텔 카운터 0 에러 시 debounce 실패 매크로 호출만 남은 순수 `#if 0` 디버그 블록(`EEPROM_LSK_Error`와 무관한 별도 블록) · **위험**
- [x] `DisalbedBackTel` 미정의로 죽은 `#else` 분기 — `tdc_isd_init_fpga.c:665-669`(5줄) · `#ifndef DisalbedBackTel` 상시 채택으로 `#else`(case 275 강제 상태전이) 영구 사장 · **주의**

## `isd/tdc_isd_stim_para_setting.c`

- [x] 바이폴라 기준전극 구현 dead-alt — `tdc_isd_stim_para_setting.c:484-490`(7줄) · `electrodeMap[]` 미적용 구버전(사장) vs 적용 버전(채택) · **위험**

## `main.c`

- [x] 데이터/BSS 섹션 로드 dead-alt — `main.c:245-249,263-267`(합 10줄) · data 섹션은 포인터 증분 루프(채택) vs `memcpy`(사장), bss 섹션은 캐스트 방식만 다른 `memset` 2종(사장/채택) - 순수 구현 차이 · **주의**(부팅 크리티컬 경로, 제거 후 부팅 재검증 필요)
- [x] 트리비얼 `#if 1`(내용 없음, else 없음) — `main.c:122-123`, `:1112-1121` · `#else` 자체가 없어 "죽은 분기"는 아니나 조건부 의미가 없는 wrapping, 가독성 정리 대상 · **안전**
- [ ] 리셋 전 LED 시퀀스 디버그 3곳 — `main.c:943-955,978-985,1042-1052` · RTT 프린트만 남기고 LED 점등 시퀀스는 생략한 현재형 - 하드웨어 디버깅 시 재활성 가능성 있어 존치 권장 · **안전**
- [ ] `TDC_TOUCH_DEBUG_PRINT_ENABLE` 분기 — `main.c:930-966`, `touch/tdc_touch.c:281-320` · `touch_config.h:21`이 `#ifndef` 가드라 빌드 옵션으로 재정의 가능한 구조 - "영구 거짓" 기준에 엄밀히 부합하지 않아 판단보류(노드09 자체도 보류) · **주의**

## `sys/tdc_sys_init.c`

- [x] TX PMIC 전용 테스트 코드 — `tdc_sys_init.c:371-441`(71줄) · 주석 "오직 TX PMIC 테스트를 위한 코드"로 용도 명시 - 제조/실험용 존치 여지 · **주의**
- [x] LSAD 안정화 대기 — `tdc_sys_init.c:476-493`(18줄) · "더 이상 EZ가 배터리 측정하지 않음" 주석과 정합, LSAD 배터리 측정 경로 폐기 방향과 일치(별도 섹션의 `tdc_pwr_lsad` 모듈 통째 제거와 연동 처리 권장) · **주의**

## `sys/tdc_sys_error.c`

- [x] 디버그 브레이크포인트 헬퍼 — `tdc_sys_error.c:62-67`(6줄) · 특정 `__LINE__` 값과 비교해 중단점을 걸기 위한 의도적 디버그 스캐폴딩 - 존치 권장 · **안전**

## `sys/tdc_sys_control.c`

- [x] `NRF_adv_powerMode()` 완전사장 함수 — `tdc_sys_control.c:113-121`(9줄) · 함수 전체가 `#if 0`, 전역 호출부 0건, 참조하는 `ENABLE_NRF_ADV_LowPower` 매크로도 이 함수 안이 유일 · **안전**

## `touch/tdc_touch.c`

- [ ] 디버그 프린트/LED 2곳 — `tdc_touch.c:294-306,315-317` · `main.c`의 리셋 전 LED 디버그와 동일 패턴(파일 분리 전 복제로 추정) - 동일 판단으로 존치 권장 · **안전**

## `stim/tdc_stim_indicator.c`

- [x] 구현교체 dead(`tdc_stim_indicator_set_level_255` 신·구 공존) — `tdc_stim_indicator.c:73-125`(53줄) · 구버전이 참조하는 `getCalculatedStimulationIndcatorLevel()`은 CM3 전체에 이 한 줄 외 존재하지 않아 되살려도 링크 실패 확정 - 되살릴 여지 없음, 제거 대상 · **주의**

## `stim/tdc_stim_para_cal.c`

- [x] 채널수 계산 구현교체 dead — `tdc_stim_para_cal.c:68-97`(28줄) · 프레임수 기반 반복계산(구, 사장) vs 나눗셈 1회 계산(신, 채택) · **주의**
- [ ] `Df_MaxDeliveryChargeLimitation` 매크로명 불일치로 영구비활성된 전하량 초과 검사 — `tdc_stim_para_cal.c:216-225` · 실제 정의된 매크로는 `Df_MaxDeliveryChargeLimitationKKK`(`processorDirective.h:112`, 동결 목록 소속)라 철자 불일치로 `#ifdef`가 상시 거짓 - 안전검사 무력화 상태라 단순 삭제가 아니라 은수님 확인 필요 · **위험**

## `pwr/tdc_pwr_battery.c`

- [x] 구 배터리 경계전압 — `tdc_pwr_battery.c:134-140`(6줄) · 구 전압분배 계수 기반 값(사장) vs 신 실측 기반 값(채택, 주석에 실측치 명시) · **주의**

## `hal/tdc_hal_dio.c`

- [x] LED 부팅점등 dead-alt — `tdc_hal_dio.c:33-42` · "기존 방식"(전부 OFF, 채택) vs "부팅시간이 길어서 임의로 LED 켜놓기"(실험적 대안, 사장) · **안전**
- [x] FPGA 3.3V/1.2V 핀 설정 dead — `tdc_hal_dio.c:48-53`(6줄) · "1.5세대 rev1p2 이전" 핀 정의에 대응하는 초기화 코드, 보드 리비전 변경으로 사장 · **주의**
- [x] 절전모드 DIO 설정 dead — `tdc_hal_dio.c:84-105`(22줄) · 이어피스/차저/케이스/가속도센서 절전 인터럽트 설정 전부 사장 - 가속도계는 폐기 확정이나 케이스 뚜껑 오픈 감지는 향후 재사용 여부 미확인(노드03 §4) · **주의**

## `hal/tdc_hal_spi.c`

- [x] DMA/SPI 초기화 순서 dead 5곳 — `tdc_hal_spi.c:126-128,200-204,264-270,282-308,321-327` · SPI/DMA 초기화·클리어 타이밍 대안 구현, `L200`은 "CS_RISE에서 초기화하도록 수정" 사유 기재 · **주의**

## `fs/tdc_fs_map.c`

- [ ] CRC+AES128 미적용 legacy read/write 8곳 — `tdc_fs_map.c:13-18,27-32,39-44,53-65,73-79,89-94,104-109,120-132` · read/write 함수 8개 전부 "CRC+AES128 검증판"(채택) vs "평문 그대로"(사장) 동일 패턴 - 매핑데이터 구포맷을 calibration 등 타 프로젝트가 아직 읽을 가능성을 배제 못해 보류 · **주의**
- [ ] 대량 디버그 덤프 — `tdc_fs_map.c:165-321`(157줄) · ISD 전체 맵데이터를 RTT로 통째 덤프하는 디버그 유틸리티 - 존치 권장 · **안전**
- [x] 작성자 지정 삭제대상 디버그 변형 — `tdc_fs_map.c:743-763`(17줄) · 주석 "디버깅을 위해서 넣은 구문이므로, 테스트 후 `df_minAudioForLogarithm` 하나만 남기면 됨" - 작성자 본인이 제거 지시 · **안전**

## `fs/tdc_fs_fft.c`

- [ ] 디버그 덤프 — `tdc_fs_fft.c:43-56`(14줄) · FFT pass-bin 값 전체를 RTT로 출력하는 디버그용 - 존치 권장 · **안전**

## `fs/tdc_fs.c`

- [ ] hex 덤프 디버그 6곳 — `tdc_fs.c:245-267,384-449,516,611,659,723` · 암복호화 전/후 데이터를 RTT hex로 통째 찍는 디버그 유틸, read/write 각 함수마다 반복 - 검증용 존치 권장 · **안전**

## `drv/tdc_drv_isl9122.c`

- [x] PMIC 레지스터 설정 dead-alt — `tdc_drv_isl9122.c:92-107`(16줄) · `OC_FAULT_MODE` 비트연산 버전(구, 사장) vs `CONV_CFG` 쓰고 재확인하는 버전(신, 채택) · **주의**

## `board/FPGA_ver2_7_0.h`

- [x] IO_MUX 죽은 enum — `FPGA_ver2_7_0.h:30-32`(3줄) · 이 값을 쓰는 필드 자체가 `isd_fpga.c` switch-case에서도 이미 주석처리 - IO_MUX 기능 자체가 미구현으로 추정 · **안전**
- [x] 백텔 캘리브레이션 값 이력 — `FPGA_ver2_7_0.h:117-131`(15줄) · 살아있는 값 `0x2D` 주변 7개 이력값이 주석으로 남아있고 그중 2개는 "백텔 안들어옴" 현장실패 개체번호 기록 - **삭제 금지**, 존치 또는 별도 문서 이관 · **위험**

## `board/electrodeMapping.c`

- [ ] `#if 1`/`#else` 전극-지그 배선 분기 — `electrodeMapping.c:2-9` · 노드10이 `electrodeMap[32]`가 5개 자극 제어 파일에서 20회 이상 실사용 중임을 확인해 **명백히 사장 아님**으로 재확정 판정(노드09 자체 판정과 방향 일치, 노드10이 더 확정적). `#else`(임피던스 측정 지그용 대체 배선)도 제조/시험 모드 스위치로 의도적 보존 · **위험**

## `ble/tdc_ble_remote.c`

- [x] BLE 리모콘 패스키 인증 영구 비활성 — `tdc_ble_remote.c:724-741` · `remocon_passkey_Match`를 먼저 `true`로 고정한 뒤 실제 비교 루프를 `#if 0`로 비활성화, 로그로 의도 명시("[PASSKEY] ALWAYS PATH OPENED") - 보안기능 의도적 비활성이라 은수님 확인 후 처리 · **위험**
- [x] 텔레코일 강제 비활성 — `tdc_ble_remote.c:291-295` · 사용자가 보낸 텔레코일 설정값을 무시하고 항상 2(꺼짐)로 고정하는 `#if 0`/`#else` - 기능 의도적 비활성으로 사양 확인 필요, 되살릴 여지 있음 · **주의**
- [x] 백텔 동기화 dead — `tdc_ble_remote.c:682-693`(12줄) · 매핑명령 수신 시점의 백텔 동기화 대기 로직, 현재는 이 대기 없이 바로 처리 · **주의**
- [x] `readInfoOfMap` 최신일자 로직 dead — `tdc_ble_remote.c:816-938`(123줄) · 4개 맵의 매핑일자를 비교해 최신 것을 전송하는 구버전 로직 전체, 가장 큰 단일 dead 블록 - 무선 프로토콜 문서 대조 후 제거 권장 · **주의**
- [x] `readUserName` 핸들러 dead — `tdc_ble_remote.c:941-964`(24줄) · `en__remoteControl_readUserName` case 전체 사장, 유저명 조회 커맨드 자체 폐기 추정 · **주의**
- [x] `read_Connected_ISD_id` (명시적 존치) — `tdc_ble_remote.c:1317-1339`(23줄) · 주석 "프로토콜 3.8부터 삭제되었으나 리모콘 에러시 확인 필요" - 작성자가 디버깅 목적 존치 명시 · **안전**

## `ble/tdc_ble_communication.c`

- [x] SPI 루프백 dead — `tdc_ble_communication.c:257-282`(26줄) · 수신 데이터를 그대로 루프백하던 구버전 vs 실제 커맨드 파싱하는 신버전(채택) · **주의**
- [x] Recover 디버깅 (명시적 존치) — `tdc_ble_communication.c:310-315`(6줄) · 주석 "Recover 디버깅 용" - 존치 권장 · **안전**

## `ble/tdc_ble_mapping.c`

- [x] 응답타이밍 구현교체 dead — `tdc_ble_mapping.c:1131-1137`(7줄) · "매핑 명령 실행하는 곳에서 최종 응답을 주게 변경" 주석 - NRF 재시작 전 루프백 방식 폐기 · **주의**
- [x] 디버그 프린트 dead — `tdc_ble_mapping.c:2257-2262`(6줄) · 링크체크 카운터 진입 로그, 단순 디버그 · **안전**
- [x] 텔레코일 강제 비활성 (중복) — `tdc_ble_mapping.c:1350-1354` · `tdc_ble_remote.c:291-295`와 동일 패턴 중복 - 동일 판단(기능 비활성, 사양 확인 필요) · **주의**

## `util/tdc_printf.h`

- [x] VT100/상세정보 3중분기 — `tdc_printf.h:71-119`(49줄) · 바깥 `#if 1`로 VT100 `#else` 영구사장, 안쪽 `#if 0`(상세정보표시)도 사장, `#elif 1`만 채택 · **안전**

## `util/tdc_printf.c`

- [x] RTT 포맷 alt — `tdc_printf.c:6-10`(5줄) · 파일명만 출력하는 구버전(사장) vs 공백 추가 버전(채택) - 순수 포맷 차이 · **안전**

## 비고

### 라인 번호 확인 상태

- 전 항목 라인 번호를 현재 소스 기준 `Grep`/`Read`로 직접 재확인했다(노드09 원 라인번호와 대조 - 대부분 정확히 일치, `isd_init.c` ID 바이트순서 항목만 노드09가 `324-337`로 적었으나 실제 `#else` 사장 구간은 `471-476`이라 바로잡았다).
- `isd/tdc_isd_init_fpga.c:648-652`은 노드09가 `EEPROM_LSK_Error` 후보(후보_18)에 묶어 보고했으나, 실제로는 `EEPROM_LSK_Error`와 무관한 별도 `#if 0`(debounce 카운터) 블록이라 분리해 기록했다.

### 불가침으로 제외한 항목

- `isd/tdc_isd_map_ecap.c`·`tdc_isd_map_impedance.c`·`tdc_isd_map_live.c`·`tdc_isd_map_specific_stim.c`·`tdc_isd_map_data.c` 내부의 `#if 0`/`#else` dead 블록 20여 곳(노드09 후보_20~24, 예: ecap.c 9쌍+대블록 2곳, impedance.c 2쌍, live.c 재연결 로직, specific_stim.c 3곳, data.c 1세대 콜백) — `isd/tdc_isd_map_*` 5대 FSM(+데이터 I/O) 불가침 규칙에 따라 목록에서 전량 제외. 노드09 자신도 "제거 후보에서 제외, 존치+후속 검토 권고"로 명기함.

### 범위 밖(다른 섹션·게이트 담당)으로 제외한 항목

- **UART 게이트**: `processorDirective.h:117-121`(`AudioInputSignal_Tx_usingUART`, 상시거짓·무consumer), `util/tdc_printf.h:34-35`(`TDC_PRINTF_INTERFACE==UART` 영구거짓 분기) — UART 전수조사(노드01) 담당 게이트로 이관.
- **FS_MEM_UART 이관 섹션**: `hal/tdc_hal_uart.h:86-132`(`FS_MEM_UART_T` 내부 `#if 0` 죽은 필드 46줄) + 전용 타입 `DebugMode_t`/`debug_agc_t`/`FS_MEM_DEBUG_AGC_STATE_*`(:48-79, 5개 매크로) — 구조체 자체는 불가침이고 내부 필드도 이관 설계(노드02) 범위와 맞물려 이 섹션에서는 제외.
- **모듈 통째 제거 5건 관련 죽은 분기**:
  - `isd/tdc_isd.c:10-14`, `tdc_isd_map_ecap.c:9-13`, `tdc_isd_map_impedance.c:9-13`, `tdc_isd_map_specific_stim.c:11-15`, `tdc_isd_map_test_stim.c:11-15`의 `#if 0 #include <tdc_hal_i2c_cfx.h> #else #include <tdc_hal_i2c_isd.h>` 5곳(노드09 후보_14) — `tdc_hal_i2c_cfx` 모듈 통째 제거 섹션과 함께 처리.
  - `pwr/tdc_pwr_lsad.h:21-31`(보드변형 dead-alt, Sullivan 1.5 vs Sound1 Test) — `tdc_pwr_lsad` 모듈 통째 제거 섹션 담당.
  - `ble/tdc_ble_mapping.c:1768-1853,2213-2230`의 `#ifndef RELEASE`(영구거짓, `RELEASE`가 `processorDirective.h:22`에서 무조건 정의) 분기 — 유일 호출 대상이 `isd/tdc_isd_map_test_stim.c`(모듈 통째 제거 대상)라 그 섹션과 연동 판단 필요(노드06 발견. 노드05는 이 호출을 "생존"으로 오판했으나 `#ifndef RELEASE` 확인 결과 실제로는 사장 - 교차 확인 요망).

### 판단이 갈리는 항목

- `stim/tdc_stim_para_cal.c:216-225`(`Df_MaxDeliveryChargeLimitation` vs `...KKK` 철자 불일치) — "사장 코드 정리" 성격보다 "자극 전하량 안전검사가 조용히 무력화된 잠재 결함"에 가까워 삭제 대상이 아니라 은수님 확인 대상으로 분류(보류 [ ]).
- 텔레코일 강제비활성 2곳(`ble_remote.c:291-295`, `ble_mapping.c:1350-1354`) — 엄밀히는 "죽은 코드"가 아니라 "의도적 기능 비활성 오버라이드"이지만 노드09가 이 섹션 후보에 포함시켰고 되살릴 여지가 명확해 보류 [ ]로 유지.
- `main.c:930-966` `TDC_TOUCH_DEBUG_PRINT_ENABLE` — `#ifndef` 가드로 빌드 옵션 재정의가 가능해 "소스에서 영구 거짓" 기준에 엄밀히 부합하지 않음(노드09 자체도 판단보류로 분류) - 보류 [ ] 유지.
- `sys/tdc_sys_init.c:476-493`(LSAD 안정화 대기)는 순수 전처리 관점에서는 제거[x] 권고했으나, `pwr/tdc_pwr_lsad` 모듈 통째 제거 여부와 함께 결정하는 것이 안전하다(동일 배터리측정 세대교체 잔재).
