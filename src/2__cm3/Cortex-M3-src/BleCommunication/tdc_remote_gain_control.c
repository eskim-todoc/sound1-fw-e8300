/*
 * tdc_remote_gain_control.c
 *
 * Gain control 프로토콜(EN__SND_BT_CMD_GAIN_CONTROL, 0x8C) 처리.
 * 앱이 QCC 를 거쳐 E8300 까지 보내는 패킷으로 Gain Conversion Table 의
 * 인덱스를 읽거나 쓴다. 설정값은 공유 메모리에 기록되어 CFX 의 믹싱 단계에서
 * Gain_A(마이크) · Gain_B(I2S 크래들 마이크)로 적용된다.
 *
 * 수신 패킷은 인자(const 포인터)로 주입받아 전역 의존 없이 독립 수행된다.
 * (0x8F 범용 디버깅 프로토콜 분리 방식과 동일)
 */

#include "tdc_remote_gain_control.h"

#include <stdbool.h>

#include "cfx_cm3_sharedMemory.h"  // cfx_cm3_sharedMemoryAll
#include "remoteControl.h"         // ST__REMOTECONTROL_PACKET
#include <TDC_PRINTF.h>
#include <tdc_fs_gain.h>

static bool gc_is_valid_request(int control_type, int gain_type, int gain_index);
static int  gc_read_index(int gain_type);
static void gc_write_index(int gain_type, int gain_index);
static bool gc_save_to_file(void);

void tdc_remote_gain_control_init(void)
{
    cfx_cm3_sharedMemoryAll.gain_table_index_a   = TDC_FS_GAIN_TABLE_INDEX_DEFAULT_A;
    cfx_cm3_sharedMemoryAll.gain_table_index_b   = TDC_FS_GAIN_TABLE_INDEX_DEFAULT_B;
    cfx_cm3_sharedMemoryAll.is_i2s_source_cradle = 0;
}

int tdc_remote_gain_control_handle(const ST__REMOTECONTROL_PACKET *packet, int *tx_buf, int tx_index)
{
    int control_type = packet->data[0];  // 0 = Read, 1 = Write
    int gain_type    = packet->data[1];  // 1 = Gain_A, 2 = Gain_B
    int gain_index   = packet->data[2];  // 0~255

    int rsp_code      = TDC_GAIN_RSP_FAILED;
    int response_index = gain_index;  // 실패 시에는 수신값을 그대로 되돌려 준다.

    if (gc_is_valid_request(control_type, gain_type, gain_index))
    {
        rsp_code = TDC_GAIN_RSP_SUCCESS;

        if (control_type == TDC_GAIN_CONTROL_TYPE_WRITE)
        {
            gc_write_index(gain_type, gain_index);

            // 파일 저장까지 성공해야 이전 설정이 유지되므로, 실패는 응답으로 알린다.
            // 저장에 실패해도 공유 메모리는 되돌리지 않는다. 소리는 이미 바뀌었고,
            // 이번 전원 주기 동안은 그대로 쓸 수 있기 때문이다.
            if (!gc_save_to_file())
            {
                rsp_code = TDC_GAIN_RSP_FAILED;
            }
        }

        // Read · Write 모두 현재 적용 중인 값으로 응답한다.
        response_index = gc_read_index(gain_type);
    }

    TDC_PRINTF_I("[GAIN] ctrl: %d, type: %d, idx: %d, rsp: %d \r\n", control_type, gain_type, response_index, rsp_code);

    // 송신 데이터 준비
    tx_buf[tx_index++] = packet->command;  // command : loop-back
    tx_buf[tx_index++] = rsp_code;
    tx_buf[tx_index++] = control_type;
    tx_buf[tx_index++] = gain_type;
    tx_buf[tx_index++] = response_index;

    return tx_index;
}

static bool gc_is_valid_request(int control_type, int gain_type, int gain_index)
{
    if ((control_type != TDC_GAIN_CONTROL_TYPE_READ) && (control_type != TDC_GAIN_CONTROL_TYPE_WRITE))
    {
        TDC_PRINTF_W("[GAIN] INVALID CONTROL TYPE: %d \r\n", control_type);
        return false;
    }

    if ((gain_type != TDC_GAIN_TYPE_A) && (gain_type != TDC_GAIN_TYPE_B))
    {
        TDC_PRINTF_W("[GAIN] INVALID GAIN TYPE: %d \r\n", gain_type);
        return false;
    }

    // 읽기 명령의 인덱스 필드는 사용하지 않으므로 범위를 따지지 않는다.
    if (control_type == TDC_GAIN_CONTROL_TYPE_WRITE)
    {
        if ((gain_index < TDC_FS_GAIN_TABLE_INDEX_MIN) || (TDC_FS_GAIN_TABLE_INDEX_MAX < gain_index))
        {
            TDC_PRINTF_W("[GAIN] INVALID TABLE INDEX: %d \r\n", gain_index);
            return false;
        }
    }

    return true;
}

static int gc_read_index(int gain_type)
{
    if (gain_type == TDC_GAIN_TYPE_A)
    {
        return cfx_cm3_sharedMemoryAll.gain_table_index_a;
    }

    return cfx_cm3_sharedMemoryAll.gain_table_index_b;
}

static void gc_write_index(int gain_type, int gain_index)
{
    if (gain_type == TDC_GAIN_TYPE_A)
    {
        cfx_cm3_sharedMemoryAll.gain_table_index_a = gain_index;
    }
    else
    {
        cfx_cm3_sharedMemoryAll.gain_table_index_b = gain_index;
    }
}

/* 현재 연결된 ISD 슬롯에 게인 설정을 저장한다. 저장까지 성공하면 true. */
static bool gc_save_to_file(void)
{
    tdc_fs_gain_setting_t gain_setting;
    int                  isd_num = read_connected_ISD_Num();

    // 매핑 모드에서는 ISD 슬롯 번호가 임시 값이라 엉뚱한 사용자 자리에 쓸 수 있다.
    if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode != en__normalMode)
    {
        TDC_PRINTF_W("[GAIN] SAVE SKIPPED (NOT NORMAL MODE) \r\n");
        return false;
    }

    // ISD 가 연결되지 않으면 QCC 가 블루투스를 켜지 않으므로 여기까지 올 일이 없다.
    // 그래도 도달했다면 비정상 상황이므로 저장하지 않고 실패로 알린다.
    if ((isd_num < 1) || (MaxNumUser < isd_num))
    {
        TDC_PRINTF_W("[GAIN] SAVE SKIPPED (ISD NOT CONNECTED : %d) \r\n", isd_num);
        return false;
    }

    gain_setting.gain_table_index_a = cfx_cm3_sharedMemoryAll.gain_table_index_a;
    gain_setting.gain_table_index_b = cfx_cm3_sharedMemoryAll.gain_table_index_b;

    return (tdc_fs_gain_save(isd_num, &gain_setting) == TDC_FS_GAIN_RET_OK);
}
