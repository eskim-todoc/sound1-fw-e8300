#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/test/tdc_ut_log.h"
static char s_output[32768];
static unsigned s_length, s_calls, s_limit;
static int32_t s_values[256], s_expected[256];
static int32_t read_a(const void *p, unsigned i) { (void) p; return s_values[i]; }
static int32_t read_e(const void *p, unsigned i) { (void) p; return s_expected[i]; }
static unsigned write_bytes(const char *p, unsigned n)
{
    unsigned accepted;
    s_calls++;
    /* Reproduce a full ring every third attempt, and partial acceptance. */
    if (s_calls % 3U == 1U) return 0U;
    accepted = n > s_limit ? s_limit : n;
    assert(s_length + accepted < sizeof(s_output));
    memcpy(s_output + s_length, p, accepted);
    s_length += accepted;
    s_output[s_length] = 0;
    return accepted;
}
static void check_lines(void)
{
    const char *p = s_output;
    while (*p)
    {
        const char *end = strstr(p, "\r\n");
        assert(end && (unsigned) (end + 2 - p) <= TDC_UT_LINE_BYTES);
        for (const char *c = p; c < end; c++) assert(*c >= 32 && *c <= 126);
        p = end + 2;
    }
}
int main(void)
{
    tdc_ut_stream_t stream = {0};
    unsigned polls = 0;
    s_limit = 7U;
    for (unsigned i = 0; i < 256U; i++)
        s_expected[i] = s_values[i] = i % 2 ? INT32_MIN : INT32_MAX;
    s_expected[255] = 0;
    assert(tdc_ut_log_begin(&stream, "HEAR", 999, 999, 256, 0, read_a, read_e, write_bytes) == 0);
    assert(tdc_ut_log_begin(&stream, "CM3", 1, 1, 1, 0, read_a, read_e, write_bytes) == -1);
    while (tdc_ut_log_poll(&stream) != 1) assert(++polls < 10000U);
    check_lines();
    assert(strstr(s_output, "A[0:1]=2147483647,-2147483648") != 0);
    assert(strstr(s_output, "I255 A=-2147483648 E=0") != 0);
    assert(strstr(s_output, "FAIL N=256 F=1") != 0);
    assert(strstr(s_output, "PASS") == 0);
    /* All values must be present exactly once despite zero/partial writes. */
    unsigned values = 0;
    const char *p = s_output;
    while ((p = strstr(p, " A[")) != 0)
    {
        unsigned first, last;
        assert(sscanf(p, " A[%u:%u]", &first, &last) == 2);
        assert(first == values);
        values = last + 1U;
        p++;
    }
    assert(values == 256U);
    s_length = s_calls = 0; s_output[0] = 0; s_limit = 60;
    assert(tdc_ut_log_begin(&stream, "CFX", 1, 1, 1, 0, read_a, 0, write_bytes) == 0);
    while (tdc_ut_log_poll(&stream) != 1) assert(++polls < 10000U);
    assert(strstr(s_output, "OBSERVED") && !strstr(s_output, "PASS"));
    check_lines();
    /* Cancellation must preserve a line boundary for each partial position,
     * including CR accepted without LF. Retry may still accept zero bytes.
     */
    for (unsigned position = 1; position < 35; position++)
    {
        s_length = s_calls = 0; s_output[0] = 0; s_limit = position;
        assert(tdc_ut_log_begin(&stream,"CM3",27,1,1,0,read_a,read_e,write_bytes)==0);
        assert(tdc_ut_log_poll(&stream)==0); /* full ring */
        (void)tdc_ut_log_poll(&stream);
        assert(stream.sent>0 && stream.active);
        tdc_ut_log_cancel(&stream);
        tdc_ut_log_cancel(&stream); /* idempotent */
        assert(tdc_ut_log_begin(&stream,"BOOT",1,1,1,0,read_a,0,write_bytes)==-1);
        while(tdc_ut_log_poll(&stream)!=1) assert(++polls<10000U);
        assert(!strstr(s_output,"PASS") && !strstr(s_output,"FAIL"));
        assert(tdc_ut_log_begin(&stream,"BOOT",1,1,1,0,read_a,0,write_bytes)==0);
        while(tdc_ut_log_poll(&stream)!=1) assert(++polls<10000U);
        check_lines();
    }
    assert(tdc_ut_log_begin(&stream, "bad", 1, 1, 1, 0, read_a, read_e, write_bytes) == -1);
    assert(tdc_ut_log_begin(&stream, "CM3", 0, 1, 1, 0, read_a, read_e, write_bytes) == -1);
    assert(tdc_ut_log_begin(&stream, "CM3", 1, 1, 0, 0, read_a, read_e, write_bytes) == -1);
    puts("RTT_LOG PASS: 60B, ASCII, full/partial retry, all 256 values, mismatch, observation");
    return 0;
}
