/* SYS-023의 실제 명령/전원 판단/알람 함수를 PC에서 실행한다. 물리 자극 없음. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tdc_sys_test_23.h"
#include "tdc_sys_test_7.h"
#include "tdc_ut_coordination_cm3.h"
#include "cfx_cm3_sharedMemory.h"
#include "systemControl.h"
#include "indicatorByStimul.h"
#include "tdc_ui_command.h"
#include "ci_map.h"
#include "ci_event_log.h"
#include "ci_stim_mute.h"

extern void tdc_ut_cm3_system_exchange(int state[16]);
extern void tdc_ut_cm3_indicator_exchange(int state[3]);
extern void tdc_ut_cm3_led_exchange(int state[LED_SRC__MAX + 3]);

static uint32_t s_tick;
static bool s_ready = true, s_locked;
static const char *s_input;
static size_t s_input_size, s_input_pos;
static char s_log[65536], s_ui[65536];
static size_t s_log_size, s_ui_size;
static unsigned s_checks, s_max_line;
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
    if (length >= 10 && memcmp(buffer, "[SYS-023] ", 10) == 0) CHECK(length <= 60);
    memcpy(s_ui + s_ui_size, buffer, length);
    s_ui_size += length; s_ui[s_ui_size] = 0;
    return length;
}
unsigned SEGGER_RTT_WriteSkipNoLock(unsigned index, const void *buffer, unsigned length)
{
    const char *line = buffer;
    CHECK(index == 0 && s_locked && length >= 2 && length <= 60);
    CHECK(line[length - 2] == '\r' && line[length - 1] == '\n');
    CHECK(strncmp(line, "[SYS-023] ", 10) == 0 || strncmp(line, "[SYS-007] ", 10) == 0);
    if (!s_ready) return 0;
    CHECK(s_log_size + length < sizeof(s_log));
    memcpy(s_log + s_log_size, buffer, length);
    s_log_size += length; s_log[s_log_size] = 0;
    if (s_max_line < length) s_max_line = length;
    return 1;
}
int tdc_timer_get_t3_tick(void) { return (int)s_tick; }
int ci_timer_get_tick(void) { return 10000; }
int SEGGER_RTT_printf(unsigned buffer, const char *format, ...)
{ (void)buffer; (void)format; return 0; }
/* UI 등록에 필요한 링크 경계. 호출되면 시험 실패: NVM/맵을 수정하면 안 된다. */
void fn_write_userSettingParameters(int isd) { (void)isd; abort(); }
void fn_copy_MappingData_toCM3(int map, int isd) { (void)map; (void)isd; abort(); }
int ci_fft_read_pass_bin(int bands) { (void)bands; abort(); }
int ci_fft_init_pass_bin(int bands) { (void)bands; abort(); }
int ci_map_init_map_data(int isd, bool force, bool side, int value)
{ (void)isd; (void)force; (void)side; (void)value; abort(); }
int ci_event_log_read(void) { abort(); }
int ci_event_log_write(uint32_t event) { (void)event; abort(); }
int ci_stim_mute_update(uint32_t enable, uint32_t level)
{ (void)enable; (void)level; abort(); }

static void drain(void)
{
    for (int i = 0; i < 24; ++i) tdc_sys_test_23_poll();
}
static int command(const char *action)
{
    char *argv[] = {"sys23", (char *)action};
    return tdc_sys_test_23_command(2, argv, false);
}
static void feed(const char *input, size_t size)
{
    s_input = input; s_input_size = size; s_input_pos = 0;
    for (int i = 0; i < 8 && s_input_pos < s_input_size; ++i) tdc_ui_command_poll();
    CHECK(s_input_pos == s_input_size);
    drain();
}
static void connected(bool value)
{
    tdc_ut_cm3_isd_state_t state = {0};
    state.state.conneded_ISD = value;
    tdc_ut_cm3_isd_state_exchange(&state);
}
static void fixture(void)
{
    CHECK(!tdc_sys_test_23_is_active()); drain();
    memset(&cfx_cm3_sharedMemoryAll, 0, sizeof(cfx_cm3_sharedMemoryAll));
    cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Disconnected;
    cfx_cm3_sharedMemoryAll.chargerState.carryingCasePluggedIn = df_Disconnected;
    cfx_cm3_sharedMemoryAll.userSettingValue.indicatorStimul_OnOff = 1;
    cfx_cm3_sharedMemoryAll.currentMapData.stimulationIndicatorChannelNum = 17;
    cfx_cm3_sharedMemoryAll.currentMapData.stimulationIndicatorAmplitude_uA = 1500;
    setCalculatedStimulationIndcatorLevel_byCM3(200);
    changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);
    snd_batt_set_percent(80);
    snd_batt_set_state(EN__SND_BATT_STATE_DISCHARGING);
    connected(true);
    int system[16] = {0}; system[7] = df_Disconnected; system[8] = 1;
    tdc_ut_cm3_system_exchange(system);
    int indicator[3] = {0}; tdc_ut_cm3_indicator_exchange(indicator);
    int led[LED_SRC__MAX + 3] = {0}; tdc_ut_cm3_led_exchange(led);
}
static ST__SYSTEM_STATE step(void)
{
    ++s_tick;
    tdc_sys_test_23_prepare(false);
    const ST__SYSTEM_STATE state = systemControl(readErrorCode(), snd_charger_get_state(),
        snd_batt_get_percent(), false, snd_isd_interface_get_state().conneded_ISD, false);
    stimulation_IndicatorOut(readStimulIndicator_OnOff(), state.StimulationIndicatorTriggerLowPower, false);
    tdc_sys_test_23_observe(state.StimulationIndicatorTriggerLowPower);
    tdc_sys_test_23_poll();
    return state;
}
static unsigned count_text(const char *text, const char *part)
{
    unsigned count = 0;
    while ((text = strstr(text, part)) != NULL) { ++count; text += strlen(part); }
    return count;
}
static void rejected(const char *reason)
{
    const size_t begin = s_log_size;
    CHECK(command("start") == 0); drain();
    CHECK(!tdc_sys_test_23_is_active());
    CHECK(strstr(s_log + begin, reason) != NULL);
}

int main(void)
{
    fixture(); tdc_ui_command_init();
    char *extra[] = {"sys23", "start", "ignored"};
    CHECK(tdc_sys_test_23_command(3, extra, false) == -1);
    CHECK(tdc_sys_test_23_command(2, NULL, false) == -1);
    CHECK(command("unknown") == -1);
    CHECK(command("status") == 0); drain();
    CHECK(strstr(s_log, "STATE=IDLE REAL=80% ALARM=80%") != NULL);
    snd_batt_set_percent(39); rejected("REAL_BATTERY_REQUIRES_40_TO_100");
    snd_batt_set_percent(101); rejected("REAL_BATTERY_REQUIRES_40_TO_100");
    fixture(); snd_batt_set_state(EN__SND_BATT_STATE_RESET); rejected("CHARGER_OR_BATTERY_NOT_READY");
    fixture(); cfx_cm3_sharedMemoryAll.chargerState.chargerConnectorPluggedIn = df_Connected;
    rejected("CHARGER_OR_BATTERY_NOT_READY");
    fixture(); connected(false); rejected("CONNECT_TEST_ISD_IN_LIVE_MODE");
    fixture(); changePcmOutputMode(PcmBitStream_Mode_NopStandby); rejected("CONNECT_TEST_ISD_IN_LIVE_MODE");
    fixture(); cfx_cm3_sharedMemoryAll.userSettingValue.indicatorStimul_OnOff = 2;
    rejected("ENABLE_STIMULATION_INDICATOR");
    fixture(); cfx_cm3_sharedMemoryAll.currentMapData.stimulationIndicatorChannelNum = 0;
    rejected("CHECK_MAP_ALARM_CHANNEL_AND_LEVEL");
    fixture(); setCalculatedStimulationIndcatorLevel_byCM3(256); rejected("CHECK_MAP_ALARM_CHANNEL_AND_LEVEL");
    fixture(); errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, 0); rejected("EXISTING_ERROR");
    clearErrorFlag(en__dataProcessing_ERROR);
    fixture(); led_request(LED_SRC_POWER, LED_ST_POWER_OFF); rejected("POWER_PATTERN_ACTIVE");
    fixture(); CHECK(tdc_sys_test_23_command(2, extra, true) == 0); drain();
    CHECK(!tdc_sys_test_23_is_active());

    /* 실제 UI부터 기존 systemControl/알람 함수까지. QCC 잔량/맵/전원 요청 보존. */
    fixture();
    const ST__CFX_CM3_SharedMemory_mapData saved_map = *getPointerCurrentMapData();
    const size_t begin = s_log_size;
    feed("--sys23 start\r\n", strlen("--sys23 start\r\n"));
    CHECK(tdc_sys_test_23_is_active());
    CHECK(tdc_sys_test_23_alarm_percent(80) == 19);
    CHECK(command("start") == 0); drain();
    CHECK(strstr(s_log + begin, "test already active") != NULL);
    const size_t ui_begin = s_ui_size;
    feed("--init_all_map\n", strlen("--init_all_map\n"));
    CHECK(strstr(s_ui + ui_begin, "use --sys23 stop first") != NULL);
    unsigned triggers = 0, on_ticks = 0, edges = 0;
    int prev_gate = 0;
    for (int i = 0; i < 600; ++i)
    {
        const ST__SYSTEM_STATE state = step();
        CHECK(state.enable_ISD && state.enablePMIC && !state.systemOff);
        CHECK(led_get_request(LED_SRC_POWER) == LED_ST_NONE);
        CHECK(snd_batt_get_percent() == 80);
        CHECK(readCurrentPcmOutputMode() == PcmBitStream_Mode_LiveStimulation);
        if (state.StimulationIndicatorTriggerLowPower) ++triggers;
        int gate = cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3;
        if (gate != prev_gate) { ++edges; prev_gate = gate; }
        if (gate) ++on_ticks;
    }
    drain();
    CHECK(triggers == 1 && edges == 6 && on_ticks >= 237 && on_ticks <= 240);
    CHECK(!tdc_sys_test_23_is_active() && !is_Triggered_stimulationIndicator());
    CHECK(tdc_sys_test_23_alarm_percent(80) == 80);
    CHECK(memcmp(&saved_map, getPointerCurrentMapData(), sizeof(saved_map)) == 0);
    CHECK(count_text(s_log + begin, "LOW_BATTERY_TRIGGER=1") == 1);
    CHECK(count_text(s_log + begin, "CM3_ALARM_GATE=1") == 3);
    CHECK(count_text(s_log + begin, "CM3_ALARM_GATE=0") == 3);
    CHECK(strstr(s_log + begin, "DONE: CM3 alarm ended") != NULL);

    /* 카운터를 시험 코드로 재설정하지 않아도 두 번째 명령에서 다시 한 회차 요청. */
    CHECK(command("start") == 0); drain();
    CHECK(step().StimulationIndicatorTriggerLowPower);
    CHECK(command("stop") == 0); drain();
    CHECK(!tdc_sys_test_23_is_active() && tdc_sys_test_23_alarm_percent(80) == 80);
    rejected("previous alarm still running");
    for (int i = 0; i < 600; ++i) step();

    /* RTT 정체가 있어도 실제 전원 보호 우선. 시험에 의해 시스템 종료가 은폐되지 않음. */
    fixture(); s_ready = false;
    CHECK(command("start") == 0);
    CHECK(command("status") == 1);
    snd_batt_set_percent(39);
    ST__SYSTEM_STATE state = step();
    CHECK(!tdc_sys_test_23_is_active() && !state.enable_ISD);
    CHECK(!state.StimulationIndicatorTriggerLowPower);
    CHECK(led_get_request(LED_SRC_POWER) == LED_ST_POWER_OFF);
    s_ready = true; drain();
    fixture(); s_ready = false; CHECK(command("start") == 0);
    CHECK(command("stop") == 0); CHECK(!tdc_sys_test_23_is_active());
    s_ready = true; drain();

    /* 순환 tick/5초: 알람 소비가 진행되지 않으면 입력을 해제한다. */
    fixture(); s_tick = UINT32_MAX - 2000u;
    CHECK(command("start") == 0); drain();
    s_tick += 4999u; tdc_sys_test_23_prepare(false); CHECK(tdc_sys_test_23_is_active());
    ++s_tick; tdc_sys_test_23_prepare(false); CHECK(!tdc_sys_test_23_is_active()); drain();
    CHECK(strstr(s_log, "TIMEOUT: input restored") != NULL);
    fixture(); CHECK(command("start") == 0); drain();
    tdc_sys_test_23_prepare(true); CHECK(!tdc_sys_test_23_is_active()); drain();
    fixture(); CHECK(command("start") == 0); drain();
    connected(false); tdc_sys_test_23_prepare(false); CHECK(!tdc_sys_test_23_is_active()); drain();

    /* 긴 행/NUL/추가 인자 거절, CRLF 중복 방지, 다음 정상 명령 복구. */
    fixture();
    char long_line[220]; memset(long_line, ' ', sizeof(long_line));
    memcpy(long_line, "--sys23 start", 13); long_line[219] = '\n';
    feed(long_line, sizeof(long_line)); CHECK(!tdc_sys_test_23_is_active());
    const char nul[] = "--sys23 start\0 ignored\n";
    feed(nul, sizeof(nul) - 1); CHECK(!tdc_sys_test_23_is_active());
    feed("--sys23 start extra\n", strlen("--sys23 start extra\n")); CHECK(!tdc_sys_test_23_is_active());
    snd_batt_set_percent(40); /* 정상 보호 하한과 같은 값은 시험 가능. */
    feed("--sys23 start\r\n", strlen("--sys23 start\r\n"));
    CHECK(tdc_sys_test_23_is_active() && step().StimulationIndicatorTriggerLowPower);
    CHECK(command("stop") == 0); drain();
#if TDC_SYS_TEST_7_ENABLE
    fixture(); connected(false); changePcmOutputMode(PcmBitStream_Mode_NopStandby);
    char *sys7[] = {"sys7", "t=100", "c=300", "w=250"};
    CHECK(tdc_sys_test_7_command(4, sys7, false) == 0);
    for (int i = 0; i < 20; ++i) tdc_sys_test_7_poll();
    CHECK(tdc_sys_test_7_is_active());
    rejected("SYS7_HOLD_REBOOT_REQUIRED");
#endif
    CHECK(strstr(s_log, "LOG_FORMAT_ERROR") == NULL);
    puts(s_log);
    printf("HOST_PASS assertions=%u max_line_bytes=%u; no device execution\n", s_checks, s_max_line);
    return 0;
}
