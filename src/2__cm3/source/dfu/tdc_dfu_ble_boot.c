/**
 * @file tdc_dfu_ble_boot.c
 */

#include <tdc_dfu_ble_boot.h>
#include <tdc_boot.h>
#include <tdc_hal_spi.h>
#include <tdc_sys_error.h>
#include <tdc_fs.h>
#include <tdc_qcc.h>

#define _DELAY_MS(ms) Sys_Delay((SystemCoreClock / 1000) * ms)

static void _send_resp_packet_boot(uint8_t *packet_data, uint8_t packet_len)
{
    uint8_t spi_buffer[BLE_DataPacketSize];
    int     spi_len;

    spi_len = packet_len;

    for (int i = 0; i < spi_len; i++)
    {
        spi_buffer[i] = packet_data[i];
    }

    tdc_hal_spi_write_tx_buffer(spi_buffer, spi_len);
}

static void _send_error_packet_boot(uint8_t error)
{
    tdc_sys_error_send_to_app(PKT_HEADER_BOOT, en__EN__BLE_PROTOCOL_ERROR, error, __LINE__);
}

static void _fetch_packet_boot_info(const uint8_t *p_packet)
{
    tdc_boot_status_t boot_status;
    uint8_t           resp_packet[RESP_PKT_SIZE_BOOT_INFO] = {0};

    // get boot status
    if (tdc_boot_get_status(&boot_status) != BOOT_RET_TRUE)
    {
        _send_error_packet_boot(PKT_BOOT_ERROR_FILE_READ);
        return;
    }

    resp_packet[RESP_PKT_IDX_BOOT_INFO_HEADER]             = PKT_HEADER_BOOT;
    resp_packet[RESP_PKT_IDX_BOOT_INFO_OPTION]             = PKT_BOOT_OPTION_INFO;
    resp_packet[RESP_PKT_IDX_BOOT_INFO_RESULT]             = PKT_BOOT_RESULT_ACCEPT;
    resp_packet[RESP_PKT_IDX_BOOT_INFO_VER_MAJOR]          = boot_status.major_ver;
    resp_packet[RESP_PKT_IDX_BOOT_INFO_VER_MINOR]          = boot_status.minor_ver;
    resp_packet[RESP_PKT_IDX_BOOT_INFO_BOOT_SLOT_NUM]      = boot_status.boot_slot_num;
    resp_packet[RESP_PKT_IDX_BOOT_INFO_LAST_BOOT_SLOT_NUM] = boot_status.last_boot_slot_num;

    _send_resp_packet_boot(resp_packet, RESP_PKT_SIZE_BOOT_INFO);
}

static void _fetch_packet_boot_select(const uint8_t *p_packet)
{
    tdc_boot_status_t boot_status;
    uint8_t           slot_num;
    uint8_t           resp_packet[RESP_PKT_SIZE_BOOT_SELECT] = {0};

    // get boot status
    if (tdc_boot_get_status(&boot_status) != BOOT_RET_TRUE)
    {
        _send_error_packet_boot(PKT_BOOT_ERROR_FILE_READ);
        return;
    }

    slot_num = p_packet[RECV_PKT_IDX_BOOT_SELECT_SLOT_NUM];

    resp_packet[RESP_PKT_IDX_BOOT_SELECT_HEADER] = PKT_HEADER_BOOT;
    resp_packet[RESP_PKT_IDX_BOOT_SELECT_OPTION] = PKT_BOOT_OPTION_SELECT;

    // 현재 부팅중인 슬롯인지 확인
    if (slot_num == boot_status.boot_slot_num)
    {
        resp_packet[RESP_PKT_IDX_BOOT_SELECT_RESULT] = PKT_BOOT_RESULT_REJECT_CURRENT_BOOT_SLOT;
        _send_resp_packet_boot(resp_packet, RESP_PKT_SIZE_BOOT_SELECT);
        return;
    }

    // 슬롯 범위 확인
    if ((slot_num < BOOT_SLOT_NUM_BEGIN) || (BOOT_SLOT_NUM_END < slot_num))
    {
        // 공장 초기화 이미지도 아니면 에러.
        if (slot_num != 0xFF)
        {
            resp_packet[RESP_PKT_IDX_BOOT_SELECT_RESULT] = PKT_BOOT_RESULT_REJECT_INVALID_SLOT_NUM;
            _send_resp_packet_boot(resp_packet, RESP_PKT_SIZE_BOOT_SELECT);
            return;
        }
    }

    boot_status.state              = SND_BOOT_STATE_ALT_BOOT;
    boot_status.sub_state          = SND_BOOT_SUB_STATE_BOOT_TRY;
    boot_status.alt_boot_slot_num  = slot_num;
    boot_status.alt_boot_try_count = 0;
    boot_status.alt_boot_result    = SND_BOOT_ALT_BOOT_RESULT_NONE;

    resp_packet[RESP_PKT_IDX_BOOT_SELECT_RESULT] = PKT_BOOT_RESULT_ACCEPT;

    // update boot status
    if (tdc_boot_update_status(&boot_status) != BOOT_RET_TRUE)
    {
        _send_error_packet_boot(PKT_BOOT_ERROR_FILE_WRITE);
        return;
    }

    _send_resp_packet_boot(resp_packet, RESP_PKT_SIZE_BOOT_SELECT);

    /* 응답 패킷 송신 까지 일정 시간 딜레이  */
    SYS_WATCHDOG_REFRESH();
    _DELAY_MS(20);

    /* 2초 이상 QCC_CTRL을 0으로 유지해야 QCC가 꺼진다. */
    tdc_qcc_set_mode(TDC_QCC_MODE_SHUTDOWN);

    for (volatile int i = 0; i < 2200; i++)
    {
        SYS_WATCHDOG_REFRESH();
        _DELAY_MS(1);
    }

    // 선택한 슬롯으로 일정 시간 후 재부팅 시작
    SYS_WATCHDOG_RESET();
}

void tdc_dfu_ble_fetch_boot(const uint8_t *p_packet)
{
    uint8_t opt;

    opt = p_packet[RECV_PKT_IDX_BOOT_OPTION];

    switch (opt)
    {
        case PKT_BOOT_OPTION_INFO:
            TDC_PRINTF("[BOOT] INFO \r\n");
            _fetch_packet_boot_info(p_packet);
            break;

        case PKT_BOOT_OPTION_SELECT:
            TDC_PRINTF("[BOOT] SELECT \r\n");
            _fetch_packet_boot_select(p_packet);
            break;

        default:  // invalid packet option
            _send_error_packet_boot(PKT_BOOT_ERROR_INVALID_OPTION);
            break;
    }
}
