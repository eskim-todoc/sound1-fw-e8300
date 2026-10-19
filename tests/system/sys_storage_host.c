/* 실제 파일 래퍼/AES와 시험 명령을 실행한다. FatFs/MMIO만 PC RAM으로 대체한다. */
#include <assert.h>
#include <stdarg.h>
#include "tdc_sys_test_storage.h"
#include "tdc_sys_test_77.h"
#include "tdc_ui_command.h"
#include "tdc_ut_coordination_cm3.h"
#include "ci_filesystem.h"
#include "ci_map.h"
#include "ci_stim_mute.h"
#include "ci_event_log.h"
#include "error.h"
#include "LedOutput.h"

static unsigned s_checks, s_max_line, s_writes, s_reads, s_removes;
static unsigned s_opens, s_closes, s_syncs, s_stats, s_mounts, s_rtt_lines;
static unsigned s_close_error_at, s_sync_error_at, s_open_error_at;
static unsigned s_direct_calls, s_direct_bytes, s_max_command_lines;
static FIL *s_open_files[3];
static bool s_locked, s_ready = true;
static const char *s_input;
static unsigned s_input_len, s_input_pos;
static char s_log[32768], s_ui[32768];
static unsigned s_log_len, s_ui_len;
static unsigned char s_storage[4096], s_image[4096];
static unsigned s_storage_size, s_image_size;
static bool s_exists, s_write_error, s_read_short;
static bool s_read_error, s_remove_error;
#define CHECK(x) do { ++s_checks; assert(x); } while (0)

void OS_SIM_EnterCriticalSection(void) { CHECK(!s_locked); s_locked = true; }
void OS_SIM_LeaveCriticalSection(void) { CHECK(s_locked); s_locked = false; }
unsigned SEGGER_RTT_Read(unsigned index, void *buffer, unsigned size)
{
    CHECK(index == 0 && size == 1);
    if (s_input_pos == s_input_len) return 0;
    *(char *)buffer = s_input[s_input_pos++]; return 1;
}
unsigned SEGGER_RTT_Write(unsigned index, const void *buffer, unsigned size)
{
    CHECK(index == 0 && s_ui_len + size < sizeof(s_ui));
    memcpy(s_ui + s_ui_len, buffer, size); s_ui_len += size; s_ui[s_ui_len] = 0;
    return size;
}
unsigned SEGGER_RTT_WriteSkipNoLock(unsigned index, const void *buffer, unsigned size)
{
    const char *text = buffer;
    CHECK(index == 0 && s_locked && size <= 60 && size >= 2);
    CHECK(text[size-2] == '\r' && text[size-1] == '\n');
    if (!s_ready) return 0;
    CHECK(s_log_len + size < sizeof(s_log));
    memcpy(s_log + s_log_len, text, size); s_log_len += size; s_log[s_log_len] = 0;
    if (size > s_max_line) s_max_line = size;
    ++s_rtt_lines;
    return 1;
}
int SEGGER_RTT_printf(unsigned index, const char *format, ...)
{
    /* 제품 직접 진단도 관찰한다. HOLD에서는 한 번이라도 새면 실패한다.
     * 평상시 진단은 정상 동작 보존 확인에만 쓰며 내용/주소를 보고서에 출력하지 않는다. */
    va_list args;
    va_start(args, format);
    const int size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    CHECK(index == 0 && size >= 0 && !tdc_sys_storage_is_active());
    ++s_direct_calls;
    s_direct_bytes += (unsigned)size;
    return size;
}
int ci_timer_get_tick(void) { return 10000; }
int tdc_timer_get_t3_tick(void) { return 10000; }
void fn_write_userSettingParameters(int isd) { (void)isd; abort(); }
void fn_copy_MappingData_toCM3(int map, int isd) { (void)map; (void)isd; abort(); }
int ci_fft_read_pass_bin(int n) { (void)n; abort(); }
int ci_fft_init_pass_bin(int n) { (void)n; abort(); }
int ci_map_init_map_data(int isd, bool force, bool side, int value)
{ (void)isd; (void)force; (void)side; (void)value; abort(); }
CI_TIMER_TIME_T ci_timer_get_reference_time_after_self_update(void) { abort(); }
int ci_stim_mute_update(uint32_t enable, uint32_t level) { (void)enable; (void)level; abort(); }

static uint16_t crc_next(uint16_t crc, uint8_t value)
{
    crc ^= (uint16_t)((uint16_t)value << 8);
    for (unsigned i = 0; i < 8; ++i) crc = (uint16_t)((crc & 0x8000u) ? (crc << 1) ^ 0x1021u : crc << 1);
    return crc;
}
uint16_t ci_crc_ccitt_calc(void *buffer, uint32_t size)
{
    uint16_t crc = 0xffffu;
    for (unsigned i = 0; i < size; ++i) crc = crc_next(crc, ((uint8_t *)buffer)[i]);
    return crc;
}
static bool image(FIL *fp) { return fp->obj.sclust == 2; }
static void opened(FIL *fp)
{
    for (unsigned i=0; i<3; ++i) CHECK(s_open_files[i] != fp);
    for (unsigned i=0; i<3; ++i)
        if (s_open_files[i] == NULL) { s_open_files[i]=fp; return; }
    CHECK(false);
}
static void closed(FIL *fp)
{
    for (unsigned i=0; i<3; ++i)
        if (s_open_files[i] == fp) { s_open_files[i]=NULL; return; }
    CHECK(false);
}
FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    ++s_opens;
    if (s_opens == s_open_error_at) return FR_DISK_ERR;
    memset(fp, 0, sizeof(*fp));
    if (path[0] == '0')
    {
        CHECK(strcmp(path, "0:/1/APP000.FEZ") == 0 && mode == (FA_READ | FA_OPEN_EXISTING));
        fp->obj.sclust = 2; fp->obj.objsize = s_image_size;
    }
    else
    {
        CHECK(strcmp(path, "1:/SYS77.TST") == 0);
        if ((mode & FA_CREATE_NEW) && s_exists) return FR_EXIST;
        if (!s_exists && !(mode & (FA_CREATE_NEW | FA_OPEN_ALWAYS))) return FR_NO_FILE;
        s_exists = true; fp->obj.sclust = 1; fp->obj.objsize = s_storage_size;
    }
    opened(fp);
    return FR_OK;
}
FRESULT f_close(FIL *fp)
{
    ++s_closes;
    if (s_closes == s_close_error_at) return FR_DISK_ERR;
    closed(fp);
    return FR_OK;
}
FRESULT f_read(FIL *fp, void *buffer, UINT request, UINT *got)
{
    ++s_reads;
    if (s_read_error && fp == &g_ci_filesystem_ohdl) { *got=0; return FR_DISK_ERR; }
    unsigned size = image(fp) ? s_image_size : s_storage_size;
    CHECK(fp->fptr <= size);
    unsigned n = size - fp->fptr; if (n > request) n = request;
    if (s_read_short && n) { --n; s_read_short = false; }
    memcpy(buffer, (image(fp) ? s_image : s_storage) + fp->fptr, n);
    fp->fptr += n; *got = n; return FR_OK;
}
FRESULT f_write(FIL *fp, const void *buffer, UINT count, UINT *wrote)
{
    CHECK(!image(fp)); ++s_writes; *wrote = 0;
    if (s_write_error) return FR_DISK_ERR;
    CHECK(fp->fptr + count <= sizeof(s_storage));
    memcpy(s_storage + fp->fptr, buffer, count); fp->fptr += count;
    if (s_storage_size < fp->fptr) s_storage_size = fp->fptr;
    fp->obj.objsize = s_storage_size; *wrote = count; return FR_OK;
}
FRESULT f_lseek(FIL *fp, FSIZE_t offset)
{
    if (image(fp)) { CHECK(offset <= s_image_size); fp->fptr=offset; return FR_OK; }
    CHECK(offset <= sizeof(s_storage));
    if (offset > s_storage_size) { memset(s_storage+s_storage_size, 0, offset-s_storage_size); s_storage_size = offset; }
    fp->obj.objsize = s_storage_size; fp->fptr = offset; return FR_OK;
}
FRESULT f_sync(FIL *fp)
{
    CHECK(!image(fp));
    ++s_syncs;
    return s_syncs == s_sync_error_at ? FR_DISK_ERR : FR_OK;
}
FRESULT f_unlink(const TCHAR *path)
{
    CHECK(strcmp(path, "1:/SYS77.TST") == 0);
    for (unsigned i=0; i<3; ++i) CHECK(s_open_files[i] == NULL);
    ++s_removes;
    if (s_remove_error) return FR_DISK_ERR;
    s_exists = false;
    return FR_OK;
}
FRESULT f_stat(const TCHAR *path, FILINFO *info)
{
    ++s_stats;
    CHECK(strcmp(path, "1:/SYS77.TST") == 0); memset(info,0,sizeof(*info)); info->fsize = s_storage_size; return FR_OK;
}
FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE opt)
{ (void)fs; (void)opt; ++s_mounts; CHECK(strcmp(path,"0:") == 0); return FR_OK; }

static void put16(uint8_t *p, unsigned value) { p[0] = value; p[1] = value >> 8; }
static void put32(uint8_t *p, unsigned value) { put16(p,value); put16(p+2,value>>16); }
static void append_crc(uint8_t *p, unsigned count)
{ unsigned c = ci_crc_ccitt_calc(p, count); p[count] = c>>8; p[count+1]=c; }
static void make_image(bool volatile_section, unsigned sections)
{
    memset(s_image, 0, sizeof(s_image));
    put32(s_image, 0x657a6169); put16(s_image+4, sections);
    uint8_t *head = s_image+12; put32(head+4, 4); put16(head+8, 4); put16(head+10, volatile_section ? 0x40 : 0);
    append_crc(head,14);
    uint8_t *data = head+16;
    for (unsigned i=0; i<16; ++i) data[i]=(uint8_t)i;
    unsigned start = ci_crc_ccitt_calc(data,16), code;
    for (code=0; code<65536; ++code)
    {
        uint16_t c=crc_next(start,(uint8_t)code); c=crc_next(c,(uint8_t)(code>>8));
        c=crc_next(c,0); c=crc_next(c,0); if(c==0) break;
    }
    CHECK(code < 65536); put16(s_image+6,code); append_crc(s_image,10); s_image_size=44;
}
static void drain(void)
{
    for (unsigned i=0; i<100 && tdc_sys_storage_pending(); ++i) tdc_ui_command_poll();
    CHECK(!tdc_sys_storage_pending());
}
static void feed(const char *s, unsigned n)
{
    s_input=s; s_input_len=n; s_input_pos=0;
    for (unsigned i=0; i<10 && s_input_pos<n; ++i) tdc_ui_command_poll();
    CHECK(s_input_pos==n);
}
static void input(const char *s)
{
    const unsigned start=s_rtt_lines;
    feed(s,(unsigned)strlen(s));
    if(s_ready) drain();
    const unsigned lines=s_rtt_lines-start;
    CHECK(lines <= 64);
    if(lines > s_max_command_lines) s_max_command_lines=lines;
}
static void has(const char *s) { CHECK(strstr(s_log,s)!=NULL); }
static void clear_log(void) { s_log_len=s_ui_len=0; s_log[0]=s_ui[0]=0; }
static unsigned fs_calls(void)
{ return s_opens+s_closes+s_syncs+s_reads+s_writes+s_removes+s_stats+s_mounts; }

static void check_latched(void)
{
    CHECK(tdc_sys_test_77_requires_reboot());
    CHECK(s_exists && strstr(s_log,"FILE_ROUNDTRIP=MATCH")==NULL);
    has("SCRATCH=RETAINED; NO_MORE_FS");
    has("FILE_ROUNDTRIP=NOT_COMPLETE");
    const unsigned calls=fs_calls(), direct=s_direct_calls;
    input("--sys77 run\n");
    input("--sys54 check 1 0\n");
    input("--sys73 write open\n");
    char *next[] = {"sys80", "check"};
    CHECK(tdc_sys_storage_command(80,2,next,false)==0);
    drain();
    char *own[] = {"sys77", "run"};
    CHECK(tdc_sys_test_77_execute(2,own)==0);
    drain();
    has("REJECT=FS_UNCERTAIN; REBOOT_REQUIRED");
    input("--sys77 status\n"); has("HOLD=1");
    input("--sys77 stop\n"); has("no automatic restore");
    input("--sys77 run\n");
    CHECK(fs_calls()==calls && s_direct_calls==direct && tdc_sys_storage_is_active());
}

int main(int argc, char **argv)
{
    CHECK(argc==2); const char *test=argv[1];
    CHECK(ci_crc_ccitt_calc("123456789",9)==0x29b1u);
    ci_aes_init(); tdc_ui_command_init(); clear_log();
    if (strcmp(test,"normal")==0 || strcmp(test,"existing")==0 || strcmp(test,"write_error")==0 || strcmp(test,"read_short")==0)
    {
        s_exists=strcmp(test,"existing")==0; s_write_error=strcmp(test,"write_error")==0;
        s_read_short=strcmp(test,"read_short")==0;
        input("--sys77 run\n");
        if(strcmp(test,"existing")==0) { has("existing file"); CHECK(s_writes==0 && s_removes==0); }
        else if(s_write_error) { CHECK(s_removes==0); has("WR_SYNC=NOT_RUN"); check_latched(); }
        else if(strcmp(test,"read_short")==0) { CHECK(s_removes==0 && s_opens==3); check_latched(); }
        else { has("FILE_ROUNDTRIP=MATCH"); has("CIPHER_CHANGED=1"); has("SAME=1"); CHECK(s_removes==1); }
    }
    else if(strcmp(test,"create_sync")==0 || strcmp(test,"create_close")==0 ||
            strcmp(test,"write_sync")==0 || strcmp(test,"write_close")==0 ||
            strcmp(test,"raw_open")==0 || strcmp(test,"raw_close")==0 ||
            strcmp(test,"read_close")==0 || strcmp(test,"read_error")==0 || strcmp(test,"remove_error")==0)
    {
        if(strcmp(test,"create_sync")==0) s_sync_error_at=1;
        if(strcmp(test,"create_close")==0) s_close_error_at=1;
        if(strcmp(test,"write_sync")==0) s_sync_error_at=2;
        if(strcmp(test,"write_close")==0) s_close_error_at=2;
        if(strcmp(test,"raw_open")==0) s_open_error_at=3;
        if(strcmp(test,"raw_close")==0) s_close_error_at=3;
        if(strcmp(test,"read_close")==0) s_close_error_at=4;
        s_read_error=strcmp(test,"read_error")==0;
        s_remove_error=strcmp(test,"remove_error")==0;
        input("--sys77 run\n");
        if(strcmp(test,"create_sync")==0) { has("CREATE_SYNC=1"); CHECK(s_opens==1 && s_closes==0); }
        if(strcmp(test,"create_close")==0) { has("CLOSE=1"); CHECK(s_opens==1 && s_closes==1); }
        if(strcmp(test,"write_sync")==0) { has("WRITE=0"); has("WR_SYNC N=1 RES=1"); CHECK(s_opens==2); }
        if(strcmp(test,"write_close")==0) { has("WRITE=0"); has("WR_CLOSE N=1 RES=1"); CHECK(s_opens==2); }
        if(strcmp(test,"raw_open")==0) { has("RAW_OPEN=1"); CHECK(s_opens==3 && s_reads==0); }
        if(strcmp(test,"raw_close")==0) { has("BYTES=144 CLOSE=1"); CHECK(s_opens==3 && s_reads==1); }
        if(strcmp(test,"read_close")==0) { has("READ_CRC=0"); has("RD_CLOSE N=1 RES=1"); }
        if(s_read_error) has("READ_CRC=-1");
        CHECK(s_removes==(s_remove_error ? 1u : 0u));
        check_latched();
    }
    else if(strcmp(test,"diagnostics")==0)
    {
        uint8_t data[16]={0}; uint32_t crc=0, padding[3]={0};
        const unsigned before=s_direct_calls;
        CHECK(ci_filesystem_read_with_crc_and_aes128("1:/SYS77.TST",data,16,&crc,padding,12,true,true)==-1);
        CHECK(s_direct_calls>before && s_direct_bytes>0);
        const unsigned observed=s_direct_calls;
        input("--sys77 run\n");
        has("FILE_ROUNDTRIP=MATCH");
        CHECK(s_direct_calls==observed);
    }
    else if(strcmp(test,"queue_limit")==0)
    {
        input("--sys77 status\n"); clear_log();
        char body[50]; memset(body,'Q',48); body[48]=0;
        tdc_sys_storage_log("%s",body); drain(); CHECK(s_max_line==60);
        body[48]='Q'; body[49]=0;
        tdc_sys_storage_log("%s",body); drain(); has("LOG_FORMAT_ERROR");
        clear_log(); s_ready=false;
        const unsigned before=s_rtt_lines;
        for(unsigned i=0;i<65;++i) tdc_sys_storage_log("QUEUE_ROW=%u",i);
        CHECK(tdc_sys_storage_pending());
        s_ready=true; drain();
        CHECK(s_rtt_lines-before==65);
        has("QUEUE_ROW=63\r\n"); CHECK(strstr(s_log,"QUEUE_ROW=64\r\n")==NULL);
        has("LOG_OVERFLOW; results incomplete"); CHECK(fs_calls()==0);
    }
    else if(strncmp(test,"audit_",6)==0)
    {
        if(strcmp(test,"audit_open")==0) input("--sys73 write open\n");
        else if(strcmp(test,"audit_init")==0) input("--sys73 init open\n");
        else input("--sys73 led\n");
        CHECK(s_writes==0 && s_reads==0 && s_removes==0);
        if(strcmp(test,"audit_init")==0)
        {
            has("CHECK=GAP"); has("OBS=NO_PRODUCT_ERROR");
            CHECK(readErrorCode().data_logging_error==0 && led_get_request(LED_SRC_ERROR)==LED_ST_NONE);
        }
        else
        {
            has("ERROR=10/1");
            CHECK(readErrorCode().data_logging_error==1 && led_get_request(LED_SRC_ERROR)==LED_ST_ERROR_MCU);
            if(strcmp(test,"audit_open")==0) has("OBS=PRODUCT_ERROR");
            else has("OBS=LED_DEMO_ONLY");
            clear_log(); input("--sys77 run\n"); has("existing error"); CHECK(s_writes==0);
        }
        CHECK(tdc_sys_storage_is_active());
    }
    else if(strcmp(test,"guards")==0)
    {
        tdc_ut_cm3_isd_state_t state={0}; state.state.conneded_ISD=true; tdc_ut_cm3_isd_state_exchange(&state);
        input("--sys77 run\n"); has("disconnect ISD"); CHECK(!tdc_sys_storage_is_active() && s_writes==0);
        state.state.conneded_ISD=false; tdc_ut_cm3_isd_state_exchange(&state); clear_log();
        tdc_ui_command_set_mapping_connected(true); input("--sys77 run\n"); has("disconnect ISD"); CHECK(s_writes==0);
        tdc_ui_command_set_mapping_connected(false); clear_log(); led_request(LED_SRC_POWER,LED_ST_POWER_ON);
        input("--sys77 run\n"); has("power pattern"); CHECK(s_writes==0);
    }
    else if(strcmp(test,"backpressure")==0)
    {
        s_ready=false; input("--sys77 run\n"); CHECK(tdc_sys_storage_pending() && s_log_len==0);
        input("--sys54 check 1 0\n"); CHECK(strstr(s_ui,"BUSY"));
        input("--init_all_map\n"); CHECK(strstr(s_ui,"HOLD"));
        s_ready=true; drain(); has("FILE_ROUNDTRIP=MATCH");
        clear_log(); input("--sys77 stop\n"); has("no automatic restore"); CHECK(tdc_sys_storage_is_active());
    }
    else if(strcmp(test,"invalid")==0)
    {
        char longline[220]; memset(longline,' ',sizeof(longline)); memcpy(longline,"--sys77 run",11); longline[219]='\n';
        feed(longline,sizeof(longline)); const char nul[]="--sys77 run\0bad\n"; feed(nul,sizeof(nul)-1);
        input("--sys54 check 1 9\n"); has("INPUT ERROR"); CHECK(s_writes==0 && s_reads==0);
    }
    else
    {
        make_image(strcmp(test,"volatile")==0, strcmp(test,"missing_section")==0 ? 2:1);
        if(strcmp(test,"truncated")==0) s_image_size=7;
        if(strcmp(test,"encrypted")==0) put32(s_image,0x455a4149);
        if(strcmp(test,"volatile")==0) s_image[28]^=1;
        uint8_t before[44]; memcpy(before,s_image,sizeof(before));
        if(strcmp(test,"header")==0) input("--sys54 header 1 0\n");
        else if(strcmp(test,"data")==0) input("--sys54 data 1 0\n");
        else input("--sys54 check 1 0\n");
        CHECK(memcmp(before,s_image,sizeof(before))==0 && s_writes==0 && s_removes==0);
        if(strcmp(test,"encrypted")==0) has("NOT_CHECKED");
        else if(strcmp(test,"image")==0) has("CRC_CHECK=MATCH");
        else if(strcmp(test,"volatile")==0) { has("VOLATILE_CRC_BAD"); has("CRC_CHECK=PARTIAL"); }
        else has("CRC_CHECK=REJECT");
        has("BOOT=NOT_RUN");
    }
    CHECK(strstr(s_log,"LOG_FORMAT_ERROR")==NULL);
    if(strcmp(test,"queue_limit")!=0) CHECK(strstr(s_log,"LOG_OVERFLOW")==NULL);
    puts(s_log);
    printf("HOST_EVIDENCE crc=29B1 direct_calls=%u direct_bytes=%u\n",s_direct_calls,s_direct_bytes);
    printf("HOST_PASS %s checks=%u max_line=%u max_command_lines=%u\n",test,s_checks,s_max_line,s_max_command_lines);
    return 0;
}
