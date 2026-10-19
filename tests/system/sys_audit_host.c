/* 실제 UI 파서와 감사기록 표시 모듈의 PC 시험. 장치/NVM은 실행하지 않는다. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tdc_sys_test_audit.h"
#include "tdc_sys_test_7.h"
#include "tdc_sys_test_23.h"
#include "tdc_ui_command.h"
#include "ci_map.h"
#include "ci_event_log.h"
#include "ci_stim_mute.h"

static bool s_ready = true, s_locked, s_sys7, s_sys23;
static const char *s_input;
static size_t s_input_size, s_input_pos;
static char s_log[16384], s_ui[32768];
static size_t s_log_size, s_ui_size;
static unsigned s_checks, s_max_line, s_lines;
#define CHECK(value) do { ++s_checks; assert(value); } while (0)

void OS_SIM_EnterCriticalSection(void) { CHECK(!s_locked); s_locked = true; }
void OS_SIM_LeaveCriticalSection(void) { CHECK(s_locked); s_locked = false; }
unsigned SEGGER_RTT_Read(unsigned index, void *buffer, unsigned length)
{
    CHECK(index == 0 && length == 1);
    if (s_input_pos == s_input_size) return 0;
    *(char *)buffer = s_input[s_input_pos++];
    return 1;
}
unsigned SEGGER_RTT_Write(unsigned index, const void *buffer, unsigned length)
{
    CHECK(index == 0 && s_ui_size + length < sizeof(s_ui));
    if (length > 10 && memcmp(buffer, "[SYS-05", 7) == 0) CHECK(length <= 60);
    memcpy(s_ui + s_ui_size, buffer, length);
    s_ui_size += length; s_ui[s_ui_size] = 0;
    return length;
}
unsigned SEGGER_RTT_WriteSkipNoLock(unsigned index, const void *buffer, unsigned length)
{
    const char *line = buffer;
    CHECK(index == 0 && s_locked && length >= 2 && length <= 60);
    CHECK(line[length - 2] == '\r' && line[length - 1] == '\n');
    CHECK(strncmp(line, "[SYS-050][TEST] ", 16) == 0 || strncmp(line, "[SYS-051][TEST] ", 16) == 0);
    if (!s_ready) return 0;
    CHECK(s_log_size + length < sizeof(s_log));
    memcpy(s_log + s_log_size, buffer, length);
    s_log_size += length; s_log[s_log_size] = 0; ++s_lines;
    if (length > s_max_line) s_max_line = length;
    return 1;
}
int ci_timer_get_tick(void) { return 10000; }
int SEGGER_RTT_printf(unsigned index, const char *format, ...)
{ (void)index; (void)format; return 0; }
/* 호출되면 실패: 시험 표시가 실제 저장/시각/연결/맵을 수정하면 안 된다. */
void fn_write_userSettingParameters(int isd) { (void)isd; abort(); }
void fn_copy_MappingData_toCM3(int map, int isd) { (void)map; (void)isd; abort(); }
int ci_fft_read_pass_bin(int bands) { (void)bands; abort(); }
int ci_fft_init_pass_bin(int bands) { (void)bands; abort(); }
int ci_map_init_map_data(int isd, bool force, bool side, int value)
{ (void)isd; (void)force; (void)side; (void)value; abort(); }
int ci_event_log_read(void) { abort(); }
int ci_event_log_write(uint32_t event) { (void)event; abort(); }
void ci_event_log_update_bt_addr(CI_EVENT_LOG_BT_ADDR_T *address) { (void)address; abort(); }
int ci_stim_mute_update(uint32_t enable, uint32_t level)
{ (void)enable; (void)level; abort(); }

#if TDC_SYS_TEST_7_ENABLE
bool tdc_sys_test_7_is_active(void) { return s_sys7; }
bool tdc_sys_test_7_log_pending(void) { return false; }
int tdc_sys_test_7_command(int argc, char *argv[], bool mapping)
{ (void)argc; (void)argv; (void)mapping; abort(); }
void tdc_sys_test_7_poll(void) {}
#endif
#if TDC_SYS_TEST_23_ENABLE
bool tdc_sys_test_23_is_active(void) { return s_sys23; }
bool tdc_sys_test_23_log_pending(void) { return false; }
int tdc_sys_test_23_command(int argc, char *argv[], bool mapping)
{ (void)argc; (void)argv; (void)mapping; abort(); }
void tdc_sys_test_23_poll(void) {}
#endif

static void drain(void)
{
    for (int i = 0; i < 16 && tdc_sys_test_audit_pending(); ++i) tdc_ui_command_poll();
    CHECK(!tdc_sys_test_audit_pending());
}
static void feed(const char *data, size_t count)
{
    s_input = data; s_input_size = count; s_input_pos = 0;
    for (int i = 0; i < 8 && s_input_pos < s_input_size; ++i) tdc_ui_command_poll();
    CHECK(s_input_pos == s_input_size);
}
static void input(const char *data) { feed(data, strlen(data)); }
static void clear_output(void)
{
    s_log_size = s_ui_size = s_lines = 0;
    s_log[0] = s_ui[0] = 0;
}

int main(void)
{
    tdc_ui_command_init(); clear_output();
    input("--sys50\r\n"); drain();
    CHECK(s_lines == 8);
    CHECK(strstr(s_log, "<0> TIME {26-05-29-10-00-00}") != NULL);
    CHECK(strstr(s_log, "<1> TIME {26-05-29-10-00-05}") != NULL);
    CHECK(strstr(s_log, "<0> ADDR {02:24:A0:00:00:50}") != NULL);
    CHECK(strstr(s_log, "<1> ADDR {02:24:A0:00:00:50}") != NULL);
    const char *connected = strstr(s_log, "EVENT {00000040} CONNECTED");
    const char *disconnected = strstr(s_log, "EVENT {00000032} DISCONNECTED");
    CHECK(connected != NULL && disconnected != NULL && connected < disconnected);
    CHECK(strstr(s_log, "PASS") == NULL && strstr(s_log, "FORMAT_ERROR") == NULL);
    puts(s_log);
    char first[2048]; CHECK(strlen(s_log) < sizeof(first)); strcpy(first, s_log);
    clear_output(); input("--sys50 show\n"); drain(); CHECK(strcmp(first, s_log) == 0);

    clear_output(); input("--sys51\n"); drain();
    CHECK(s_lines == 5);
    CHECK(strstr(s_log, "<0> TIME {26-05-29-10-01-00}") != NULL);
    CHECK(strstr(s_log, "ADDR {00:00:00:00:00:00}") != NULL);
    CHECK(strstr(s_log, "EVENT {000000F0} INTEGRITY_ERROR") != NULL);
    CHECK(strstr(s_log, "FORMAT_ERROR") == NULL && strstr(s_log, "PASS") == NULL);
    puts(s_log);

    /* 용량이 없으면 부분 행/중복 출력을 만들지 않고 같은 행을 재시도한다. */
    clear_output(); s_ready = false; input("--sys50\n");
    for (int i = 0; i < 30; ++i) tdc_ui_command_poll();
    CHECK(s_lines == 0 && tdc_sys_test_audit_pending());
    input("--sys51\n"); CHECK(strstr(s_ui, "BUSY") != NULL);
    input("--init_all_map\n"); CHECK(strstr(s_ui, "Wait for output") != NULL);
    s_ready = true; drain(); CHECK(strcmp(first, s_log) == 0);

    clear_output(); s_ready = false; input("--sys51\n"); input("--sys51 stop\n");
    s_ready = true; drain(); CHECK(s_lines == 1 && strstr(s_log, "STOPPED") != NULL);
    CHECK(strstr(s_log, "EVENT") == NULL);
    clear_output(); input("--sys50\n"); input("--sys50 stop\n"); drain();
    CHECK(strstr(s_log, "STOPPED") != NULL && strstr(s_log, "EVENT") == NULL);

    clear_output();
    const char *bad[] = {"--sys50 unknown\n", "--sys51 show extra\n", "--sys50 stop extra\n"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    { input(bad[i]); CHECK(!tdc_sys_test_audit_pending()); }
    char long_line[220]; memset(long_line, ' ', sizeof(long_line));
    memcpy(long_line, "--sys50", 7); long_line[219] = '\n'; feed(long_line, sizeof(long_line));
    const char nul[] = "--sys51\0bad\n"; feed(nul, sizeof(nul) - 1);
    CHECK(s_lines == 0 && !tdc_sys_test_audit_pending());
    char *args[] = {"sys50", NULL};
    CHECK(tdc_sys_test_audit_command(52, 1, args) == -1);
    CHECK(tdc_sys_test_audit_command(51, 1, args) == -1);
    CHECK(tdc_sys_test_audit_command(50, 0, args) == -1);
    CHECK(tdc_sys_test_audit_command(50, 2, args) == -1);
    CHECK(tdc_sys_test_audit_command(50, 1, NULL) == -1);
    input("--sys50 shoX\bw\n"); drain(); CHECK(s_lines == 8);
#if TDC_SYS_TEST_7_ENABLE
    clear_output(); s_sys7 = true; input("--sys50\n");
    CHECK(s_lines == 0 && strstr(s_ui, "HOLD") != NULL); s_sys7 = false;
#endif
#if TDC_SYS_TEST_23_ENABLE
    clear_output(); s_sys23 = true; input("--sys51\n");
    CHECK(s_lines == 0 && strstr(s_ui, "ACTIVE") != NULL); s_sys23 = false;
#endif
    CHECK(s_max_line <= 60);
    printf("HOST_PASS assertions=%u max_line_bytes=%u; display only, no device execution\n", s_checks, s_max_line);
    return 0;
}
