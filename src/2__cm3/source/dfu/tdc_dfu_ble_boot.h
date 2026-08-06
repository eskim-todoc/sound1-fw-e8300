/**
 * @file tdc_dfu_ble_boot.h
 */

#ifndef __tdc_dfu_ble_boot_h__
#define __tdc_dfu_ble_boot_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_ble_protocol.h>

#include <tdc_dfu_sdk_boot.h>

// 패킷 헤더
typedef enum
{
    PKT_HEADER_BOOT = 0xC2
} EN__PKT_HEADER_BOOT;

// 옵션에 따른 수신 패킷 사이즈
typedef enum
{
    RECV_PKT_SIZE_BOOT_INFO   = 3,
    RECV_PKT_SIZE_BOOT_SELECT = 3,
} EN__RECV_PKT_SIZE_BOOT;

// 옵션에 따른 응답 패킷 사이즈
typedef enum
{
    RESP_PKT_SIZE_BOOT_INFO   = 7,
    RESP_PKT_SIZE_BOOT_SELECT = 3,
} EN__RESP_PKT_SIZE_BOOT;

// 공통 수신 패킷 인덱스
typedef enum
{
    RECV_PKT_IDX_BOOT_HEADER = 0,
    RECV_PKT_IDX_BOOT_OPTION = 1,
} EN__RECV_PKT_IDX_BOOT;

// 공통 응답 패킷 인덱스
typedef enum
{
    RESP_PKT_IDX_BOOT_HEADER = 0,
    RESP_PKT_IDX_BOOT_OPTION = 1,
    RESP_PKT_IDX_BOOT_RESULT = 2,
} EN__RESP_PKT_IDX_BOOT;

// 옵션=INFO의 수신 패킷 인덱스 정보
typedef enum
{
    RECV_PKT_IDX_BOOT_INFO_HEADER = RECV_PKT_IDX_BOOT_HEADER,
    RECV_PKT_IDX_BOOT_INFO_OPTION = RECV_PKT_IDX_BOOT_OPTION,
} EN__RECV_PKT_IDX_BOOT_INFO;

// 옹셥=INFO의 응답 패킷 인덱스 정보
typedef enum
{
    RESP_PKT_IDX_BOOT_INFO_HEADER             = RESP_PKT_IDX_BOOT_HEADER,
    RESP_PKT_IDX_BOOT_INFO_OPTION             = RESP_PKT_IDX_BOOT_OPTION,
    RESP_PKT_IDX_BOOT_INFO_RESULT             = RESP_PKT_IDX_BOOT_RESULT,
    RESP_PKT_IDX_BOOT_INFO_VER_MAJOR          = 3,
    RESP_PKT_IDX_BOOT_INFO_VER_MINOR          = 4,
    RESP_PKT_IDX_BOOT_INFO_BOOT_SLOT_NUM      = 5,
    RESP_PKT_IDX_BOOT_INFO_LAST_BOOT_SLOT_NUM = 6,
} EN__RESP_PKT_IDX_BOOT_INFO;

// 옵션=SELECT의 수신 패킷 인덱스 정보
typedef enum
{
    RECV_PKT_IDX_BOOT_SELECT_HEADER   = RECV_PKT_IDX_BOOT_HEADER,
    RECV_PKT_IDX_BOOT_SELECT_OPTION   = RECV_PKT_IDX_BOOT_OPTION,
    RECV_PKT_IDX_BOOT_SELECT_SLOT_NUM = 2,
} EN__RECV_PKT_IDX_BOOT_SELECT;

// 옵션=SELECT의 응답 패킷 인덱스 정보
typedef enum
{
    RESP_PKT_IDX_BOOT_SELECT_HEADER = RESP_PKT_IDX_BOOT_HEADER,
    RESP_PKT_IDX_BOOT_SELECT_OPTION = RESP_PKT_IDX_BOOT_OPTION,
    RESP_PKT_IDX_BOOT_SELECT_RESULT = RESP_PKT_IDX_BOOT_RESULT,
} EN__RESP_PKT_IDX_BOOT_SELECT;

// 옵션 리스트
typedef enum
{
    PKT_BOOT_OPTION_INFO   = 1,
    PKT_BOOT_OPTION_SELECT = 2,
} EN__PKT_BOOT_OPTION;

// RESULT 리스트
typedef enum
{
    PKT_BOOT_RESULT_ACCEPT                   = 1,
    PKT_BOOT_RESULT_REJECT_INVALID_SLOT_NUM  = 2,
    PKT_BOOT_RESULT_REJECT_CURRENT_BOOT_SLOT = 3,
} EN__PKT_BOOT_RESULT;

// ERROR 리스트
typedef enum
{
    PKT_BOOT_ERROR_FILE_READ      = 1,
    PKT_BOOT_ERROR_FILE_WRITE     = 2,
    PKT_BOOT_ERROR_INVALID_OPTION = 3,
} EN__PKT_BOOT_ERROR;

// 운용할 수 있는 슬롯의 시작, 끝 번호
typedef enum
{
    BOOT_SLOT_NUM_BEGIN = 1,
    BOOT_SLOT_NUM_1     = BOOT_SLOT_NUM_BEGIN,
    BOOT_SLOT_NUM_2     = 2,
    BOOT_SLOT_NUM_END   = BOOT_SLOT_NUM_2,
} EN__BOOT_SLOT_NUM;

void tdc_dfu_ble_fetch_boot(const uint8_t *p_packet);

#endif  // __tdc_dfu_ble_boot_h__
