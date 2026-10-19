#if defined(TDC_UNIT_TEST)
#include "tdc_ut_log.h"
#include <stdio.h>
#include <string.h>

int tdc_ut_log_begin(tdc_ut_stream_t *s, const char *core,
                     unsigned unit, unsigned vector, unsigned count,
                     const void *context, tdc_ut_read_t actual,
                     tdc_ut_read_t expected, tdc_ut_write_t write)
{
    if (s == 0 || core == 0 || actual == 0 || write == 0 ||
        unit == 0U || unit > 999U || vector == 0U || vector > 999U ||
        count == 0U || count > 65535U || s->active != 0U)
        return -1;
    if (strcmp(core, "CFX") != 0 && strcmp(core, "HEAR") != 0 &&
        strcmp(core, "CM3") != 0 && strcmp(core, "BOOT") != 0)
        return -1;
    memset(s, 0, sizeof(*s));
    strcpy(s->core, core);
    s->context = context;
    s->actual = actual;
    s->expected = expected;
    s->write = write;
    s->unit = unit;
    s->vector = vector;
    s->count = count;
    s->active = 1U;
    return 0;
}

/* Build a complete range before sending it. Chunk size follows the actual
 * decimal width (including minus signs), not an assumed samples-per-line.
 */
static int build_values(tdc_ut_stream_t *s)
{
    char values[TDC_UT_LINE_BYTES + 1U] = "";
    char trial[TDC_UT_LINE_BYTES + 1U];
    unsigned first = s->index, end = first, used = 0U;
    int size;
    do
    {
        char number[14]; /* INT32_MIN plus comma and terminator. */
        unsigned n;
        size = snprintf(number, sizeof(number), "%s%ld", end == first ? "" : ",",
                        (long) s->actual(s->context, end));
        if (size < 0 || (unsigned) size >= sizeof(number)) return -1;
        n = (unsigned) size;
        if (used + n >= sizeof(values)) break;
        memcpy(values + used, number, n + 1U);
        size = snprintf(trial, sizeof(trial), "[UT-%s-%03u] C%u A[%u:%u]=%s\r\n",
                        s->core, s->unit, s->vector, first, end, values);
        if (size < 0) return -1;
        if ((unsigned) size > TDC_UT_LINE_BYTES)
        {
            values[used] = '\0';
            break;
        }
        memcpy(s->line, trial, (unsigned) size + 1U);
        s->bytes = (unsigned) size;
        used += n;
        end++;
    } while (end < s->count);
    if (end == first) return -1;
    /* Advance only the staging index. The caller may not replace the stream
     * until the pending line (including a partial write) has been accepted.
     */
    s->index = end;
    return 0;
}

int tdc_ut_log_poll(tdc_ut_stream_t *s)
{
    int size;
    unsigned wrote;
    if (s == 0) return -1;
    if (s->active == 0U) return 1;
    if (s->bytes == 0U)
    {
        if (s->phase == 0U)
        {
            if (s->index < s->count)
            {
                if (build_values(s) != 0) return -1;
            }
            else { s->phase = 1U; s->index = 0U; }
        }
        if (s->phase == 1U)
        {
            while (s->expected != 0 && s->index < s->count)
            {
                unsigned i = s->index++;
                int32_t a = s->actual(s->context, i);
                int32_t e = s->expected(s->context, i);
                if (a == e) continue;
                s->failed++;
                size = snprintf(s->line, sizeof(s->line),
                                "[UT-%s-%03u] C%u I%u A=%ld E=%ld\r\n",
                                s->core, s->unit, s->vector, i, (long) a, (long) e);
                if (size < 0 || (unsigned) size > TDC_UT_LINE_BYTES) return -1;
                s->bytes = (unsigned) size;
                break;
            }
            if (s->bytes == 0U) s->phase = 2U;
        }
        if (s->phase == 2U)
        {
            size = snprintf(s->line, sizeof(s->line), "[UT-%s-%03u] C%u %s N=%u F=%u\r\n",
                            s->core, s->unit, s->vector,
                            s->expected == 0 ? "OBSERVED" : s->failed ? "FAIL" : "PASS",
                            s->count, s->failed);
            if (size < 0 || (unsigned) size > TDC_UT_LINE_BYTES) return -1;
            s->bytes = (unsigned) size;
            s->phase = 3U;
        }
    }
    wrote = s->write(s->line + s->sent, s->bytes - s->sent);
    if (wrote > s->bytes - s->sent) return -1;
    s->sent += wrote;
    if (s->sent != s->bytes) return 0;
    s->bytes = s->sent = 0U;
    if (s->phase >= 3U) { s->active = 0U; return 1; }
    return 0;
}

void tdc_ut_log_cancel(tdc_ut_stream_t *s)
{
    if (s == 0 || s->phase == 4U) return;
    if (!s->active || s->sent == 0U)
    {
        memset(s, 0, sizeof(*s));
        return;
    }
    /* If CR was already accepted, only LF remains. Otherwise replace the
     * unsent suffix by CRLF. bytes never exceeds the original line length.
     * Phase 4 must not read the cancelled result storage again.
     */
    if (s->sent == s->bytes - 1U)
        s->line[s->sent] = '\n';
    else
    {
        s->line[s->sent] = '\r';
        s->line[s->sent + 1U] = '\n';
        s->bytes = s->sent + 2U;
    }
    s->line[s->bytes] = '\0';
    s->phase = 4U;
}
#endif
