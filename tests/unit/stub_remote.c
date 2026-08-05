// ble/remote/ 파싱 함수용 프로덕션 심볼 대역.
// 설계 의도는 stub_remote.h 주석 참조.

#include <string.h>
#include <stdint.h>

#include <tdc_shm.h>
#include <tdc_ble_remote.h>
#include <tdc_isd_map_data.h>
#include <tdc_pwr_battery.h>
#include <tdc_hal_spi.h>
#include <tdc_hal_timer.h>
#include <tdc_fs_event_log.h>
#include <tdc_fs_stim_mute.h>
#include <tdc_ble_general_debug.h>
#include <tdc_ble_gain_control.h>
#include <tdc_ble_remote_sp_para.h>
#include <tdc_stim_para_cal.h>
#include <tdc_stim_definitions.h>
#include <tdc_isd.h>

#include "stub_remote.h"

#define STUB_REMOTE_MAP_INDEX_SIZE 8
#define STUB_REMOTE_MAP_STAMP_SIZE 8

// ---- 상태 ----

// 공유메모리 실체. 프로덕션에서는 링커가 .shared_memory 섹션에 놓는다.
// 호스트에서는 평범한 전역이면 충분하다(섹션 속성은 헤더의 extern 선언에만 있다).
ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll;

static int g_stimul_volume;
static int g_audio_volume;
static int g_program_map_num;
static int g_tele_coil_on_off;
static int g_stimul_indicator_on_off;
static int g_led_indicator_on_off;
static int g_usable_map_num;

static int g_usable_map_index[STUB_REMOTE_MAP_INDEX_SIZE];
static int g_map_stamp[STUB_REMOTE_MAP_STAMP_SIZE];

static int g_flash_call_count;
static int g_flash_last_command;
static int g_flash_last_slot;
static int g_flash_last_map;

static int g_battery_percent;

static int g_stim_mute_call_count;
static int g_stim_mute_last_enable;
static int g_stim_mute_last_level;

static int g_event_log_write_count;
static int g_event_log_last_type;

static char g_firmware_info[16];

// sp_para(실물 컴파일)가 읽는 값들
static int                         g_connected_isd_id;
static int                         g_audio_signal_max;
static int                         g_current_stimul_level_255[df_MaxNumOfElectrode];
static ST_STIUL_DAC_REGISTER_VALUE g_dac_register_value;

// ---- 리셋 ----

void stub_remote_reset(void)
{
    memset(&cfx_cm3_sharedMemoryAll, 0, sizeof(cfx_cm3_sharedMemoryAll));

    g_stimul_volume           = 0;
    g_audio_volume            = 0;
    g_program_map_num         = 1;
    g_tele_coil_on_off        = 0;
    g_stimul_indicator_on_off = 0;
    g_led_indicator_on_off    = 0;
    g_usable_map_num          = 1;

    memset(g_usable_map_index, 0, sizeof(g_usable_map_index));
    g_usable_map_index[0] = 1; /* 맵 1 은 기본 사용 가능 */
    memset(g_map_stamp, 0, sizeof(g_map_stamp));

    g_flash_call_count   = 0;
    g_flash_last_command = -1;
    g_flash_last_slot    = -1;
    g_flash_last_map     = -1;

    g_battery_percent = 100;

    g_stim_mute_call_count  = 0;
    g_stim_mute_last_enable = -1;
    g_stim_mute_last_level  = -1;

    g_event_log_write_count = 0;
    g_event_log_last_type   = -1;

    memset(g_firmware_info, 0, sizeof(g_firmware_info));
    g_firmware_info[0] = 1; /* version[0] */

    g_connected_isd_id = 0;
    g_audio_signal_max = 0;
    memset(g_current_stimul_level_255, 0, sizeof(g_current_stimul_level_255));
    memset(&g_dac_register_value, 0, sizeof(g_dac_register_value));
}

// ---- 조회 API ----

int stub_remote_get_stimul_volume(void) { return g_stimul_volume; }
int stub_remote_get_audio_volume(void) { return g_audio_volume; }
int stub_remote_get_program_map_num(void) { return g_program_map_num; }
int stub_remote_get_tele_coil_on_off(void) { return g_tele_coil_on_off; }
int stub_remote_get_stimul_indicator_on_off(void) { return g_stimul_indicator_on_off; }
int stub_remote_get_led_indicator_on_off(void) { return g_led_indicator_on_off; }

void stub_remote_set_stimul_volume(int v) { g_stimul_volume = v; }
void stub_remote_set_audio_volume(int v) { g_audio_volume = v; }
void stub_remote_set_program_map_num(int v) { g_program_map_num = v; }
void stub_remote_set_usable_map_num(int v) { g_usable_map_num = v; }

int *stub_remote_usable_map_index(void) { return g_usable_map_index; }

int stub_remote_flash_call_count(void) { return g_flash_call_count; }
int stub_remote_flash_last_command(void) { return g_flash_last_command; }
int stub_remote_flash_last_slot(void) { return g_flash_last_slot; }
int stub_remote_flash_last_map(void) { return g_flash_last_map; }

int  stub_remote_battery_percent(void) { return g_battery_percent; }
void stub_remote_set_battery_percent(int pct) { g_battery_percent = pct; }

int stub_remote_stim_mute_call_count(void) { return g_stim_mute_call_count; }
int stub_remote_stim_mute_last_enable(void) { return g_stim_mute_last_enable; }
int stub_remote_stim_mute_last_level(void) { return g_stim_mute_last_level; }

int stub_remote_event_log_write_count(void) { return g_event_log_write_count; }
int stub_remote_event_log_last_type(void) { return g_event_log_last_type; }

// ---- 프로덕션 심볼 대역: 공유메모리 설정값 ----
//
// 값을 실제로 보관한다. 리모콘이 "바꾸고 되읽어 응답" 하는 구조라
// 보관하지 않으면 응답 검증이 성립하지 않는다.

void tdc_shm_change_stimul_volume(int volume) { g_stimul_volume = volume; }
int  tdc_shm_read_stimul_volume(void) { return g_stimul_volume; }

void tdc_shm_change_audio_volume(int volume) { g_audio_volume = volume; }
int  tdc_shm_read_audio_volume(void) { return g_audio_volume; }

void tdc_shm_change_program_map_num(int mapNum) { g_program_map_num = mapNum; }
int  tdc_shm_read_program_map_num(void) { return g_program_map_num; }

void tdc_shm_change_tele_coil_on_off(EN__PAYLOAD_ON_OFF OnOff) { g_tele_coil_on_off = (int) OnOff; }
int  tdc_shm_read_tele_coil_on_off(void) { return g_tele_coil_on_off; }

void tdc_shm_change_stimul_indicator_on_off(EN__PAYLOAD_ON_OFF OnOff) { g_stimul_indicator_on_off = (int) OnOff; }
int  tdc_shm_read_stimul_indicator_on_off(void) { return g_stimul_indicator_on_off; }

void tdc_shm_change_led_indicator_on_off(EN__PAYLOAD_ON_OFF OnOff) { g_led_indicator_on_off = (int) OnOff; }
int  tdc_shm_read_led_indicator_on_off(void) { return g_led_indicator_on_off; }

int  tdc_shm_read_connected_isd_usable_map_num(void) { return g_usable_map_num; }
int *tdc_shm_read_connected_isd_usable_map_index(void) { return g_usable_map_index; }
int *tdc_shm_read_connected_isd_map_stamp(void) { return g_map_stamp; }

// ---- 프로덕션 심볼 대역: 플래시 명령 ----
//
// 실제 NVM 접근은 하지 않고 호출만 기록한다.
// 리모콘 테스트의 관심사는 "올바른 슬롯·맵으로 요청했는가" 다.

static void stub_remote_note_flash(int command, int slot_index, int map_index)
{
    g_flash_call_count++;
    g_flash_last_command = command;
    g_flash_last_slot    = slot_index;
    g_flash_last_map     = map_index;
}

void tdc_isd_map_read_info_setting(bool startFlag, int commnad, int slot_index)
{
    (void) startFlag;
    stub_remote_note_flash(commnad, slot_index, -1);
}

void tdc_isd_map_write_info_setting(bool startFlag, int commnad, int slot_index)
{
    (void) startFlag;
    stub_remote_note_flash(commnad, slot_index, -1);
}

void tdc_isd_map_read_stim_para(bool startFlag, int commnad, int slot_index, int map_index)
{
    (void) startFlag;
    stub_remote_note_flash(commnad, slot_index, map_index);
}

void tdc_isd_map_write_stim_para(bool startFlag, int commnad, int slot_index, int map_index)
{
    (void) startFlag;
    stub_remote_note_flash(commnad, slot_index, map_index);
}

void tdc_isd_map_reset_nvm_selected(bool startFlag, int commnad, int slot_index, EN__mapping_ReadWriteMap_command RecoverOrErase)
{
    (void) startFlag;
    (void) RecoverOrErase;
    stub_remote_note_flash(commnad, slot_index, -1);
}

void tdc_isd_map_reset_nvm_map_data(bool startFlag, int commnad, int slot_index, int map_index, EN__mapping_ReadWriteMap_command RecoverOrErase)
{
    (void) startFlag;
    (void) RecoverOrErase;
    stub_remote_note_flash(commnad, slot_index, map_index);
}

void tdc_isd_map_reset_nvm_2to4(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase)
{
    (void) startFlag;
    (void) RecoverOrErase;
    stub_remote_note_flash(command, -1, -1);
}

bool tdc_isd_map_reset_nvm_all(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase)
{
    (void) startFlag;
    (void) RecoverOrErase;
    stub_remote_note_flash(command, -1, -1);
    return true; /* 완료로 응답 - 상태머신이 다음으로 진행한다 */
}

// ---- 프로덕션 심볼 대역: 기타 ----

int tdc_pwr_battery_read_percentage(void)
{
    return g_battery_percent;
}

bool tdc_hal_spi_is_tx_buffer_empty(void)
{
    return true; /* 항상 송신 가능 - 대기 루프를 돌지 않게 한다 */
}

void tdc_hal_timer_update_reference_time(tdc_hal_timer_time_t *p_time, uint32_t count_init_value)
{
    (void) count_init_value;
    if (p_time != NULL)
    {
        memset(p_time, 0, sizeof(*p_time));
    }
}

int tdc_hal_timer_get_tick(void)
{
    return 0;
}

void tdc_fs_event_log_update_bt_addr(TDC_FS_EVENT_LOG_BT_ADDR_T *p_bt_addr)
{
    (void) p_bt_addr;
}

int tdc_fs_event_log_write(uint32_t event_type)
{
    g_event_log_write_count++;
    g_event_log_last_type = (int) event_type;
    return 0;
}

int tdc_fs_stim_mute_update(uint32_t enable, uint32_t level)
{
    g_stim_mute_call_count++;
    g_stim_mute_last_enable = (int) enable;
    g_stim_mute_last_level  = (int) level;
    return TDC_FS_STIM_MUTE_RET_TRUE;
}

char *readFirmwareInfo(void)
{
    return g_firmware_info;
}

// SEGGER_RTT_printf 와 tdc_printf_file_func_line 은 stub_ble.c 가 이미 정의한다.
// 여기서 또 정의하면 중복 심볼로 링크가 깨진다.

// ---- tdc_ble_remote_sp_para.c (실물 컴파일) 가 요구하는 것 ----
//
// sp_para 는 스텁이 아니라 실물을 함께 컴파일한다. 리모콘 0x40~0x59 와 같은
// 파싱 계통이므로 분해 검증의 대상에 포함되어야 한다.

int tdc_isd_read_connected_id(void)
{
    return g_connected_isd_id;
}

int tdc_shm_read_audio_signal_max(void)
{
    return g_audio_signal_max;
}

int *tdc_shm_read_current_stimul_level_255(void)
{
    return g_current_stimul_level_255;
}

ST_STIUL_DAC_REGISTER_VALUE *tdc_stim_read_dac_register_value(void)
{
    return &g_dac_register_value;
}

// 0x8F(범용 디버깅) · 게인 제어는 각자 별도 모듈이며 리모콘 분해의 관심사가 아니다.
// tx_index 를 그대로 돌려주어 "아무것도 쓰지 않았다" 로 만든다.

int tdc_ble_general_debug_handle(const ST__REMOTECONTROL_PACKET *packet, uint8_t *tx_buf, int tx_index)
{
    (void) packet;
    (void) tx_buf;
    return tx_index;
}

int tdc_ble_gain_control_handle(const ST__REMOTECONTROL_PACKET *packet, uint8_t *tx_buf, int tx_index)
{
    (void) packet;
    (void) tx_buf;
    return tx_index;
}
