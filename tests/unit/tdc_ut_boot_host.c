/* 호스트 경계만 대체한다. 시험 대상은 별도 컴파일한 제품 .c의 실제 함수다.
 * 파일은 실행 시 전달한 임시 fixture 루트에만 생성한다. 장치 I/O는 없다. */
#include "tdc_ut_boot_host.h"
#include "tdc_ut_boot.h"
#include <bootloader_internal.h>
#include <main.h>
#include <service.h>
#include <tdc_boot_led.h>
#include <rtt_printf.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <trims.h>

CRC_Type g_tdc_boot_crc_regs;
D_CLK_Type g_tdc_boot_clock_regs;
uint32_t g_tdc_boot_manu[128], g_tdc_boot_key[4], g_tdc_boot_app_number;
uint32_t SystemCoreClock = 30720000U;
static char s_tdc_root[512], s_tdc_cwd[32] = "/";
static unsigned s_tdc_crc_value;
static unsigned s_tdc_crc_bytes;
static int s_tdc_nvm_error, s_tdc_mount_error, s_tdc_read_error;
static int s_tdc_chdrive_error, s_tdc_opendir_error, s_tdc_closedir_error, s_tdc_sync_error;
static unsigned s_tdc_nvm_read_calls, s_tdc_nvm_fail_mask, s_tdc_reinit_calls;
static unsigned s_tdc_file_read_calls, s_tdc_file_fail_call, s_tdc_open_handles;
static unsigned s_tdc_delay_calls, s_tdc_trim_calls, s_tdc_timer_calls, s_tdc_dio_configs;
static unsigned s_tdc_nvm_reads, s_tdc_boot_calls, s_tdc_reset_calls;
static unsigned s_tdc_gpio_writes, s_tdc_timer_active, s_tdc_delay_cycles;
static unsigned s_tdc_lock_count, s_tdc_transfer_count;
static uint32_t s_tdc_locks[16];
static unsigned s_tdc_lock_after_transfer;
static unsigned s_tdc_events[128], s_tdc_event_count;
static unsigned s_tdc_app_opens, s_tdc_app_order[8];
static uint8_t s_tdc_nvm[2048];
static char s_tdc_output[16384];
static size_t s_tdc_output_len;
static const char *s_tdc_rtt_input;
static const char *s_tdc_open_error_name;
static unsigned s_tdc_service_ticks;
static unsigned s_tdc_observe_counts[6];
static uint32_t s_tdc_observe_values[6][4];

static void observe(unsigned id, const uint32_t *values, unsigned count)
{
    if(id>=6||count>4)abort();
    ++s_tdc_observe_counts[id];
    memcpy(s_tdc_observe_values[id],values,count*sizeof(*values));
}

typedef struct { uint32_t config, count; uintptr_t src, dst; } tdc_ut_boot_dma_t;
static tdc_ut_boot_dma_t s_tdc_dma[8];
static uint8_t s_tdc_transferred[8192];
static size_t s_tdc_transferred_bytes;
static uint32_t s_tdc_transfer_dest, s_tdc_transfer_config, s_tdc_transfer_length;

static void event(unsigned id)
{
    if (s_tdc_event_count < 128) s_tdc_events[s_tdc_event_count++] = id;
}

/* CRC-CCITT 경계 모델: poly=0x1021, init=0xffff, little-endian 입력.
 * 제품 CRC 함수의 워드/나머지 처리와 실제 AES는 대체하지 않는다. */
static unsigned crc_byte(unsigned crc, unsigned value)
{
    crc ^= value << 8;
    for (unsigned i = 0; i < 8; ++i)
        crc = (crc & 0x8000U) ? ((crc << 1) ^ 0x1021U) : (crc << 1);
    return crc & 0xffffU;
}
static unsigned crc_bytes(const void *data, size_t count)
{
    unsigned crc = 0xffffU;
    const uint8_t *bytes = data;
    for (size_t i = 0; i < count; ++i) crc = crc_byte(crc, bytes[i]);
    return crc;
}
void Sys_Set_CRC_Config(CRC_Type *crc, uint32_t config) { crc->CFG = config; }
void Sys_CRC_CCITTInitValue(CRC_Type *crc)
{ (void)crc; s_tdc_crc_value = 0xffffU; s_tdc_crc_bytes = 0; }
void Sys_CRC_Add(CRC_Type *crc, uint32_t value, uint32_t bits)
{
    (void)crc;
    for (unsigned i = 0; i < bits / 8; ++i)
    {
        s_tdc_crc_value = crc_byte(s_tdc_crc_value, (value >> (8 * i)) & 255U);
        ++s_tdc_crc_bytes;
    }
}
uint32_t Sys_CRC_GetFinalValue(const CRC_Type *crc) { (void)crc; return s_tdc_crc_value; }

void Sys_Memory_Enable(uint32_t cfg0, uint32_t cfg1)
{ (void)cfg0; (void)cfg1; event(10); }
void Sys_PRAMSec_SetLock(uint32_t config)
{
    if (s_tdc_lock_count < 16) s_tdc_locks[s_tdc_lock_count++] = config;
    if (s_tdc_transfer_count) ++s_tdc_lock_after_transfer;
    event(11);
}
void Sys_DMA_ChannelConfig(unsigned ch, uint32_t config, uint32_t count,
                           uint32_t unused, uintptr_t src, uintptr_t dst)
{
    (void)unused;
    if (ch >= 8) abort();
    s_tdc_dma[ch] = (tdc_ut_boot_dma_t){config,count,src,dst};
}
void Sys_DMA_Mode_Enable(unsigned ch, uint32_t mode)
{
    if (mode != DMA_ENABLE) return;
    tdc_ut_boot_dma_t *d = &s_tdc_dma[ch];
    const uint8_t *src = (const uint8_t *)d->src;
    if (ch == CRC_CALCULATION_DMA)
    {
        for (unsigned i = 0; i < d->count; ++i)
        {
            uint32_t word;
            memcpy(&word, src + 4 * i, 4);
            Sys_CRC_Add(CRC, word, 32);
        }
        return;
    }
    if (ch != SECTION_TRANSFER_DMA) abort();
    /* DMA 전송 규격을 기록한다. 메모리 주소를 역참조하지 않는다.
     * 8->24는 3바이트가 1워드에, 32->32는 4바이트가 1워드에 전달된다. */
    uint32_t word_mode = d->config & DMA_CFG0_SRC_DEST_WORD_SIZE_Mask;
    size_t bytes = word_mode == WORD_SIZE_32BITS_TO_32BITS ? d->count * 4U : d->count;
    if (s_tdc_transferred_bytes + bytes > sizeof(s_tdc_transferred)) abort();
    if ((d->config & DMA_CFG0_SRC_ADDR_STEP_Mask) == DMA_SRC_ADDR_STATIC)
        memset(s_tdc_transferred + s_tdc_transferred_bytes, 0, bytes);
    else memcpy(s_tdc_transferred + s_tdc_transferred_bytes, src, bytes);
    s_tdc_transferred_bytes += bytes;
    s_tdc_transfer_dest = (uint32_t)d->dst;
    s_tdc_transfer_config = word_mode;
    s_tdc_transfer_length = d->count;
    ++s_tdc_transfer_count;
    event(12);
}
uint32_t Sys_DMA_Get_Status(unsigned ch) { (void)ch; return 0; }
void Sys_DMA_Set_Ctrl(unsigned ch, uint32_t cfg) { (void)ch; (void)cfg; }
void Sys_GPIO_Write(unsigned pin, unsigned value)
{ (void)pin; (void)value; ++s_tdc_gpio_writes; }
void Sys_GPIO_Set_Low(unsigned pin) { Sys_GPIO_Write(pin, 0); }
void Sys_DIO_Config(unsigned pin, uint32_t config) { (void)pin; (void)config; ++s_tdc_dio_configs; }
unsigned Sys_GPIO_Read(unsigned pin) { (void)pin; return 1; }
void Sys_Timer_Stop(unsigned timer) { (void)timer; s_tdc_timer_active = 0; ++s_tdc_timer_calls; }
void Sys_Timer_Start(unsigned timer) { (void)timer; s_tdc_timer_active = 1; ++s_tdc_timer_calls; }
void Sys_Timer_Config(unsigned t, uint32_t p, uint32_t m, uint32_t r)
{ (void)t; (void)p; (void)m; (void)r; ++s_tdc_timer_calls; }
void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
void NVIC_SetPriority(int irq, unsigned priority) { (void)irq; (void)priority; }
void NVIC_EnableIRQ(int irq) { (void)irq; }
void NVIC_DisableIRQ(int irq) { (void)irq; }
void Sys_Delay(uint32_t cycles) { s_tdc_delay_cycles = cycles; ++s_tdc_delay_calls; }
void tdc_ut_boot_host_reset(void) { ++s_tdc_reset_calls; }

/* 경로는 소유한 임시 fixture 루트 안에만 해석한다. */
static void file_path(const char *name, char out[1024])
{
    if (strstr(name, "..") || strchr(name, ':') || strchr(name, '\\')) abort();
    int n = snprintf(out, 1024, "%s%s%s%s", s_tdc_root,
                     name[0] == '/' ? "" : s_tdc_cwd,
                     name[0] == '/' || strcmp(s_tdc_cwd, "/") == 0 ? "" : "/", name);
    if (n < 0 || n >= 1024) abort();
}
FRESULT f_open(FIL *fp, const char *name, unsigned mode)
{
    if(s_tdc_open_error_name && strcmp(name,s_tdc_open_error_name)==0)return FR_NO_FILE;
    char path[1024]; file_path(name, path);
    fp->file = fopen(path, (mode & FA_WRITE) ? "r+b" : "rb");
    if (!fp->file && (mode & FA_OPEN_ALWAYS)) fp->file = fopen(path, "w+b");
    fp->obj.sclust = 1;
    if (!fp->file) return FR_NO_FILE;
    ++s_tdc_open_handles;
    if (strstr(name, ".FEZ"))
    {
        if (s_tdc_app_opens < 8) s_tdc_app_order[s_tdc_app_opens] = strstr(name, "B.FEZ") ? 2 : 1;
        ++s_tdc_app_opens;
    }
    return FR_OK;
}
FRESULT f_close(FIL *fp)
{
    if (!fp->file) return FR_INVALID_OBJECT;
    int ret = fclose(fp->file); fp->file = NULL;
    --s_tdc_open_handles;
    return ret == 0 ? FR_OK : FR_DISK_ERR;
}
FRESULT f_read(FIL *fp, void *out, UINT bytes, UINT *read)
{
    *read = 0;
    if (!fp || !fp->file) return FR_INVALID_OBJECT;
    ++s_tdc_file_read_calls;
    if (s_tdc_read_error || s_tdc_file_read_calls == s_tdc_file_fail_call) return FR_DISK_ERR;
    *read = fread(out, 1, bytes, fp->file);
    return ferror(fp->file) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_write(FIL *fp, const void *data, UINT bytes, UINT *written)
{
    *written = 0;
    if (!fp || !fp->file) return FR_INVALID_OBJECT;
    *written = fwrite(data, 1, bytes, fp->file);
    return *written == bytes ? FR_OK : FR_DISK_ERR;
}
FRESULT f_lseek(FIL *fp, size_t pos)
{
    if (!fp->file) return FR_INVALID_OBJECT;
    if (fseek(fp->file, 0, SEEK_END)) return FR_DISK_ERR;
    long size = ftell(fp->file);
    if (size < 0) return FR_DISK_ERR;
    if (pos > (size_t)size)
    {
        if (fseek(fp->file, (long)pos - 1, SEEK_SET) || fputc(0, fp->file) == EOF)
            return FR_DISK_ERR;
    }
    return fseek(fp->file, (long)pos, SEEK_SET) ? FR_DISK_ERR : FR_OK;
}
FRESULT f_sync(FIL *fp) { return fflush(fp->file) ? FR_DISK_ERR : FR_OK; }
FRESULT f_stat(const char *name, FILINFO *info)
{
    char path[1024]; struct stat st; file_path(name, path);
    if (stat(path, &st)) return FR_NO_FILE;
    info->fsize = (size_t)st.st_size;
    snprintf(info->fname, sizeof(info->fname), "%s", name);
    return FR_OK;
}
FRESULT f_chdir(const char *name)
{
    char path[1024]; struct stat st; file_path(name, path);
    if (stat(path, &st)) return FR_NO_FILE;
    snprintf(s_tdc_cwd, sizeof(s_tdc_cwd), "%s", name);
    return FR_OK;
}
FRESULT f_mount(FATFS *fs, const char *drive, unsigned mount)
{ (void)fs; (void)drive; (void)mount; event(2); return s_tdc_mount_error; }
FRESULT f_chdrive(const char *drive) { (void)drive; return s_tdc_chdrive_error; }
FRESULT f_opendir(DIR *dir, const char *path) { (void)dir; (void)path; return s_tdc_opendir_error; }
FRESULT f_closedir(DIR *dir) { (void)dir; return s_tdc_closedir_error; }
FRESULT f_unmount(const char *drive) { (void)drive; return FR_OK; }
void NVMGetDefaultOptions(NVMCTRL_Options_t *options) { options->config = 0; }
int NVMInit(NVMCTRL_Options_t *options)
{ (void)options; ++s_tdc_nvm_reads; event(1); return s_tdc_nvm_error; }
void NVMReInitOptions(NVMCTRL_Options_t *options) { (void)options; ++s_tdc_reinit_calls; }
int NVMReadBytes(uint32_t offset, uint32_t bytes, uint32_t *out)
{
    ++s_tdc_nvm_read_calls;
    if (s_tdc_nvm_read_calls <= 32 &&
        (s_tdc_nvm_fail_mask & (1U << (s_tdc_nvm_read_calls - 1)))) return -9;
    if (offset > sizeof(s_tdc_nvm) || bytes > sizeof(s_tdc_nvm) - offset) return -1;
    memcpy(out, s_tdc_nvm + offset, bytes);
    ++s_tdc_nvm_reads;
    return 0;
}
bool NVMSync(void) { return !s_tdc_sync_error; }
void Sys_Trims_LoadManuTable(uint32_t *table) { memcpy(g_tdc_boot_manu, table, MANU_TABLE_SIZE * 4); ++s_tdc_trim_calls; }
void Sys_Trims_SetVREGAndLSAD(void) { ++s_tdc_trim_calls; }
void Sys_Trims_SetADCOffsets(void) { ++s_tdc_trim_calls; }
void Sys_Trims_SetOperatingFrequency(unsigned target) { (void)target; ++s_tdc_trim_calls; }
#define TDC_TRIM_STUB(name) void name(unsigned value) { (void)value; ++s_tdc_trim_calls; }
TDC_TRIM_STUB(Sys_Trims_SetVDDIF)
TDC_TRIM_STUB(Sys_Trims_SetVDDA)
TDC_TRIM_STUB(Sys_Trims_SetVDDC)
TDC_TRIM_STUB(Sys_Trims_SetVDDC_CP)
TDC_TRIM_STUB(Sys_Trims_SetVDDM)
TDC_TRIM_STUB(Sys_Trims_SetVDDM_CP)
TDC_TRIM_STUB(Sys_Trims_SetVDDOD)
TDC_TRIM_STUB(Sys_Trims_SetVMIC)
int bootloader_boot_cfx(uint32_t address) { (void)address; ++s_tdc_boot_calls; return 0; }
int bootloader_boot_cm3(uint32_t stack, uint32_t address)
{ (void)stack; (void)address; ++s_tdc_boot_calls; return 0; }
void tdc_uart_init(void) { }
/* 기존 제품 debug 문자열은 합격 판단에 사용하지 않는다. */
void tdc_uart_printf(const char *format, ...) { (void)format; }
void rtt_printf(const char *format, ...)
{
    va_list args; va_start(args, format);
    int n = vsnprintf(s_tdc_output + s_tdc_output_len,
                      sizeof(s_tdc_output) - s_tdc_output_len, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(s_tdc_output) - s_tdc_output_len) abort();
    s_tdc_output_len += (size_t)n;
}
int rtt_getch(char *ch)
{
    if (!s_tdc_rtt_input || !*s_tdc_rtt_input) return 0;
    *ch = *s_tdc_rtt_input++;
    return 1;
}
int SEGGER_RTT_SetFlagsUpBuffer(unsigned index, unsigned flags)
{ (void)index; (void)flags; return 0; }
int tdc_ut_boot_host_service_continue(void)
{
    if (s_tdc_service_ticks) { --s_tdc_service_ticks; return 1; }
    return s_tdc_rtt_input && *s_tdc_rtt_input;
}

static void clear_trace(void)
{
    /* 누수는 trace 초기화로 숨기지 않는다. 각 파일은 호출자가 닫아야 한다. */
    if (s_tdc_open_handles) abort();
    s_tdc_nvm_error = s_tdc_mount_error = s_tdc_read_error = 0;
    s_tdc_chdrive_error = s_tdc_opendir_error = s_tdc_closedir_error = s_tdc_sync_error = 0;
    s_tdc_nvm_read_calls = s_tdc_nvm_fail_mask = s_tdc_reinit_calls = 0;
    s_tdc_file_read_calls = s_tdc_file_fail_call = 0;
    s_tdc_delay_calls = s_tdc_trim_calls = s_tdc_timer_calls = s_tdc_dio_configs = 0;
    s_tdc_nvm_reads = s_tdc_boot_calls = s_tdc_reset_calls = 0;
    s_tdc_lock_count = s_tdc_transfer_count = s_tdc_lock_after_transfer = 0;
    s_tdc_event_count = s_tdc_app_opens = 0;
    s_tdc_transferred_bytes = 0;
    s_tdc_output_len = 0; s_tdc_output[0] = 0;
    s_tdc_rtt_input = NULL; s_tdc_service_ticks = 0;
    s_tdc_open_error_name = NULL;
    memset(s_tdc_observe_counts,0,sizeof(s_tdc_observe_counts));
    memset(s_tdc_observe_values,0,sizeof(s_tdc_observe_values));
    strcpy(s_tdc_cwd, "/");
}
static void save_fixture(const char *name, const void *data, size_t bytes)
{
    char path[1024]; file_path(name, path);
    FILE *fp = fopen(path, "wb");
    if (!fp || fwrite(data, 1, bytes, fp) != bytes || fclose(fp)) abort();
}
static void fixture_nvm(void)
{
    bootloader_boot_information info = {0};
    memset(s_tdc_nvm, 0, sizeof(s_tdc_nvm));
    strcpy((char *)info.boot_app_load, "A"); strcpy((char *)info.boot_app_ext, "FEZ");
    info.boot_speed = BOOT_SPEED_SSPI; info.boot_manu_off = 1024;
    info.boot_addr = 0x12345678; info.boot_manu_len = 64;
    info.boot_info_crc = (uint16_t)crc_bytes(&info, sizeof(info) - 2);
    memcpy(s_tdc_nvm + 512, &info, sizeof(info));
    MANU_TABLE_Type table = {0};
    table.Reserved[0] = 0x11223344;
    table.MANU_CRC_CCITT.upper = crc_bytes(&table, BOOTSTRAP_MANU_CRC_OFFSET);
    memcpy(s_tdc_nvm + 1024, &table, sizeof(table));
}
static void one(unsigned unit, unsigned vector, int actual, int expected)
{ tdc_ut_boot_record(unit, vector, &actual, &expected, 1); }
static void app_header_bytes(uint8_t out[12], uint16_t entries, uint16_t crc)
{
    memset(out, 0, 12);
    uint32_t ident = APPFILE_IDENT_APP;
    memcpy(out, &ident, 4); memcpy(out + 4, &entries, 2); memcpy(out + 6, &crc, 2);
    size_t bytes = entries ? 12 : 8;
    unsigned residue = crc_bytes(out, bytes - 2);
    out[bytes - 2] = (uint8_t)(residue >> 8); out[bytes - 1] = (uint8_t)residue;
}
static void seal_section(struct data_section_header *header)
{
    unsigned crc = crc_bytes(header, sizeof(*header) - 2);
    uint8_t *p = (uint8_t *)header;
    p[14] = (uint8_t)(crc >> 8); p[15] = (uint8_t)crc;
}
static uint16_t data_residue(const void *data, size_t bytes)
{
    /* 제품은 table 항목을 32비트 ADD로 추가한다: CRC 2바이트 뒤 0 2바이트.
     * payload + big-endian CRC의 잔여값이 0이면 뒤 0에도 0이다. */
    unsigned crc = crc_bytes(data, bytes);
    return (uint16_t)((crc >> 8) | ((crc & 255U) << 8));
}
static FIL open_fixture(const char *name)
{
    FIL fp = {0}; if (f_open(&fp, name, FA_READ) != FR_OK) abort(); return fp;
}

static void run_delay(void)
{
    /* 입력: 0/1000 us. 출력: 실제 지연 함수의 경계 cycle. 물리 시간 아님. */
    tdc_delay_us(0); one(2, 1, (int)s_tdc_delay_cycles, 0);
    tdc_delay_us(1000); one(2, 2, (int)s_tdc_delay_cycles, 30000);
    /* 30.72 MHz에서 us당 30으로 먼저 절삭하는 기존 인자 계약의 특성화다.
     * 정확한 경과 시간/오버플로 입력의 지원을 보장하지 않는다. */
    const uint32_t input[4] = {0,1,999,1000};
    const int us_cycles[4] = {0,30,29970,30000};
    const int ms_cycles[4] = {0,30720,30689280,30720000};
    for(unsigned i=0;i<4;++i)
    {
        tdc_delay_us(input[i]); one(2,3+i,(int)s_tdc_delay_cycles,us_cycles[i]);
        tdc_delay_ms(input[i]); one(2,7+i,(int)s_tdc_delay_cycles,ms_cycles[i]);
    }
    uint32_t saved_clock=SystemCoreClock;
    SystemCoreClock=32000000;
    tdc_delay_us(1000); one(2,11,(int)s_tdc_delay_cycles,32000);
    tdc_delay_ms(1); one(2,12,(int)s_tdc_delay_cycles,32000);
    SystemCoreClock=saved_clock;
}
void TIMER_2_IRQHandler(void);
static void run_led(void)
{
    /* 실제 ISR을 timer-active일 때만 주입. 정지 후에는 IRQ가 발생하지 않는다. */
    tdc_boot_led_init(); tdc_boot_led_start();
    one(4, 1, (int)tdc_boot_led_elapsed_ms(), 0);
    for (unsigned i = 0; i < 40 * 100; ++i) if (s_tdc_timer_active) TIMER_2_IRQHandler();
    one(4, 2, (int)tdc_boot_led_elapsed_ms(), 100);
    for (unsigned i = 0; i < 40 * 261; ++i) if (s_tdc_timer_active) TIMER_2_IRQHandler();
    one(4, 3, tdc_boot_led_is_done(), 1);
    tdc_boot_led_stop(); unsigned writes = s_tdc_gpio_writes;
    for (unsigned i = 0; i < 100; ++i) if (s_tdc_timer_active) TIMER_2_IRQHandler();
    const int actual[3] = {(int)tdc_boot_led_elapsed_ms(),tdc_boot_led_is_done(),s_tdc_gpio_writes == writes};
    const int expected[3] = {361,1,1}; tdc_ut_boot_record(4, 4, actual, expected, 3);
    one(4,5,(int)s_tdc_observe_counts[TDC_UT_BOOT_OBSERVE_LED],361);
}
static void run_storage(void)
{
    /* setup: 공개 NVM fixture. input: init/mount 오류. output: 반환과 경계 순서.
     * restore: 호스트 경계 설정을 0으로 돌린다. 실제 제조 데이터는 읽지 않는다. */
    fixture_nvm();
    s_tdc_nvm_error = -7; one(7, 1, tdc_boot_storage_init(NULL), -7);
    one(7, 2, (int)s_tdc_event_count, 1);
    clear_trace(); fixture_nvm(); s_tdc_mount_error = FR_DISK_ERR;
    one(7, 3, tdc_boot_storage_init(NULL), FR_DISK_ERR);
    clear_trace(); fixture_nvm();
    one(7, 4, tdc_boot_storage_init(NULL), 0);
    const int order[2] = {(int)s_tdc_events[0],(int)s_tdc_events[1]};
    const int expected[2] = {1,2}; tdc_ut_boot_record(7, 5, order, expected, 2);
    clear_trace(); fixture_nvm(); s_tdc_nvm[1024]^=1;
    /* 제조 정보 CRC 오류가 상위에서 무시되는 실제 반례를 보존한다. */
    one(7,6,tdc_boot_storage_init(NULL),-1);
    /* 출력은 성공 때만 게시한다. 하위 오류 반환과 실패 후 출력 불변을
     * 함께 확인한다. 실제 trim/NVM/파일시스템은 전부 호스트 경계다. */
    for(unsigned i=0;i<12;++i)
    {
        clear_trace(); fixture_nvm();
        bootloader_boot_information input, out, sentinel;
        memcpy(&input,s_tdc_nvm+512,sizeof(input));
        memset(&out,0xa6,sizeof(out)); sentinel=out;
        int expected_ret=0;
        if(i==0) { s_tdc_nvm_fail_mask=1; expected_ret=-9; }
        if(i==1) { input.boot_speed=255; expected_ret=-1; }
        if(i==2) { s_tdc_chdrive_error=FR_DISK_ERR; expected_ret=FR_DISK_ERR; }
        if(i==3) { s_tdc_opendir_error=FR_DISK_ERR; expected_ret=FR_DISK_ERR; }
        if(i==4) { s_tdc_closedir_error=FR_DISK_ERR; expected_ret=FR_DISK_ERR; }
        if(i==5) { s_tdc_sync_error=1; expected_ret=-1; }
        if(i==6) { input.boot_speed=BOOT_SPEED_AUTO; s_tdc_nvm_fail_mask=2; }
        if(i==7) { input.boot_speed=BOOT_SPEED_AUTO; s_tdc_nvm_fail_mask=6; }
        if(i==8) { input.boot_speed=BOOT_SPEED_DSPI; s_tdc_nvm_fail_mask=2; expected_ret=-9; }
        if(i==9) { input.boot_speed=BOOT_SPEED_QSPI; s_tdc_nvm_fail_mask=2; expected_ret=-9; }
        /* 제조 정보 read 오류도 CRC 오류와 동일하게 상위에서 유실된다. */
        if(i==10) { s_tdc_nvm_fail_mask=2; expected_ret=-9; }
        input.boot_info_crc=(uint16_t)crc_bytes(&input,sizeof(input)-2);
        memcpy(s_tdc_nvm+512,&input,sizeof(input));
        int ret=tdc_boot_storage_init(&out);
        const int actual[3]={ret,memcmp(&out,expected_ret?&sentinel:&input,sizeof(out))==0,
            (int)s_tdc_reinit_calls};
        const int expected[3]={expected_ret,1,i==6?2:i==7?3:(i==8||i==9)?1:0};
        tdc_ut_boot_record(7,7+i,actual,expected,3);
        if(s_tdc_boot_calls)abort();
    }
}
static void run_boot_info(void)
{
    fixture_nvm(); uint32_t data[128] = {0};
    one(8, 1, bootloader_read_nvm_boot_info(data), 0);
    bootloader_boot_information *info = (bootloader_boot_information *)data;
    int actual[4] = {(int)sizeof(*info),(int)info->boot_addr,info->boot_manu_off,info->boot_manu_len};
    const int expected[4] = {32,0x12345678,1024,64};
    tdc_ut_boot_record(8, 2, actual, expected, 4);
    s_tdc_nvm[512] ^= 1; one(8, 3, bootloader_read_nvm_boot_info(data), -1);
    one(8, 4, bootloader_read_nvm_boot_info(NULL), -1);
    fixture_nvm(); one(8,5,bootloader_load_manu_table(1024),0);
    MANU_TABLE_Type *table=(MANU_TABLE_Type *)g_tdc_boot_manu;
    one(8,6,(int)table->Reserved[0],0x11223344);
}
static void run_file(void)
{
    uint8_t header[12]; app_header_bytes(header, 0, 0);
    one(10, 1, bootloader_load_app_file("missing.FEZ"), -1);
    save_fixture("short.FEZ", header, 3);
    one(10, 2, bootloader_load_app_file("short.FEZ"), -1);
    header[7] ^= 1; save_fixture("bad.FEZ", header, 8);
    one(10, 3, bootloader_load_app_file("bad.FEZ"), -1);
    app_header_bytes(header, 0, 0); save_fixture("A.FEZ", header, 8);
    one(10, 4, bootloader_load_app_file("A.FEZ"), 0);
    one(10, 5, (int)s_tdc_transfer_count, 0);
    one(10, 6, (int)s_tdc_boot_calls, 0);
    one(10, 7, bootloader_load_app_file(NULL), -1);
    /* 정상 헤더 CRC 목록 255개: 512B 출력 버퍼 초과를 거부한다. */
    uint16_t count = 255; memcpy(header + 4, &count, 2);
    save_fixture("large.FEZ", header, 8);
    one(10, 8, bootloader_load_app_file("large.FEZ"), -1);
    app_header_bytes(header,0,0); memset(header,0,4);
    unsigned crc=crc_bytes(header,6); header[6]=(uint8_t)(crc>>8); header[7]=(uint8_t)crc;
    save_fixture("unknown.FEZ",header,8);
    one(10,9,bootloader_load_app_file("unknown.FEZ"),-1);
}
static void run_sections(void)
{
    uint32_t data[4] = {0x11,0x22,0x33,0x44};
    struct data_section_header header = {.size=4,.type=SEC_CFX_P};
    save_fixture("section.bin", data, sizeof(data));
    FIL fp = open_fixture("section.bin");
    one(11, 1, bootloader_transfer_section_data(&header,&fp,data_residue(data,sizeof(data))),0);
    f_close(&fp);
    header.type = 0xffff; fp = open_fixture("section.bin");
    one(11, 2, bootloader_transfer_section_data(&header,&fp,0),-1); f_close(&fp);
    header.type = SEC_CFX_P; header.offset = (CFX_PRAM_TOP-CFX_PRAM_BASE+1)/4;
    fp = open_fixture("section.bin");
    one(11, 3, bootloader_transfer_section_data(&header,&fp,0),-1); f_close(&fp);
    header.offset = 0; header.flags = APPDATA_FLAGS_ENCRYPTED; header.size = 3;
    fp = open_fixture("section.bin");
    one(11, 4, bootloader_transfer_section_data(&header,&fp,0),-1); f_close(&fp);
    header.flags = 0; header.size = 5; fp = open_fixture("section.bin");
    one(11, 5, bootloader_transfer_section_data(&header,&fp,0),-1); f_close(&fp);
    /* crc_table_size=1인데 구획이 없는 반례: 현 제품 코드는 성공으로 반환한다.
     * 기대값은 오류(-1)이며 이 결과는 checker에서 알려진 결함으로 보고한다. */
    uint32_t app_words[4] = {0};
    struct application_file_header *app = (struct application_file_header *)app_words;
    app->crc_table_size = 1; save_fixture("empty.bin", data, 0); fp=open_fixture("empty.bin");
    struct boot_section_data cfx={0}, cm3={0};
    one(11, 6, bootloader_load_sections(&fp,app,&cfx,&cm3),-1); f_close(&fp);
}
static void run_manifest(void)
{
    uint8_t header[12]; app_header_bytes(header,0,0);
    save_fixture("A.FEZ",header,8); save_fixture("B.FEZ",header,8);
    const char list[] = "A.FEZ\nB.FEZ\n";
    uint32_t words[4] = {0}; struct application_file_header *app=(struct application_file_header *)words;
    app->ident=APPFILE_IDENT_MANIFEST; app->crc_table_size=1;
    app->crc_table[0]=data_residue(list,sizeof(list)-1);
    save_fixture("manifest.bin",list,sizeof(list)-1); FIL fp=open_fixture("manifest.bin");
    one(12,1,bootloader_process_manifest(&fp,app),0);
    const int actual[3]={(int)s_tdc_app_opens,(int)s_tdc_app_order[0],(int)s_tdc_app_order[1]};
    const int expected[3]={2,1,2}; tdc_ut_boot_record(12,2,actual,expected,3);
    if (fp.file) f_close(&fp);
    fp=open_fixture("manifest.bin"); s_tdc_read_error=1;
    one(12,3,bootloader_process_manifest(&fp,app),-1); s_tdc_read_error=0; f_close(&fp);
    const char missing[]="missing.FEZ\n"; app->crc_table[0]=data_residue(missing,sizeof(missing)-1);
    save_fixture("manifest.bin",missing,sizeof(missing)-1); fp=open_fixture("manifest.bin");
    one(12,4,bootloader_process_manifest(&fp,app),-1); if(fp.file)f_close(&fp);
    app->crc_table[0]^=1; fp=open_fixture("manifest.bin");
    one(12,5,bootloader_process_manifest(&fp,app),-1); if(fp.file)f_close(&fp);
    one(12,6,bootloader_process_manifest(NULL,NULL),-1);
}
static void encrypt_header(struct data_section_header *header)
{
    struct AES_ctx ctx; bootloader_initialize_aes_ctx(&ctx,g_tdc_boot_key);
    uint32_t *words=(uint32_t *)header;
    for(unsigned i=0;i<4;++i)words[i]=htonl(words[i]);
    AES_ECB_encrypt(&ctx,(uint8_t *)words);
    for(unsigned i=0;i<4;++i)words[i]=htonl(words[i]);
}
static void run_crc(void)
{
    /* 입력: 공개 키, CRC 잔여값 0/오류, 복호화 표시 0/1.
     * 출력: 실제 헤더 검사 결과. 복원: 공개 키 override를 해제한다. */
    struct data_section_header header={.size=4,.type=SEC_CFX_P}; seal_section(&header);
    one(13,1,bootloader_check_header_crc(&header),0);
    one(13,2,bootloader_check_header_crc(NULL),-1);
    header.flags=APPDATA_FLAGS_ENCRYPTED; seal_section(&header); encrypt_header(&header);
    one(13,3,bootloader_check_header_crc(&header),0);
    header.flags=0; seal_section(&header); encrypt_header(&header);
    one(13,4,bootloader_check_header_crc(&header),-1);
    header.flags=APPDATA_FLAGS_ENCRYPTED; seal_section(&header); header.offset^=1; encrypt_header(&header);
    one(13,5,bootloader_check_header_crc(&header),-1);
    /* 독립 CCITT 기대값: 01 02 03 04 05. 실제 word/remainder loop 실행. */
    const uint32_t bytes[2]={0x04030201,0x00000005};
    const unsigned lengths[5]={0,1,3,4,5};
    const int expected[5]={0xffff,0xf1d1,0xadad,0x89c3,0x9304};
    for(unsigned i=0;i<5;++i)
    {
        one(13,6+i,(int)bootloader_CRC_calc((uint8_t *)bytes,lengths[i]),expected[i]);
        one(13,11+i,(int)s_tdc_crc_bytes,(int)lengths[i]);
    }
}
static void run_transfer(void)
{
    uint32_t data[4]={0x11,0x22,0x33,0x44}; save_fixture("data.bin",data,sizeof(data));
    const unsigned types[5]={SEC_CFX_X,SEC_CFX_Y,SEC_CFX_P,SEC_CM3_P,SEC_IOMEM};
    const uint32_t dest[5]={CFX_XMEM_LSB_UNSIGNED_BASE,CFX_YRAM0_LSB_UNSIGNED_BASE,
                            CFX_PRAM_BASE,PRAM_BASE,DRAM_BASE};
    for(unsigned i=0;i<5;++i)
    {
        clear_trace();
        struct data_section_header header={.size=4,.type=types[i]};
        if(types[i]==SEC_IOMEM)header.offset=DRAM_BASE/4;
        FIL fp=open_fixture("data.bin");
        int actual[7]={bootloader_transfer_section_data(&header,&fp,data_residue(data,sizeof(data))),
                      (int)s_tdc_transfer_dest,(int)s_tdc_transferred_bytes,
                      (int)s_tdc_transfer_length,(int)s_tdc_transfer_config,
                      memcmp(s_tdc_transferred,data,sizeof(data))==0,(int)s_tdc_transfer_count};
        const int expected[7]={0,(int)dest[i],16,i<2?16:4,
                                i<2?WORD_SIZE_8BITS_TO_24BITS:WORD_SIZE_32BITS_TO_32BITS,1,1};
        tdc_ut_boot_record(15,i+1,actual,expected,7); f_close(&fp);
        const int observed[4]={(int)s_tdc_observe_values[TDC_UT_BOOT_OBSERVE_TRANSFER][0],
            (int)s_tdc_observe_values[TDC_UT_BOOT_OBSERVE_TRANSFER][1],
            (int)s_tdc_observe_values[TDC_UT_BOOT_OBSERVE_TRANSFER][2],
            (int)s_tdc_observe_values[TDC_UT_BOOT_OBSERVE_TRANSFER][3]};
        const int wanted[4]={(int)types[i],(int)dest[i],
            i<2?WORD_SIZE_8BITS_TO_24BITS:WORD_SIZE_32BITS_TO_32BITS,i<2?16:4};
        tdc_ut_boot_record(15,101+i,observed,wanted,4);
    }
    clear_trace();
    struct data_section_header header={.size=4,.type=SEC_CM3_P,.flags=APPDATA_FLAGS_EMPTY};
    FIL fp={0}; one(15,6,bootloader_transfer_section_data(&header,&fp,0),0);
    const uint8_t zeros[16]={0}; one(15,7,memcmp(s_tdc_transferred,zeros,16)==0,1);
}
static void run_lock(void)
{
    uint32_t data[4]={0x11,0x22,0x33,0x44}; save_fixture("lock.bin",data,sizeof(data));
    struct data_section_header header={.size=4,.type=SEC_CFX_P,.flags=APPDATA_FLAGS_LOCKED};
    FIL fp=open_fixture("lock.bin");
    one(16,1,bootloader_transfer_section_data(&header,&fp,data_residue(data,sizeof(data))),0);
    const int actual[4]={(int)s_tdc_lock_count,(int)s_tdc_locks[0],(int)s_tdc_locks[1],(int)s_tdc_lock_after_transfer};
    const int expected[4]={2,CFX_PRAM0_NOT_LOCKED|CFX_PRAM1_NOT_LOCKED,CFX_PRAM0_LOCK,1};
    tdc_ut_boot_record(16,2,actual,expected,4); f_close(&fp);
}
static unsigned output_occurrences(const char *needle)
{
    unsigned count=0;
    for(const char *p=s_tdc_output;(p=strstr(p,needle))!=NULL;p+=strlen(needle))++count;
    return count;
}

static int service_file_matches(const char *name, const uint8_t *bytes, unsigned length)
{
    /* 파일 입력과 명세의 16바이트/LF/마커 규칙으로 별도 예상 블록을 만든다. */
    char expected[512];
    int pos=snprintf(expected,sizeof(expected),":::FILE:%s:BEGIN:::\n",name);
    for(unsigned i=0;i<length;++i)
    {
        pos+=snprintf(expected+pos,sizeof(expected)-(size_t)pos,"%02X ",bytes[i]);
        if((i+1)%16==0 || i+1==length)expected[pos++]='\n';
    }
    snprintf(expected+pos,sizeof(expected)-(size_t)pos,":::FILE:%s:END:::\n",name);
    return strstr(s_tdc_output,expected)!=NULL;
}

static void run_service(unsigned unit)
{
    uint32_t saved_iter; uint8_t saved_color;
    tdc_ut_boot_service_snapshot(&saved_iter,&saved_color);
    /* setup: local test files and RTT input. output: actual parser/hex/reset trace.
     * restore: static service counters, queues and fault overrides are restored. */
    fixture_nvm(); const uint8_t bytes[2]={0x12,0x34};
    const char *names[4]={"/MANIFEST.TXT","/APP000.FEZ","/APP001.FEZ","/APP002.FEZ"};
    for(unsigned i=0;i<4;++i)save_fixture(names[i],bytes,2);
    if(unit==17)
    {
        s_tdc_rtt_input="dump\n"; service_main();
        int actual[3]={strstr(s_tdc_output,":::FILE:MANIFEST.TXT:BEGIN:::\n12 34 \n:::FILE:MANIFEST.TXT:END:::")!=NULL,
                      strstr(s_tdc_output,"FW DUMP END")!=NULL,(int)s_tdc_boot_calls};
        const int expected[3]={1,1,0}; tdc_ut_boot_record(17,1,actual,expected,3);
        clear_trace(); s_tdc_rtt_input="unknown\n"; service_main();
        one(17,2,strstr(s_tdc_output,"invalid command.")!=NULL,1);
        /* 경계 fault는 파일 read만 실패시키고 NVM 초기화는 성공한다. */
        clear_trace(); s_tdc_read_error=1; s_tdc_rtt_input="dump\n"; service_main();
        one(17,3,strstr(s_tdc_output,"[ERROR] f_read")!=NULL,1);
        clear_trace(); s_tdc_open_error_name="/APP002.FEZ"; s_tdc_rtt_input="dump\n"; service_main();
        one(17,4,strstr(s_tdc_output,"[ERROR] f_open 'APP002.FEZ'")!=NULL,1);
        const unsigned lengths[6]={0,1,15,16,17,33};
        uint8_t payload[33];
        for(unsigned i=0;i<sizeof(payload);++i)payload[i]=(uint8_t)(i+0x80);
        for(unsigned i=0;i<6;++i)
        {
            clear_trace();
            for(unsigned j=0;j<4;++j)save_fixture(names[j],payload,lengths[i]);
            s_tdc_rtt_input="dump\r\n"; service_main();
            int matches=1;
            for(unsigned j=0;j<4;++j)matches &= service_file_matches(names[j]+1,payload,lengths[i]);
            const int actual[3]={matches,(int)s_tdc_open_handles,(int)s_tdc_reset_calls};
            const int expected[3]={1,0,0}; tdc_ut_boot_record(17,5+i,actual,expected,3);
        }
        char long_command[80]; memset(long_command,'x',70);
        memcpy(long_command+70,"\ndump\n",7);
        const char *commands[4]={"\r\n\n","dumb\bp\r\n",long_command,"\b\bdump\ndump\n"};
        const int dumps[4]={0,1,1,2}, invalid[4]={0,0,1,0};
        for(unsigned i=0;i<4;++i)
        {
            clear_trace(); s_tdc_rtt_input=commands[i]; service_main();
            const int actual[3]={(int)output_occurrences("========== FW DUMP BEGIN"),
                (int)output_occurrences("invalid command."),(int)s_tdc_open_handles};
            const int expected[3]={dumps[i],invalid[i],0};
            tdc_ut_boot_record(17,11+i,actual,expected,3);
        }
        /* 한 청크 출력 뒤 오류: 마커를 닫고 다음 파일을 계속 읽어야 한다. */
        clear_trace(); s_tdc_file_fail_call=2; s_tdc_rtt_input="dump\n"; service_main();
        const int partial[3]={service_file_matches("MANIFEST.TXT",payload,16),
            strstr(s_tdc_output,"[ERROR] f_read 'MANIFEST.TXT'")!=NULL &&
            service_file_matches("APP002.FEZ",payload,33),(int)s_tdc_open_handles};
        const int partial_expected[3]={1,1,0};
        tdc_ut_boot_record(17,15,partial,partial_expected,3);
        clear_trace(); s_tdc_rtt_input="dump\n"; service_main();
        const int recovered[3]={service_file_matches("MANIFEST.TXT",payload,33),
            strstr(s_tdc_output,"[ERROR]")==NULL,(int)s_tdc_open_handles};
        tdc_ut_boot_record(17,16,recovered,partial_expected,3);
    }
    else
    {
        unsigned gpio_before=s_tdc_gpio_writes;
        s_tdc_service_ticks=380; s_tdc_rtt_input="reboot\n"; service_main();
        const int actual[3]={(int)s_tdc_reset_calls,s_tdc_gpio_writes>gpio_before,(int)s_tdc_boot_calls};
        const int expected[3]={1,1,0}; tdc_ut_boot_record(18,1,actual,expected,3);
    }
    tdc_ut_boot_service_restore(saved_iter,saved_color);
    uint32_t restored_iter; uint8_t restored_color;
    tdc_ut_boot_service_snapshot(&restored_iter,&restored_color);
    if(restored_iter!=saved_iter || restored_color!=saved_color || s_tdc_open_handles)abort();
}
static void read_status(snd_boot_status_t *status)
{
    FIL fp=open_fixture(SND_BOOT_FILE_NAME); size_t got;
    if(f_read(&fp,status,sizeof(*status),&got)!=FR_OK||got!=sizeof(*status))abort();
    f_close(&fp);
}
static void run_slot(unsigned unit)
{
    snd_boot_status_t saved,status={0}; uint8_t saved_slot; FIL *saved_fp; FIL fp={0};
    tdc_ut_boot_slot_snapshot(&saved,&saved_slot,&saved_fp);
    snd_boot_set_fp(&fp);
    if(unit==19)
    {
        /* 없음은 파일명별 새 임시 루트에서 첫 실행한다. 4KB 파일을 다시 읽는다. */
        snd_boot_handle_file(); read_status(&status);
        const int actual[3]={tdc_boot_get_slot_num(),status.state,status.boot_slot_num};
        const int expected[3]={255,SND_BOOT_STATE_BOOT,255}; tdc_ut_boot_record(19,1,actual,expected,3);
        save_fixture(SND_BOOT_FILE_NAME,&status,3); snd_boot_handle_file(); read_status(&status);
        one(19,2,status.boot_slot_num,255);
        status.state=SND_BOOT_STATE_BOOT; status.boot_slot_num=1; status.crc32=0xdeadbeef;
        save_fixture(SND_BOOT_FILE_NAME,&status,sizeof(status)); snd_boot_handle_file();
        one(19,3,tdc_boot_get_slot_num(),1);
        read_status(&status); one(19,4,(int)status.crc32,(int)0xdeadbeefU);
    }
    else
    {
        const unsigned substates[6]={SND_BOOT_SUB_STATE_IDLE,SND_BOOT_SUB_STATE_BOOT_TRY,
            SND_BOOT_SUB_STATE_BOOT_TRY,SND_BOOT_SUB_STATE_BOOT_TRY_DONE,
            SND_BOOT_SUB_STATE_BOOT_TRY_DONE,99};
        const unsigned expected_slots[6]={1,2,1,2,1,1};
        const unsigned expected_counts[6]={0,3,4,0,0,0};
        const unsigned expected_substates[6]={SND_BOOT_SUB_STATE_IDLE,SND_BOOT_SUB_STATE_BOOT_TRY,
            SND_BOOT_SUB_STATE_BOOT_TRY_DONE,SND_BOOT_SUB_STATE_BOOT_TRY_DONE,
            SND_BOOT_SUB_STATE_UNKNOWN,SND_BOOT_SUB_STATE_UNKNOWN};
        for(unsigned i=0;i<6;++i)
        {
            memset(&status,0,sizeof(status)); status.state=i?SND_BOOT_STATE_ALT_BOOT:SND_BOOT_STATE_BOOT;
            status.sub_state=substates[i]; status.boot_slot_num=1; status.alt_boot_slot_num=2;
            status.alt_boot_try_count=i==1?2:i==2?3:0;
            status.alt_boot_result=i==3?SND_BOOT_ALT_BOOT_RESULT_SUCCESS:SND_BOOT_ALT_BOOT_RESULT_FAIL;
            save_fixture(SND_BOOT_FILE_NAME,&status,sizeof(status)); snd_boot_handle_file(); read_status(&status);
            const int actual[5]={tdc_boot_get_slot_num(),status.alt_boot_try_count,status.sub_state,status.state,status.alt_boot_result};
            const int expected[5]={(int)expected_slots[i],(int)expected_counts[i],(int)expected_substates[i],
                i==0||i==2?SND_BOOT_STATE_BOOT:SND_BOOT_STATE_ALT_BOOT,
                i==3?SND_BOOT_ALT_BOOT_RESULT_SUCCESS:SND_BOOT_ALT_BOOT_RESULT_FAIL};
            tdc_ut_boot_record(20,i+1,actual,expected,5);
        }
    }
    tdc_ut_boot_slot_restore(&saved,saved_slot,saved_fp);
}
int tdc_ut_boot_host_run_unit(unsigned unit)
{
    clear_trace();
    tdc_ut_boot_observer_t saved=tdc_ut_boot_observer_get();
    tdc_ut_boot_observer_set(observe);
    switch(unit)
    {
        case 2:run_delay();break;
        case 4:run_led();break;
        case 7:run_storage();break;
        case 8:run_boot_info();break;
        case 10:run_file();break;
        case 11:run_sections();break;
        case 12:run_manifest();break;
        case 13:run_crc();break;
        case 15:run_transfer();break;
        case 16:run_lock();break;
        case 17:case 18:run_service(unit);break;
        case 19:case 20:run_slot(unit);break;
        default:tdc_ut_boot_observer_set(saved);return TDC_UT_BOOT_UNSUPPORTED;
    }
    tdc_ut_boot_observer_set(saved);
    return 0;
}
#if defined(TDC_UNIT_TEST_RAM_ONLY)
/* 타깃 RAM 경로만 실행하는 호스트 QA. 기존 override/슬롯/scratch/CRC를
 * 서로 다른 값으로 채우고, 성공/오류 벡터 뒤 같은 상태가 남는지 확인한다.
 * sentinel 호출은 fixture 격리 누락이며 검사 자체를 중단한다. */
static unsigned s_tdc_ram_marker;
static FIL s_tdc_ram_previous_fp;
static snd_boot_status_t s_tdc_ram_previous_slot;
static uint32_t s_tdc_ram_previous_scratch[TDC_UT_BOOT_SCRATCH_WORDS];
static uint32_t s_tdc_ram_previous_key[4]={1,2,3,4};
extern uint32_t bootloader_error;
extern struct boot_section_data cfx_boot_data, cm3_boot_data;
extern uint8_t cfx_boot_flag, cm3_boot_flag;

static FRESULT sentinel_read(void *c,FIL *f,void *o,UINT n,UINT *r)
{ (void)c;(void)f;(void)o;(void)n;(void)r;abort(); }
static FRESULT sentinel_open(void *c,FIL *f,const char *p,unsigned m)
{ (void)c;(void)f;(void)p;(void)m;abort(); }
static FRESULT sentinel_close(void *c,FIL *f)
{ (void)c;(void)f;abort(); }
static FRESULT sentinel_stat(void *c,const char *p,FILINFO *i)
{ (void)c;(void)p;(void)i;abort(); }
static FRESULT sentinel_write(void *c,FIL *f,const void *i,UINT n,UINT *w)
{ (void)c;(void)f;(void)i;(void)n;(void)w;abort(); }
static FRESULT sentinel_seek(void *c,FIL *f,FSIZE_t o)
{ (void)c;(void)f;(void)o;abort(); }
static FRESULT sentinel_sync(void *c,FIL *f)
{ (void)c;(void)f;abort(); }
static FRESULT sentinel_chdir(void *c,const char *p)
{ (void)c;(void)p;abort(); }
static int sentinel_nvm(void *c,uint32_t o,uint32_t n,uint32_t *d)
{ (void)c;(void)o;(void)n;(void)d;abort(); }
static void sentinel_observer(unsigned e,const uint32_t *v,unsigned n)
{ (void)e;(void)v;(void)n;abort(); }

static void ram_state_prepare(void)
{
    tdc_ut_boot_file_ops_t ops={.open=sentinel_open,.close=sentinel_close,
        .stat=sentinel_stat,.write=sentinel_write,.seek=sentinel_seek,
        .sync=sentinel_sync,.chdir=sentinel_chdir,.context=&s_tdc_ram_marker};
    tdc_ut_boot_file_hook_t read={sentinel_read,&s_tdc_ram_marker};
    tdc_ut_boot_nvm_hook_t nvm={sentinel_nvm,&s_tdc_ram_marker};
    tdc_ut_boot_file_ops_set(&ops); tdc_ut_boot_file_hook_set(&read);
    tdc_ut_boot_nvm_hook_set(&nvm); tdc_ut_boot_set_public_key(s_tdc_ram_previous_key);
    tdc_ut_boot_observer_set(sentinel_observer);
    memset(&s_tdc_ram_previous_slot,0xa6,sizeof(s_tdc_ram_previous_slot));
    tdc_ut_boot_slot_restore(&s_tdc_ram_previous_slot,3,&s_tdc_ram_previous_fp);
    for(unsigned i=0;i<TDC_UT_BOOT_SCRATCH_WORDS;++i)
        s_tdc_ram_previous_scratch[i]=0xa5000000U+i;
    tdc_ut_boot_scratch_exchange(s_tdc_ram_previous_scratch);
    CRC->CFG=0x76543210; CRC->VALUE=0x1234;
    bootloader_error=0x43211234;
    cfx_boot_data=(struct boot_section_data){0x2345,0x6789};
    cm3_boot_data=(struct boot_section_data){0x3456,0x789a};
    cfx_boot_flag=0x67; cm3_boot_flag=0x89;
}
static void ram_state_verify(void)
{
    tdc_ut_boot_file_ops_t ops; tdc_ut_boot_file_hook_t read; tdc_ut_boot_nvm_hook_t nvm;
    snd_boot_status_t status; uint8_t slot; FIL *fp;
    tdc_ut_boot_file_ops_get(&ops); tdc_ut_boot_file_hook_get(&read); tdc_ut_boot_nvm_hook_get(&nvm);
    tdc_ut_boot_slot_snapshot(&status,&slot,&fp);
    if(ops.open!=sentinel_open || ops.close!=sentinel_close || ops.stat!=sentinel_stat ||
       ops.write!=sentinel_write || ops.seek!=sentinel_seek || ops.sync!=sentinel_sync ||
       ops.chdir!=sentinel_chdir || ops.context!=&s_tdc_ram_marker ||
       read.read!=sentinel_read || read.context!=&s_tdc_ram_marker ||
       nvm.read!=sentinel_nvm || nvm.context!=&s_tdc_ram_marker ||
       tdc_ut_boot_public_key()!=s_tdc_ram_previous_key ||
       tdc_ut_boot_observer_get()!=sentinel_observer || slot!=3 || fp!=&s_tdc_ram_previous_fp ||
       memcmp(&status,&s_tdc_ram_previous_slot,sizeof(status)) ||
       CRC->CFG!=0x76543210 || CRC->VALUE!=0x1234 || bootloader_error!=0x43211234 ||
       cfx_boot_data.stack_pointer!=0x2345 || cfx_boot_data.init_pointer!=0x6789 ||
       cm3_boot_data.stack_pointer!=0x3456 || cm3_boot_data.init_pointer!=0x789a ||
       cfx_boot_flag!=0x67 || cm3_boot_flag!=0x89 || s_tdc_nvm_reads || s_tdc_boot_calls ||
       s_tdc_reset_calls || s_tdc_app_opens || s_tdc_transfer_count || s_tdc_gpio_writes)
        abort();
    if(s_tdc_delay_calls || s_tdc_trim_calls || s_tdc_timer_calls || s_tdc_dio_configs ||
       s_tdc_nvm_read_calls || s_tdc_reinit_calls || s_tdc_file_read_calls || s_tdc_open_handles)
        abort();
    uint32_t scratch[TDC_UT_BOOT_SCRATCH_WORDS]={0};
    tdc_ut_boot_scratch_exchange(scratch);
    for(unsigned i=0;i<TDC_UT_BOOT_SCRATCH_WORDS;++i)
        if(scratch[i]!=0xa5000000U+i)abort();
    tdc_ut_boot_scratch_exchange(scratch);
}
#endif

int main(int argc,char **argv)
{
    if(argc!=3||strlen(argv[1])>=sizeof(s_tdc_root))return 2;
    strcpy(s_tdc_root,argv[1]);
    const uint32_t key[4]={0x2b7e1516,0x28aed2a6,0xabf71588,0x09cf4f3c};
    memcpy(g_tdc_boot_key,key,sizeof(key));
    unsigned unit=(unsigned)strtoul(argv[2],NULL,10);
#if defined(TDC_UNIT_TEST_RAM_ONLY)
    ram_state_prepare();
#endif
    int ret=tdc_ut_boot_run_unit(unit);
#if defined(TDC_UNIT_TEST_RAM_ONLY)
    ram_state_verify();
#endif
    printf("STATUS %u %d %u %d %d\n",unit,ret,g_tdc_ut_boot_count,g_tdc_ut_boot_done,g_tdc_ut_boot_overflow);
    for(unsigned pos=0;pos<g_tdc_ut_boot_count;)
    {
        unsigned length=(unsigned)g_tdc_ut_boot_actual[pos+3];
        printf("ACTUAL");for(unsigned i=0;i<length+4;++i)printf(" %d",g_tdc_ut_boot_actual[pos+i]);puts("");
        printf("EXPECTED");for(unsigned i=0;i<length+4;++i)printf(" %d",g_tdc_ut_boot_expected[pos+i]);puts("");
        pos+=length+4;
    }
    if(ret==0)
    {
        /* 저장소 drain 전 재실행 거부를 실제로 확인한다. */
        int actual[TDC_UT_BOOT_CAPACITY], expected[TDC_UT_BOOT_CAPACITY];
        unsigned count=g_tdc_ut_boot_count;
        memcpy(actual,g_tdc_ut_boot_actual,sizeof(actual));
        memcpy(expected,g_tdc_ut_boot_expected,sizeof(expected));
        if(tdc_ut_boot_run_unit(unit)!=TDC_UT_BOOT_BUSY || g_tdc_ut_boot_count!=count ||
            memcmp(actual,g_tdc_ut_boot_actual,sizeof(actual)) ||
            memcmp(expected,g_tdc_ut_boot_expected,sizeof(expected)))return 3;
#if !defined(TDC_UNIT_TEST_RAM_ONLY)
        tdc_ut_boot_file_hook_t hook;tdc_ut_boot_file_hook_get(&hook);
        tdc_ut_boot_file_ops_t ops;tdc_ut_boot_file_ops_get(&ops);
        tdc_ut_boot_nvm_hook_t nvm;tdc_ut_boot_nvm_hook_get(&nvm);
        if(hook.read||hook.context||tdc_ut_boot_observer_get()||tdc_ut_boot_public_key()||
           nvm.read||nvm.context||ops.open||ops.close||ops.stat||ops.write||ops.seek||
           ops.sync||ops.chdir||ops.context)return 4;
#endif
        tdc_ut_boot_release();
#if defined(TDC_UNIT_TEST_RAM_ONLY)
        if(tdc_ut_boot_run_unit(unit)!=0 || g_tdc_ut_boot_count!=count ||
           memcmp(actual,g_tdc_ut_boot_actual,count*sizeof(int)) ||
           memcmp(expected,g_tdc_ut_boot_expected,count*sizeof(int)))return 5;
        ram_state_verify(); tdc_ut_boot_release();
#endif
    }
#if defined(TDC_UNIT_TEST_RAM_ONLY)
    tdc_ut_boot_scratch_exchange(s_tdc_ram_previous_scratch);
    tdc_ut_boot_file_hook_set(NULL); tdc_ut_boot_file_ops_set(NULL);
    tdc_ut_boot_nvm_hook_set(NULL); tdc_ut_boot_observer_set(NULL); tdc_ut_boot_set_public_key(NULL);
#endif
    return ret==TDC_UT_BOOT_UNSUPPORTED?0:ret!=0;
}
