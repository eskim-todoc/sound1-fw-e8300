/* SYS-023: 알람 판단만 19%로 시험한다. 실제 잔량/종료 기준/맵은 그대로 둔다. */
#include "tdc_sys_test_23.h"

#if TDC_SYS_TEST_23_ENABLE
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SEGGER_RTT.h>

#include "tdc_pwr_battery.h"
#include "tdc_shm.h"
#include "tdc_hal_timer.h"
#include "tdc_sys_error.h"
#include "tdc_stim_indicator.h"
#include "tdc_isd.h"
#include "tdc_led_output.h"
#include "tdc_sys_test_7.h"

#define TDC_SYS23_PREFIX "[SYS-023] "
#define TDC_SYS23_LINE_BYTES 60u
#define TDC_SYS23_BATCH_LINES 16u
#define TDC_SYS23_TIMEOUT_MS 5000u
#define TDC_SYS23_ALARM_PERCENT 19

typedef enum
{
    TDC_SYS23_IDLE,
    TDC_SYS23_ARMED,
    TDC_SYS23_OBSERVING
} tdc_sys23_state_t;

/* UI, prepare, observe, poll 모두 CM3 전경 소유. ISR 접근/블로킹 대기 없음.
 * 로그가 막혀도 stop/시간 제한은 작동한다. 새 응답은 이전 batch 배출 뒤 받는다. */
static tdc_sys23_state_t s_tdc_state;
static uint32_t s_tdc_started;
static int s_tdc_prev_gate;
static char s_tdc_lines[TDC_SYS23_BATCH_LINES][TDC_SYS23_LINE_BYTES + 1u];
static unsigned s_tdc_line_count;
static unsigned s_tdc_line_next;

static void log_line(const char *format, ...)
{
    if (s_tdc_line_count >= TDC_SYS23_BATCH_LINES)
        return; /* 시작/요청/게이트 6변화/종료의 고정 상한은 호스트 시험에서 확인. */
    char *line = s_tdc_lines[s_tdc_line_count++];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, TDC_SYS23_LINE_BYTES - 1u, format, args);
    va_end(args);
    if (length < 0 || (unsigned)length > TDC_SYS23_LINE_BYTES - 2u)
    {
        strcpy(line, TDC_SYS23_PREFIX "LOG_FORMAT_ERROR");
        length = (int)strlen(line);
    }
    line[length++] = '\r';
    line[length++] = '\n';
    line[length] = '\0';
}

static bool has_error(void)
{
    const tdc_sys_error_code_t tdc_sys_error = tdc_sys_error_read();
    return tdc_sys_error.dataProcessingErrorFlag || tdc_sys_error.accelerometerErrorFlag ||
           tdc_sys_error.PowerIcErrorFlag || tdc_sys_error.FPGA_CommunicationErrorFlag ||
           tdc_sys_error.FPGA_ConfiguraionErrorFlag || tdc_sys_error.ISD_ErrorFlag ||
           tdc_sys_error.FPGA_OR_ISD_ErrorFlag || tdc_sys_error.FPGA_systemError ||
           tdc_sys_error.FPGA_backtelError || tdc_sys_error.data_logging_error;
}

static const char *not_ready(bool mapping_connected)
{
#if TDC_SYS_TEST_7_ENABLE
    if (tdc_sys_test_7_is_active())
        return "SYS7_HOLD_REBOOT_REQUIRED";
#endif
    const int real_percent = tdc_pwr_battery_get_percent();
    /* 현재 제품 종료 기준은 40% 미만이다. 시험값으로 이를 덮어쓰지 않는다. */
    if (real_percent < 40 || real_percent > 100)
        return "REAL_BATTERY_REQUIRES_40_TO_100";
    if (tdc_pwr_battery_get_state() != TDC_PWR_BATTERY_STATE_DISCHARGING ||
        tdc_pwr_charger_get_state().chargerConnectorPluggedIn != df_Disconnected)
        return "CHARGER_OR_BATTERY_NOT_READY";
    if (mapping_connected)
        return "DISCONNECT_MAPPING";
    if (!tdc_isd_get_state().conneded_ISD ||
        tdc_shm_read_current_pcm_output_mode() != PcmBitStream_Mode_LiveStimulation)
        return "CONNECT_TEST_ISD_IN_LIVE_MODE";
    if (has_error() || tdc_led_get_request(TDC_LED_SRC_ERROR) != TDC_LED_ST_NONE)
        return "EXISTING_ERROR";
    if (tdc_led_is_burst_pending() || tdc_led_get_request(TDC_LED_SRC_POWER) != TDC_LED_ST_NONE)
        return "POWER_PATTERN_ACTIVE";
    if (tdc_shm_read_stimul_indicator_on_off() != 1)
        return "ENABLE_STIMULATION_INDICATOR";
    const ST__CFX_CM3_SharedMemory_mapData *map = tdc_shm_get_pointer_current_map_data();
    const int level = cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255;
    if (map->stimulationIndicatorChannelNum < 1 || map->stimulationIndicatorChannelNum > 32 ||
        level < 1 || level > 255)
        return "CHECK_MAP_ALARM_CHANNEL_AND_LEVEL";
    return NULL;
}

bool tdc_sys_test_23_is_active(void)
{
    return s_tdc_state != TDC_SYS23_IDLE;
}

bool tdc_sys_test_23_log_pending(void)
{
    return s_tdc_line_count != 0u;
}

int tdc_sys_test_23_alarm_percent(int real_percent)
{
    return tdc_sys_test_23_is_active() ? TDC_SYS23_ALARM_PERCENT : real_percent;
}

int tdc_sys_test_23_command(int argc, char *argv[], bool mapping_connected)
{
    if (argc != 2 || argv == NULL || argv[1] == NULL)
        return -1;
    const char *action = argv[1];
    if (strcmp(action, "stop") == 0 && tdc_sys_test_23_is_active())
    {
        /* 새 시험 입력만 즉시 해제한다. 진행 중 알람의 카운터/게이트는 소유하지
         * 않으므로 기존 함수가 끝내게 한다. 이미 전송된 자극을 취소하지 못한다. */
        s_tdc_state = TDC_SYS23_IDLE;
        log_line(TDC_SYS23_PREFIX "STOP: input restored; alarm may finish");
        return 0;
    }
    if (strcmp(action, "start") != 0 && strcmp(action, "status") != 0 && strcmp(action, "stop") != 0)
        return -1;
    if (tdc_sys_test_23_log_pending())
        return 1;
    if (strcmp(action, "status") == 0)
    {
        const char *state = s_tdc_state == TDC_SYS23_ARMED ? "ARMED" :
                            s_tdc_state == TDC_SYS23_OBSERVING ? "OBSERVING" : "IDLE";
        log_line(TDC_SYS23_PREFIX "STATE=%s REAL=%d%% ALARM=%d%%", state,
                 tdc_pwr_battery_get_percent(), tdc_sys_test_23_alarm_percent(tdc_pwr_battery_get_percent()));
        log_line(TDC_SYS23_PREFIX "INDICATOR=%d PCM=%d", tdc_shm_read_stimul_indicator_on_off(), tdc_shm_read_current_pcm_output_mode());
        return 0;
    }
    if (strcmp(action, "stop") == 0)
    {
        log_line(TDC_SYS23_PREFIX "STOP: no active test");
        return 0;
    }
    if (tdc_sys_test_23_is_active())
    {
        log_line(TDC_SYS23_PREFIX "REJECT: test already active");
        return 0;
    }
    const char *reason = not_ready(mapping_connected);
    if (reason != NULL)
    {
        log_line(TDC_SYS23_PREFIX "REJECT: %s", reason);
        return 0;
    }
    if (tdc_stim_indicator_is_triggered())
    {
        log_line(TDC_SYS23_PREFIX "REJECT: previous alarm still running");
        return 0;
    }
    s_tdc_started = (uint32_t)tdc_hal_timer_get_t3_tick();
    s_tdc_prev_gate = 0;
    s_tdc_state = TDC_SYS23_ARMED;
    log_line(TDC_SYS23_PREFIX "START SOURCE=TEST_ALARM_INPUT");
    log_line(TDC_SYS23_PREFIX "REAL=%d%% ALARM=19%% TIMEOUT_MS=5000", tdc_pwr_battery_get_percent());
    log_line(TDC_SYS23_PREFIX "POWER_SOURCE=REAL; test input cannot sleep");
    log_line(TDC_SYS23_PREFIX "MAP_CH=%d CODE=%d (current map unchanged)",
             tdc_shm_get_pointer_current_map_data()->stimulationIndicatorChannelNum,
             cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255);
    log_line(TDC_SYS23_PREFIX "SCOPE=CM3 request; measure electrode output");
    return 0;
}

void tdc_sys_test_23_prepare(bool mapping_connected)
{
    if (!tdc_sys_test_23_is_active())
        return;
    const char *reason = not_ready(mapping_connected);
    if (reason != NULL)
    {
        s_tdc_state = TDC_SYS23_IDLE;
        log_line(TDC_SYS23_PREFIX "ABORT: %s", reason);
    }
    else if ((uint32_t)((uint32_t)tdc_hal_timer_get_t3_tick() - s_tdc_started) >= TDC_SYS23_TIMEOUT_MS)
    {
        s_tdc_state = TDC_SYS23_IDLE;
        log_line(TDC_SYS23_PREFIX "TIMEOUT: input restored; no system PASS");
    }
}

void tdc_sys_test_23_observe(bool low_power_trigger)
{
    if (!tdc_sys_test_23_is_active())
        return;
    if (low_power_trigger && s_tdc_state == TDC_SYS23_ARMED)
    {
        s_tdc_state = TDC_SYS23_OBSERVING;
        log_line(TDC_SYS23_PREFIX "LOW_BATTERY_TRIGGER=1");
    }
    if (s_tdc_state == TDC_SYS23_OBSERVING)
    {
        const int gate = cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3;
        if (gate != s_tdc_prev_gate)
        {
            s_tdc_prev_gate = gate;
            log_line(TDC_SYS23_PREFIX "CM3_ALARM_GATE=%d", gate);
        }
        if (!tdc_stim_indicator_is_triggered())
        {
            s_tdc_state = TDC_SYS23_IDLE;
            log_line(TDC_SYS23_PREFIX "DONE: CM3 alarm ended; input restored");
        }
    }
}

void tdc_sys_test_23_poll(void)
{
    if (s_tdc_line_count == 0u)
        return;
    const char *line = s_tdc_lines[s_tdc_line_next];
    const unsigned length = (unsigned)strlen(line);
    unsigned written;
    /* SDK 계약: 용량이 충분할 때 한 행 전체를 쓰고 1, 부족하면 0. 대기하지 않는다. */
    SEGGER_RTT_LOCK();
    written = SEGGER_RTT_WriteSkipNoLock(0, line, length);
    SEGGER_RTT_UNLOCK();
    if (written == 1u && ++s_tdc_line_next == s_tdc_line_count)
    {
        s_tdc_line_next = 0u;
        s_tdc_line_count = 0u;
    }
}
#endif
