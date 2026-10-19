/* SW-FW-SYS-Test-ID-7: 시험 전하량 검사/제품 계산 관찰/오류 주입을 구분한다.
 * 정상 빌드는 이 파일 전체를 제외한다. 착용/이식 내부기 연결 상태 사용 금지. */
#include "tdc_sys_test_7.h"

#if TDC_SYS_TEST_7_ENABLE
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SEGGER_RTT.h>

#include "tdc_shm.h"
#include "tdc_hal_timer.h"
#include "tdc_sys_error.h"
#include "tdc_isd.h"
#include "tdc_led_output.h"
#include "tdc_sys_control.h"
#include <tdc_ui_command.h>

#define TDC_SYS7_LINE_BYTES 60u /* CRLF 포함. 문자열 종료 NUL은 전송하지 않는다. */
#define TDC_SYS7_BATCH_LINES 16u
#define TDC_SYS7_LED_HOLD_MS 5000u
#define TDC_SYS7_INPUT_MAX 65535u /* 시험 콘솔 범위. 실제 맵/DAC 허용 범위가 아니다. */
#define TDC_SYS7_PREFIX "[SYS-007] "
_Static_assert(df_MaxNumOfElectrode == 32, "SYS-007 vectors require 32 channels");

/* 명령/배출 모두 CM3 전경에서만 접근한다. 한 명령의 로그 배출 중 다음 시험은 거절.
 * 각 응답은 16줄 이내이며 종료 알림은 다음 batch. ISR/동적 할당/대기 루프 없음. */
static char s_tdc_lines[TDC_SYS7_BATCH_LINES][TDC_SYS7_LINE_BYTES + 1u];
static unsigned s_tdc_line_count;
static unsigned s_tdc_line_next;
static bool s_tdc_active;
static bool s_tdc_observing;
static int s_tdc_owned_error;
static bool s_tdc_owns_led;
static uint32_t s_tdc_observe_started;
static const char *s_tdc_end_notice;

static void tdc_sys7_log(const char *format, ...)
{
    va_list args;
    if (s_tdc_line_count >= TDC_SYS7_BATCH_LINES)
        return; /* 고정 응답의 상한은 호스트 시험에서 확인한다. */
    char *line = s_tdc_lines[s_tdc_line_count++];
    va_start(args, format);
    int length = vsnprintf(line, TDC_SYS7_LINE_BYTES - 1u, format, args);
    va_end(args);
    if (length < 0 || (unsigned)length > TDC_SYS7_LINE_BYTES - 2u)
    {
        strcpy(line, TDC_SYS7_PREFIX "LOG_FORMAT_ERROR");
        length = (int)strlen(line);
    }
    line[length++] = '\r';
    line[length++] = '\n';
    line[length] = '\0';
}

static bool tdc_sys7_has_error(tdc_sys_error_code_t tdc_sys_error)
{
    return tdc_sys_error.dataProcessingErrorFlag || tdc_sys_error.accelerometerErrorFlag ||
           tdc_sys_error.PowerIcErrorFlag || tdc_sys_error.FPGA_CommunicationErrorFlag ||
           tdc_sys_error.FPGA_ConfiguraionErrorFlag || tdc_sys_error.ISD_ErrorFlag ||
           tdc_sys_error.FPGA_OR_ISD_ErrorFlag || tdc_sys_error.FPGA_systemError ||
           tdc_sys_error.FPGA_backtelError || tdc_sys_error.data_logging_error;
}

static void tdc_sys7_hold_nop(void)
{
    /* SpecificCommand 처리 중에도 다음 모드가 Live로 되돌아가지 않게 먼저 지정.
     * 이미 전송된 패킷의 취소/물리적 무자극을 보장하는 API는 아니므로 내부기 분리 필수. */
    tdc_shm_change_next_pcm_output_mode(PcmBitStream_Mode_NopStandby);
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);
}

static void tdc_sys7_show_error(void)
{
    const tdc_sys_error_code_t tdc_sys_error = tdc_sys_error_read();
    if (tdc_sys_error.dataProcessingErrorFlag != en__NA || tdc_sys_error.accelerometerErrorFlag != en__NA ||
        tdc_sys_error.FPGA_CommunicationErrorFlag != en__NA || tdc_sys_error.data_logging_error != en__NA)
    {
        /* 실제 오류 분기만 호출한다. 정상 전원/맵 제어는 시험 hold에서 실행하지 않는다.
         * GPIO 직접 조작 없이 기존 tdc_sys_control_step -> Timer 3 LED 경로로 관찰한다. */
        const ST__USB_CONNECTOR charger = {0};
        (void)tdc_sys_control_step(tdc_sys_error, charger, 100, false, false, false);
    }
}

static void tdc_sys7_end_observation(const char *notice)
{
    if (s_tdc_owned_error != en__NA && tdc_sys_error_read().dataProcessingErrorFlag == s_tdc_owned_error)
        tdc_sys_error_clear_flag(en__dataProcessing_ERROR);
    s_tdc_owned_error = en__NA;
    s_tdc_observing = false;
    /* 다른 오류가 발생했으면 지우지 않는다. LED 엔진의 중간 상태를 임의 복원하지 않는다. */
    if (s_tdc_owns_led && !tdc_sys7_has_error(tdc_sys_error_read()) && tdc_led_get_request(TDC_LED_SRC_ERROR) == TDC_LED_ST_ERROR_MAP)
        tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_NONE);
    s_tdc_owns_led = false;
    tdc_sys7_show_error();
    if (s_tdc_end_notice == NULL)
        s_tdc_end_notice = notice;
}

static bool tdc_sys7_begin(bool mapping_connected)
{
    if (tdc_isd_get_state().conneded_ISD || mapping_connected)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "REJECT: disconnect ISD and mapping");
        return false;
    }
    if (tdc_sys7_has_error(tdc_sys_error_read()) || tdc_led_get_request(TDC_LED_SRC_ERROR) != TDC_LED_ST_NONE)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "REJECT: existing tdc_sys_error; resolve first");
        return false;
    }
    if (tdc_ui_command_is_led_override(TDC_LED_SRC_ERROR) || tdc_led_is_burst_pending() ||
        tdc_led_get_request(TDC_LED_SRC_POWER) != TDC_LED_ST_NONE)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "REJECT: LED override or power pattern");
        return false;
    }
    s_tdc_active = true;
    tdc_sys7_hold_nop();
    return true;
}

/* 숫자 일부만 읽는 atoi 대신 전체 문자열을 검사한다. 키 순서는 자유지만 중복은 금지.
 * 5자리 제한과 누적 전 범위 검사로 긴 입력/정수 넘침을 거절한다. 출력은 성공 시만 갱신. */
static bool tdc_sys7_parse_input(int argc, char *argv[], uint32_t values[3])
{
    uint32_t parsed[3] = {0u, 0u, 0u}; /* t, c, w */
    unsigned seen = 0u;
    if (argc != 4)
        return false;
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (arg == NULL || arg[0] == '\0' || arg[1] != '=')
            return false;
        const unsigned index = arg[0] == 't' ? 0u : arg[0] == 'c' ? 1u : 2u;
        if ((arg[0] != 't' && arg[0] != 'c' && arg[0] != 'w') || (seen & (1u << index)))
            return false;
        unsigned digits = 0u;
        uint32_t value = 0u;
        for (const char *p = arg + 2; *p != '\0'; ++p)
        {
            if (*p < '0' || *p > '9' || ++digits > 5u)
                return false;
            const uint32_t digit = (uint32_t)(*p - '0');
            if (value > (TDC_SYS7_INPUT_MAX - digit) / 10u)
                return false;
            value = value * 10u + digit;
        }
        if (digits == 0u)
            return false;
        parsed[index] = value;
        seen |= 1u << index;
    }
    if (seen != 7u || parsed[0] > parsed[1] || parsed[1] == 0u || parsed[2] == 0u)
        return false;
    memcpy(values, parsed, sizeof(parsed));
    return true;
}

static void tdc_sys7_check_charge(const uint32_t values[3])
{
    /* uA x us = pC. t <= c를 확인했으므로 C가 최대 전하량이다.
     * 65535 x 65535는 uint32_t에 들어간다. signed 곱셈/실제 자극 설정은 하지 않는다.
     * 제품 보호 결함을 보존하라는 지정에 따라 이 분기는 시험 명령에만 존재한다. */
    const uint32_t charge_pC = values[1] * values[2];
    const uint32_t limit_pC = df_MaxDeliveryCharge_pC;
    tdc_sys7_log(TDC_SYS7_PREFIX "CASE=input SOURCE=TEST_CHARGE");
    tdc_sys7_log(TDC_SYS7_PREFIX "IN T=%lu C=%lu uA W=%lu us", (unsigned long)values[0],
                 (unsigned long)values[1], (unsigned long)values[2]);
    tdc_sys7_log(TDC_SYS7_PREFIX "CHARGE=%lu LIMIT=%lu pC", (unsigned long)charge_pC, (unsigned long)limit_pC);
    tdc_sys7_log(TDC_SYS7_PREFIX "SCOPE=charge only; DAC/map not checked");
    if (charge_pC > limit_pC)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "CHARGE_CHECK=WARN EXCESS=%lu pC", (unsigned long)(charge_pC - limit_pC));
        tdc_sys_error_update(en__dataProcessing_ERROR, en__MaxChargeOver_mapdata, __LINE__);
    }
    else
        tdc_sys7_log(TDC_SYS7_PREFIX "CHARGE_CHECK=PASS");
}

bool tdc_sys_test_7_is_active(void)
{
    return s_tdc_active;
}

bool tdc_sys_test_7_log_pending(void)
{
    return s_tdc_line_count != 0u || s_tdc_end_notice != NULL;
}

int tdc_sys_test_7_command(int argc, char *argv[], bool mapping_connected)
{
    if (argc < 2 || argv == NULL || argv[1] == NULL)
        return -1;
    const char *action = argv[1];
    if (argc == 2 && strcmp(action, "stop") == 0)
    {
        /* RTT가 막혀도 중단은 즉시 처리한다. 로그만 기존 batch 뒤에 보낸다.
         * 정상 제어로 돌아가지 않으며 사용자가 재부팅할 때까지 NOP hold를 유지한다. */
        if (s_tdc_active)
            tdc_sys7_end_observation("STOP: NOP hold; reboot to resume");
        else if (s_tdc_end_notice == NULL)
            s_tdc_end_notice = "STOP: no active test";
        return 0;
    }
    uint32_t values[3];
    const bool input = tdc_sys7_parse_input(argc, argv, values);
    if (!input && (argc != 2 || (strcmp(action, "charge") != 0 && strcmp(action, "dac") != 0 &&
                                strcmp(action, "led") != 0 && strcmp(action, "status") != 0)))
        return -1;
    if (s_tdc_line_count != 0u || s_tdc_end_notice != NULL)
        return 1; /* UI는 BUSY를 알린다. 기존 로그를 덮어쓰지 않는다. */
    if (strcmp(action, "status") == 0)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "HOLD=%u OBS=%u PCM=%d", s_tdc_active ? 1u : 0u,
                     s_tdc_observing ? 1u : 0u, tdc_shm_read_current_pcm_output_mode());
        tdc_sys7_log(TDC_SYS7_PREFIX "DATA_ERR=%d LED_REQ=%d",
                     (int)tdc_sys_error_read().dataProcessingErrorFlag, (int)tdc_led_get_request(TDC_LED_SRC_ERROR));
        return 0;
    }
    if (s_tdc_observing)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "REJECT: observation active; use stop");
        return 0;
    }
    if (!tdc_sys7_begin(mapping_connected))
        return 0;

    if (input)
        tdc_sys7_check_charge(values);
    else if (strcmp(action, "led") == 0)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "LED SOURCE=INJECTED; not charge result");
        tdc_sys_error_update(en__dataProcessing_ERROR, en__MaxChargeOver_mapdata, __LINE__);
    }
    else
    {
        const bool charge = strcmp(action, "charge") == 0;
        const int pulse = charge ? 251 : 25;
        const int current = charge ? 300 : 2500;
        tdc_sys7_log(TDC_SYS7_PREFIX "CASE=%s SOURCE=ACTUAL_CALC", action);
        tdc_sys7_log(TDC_SYS7_PREFIX "IN T=100 C=%d uA PW=%d us N=32", current, pulse);
        tdc_sys7_log(TDC_SYS7_PREFIX "CHARGE=%d LIMIT=%d pC", current * pulse, df_MaxDeliveryCharge_pC);
#ifdef Df_MaxDeliveryChargeLimitation
        tdc_sys7_log(TDC_SYS7_PREFIX "CHARGE_GUARD=ENABLED");
#else
        tdc_sys7_log(TDC_SYS7_PREFIX "CHARGE_GUARD=DISABLED");
#endif
        const bool result = tdc_sys_test_7_calculate(pulse, 100, current);
        const int tdc_sys_error = (int)tdc_sys_error_read().dataProcessingErrorFlag;
        tdc_sys7_log(TDC_SYS7_PREFIX "CALC=%s EXPECT=REJECT", result ? "ACCEPT" : "REJECT");
        tdc_sys7_log(TDC_SYS7_PREFIX "DATA_ERR=%d EXPECT=%d", tdc_sys_error, charge ? en__MaxChargeOver_mapdata : en__NA);
        const bool matches = !result && tdc_sys_error == (charge ? en__MaxChargeOver_mapdata : en__NA);
        tdc_sys7_log(TDC_SYS7_PREFIX "CHECK=%s; calculation only", matches ? "MATCH" : "MISMATCH");
    }

    s_tdc_owned_error = (int)tdc_sys_error_read().dataProcessingErrorFlag;
    tdc_sys7_show_error();
    s_tdc_owns_led = s_tdc_owned_error != en__NA && tdc_led_get_request(TDC_LED_SRC_ERROR) == TDC_LED_ST_ERROR_MAP;
    tdc_sys7_log(TDC_SYS7_PREFIX "LED_REQ=%d RED_MAP=%d", (int)tdc_led_get_request(TDC_LED_SRC_ERROR), TDC_LED_ST_ERROR_MAP);
    if (s_tdc_owned_error != en__NA)
    {
        s_tdc_observing = true;
        s_tdc_observe_started = (uint32_t)tdc_hal_timer_get_t3_tick();
        tdc_sys7_log(TDC_SYS7_PREFIX "VISUAL=CHECK_REQUIRED HOLD_MS=5000");
    }
    tdc_sys7_log(TDC_SYS7_PREFIX "NOP hold; reboot to resume; no system PASS");
    return 0;
}

void tdc_sys_test_7_poll(void)
{
    if (s_tdc_active)
    {
        tdc_sys7_hold_nop();
        if (s_tdc_observing && (uint32_t)((uint32_t)tdc_hal_timer_get_t3_tick() - s_tdc_observe_started) >= TDC_SYS7_LED_HOLD_MS)
            tdc_sys7_end_observation("TIMEOUT: NOP hold; reboot to resume");
        tdc_sys7_show_error();
    }
    if (s_tdc_line_count == 0u && s_tdc_end_notice != NULL)
    {
        tdc_sys7_log(TDC_SYS7_PREFIX "%s", s_tdc_end_notice);
        s_tdc_end_notice = NULL;
    }
    if (s_tdc_line_count != 0u)
    {
        const char *line = s_tdc_lines[s_tdc_line_next];
        const unsigned length = (unsigned)strlen(line);
        unsigned written;
        /* 이 SDK의 WriteSkipNoLock은 성공 시 1, 용량 부족 시 0. 부분 행은 보내지 않는다. */
        SEGGER_RTT_LOCK();
        written = SEGGER_RTT_WriteSkipNoLock(0, line, length);
        SEGGER_RTT_UNLOCK();
        if (written == 1u && ++s_tdc_line_next == s_tdc_line_count)
        {
            s_tdc_line_next = 0u;
            s_tdc_line_count = 0u;
        }
    }
}
#endif
