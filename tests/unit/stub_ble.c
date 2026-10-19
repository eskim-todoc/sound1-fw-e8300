#include <string.h>
#include <stdint.h>

#include <tdc_ble_mapping.h>
#include <tdc_sys_error.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>

#include "stub_ble.h"

// 저장소 크기.
// isd_info 는 0x6C 데이터 인덱스 3 에서 인덱스 45 까지 쓴다(i = 41..46 의 [i-1]).
// stimul_para 는 파라미터 13 + 전극 64 + T/C 레벨 64 + xmin/xmax 64 로 200 대까지 간다.
// 여유를 두되 가드 워드로 초과를 잡는다.
#define STUB_ISD_INFO_SIZE    64
#define STUB_STIMUL_PARA_SIZE 256
#define STUB_GUARD_WORD       0x5A5AA5A5

// ---- 상태 ----
static ST__MAPPING_PACKET g_packet;
static int                g_seq_index;

static int g_error_count;
static int g_error_last_command;
static int g_error_last_major;
static int g_error_last_minor;
static int g_error_last_line;

static int     g_tx_count;
static int     g_tx_len;
static uint8_t g_tx_buffer[BLE_DataPacketSize];

static int g_isd_info[STUB_ISD_INFO_SIZE + 1];
static int g_stimul_para[STUB_STIMUL_PARA_SIZE + 1];

static ST__CFX_CM3_SharedMemory_mapData g_map_data;

void stub_reset(void)
{
    memset(&g_packet, 0, sizeof(g_packet));
    g_seq_index = 0;

    g_error_count        = 0;
    g_error_last_command = -1;
    g_error_last_major   = -1;
    g_error_last_minor   = -1;
    g_error_last_line    = -1;

    g_tx_count = 0;
    g_tx_len   = 0;
    memset(g_tx_buffer, 0, sizeof(g_tx_buffer));

    memset(g_isd_info, 0, sizeof(g_isd_info));
    memset(g_stimul_para, 0, sizeof(g_stimul_para));
    g_isd_info[STUB_ISD_INFO_SIZE]        = STUB_GUARD_WORD;
    g_stimul_para[STUB_STIMUL_PARA_SIZE]  = STUB_GUARD_WORD;

    memset(&g_map_data, 0, sizeof(g_map_data));
}

// ---- 프로덕션 심볼 대역 ----

ST__MAPPING_PACKET *tdc_ble_mapping_get_packet(void)
{
    return &g_packet;
}

int tdc_ble_mapping_get_seq_index(void)
{
    return g_seq_index;
}

void tdc_ble_mapping_set_seq_index(int seqIndex)
{
    g_seq_index = seqIndex;
}

void tdc_sys_error_send_to_app(EN__MAPPING_COMMAND command, tdc_sys_error_major_t majorError, int minorError, int lineNumber)
{
    g_error_count++;
    g_error_last_command = (int) command;
    g_error_last_major   = (int) majorError;
    g_error_last_minor   = minorError;
    g_error_last_line    = lineNumber;
}

void tdc_hal_spi_write_tx_buffer(const uint8_t *source, int dataSize)
{
    g_tx_count++;

    g_tx_len = dataSize;
    if (g_tx_len > (int) sizeof(g_tx_buffer))
    {
        g_tx_len = (int) sizeof(g_tx_buffer);
    }

    memset(g_tx_buffer, 0, sizeof(g_tx_buffer));
    if (g_tx_len > 0)
    {
        memcpy(g_tx_buffer, source, (size_t) g_tx_len);
    }
}

int *tdc_shm_get_pointer_repository_for_read_write_map_data_isd_info(void)
{
    return g_isd_info;
}

int *tdc_shm_get_pointer_repository_for_read_write_map_data_stimul_para(void)
{
    return g_stimul_para;
}

ST__CFX_CM3_SharedMemory_mapData *tdc_shm_get_pointer_current_map_data(void)
{
    return &g_map_data;
}

// ---- 로그 (map_stim 이 TDC_PRINTF_* 를 쓴다) ----
//
// 테스트에서는 아무것도 출력하지 않는다. 로그 문구는 검증 대상이 아니고
// (실기 로그 대조는 은수님 게이트 소관) 출력이 섞이면 결과를 읽기 어렵다.

int SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...)
{
    (void) BufferIndex;
    (void) sFormat;

    return 0;
}

void tdc_printf_file_func_line(const char *file, const char *func, int line)
{
    (void) file;
    (void) func;
    (void) line;
}

// ---- 조회 API ----

int stub_error_count(void)        { return g_error_count; }
int stub_error_last_command(void) { return g_error_last_command; }
int stub_error_last_major(void)   { return g_error_last_major; }
int stub_error_last_minor(void)   { return g_error_last_minor; }
int stub_error_last_line(void)    { return g_error_last_line; }

int            stub_tx_count(void)  { return g_tx_count; }
int            stub_tx_len(void)    { return g_tx_len; }
const uint8_t *stub_tx_buffer(void) { return g_tx_buffer; }

int *stub_isd_info_repository(void)    { return g_isd_info; }
int *stub_stimul_para_repository(void) { return g_stimul_para; }

int stub_repository_overflow(void)
{
    if (g_isd_info[STUB_ISD_INFO_SIZE] != STUB_GUARD_WORD)
    {
        return 1;
    }
    if (g_stimul_para[STUB_STIMUL_PARA_SIZE] != STUB_GUARD_WORD)
    {
        return 2;
    }

    return 0;
}
