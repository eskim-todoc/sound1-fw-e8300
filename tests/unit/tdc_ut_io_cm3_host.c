/* 격리 호스트 fixture: SDK MMIO, IRQ 시간, FatFs/기동 저장 경계만 모의한다.
 * 제품 I2C ISR, SPI 버퍼, 센서/FPGA/PMIC 변환, 게인/OTA 처리는 원 함수를 링크한다.
 * 실제 파일, NVM, 장치, 리셋에 접근하는 함수는 이 실행파일에 링크하지 않는다.
 */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include "tdc_ut_io_cm3_host_fixture.h"
#include <tdc_shm.h>
#include <tdc_touch_iqs323.h>
#include <tdc_qcc.h>
#include <tdc_fs_gain.h>
#include <tdc_hal_spi.h>
#include <tdc_pwr_battery.h>
#include <tdc_led_output.h>
#include <assert.h>
#include <stdarg.h>

I2C_Type g_tdc_ut_io_cm3_i2c;
SPI_Type g_tdc_ut_io_cm3_spi;
SPI_STATUS_Type g_tdc_ut_io_cm3_spi_status;
DMA_Type g_tdc_ut_io_cm3_dma[2];
DMA_CNTS_Type g_tdc_ut_io_cm3_dma_count[2];
uint32_t SystemCoreClock = 30720000;
volatile int g_tdc_ut_cm3_vector_done;
tdc_ut_io_cm3_transfer_t g_tdc_ut_io_cm3_transfer[128];
int g_tdc_ut_io_cm3_transfer_count;
int g_tdc_ut_io_cm3_gpio[32];
int g_tdc_ut_io_cm3_bus_error;
int g_tdc_ut_io_cm3_bus_error_transfer;
int g_tdc_ut_io_cm3_error_code;
int g_tdc_ut_io_cm3_error_count;
int g_tdc_ut_io_cm3_reset_count;
int g_tdc_ut_io_cm3_fs_error;
int g_tdc_ut_io_cm3_fs_short;
int g_tdc_ut_io_cm3_fs_drive;
int g_tdc_ut_io_cm3_fs_close_count;
char g_tdc_ut_io_cm3_fs_path[32];
unsigned char g_tdc_ut_io_cm3_fs_bytes[256];
unsigned g_tdc_ut_io_cm3_fs_length;
tdc_boot_status_t g_tdc_ut_io_cm3_boot_status;
int g_tdc_ut_io_cm3_boot_error;

static ST__CFX_CM3_SharedMemory_ALL s_tdc_saved_shared;
static void (*s_tdc_saved_hook)(void);
static int s_tdc_rx[256], s_tdc_rx_count, s_tdc_rx_index;
static int s_tdc_tick, s_tdc_window_reads;
static FIL s_tdc_file;
static unsigned s_tdc_file_offset;
static tdc_fs_gain_file_t s_tdc_gain_file;
static int s_tdc_gain_present;

extern void tdc_ut_cm3_battery_exchange(int state[4]);
extern void tdc_ut_cm3_led_exchange(int state[TDC_LED_SRC__MAX + 3]);
extern void tdc_ut_cm3_error_exchange(tdc_sys_error_code_t *state);
extern int g_tdc_ut_cm3_suppress_diagnostics;

/* 복원 회귀 시험은 기본값 0만 비교하지 않는다. 실제 private 상태에 식별값을
 * 넣고 매 시험 전후를 비교하며 같은 입력을 두 번 실행해 숨은 상태 누적도 확인한다.
 * 이 구조와 교환은 호스트 fixture 전용이며 제품 저장소/장치에는 접근하지 않는다.
 */
typedef struct
{
    ST__CFX_CM3_SharedMemory_ALL shared;
    tdc_hal_i2c_driver_t i2c;
    ST__OTA_FILE_WRITE_INFO ota;
    tdc_sys_error_code_t tdc_sys_error;
    int i2c_cache[41], ble[1 + todoc_PayloadSize], battery[4];
    int led[TDC_LED_SRC__MAX + 3], tx[TDC_HAL_SPI_COMM_PACKET_SIZE], fpga[4];
    int tx_empty, ulp, dfu, led_ind, diagnostics;
    const tdc_ut_io_cm3_i2c_ops_t *i2c_ops;
    const tdc_ut_io_cm3_ota_ops_t *ota_ops;
    void (*poll)(void);
    int (*spi)(int);
    int (*isd)(int, int, int *, int);
    int (*pmic)(int, int, int *, int);
    int (*touch)(int, unsigned char *, int);
    int (*gain)(int, int, int);
} tdc_ut_io_cm3_saved_ram_t;

static void capture_product_ram(tdc_ut_io_cm3_saved_ram_t *out)
{
    memset(out, 0, sizeof(*out));
    out->shared = cfx_cm3_sharedMemoryAll;
    out->tx_empty = tdc_hal_spi_is_tx_buffer_empty();
    out->ulp = tdc_touch_iqs323_is_ulp();
    out->dfu = tdc_dfu_get_conn_state();
    out->led_ind = tdc_led_get_ind_state();
    out->diagnostics = g_tdc_ut_cm3_suppress_diagnostics;
    out->i2c_ops = g_tdc_ut_io_cm3_i2c_ops;
    out->ota_ops = g_tdc_ut_io_cm3_ota_ops;
    out->poll = g_tdc_ut_io_cm3_i2c_poll_hook;
    out->spi = g_tdc_ut_io_cm3_spi_hw_hook;
    out->isd = g_tdc_ut_io_cm3_isd_bus_hook;
    out->pmic = g_tdc_ut_io_cm3_pmic_bus_hook;
    out->touch = g_tdc_ut_io_cm3_touch_bus_hook;
    out->gain = g_tdc_ut_io_cm3_gain_save_hook;
    tdc_ut_io_cm3_spi_snapshot(out->tx, TDC_HAL_SPI_COMM_PACKET_SIZE);
    tdc_ut_io_cm3_fpga_cache(out->fpga, 0);
    /* exchange가 실제 상태를 바꾸므로 읽은 사본을 다시 교환해 즉시 복원한다. */
    tdc_ut_io_cm3_i2c_exchange(&out->i2c, out->i2c_cache);
    tdc_hal_i2c_driver_t i2c = out->i2c; int cache[41];
    memcpy(cache, out->i2c_cache, sizeof(cache));
    tdc_ut_io_cm3_i2c_exchange(&i2c, cache);
    tdc_ut_io_cm3_ota_exchange(&out->ota);
    ST__OTA_FILE_WRITE_INFO ota = out->ota;
    tdc_ut_io_cm3_ota_exchange(&ota);
    tdc_ut_io_cm3_ble_exchange(out->ble, 1 + todoc_PayloadSize);
    int ble[1 + todoc_PayloadSize]; memcpy(ble, out->ble, sizeof(ble));
    tdc_ut_io_cm3_ble_exchange(ble, 1 + todoc_PayloadSize);
    tdc_ut_cm3_battery_exchange(out->battery);
    int battery[4]; memcpy(battery, out->battery, sizeof(battery));
    tdc_ut_cm3_battery_exchange(battery);
    tdc_ut_cm3_led_exchange(out->led);
    int led[TDC_LED_SRC__MAX + 3]; memcpy(led, out->led, sizeof(led));
    tdc_ut_cm3_led_exchange(led);
    tdc_ut_cm3_error_exchange(&out->tdc_sys_error);
    tdc_sys_error_code_t tdc_sys_error = out->tdc_sys_error;
    tdc_ut_cm3_error_exchange(&tdc_sys_error);
}

static void seed_product_ram(void)
{
    tdc_hal_i2c_driver_t i2c = {0}; int cache[41];
    i2c.i2c_diver_state = i2c_state_Idle;
    i2c.slaveAddress = 0x22;
    for (int i = 0; i < 41; i++) cache[i] = 0x60 + i;
    cache[8] = 7; /* dump index는 유효한 범위로 둔다. */
    tdc_ut_io_cm3_i2c_exchange(&i2c, cache);
    ST__OTA_FILE_WRITE_INFO ota = {7, 2, 3, 1, 16, 1, 16, 8, 11};
    tdc_ut_io_cm3_ota_exchange(&ota);
    int ble[1 + todoc_PayloadSize];
    for (int i = 0; i < 1 + todoc_PayloadSize; i++) ble[i] = 0x20 + i;
    tdc_ut_io_cm3_ble_exchange(ble, 1 + todoc_PayloadSize);
    int battery[4] = {83, 1, 2, 2}; tdc_ut_cm3_battery_exchange(battery);
    tdc_led_set_ind_state(TDC_LED_IND_STATE_PAIR);
    int led[TDC_LED_SRC__MAX + 3] = {0};
    led[TDC_LED_SRC_BLE_IND] = TDC_LED_ST_PAIR;
    led[TDC_LED_SRC__MAX] = 1357; led[TDC_LED_SRC__MAX + 1] = 1; led[TDC_LED_SRC__MAX + 2] = 1;
    tdc_ut_cm3_led_exchange(led);
    tdc_dfu_set_conn_state(1);
    tdc_touch_iqs323_set_ulp();
    g_tdc_ut_cm3_suppress_diagnostics = 0;
}

static void reg_write(volatile const uint32_t *reg, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)reg = value;
}

void tdc_ut_io_cm3_host_rx(const int *data, int count)
{
    assert(count >= 0 && count <= 256);
    if (count > 0) memcpy(s_tdc_rx, data, (size_t)count * sizeof(int));
    s_tdc_rx_count = count;
    s_tdc_rx_index = 0;
}

/* 각 poll은 실제 ISR 한 번을 실행한다. TX 데이터와 RX 길이는 제품 상태에서 관측한다. */
void tdc_ut_io_cm3_host_pump(void)
{
    tdc_hal_i2c_driver_t driver;
    tdc_hal_i2c_status_t status = {0};
    tdc_ut_io_cm3_transfer_t *transfer;
    tdc_ut_io_cm3_i2c_snapshot(&driver);
    if (driver.i2c_diver_state != i2c_state_WriteTriggered && driver.i2c_diver_state != i2c_state_ReadTriggerd) return;
    assert(g_tdc_ut_io_cm3_transfer_count > 0);
    transfer = &g_tdc_ut_io_cm3_transfer[g_tdc_ut_io_cm3_transfer_count - 1];
    int injected_error = g_tdc_ut_io_cm3_bus_error ||
        (g_tdc_ut_io_cm3_bus_error_transfer > 0 && g_tdc_ut_io_cm3_transfer_count == g_tdc_ut_io_cm3_bus_error_transfer);
    if (injected_error || driver.i2cTxRx_RemaindedDataLength == 0)
    {
        status.fields.stop_detected = 1;
        status.fields.bus_error = injected_error != 0;
    }
    else
    {
        status.fields.busy = 1;
        status.fields.clk_stretch = 1;
        status.fields.read_write = transfer->read;
        status.fields.tx_req = !transfer->read;
        status.fields.rx_req = transfer->read;
        if (transfer->read)
        {
            int value = s_tdc_rx_index < s_tdc_rx_count ? s_tdc_rx[s_tdc_rx_index++] : 0;
            reg_write(&I2C0->RX_DATA, (uint32_t)value);
        }
    }
    reg_write(&I2C0->STATUS, status.status);
    I2C_0_IRQHandler();
    if (status.fields.stop_detected) tdc_ut_io_cm3_host_i2c_reset();
    if (!status.fields.stop_detected)
    {
        assert(transfer->count < 64);
        transfer->data[transfer->count++] = transfer->read ? (int)I2C0->RX_DATA : (int)I2C0->TX_DATA;
    }
}

void tdc_ut_io_cm3_host_i2c_start(int addr, int read)
{
    assert(g_tdc_ut_io_cm3_transfer_count < 128);
    tdc_ut_io_cm3_transfer_t *record = &g_tdc_ut_io_cm3_transfer[g_tdc_ut_io_cm3_transfer_count++];
    memset(record, 0, sizeof(*record));
    record->addr = addr;
    record->read = read;
}

void tdc_ut_io_cm3_host_i2c_reset(void)
{
    tdc_hal_i2c_status_t status = {0};
    status.fields.line_free = 1;
    status.fields.tx_req = 1;
    reg_write(&I2C0->STATUS, status.status);
}

void tdc_ut_io_cm3_host_delay(uint32_t cycles) { (void)cycles; s_tdc_tick++; tdc_ut_io_cm3_host_pump(); tdc_hal_spi_clear_master_read_command(); }
void tdc_ut_io_cm3_host_gpio(int pin, int value) { assert(pin >= 0 && pin < 32); g_tdc_ut_io_cm3_gpio[pin] = value; }
int tdc_ut_io_cm3_host_gpio_read(int pin) { return pin == TDC_TOUCH_IQS323_RDY_PIN ? (s_tdc_window_reads++ & 1) : g_tdc_ut_io_cm3_gpio[pin]; }
void tdc_ut_io_cm3_host_reset_request(void) { g_tdc_ut_io_cm3_reset_count++; }
void tdc_ut_io_cm3_host_ack(void) { tdc_hal_spi_clear_master_read_command(); }
int tdc_ut_io_cm3_host_tick(void) { return s_tdc_tick; }
void tdc_ut_io_cm3_host_advance(int milliseconds) { assert(milliseconds >= 0); s_tdc_tick += milliseconds; }
int tdc_hal_timer_get_tick(void) { return s_tdc_tick++; }
int tdc_hal_timer_get_t3_tick(void) { return s_tdc_tick; }
int SEGGER_RTT_printf(unsigned channel, const char *format, ...) { (void)channel; (void)format; return 0; }
int SEGGER_RTT_Wrapper_printf(unsigned channel, const char *format, ...) { (void)channel; (void)format; return 0; }
void tdc_printf_file_func_line(const char *file, const char *function, int line) { (void)file; (void)function; (void)line; }
int RTT_printf(const char *format, ...) { (void)format; return 0; }
void tdc_util_assert(int value) { assert(value); }

FIL *tdc_fs_get_fp(void) { return &s_tdc_file; }
int tdc_fs_fatfs_remount(int drive) { g_tdc_ut_io_cm3_fs_drive = drive; return 1; }
EN__BOOT_RET tdc_boot_get_status(tdc_boot_status_t *out) { if (g_tdc_ut_io_cm3_boot_error) return BOOT_RET_FAIL; *out = g_tdc_ut_io_cm3_boot_status; return BOOT_RET_TRUE; }
EN__BOOT_RET tdc_boot_update_status(tdc_boot_status_t *status) { if (g_tdc_ut_io_cm3_fs_error) return BOOT_RET_FAIL; g_tdc_ut_io_cm3_boot_status = *status; return BOOT_RET_TRUE; }

FRESULT f_stat(const TCHAR *path, FILINFO *info) { (void)path; memset(info, 0, sizeof(*info)); info->fsize = g_tdc_ut_io_cm3_fs_length; return g_tdc_ut_io_cm3_fs_error ? FR_DISK_ERR : FR_OK; }
FRESULT f_mkdir(const TCHAR *path) { (void)path; return g_tdc_ut_io_cm3_fs_error ? FR_DISK_ERR : FR_OK; }
FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode) { (void)file; (void)mode; assert(strlen(path) < sizeof(g_tdc_ut_io_cm3_fs_path)); strcpy(g_tdc_ut_io_cm3_fs_path, path); s_tdc_file_offset = 0; return g_tdc_ut_io_cm3_fs_error ? FR_DISK_ERR : FR_OK; }
FRESULT f_lseek(FIL *file, FSIZE_t offset) { (void)file; s_tdc_file_offset = (unsigned)offset; return FR_OK; }
FRESULT f_close(FIL *file) { (void)file; g_tdc_ut_io_cm3_fs_close_count++; return FR_OK; }
FRESULT f_write(FIL *file, const void *data, UINT count, UINT *written)
{
    (void)file;
    *written = 0;
    if (g_tdc_ut_io_cm3_fs_error) return FR_DISK_ERR;
    *written = count - ((g_tdc_ut_io_cm3_fs_short && count > 0) ? 1u : 0u);
    assert(s_tdc_file_offset + *written <= sizeof(g_tdc_ut_io_cm3_fs_bytes));
    memcpy(g_tdc_ut_io_cm3_fs_bytes + s_tdc_file_offset, data, *written);
    s_tdc_file_offset += *written;
    g_tdc_ut_io_cm3_fs_length = s_tdc_file_offset;
    return FR_OK;
}

/* 게인 시험의 저장 경계다. CRC/AES 구현의 정확성은 이 fixture의 시험 범위가 아니다. */
int tdc_fs_read_with_crc_and_aes128(char *name, uint8_t *data, int count, uint32_t *crc, uint32_t *padding, int padding_count, bool enable_crc, bool enable_aes)
{
    (void)crc; (void)padding; (void)padding_count; (void)enable_crc; (void)enable_aes;
#if !defined(TDC_UT_IO_CM3_RAM_ONLY)
    if (strcmp(name, TDC_FS_GAIN_FILE_NAME) != 0)
        return tdc_ut_io_cm3_store_read(name, data, count, enable_crc, enable_aes);
#endif
    assert(strcmp(name, TDC_FS_GAIN_FILE_NAME) == 0 && count == sizeof(s_tdc_gain_file));
    if (!s_tdc_gain_present || g_tdc_ut_io_cm3_fs_error) return -1;
    memcpy(data, &s_tdc_gain_file, (size_t)count); return count;
}
int tdc_fs_write_with_crc_and_aes128(char *name, uint8_t *data, int count, uint32_t *crc, uint32_t *padding, int padding_count, bool enable_crc, bool enable_aes)
{
    (void)crc; (void)padding; (void)padding_count; (void)enable_crc; (void)enable_aes;
#if !defined(TDC_UT_IO_CM3_RAM_ONLY)
    if (strcmp(name, TDC_FS_GAIN_FILE_NAME) != 0)
        return tdc_ut_io_cm3_store_write(name, data, count, enable_crc, enable_aes);
#endif
    assert(strcmp(name, TDC_FS_GAIN_FILE_NAME) == 0 && count == sizeof(s_tdc_gain_file));
    if (g_tdc_ut_io_cm3_fs_error || g_tdc_ut_io_cm3_fs_short) return -1;
    memcpy(&s_tdc_gain_file, data, (size_t)count); s_tdc_gain_present = 1; return count;
}

void tdc_ut_io_cm3_host_setup(void)
{
    s_tdc_saved_shared = cfx_cm3_sharedMemoryAll;
    s_tdc_saved_hook = g_tdc_ut_io_cm3_i2c_poll_hook;
    memset(&cfx_cm3_sharedMemoryAll, 0, sizeof(cfx_cm3_sharedMemoryAll));
    cfx_cm3_sharedMemoryAll.connected_ISD_num = 1;
    memset(g_tdc_ut_io_cm3_transfer, 0, sizeof(g_tdc_ut_io_cm3_transfer));
    g_tdc_ut_io_cm3_transfer_count = 0;
    memset(g_tdc_ut_io_cm3_gpio, 0, sizeof(g_tdc_ut_io_cm3_gpio));
    g_tdc_ut_io_cm3_bus_error = g_tdc_ut_io_cm3_reset_count = 0;
    g_tdc_ut_io_cm3_bus_error_transfer = 0;
    g_tdc_ut_io_cm3_fs_error = g_tdc_ut_io_cm3_fs_short = 0;
    g_tdc_ut_io_cm3_fs_close_count = 0;
    g_tdc_ut_io_cm3_fs_drive = SND_FATFS_LDRV_NUM_USER_DATA;
    g_tdc_ut_io_cm3_fs_path[0] = 0;
    g_tdc_ut_io_cm3_fs_length = 0;
    memset(g_tdc_ut_io_cm3_fs_bytes, 0, sizeof(g_tdc_ut_io_cm3_fs_bytes));
    memset(&g_tdc_ut_io_cm3_boot_status, 0, sizeof(g_tdc_ut_io_cm3_boot_status));
    g_tdc_ut_io_cm3_boot_status.boot_slot_num = 1;
    g_tdc_ut_io_cm3_boot_error = 0;
    s_tdc_tick = s_tdc_window_reads = 0;
    tdc_ut_io_cm3_host_rx(NULL, 0);
    tdc_hal_i2c_clear_driver_status(); tdc_ut_io_cm3_host_i2c_reset();
    tdc_hal_spi_clear_tx_buffer(); tdc_hal_spi_clear_master_read_command();
    g_tdc_ut_io_cm3_i2c_poll_hook = tdc_ut_io_cm3_host_pump;
}
void tdc_ut_io_cm3_host_restore(void)
{
    g_tdc_ut_io_cm3_i2c_poll_hook = s_tdc_saved_hook;
    tdc_hal_i2c_clear_driver_status(); tdc_hal_spi_clear_tx_buffer(); tdc_hal_spi_clear_master_read_command();
    cfx_cm3_sharedMemoryAll = s_tdc_saved_shared;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    seed_product_ram();
    g_tdc_ut_cm3_vector_done=1;
    assert(tdc_ut_io_cm3_run_unit(26)==TDC_UT_IO_CM3_BUSY);
    assert(tdc_ut_io_cm3_observe_unit(32,1)==TDC_UT_IO_CM3_BUSY);
    g_tdc_ut_cm3_vector_done=0;
#if defined(TDC_UT_IO_CM3_RAM_ONLY)
    static const int units[] = {10,25,26,34,35,36,37,40,45,47,50,51};
#else
    static const int units[] = {2,10,25,26,34,35,36,37,40,45,47,50,51};
#endif
    static const int observed_units[] = {2,10,25,26,31,32,33,34,35,36,37,38,39,40,41,42,43,45,46,47,48,49,50,51};
    int failed = 0, total_cases = 0, total_words = 0;
    for (unsigned u = 0; u < sizeof(units)/sizeof(units[0]); u++)
    {
        tdc_ut_io_cm3_saved_ram_t before, after;
        capture_product_ram(&before);
        int ret = tdc_ut_io_cm3_run_unit(units[u]);
        assert(ret == TDC_UT_IO_CM3_OK && g_tdc_ut_io_cm3_done && !g_tdc_ut_io_cm3_overflow);
        capture_product_ram(&after);
        assert(memcmp(&before, &after, sizeof(before)) == 0);
        assert(tdc_ut_io_cm3_run_unit(units[u]) == TDC_UT_IO_CM3_BUSY);
        assert(!g_tdc_ut_io_cm3_observed);
        int cursor=0, cases=0, word_case[TDC_UT_IO_CM3_CAPACITY], word_field[TDC_UT_IO_CM3_CAPACITY];
        while(cursor<g_tdc_ut_io_cm3_count)
        {
            int count=g_tdc_ut_io_cm3_actual[cursor+3];
            assert(g_tdc_ut_io_cm3_actual[cursor]==-999 && g_tdc_ut_io_cm3_actual[cursor+1]==units[u]);
            assert(g_tdc_ut_io_cm3_actual[cursor+2]==++cases);
            assert(count>=0 && cursor+4+count<=g_tdc_ut_io_cm3_count);
            for(int idx=cursor;idx<cursor+4+count;idx++) { word_case[idx]=cases; word_field[idx]=idx-cursor-4; }
            cursor+=4+count;
        }
        assert(cursor==g_tdc_ut_io_cm3_count && cases>0);
        int mismatch = 0;
        for (int idx = 0; idx < g_tdc_ut_io_cm3_count; idx++)
        {
            if (g_tdc_ut_io_cm3_actual[idx] != g_tdc_ut_io_cm3_expected[idx])
            {
                fprintf(stderr, "CM3 unit=%d case=%d field=%d word=%d actual=%d expected=%d\n", units[u], word_case[idx],word_field[idx],idx, g_tdc_ut_io_cm3_actual[idx], g_tdc_ut_io_cm3_expected[idx]);
                mismatch++;
#if defined(TDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS)
                assert(units[u]==51 && (word_case[idx]==17 || word_case[idx]==18) && (word_field[idx]==0 || word_field[idx]==1));
                assert(g_tdc_ut_io_cm3_actual[idx]==(word_field[idx]==0?195:0));
                assert(g_tdc_ut_io_cm3_expected[idx]==(word_field[idx]==0?240:(word_case[idx]==17?3:4)));
#endif
            }
        }
        printf("CM3 unit=%d cases=%d words=%d mismatches=%d done=1 overflow=0\n", units[u], cases, g_tdc_ut_io_cm3_count, mismatch);
        total_cases += cases; total_words += g_tdc_ut_io_cm3_count;
        failed += mismatch;
#if defined(TDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS)
        assert(mismatch==(units[u]==51?4:0));
#endif
        int first_count = g_tdc_ut_io_cm3_count;
        int first_actual[TDC_UT_IO_CM3_CAPACITY], first_expected[TDC_UT_IO_CM3_CAPACITY];
        memcpy(first_actual, g_tdc_ut_io_cm3_actual, (size_t)first_count * sizeof(int));
        memcpy(first_expected, g_tdc_ut_io_cm3_expected, (size_t)first_count * sizeof(int));
        assert(tdc_ut_io_cm3_release() == TDC_UT_IO_CM3_OK);
        assert(tdc_ut_io_cm3_run_unit(units[u]) == TDC_UT_IO_CM3_OK);
        assert(g_tdc_ut_io_cm3_count == first_count && !g_tdc_ut_io_cm3_overflow);
        assert(memcmp(first_actual, g_tdc_ut_io_cm3_actual, (size_t)first_count * sizeof(int)) == 0);
        assert(memcmp(first_expected, g_tdc_ut_io_cm3_expected, (size_t)first_count * sizeof(int)) == 0);
        capture_product_ram(&after);
        assert(memcmp(&before, &after, sizeof(before)) == 0);
        assert(tdc_ut_io_cm3_release() == TDC_UT_IO_CM3_OK);
    }
    assert(tdc_ut_io_cm3_run_unit(999) == TDC_UT_IO_CM3_UNSUPPORTED);
    assert(tdc_ut_io_cm3_observe_unit(999,1)==TDC_UT_IO_CM3_UNSUPPORTED);
    assert(tdc_ut_io_cm3_observe_unit(32,0)==TDC_UT_IO_CM3_INVALID);
    ST__CFX_CM3_SharedMemory_ALL saved_shared=cfx_cm3_sharedMemoryAll;
    int saved_tx[TDC_HAL_SPI_COMM_PACKET_SIZE], tx[TDC_HAL_SPI_COMM_PACKET_SIZE];
    tdc_ut_io_cm3_spi_snapshot(saved_tx,TDC_HAL_SPI_COMM_PACKET_SIZE);
    for(unsigned u=0;u<sizeof(observed_units)/sizeof(observed_units[0]);u++)
    {
        assert(tdc_ut_io_cm3_observe_unit(observed_units[u],100)==TDC_UT_IO_CM3_OK);
        assert(g_tdc_ut_io_cm3_observed && g_tdc_ut_io_cm3_done && !g_tdc_ut_io_cm3_overflow);
        assert(g_tdc_ut_io_cm3_actual[0]==-999 && g_tdc_ut_io_cm3_actual[1]==observed_units[u]);
        assert(g_tdc_ut_io_cm3_actual[2]==100 && g_tdc_ut_io_cm3_actual[3]+4==g_tdc_ut_io_cm3_count);
        assert(tdc_ut_io_cm3_observe_unit(observed_units[u],101)==TDC_UT_IO_CM3_BUSY);
        assert(memcmp(&saved_shared,&cfx_cm3_sharedMemoryAll,sizeof(saved_shared))==0);
        tdc_ut_io_cm3_spi_snapshot(tx,TDC_HAL_SPI_COMM_PACKET_SIZE);
        assert(memcmp(tx,saved_tx,sizeof(tx))==0);
        assert(g_tdc_ut_io_cm3_reset_count==0);
        assert(tdc_ut_io_cm3_release()==TDC_UT_IO_CM3_OK);
    }
    printf("OBSERVER API checked=%u nonmutating=1 NOT_PRODUCT_PASS\n",(unsigned)(sizeof(observed_units)/sizeof(observed_units[0])));
    printf("RESTORE/REPEAT checked=%u seeded_private_state=1 unchanged=1 identical_vectors=1\n", (unsigned)(sizeof(units)/sizeof(units[0])));
    printf("CM3 IO executed=%u cases=%d words=%d failed_words=%d\n", (unsigned)(sizeof(units)/sizeof(units[0])), total_cases, total_words, failed);
#if !defined(TDC_UT_IO_CM3_RAM_ONLY)
    failed += tdc_ut_io_cm3_extended_run();
#endif
#if defined(TDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS)
    puts("KNOWN FINDINGS CHECK OK; affected units remain contract FAIL");
    return 0;
#else
    return failed ? 1 : 0;
#endif
}
#endif
