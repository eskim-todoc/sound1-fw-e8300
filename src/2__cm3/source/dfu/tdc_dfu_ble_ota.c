/**
 * @file tdc_dfu_ota.c
 */

#include <tdc_dfu_ble_ota.h>
#include <tdc_boot.h>
#include <tdc_hal_spi.h>
#include <tdc_sys_error.h>
#include <tdc_fs.h>
#include <SEGGER_RTT_Wrapper.h>
#include <tdc_hal_dio.h>

static ST__OTA_FILE_WRITE_INFO _g_file_write_info       = {0};
static int                     s_tdc_ota_dfu_conn_state = TDC_DFU_CONN_ST_DISCONN;

int tdc_dfu_get_conn_state(void)
{
    return s_tdc_ota_dfu_conn_state;
}

void tdc_dfu_set_conn_state(int state)
{
    s_tdc_ota_dfu_conn_state = state;
}

char *_get_ota_file_name(int file_type)
{
    switch (file_type)
    {
        case OTA_FILE_TYPE_MANIFEST:
            return "MANIFEST.TXT";

        case OTA_FILE_TYPE_APP000:
            return "APP000.FEZ";

        case OTA_FILE_TYPE_APP001:
            return "APP001.FEZ";

        case OTA_FILE_TYPE_APP002:
            return "APP002.FEZ";

        default:
            return NULL;
    }
}

static void _send_resp_packet_boot(uint8_t *packet_data, uint8_t packet_len)
{
    uint8_t spi_buffer[BLE_DataPacketSize];
    int spi_len;

    spi_len = packet_len;

    for (int i = 0; i < spi_len; i++)
    {
        spi_buffer[i] = packet_data[i];
    }

    tdc_hal_spi_write_tx_buffer(spi_buffer, spi_len);
}

static void _send_error_packet_boot(uint8_t error)
{
    tdc_sys_error_send_to_app(PKT_HEADER_OTA, en__EN__BLE_PROTOCOL_ERROR, error, __LINE__);
}

static void _handle_command_option_write(int slot_num, int file_type, const uint8_t *p_packet)
{
    FILINFO           fno;
    FIL              *fp;
    tdc_boot_status_t boot_status;
    uint8_t           resp_packet[9] = {0};
    int               total_byte;
    int               end_data_index;
    int               end_data_index_byte;
    char             *p_name;
    char              path[20];
    FRESULT           res;

    // get boot status
    if (tdc_boot_get_status(&boot_status) != BOOT_RET_TRUE)
    {
        _send_error_packet_boot(1);  // Error : File read
        return;
    }

    if (slot_num == boot_status.boot_slot_num)
    {
        _send_error_packet_boot(3);  // Reject current boot slot
        return;
    }

    // fp = tdc_fs_get_fp();
    fp = tdc_fs_get_fp();

    // 쓰기 옵션 첫 데이터 인덱스에서 파일을 준비해야 한다.
    // 드라이브를 1에서 0으로 변경해준다.
    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_BOOT));

    // 다른 파일을 처리하는 중이었는지 확인
    if (_g_file_write_info.file_type != 0)
    {
        f_close(fp);
    }

    // 입력 받은 파일 유형에 맞는 파일을 새로 생성
    p_name = _get_ota_file_name(file_type);

    path[0] = '/';
    path[1] = '0' + slot_num;
    path[2] = '/';
    path[3] = 0;

    res = f_stat(path, &fno);

    if (res == FR_NO_FILE)
    {
        res = f_mkdir(path);
    }

    // 슬롯 번호에 해당하는 디렉토리 생성 실패시 에러 처리
    if (res != FR_OK)
    {
        tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));  // NOTE: 디렉토리 생성 실패시 드라이브1로 되돌린다.
        _send_error_packet_boot(2);                                       // File write
        return;
    }

    strcat(path, p_name);  // slot + name

    // Sys_GPIO_Set_High(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

    res = f_open(fp, path, (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));

    // Sys_GPIO_Set_Low(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

    if (res != FR_OK)
    {
        tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));  // NOTE: 파일 오픈 실패시 드라이브1로 되돌린다.
        _send_error_packet_boot(2);                                       // File write
        return;
    }

    // 데이터 인덱스 1에서는 파일을 열고 준비하는 것이니까, 파일의 위치를 0으로 초기화 시켜준다.
    f_lseek(fp, 0);

    total_byte          = (p_packet[8] << 24) | (p_packet[9] << 16) | (p_packet[10] << 8) | p_packet[11];
    end_data_index      = (p_packet[12] << 24) | (p_packet[13] << 16) | (p_packet[14] << 8) | p_packet[15];
    end_data_index_byte = (p_packet[16] << 8) | p_packet[17];

    _g_file_write_info.data_index               = 0;
    _g_file_write_info.slot_num                 = slot_num;
    _g_file_write_info.file_type                = file_type;
    _g_file_write_info.option                   = 1;
    _g_file_write_info.total_byte               = total_byte;
    _g_file_write_info.end_data_index           = end_data_index;
    _g_file_write_info.end_data_index_byte      = end_data_index_byte;
    _g_file_write_info.expected_next_data_index = 1;
    _g_file_write_info.recv_total_byte          = 0;

    resp_packet[0] = PKT_HEADER_OTA;  // Header
    resp_packet[1] = 0;               // Data index (MSB)
    resp_packet[2] = 0;               // Data index
    resp_packet[3] = 0;               // Data index (LSB)
    resp_packet[4] = slot_num;        // Slot num
    resp_packet[5] = file_type >> 8;  // File type (MSB)
    resp_packet[6] = file_type;       // File type (LSB)
    resp_packet[7] = 1;               // Option: write
    resp_packet[8] = 1;               // Result: Success

    RTT_printf("Slot=%u, File='%s' \r\n", _g_file_write_info.slot_num, path);
    RTT_printf("Total byte=%u, end data index=%u, end data index byte=%u \r\n", _g_file_write_info.total_byte, _g_file_write_info.end_data_index, _g_file_write_info.end_data_index_byte);
    _send_resp_packet_boot(resp_packet, 9);

    // NOTE: 지금부터 데이터 인덱스 1부터 지금 생성한 파일에 펌웨어 이미지 쓰기를 시작할 것이기 때문에
    // 여기서 드라이브1로 되돌리지 않아야 한다.
}

static void _handle_command_option_read(int slot_num, int file_type, const uint8_t *p_packet)
{
    uint8_t resp_packet[9] = {0};

    resp_packet[0] = PKT_HEADER_OTA;  // Header
    resp_packet[1] = 0;               // Data index (MSB)
    resp_packet[2] = 0;               // Data index
    resp_packet[3] = 0;               // Data index (LSB)
    resp_packet[4] = slot_num;        // Slot num
    resp_packet[5] = file_type >> 8;  // File type (MSB)
    resp_packet[6] = file_type;       // File type (LSB)
    resp_packet[7] = 2;               // Option: Read
    resp_packet[8] = 1;               // Result: Success

    _send_resp_packet_boot(resp_packet, 9);
}

static void _handle_command_option_size(int slot_num, int file_type, const uint8_t *p_packet)
{
    FILINFO fno;
    uint8_t resp_packet[13] = {0};
    char   *p_name;
    char    path[20];
    FRESULT res;

    // 입력 받은 파일 유형에 맞는 파일을 새로 생성
    p_name = _get_ota_file_name(file_type);

    path[0] = '/';
    path[1] = '0' + slot_num;
    path[2] = '/';
    path[3] = 0;

    strcat(path, p_name);  // slot + name

    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_BOOT));  // NOTE: 드라이브 0으로 변경 후 정보를 읽고

    res = f_stat(path, &fno);

    tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));  // NOTE: 드라이브1로 다시 되돌린다.

    if (res != FR_OK)
    {
        _send_error_packet_boot(1);  // File read
        return;
    }

    for (int i = 0; i < 8; i++)
    {
        resp_packet[i] = p_packet[i];
    }

    resp_packet[8]  = 1;  // Result: Success
    resp_packet[9]  = fno.fsize >> 24;
    resp_packet[10] = fno.fsize >> 16;
    resp_packet[11] = fno.fsize >> 8;
    resp_packet[12] = fno.fsize;

    _send_resp_packet_boot(resp_packet, 13);
}

static void _fetch_packet_command(const uint8_t *p_packet)
{
    int slot_num;
    int file_type;
    int option;

    slot_num  = p_packet[4];
    file_type = (p_packet[5] << 8) | p_packet[6];
    option    = p_packet[7];

    if ((slot_num < 1) || (2 < slot_num))
    {
        // 에러
        _send_error_packet_boot(3);  // Invalid slot num
    }

    if ((file_type < 1) || (4 < file_type))
    {
        // 에러
        _send_error_packet_boot(4);  // Invalid file type
    }

    switch (option)
    {
        case 1:  // Write
            _handle_command_option_write(slot_num, file_type, p_packet);
            break;

        case 2:  // Read
            _handle_command_option_read(slot_num, file_type, p_packet);
            break;

        case 3:  // Size
            _handle_command_option_size(slot_num, file_type, p_packet);
            break;

        default:                         // 에러
            _send_error_packet_boot(5);  // Invalid option
            return;
    }
}

static void _fetch_packet_data(const uint8_t *p_packet, int data_index)
{
    UINT    btw;
    UINT    bw;
    uint8_t wbuf[16];
    uint8_t fbuf[16];
    FRESULT res;
    FIL    *fp;

    // fp = tdc_fs_get_fp();
    fp = tdc_fs_get_fp();

    wbuf[0] = p_packet[0];  // Header
    wbuf[1] = p_packet[1];  // Data index (MSB)
    wbuf[2] = p_packet[2];
    wbuf[3] = p_packet[3];  // Data index (LSB)

    // 예상했던 다음 데이터 인덱스랑, 지금 받은 데이터 인덱스가 다르면 에러다.
    // 파일을 닫고 에러 응답을 보내야한다.
    if (data_index != _g_file_write_info.expected_next_data_index)
    {
        // Sys_GPIO_Set_High(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

        f_close(fp);
        tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));  // NOTE: 데이터 인덱스 에러 발생이므로 드라이브0에서 드라이브1로 되돌린다.

        // Sys_GPIO_Set_Low(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

        _g_file_write_info.file_type = 0;
        wbuf[4]                      = 2;  // Result: Invalid data index
        _send_resp_packet_boot(wbuf, 5);
        return;
    }

    if (data_index == _g_file_write_info.end_data_index)
    {
        btw = _g_file_write_info.end_data_index_byte;
    }
    else
    {
        btw = 16;
    }

    for (int i = 0; i < btw; i++)
    {
        fbuf[i] = p_packet[4 + i];
    }

    bw = 0;
    // Sys_GPIO_Set_High(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

    res = f_write(fp, fbuf, btw, &bw);

    // Sys_GPIO_Set_Low(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

    // 파일 데이터 쓰기에 실패하면 에러 처리 해야한다.
    if ((res != FR_OK) || (bw != btw))
    {
        f_close(fp);

        tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));  // NOTE: 파일 데이터 쓰기를 실패 했으므로 드라이브0에서 드라이브1로 되돌린다.

        _g_file_write_info.file_type = 0;
        _send_error_packet_boot(2);  // File write
        return;
    }

    // 파일 데이터 쓰기를 마지막 데이터 인덱스까지 모두 성공했다.
    if (data_index == _g_file_write_info.end_data_index)
    {
        _g_file_write_info.file_type = 0;

        // Sys_GPIO_Set_High(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

        f_close(fp);
        tdc_util_assert(tdc_fs_fatfs_remount(SND_FATFS_LDRV_NUM_USER_DATA));  // NOTE: 파일 데이터 쓰기를 마지막 데이터 인덱스까지 모두 성공 했으므로 드라이브0에서 드라이브1로 되돌린다.

        // Sys_GPIO_Set_Low(DIO_NUM_OTA_DETAIL_DEBUG);  // NOTE: OTA DEBUG

        RTT_printf("[OTA] WRITING DONE. \r\n");
    }
    else
    {
        _g_file_write_info.expected_next_data_index++;
    }

    wbuf[4] = 1;  // Result: Success
    _send_resp_packet_boot(wbuf, 5);
}

void tdc_dfu_ble_fetch_ota_start_end(const uint8_t *p_packet)
{
    uint8_t wbuf[16];

    wbuf[0] = p_packet[0];  // Header
    wbuf[1] = 0;            // RSP code: ACK

    if (p_packet[0] == CI_BLE_OTA_COMMAND_OTA_START)
    {
        wbuf[2] = p_packet[2];  // Target(req_type = write)
    }
    else if (p_packet[0] == CI_BLE_OTA_COMMAND_OTA_END)
    {
        wbuf[2] = p_packet[1];  // Target
    }

    _send_resp_packet_boot(wbuf, 3);
}

void tdc_dfu_ble_fetch_ota(const uint8_t *p_packet)
{
    int data_index;

    data_index = (p_packet[1] << 16) | (p_packet[2] << 8) | p_packet[3];

    TDC_PRINTF("[OTA] DATA INDEX %d \r\n", data_index);

    // 데이터 인덱스가 0일 때만 특별한 처리를 수행한다.
    if (data_index == 0)
    {
        _fetch_packet_command(p_packet);
    }
    else
    {
        _fetch_packet_data(p_packet, data_index);
    }
}
