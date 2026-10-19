/**
 * @file tdc_dfu_ble_ota.h
 */

#ifndef __tdc_dfu_ble_ota_h__
#define __tdc_dfu_ble_ota_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>

#include <processorDirective.h>

#include <tdc_sys_error.h>

#include <tdc_hal_spi.h>

#include <tdc_ble_protocol.h>

#include <tdc_dfu_sdk_boot.h>

#define CI_OTA_PKT_HEADER 0x80

#define OTA_FILE_TYPE_MANIFEST 1
#define OTA_FILE_TYPE_APP000   2
#define OTA_FILE_TYPE_APP001   3
#define OTA_FILE_TYPE_APP002   4

#define CI_OTA_PKT_RESP_OK   1
#define CI_OTA_PKT_RESP_FAIL 2

#define CI_OTA_PKT_WRITE_DATA_LEN 16

// #define CI_OTA_RET_SUCCESS          1
// #define CI_OTA_RET_ERROR_OPEN_FAIL  2
// #define CI_OTA_RET_ERROR_WRITE_FAIL 3
// #define CI_OTA_RET_ERROR_READ_FAIL  4

#define TDC_DFU_CONN_ST_DISCONN 0
#define TDC_DFU_CONN_ST_CONN    1

typedef struct
{
    int data_index;
    int slot_num;
    int file_type;
    int option;
    int total_byte;
    int end_data_index;
    int end_data_index_byte;
    int expected_next_data_index;
    int recv_total_byte;
} ST__OTA_FILE_WRITE_INFO;

// 패킷 헤더
typedef enum
{
    PKT_HEADER_OTA = 0xC3,
} EN__PKT_HEADER_OTA;

// 옵션에 따른 수신 패킷 사이즈 (데이터 인덱스 0일 때)
typedef enum
{
    RECV_PKT_SIZE_OTA_WRITE = 18,
    RECV_PKT_SIZE_OTA_READ  = 8,
    RECV_PKT_SIZE_OTA_SIZE  = 8,
} EN__RECV_PKT_SIZE_OTA;

// 옵션에 따른 응답 패킷 사이즈 (데이터 인덱스 0일 때)
typedef enum
{
    RESP_PKT_SIZE_OTA_WRITE = 9,
    RESP_PKT_SIZE_OTA_READ  = 8,
    RESP_PKT_SIZE_OTA_SIZE  = 13,
} EN__RESP_PKT_SIZE_OTA;

// 공통 수신 패킷 인덱스 (데이터 인덱스 0일 때)
typedef enum
{
    RECV_PKT_IDX_OTA_HEADER       = 0,
    RECV_PKT_IDX_OTA_DATA_INDEX_0 = 1,
    RECV_PKT_IDX_OTA_DATA_INDEX_1 = 2,
    RECV_PKT_IDX_OTA_DATA_INDEX_2 = 3,
    RECV_PKT_IDX_OTA_SLOT_NUM     = 4,
    RECV_PKT_IDX_OTA_FILE_TYPE_0  = 5,
    RECV_PKT_IDX_OTA_FILE_TYPE_1  = 6,
    RECV_PKT_IDX_OTA_OPTION       = 7,
} EN__RECV_PKT_IDX_OTA;

// 공통 응답 패킷 인덱스 (데이터 인덱스 0일 때)
typedef enum
{
    RESP_PKT_IDX_OTA_HEADER       = 0,
    RESP_PKT_IDX_OTA_DATA_INDEX_0 = 1,
    RESP_PKT_IDX_OTA_DATA_INDEX_1 = 2,
    RESP_PKT_IDX_OTA_DATA_INDEX_2 = 3,
    RESP_PKT_IDX_OTA_SLOT_NUM     = 4,
    RESP_PKT_IDX_OTA_FILE_TYPE_0  = 5,
    RESP_PKT_IDX_OTA_FILE_TYPE_1  = 6,
    RESP_PKT_IDX_OTA_OPTION       = 7,
} EN__RESP_PKT_IDX_OTA;

typedef enum
{
    CI_BLE_OTA_COMMAND_OTA_START = 0xC0,
    CI_BLE_OTA_COMMAND_OTA_END   = 0xC1,
    CI_BLE_OTA_COMMAND_OTA       = 0xC3
} CI_BLE_CONTROL_COMMAND_OTA_E;

typedef struct
{
    CI_BLE_CONTROL_COMMAND_OTA_E command;
    int                          data[todoc_PayloadSize];
} ST__OTA_CONTROL_PACKET;

#define OTA_RETRY_MAX 3

typedef enum
{
    CI_OTA_STATE_FALL_BACK  = 0,
    CI_OTA_STATE_OTA_UPDATE = 1
} CI_OTA_STATE_E;

typedef enum
{
    CI_OTA_UPDATE_STATE_IDLE         = 0,
    CI_OTA_UPDATE_STATE_BOOT_TRY     = 1,
    CI_OTA_UPDATE_STATE_BOOT_OK      = 2,
    CI_OTA_UPDATE_STATE_BOOT_CONFIRM = 3
} CI_OTA_UPDATE_STATE_E;

typedef enum
{
    CI_OTA_RESULT_SUCCESS = 0,
    CI_OTA_RESULT_FAIL    = 1
} CI_OTA_RESULT_E;

void tdc_dfu_ble_fetch_ota_start_end(const uint8_t *p_packet);
void tdc_dfu_ble_fetch_ota(const uint8_t *p_packet);

int  tdc_dfu_get_conn_state(void);
void tdc_dfu_set_conn_state(int state);

#endif  // __tdc_dfu_ble_ota_h__
