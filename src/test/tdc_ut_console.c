/* The operator has already stopped product loops and other RAM writers.
 * This reader never injects product commands, controls power, or runs in ISR.
 * A batch is the only output owner until it has terminated its final line.
 */
#if defined(TDC_UNIT_TEST)
#include "tdc_ut_console.h"
#include <SEGGER_RTT.h>
#include <string.h>
#include <stdio.h>
#ifndef TDC_UT_CONSOLE_CAN_EXIT
#define TDC_UT_CONSOLE_CAN_EXIT() 1
#endif

static const tdc_ut_console_command_t *s_commands;
static unsigned s_count;
static int s_active = -1;
static char s_input[48], s_output[61];
static unsigned s_input_used, s_output_size, s_output_sent;
static int s_discard, s_notice, s_exit;
static int s_restore_notice;
volatile int g_tdc_ut_console_exit_requested;
volatile int g_tdc_ut_console_opened;
volatile int g_tdc_ut_console_transport_fault;

static void message(const char *text)
{
    /* All messages here are fixed ASCII literals shorter than 60 bytes. */
    unsigned n = (unsigned)strlen(text);
    if (n > 60U) return;
    memcpy(s_output, text, n + 1U);
    s_output_size = n;
    s_output_sent = 0;
}

static int send_pending(void)
{
    unsigned left = s_output_size - s_output_sent;
    unsigned n = SEGGER_RTT_Write(0, s_output + s_output_sent, left);
    if (n > left)
    {
        /* Unknown transport progress cannot be retried without duplicating
         * bytes or treated as a complete line. Keep ownership and stop here. */
        g_tdc_ut_console_transport_fault = 1;
        return 0;
    }
    s_output_sent += n;
    if (s_output_sent == s_output_size)
        s_output_size = s_output_sent = 0;
    return 0;
}

static int number(const char **input, unsigned *value)
{
    const char *p = *input;
    unsigned n = 0;
    if (*p < '0' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9')
    {
        n = n * 10U + (unsigned)(*p++ - '0');
        if (n > 999U) return -1;
    }
    if (!n) return -1;
    *value = n;
    *input = p;
    return 0;
}

static void command(void)
{
    unsigned i, group = 0, step = 0;
    const char *p;
    if (!strcmp(s_input, "ut stop") || !strcmp(s_input, "ut quit"))
    {
        if (!strcmp(s_input, "ut quit")) s_exit = 1;
        if (s_active >= 0) s_commands[s_active].cancel();
        else if (!s_exit) message("[UT-CONSOLE] IDLE\r\n");
        return;
    }
    if (s_active >= 0)
    {
        /* Do not place a console response inside a partial batch line. */
        if (s_notice != 2) s_notice = !strcmp(s_input, "ut status") ? 1 : 2;
        return;
    }
    if (!strcmp(s_input, "ut status"))
    { message("[UT-CONSOLE] IDLE\r\n"); return; }
    if (!strcmp(s_input, "ut help"))
    { message("[UT-CONSOLE] ut NAME GROUP [STEP] / stop / quit\r\n"); return; }
    if (strncmp(s_input, "ut ", 3))
    { message("[UT-CONSOLE] ERROR COMMAND\r\n"); return; }
    p = s_input + 3;
    for (i = 0; i < s_count; ++i)
    {
        unsigned n = (unsigned)strlen(s_commands[i].name);
        if (!strncmp(p, s_commands[i].name, n) && p[n] == ' ')
        { p += n + 1; break; }
    }
    if (i == s_count || number(&p, &group) < 0)
    { message("[UT-CONSOLE] ERROR COMMAND\r\n"); return; }
    if (*p == ' ')
    {
        ++p;
        if (number(&p, &step) < 0)
        { message("[UT-CONSOLE] ERROR STEP\r\n"); return; }
    }
    if (*p || s_commands[i].begin(group, step) != 0)
    { message("[UT-CONSOLE] ERROR NOT_STARTED\r\n"); return; }
    s_active = (int)i;
}

int tdc_ut_console_open(const tdc_ut_console_command_t *commands, unsigned count)
{
    unsigned i;
    if (g_tdc_ut_console_opened || !commands || !count || count > 8U) return -1;
    for (i = 0; i < count; ++i)
        if (!commands[i].name || !commands[i].begin ||
            !commands[i].poll || !commands[i].cancel) return -1;
    s_commands = commands;
    s_count = count;
    s_active = -1;
    s_input_used = s_discard = s_notice = s_exit = 0;
    s_restore_notice = 0;
    g_tdc_ut_console_transport_fault = 0;
    g_tdc_ut_console_exit_requested = 0;
    g_tdc_ut_console_opened = 1;
    message("[UT-CONSOLE] READY\r\n");
    return 0;
}

int tdc_ut_console_poll(void)
{
    unsigned char ch;
    int result;
    if (!g_tdc_ut_console_opened) return 1;
    if (g_tdc_ut_console_transport_fault) return 0;
    if (g_tdc_ut_console_exit_requested && !s_exit)
    {
        s_exit = 1;
        if (s_active >= 0) s_commands[s_active].cancel();
    }
    if (s_output_size)
        return send_pending();
    if (s_active >= 0)
    {
        result = s_commands[s_active].poll();
        if (result != 0)
        {
            s_active = -1;
            message(result < 0 ? "[UT-CONSOLE] STOPPED_OR_ERROR\r\n" :
                                "[UT-CONSOLE] READY\r\n");
            return 0;
        }
    }
    if (s_active < 0 && s_notice)
    {
        message(s_notice == 2 ? "[UT-CONSOLE] BUSY_COMMAND_REJECTED\r\n" :
                                "[UT-CONSOLE] IDLE\r\n");
        s_notice = 0;
        return 0;
    }
    if (s_exit && s_active < 0)
    {
        if (!TDC_UT_CONSOLE_CAN_EXIT())
        {
            if (!s_restore_notice)
            {
                message("[UT-CONSOLE] WAITING_RESTORE\r\n");
                s_restore_notice = 1;
            }
            return 0;
        }
        /* Never return to product execution before the active writer ends. */
        g_tdc_ut_console_opened = 0;
        return 1;
    }
    if (s_exit || !SEGGER_RTT_Read(0, &ch, 1)) return 0;
    if (ch == '\r') return 0;
    if (ch == '\n')
    {
        if (s_discard)
        {
            if (s_active >= 0) s_notice = 2;
            else message("[UT-CONSOLE] ERROR LINE\r\n");
        }
        else if (s_input_used)
        { s_input[s_input_used] = '\0'; command(); }
        s_input_used = s_discard = 0;
    }
    else if (ch < 32U || ch > 126U || s_input_used >= sizeof(s_input) - 1U)
        s_discard = 1;
    else if (!s_discard) s_input[s_input_used++] = (char)ch;
    return 0;
}

int tdc_ut_console_run(unsigned budget)
{
    int result = 0;
    if (!budget) return -1;
    while (budget-- && result == 0) result = tdc_ut_console_poll();
    return result;
}
#endif
