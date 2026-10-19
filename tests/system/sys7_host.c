/* 실제 계산/오류/LED 요청 함수의 PC 검사. RTT와 시간만 모의 처리하며 GPIO는 실행하지 않는다. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "tdc_sys_test_7.h"
#include "tdc_ut_coordination_cm3.h"
#include "cfx_cm3_sharedMemory.h"
#include "error.h"
#include "stimulationParaCal.h"
#include "LedOutput.h"
#include "tdc_ui_command.h"
#include "ci_map.h"
#include "ci_stim_mute.h"
#include "ci_event_log.h"

extern int tdc_ut_cm3_dac_offset_exchange(int offset);
extern int tdc_ut_cm3_led_select(void);
extern void tdc_ut_cm3_led_exchange(int state[LED_SRC__MAX + 3]);
static uint32_t s_tick;
static bool s_rtt_ready = true;
static char s_log[32768];
static size_t s_log_length;
static unsigned s_line_max;
static unsigned s_assertions;
static bool s_rtt_locked;
static const char *s_input;
static size_t s_input_length;
static size_t s_input_next;
static char s_ui_log[8192];
static size_t s_ui_length;
#define CHECK(condition) do { ++s_assertions; assert(condition); } while (0)

void OS_SIM_EnterCriticalSection(void) { CHECK(!s_rtt_locked); s_rtt_locked = true; }
void OS_SIM_LeaveCriticalSection(void) { CHECK(s_rtt_locked); s_rtt_locked = false; }

unsigned SEGGER_RTT_Read(unsigned index, void *buffer, unsigned length)
{
    CHECK(index == 0 && length == 1);
    if (s_input_next == s_input_length) return 0;
    *(char *)buffer = s_input[s_input_next++];
    return 1;
}
unsigned SEGGER_RTT_Write(unsigned index, const void *buffer, unsigned length)
{
    CHECK(index == 0 && s_ui_length + length < sizeof(s_ui_log));
    if (length >= 10 && memcmp(buffer, "[SYS-007] ", 10) == 0)
        CHECK(length <= 60);
    memcpy(s_ui_log + s_ui_length, buffer, length);
    s_ui_length += length;
    s_ui_log[s_ui_length] = '\0';
    return length;
}

unsigned SEGGER_RTT_WriteSkipNoLock(unsigned index, const void *buffer, unsigned length)
{
    CHECK(index == 0 && length <= 60 && length >= 2);
    CHECK(s_rtt_locked);
    const char *text = buffer;
    CHECK(text[length - 2] == '\r' && text[length - 1] == '\n');
    CHECK(strncmp(text, "[SYS-007] ", 10) == 0);
    if (!s_rtt_ready) return 0;
    CHECK(s_log_length + length < sizeof(s_log));
    memcpy(s_log + s_log_length, buffer, length);
    s_log_length += length;
    s_log[s_log_length] = '\0';
    if (s_line_max < length) s_line_max = length;
    return 1; /* SDK 구현의 반환 계약: 바이트 수가 아니다. */
}
int tdc_timer_get_t3_tick(void) { return (int)s_tick; }
int ci_timer_get_tick(void) { return 10000; }
int SEGGER_RTT_printf(unsigned buffer, const char *format, ...)
{
    (void)buffer; (void)format; return 0;
}
void fn_write_userSettingParameters(int isd_num) { (void)isd_num; abort(); }
void fn_copy_MappingData_toCM3(int map, int isd) { (void)map; (void)isd; abort(); }
int ci_fft_read_pass_bin(int bands) { (void)bands; abort(); }
int ci_fft_init_pass_bin(int bands) { (void)bands; abort(); }
/* UI 등록표의 다른 명령은 링크만 가능하게 한다. 실행되면 시험을 즉시 실패시킨다. */
int ci_map_init_map_data(int isd, bool force, bool side, int value)
{ (void)isd; (void)force; (void)side; (void)value; abort(); }
int ci_event_log_read(void) { abort(); }
int ci_event_log_write(uint32_t event) { (void)event; abort(); }
int ci_stim_mute_update(uint32_t enable, uint32_t level)
{ (void)enable; (void)level; abort(); }

static int command(const char *action)
{
    char *argv[] = {"sys7", (char *)action};
    return tdc_sys_test_7_command(2, argv, false);
}
static void drain(void)
{
    for (unsigned i = 0; i < 20; ++i) tdc_sys_test_7_poll();
}
static void expect_log(size_t begin, const char *text)
{
    CHECK(strstr(s_log + begin, text) != NULL);
}
static void set_connected(bool connected)
{
    tdc_ut_cm3_isd_state_t state = {0};
    state.state.conneded_ISD = connected;
    tdc_ut_cm3_isd_state_exchange(&state);
}

static int input_command(const char *t, const char *c, const char *w)
{
    char *argv[] = {"sys7", (char *)t, (char *)c, (char *)w};
    return tdc_sys_test_7_command(4, argv, false);
}

static void feed_ui(const char *input, size_t length)
{
    s_input = input; s_input_length = length; s_input_next = 0;
    for (unsigned i = 0; i < 8 && s_input_next < s_input_length; ++i)
        tdc_ui_command_poll();
    CHECK(s_input_next == s_input_length);
    drain();
}

int main(void)
{
    (void)_setmode(_fileno(stdout), _O_BINARY);
    char *bad[] = {"sys7", "charge", "extra"};
    CHECK(tdc_sys_test_7_command(3, bad, false) == -1);
    CHECK(command("unknown") == -1);
    CHECK(!tdc_sys_test_7_is_active());
    CHECK(command("status") == 0); drain();
    expect_log(0, "HOLD=0 OBS=0");
    CHECK(tdc_sys_test_7_command(2, bad, true) == 0); drain();
    expect_log(0, "disconnect ISD and mapping");
    set_connected(true); CHECK(command("charge") == 0); drain();
    CHECK(!tdc_sys_test_7_is_active());
    set_connected(false);
    errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, 0);
    CHECK(command("led") == 0); drain();
    CHECK(readErrorCode().dataProcessingErrorFlag == en__unusableMapData);
    CHECK(!tdc_sys_test_7_is_active()); clearErrorFlag(en__dataProcessing_ERROR);
    led_request(LED_SRC_POWER, LED_ST_POWER_ON);
    CHECK(command("led") == 0); drain(); CHECK(!tdc_sys_test_7_is_active());
    led_request(LED_SRC_POWER, LED_ST_NONE);
    /* 엔진/GPIO를 실행하지 않는 PC fixture에서 burst 완료 상태를 주입한다. */
    int reset_led[LED_SRC__MAX + 3] = {0};
    tdc_ut_cm3_led_exchange(reset_led);

    /* 잘못된 숫자/키는 시험 시작이나 오류/LED 변경 전에 거절한다. */
    const char *invalid[][3] = {
        {"t=", "c=300", "w=251"}, {"t=-1", "c=300", "w=251"},
        {"t=+1", "c=300", "w=251"}, {"t=301", "c=300", "w=251"},
        {"t=0", "c=0", "w=251"}, {"t=0", "c=300", "w=0"},
        {"t=100", "c=300", "w=65536"}, {"t=100", "c=65536", "w=1"},
        {"t=100", "c=4294967296", "w=1"}, {"t=100", "c=300uA", "w=1"},
        {"t=100", "c=300", "w=251.0"}, {"t=100", "c=300", "w=000001"},
        {"t=100", "t=300", "w=251"}, {"t=100", "c=300", "x=251"},
        {"T=100", "c=300", "w=251"}, {"t=100", "c=300", "w=0xFB"},
        {"", "c=300", "w=251"}, {"t", "c=300", "w=251"},
        {"t=100", "c=300", NULL}
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        CHECK(input_command(invalid[i][0], invalid[i][1], invalid[i][2]) == -1);
        CHECK(!tdc_sys_test_7_is_active());
        CHECK(!tdc_sys_test_7_log_pending());
        CHECK(readErrorCode().dataProcessingErrorFlag == en__NA);
        CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_NONE);
    }
    CHECK(command("t=100") == -1);
    char *extra[] = {"sys7", "t=100", "c=300", "w=251", "extra"};
    CHECK(tdc_sys_test_7_command(3, extra, false) == -1);
    CHECK(tdc_sys_test_7_command(5, extra, false) == -1);
    CHECK(tdc_sys_test_7_command(4, extra, true) == 0); drain();
    CHECK(!tdc_sys_test_7_is_active());
    set_connected(true);
    CHECK(input_command("t=100", "c=300", "w=251") == 0); drain();
    CHECK(!tdc_sys_test_7_is_active()); set_connected(false);

    /* 맵과 공유 자료는 모드 두 필드 외 모두 보존되어야 한다. */
    memset(&cfx_cm3_sharedMemoryAll, 0x25, sizeof(cfx_cm3_sharedMemoryAll));
    ST__CFX_CM3_SharedMemory_ALL expected = cfx_cm3_sharedMemoryAll;
    expected.cfx_PCM_interface.PCM_mode = PcmBitStream_Mode_NopStandby;
    expected.cfx_PCM_interface.PCM_mode_next = PcmBitStream_Mode_NopStandby;
    const ST_STIUL_DAC_REGISTER_VALUE saved_dac = {7, 8, 9};
    *readStimulDAC_RegisterValue() = saved_dac;
    (void)tdc_ut_cm3_dac_offset_exchange(123);
    size_t begin = s_log_length;
    s_rtt_ready = false;
    CHECK(command("charge") == 0);
    CHECK(tdc_sys_test_7_is_active());
    drain(); CHECK(s_log_length == begin);
    CHECK(command("dac") == 1); /* 전송이 막혀도 이전 기록을 덮지 않는다. */
    CHECK(!memcmp(&expected, &cfx_cm3_sharedMemoryAll, sizeof(expected)));
    CHECK(!memcmp(&saved_dac, readStimulDAC_RegisterValue(), sizeof(saved_dac)));
    CHECK(readDAC_offsetValue() == 123);
    s_rtt_ready = true; drain();
    expect_log(begin, "CHARGE=75300 LIMIT=75000 pC");
    expect_log(begin, "CALC=ACCEPT EXPECT=REJECT");
    expect_log(begin, "CHECK=MISMATCH");
#ifdef Df_MaxDeliveryChargeLimitation
    expect_log(begin, "CHARGE_GUARD=ENABLED");
    CHECK(readErrorCode().dataProcessingErrorFlag == en__MaxChargeOver_mapdata);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_ERROR_MAP);
    CHECK(command("stop") == 0); drain();
#else
    expect_log(begin, "CHARGE_GUARD=DISABLED");
    CHECK(readErrorCode().dataProcessingErrorFlag == en__NA);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_NONE);
#endif
    begin = s_log_length;
    CHECK(command("dac") == 0); drain();
    expect_log(begin, "CHARGE=62500 LIMIT=75000 pC");
    expect_log(begin, "CALC=REJECT EXPECT=REJECT");
    expect_log(begin, "CHECK=MATCH; calculation only");
    CHECK(!memcmp(&saved_dac, readStimulDAC_RegisterValue(), sizeof(saved_dac)));
    CHECK(readDAC_offsetValue() == 123);

    /* 32비트 tick wrap, 기존 systemControl의 오류 LED 요청/우선순위, 5초 종료. */
    s_tick = UINT32_MAX - 2000u;
    begin = s_log_length; CHECK(command("led") == 0); drain();
    expect_log(begin, "LED SOURCE=INJECTED; not charge result");
    CHECK(readErrorCode().dataProcessingErrorFlag == en__MaxChargeOver_mapdata);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_ERROR_MAP);
    CHECK(tdc_ut_cm3_led_select() == LED_ST_ERROR_MAP);
    CHECK(command("charge") == 0); drain();
    expect_log(begin, "observation active");
    s_tick += 4999; drain();
    CHECK(readErrorCode().dataProcessingErrorFlag == en__MaxChargeOver_mapdata);
    ++s_tick; drain();
    CHECK(readErrorCode().dataProcessingErrorFlag == en__NA);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_NONE);
    CHECK(tdc_sys_test_7_is_active());
    expect_log(begin, "TIMEOUT: NOP hold; reboot to resume");

    /* RTT가 끝까지 막혀도 stop은 즉시 작동한다. 오류 주입 로그는 나중에 전송한다. */
    begin = s_log_length; s_rtt_ready = false;
    CHECK(command("led") == 0); drain();
    CHECK(command("stop") == 0);
    CHECK(readErrorCode().dataProcessingErrorFlag == en__NA);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_NONE);
    CHECK(tdc_sys_test_7_is_active());
    s_rtt_ready = true; drain();
    expect_log(begin, "SOURCE=INJECTED"); expect_log(begin, "STOP: NOP hold");
    CHECK(command("led") == 0); drain();
    errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, 0);
    errorCodeUpdate(en__ACCELEROMETER_ERROR, en__I2C_ACCELER_ReadingError, 0);
    CHECK(command("stop") == 0); drain();
    CHECK(readErrorCode().dataProcessingErrorFlag == en__unusableMapData);
    CHECK(readErrorCode().accelerometerErrorFlag == en__I2C_ACCELER_ReadingError);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_ERROR_ACCEL);
    clearErrorFlag(en__dataProcessing_ERROR);
    clearErrorFlag(en__ACCELEROMETER_ERROR);
    led_request(LED_SRC_ERROR, LED_ST_NONE);

    /* 요구 전하량 기준: 75000과 같으면 허용. 16비트 입력의 최대 곱도 정확히 계산. */
    const struct {
        const char *t, *c, *w, *charge, *result;
        bool over;
    } vectors[] = {
        {"t=100", "c=300", "w=249", "CHARGE=74700 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"t=100", "c=300", "w=250", "CHARGE=75000 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"t=100", "c=300", "w=251", "CHARGE=75300 LIMIT=75000 pC", "CHARGE_CHECK=WARN EXCESS=300 pC", true},
        {"t=0", "c=24999", "w=3", "CHARGE=74997 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"t=0", "c=25001", "w=3", "CHARGE=75003 LIMIT=75000 pC", "CHARGE_CHECK=WARN EXCESS=3 pC", true},
        {"t=0", "c=1", "w=1", "CHARGE=1 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"t=300", "c=300", "w=250", "CHARGE=75000 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"t=100", "c=2500", "w=25", "CHARGE=62500 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"w=250", "t=00100", "c=300", "CHARGE=75000 LIMIT=75000 pC", "CHARGE_CHECK=PASS", false},
        {"t=65535", "c=65535", "w=65535", "CHARGE=4294836225 LIMIT=75000 pC",
         "CHARGE_CHECK=WARN EXCESS=4294761225 pC", true}
    };
    for (unsigned i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i)
    {
        begin = s_log_length;
        CHECK(input_command(vectors[i].t, vectors[i].c, vectors[i].w) == 0); drain();
        expect_log(begin, "CASE=input SOURCE=TEST_CHARGE");
        expect_log(begin, "SCOPE=charge only; DAC/map not checked");
        expect_log(begin, vectors[i].charge); expect_log(begin, vectors[i].result);
        CHECK(readErrorCode().dataProcessingErrorFlag == (vectors[i].over ? en__MaxChargeOver_mapdata : en__NA));
        CHECK(led_get_request(LED_SRC_ERROR) == (vectors[i].over ? LED_ST_ERROR_MAP : LED_ST_NONE));
        CHECK(!memcmp(&expected, &cfx_cm3_sharedMemoryAll, sizeof(expected)));
        CHECK(!memcmp(&saved_dac, readStimulDAC_RegisterValue(), sizeof(saved_dac)));
        CHECK(readDAC_offsetValue() == 123);
        if (vectors[i].over)
        {
            CHECK(tdc_ut_cm3_led_select() == LED_ST_ERROR_MAP);
            CHECK(strstr(s_log + begin, "CHARGE_CHECK=PASS") == NULL);
            CHECK(command("stop") == 0); drain();
        }
    }

    /* 새 입력 경로도 RTT 정체/5초 종료/중단 중 기존 오류 소유권을 지킨다. */
    begin = s_log_length; s_rtt_ready = false;
    CHECK(input_command("t=100", "c=300", "w=251") == 0); drain();
    CHECK(s_log_length == begin);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_ERROR_MAP);
    CHECK(input_command("t=100", "c=300", "w=250") == 1);
    s_tick += 5000; drain();
    CHECK(readErrorCode().dataProcessingErrorFlag == en__NA);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_NONE);
    s_rtt_ready = true; drain();
    expect_log(begin, "EXCESS=300 pC"); expect_log(begin, "TIMEOUT:");
    CHECK(input_command("t=100", "c=300", "w=251") == 0); drain();
    errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, 0);
    CHECK(command("stop") == 0); drain();
    CHECK(readErrorCode().dataProcessingErrorFlag == en__unusableMapData);
    clearErrorFlag(en__dataProcessing_ERROR); led_request(LED_SRC_ERROR, LED_ST_NONE);

    /* 실제 RTT UI의 토큰화/dispatch/CRLF 경로를 통과시킨다. GPIO는 호출하지 않는다. */
    tdc_ui_command_init();
    begin = s_log_length;
    const char cli_over[] = "--sys7 t=100 c=300 w=251\r\n";
    feed_ui(cli_over, sizeof(cli_over) - 1);
    expect_log(begin, "CHARGE_CHECK=WARN EXCESS=300 pC");
    const char *first = strstr(s_log + begin, "CASE=input");
    CHECK(first != NULL && strstr(first + 1, "CASE=input") == NULL);
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_ERROR_MAP);
    feed_ui("--sys7 stop\r\n", strlen("--sys7 stop\r\n"));
    begin = s_log_length;
    const char cli_pass[] = "--sys7\tw=251\b0 c=300 t=100\r\n";
    feed_ui(cli_pass, sizeof(cli_pass) - 1);
    expect_log(begin, "CHARGE=75000 LIMIT=75000 pC");
    expect_log(begin, "CHARGE_CHECK=PASS");
    CHECK(led_get_request(LED_SRC_ERROR) == LED_ST_NONE);
    size_t ui_begin = s_ui_length;
    feed_ui("--sys7 t=100 c=300\n", strlen("--sys7 t=100 c=300\n"));
    CHECK(strstr(s_ui_log + ui_begin, "INPUT ERROR") != NULL);
    begin = s_log_length;
    char too_long[240];
    memset(too_long, ' ', sizeof(too_long));
    memcpy(too_long, cli_over, sizeof(cli_over) - 3); /* 앞부분이 유효해도 긴 행 전체를 거절한다. */
    too_long[sizeof(too_long) - 1] = '\n';
    feed_ui(too_long, sizeof(too_long));
    CHECK(s_log_length == begin);
    const char embedded_nul[] = "--sys7 t=100 c=300 w=251\0 ignored\n";
    feed_ui(embedded_nul, sizeof(embedded_nul) - 1);
    CHECK(s_log_length == begin);
    CHECK(readErrorCode().dataProcessingErrorFlag == en__NA);
    feed_ui(cli_pass, sizeof(cli_pass) - 1); /* 거절 다음 정상 행은 다시 받을 수 있다. */
    expect_log(begin, "CHARGE_CHECK=PASS");
    ui_begin = s_ui_length;
    feed_ui("--init_all_map\n", strlen("--init_all_map\n"));
    CHECK(strstr(s_ui_log + ui_begin, "HOLD: use --sys7") != NULL);
    CHECK(!memcmp(&expected, &cfx_cm3_sharedMemoryAll, sizeof(expected)));
    CHECK(strstr(s_log, "LOG_FORMAT_ERROR") == NULL);
    puts(s_log);
    printf("HOST_PASS assertions=%u max_line_bytes=%u; no hardware execution\n", s_assertions, s_line_max);
    return 0;
}
