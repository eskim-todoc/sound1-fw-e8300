/*
 * tdc_ble_general_debug.c
 *
 * 범용 디버깅 프로토콜(EN__SND_BT_CMD_GENERAL_DEBUG, 0x8F) 처리.
 * tdc_ble_remote_step.c의 remote command 핸들러에서 분리(2026-07-01).
 * packet->data[0]=option 으로 서브 프로토콜을 분기한다.
 *   option 1 : 터치센서 디버깅  (최근 LTA/count/delta/abs_thr/pressed/ati 상태 조회)
 *   option 2 : 백텔 체크 무시    (OTA DFU 연결 상태 강제 설정)
 *   option 3 : 맵 초기화         (지정 RL로 맵 데이터 강제 초기화)
 *
 * 응답 바이트 포맷은 분리 전 tdc_ble_remote_step.c의 case 블록과 동일하다.
 * 수신 패킷은 인자(const 포인터)로 주입받아 전역 의존 없이 독립 수행된다.
 */

#include <tdc_ble_general_debug.h>

#include <stdint.h>
#include <stdbool.h>

#include <hw.h>  // SYS_WATCHDOG_REFRESH

#include <tdc_ble_remote.h>  // ST__REMOTECONTROL_PACKET, EN__SND_BT_CMD_GENERAL_DEBUG
#include <tdc_touch.h>
#include <tdc_printf.h>
#include <tdc_dfu_ble_ota.h>
#include <tdc_fs_map.h>

static int gd_handle_touch_debug(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index, int option);      // option 1
static int gd_handle_no_backtel(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index, int option);       // option 2
static int gd_handle_map_init(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index, int option);         // option 3
/* option 4(운영 모드 설정)는 fake sleep mode 제거와 함께 삭제됨(2026-07-20).
 * 미지정 option 은 아래 else 에서 command 만 loop-back 하고 무시된다.
 * 상세: docs/tasks/main/20260720_fake-sleep-removal/ */

int tdc_ble_general_debug_handle(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index)
{
    int option = packet->data[0];  // 옵션

    TDC_PRINTF_D("[GD] opt: %d \r\n", option);

    if (option == 1)  // 터치센서 디버깅 프로토콜
    {
        tx_index = gd_handle_touch_debug(packet, tx_buf, tx_index, option);
    }
    else if (option == 2)  // 백텔 체크 무시하기
    {
        tx_index = gd_handle_no_backtel(packet, tx_buf, tx_index, option);
    }
    else if (option == 3)  // 맵 초기화 디버깅 프로토콜
    {
        tx_index = gd_handle_map_init(packet, tx_buf, tx_index, option);
    }
    else
    {
        // 송신 데이터 준비
        tx_buf[tx_index++] = packet->command;  //     command : loop-back
    }

    return tx_index;
}

static int gd_handle_touch_debug(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index, int option)
{
    uint16_t lta;
    uint16_t count;
    uint16_t delta;
    uint16_t abs_thr;
    uint8_t  pressed;
    uint8_t  ati_error;
    uint8_t  ati_active;

    // 송신 데이터 준비
    tx_buf[tx_index++] = packet->command;  //     command : loop-back

    lta        = tdc_touch_debug_get_recent_lta();
    count      = tdc_touch_debug_get_recent_count();
    delta      = tdc_touch_debug_get_recent_delta();
    abs_thr    = tdc_touch_debug_get_recent_abs_thr();
    pressed    = tdc_touch_debug_get_recent_pressed();
    ati_error  = tdc_touch_debug_get_recent_ati_error();
    ati_active = tdc_touch_debug_get_recent_ati_active();

    TDC_PRINTF_I("[GD] lta: %4u, count: %4u, delta: %4u, abs_thr: %4u, ", lta, count, delta, abs_thr);
    TDC_PRINTF_I("pressed: %u, ati_error: %u, ati_active: %u \r\n", pressed, ati_error, ati_active);

    tx_buf[tx_index++] = option;
    tx_buf[tx_index++] = (lta >> 8) & 0x00FF;
    tx_buf[tx_index++] = lta & 0x00FF;
    tx_buf[tx_index++] = (count >> 8) & 0x00FF;
    tx_buf[tx_index++] = count & 0x00FF;
    tx_buf[tx_index++] = (delta >> 8) & 0x00FF;
    tx_buf[tx_index++] = delta & 0x00FF;
    tx_buf[tx_index++] = (abs_thr >> 8) & 0x00FF;
    tx_buf[tx_index++] = abs_thr & 0x00FF;
    tx_buf[tx_index++] = pressed;
    tx_buf[tx_index++] = ati_error;
    tx_buf[tx_index++] = ati_active;

    return tx_index;
}

static int gd_handle_no_backtel(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index, int option)
{
    int noBacktel_mode;

    noBacktel_mode = packet->data[1];  // 옵션 이후 데이터

    if (noBacktel_mode == 1)
    {
        tdc_dfu_set_conn_state(TDC_DFU_CONN_ST_CONN);
    }
    else
    {
        tdc_dfu_set_conn_state(TDC_DFU_CONN_ST_DISCONN);
    }

    TDC_PRINTF_I("[GD] noBacktel_mode : %d \r\n", noBacktel_mode);

    // 송신 데이터 준비
    tx_buf[tx_index++] = packet->command;  //     command : loop-back
    tx_buf[tx_index++] = option;
    tx_buf[tx_index++] = noBacktel_mode;

    return tx_index;
}

static int gd_handle_map_init(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index, int option)
{
    int RL;  // 옵션 이후 데이터

    RL = packet->data[1];  // 옵션 이후 데이터

    if ((RL == 1) || (RL == 2))
    {
        // PARAMETER ORDER : ISD_NUM, FORCE_INIT, SPECIFIC_RL, VAL_RL
        tdc_fs_map_init_map_data(1, true, true, RL);  // 왼쪽 = 1 / 오른쪽 = 2
        SYS_WATCHDOG_REFRESH();
        tdc_fs_map_init_map_data(2, true, false, 1);  // 왼쪽
        SYS_WATCHDOG_REFRESH();
        tdc_fs_map_init_map_data(3, true, false, 1);  // 왼쪽
        SYS_WATCHDOG_REFRESH();
        tdc_fs_map_init_map_data(4, true, false, 1);  // 왼쪽
        SYS_WATCHDOG_REFRESH();
    }
    else
    {
        TDC_PRINTF_W("[GD] Specific RL Command invalid. \r\n");
    }

    // 송신 데이터 준비
    tx_buf[tx_index++] = packet->command;  //     command : loop-back
    tx_buf[tx_index++] = option;
    tx_buf[tx_index++] = RL;

    return tx_index;
}
