/* TD24A 기준서의 CM3 제어/계산 벡터. 테스트 로직은 제품 함수를 직접 호출한다. */
#if defined(TDC_UNIT_TEST)
#include "tdc_ut_vectors_cm3.h"
#include <string.h>
#include <tdc_shm.h>
#include <tdc_shm_addr.h>
#include <tdc_touch_logic.h>
#include <tdc_pwr_battery.h>
#include <tdc_sys_control.h>
#include <tdc_stim_para_cal.h>
#include <tdc_stim_common.h>
#include <tdc_stim_indicator.h>
#include <tdc_sys_error.h>
#include <tdc_led_output.h>
#include <tdc_isd.h>
#include <tdc_link_control.h>
#include "tdc_ut_coordination_cm3.h"
#include "tdc_ut_state_cm3.h"

extern void tdc_ut_cm3_error_exchange(tdc_sys_error_code_t *state);
extern int tdc_ut_cm3_dac_offset_exchange(int offset);
extern int tdc_ut_cm3_frame_duration_exchange(int duration);
extern int tdc_ut_cm3_last_frame_duration(void);
extern void tdc_ut_cm3_battery_exchange(int state[4]);
extern void tdc_ut_cm3_indicator_exchange(int state[3]);
extern void tdc_ut_cm3_led_exchange(int state[TDC_LED_SRC__MAX + 3]);
extern int tdc_ut_cm3_led_select(void);
extern void tdc_ut_cm3_system_exchange(int state[16]);
extern int tdc_ut_cm3_disconnection_counter(void);
extern int tdc_ut_cm3_poweroff_requested(void);
/* IO owns this drain flag. It is not a multicore/ISR lock. */
extern volatile int g_tdc_ut_io_cm3_done;

int g_tdc_ut_cm3_vector_actual[TDC_UT_CM3_VECTOR_CAPACITY];
int g_tdc_ut_cm3_vector_expected[TDC_UT_CM3_VECTOR_CAPACITY];
unsigned g_tdc_ut_cm3_vector_count;
int g_tdc_ut_cm3_vector_overflow;
volatile int g_tdc_ut_cm3_vector_done;
int g_tdc_ut_cm3_vector_observed;
const tdc_ut_cm3_observer_ops_t *g_tdc_ut_cm3_observer_ops;
static ST__CFX_CM3_SharedMemory_ALL s_tdc_ut_saved_shared;
static int s_tdc_ut_calculation_copy_attempts;
static int s_tdc_ut_calculation_copy_skipped;

/* 복사 직전까지 실제 제품 함수를 실행한다. false인 지역 배열은 초기화되지
 * 않았으므로 값으로 읽지 않고 시도/생략만 기록한다. 성공 계산은 그대로 복사한다. */
static int observe_calculation_copy(int calculation_done)
{
    ++s_tdc_ut_calculation_copy_attempts;
    s_tdc_ut_calculation_copy_skipped = !calculation_done;
    return !calculation_done;
}

static void put(int actual, int expected)
{
    unsigned index = g_tdc_ut_cm3_vector_count;
    if (index >= TDC_UT_CM3_VECTOR_CAPACITY)
    {
        g_tdc_ut_cm3_vector_overflow = 1;
        return;
    }
    g_tdc_ut_cm3_vector_actual[index] = actual;
    g_tdc_ut_cm3_vector_expected[index] = expected;
    g_tdc_ut_cm3_vector_count = index + 1;
}

static void mark(unsigned unit, unsigned case_id, unsigned length)
{
    put(-999, -999);
    put((int)unit, (int)unit);
    put((int)case_id, (int)case_id);
    put((int)length, (int)length);
}

/* Setup: RAM 맵/고정 헤더 상수. Input: 32채널 T/C=100/300uA, 25us.
 * Output: DAC A/A, offset 100uA/44, T/C 0/127.
 * Restore: run_unit이 공유 자료, DAC 레지스터/오프셋, 오류 상태를 복원한다. */
static void prepare_map(void)
{
    ST__CFX_CM3_SharedMemory_mapData *map = tdc_shm_get_pointer_current_map_data();
    map->stimulationPulsePhaseWidth = 25;
    map->numFrequencyBand = 32;
    map->stimulationStrategy = df_stimulationStrategy_CIS;
    map->stimulationIndicatorChannelNum = 1;
    map->stimulationIndicatorAmplitude_uA = 300;
    for (int i = 0; i < 32; ++i)
    {
        map->T_level_uA[i] = 100;
        map->C_level_uA[i] = 300;
    }
}

static void touch_input(tdc_touch_logic_state_t *state, tdc_touch_in_t *input,
                         tdc_touch_out_t *output, uint32_t time, int pressed, int read_ok)
{
    input->now_ms = time;
    input->pressed = pressed != 0;
    input->read_ok = read_ok != 0;
    input->ati_active = false;
    input->ati_error = false;
    tdc_touch_logic_step(state, input, output);
}

/* Setup: 지역 FSM fixture만 사용한다. Input: 기준서의 가상 시각/읽기 실패.
 * Output: one-shot, HOLD/NOT_TOUCH, 회복 요청. Restore: 외부 상태 변경 없음.
 * 센서/워치독은 호출하지 않으며 MCLR은 반환된 요청값으로만 관측한다. */
static void run_touch(void)
{
    tdc_touch_logic_state_t state;
    tdc_touch_in_t input;
    tdc_touch_out_t output;
    tdc_touch_logic_init(&state, 0);
    touch_input(&state, &input, &output, 0, 1, 1);
    mark(9, 1, 3);
    touch_input(&state, &input, &output, 2399, 1, 1); put(output.long_touch, 0);
    touch_input(&state, &input, &output, 2400, 1, 1); put(output.long_touch, 1);
    touch_input(&state, &input, &output, 2401, 1, 1); put(output.long_touch, 0);
    touch_input(&state, &input, &output, 2500, 0, 1);
    touch_input(&state, &input, &output, 2600, 1, 1);
    touch_input(&state, &input, &output, 5000, 1, 1);
    mark(9, 2, 1); put(output.long_touch, 1);
    tdc_touch_logic_init(&state, 0);
    tdc_touch_logic_set_boot_ignore(&state, 0);
    mark(9, 3, 3);
    for (int i = 1; i <= 3; ++i)
    {
        touch_input(&state, &input, &output, (uint32_t)i * 100, 0, 1);
        put(state.boot_ignore, i < 3);
    }
    tdc_touch_logic_init(&state, 0);
    touch_input(&state, &input, &output, 0, 1, 1);
    mark(9, 4, 42);
    for (int i = 1; i <= 21; ++i)
    {
        touch_input(&state, &input, &output, (uint32_t)i * 100, 0, 0);
        put(output.action, i <= 20 ? TDC_TOUCH_ACT_HOLD : TDC_TOUCH_ACT_NONE);
        put(output.curr_state, i <= 20 ? TDC_TOUCH_STATE_TOUCH : TDC_TOUCH_STATE_NOT_TOUCH);
    }
    tdc_touch_logic_init(&state, 0);
    touch_input(&state, &input, &output, 0, 1, 1);
    mark(9, 5, 3);
    touch_input(&state, &input, &output, 30000, 1, 1); put(output.action, TDC_TOUCH_ACT_RESEED);
    touch_input(&state, &input, &output, 60000, 1, 1); put(output.action, TDC_TOUCH_ACT_RE_ATI);
    touch_input(&state, &input, &output, 90000, 1, 1); put(output.action, TDC_TOUCH_ACT_MCLR);
}

/* Setup/input: 독립 counter fixture 3개. Output: 종료 요청/허용 상태/LED 요청.
 * Restore: 실제 함수의 지역 static과 LED 요청 상태를 각각 보관/복원한다. */
static void run_system(void)
{
    int saved_led[TDC_LED_SRC__MAX + 3] = {0};
    tdc_sys_error_code_t errors = {0};
    ST__USB_CONNECTOR charger = {df_Disconnected, df_Disconnected, df_Connected};
    tdc_ut_cm3_led_exchange(saved_led);
    for (int i = 0; i < 3; ++i)
    {
        int saved_system[16] = {0};
        saved_system[5] = i == 0 ? 179999 : 180000;
        saved_system[7] = df_Disconnected;
        saved_system[8] = 1;
        tdc_ut_cm3_system_exchange(saved_system);
        tdc_sys_state_t output = tdc_sys_control_step(errors, charger, 70, false, i == 2, false);
        mark(7, i + 1, 4);
        put(tdc_ut_cm3_disconnection_counter(), i == 0 ? 180000 : 0);
        put(tdc_ut_cm3_poweroff_requested(), i == 1);
        put(output.enable_ISD, i != 1);
        put(tdc_led_get_request(TDC_LED_SRC_POWER), i == 1 ? TDC_LED_ST_POWER_OFF : TDC_LED_ST_NONE);
        tdc_ut_cm3_system_exchange(saved_system);
        /* 다음 독립 벡터에 이전 LED burst 대기 상태를 넘기지 않는다. */
        int zero_led[TDC_LED_SRC__MAX + 3] = {0};
        tdc_ut_cm3_led_exchange(zero_led);
    }
    tdc_ut_cm3_led_exchange(saved_led);
}

/* Setup/input: 독립 배터리/충전/뚜껑 RAM fixture. Output: 실제 setter/getter.
 * Restore: 초기 미수신 값을 포함한 내부 상태를 exchange로 복원한다. */
static void run_battery(void)
{
    int saved[4] = {0};
    tdc_ut_cm3_battery_exchange(saved);
    mark(11, 1, 8);
    tdc_pwr_battery_set_percent(40); put(tdc_pwr_battery_get_percent(), 40); put(tdc_pwr_battery_read_percentage(), 40);
    tdc_pwr_battery_set_percent(70); put(tdc_pwr_battery_get_percent(), 70); put(tdc_pwr_battery_read_percentage(), 70);
    tdc_pwr_charger_set_state(TDC_PWR_CHARGER_STATE_CONNECTED);
    put(tdc_pwr_charger_get_state().chargerConnectorPluggedIn, df_Connected);
    tdc_pwr_cradle_set_cover_state(1); put(tdc_pwr_cradle_get_cover_state(), df_Connected);
    tdc_pwr_charger_set_state(TDC_PWR_CHARGER_STATE_DISCONNECTED);
    put(tdc_pwr_charger_get_state().chargerConnectorPluggedIn, df_Disconnected);
    tdc_pwr_cradle_set_cover_state(2); put(tdc_pwr_cradle_get_cover_state(), df_Disconnected);
    tdc_ut_cm3_battery_exchange(saved);
}

/* Setup/input: 서로 다른 우선순위 요청 2개. Output: 실제 선택 함수 결과.
 * Restore: 엔진/GPIO를 실행하지 않고 요청/중재 상태만 보관/복원한다. */
static void run_led(void)
{
    int saved[TDC_LED_SRC__MAX + 3] = {0};
    tdc_ut_cm3_led_exchange(saved);
    cfx_cm3_sharedMemoryAll.userSettingValue.indicatorLED_OnOff = 1;
    tdc_led_request(TDC_LED_SRC_BATTERY, TDC_LED_ST_BATT_READY);
    tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_ERROR_MCU);
    mark(13, 1, 4);
    put(tdc_led_get_request(TDC_LED_SRC_BATTERY), TDC_LED_ST_BATT_READY);
    put(tdc_led_get_request(TDC_LED_SRC_ERROR), TDC_LED_ST_ERROR_MCU);
    put(tdc_ut_cm3_led_select(), TDC_LED_ST_ERROR_MCU);
    tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_NONE);
    put(tdc_ut_cm3_led_select(), TDC_LED_ST_BATT_READY);
    mark(13, 2, 3);
    put(tdc_led_get_request(TDC_LED_SRC_ERROR), TDC_LED_ST_NONE);
    tdc_led_request(TDC_LED_SRC_BATTERY, TDC_LED_ST_NONE);
    put(tdc_led_get_request(TDC_LED_SRC_BATTERY), TDC_LED_ST_NONE);
    put(tdc_ut_cm3_led_select(), TDC_LED_ST_NONE);
    tdc_ut_cm3_led_exchange(saved);
}

/* Setup/input: 오류/주소/적재 플래그 fixture. Output: 실제 조회/clear 결과.
 * Restore: run_unit에서 오류, 공유 자료, 주소 override를 모두 복원한다. */
static void run_errors(void)
{
    tdc_sys_error_clear_all();
    tdc_sys_error_update(en__dataProcessing_ERROR, en__unusableMapData, 0);
    cfx_cm3_sharedMemoryAll.CFX_ErrorCode = 7;
    mark(16, 1, 9);
    put(tdc_sys_error_read().dataProcessingErrorFlag, en__unusableMapData);
    put(tdc_shm_read_cfx_error_code(), 7);
    tdc_sys_error_clear_all(); put(tdc_sys_error_read().dataProcessingErrorFlag, 0);
    put(tdc_shm_read_cfx_error_code(), 7);
    g_tdc_ut_cm3_expected_shared_address = (uintptr_t)&cfx_cm3_sharedMemoryAll;
    put(tdc_shm_shared_memory_address_error(), 0);
    g_tdc_ut_cm3_expected_shared_address += sizeof(int);
    put(tdc_shm_shared_memory_address_error(), 1);
    cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded = 0;
    put(tdc_shm_is_cfx_eeprom_data_loaded(), 0);
    cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded = 1;
    put(tdc_shm_is_cfx_eeprom_data_loaded(), 1);
    /* default 주소의 실제 판정도 기록하되 host/타깃 배치를 구분한다. */
    g_tdc_ut_cm3_expected_shared_address = 0;
    put(tdc_shm_shared_memory_address_error(), (uintptr_t)&cfx_cm3_sharedMemoryAll != (uintptr_t)StartAddressCM3_sharedVarialbe);
}

/* 맵 번호 setter의 파일 입력만 RAM으로 격리한다. 목록/날짜는 아래의 서로
 * 다른 A/B 입력이다. 실제 번호/요청 판단과 조회는 원래 제품 함수가 수행한다. */
static void map_list_copy_boundary(int map, int isd)
{
    (void)map;
    (void)isd;
    tdc_shm_get_pointer_current_map_data()->numFrequencyBand = 32;
}
static int map_list_fft_boundary(int bands)
{
    return bands == 32 ? 0 : -1;
}

/* Setup/input: 비normal 공유 RAM fixture, 유닛별 서로 다른 값.
 * Output: 실제 공유 자료 setter/getter. Restore: run_unit에서 전체 복원한다.
 * 21/22는 파일/선택 경로를 실행하지 않으며 partial로 명시한다. */
static void run_shared(unsigned unit)
{
    switch (unit)
    {
        case 12:
            mark(12, 1, 7);
            cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3 = 77;
            tdc_shm_on_off_3_v_pmic_cm3_to_cfx(true);
            put(cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX, 1);
            put(cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX, 0);
            tdc_shm_on_off_3_v_pmic_cm3_to_cfx(false);
            put(cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX, 2);
            tdc_shm_enter_low_power_mode_cm3_to_cfx();
            put(cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX, 1);
            put(cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX, 2);
            put(cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3, 77);
            put(cfx_cm3_sharedMemoryAll.CM3_status, 0);
            break;
        case 17:
            mark(17, 1, 5);
            tdc_shm_change_pcm_output_mode(1); tdc_shm_change_next_pcm_output_mode(2);
            for (int i = 0; i < 3; ++i) tdc_shm_fill_specific_command_buffer(i, i + 1);
            put(tdc_shm_read_current_pcm_output_mode(), 1);
            put(cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode_next, 2);
            for (int i = 0; i < 3; ++i) put(cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[i], i + 1);
            break;
        case 18:
            mark(18, 1, 2);
            cfx_cm3_sharedMemoryAll.cfx_PCM_interface.conneded_ISDCheckPCM_state = 1;
            put(tdc_shm_read_connection_check_pcm_state(), 1);
            tdc_shm_clear_connection_check_pcm_fired_flag();
            put(tdc_shm_read_connection_check_pcm_state(), BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation);
            break;
        case 19:
            mark(19, 1, 33);
            cfx_cm3_sharedMemoryAll.maxAudioInput = 1234;
            for (int i = 0; i < 32; ++i) cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[i] = i;
            put(tdc_shm_read_audio_signal_max(), 1234);
            for (int i = 0; i < 32; ++i) put(tdc_shm_read_current_stimul_level_255()[i], i);
            break;
        case 20:
            mark(20, 1, 10);
            put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag, 0);
            put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange, 0);
            put(tdc_shm_get_pointer_calculated_stimul_para_by_cm3() == &cfx_cm3_sharedMemoryAll.calculatedStimulPara_byCM3, 1);
            cfx_cm3_sharedMemoryAll.calculatedStimulPara_byCM3.T_level_255[0] = 31;
            tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx();
            put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag, 1);
            put(tdc_shm_get_pointer_calculated_stimul_para_by_cm3()->T_level_255[0], 31);
            cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag = 1;
            tdc_shm_set_command_map_change_cm3_to_cfx();
            put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange, 1);
            put(tdc_shm_is_map_data_loaded_cfx(), 0);
            tdc_shm_share_mapping_program_connection(true); put(cfx_cm3_sharedMemoryAll.isMappingProgramConneted, 1);
            tdc_shm_share_mapping_program_connection(false); put(cfx_cm3_sharedMemoryAll.isMappingProgramConneted, 0);
            cfx_cm3_sharedMemoryAll.userSettingValueLoadedFlag = 1; put(tdc_shm_is_user_setting_value_loaded_cfx(), 1);
            break;
        case 21:
            /* 실제 개인정보/키가 아닌 합성 자료. 두 슬롯을 먼저 채우고 A/B/A를
             * 읽어 모든 이름/연결 정보 칸이 다른 슬롯과 섞이지 않는지 확인한다.
             * 입력 선택은 RAM fixture이며 제품 파일 로드 경로는 실행하지 않는다. */
            for (int i = 0; i < 2; ++i)
            {
                ST__CFX_CM3_SharedMemory_ISD_info *info = &cfx_cm3_sharedMemoryAll.cfx_ISD_info[i];
                info->isd_year = 20 + i;
                info->isd_month_model = 1 + i;
                info->isd_serial = 101 + i;
                info->isd_location_RL = i == 0 ? Left_Ear : Right_Ear;
                for (int j = 0; j < 25; ++j)
                    info->isd_userName[j] = j == 24 ? 0 : (i == 0 ? 'A' : 'a') + j;
                for (int j = 0; j < 4; ++j) info->remocon_passkey[j] = 1000 + 10 * i + j;
            }
            for (int step = 0; step < 3; ++step)
            {
                int i = step == 1;
                cfx_cm3_sharedMemoryAll.connected_ISD_num = i + 1;
                mark(21, step + 1, 32);
                put(tdc_shm_read_connected_isd_num(), i + 1);
                put(tdc_shm_read_isd_manufacture_id(i), i == 0 ? 0x14010065 : 0x15020066);
                put(tdc_shm_read_connected_isd_location(i + 1), i == 0 ? Left_Ear : Right_Ear);
                for (int j = 0; j < 25; ++j)
                    put(tdc_shm_read_connected_isd_user_name(i + 1)[j], j == 24 ? 0 : (i == 0 ? 'A' : 'a') + j);
                for (int j = 0; j < 4; ++j)
                    put(tdc_shm_read_remocon_passkey_connected_isd(i + 1)[j], 1000 + 10 * i + j);
            }
            break;
        case 22:
        {
            static const tdc_ut_cm3_map_ops_t list_ops = {
                map_list_copy_boundary, map_list_fft_boundary, map_list_fft_boundary};
            const tdc_ut_cm3_map_ops_t *saved_ops = g_tdc_ut_cm3_map_ops;
            int saved_map_flag = tdc_ut_cm3_map_flag_exchange(0);
            g_tdc_ut_cm3_map_ops = &list_ops;
            cfx_cm3_sharedMemoryAll.connected_ISD_num = 1;
            /* 목록/날짜는 입력 경계, 맵 선택은 실제 setter다. 저장장치 로드는 별도. */
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.user_usableMapNum = 2;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex[0] = 1;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex[1] = 1;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.mapStamp.mapping_year = 26;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[0].mapping_day = 1;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[1].mapping_day = 2;
            mark(22, 1, 10);
            put(tdc_shm_read_connected_isd_usable_map_num(), 2);
            for (int i = 0; i < 4; ++i) put(tdc_shm_read_connected_isd_usable_map_index()[i], i < 2);
            put(tdc_shm_read_connected_isd_map_stamp()[0], 26);
            put(tdc_shm_read_connected_isd_map_date(1)[2], 1);
            put(tdc_shm_read_connected_isd_map_date(2)[2], 2);
            tdc_shm_change_program_map_num(1);
            put(tdc_shm_read_program_map_num(), 1);
            tdc_shm_change_program_map_num(2);
            put(tdc_shm_read_program_map_num(), 2);
            /* B 입력도 별도 수신 fixture로 교체한다. 제품 파일 선택 함수는 호출하지 않는다. */
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.user_usableMapNum = 1;
            cfx_cm3_sharedMemoryAll.connected_ISD_num = 2;
            for (int i = 0; i < 4; ++i)
                cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex[i] = i == 2;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.mapStamp.mapping_year = 25;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[2].mapping_day = 3;
            tdc_shm_change_program_map_num(3);
            mark(22, 2, 8);
            put(tdc_shm_read_connected_isd_usable_map_num(), 1);
            for (int i = 0; i < 4; ++i) put(tdc_shm_read_connected_isd_usable_map_index()[i], i == 2);
            put(tdc_shm_read_connected_isd_map_stamp()[0], 25);
            put(tdc_shm_read_connected_isd_map_date(3)[2], 3);
            put(tdc_shm_read_program_map_num(), 3);
            /* 선택 setter가 목록 입력을 덮어쓰지 않았는지 추가 확인한다. */
            mark(22, 3, 5);
            for (int i = 0; i < 4; ++i) put(tdc_shm_read_connected_isd_usable_map_index()[i], i == 2);
            put(tdc_shm_read_connected_isd_map_date(3)[2], 3);
            g_tdc_ut_cm3_map_ops = saved_ops;
            (void)tdc_ut_cm3_map_flag_exchange(saved_map_flag);
            break;
        }
        case 23:
            mark(23, 1, 6);
            tdc_shm_change_audio_volume(1); tdc_shm_change_stimul_volume(1);
            put(tdc_shm_read_audio_volume(), 1); put(tdc_shm_read_stimul_volume(), 1);
            tdc_shm_change_audio_volume(10); put(tdc_shm_read_audio_volume(), 10); put(tdc_shm_read_stimul_volume(), 1);
            tdc_shm_change_stimul_volume(4); put(tdc_shm_read_audio_volume(), 10); put(tdc_shm_read_stimul_volume(), 4);
            break;
        case 24:
            mark(24, 1, 4);
            tdc_shm_change_led_indicator_on_off(1); tdc_shm_change_stimul_indicator_on_off(2);
            put(tdc_shm_read_led_indicator_on_off(), 1); put(tdc_shm_read_stimul_indicator_on_off(), 2);
            tdc_shm_change_led_indicator_on_off(2); tdc_shm_change_stimul_indicator_on_off(1);
            put(tdc_shm_read_led_indicator_on_off(), 2); put(tdc_shm_read_stimul_indicator_on_off(), 1);
            break;
    }
}

/* Setup/input: 원래 ISD 상수와 32채널 100/300uA 및 지원 상한 초과 입력.
 * Output: 실제 DAC/오프셋/T/C와 실패 반환. Restore: run_unit의 DAC/오류 복원. */
static void run_dac(void)
{
    int t[32], c[32], td[32], cd[32];
    /* 정상 입력의 예상 밖 실패도 미정의 읽기 없이 비교할 수 있게 출력 버퍼를
     * 구분 가능한 값으로 먼저 채운다. 실패 후 제품 값 보존 기준은 아니다. */
    for (int i = 0; i < 32; ++i) { t[i] = 100; c[i] = 300; td[i] = -901; cd[i] = -902; }
    mark(28, 1, 69);
    put(tdc_stim_set_range(25, t, c, 32, td, cd), 1);
    put(tdc_stim_read_dac_register_value()->DAC_Slope_register, Stimulation_DAC_A);
    put(tdc_stim_read_dac_register_value()->DAC_offsetSlope_register, Offset_DAC_A);
    put(tdc_stim_read_dac_offset_value(), 100);
    put(tdc_stim_read_dac_register_value()->DAC_offsetLevel_register, 44);
    for (int i = 0; i < 32; ++i) { put(td[i], 0); put(cd[i], 127); }
    for (int i = 0; i < 32; ++i) c[i] = 2500;
    mark(28, 2, 1); put(tdc_stim_set_range(25, t, c, 32, td, cd), 0);
    mark(28, 3, 1); put(tdc_stim_data_clear_bit(0xFFFF, 4, 4), 0xFF0F);
    /* DAC 범위 초과 C=2500uA/25us는 62500pC로 전하량 한계 이하다.
     * 아래는 같은 유효 DAC 입력 300uA에서 펄스폭만 바꾼 별도 전하량 경계다.
     * Q = I(uA) x PW(us): 74700/75000/75300pC. 맵의 허용 폭 13..255us 이내다.
     * 초과 후 T/C 값은 설계에서 정하지 않았으므로 반환만 비교한다. */
    for (int i = 0; i < 32; ++i) { t[i] = 100; c[i] = 300; }
    for (int boundary = 0; boundary < 3; ++boundary)
    {
        int pulse_us = 249 + boundary;
        int result = tdc_stim_set_range(pulse_us, t, c, 32, td, cd);
        mark(28, 4 + boundary, boundary < 2 ? 11 : 5);
        put(pulse_us, 249 + boundary);
        put(c[0], 300);
        put(c[0] * pulse_us, 74700 + 300 * boundary);
        put(df_MaxDeliveryCharge_pC, 75000);
        put(result, boundary < 2);
        if (boundary < 2)
        {
            put(tdc_stim_read_dac_register_value()->DAC_Slope_register, Stimulation_DAC_A);
            put(tdc_stim_read_dac_offset_value(), 100);
            put(td[0], 0); put(cd[0], 127);
            put(td[31], 0); put(cd[31], 127);
        }
    }
}

/* Setup/input: 같은 RAM 맵을 CIS와 NofM으로 각각 계산한다.
 * Output: 공유된 frames/channels/T/C와 완료 플래그. Restore: 전체 RAM/DAC 복원. */
static void run_calculated(void)
{
    prepare_map();
    g_tdc_ut_cm3_calculation_before_copy = observe_calculation_copy;
    for (int mode = 0; mode < 2; ++mode)
    {
        tdc_shm_get_pointer_current_map_data()->stimulationStrategy = mode == 0 ? df_stimulationStrategy_CIS : df_stimulationStrategy_nOFm;
        mark(29, mode + 1, 68);
        put(tdc_stim_calc_para_and_cfx_share(), 1);
        ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3 *value = tdc_shm_get_pointer_calculated_stimul_para_by_cm3();
        put(value->frameNumPerChannel, mode == 0 ? 2 : 3);
        put(value->transferableFrameNum, mode == 0 ? 12 : 8);
        for (int i = 0; i < 32; ++i) { put(value->T_level_255[i], 0); put(value->C_level_255[i], 127); }
        put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag, 0);
    }
    tdc_shm_get_pointer_current_map_data()->stimulationStrategy = df_stimulationStrategy_CIS;
    for (int boundary = 0; boundary < 3; ++boundary)
    {
        int pulse_us = 249 + boundary;
        tdc_shm_get_pointer_current_map_data()->stimulationPulsePhaseWidth = pulse_us;
        int result = tdc_stim_calc_para_and_cfx_share();
        mark(29, 3 + boundary, boundary < 2 ? 7 : 4);
        put(pulse_us, 249 + boundary);
        put(300 * pulse_us, 74700 + 300 * boundary);
        put(df_MaxDeliveryCharge_pC, 75000);
        put(result, boundary < 2);
        if (boundary < 2)
        {
            /* duration = 2*PW+14 = 512/514us; 13 frames, floor(24/13)=1. */
            put(tdc_shm_get_pointer_calculated_stimul_para_by_cm3()->frameNumPerChannel, 13);
            put(tdc_shm_get_pointer_calculated_stimul_para_by_cm3()->transferableFrameNum, 1);
            put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag, 0);
        }
    }
}

/* 25us/C2500uA는 전하량 62500pC지만 DAC 상한 2198uA를 넘는 실패 입력이다.
 * 실패 후 공유 값/완료 정책은 현행 설계에서 미정이다. 정의되지 않은 지역 배열을
 * 읽지 않고 실제 반환/복사 시도/앞선 상태 변경을 OBSERVED로만 남긴다.
 * T/C sentinel이 남는 것은 callback이 읽기를 막은 결과이며 제품 보존의 증거가 아니다. */
static void observe_calculation_failure(void)
{
    g_tdc_ut_cm3_vector_observed = 1;
    g_tdc_ut_cm3_calculation_before_copy = observe_calculation_copy;
    for (int initial_done = 0; initial_done <= 1; ++initial_done)
    {
        prepare_map();
        ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3 *value = tdc_shm_get_pointer_calculated_stimul_para_by_cm3();
        for (int i = 0; i < 32; ++i)
        {
            tdc_shm_get_pointer_current_map_data()->C_level_uA[i] = 2500;
            value->T_level_255[i] = 600 + i;
            value->C_level_255[i] = 700 + i;
        }
        value->frameNumPerChannel = 17;
        value->transferableFrameNum = 18;
        cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag = initial_done;
        cfx_cm3_sharedMemoryAll.CM3_tempValue1 = 91;
        cfx_cm3_sharedMemoryAll.CM3_tempValue2 = 92;
        tdc_sys_error_clear_all();
        s_tdc_ut_calculation_copy_attempts = 0;
        s_tdc_ut_calculation_copy_skipped = 0;
        int result = tdc_stim_calc_para_and_cfx_share();
        mark(29, 91 + initial_done, 16);
        put(result, 0);
        put(s_tdc_ut_calculation_copy_attempts, 0);
        put(s_tdc_ut_calculation_copy_skipped, 0);
        put(value->frameNumPerChannel, 0); put(value->transferableFrameNum, 0);
        put(tdc_shm_read_current_pcm_output_mode(), 0);
        put(initial_done, 0);
        put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag, 0);
        put(cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration, 0);
        put(tdc_sys_error_read().dataProcessingErrorFlag, 0);
        put(value->T_level_255[0], 0); put(value->T_level_255[31], 0);
        put(value->C_level_255[0], 0); put(value->C_level_255[31], 0);
        put(cfx_cm3_sharedMemoryAll.CM3_tempValue1, 0);
        put(cfx_cm3_sharedMemoryAll.CM3_tempValue2, 0);
    }
}

/* Setup/input: 채널 1, 300uA, 트리거 1회와 481개의 가상 tick.
 * Output: 공유 크기/ON-OFF/세 번의 상승 전이/종료. 실제 펄스는 출력하지 않는다.
 * Restore: trigger와 지역 counter, 공유/DAC 자료를 모두 복원한다. */
static void run_indicator(void)
{
    int saved[3] = {0};
    int td[32], cd[32];
    tdc_ut_cm3_indicator_exchange(saved);
    prepare_map();
    tdc_stim_set_range(25, tdc_shm_get_pointer_current_map_data()->T_level_uA,
                             tdc_shm_get_pointer_current_map_data()->C_level_uA, 32, td, cd);
    tdc_stim_indicator_set_level_255();
    tdc_stim_indicator_set_trigger();
    mark(44, 1, 8);
    put(cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255, 127);
    put(tdc_shm_get_pointer_current_map_data()->stimulationIndicatorChannelNum, 1);
    int rising_count = 0, previous_output = 0;
    for (int tick = 1; tick <= 481; ++tick)
    {
        tdc_stim_indicator_out(1, false, false);
        int current_output = cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3;
        if (current_output && !previous_output) ++rising_count;
        previous_output = current_output;
        if (tick == 1 || tick == 80 || tick == 81)
            put(cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3, tick <= 80);
    }
    put(tdc_stim_indicator_is_triggered(), 0);
    put(cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3, 0);
    put(rising_count, 3);
    tdc_ut_cm3_indicator_exchange(saved);
}

/* Setup: 호출자가 정지점과 private-state gateway를 준비한다. 기본은 읽기만 한다.
 * Input: 기준서 4/6/30의 단계별 요청. Output: 실제 공유 getter/ISD getter와 적용 상태.
 * Restore: gateway private 상태와 runner 전체 공유 상태. 기대값/합격은 생성하지 않는다.
 * case 90/99는 gateway begin/restore 반환값이다. -5는 입력 적용 미설정이다.
 * 로그의 case ID는 1..999이므로 준비 기록에 0을 사용하지 않는다. */
static int run_observer(unsigned unit)
{
    const tdc_ut_cm3_observer_ops_t *ops = g_tdc_ut_cm3_observer_ops;
    int configured = ops && ops->begin && ops->apply && ops->restore;
    int begin_result = configured ? ops->begin(unit) : TDC_UT_CM3_OBSERVER_NOT_CONFIGURED;
    int applied_all = configured && begin_result == 0;
    unsigned cases = unit == 6 ? 2u : 3u;
    g_tdc_ut_cm3_vector_observed = 1;
    mark(unit, 90, 1); put(begin_result, 0);
    for (unsigned case_id = 1; case_id <= cases; ++case_id)
    {
        int requested = unit == 4 ? (case_id == 2) : (case_id == 3 ? 0 : (int)case_id);
        int result = applied_all ? ops->apply(unit, case_id, requested)
                                                       : TDC_UT_CM3_OBSERVER_NOT_CONFIGURED;
        if (result != 0) applied_all = 0;
        if (unit == 4)
        {
            mark(unit, case_id, 6);
            put(requested, 0); put(result, 0);
            put(cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration, 0);
            put(cfx_cm3_sharedMemoryAll.is_CFX_started, 0);
            put(tdc_shm_shared_memory_address_error(), 0);
            put(cfx_cm3_sharedMemoryAll.systemShare.system_opMode, 0);
        }
        else if (unit == 6)
        {
            mark(unit, case_id, 9);
            put(requested, 0); put(result, 0); put(tdc_shm_read_program_map_num(), 0);
            put(cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange, 0);
            put(tdc_shm_is_map_data_loaded_cfx(), 0); put(tdc_shm_is_user_setting_value_loaded_cfx(), 0);
            put(cfx_cm3_sharedMemoryAll.CM3_status, 0);
            put(cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX, 0);
            put(cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX, 0);
        }
        else
        {
            ST__ISD_STATUS state = tdc_isd_get_state();
            int extra[4] = {0};
            int count = configured && begin_result == 0 && ops->sample_extra ? ops->sample_extra(unit, extra)
                                                         : TDC_UT_CM3_OBSERVER_NOT_CONFIGURED;
            if (count < 0 || count > 4) count = TDC_UT_CM3_OBSERVER_NOT_CONFIGURED;
            mark(unit, case_id, 10);
            put(requested, 0); put(result, 0); put(state.isd_controlState, 0);
            put(state.conneded_ISD, 0); put(tdc_shm_read_current_pcm_output_mode(), 0); put(count, 0);
            for (unsigned i = 0; i < 4; ++i) put(extra[i], 0);
        }
    }
    /* begin 실패 중에도 부분 저장 상태가 있을 수 있으므로 restore는 반드시 호출한다. */
    int restore_result = configured ? ops->restore(unit) : TDC_UT_CM3_OBSERVER_NOT_CONFIGURED;
    mark(unit, 99, 1); put(restore_result, 0);
    if (restore_result != 0) applied_all = 0;
    return applied_all ? TDC_UT_CM3_VECTOR_OBSERVED : TDC_UT_CM3_VECTOR_OBSERVED_NO_INPUT;
}

static int run_unit(unsigned unit, int failure_observation)
{
    int partial = unit == 4 || unit == 21 || unit == 22 || unit == 29;
    /* 별도 수동 gateway를 지정하지 않으면 제공된 안전 RAM 입력을 사용한다. */
    int observer = g_tdc_ut_cm3_observer_ops && (unit == 4 || unit == 6 || unit == 30);
    int observer_result = TDC_UT_CM3_VECTOR_OBSERVED_NO_INPUT;
    if (g_tdc_ut_cm3_vector_done || g_tdc_ut_io_cm3_done) return TDC_UT_CM3_VECTOR_BUSY;
    switch (unit)
    {
        case 4: case 6: case 30:
        case 7: case 9: case 11: case 12: case 13: case 16: case 17: case 18:
        case 19: case 20: case 21: case 22: case 23: case 24: case 27: case 28:
        case 29: case 44: break;
        default: return TDC_UT_CM3_VECTOR_UNSUPPORTED;
    }
    if (cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration != 0) return TDC_UT_CM3_VECTOR_UNSAFE;
    /* Setup: CFX 중지와 격리된 비normal fixture를 전제로 한다. 전체 상태 백업은
     * ISR/다른 코어 경쟁을 해결하지 못하므로 호출자는 시험 전용 정지점을 사용한다. */
    s_tdc_ut_saved_shared = cfx_cm3_sharedMemoryAll;
    /* 두 번 교환하여 현재 관찰 입력을 유지하면서 독립 복원 사본을 얻는다. */
    tdc_ut_link_control_state_t saved_link = {0};
    tdc_ut_link_control_exchange(&saved_link);
    tdc_ut_link_control_state_t active_link = saved_link;
    tdc_ut_link_control_exchange(&active_link);
    tdc_sys_error_code_t saved_error = {0};
    tdc_ut_cm3_error_exchange(&saved_error);
    ST_STIUL_DAC_REGISTER_VALUE saved_dac = *tdc_stim_read_dac_register_value();
    int saved_offset = tdc_ut_cm3_dac_offset_exchange(0);
    int saved_duration = tdc_ut_cm3_frame_duration_exchange(0);
    uintptr_t saved_address = g_tdc_ut_cm3_expected_shared_address;
    int saved_uart = g_tdc_ut_cm3_suppress_uart_flag;
    int saved_diagnostics = g_tdc_ut_cm3_suppress_diagnostics;
    int (*saved_before_copy)(int) = g_tdc_ut_cm3_calculation_before_copy;
    int saved_copy_attempts = s_tdc_ut_calculation_copy_attempts;
    int saved_copy_skipped = s_tdc_ut_calculation_copy_skipped;
    if (!observer)
    {
        memset(&cfx_cm3_sharedMemoryAll, 0, sizeof(cfx_cm3_sharedMemoryAll));
        cfx_cm3_sharedMemoryAll.systemShare.system_opMode = en__mappingMode;
    }
    g_tdc_ut_cm3_suppress_uart_flag = 1;
    g_tdc_ut_cm3_suppress_diagnostics = 1;
    g_tdc_ut_cm3_vector_count = 0;
    g_tdc_ut_cm3_vector_overflow = 0;
    g_tdc_ut_cm3_vector_observed = 0;
    switch (unit)
    {
        case 4: case 6:
            if (observer) observer_result = run_observer(unit);
            else
            {
                int result = tdc_ut_cm3_coordination_run(unit, g_tdc_ut_cm3_vector_actual,
                    g_tdc_ut_cm3_vector_expected, TDC_UT_CM3_VECTOR_CAPACITY, &g_tdc_ut_cm3_vector_count);
                if (result < 0) g_tdc_ut_cm3_vector_overflow = 1;
            }
            break;
        case 30:
            if (observer) observer_result = run_observer(unit);
            else if (tdc_ut_cm3_state_run(g_tdc_ut_cm3_vector_actual,
                     g_tdc_ut_cm3_vector_expected, TDC_UT_CM3_VECTOR_CAPACITY,
                     &g_tdc_ut_cm3_vector_count) < 0) g_tdc_ut_cm3_vector_overflow = 1;
            break;
        case 7: run_system(); break;
        case 9: run_touch(); break;
        case 11: run_battery(); break;
        case 13: run_led(); break;
        case 16: run_errors(); break;
        case 27:
            for (int i = 0; i < 2; ++i)
            {
                int frame = tdc_stim_calc_frame_per_channel(13 + i);
                mark(27, i + 1, 3);
                put(tdc_ut_cm3_last_frame_duration(), i == 0 ? 40 : 42);
                put(frame, i + 1);
                put(frame > 0 ? tdc_stim_calc_transferable_channel_num(frame, 32) : -1, i == 0 ? 24 : 12);
            }
            break;
        case 28: run_dac(); break;
        case 29:
            if (failure_observation) observe_calculation_failure();
            else run_calculated();
            break;
        case 44: run_indicator(); break;
        default: run_shared(unit); break;
    }
    /* Restore: 보고용 배열만 유지하고 시험에서 바꾼 제품 상태는 모두 되돌린다. */
    cfx_cm3_sharedMemoryAll = s_tdc_ut_saved_shared;
    tdc_ut_link_control_exchange(&saved_link);
    tdc_ut_cm3_error_exchange(&saved_error);
    *tdc_stim_read_dac_register_value() = saved_dac;
    (void)tdc_ut_cm3_dac_offset_exchange(saved_offset);
    (void)tdc_ut_cm3_frame_duration_exchange(saved_duration);
    g_tdc_ut_cm3_expected_shared_address = saved_address;
    g_tdc_ut_cm3_suppress_uart_flag = saved_uart;
    g_tdc_ut_cm3_suppress_diagnostics = saved_diagnostics;
    g_tdc_ut_cm3_calculation_before_copy = saved_before_copy;
    s_tdc_ut_calculation_copy_attempts = saved_copy_attempts;
    s_tdc_ut_calculation_copy_skipped = saved_copy_skipped;
    g_tdc_ut_cm3_vector_done = 1;
    if (g_tdc_ut_cm3_vector_overflow) return TDC_UT_CM3_VECTOR_OVERFLOW;
    if (failure_observation) return TDC_UT_CM3_VECTOR_OBSERVED;
    if (observer) return observer_result;
    return partial ? TDC_UT_CM3_VECTOR_PARTIAL : TDC_UT_CM3_VECTOR_OK;
}

int tdc_ut_cm3_vectors_run_unit(unsigned unit)
{
    return run_unit(unit, 0);
}

int tdc_ut_cm3_vectors_observe_calculation_failure(void)
{
    return run_unit(29, 1);
}

void tdc_ut_cm3_vectors_release(void)
{
    g_tdc_ut_cm3_vector_done = 0;
}

const int *tdc_ut_cm3_vectors_expected(void)
{
    return g_tdc_ut_cm3_vector_observed ? 0 : g_tdc_ut_cm3_vector_expected;
}
#endif
