#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/test/tdc_ut_console.c"

static char output[16384];
static unsigned out_count, chunk = 60;
static const char *input = "";
static int starts, cancelled, work, next_error;
static int bad_writer;
static unsigned last_group, last_step;

unsigned SEGGER_RTT_Write(unsigned ch, const void *data, unsigned count)
{
    assert(ch == 0 && count <= 60);
    if (bad_writer) return count + 1;
    if (count > chunk) count = chunk;
    assert(out_count + count < sizeof(output));
    memcpy(output + out_count, data, count);
    out_count += count;
    output[out_count] = 0;
    return count;
}
unsigned SEGGER_RTT_Read(unsigned ch, void *data, unsigned count)
{
    assert(ch == 0 && count == 1);
    if (!*input) return 0;
    *(char *)data = *input++;
    return 1;
}
static int begin(unsigned group, unsigned step)
{
    if (group > 3) return -1;
    ++starts; last_group = group; last_step = step;
    work = 100;
    return 0;
}
static int poll_batch(void)
{
    if (work > 0) { --work; return 0; }
    if (next_error) { next_error = 0; return -1; }
    return 1;
}
static void cancel_batch(void) { cancelled++; work = 3; next_error = 1; }
static const tdc_ut_console_command_t commands[] = {{"cm3", begin, poll_batch, cancel_batch}};
static void feed(const char *text)
{
    assert(!*input); input = text;
    assert(tdc_ut_console_run(10000) >= 0);
    assert(!*input);
}
int main(void)
{
    assert(tdc_ut_console_open(0, 1) == -1);
    assert(tdc_ut_console_open(commands, 0) == -1);
    assert(tdc_ut_console_open(commands, 1) == 0);
    assert(tdc_ut_console_open(commands, 1) == -1);
    assert(tdc_ut_console_run(0) == -1);
    chunk = 0;
    assert(tdc_ut_console_run(20) == 0 && out_count == 0);
    chunk = 1;
    feed("ut cm3 1\r\n");
    assert(starts == 1 && last_group == 1 && last_step == 0);
    assert(strstr(output,"[UT-CONSOLE] READY\r\n"));
    feed("ut cm3 3 9\n"); assert(starts == 2 && last_step == 9);
    feed("ut cm3 0\nut cm3 4\nut cm3 1000\nut cm3 1 xx\nut cm3 1 0\nut cm3 1 2x\n");
    assert(starts == 2);
    feed("ut cm3 1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\nut help\n");
    assert(starts == 2 && strstr(output,"ERROR LINE"));
    feed("ut cm3 1\nut cm3 2\nut status\n");
    assert(starts == 3);
    feed("ut cm3 1\nut cm3 2\n");
    assert(starts == 4 && strstr(output,"BUSY_COMMAND_REJECTED"));
    feed("ut cm3 1\nut stop\n");
    assert(cancelled == 1 && strstr(output,"STOPPED_OR_ERROR"));
    feed("ut cm3 1\nut quit\n");
    assert(cancelled == 2 && !g_tdc_ut_console_opened);
    assert(tdc_ut_console_run(1) == 1);
    assert(tdc_ut_console_open(commands, 1) == 0);
    feed("ut cm3 1\n");
    input = "ut cm3 1\n";
    while (*input) assert(tdc_ut_console_poll() == 0);
    g_tdc_ut_console_exit_requested = 1;
    assert(tdc_ut_console_run(1000) == 1 && cancelled == 3);
    /* Every console response is one complete ASCII line, including retries. */
    const char *p = output;
    while (*p) {
        const char *end = strstr(p,"\r\n");
        assert(end && end - p + 2 <= 60);
        for (const char *q=p;q<end;q++) assert(*q>=32 && *q<=126);
        p = end + 2;
    }
    assert(tdc_ut_console_open(commands, 1) == 0);
    chunk = 1;
    assert(tdc_ut_console_poll() == 0);
    bad_writer = 1;
    assert(tdc_ut_console_poll() == 0 && g_tdc_ut_console_transport_fault);
    unsigned saved_output = out_count;
    bad_writer = 0;
    g_tdc_ut_console_exit_requested = 1;
    assert(tdc_ut_console_run(100) == 0 && g_tdc_ut_console_opened);
    assert(out_count == saved_output && tdc_ut_console_open(commands, 1) == -1);
    puts("CONSOLE_HARNESS_OK: commands, busy, retries, cancel, repeat, transport latch");
    return 0;
}
