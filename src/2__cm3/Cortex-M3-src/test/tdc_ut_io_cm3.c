/* CM3 I/O 시험 러너. 설정/입력/출력/복원 계약은 각 벡터와 헤더에 있다.
 * 평시 번역 단위는 비어 있다. 디버거가 명시적으로 run_unit을 호출해야 한다.
 * 실제 함수의 결과만 actual에 기록하며 expected는 독립된 명세 값이다.
 */
#if defined(TDC_UNIT_TEST)
#include "tdc_ut_io_cm3.h"
#include <cfx_cm3_sharedMemory.h>
#include <tdc_remote_gain_control.h>
#include <ci_ble_control_ota.h>
#include <isd_interface_FPGA.h>
#include <driver_REN_ISL9122.h>
#include <tdc_touch_iqs323.h>
#include <isd_interface.h>
#include <isd_interface_stimulationParaSetting.h>
#include <mappingControl.h>
#include <ble_communication.h>
#include <batteryNPowerControl.h>
#include <LedOutput.h>
/* Root 시험 지원의 기존 교환 API를 사용하며 error.c는 이 작업에서 수정하지 않는다. */
extern void tdc_ut_cm3_error_exchange(ST__ERROR_CODE *state);
extern volatile int g_tdc_ut_cm3_vector_done;
extern int g_tdc_ut_cm3_suppress_diagnostics;
extern void tdc_ut_cm3_battery_exchange(int state[4]);
extern void tdc_ut_cm3_led_exchange(int state[LED_SRC__MAX + 3]);
extern void fetch_readDataForBleSetting(const int *packet);
extern void setting_nrf_ble_adv_info(void);

#if defined(TDC_UT_IO_CM3_HOST)
#include "tdc_ut_io_cm3_host_fixture.h"
#include <ci_ble_control_boot.h>
#include <dirver_i2c_for_ISD.h>
#endif

int g_tdc_ut_io_cm3_actual[TDC_UT_IO_CM3_CAPACITY];
int g_tdc_ut_io_cm3_expected[TDC_UT_IO_CM3_CAPACITY];
int g_tdc_ut_io_cm3_count;
int g_tdc_ut_io_cm3_overflow;
volatile int g_tdc_ut_io_cm3_done;
int g_tdc_ut_io_cm3_observed;
static int s_tdc_io_running;
static int s_tdc_spi_ready;
static int s_tdc_spi_auto_ack;
static int s_tdc_gain_save_error;
static int s_tdc_gain_saved[MaxNumUser][2];
static ST__CFX_CM3_SharedMemory_ALL s_tdc_saved_shared;
static int s_tdc_bus_rx[4], s_tdc_bus_rx_index, s_tdc_bus_error;
static int s_tdc_bus_addr, s_tdc_bus_count, s_tdc_bus_write[4];
static FIL s_tdc_ota_file;
static snd_boot_status_t s_tdc_ota_boot;
static unsigned char s_tdc_ota_bytes[64];
static char s_tdc_ota_path[32];
static unsigned s_tdc_ota_offset, s_tdc_ota_length;
static int s_tdc_ota_drive, s_tdc_ota_close_count;
static int s_tdc_ota_open_error, s_tdc_ota_write_error, s_tdc_ota_short_write;
static uint32_t s_tdc_i2c_status, s_tdc_i2c_rx, s_tdc_i2c_ctrl;
static int s_tdc_i2c_start, s_tdc_i2c_addr, s_tdc_i2c_tx[4], s_tdc_i2c_tx_count, s_tdc_i2c_reset_count;
static void put(int actual, int expected);
static void mark(int unit, int vector, int count);

/* 설정: RAM 응답 및 오류를 지정. 입력: 실제 함수가 조립한 주소/데이터.
 * 출력: 실제 요청 주소/쓰기 데이터와 주입 RX. 전송 결과만 대체한다.
 * 복원: run_unit이 원래 훅을 복원하며 실제 I2C/핀에는 접근하지 않는다.
 */
static int bus_ram(int read, int addr, int *data, int count)
{
    s_tdc_bus_addr = addr;
    if (count < 0 || count > 4 || s_tdc_bus_error) return 0;
    if (read)
    {
        if (s_tdc_bus_rx_index + count > 4) return 0;
        for (int idx = 0; idx < count; idx++) data[idx] = s_tdc_bus_rx[s_tdc_bus_rx_index++];
    }
    else
    {
        s_tdc_bus_count = count;
        for (int idx = 0; idx < count; idx++) s_tdc_bus_write[idx] = data[idx];
    }
    return 1;
}
static int touch_ram(int read, unsigned char *data, int count)
{
    int values[4] = {0};
    if (count < 0 || count > 4) return 0;
    if (!read) for (int idx = 0; idx < count; idx++) values[idx] = data[idx];
    int result = bus_ram(read, TDC_TOUCH_IQS323_SLAVE_ADDR, values, count);
    if (read && result) for (int idx = 0; idx < count; idx++) data[idx] = (unsigned char)values[idx];
    return result;
}
static void bus_input(int a, int b, int c, int d)
{
    s_tdc_bus_rx[0] = a; s_tdc_bus_rx[1] = b; s_tdc_bus_rx[2] = c; s_tdc_bus_rx[3] = d;
    s_tdc_bus_rx_index = s_tdc_bus_error = 0;
}

/* CM3-10 RAM: 레지스터 조립과 상태 해석은 실제 센서 함수다.
 * RDY/타이밍/버스만 시험 경계이며 RDY 타임아웃 자체는 호스트/실기 별도 범위다.
 */
static void vector_touch_ram(void)
{
    static const int input[][2] = {{0x20,0},{0,3},{0x60,0},{0xEE,0xEE}};
    tdc_touch_iqs323_status_t status;
    for (int vector = 0; vector < 4; vector++)
    {
        bus_input(input[vector][0], input[vector][1], 0, 0);
        mark(10, vector + 1, 8);
        put(tdc_touch_iqs323_read_status(&status), vector != 3);
        put(status.ok, vector != 3); put(status.ati_active, vector == 0 || vector == 2);
        put(status.ati_error, vector == 2); put(status.pressed, vector == 1); put(status.prox, vector == 1);
        put(s_tdc_bus_addr, 0x44); put(s_tdc_bus_write[0], 0x10);
    }
    bus_input(0x20,0,0,0); mark(10,5,1); put(tdc_touch_iqs323_is_ati_done(),0);
    bus_input(0,0,0,0); mark(10,6,1); put(tdc_touch_iqs323_is_ati_done(),1);
    mark(10,7,10);
    tdc_touch_iqs323_set_ulp(); put(tdc_touch_iqs323_is_ulp(),1);
    tdc_touch_iqs323_clear_ulp(); put(tdc_touch_iqs323_is_ulp(),0);
    put(tdc_touch_iqs323_re_ati(),1); put(s_tdc_bus_write[0],0xC0); put(s_tdc_bus_write[1],0x54); put(s_tdc_bus_write[2],7);
    put(tdc_touch_iqs323_reseed(),1); put(s_tdc_bus_write[0],0xC0); put(s_tdc_bus_write[1],0x58); put(s_tdc_bus_write[2],7);
    s_tdc_bus_error=1; mark(10,8,4);
    put(tdc_touch_iqs323_read_status(&status),0); put(status.ok,0);
    put(tdc_touch_iqs323_re_ati(),0); put(tdc_touch_iqs323_reseed(),0);
    bus_input(0x34,0x12,0x78,0x56);
    tdc_touch_iqs323_debug_t debug; mark(10,9,4);
    put(tdc_touch_iqs323_read_debug(&debug),1); put(debug.ok,1); put(debug.counts,0x1234); put(debug.lta,0x5678);
}

/* CM3-34/35/36/40 RAM: 실제 FPGA 및 현 보드 ISL9122 함수를 호출한다.
 * 레지스터/FIFO 응답만 준비하고 호출 뒤 캐시/공유 상태는 외부 run_unit이 복원한다.
 */
static void vector_device_ram(int unit)
{
    bool flag=false; int value=0;
    reset_Fpga_variable(); bus_input(0,0,0,0);
    if (unit==34)
    {
        mark(34,1,9);
        put(write_FPGA_systemResgister_1st(0x1C),1); put(write_FPGA_enable_RF_tx(),1);
        put(get_last_fpga_systemResgister_1st_written_Value(),0x1E); put(s_tdc_bus_write[0],0x1E);
        put(write_FPGA_disable_RF_tx(),1); put(get_last_fpga_systemResgister_1st_written_Value(),0x1C); put(s_tdc_bus_write[0],0x1C);
        put(write_FPGA_reset(),1); put(s_tdc_bus_write[0],1);
    }
    else if (unit==35)
    {
        int input[2]={MinTxPowerValue,MaxVoltageControlValue};
        for(int vector=0;vector<2;vector++)
        {
            bus_input(input[vector],0,0,0); mark(35,vector+1,6);
            put(write_change_TxPowerLevel(input[vector]),1); put(s_tdc_bus_addr,0x18);
            put(s_tdc_bus_write[0],REN_ISL9122_registerAddr_VoltageSet); put(s_tdc_bus_write[1],input[vector]);
            put(read_txPowerLevel(&value),1); put(value,input[vector]);
        }
    }
    else if(unit==36)
    {
        for(int vector=0;vector<6;vector++)
        {
            bus_input(vector==0?0:(1<<(vector-1)),0,0,0); mark(36,vector+1,3);
            put(check_FPGA_PCM_Error(&flag),1); put(flag,vector!=0); put(s_tdc_bus_addr,i2cAddr_FPGA_error_Flag);
        }
        for(int vector=0;vector<2;vector++)
        {
            bus_input(vector?(1<<FPGA_BitPosition_FIFO_is_empty):0,0,0,0); mark(36,vector+7,3);
            put(check_FPGA_FIFO_empty(&flag),1); put(flag,vector); put(s_tdc_bus_addr,i2cAddr_FPGA_systemResgister_2nd);
        }
        int fifo[2]={0}; bus_input(0x12,0x34,0,0); mark(36,9,4);
        put(read_FPGA_backtel_FIFO(fifo,2),1); put(fifo[0],0x12); put(fifo[1],0x34); put(s_tdc_bus_rx_index,2);
        bus_input(0xA5,0,0,0); mark(36,10,3);
        put(read_FPGA_version(&value),1); put(value,0xA5); put(s_tdc_bus_addr,i2cAddr_FPGA_version);
    }
    else
    {
        bus_input(0x5A,0,0,0); mark(40,1,5);
        put(read_REN_ISL9122_register_byCM3_I2C(0x11,&value),1); put(value,0x5A);
        put(s_tdc_bus_addr,0x18); put(s_tdc_bus_count,1); put(s_tdc_bus_write[0],0x11);
        bus_input(0x81,75,0,0); mark(40,2,3);
        put(Reset_REN_ISL9122(),1); put(s_tdc_bus_write[0],REN_ISL9122_registerAddr_VoltageSet); put(s_tdc_bus_rx_index,2);
        s_tdc_bus_error=1; mark(40,3,2);
        put(read_REN_ISL9122_register_byCM3_I2C(0x11,&value),0); put(write_REN_ISL9122_register_byCM3_I2C(0x11,0x12),0);
        s_tdc_bus_error=0;
    }
}

static void put(int actual, int expected)
{
    int idx = g_tdc_ut_io_cm3_count;
    if (idx >= TDC_UT_IO_CM3_CAPACITY) { g_tdc_ut_io_cm3_overflow = 1; return; }
    g_tdc_ut_io_cm3_actual[idx] = actual;
    g_tdc_ut_io_cm3_expected[idx] = expected;
    g_tdc_ut_io_cm3_count = idx + 1;
}
static void mark(int unit, int vector, int count)
{
    put(-999, -999); put(unit, unit); put(vector, vector); put(count, count);
}
static int spi_ram(int phase)
{
    if (phase < 2) s_tdc_spi_ready = phase;
    /* OTA can publish error then READ response in one call. With the real
     * driver and stopped DMA, the second publication would wait forever.
     * Only its RAM fixture acknowledges each completed TX through the existing
     * SPI boundary. Buffer contents and the erroneous second response remain
     * observable; this does not simulate correct product error handling.
     */
    if (phase == 1 && s_tdc_spi_auto_ack) clear_ReadCommandForSPI_Master();
    return 1;
}
static int gain_ram(int slot, int a, int b)
{
    if (s_tdc_gain_save_error) return TDC_GAIN_STORAGE_RET_FAIL;
    s_tdc_gain_saved[slot - 1][0] = a;
    s_tdc_gain_saved[slot - 1][1] = b;
    return TDC_GAIN_STORAGE_RET_OK;
}
static void ack(void) { clear_ReadCommandForSPI_Master(); }

static uint32_t i2c_status_ram(void) { return s_tdc_i2c_status; }
static uint32_t i2c_rx_ram(void) { return s_tdc_i2c_rx; }
static void i2c_action_ram(int action, uint32_t value)
{
    if (action == 2 || action == 3) { s_tdc_i2c_start = action; s_tdc_i2c_addr = (int)value; }
    if (action == 4 && s_tdc_i2c_tx_count < 4) s_tdc_i2c_tx[s_tdc_i2c_tx_count++] = (int)value;
    if (action == 5) s_tdc_i2c_ctrl = value;
    if (action == 6) s_tdc_i2c_reset_count++;
}
static const tdc_ut_io_cm3_i2c_ops_t s_tdc_i2c_ops = { i2c_status_ram, i2c_rx_ram, i2c_action_ram };
static void i2c_input(int read, int stop, int bus_error, int free_line)
{
    OTE_1_5gen_I2C_STATUS_T status = {0};
    status.fields.busy = !free_line && !stop;
    status.fields.clk_stretch = 1; status.fields.stop_detected = stop;
    status.fields.read_write = read; status.fields.bus_error = bus_error;
    status.fields.line_free = free_line; status.fields.tx_req = !read;
    status.fields.rx_req = read && !stop;
    s_tdc_i2c_status = status.status;
}

/* CM3-25 RAM: 실제 start/ISR의 2바이트 쓰기/읽기, 완료, 버스 오류, busy를 실행한다.
 * 고정된 상태/데이터 레지스터 응답만 대신 넣는다. NVIC/실제 버스는 접근하지 않는다.
 * IRQ 실제 발생/클럭과 init_I2c 레지스터 설정은 이 벡터의 판정 범위가 아니다.
 */
static void vector_i2c_ram(void)
{
    int source[2] = {0x56,0x78}, dest[2] = {0};
    g_tdc_ut_io_cm3_i2c_ops = &s_tdc_i2c_ops;
    g_tdc_ut_io_cm3_i2c_poll_hook = NULL;
    clearI2cDriverStatus(); s_tdc_i2c_tx_count = s_tdc_i2c_reset_count = 0;
    i2c_input(0,0,0,1); i2c_startWriteData(0x44, source, 2);
    mark(25,1,8); put(get_i2cDriverStatus(), i2c_state_WriteTriggered);
    i2c_input(0,0,0,0); I2C_0_IRQHandler(); I2C_0_IRQHandler();
    put(s_tdc_i2c_start, 2); put(s_tdc_i2c_addr, 0x44); put(s_tdc_i2c_tx_count, 2);
    put(s_tdc_i2c_tx[0], 0x56); put(s_tdc_i2c_tx[1], 0x78); put(s_tdc_i2c_ctrl, I2C_LAST_DATA);
    i2c_input(0,1,0,0); I2C_0_IRQHandler(); put(get_i2cDriverStatus(), i2c_state_WritingDone);
    setI2cDriverStatusIdle();
    i2c_input(0,0,0,1); i2c_startReadData(0x44, dest, 2);
    mark(25,2,8); put(get_i2cDriverStatus(), i2c_state_ReadTriggerd);
    put(s_tdc_i2c_start, 3); put(s_tdc_i2c_addr, 0x44);
    i2c_input(1,0,0,0); s_tdc_i2c_rx = 0x12; I2C_0_IRQHandler(); put(s_tdc_i2c_ctrl, I2C_ACK);
    s_tdc_i2c_rx = 0x34; I2C_0_IRQHandler(); put(s_tdc_i2c_ctrl, I2C_NACK | I2C_STOP);
    i2c_input(1,1,0,0); I2C_0_IRQHandler(); put(get_i2cDriverStatus(), i2c_state_ReadingDone);
    put(dest[0], 0x12); put(dest[1], 0x34); setI2cDriverStatusIdle();
    i2c_input(0,0,0,1); i2c_startReadData(0x44, dest, 2);
    i2c_input(1,1,1,0); I2C_0_IRQHandler();
    mark(25,3,4); put(get_i2cDriverStatus(), i2c_state_Error);
    ST__I2C_DRIVER state; tdc_ut_io_cm3_i2c_snapshot(&state); put(state.i2c_Error_Code, I2C_SLAVE_DEVICE_NO_REACTION);
    init_I2c(); put(isI2cDriverStatusIdle(), 1); put(s_tdc_i2c_reset_count, 1);
    i2c_input(0,0,0,0); s_tdc_i2c_start = 0;
    i2c_startWriteData(0x44, source, 2);
    mark(25,4,3); put(get_i2cDriverStatus(), i2c_state_Error);
    put(s_tdc_i2c_start, 0); init_I2c(); put(isI2cDriverStatusIdle(), 1);
}

/* 파일 경계는 메모리만 사용한다. 실제 parser의 분기/상태/응답은 대체하지 않는다.
 * 함수 인자 길이를 먼저 검사하여 잘못된 fixture가 실제 저장소로 빠지지 않게 한다.
 */
static EN__BOOT_RET ota_boot_ram(snd_boot_status_t *out) { *out = s_tdc_ota_boot; return BOOT_RET_TRUE; }
static FIL *ota_pointer_ram(void) { return &s_tdc_ota_file; }
static int ota_remount_ram(int drive) { s_tdc_ota_drive = drive; return 1; }
static FRESULT ota_stat_ram(const TCHAR *path, FILINFO *info)
{
    (void)path; memset(info, 0, sizeof(*info)); info->fsize = s_tdc_ota_length; return FR_OK;
}
static FRESULT ota_mkdir_ram(const TCHAR *path) { (void)path; return FR_OK; }
static FRESULT ota_open_ram(FIL *file, const TCHAR *path, BYTE mode)
{
    (void)file; (void)mode;
    size_t length = strlen(path);
    if (length >= sizeof(s_tdc_ota_path) || s_tdc_ota_open_error) return FR_DISK_ERR;
    memcpy(s_tdc_ota_path, path, length + 1); s_tdc_ota_offset = 0; return FR_OK;
}
static FRESULT ota_seek_ram(FIL *file, FSIZE_t offset)
{
    (void)file;
    if (offset > sizeof(s_tdc_ota_bytes)) return FR_DISK_ERR;
    s_tdc_ota_offset = (unsigned)offset; return FR_OK;
}
static FRESULT ota_close_ram(FIL *file) { (void)file; s_tdc_ota_close_count++; return FR_OK; }
static FRESULT ota_write_ram(FIL *file, const void *data, UINT count, UINT *written)
{
    (void)file; *written = 0;
    if (s_tdc_ota_write_error || count > sizeof(s_tdc_ota_bytes) - s_tdc_ota_offset) return FR_DISK_ERR;
    *written = count - ((s_tdc_ota_short_write && count) ? 1u : 0u);
    memcpy(s_tdc_ota_bytes + s_tdc_ota_offset, data, *written);
    s_tdc_ota_offset += *written; s_tdc_ota_length = s_tdc_ota_offset; return FR_OK;
}
static const tdc_ut_io_cm3_ota_ops_t s_tdc_ota_ops = {
    ota_boot_ram, ota_pointer_ram, ota_remount_ram, ota_stat_ram, ota_mkdir_ram,
    ota_open_ram, ota_seek_ram, ota_close_ram, ota_write_ram
};

/* CM3-45: QCC 특수 명령의 payload를 실제 fetch/적용 함수로 처리한다.
 * 전원 정보, LED/DFU, I2S 종류와 ACK를 비교한다. QCC 무선 내부 시험은 아니다.
 * full bleCommunication의 SPI 분배/매핑/리모콘 루프는 이 부분 시험에 포함하지 않는다.
 */
static void vector_qcc_ram(void)
{
    static const int power[][3] = {{37,1,1},{72,0,2},{0,9,0}};
    int packet[BLE_DataPacketSize] = {0}, tx[SPI_COMM_PACKET_SIZE];
    for (int vector = 0; vector < 3; vector++)
    {
        packet[0] = EN__SND_BT_CMD_SYSTEM_INFO_POWER;
        for (int idx = 0; idx < 3; idx++) packet[idx + 1] = power[vector][idx];
        ack(); fetch_readDataForBleSetting(packet); setting_nrf_ble_adv_info();
        tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        mark(45, vector + 1, 9);
        put(snd_batt_get_percent(), power[vector][0]);
        put(snd_batt_get_state(), vector == 0 ? EN__SND_BATT_STATE_CHARGING : vector == 1 ? EN__SND_BATT_STATE_DISCHARGING : EN__SND_BATT_STATE_RESET);
        put(snd_charger_get_state().chargerConnectorPluggedIn, vector == 0 ? EN__SND_CHARGER_STATE_CONNECTED : vector == 1 ? EN__SND_CHARGER_STATE_DISCONNECTED : EN__SND_CHARGER_STATE_RESET);
        put(tdc_cradle_get_cover_state(), vector == 1 ? df_Disconnected : df_Connected);
        put(tx[0], 0x34); put(tx[1], 1); put(tx[20], 2);
        int state[11]; tdc_ut_io_cm3_ble_snapshot(state, 11);
        put(state[0], en__bleSetting_IDLE); put(state[1], power[vector][0]);
    }
    static const int classic[][3] = {{1,1,1},{1,2,0},{0,1,0},{9,1,0}};
    for (int vector = 0; vector < 4; vector++)
    {
        packet[0] = EN__SND_BT_CMD_SYSTEM_INFO_CLASSIC_STATE;
        packet[1] = classic[vector][0]; packet[2] = classic[vector][1];
        ack(); fetch_readDataForBleSetting(packet); setting_nrf_ble_adv_info();
        tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        mark(45, vector + 4, 4);
        put(cfx_cm3_sharedMemoryAll.is_i2s_source_cradle, classic[vector][2]);
        put(tx[0], 0x36); put(tx[1], 1); put(tx[20], 2);
    }
    for (int vector = 0; vector < 2; vector++)
    {
        packet[0] = EN__SND_BT_CMD_SYSTEM_INFO_LED_IND;
        packet[1] = vector == 0 ? TDC_LED_IND_STATE_OTA_EZAIRO : TDC_LED_IND_STATE_NONE;
        ack(); fetch_readDataForBleSetting(packet); setting_nrf_ble_adv_info();
        tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        mark(45, vector + 8, 5);
        put(tdc_led_get_ind_state(), packet[1]); put(tdc_get_ota_dfu_conn_state(), vector == 0 ? 1 : 0);
        put(tx[0], 0x35); put(tx[1], 1); put(tx[20], 2);
    }
}

/* CM3-26: 빈 버퍼에서 2바이트 응답, 준비 High/Low, 데이터 clear와 신호 분리.
 * 실제 writeDataToSpiTxBuff와 준비 관리 함수를 호출한다. DMA/핀만 RAM 훅이다.
 * 마지막 길이 필드가 2이며 초기화는 전 21워드를 0으로 만든다.
 */
static void vector_spi(void)
{
    int input[2] = {0x12, 0x34}, output[SPI_COMM_PACKET_SIZE];
    int (*saved)(int) = g_tdc_ut_io_cm3_spi_hw_hook;
    g_tdc_ut_io_cm3_spi_hw_hook = spi_ram;
    ack(); clear_SPI_Tx_Buffer();
    mark(26, 1, 6);
    put(isSpiTxBuffEmpty(), 1);
    writeDataToSpiTxBuff(input, 2);
    tdc_ut_io_cm3_spi_snapshot(output, SPI_COMM_PACKET_SIZE);
    put(output[0], 0x12); put(output[1], 0x34); put(output[20], 2);
    put(isSpiTxBuffEmpty(), 0); put(s_tdc_spi_ready, 1);
    clear_SPI_Tx_Buffer();
    mark(26, 2, 23);
    for (int idx = 0; idx < SPI_COMM_PACKET_SIZE; idx++)
    {
        tdc_ut_io_cm3_spi_snapshot(output, SPI_COMM_PACKET_SIZE);
        put(output[idx], 0);
    }
    put(isSpiTxBuffEmpty(), 0);
    ack(); put(isSpiTxBuffEmpty(), 1);
    g_tdc_ut_io_cm3_spi_hw_hook = saved;
}

/* CM3-37: 실제 PCM 명령 생성. 설정 상태는 원 함수로 초기화한 후 별도 복원한다.
 * 최소 펄스폭 13us 기준 13/14us 인코딩 0/1, 백텔 bit0 ON/OFF다.
 */
static void vector_pcm(void)
{
    int *buffer = cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer;
    mark(37, 1, 3);
    chang_PulseWidth(0, 13); chang_PulseWidth(1, 14); chang_PulseWidth_minimum(2);
    put(buffer[0], pcm_Mold_PulsePhaseWidth); put(buffer[1], pcm_Mold_PulsePhaseWidth | 1); put(buffer[2], pcm_Mold_PulsePhaseWidth);
    upadte_fpga_backtelConfig_written_Value(0x2E);
    mark(37, 2, 4);
    put(change_8BitBacktel_mode(3), 0x2D); put(buffer[3], pcm_Mold_BacktelConfiguration | 0x2D);
    upadte_fpga_backtelConfig_written_Value(0x2D);
    put(disable_Backtel(4), 0x2C); put(buffer[4], pcm_Mold_BacktelConfiguration | 0x2C);
}

/* CM3-47: A/B 끝값, read 인덱스 무시, invalid 종류/인덱스, 저장 실패.
 * 실제 handle의 모드/슬롯/범위 판정 이후 저장 경계만 RAM으로 교체한다.
 * 저장 실패 때 현재 세션의 공유 인덱스는 변경되지만 실패 응답이어야 한다.
 */
static void vector_gain(void)
{
    static const int request[][4] = {{1,1,0,1},{1,2,255,1},{0,1,999,1},{0,2,999,1},{1,1,-1,0},{1,2,256,0},{1,0,42,0},{2,1,42,0},{1,1,128,0}};
    int expected_a = 128, expected_b = 128;
    int (*saved)(int,int,int) = g_tdc_ut_io_cm3_gain_save_hook;
    ST__REMOTECONTROL_PACKET packet = {0};
    int tx[10];
    g_tdc_ut_io_cm3_gain_save_hook = gain_ram;
    tdc_remote_gain_control_init();
    for (unsigned vector = 0; vector < sizeof(request)/sizeof(request[0]); vector++)
    {
        packet.command = (EN__REMOTE_CONTROL_COMMAND)EN__SND_BT_CMD_GAIN_CONTROL;
        for (int idx = 0; idx < 3; idx++) packet.data[idx] = request[vector][idx];
        s_tdc_gain_save_error = vector == 8;
        if (vector == 0) expected_a = 0;
        if (vector == 1) expected_b = 255;
        if (vector == 8) expected_a = 128;
        mark(47, (int)vector + 1, 8);
        put(tdc_remote_gain_control_handle(&packet, tx, 0), 5);
        put(tx[0], 0x8C); put(tx[1], request[vector][3]); put(tx[2], request[vector][0]); put(tx[3], request[vector][1]);
        put(tx[4], vector < 4 || vector == 8 ? (packet.data[1] == 1 ? expected_a : expected_b) : request[vector][2]);
        put(cfx_cm3_sharedMemoryAll.gain_table_index_a, expected_a); put(cfx_cm3_sharedMemoryAll.gain_table_index_b, expected_b);
    }
    mark(47, 10, 2); put(s_tdc_gain_saved[0][0], 0); put(s_tdc_gain_saved[0][1], 255);
    s_tdc_gain_save_error = 0;
    g_tdc_ut_io_cm3_gain_save_hook = saved;
}

/* CM3-50: START/END ACK와 QCC 0x35의 DFU 상태 갱신을 별도 case로 구분한다.
 * 확인한 명령 계약에서 START/END는 ACK만 보내므로 기존 DFU 값이 유지돼야 한다.
 * 3/4는 상태 소유 경로다. 기준 입력 정정에 따른 과거 판정 차이는 criteria JSON 참조.
 */
static void vector_ota_session(void)
{
    int packet[BLE_DataPacketSize] = {0}, tx[SPI_COMM_PACKET_SIZE];
    int saved_state = tdc_get_ota_dfu_conn_state();
    int (*saved_hook)(int) = g_tdc_ut_io_cm3_spi_hw_hook;
    g_tdc_ut_io_cm3_spi_hw_hook = spi_ram;
    tdc_set_ota_dfu_conn_state(TDC_OTA_DFU_CONN_ST_DISCONN);
    packet[0] = CI_BLE_OTA_COMMAND_OTA_START; packet[2] = 7;
    ack(); ci_ble_fetch_packet_ota_start_end(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(50, 1, 5); put(tx[0], 0xC0); put(tx[1], 0); put(tx[2], 7); put(tx[20], 3); put(tdc_get_ota_dfu_conn_state(), 0);
    tdc_set_ota_dfu_conn_state(TDC_OTA_DFU_CONN_ST_CONN);
    packet[0] = CI_BLE_OTA_COMMAND_OTA_END; packet[1] = 9;
    ack(); ci_ble_fetch_packet_ota_start_end(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(50, 2, 5); put(tx[0], 0xC1); put(tx[1], 0); put(tx[2], 9); put(tx[20], 3); put(tdc_get_ota_dfu_conn_state(), 1);
    for (int vector = 0; vector < 2; vector++)
    {
        packet[0] = EN__SND_BT_CMD_SYSTEM_INFO_LED_IND;
        packet[1] = vector == 0 ? TDC_LED_IND_STATE_OTA_EZAIRO : TDC_LED_IND_STATE_NONE;
        ack(); fetch_readDataForBleSetting(packet); setting_nrf_ble_adv_info();
        tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        mark(50, vector + 3, 5);
        put(tx[0], 0x35); put(tx[1], 1); put(tx[20], 2);
        put(tdc_get_ota_dfu_conn_state(), vector == 0 ? 1 : 0); put(tdc_led_get_ind_state(), packet[1]);
    }
    tdc_set_ota_dfu_conn_state(saved_state); ack();
    g_tdc_ut_io_cm3_spi_hw_hook = saved_hook;
}

/* CM3-51 RAM: 두 슬롯/네 파일 종류, 2바이트 완료, 순서 오류, 저장 실패.
 * 제품 파일은 열지 않는다. 호출 전 OTA private state는 run_unit에서 교환/복원한다.
 * 잘못된 슬롯/파일 종류의 READ 응답 덮어쓰기 불일치는 그대로 FAIL 기록한다.
 * 현재 구현에 길이 검사가 없는 16바이트 초과 tail은 자동 실행하지 않는다.
 */
static void vector_ota_ram(void)
{
    int packet[BLE_DataPacketSize] = {0}, tx[SPI_COMM_PACKET_SIZE], state[10];
    static const char *names[] = {"MANIFEST.TXT", "APP000.FEZ", "APP001.FEZ", "APP002.FEZ"};
    int vector = 0;
    g_tdc_ut_io_cm3_ota_ops = &s_tdc_ota_ops;
    memset(&s_tdc_ota_boot, 0, sizeof(s_tdc_ota_boot));
    memset(s_tdc_ota_bytes, 0, sizeof(s_tdc_ota_bytes));
    s_tdc_ota_offset = s_tdc_ota_length = 0;
    s_tdc_ota_close_count = s_tdc_ota_open_error = s_tdc_ota_write_error = s_tdc_ota_short_write = 0;
    for (int slot = 1; slot <= 2; slot++)
    {
        s_tdc_ota_boot.boot_slot_num = slot == 1 ? 2 : 1;
        for (int type = 1; type <= 4; type++)
        {
            char path[32]; path[0] = '/'; path[1] = (char)('0' + slot); path[2] = '/'; path[3] = 0;
            strcat(path, names[type - 1]);
            memset(packet, 0, sizeof(packet));
            packet[0] = PKT_HEADER_OTA; packet[4] = slot; packet[6] = type;
            packet[7] = 1; packet[11] = 2; packet[15] = 1; packet[17] = 2;
            ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
            mark(51, ++vector, 7);
            put(tx[0], 0xC3); put(tx[4], slot); put(tx[6], type); put(tx[8], 1);
            put(strcmp(s_tdc_ota_path, path) == 0, 1); put(tx[20], 9); put(s_tdc_ota_drive, SND_FATFS_LDRV_NUM_BOOT);
            packet[3] = 1; packet[4] = 0x12; packet[5] = 0x34;
            ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
            tdc_ut_io_cm3_ota_snapshot(state, 10);
            mark(51, ++vector, 7);
            put(tx[4], 1); put(s_tdc_ota_bytes[0], 0x12); put(s_tdc_ota_bytes[1], 0x34);
            put(s_tdc_ota_drive, SND_FATFS_LDRV_NUM_USER_DATA); put(state[3], 0);
            put(s_tdc_ota_length, 2); put(s_tdc_ota_close_count, slot == 1 ? type : type + 4);
        }
    }
    memset(packet, 0, sizeof(packet));
    packet[0] = PKT_HEADER_OTA; packet[4] = 3; packet[6] = 1; packet[7] = 2;
    ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 2); put(tx[0], en__ERROR_Response); put(tx[3], 3);
    packet[4] = 2; packet[6] = 5;
    ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 2); put(tx[0], en__ERROR_Response); put(tx[3], 4);
    packet[4] = 2; packet[6] = 1; packet[7] = 1; packet[11] = 2; packet[15] = 1; packet[17] = 2;
    for (int failure = 0; failure < 3; failure++)
    {
        packet[3] = 0; ack(); ci_ble_fetch_packet_ota(packet);
        int closes_before = s_tdc_ota_close_count;
        packet[3] = failure == 0 ? 2 : 1;
        s_tdc_ota_short_write = failure == 1; s_tdc_ota_write_error = failure == 2;
        ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        tdc_ut_io_cm3_ota_snapshot(state, 10);
        mark(51, ++vector, 5);
        put(tx[0], failure == 0 ? 0xC3 : en__ERROR_Response);
        put(tx[failure == 0 ? 4 : 3], 2); put(state[3], 0);
        put(s_tdc_ota_drive, SND_FATFS_LDRV_NUM_USER_DATA); put(s_tdc_ota_close_count - closes_before, 1);
        s_tdc_ota_short_write = s_tdc_ota_write_error = 0;
    }
    packet[3] = 0; s_tdc_ota_open_error = 1;
    ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 3); put(tx[0], en__ERROR_Response); put(tx[3], 2); put(s_tdc_ota_drive, SND_FATFS_LDRV_NUM_USER_DATA);
    s_tdc_ota_open_error = 0;
    packet[4] = s_tdc_ota_boot.boot_slot_num;
    ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 2); put(tx[0], en__ERROR_Response); put(tx[3], 3);
    ack();
}

#if defined(TDC_UT_IO_CM3_HOST) && !defined(TDC_UT_IO_CM3_RAM_ONLY)
static void vector_touch(int unit)
{
    static const int status_bytes[][2] = {{0x20,0},{0x00,0x03},{0x60,0x00},{0xEE,0xEE}};
    tdc_touch_iqs323_status_t status;
    if (unit == 2)
    {
        tdc_touch_iqs323_mclr(); tdc_touch_iqs323_apply_settings();
        mark(2, 1, 5); put(g_tdc_ut_io_cm3_transfer[0].addr, 0x44); put(g_tdc_ut_io_cm3_transfer[0].data[0], 0xC0); put(g_tdc_ut_io_cm3_transfer[0].data[1], 1); put(g_tdc_ut_io_cm3_transfer[0].data[2], 0); put(tdc_touch_iqs323_is_ulp(), 0);
        int found = 0;
        for (int idx = 0; idx < g_tdc_ut_io_cm3_transfer_count; idx++)
        {
            tdc_ut_io_cm3_transfer_t *record = &g_tdc_ut_io_cm3_transfer[idx];
            if (!record->read && record->count == 3 && record->data[0] == 0x36)
            {
                mark(2, 2, 2); put(record->data[1], 0x0C); put(record->data[2], 4); found++;
            }
        }
        mark(2, 3, 1); put(found, 1);
        return;
    }
    for (int vector = 0; vector < 4; vector++)
    {
        tdc_ut_io_cm3_host_rx(status_bytes[vector], 2);
        mark(10, vector + 1, 6);
        put(tdc_touch_iqs323_read_status(&status), vector != 3); put(status.ok, vector != 3);
        put(status.ati_active, vector == 0 || vector == 2); put(status.ati_error, vector == 2);
        put(status.pressed, vector == 1); put(status.prox, vector == 1);
    }
    tdc_ut_io_cm3_host_rx(status_bytes[0], 2); mark(10, 5, 1); put(tdc_touch_iqs323_is_ati_done(), 0);
    tdc_ut_io_cm3_host_rx(status_bytes[1], 2); mark(10, 6, 1); put(tdc_touch_iqs323_is_ati_done(), 1);
    mark(10, 7, 4); tdc_touch_iqs323_set_ulp(); put(tdc_touch_iqs323_is_ulp(), 1); tdc_touch_iqs323_clear_ulp(); put(tdc_touch_iqs323_is_ulp(), 0); put(tdc_touch_iqs323_re_ati(), 1); put(tdc_touch_iqs323_reseed(), 1);
    g_tdc_ut_io_cm3_bus_error = 1; mark(10, 8, 2); put(tdc_touch_iqs323_read_status(&status), 0); put(status.ok, 0);
}
static void vector_i2c(void)
{
    int source[2] = {0x56,0x78}, dest[2] = {0}, rx[2] = {0x12,0x34};
    mark(25, 1, 4);
    i2c_startWriteData(0x44, source, 2);
    while (get_i2cDriverStatus() == i2c_state_WriteTriggered) {}
    put(get_i2cDriverStatus(), i2c_state_WritingDone); put(g_tdc_ut_io_cm3_transfer[0].data[0], 0x56); put(g_tdc_ut_io_cm3_transfer[0].data[1], 0x78);
    setI2cDriverStatusIdle(); put(isI2cDriverStatusIdle(), 1);
    tdc_ut_io_cm3_host_i2c_reset(); tdc_ut_io_cm3_host_rx(rx, 2);
    i2c_startReadData(0x44, dest, 2); while (get_i2cDriverStatus() == i2c_state_ReadTriggerd) {}
    mark(25, 2, 3); put(get_i2cDriverStatus(), i2c_state_ReadingDone); put(dest[0], 0x12); put(dest[1], 0x34);
    setI2cDriverStatusIdle(); tdc_ut_io_cm3_host_i2c_reset(); g_tdc_ut_io_cm3_bus_error = 1;
    i2c_startReadData(0x44, dest, 2); mark(25, 3, 2); put(get_i2cDriverStatus(), i2c_state_Error); init_I2c(); put(isI2cDriverStatusIdle(), 1);
    I2C0->STATUS = (1u << 21); i2c_startWriteData(0x44, source, 2);
    mark(25, 4, 1); put(get_i2cDriverStatus(), i2c_state_Error); init_I2c();
}
static void vector_fpga(int unit)
{
    int value = 0, rx[2] = {0x12,0x34}; bool flag = false;
    reset_Fpga_variable();
    if (unit == 34)
    {
        mark(34, 1, 6); put(write_FPGA_systemResgister_1st(0x1C), 1); put(write_FPGA_enable_RF_tx(), 1); put(get_last_fpga_systemResgister_1st_written_Value(), 0x1E); put(write_FPGA_disable_RF_tx(), 1); put(get_last_fpga_systemResgister_1st_written_Value(), 0x1C); put(g_tdc_ut_io_cm3_transfer[2].data[0], 0x1C);
        mark(34, 2, 2); put(write_FPGA_reset(), 1); put(g_tdc_ut_io_cm3_transfer[3].data[0], 1);
    }
    else if (unit == 35)
    {
        int levels[2] = {MinTxPowerValue,MaxVoltageControlValue};
        for (int vector = 0; vector < 2; vector++)
        {
            tdc_ut_io_cm3_host_i2c_reset(); tdc_ut_io_cm3_host_rx(&levels[vector], 1);
            mark(35, vector + 1, 4); put(write_change_TxPowerLevel(levels[vector]), 1); put(read_txPowerLevel(&value), 1); put(value, levels[vector]); put(g_tdc_ut_io_cm3_transfer[g_tdc_ut_io_cm3_transfer_count - 3].data[1], levels[vector]);
        }
    }
    else if (unit == 36)
    {
        for (int vector = 0; vector < 6; vector++)
        {
            value = vector == 0 ? 0 : (1 << (vector - 1)); tdc_ut_io_cm3_host_rx(&value, 1); tdc_ut_io_cm3_host_i2c_reset();
            mark(36, vector + 1, 2); put(check_FPGA_PCM_Error(&flag), 1); put(flag, vector != 0);
        }
        value = 1 << FPGA_BitPosition_FIFO_is_empty; tdc_ut_io_cm3_host_rx(&value, 1); tdc_ut_io_cm3_host_i2c_reset();
        mark(36, 7, 2); put(check_FPGA_FIFO_empty(&flag), 1); put(flag, 1);
        tdc_ut_io_cm3_host_rx(rx, 2); tdc_ut_io_cm3_host_i2c_reset();
        int fifo[2] = {0}; mark(36, 8, 3); put(read_FPGA_backtel_FIFO(fifo, 2), 1); put(fifo[0], 0x12); put(fifo[1], 0x34);
        g_tdc_ut_io_cm3_bus_error = 1; tdc_ut_io_cm3_host_i2c_reset(); mark(36, 9, 1); put(read_FPGA_version(&value), 0);
    }
    else
    {
        int response = 0x5A; tdc_ut_io_cm3_host_rx(&response, 1);
        mark(40, 1, 4); put(read_REN_ISL9122_register_byCM3_I2C(0x11, &value), 1); put(value, 0x5A); put(g_tdc_ut_io_cm3_transfer[0].addr, 0x18); put(g_tdc_ut_io_cm3_transfer[0].data[0], 0x11);
        int reset_rx[2] = {0x81,75}; tdc_ut_io_cm3_host_rx(reset_rx, 2); tdc_ut_io_cm3_host_i2c_reset();
        mark(40, 2, 1); put(Reset_REN_ISL9122(), 1);
        g_tdc_ut_io_cm3_bus_error = 1; tdc_ut_io_cm3_host_i2c_reset(); mark(40, 3, 1); put(read_REN_ISL9122_register_byCM3_I2C(0x11, &value), 0);
    }
}
static void vector_ota_data(void)
{
    int packet[BLE_DataPacketSize] = {0}, tx[SPI_COMM_PACKET_SIZE];
    static const char *names[4]={"MANIFEST.TXT","APP000.FEZ","APP001.FEZ","APP002.FEZ"};
    int vector=0;
    for (int slot=1;slot<=2;slot++)
    {
      g_tdc_ut_io_cm3_boot_status.boot_slot_num=slot==1?2:1;
      for (int type = 1; type <= 4; type++)
      {
        char path[20]; path[0]='/'; path[1]=(char)('0'+slot); path[2]='/'; path[3]=0;
        strcat(path,names[type-1]);
        packet[0] = 0xC3; packet[4] = slot; packet[6] = type; packet[7] = 1; packet[11] = 2; packet[15] = 1; packet[17] = 2;
        ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        mark(51, ++vector, 7); put(tx[0], 0xC3); put(tx[4], slot); put(tx[6], type); put(tx[8], 1);
        put(strcmp(g_tdc_ut_io_cm3_fs_path,path)==0,1); put(tx[20],9); put(g_tdc_ut_io_cm3_fs_drive,SND_FATFS_LDRV_NUM_BOOT);
        packet[3] = 1; packet[4] = 0x12; packet[5] = 0x34;
        ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
        mark(51, ++vector, 4); put(tx[4], 1); put(g_tdc_ut_io_cm3_fs_bytes[0], 0x12); put(g_tdc_ut_io_cm3_fs_bytes[1], 0x34); put(g_tdc_ut_io_cm3_fs_drive, SND_FATFS_LDRV_NUM_USER_DATA);
        memset(packet, 0, sizeof(packet));
      }
    }
    packet[0] = 0xC3; packet[4] = 3; packet[6] = 1; packet[7] = 2;
    ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 2); put(tx[0], en__ERROR_Response); put(tx[3], 3);
    packet[4] = 2; packet[6] = 5; ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 2); put(tx[0], en__ERROR_Response); put(tx[3], 4);
    packet[6] = 1; packet[7] = 1; packet[11] = 2; packet[15] = 1; packet[17] = 2;
    ack(); ci_ble_fetch_packet_ota(packet); g_tdc_ut_io_cm3_fs_short = 1; packet[3] = 1; ack(); ci_ble_fetch_packet_ota(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 3); put(tx[0], en__ERROR_Response); put(tx[3], 2); put(g_tdc_ut_io_cm3_fs_drive, SND_FATFS_LDRV_NUM_USER_DATA);
    ack(); g_tdc_ut_io_cm3_fs_short = 0;
    memset(packet, 0, sizeof(packet)); packet[0] = PKT_HEADER_BOOT; packet[1] = PKT_BOOT_OPTION_INFO;
    ci_ble_fetch_packet_boot(packet); tdc_ut_io_cm3_spi_snapshot(tx, SPI_COMM_PACKET_SIZE);
    mark(51, ++vector, 2); put(tx[0], PKT_HEADER_BOOT); put(tx[RESP_PKT_IDX_BOOT_INFO_BOOT_SLOT_NUM], 1);
    packet[1]=PKT_BOOT_OPTION_SELECT; packet[2]=1;
    ack(); ci_ble_fetch_packet_boot(packet); tdc_ut_io_cm3_spi_snapshot(tx,SPI_COMM_PACKET_SIZE);
    mark(51,++vector,2); put(tx[RESP_PKT_IDX_BOOT_SELECT_RESULT],PKT_BOOT_RESULT_REJECT_CURRENT_BOOT_SLOT); put(g_tdc_ut_io_cm3_reset_count,0);
    packet[2]=3; ack(); ci_ble_fetch_packet_boot(packet); tdc_ut_io_cm3_spi_snapshot(tx,SPI_COMM_PACKET_SIZE);
    mark(51,++vector,2); put(tx[RESP_PKT_IDX_BOOT_SELECT_RESULT],PKT_BOOT_RESULT_REJECT_INVALID_SLOT_NUM); put(g_tdc_ut_io_cm3_reset_count,0);
    packet[2]=2; g_tdc_ut_io_cm3_fs_error=1;
    ack(); ci_ble_fetch_packet_boot(packet); tdc_ut_io_cm3_spi_snapshot(tx,SPI_COMM_PACKET_SIZE);
    mark(51,++vector,3); put(tx[0],en__ERROR_Response); put(tx[3],PKT_BOOT_ERROR_FILE_WRITE); put(g_tdc_ut_io_cm3_reset_count,0);
    g_tdc_ut_io_cm3_fs_error=0;
}
#endif

int tdc_ut_io_cm3_run_unit(int unit)
{
    int supported = unit == 10 || unit == 25 || unit == 26 || unit == 34 || unit == 35 || unit == 36 || unit == 37 || unit == 40 || unit == 45 || unit == 47 || unit == 50 || unit == 51;
#if defined(TDC_UT_IO_CM3_HOST) && !defined(TDC_UT_IO_CM3_RAM_ONLY)
    supported = supported || unit == 2 || unit == 25 || unit == 51;
#endif
    if (s_tdc_io_running || g_tdc_ut_io_cm3_done || g_tdc_ut_cm3_vector_done) return TDC_UT_IO_CM3_BUSY;
    if (!supported) return TDC_UT_IO_CM3_UNSUPPORTED;
    /* 타깃은 CFX 개방 전/모든 코어 정지 시험 맥락에서 명시적으로 호출한다. */
#if !defined(TDC_UT_IO_CM3_HOST)
    if (cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration || !isSpiTxBuffEmpty() || !isI2cDriverStatusIdle()) return TDC_UT_IO_CM3_BUSY;
#endif
    s_tdc_saved_shared = cfx_cm3_sharedMemoryAll;
    int saved_cache[4], saved_tx[SPI_COMM_PACKET_SIZE], saved_empty=isSpiTxBuffEmpty();
    int saved_ulp=tdc_touch_iqs323_is_ulp();
    ST__OTA_FILE_WRITE_INFO saved_ota = {0};
    int saved_ble[1 + todoc_PayloadSize] = {0}, saved_battery[4] = {0};
    int saved_led[LED_SRC__MAX + 3] = {0};
    tdc_led_ind_state_t saved_led_ind = tdc_led_get_ind_state();
    int saved_dfu = tdc_get_ota_dfu_conn_state();
    int saved_diagnostics = g_tdc_ut_cm3_suppress_diagnostics;
    const tdc_ut_io_cm3_ota_ops_t *saved_ota_ops = g_tdc_ut_io_cm3_ota_ops;
    const tdc_ut_io_cm3_i2c_ops_t *saved_i2c_ops = g_tdc_ut_io_cm3_i2c_ops;
    void (*saved_i2c_poll)(void) = g_tdc_ut_io_cm3_i2c_poll_hook;
    ST__I2C_DRIVER saved_i2c = {0}; int saved_i2c_cache[41] = {0};
    tdc_ut_io_cm3_i2c_exchange(&saved_i2c, saved_i2c_cache);
    tdc_ut_io_cm3_ota_exchange(&saved_ota);
    tdc_ut_io_cm3_ble_exchange(saved_ble, 1 + todoc_PayloadSize);
    tdc_ut_cm3_battery_exchange(saved_battery);
    tdc_ut_cm3_led_exchange(saved_led);
    g_tdc_ut_cm3_suppress_diagnostics = 1;
    tdc_ut_io_cm3_fpga_cache(saved_cache,0);
    tdc_ut_io_cm3_spi_snapshot(saved_tx,SPI_COMM_PACKET_SIZE);
    int (*saved_spi)(int) = g_tdc_ut_io_cm3_spi_hw_hook;
    int saved_auto_ack = s_tdc_spi_auto_ack;
    int (*saved_isd)(int,int,int*,int)=g_tdc_ut_io_cm3_isd_bus_hook;
    int (*saved_pmic)(int,int,int*,int)=g_tdc_ut_io_cm3_pmic_bus_hook;
    int (*saved_touch)(int,unsigned char*,int)=g_tdc_ut_io_cm3_touch_bus_hook;
    ST__ERROR_CODE saved_error={0};
    tdc_ut_cm3_error_exchange(&saved_error);
    s_tdc_io_running = 1; g_tdc_ut_io_cm3_count = g_tdc_ut_io_cm3_overflow = g_tdc_ut_io_cm3_done = 0;
    g_tdc_ut_io_cm3_observed = 0;
#if defined(TDC_UT_IO_CM3_HOST)
    tdc_ut_io_cm3_host_setup();
#else
    cfx_cm3_sharedMemoryAll.connected_ISD_num = 1;
    cfx_cm3_sharedMemoryAll.systemShare.system_opMode = en__normalMode;
#endif
    g_tdc_ut_io_cm3_spi_hw_hook = spi_ram;
    s_tdc_spi_auto_ack = unit == 51;
#if !defined(TDC_UT_IO_CM3_HOST) || defined(TDC_UT_IO_CM3_RAM_ONLY)
    g_tdc_ut_io_cm3_isd_bus_hook=bus_ram;
    g_tdc_ut_io_cm3_pmic_bus_hook=bus_ram;
    g_tdc_ut_io_cm3_touch_bus_hook=touch_ram;
#endif
    switch (unit)
    {
        case 26: vector_spi(); break;
        case 37: vector_pcm(); break;
        case 47: vector_gain(); break;
        case 45: vector_qcc_ram(); break;
        case 50: vector_ota_session(); break;
#if defined(TDC_UT_IO_CM3_HOST) && !defined(TDC_UT_IO_CM3_RAM_ONLY)
        case 2: case 10: vector_touch(unit); break;
        case 25: vector_i2c(); break;
        case 34: case 35: case 36: case 40: vector_fpga(unit); break;
        case 51: vector_ota_data(); break;
#else
        case 10: vector_touch_ram(); break;
        case 25: vector_i2c_ram(); break;
        case 34: case 35: case 36: case 40: vector_device_ram(unit); break;
        case 51: vector_ota_ram(); break;
#endif
        default: break;
    }
    ack(); clear_SPI_Tx_Buffer();
    g_tdc_ut_io_cm3_spi_hw_hook = saved_spi;
    s_tdc_spi_auto_ack = saved_auto_ack;
#if defined(TDC_UT_IO_CM3_HOST)
    tdc_ut_io_cm3_host_restore();
#endif
    g_tdc_ut_io_cm3_isd_bus_hook=saved_isd;
    g_tdc_ut_io_cm3_pmic_bus_hook=saved_pmic;
    g_tdc_ut_io_cm3_touch_bus_hook=saved_touch;
    g_tdc_ut_io_cm3_i2c_ops = saved_i2c_ops;
    g_tdc_ut_io_cm3_i2c_poll_hook = saved_i2c_poll;
    tdc_ut_io_cm3_i2c_exchange(&saved_i2c, saved_i2c_cache);
    g_tdc_ut_io_cm3_ota_ops = saved_ota_ops;
    tdc_ut_io_cm3_ota_exchange(&saved_ota);
    tdc_ut_io_cm3_ble_exchange(saved_ble, 1 + todoc_PayloadSize);
    tdc_set_ota_dfu_conn_state(saved_dfu);
    tdc_led_set_ind_state(saved_led_ind);
    tdc_ut_cm3_led_exchange(saved_led);
    tdc_ut_cm3_battery_exchange(saved_battery);
    g_tdc_ut_cm3_suppress_diagnostics = saved_diagnostics;
    if(saved_ulp) tdc_touch_iqs323_set_ulp(); else tdc_touch_iqs323_clear_ulp();
    tdc_ut_io_cm3_fpga_cache(saved_cache,1);
    cfx_cm3_sharedMemoryAll = s_tdc_saved_shared;
    tdc_ut_cm3_error_exchange(&saved_error);
    tdc_ut_io_cm3_spi_restore(saved_tx,SPI_COMM_PACKET_SIZE,saved_empty);
    s_tdc_io_running = 0; g_tdc_ut_io_cm3_done = 1;
    return TDC_UT_IO_CM3_OK;
}

/* 설정: 시험자가 해당 실제 함수를 안전 시험환경에서 실행한 뒤 코어/IRQ를 정지.
 * 입력: case는 시험자의 before/after/정상/오류 구분 번호다. 이 함수는 입력을 만들지 않는다.
 * 출력: 실제 상태와 마지막 반환/전송의 snapshot. 예상값 없이 OBSERVED로 전송한다.
 * 복원: 제품 상태를 변경하지 않으므로 필요 없다. done까지도 PASS로 해석하면 안 된다.
 * 보드 비활성 소자는 active=0으로 기록한다. 이것은 해당 PMIC 시험 실행이 아니다.
 */
int tdc_ut_io_cm3_observe_unit(int unit, int case_id)
{
    int supported = unit==2 || unit==10 || unit==25 || unit==26 || (31<=unit && unit<=43) || (45<=unit && unit<=51);
    int values[64], n=0, trace[8], tx[SPI_COMM_PACKET_SIZE];
    ST__I2C_DRIVER driver;
    if (s_tdc_io_running || g_tdc_ut_io_cm3_done || g_tdc_ut_cm3_vector_done) return TDC_UT_IO_CM3_BUSY;
    if (!supported) return TDC_UT_IO_CM3_UNSUPPORTED;
    if (case_id<1) return TDC_UT_IO_CM3_INVALID;
    s_tdc_io_running=1;
    g_tdc_ut_io_cm3_count=g_tdc_ut_io_cm3_overflow=g_tdc_ut_io_cm3_done=0;
    g_tdc_ut_io_cm3_observed=1;
    tdc_ut_io_cm3_i2c_snapshot(&driver); /* get_status의 시험 poll을 실행하지 않는다. */
    tdc_ut_io_cm3_spi_snapshot(tx,SPI_COMM_PACKET_SIZE);
    switch(unit)
    {
        case 31: case 32: case 33:
        {
            ST__ISD_STATUS state=snd_isd_interface_get_state();
            values[n++]=state.isd_controlState; values[n++]=state.conneded_ISD;
            values[n++]=readConnectionCheckPcmState(); values[n++]=isING_connectionCheckWithMapping();
            values[n++]=is_i2c_free(); values[n++]=cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode;
            values[n++]=driver.i2c_diver_state; values[n++]=driver.i2c_Error_Code;
            tdc_ut_io_cm3_i2c_trace(trace,8);
            for(int idx=0;idx<8;idx++) values[n++]=trace[idx];
        } break;
        case 38: case 39:
        {
            int active=0;
#if defined(Board_is_OTE_VER_1_2)
            if(unit==38) active=1;
#elif defined(Board_is_OTE_VER_1_3)
            if(unit==39) active=1;
#endif
            values[n++]=active; values[n++]=driver.i2c_diver_state; values[n++]=driver.i2c_Error_Code;
            tdc_ut_io_cm3_i2c_trace(trace,8);
            for(int idx=0;idx<8;idx++) values[n++]=trace[idx];
        } break;
        case 41:
        {
            values[n++]=cfx_cm3_sharedMemoryAll.currentMapData.stimulationMode;
            values[n++]=isStimulParaIsSettingDone();
            values[n++]=cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange;
            values[n++]=cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag;
            values[n++]=cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag;
            values[n++]=cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode;
            values[n++]=cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode_next;
            values[n++]=cfx_cm3_sharedMemoryAll.is_pcm_specific_command_reading;
            values[n++]=cfx_cm3_sharedMemoryAll.pcm_specific_command_read_index;
            for(int idx=0;idx<df_MaxNumTransferableChannel;idx++) values[n++]=cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[idx];
        } break;
        case 42: case 43:
        {
            values[n++]=cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand;
            values[n++]=cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
            values[n++]=cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;
            values[n++]=isReadWriteMapDataFlashCommandDone();
            values[n++]=cfx_cm3_sharedMemoryAll.connected_ISD_num;
            values[n++]=cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.userSettingValue_mapData.mapNum;
            values[n++]=cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.userSettingValue_mapData.stimulVolume;
            values[n++]=cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.userSettingValue_mapData.audioVolume;
            values[n++]=tx[0]; values[n++]=tx[20];
            /* 파일 목록/오류/짧은 읽기는 별도 격리 저장소의 operator evidence가 필요하다. */
        } break;
        case 45:
            n=tdc_ut_io_cm3_ble_snapshot(values,64);
            values[n++]=tx[0]; values[n++]=tx[1]; values[n++]=tx[20]; break;
        case 46:
            values[n++]=is_isd_passKeyMatch(); values[n++]=getRemoteCommand();
            values[n++]=cfx_cm3_sharedMemoryAll.systemShare.system_opMode;
            values[n++]=cfx_cm3_sharedMemoryAll.gain_table_index_a; values[n++]=cfx_cm3_sharedMemoryAll.gain_table_index_b;
            values[n++]=tx[0]; values[n++]=tx[1]; values[n++]=tx[20]; break;
        case 48: case 49:
            n=tdc_ut_io_cm3_mapping_snapshot(values,64);
            values[n++]=cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode;
            values[n++]=tx[0]; values[n++]=tx[1]; values[n++]=tx[20]; break;
        case 50: case 51:
            n=tdc_ut_io_cm3_ota_snapshot(values,64);
            values[n++]=tx[0]; values[n++]=tx[3]; values[n++]=tx[20]; break;
        case 2: case 10:
            values[n++]=tdc_touch_iqs323_is_ulp(); values[n++]=driver.i2c_diver_state;
            values[n++]=driver.i2c_Error_Code;
            tdc_ut_io_cm3_i2c_trace(trace,8);
            for(int idx=0;idx<8;idx++) { values[n++]=trace[idx]; }
            break;
        case 25:
            values[n++]=driver.i2c_diver_state; values[n++]=driver.i2c_Error_Code;
            values[n++]=driver.slaveAddress; values[n++]=driver.i2cTxRx_RemaindedDataLength;
            tdc_ut_io_cm3_i2c_trace(trace,8);
            for(int idx=0;idx<8;idx++) { values[n++]=trace[idx]; }
            break;
        case 26:
            values[n++]=isSpiTxBuffEmpty();
            for(int idx=0;idx<SPI_COMM_PACKET_SIZE;idx++) { values[n++]=tx[idx]; }
            break;
        case 47:
            values[n++]=cfx_cm3_sharedMemoryAll.connected_ISD_num;
            values[n++]=cfx_cm3_sharedMemoryAll.gain_table_index_a; values[n++]=cfx_cm3_sharedMemoryAll.gain_table_index_b;
            for(int idx=0;idx<5;idx++) { values[n++]=tx[idx]; }
            values[n++]=tx[20]; break;
        default: /* FPGA cache/PCM commands. No hardware register read. */
            tdc_ut_io_cm3_fpga_cache(values,0); n=4;
            for(int idx=0;idx<5;idx++) { values[n++]=cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[idx]; }
            break;
    }
    mark(unit,case_id,n);
    for(int idx=0;idx<n;idx++) put(values[idx],0);
    s_tdc_io_running=0; g_tdc_ut_io_cm3_done=1;
    return TDC_UT_IO_CM3_OK;
}
int tdc_ut_io_cm3_release(void)
{
    if (s_tdc_io_running) return TDC_UT_IO_CM3_BUSY;
    g_tdc_ut_io_cm3_done = 0;
    return TDC_UT_IO_CM3_OK;
}
#endif
