/* Foreground-only bridge for immutable [-999,unit,case,length,values...] data.
 * No shared-memory ABI change: CFX XMEM symbol word addresses come from the
 * test build's debugger/map, never from an address copied from another build.
 */
#if defined(TDC_UNIT_TEST)
#include <hw.h>
#include <SEGGER_RTT.h>
#include "../../../test/tdc_ut_log.c"

static tdc_ut_stream_t s_tdc_ut_stream;
static const int *s_tdc_ut_actual;
static const int *s_tdc_ut_expected;
static unsigned s_tdc_ut_count, s_tdc_ut_cursor, s_tdc_ut_offset;
static int s_tdc_ut_cfx_words;
static const char *s_tdc_ut_core;

static int32_t read_word(const int *p, unsigned index)
{
    if (s_tdc_ut_cfx_words)
    {
        uint32_t raw = ((const volatile uint32_t *) p)[index] & 0x00ffffffU;
        /* LSB unsigned mapping zero-extends CFX's signed 24-bit words. */
        return raw & 0x00800000U ? (int32_t) (raw - 0x01000000U) : (int32_t) raw;
    }
    return p[index];
}
static int32_t read_actual(const void *unused, unsigned index)
{
    (void) unused;
    return read_word(s_tdc_ut_actual, s_tdc_ut_offset + index);
}
static int32_t read_expected(const void *unused, unsigned index)
{
    (void) unused;
    return read_word(s_tdc_ut_expected, s_tdc_ut_offset + index);
}
static unsigned write_line(const char *data, unsigned length)
{
    return SEGGER_RTT_Write(0, data, length);
}

/* Caller must first stop every writer of the result arrays. done alone does
 * not prevent another core from beginning the next test. No ISR calls.
 */
int tdc_ut_report_begin(const char *core, const int *actual, const int *expected,
                        unsigned count, unsigned capacity, int done, int overflow)
{
    if (s_tdc_ut_stream.active || s_tdc_ut_cursor < s_tdc_ut_count ||
        !done || overflow || core == 0 || actual == 0 ||
        count == 0U || count > capacity || capacity > 65535U)
        return -1;
    if (strcmp(core, "CM3") && strcmp(core, "CFX") &&
        strcmp(core, "HEAR") && strcmp(core, "BOOT")) return -1;
    s_tdc_ut_actual = actual;
    s_tdc_ut_expected = expected;
    s_tdc_ut_core = core;
    s_tdc_ut_cfx_words = 0;
    s_tdc_ut_count = count;
    s_tdc_ut_cursor = 0U;
    return 0;
}

/* One pending line per call. Invalid/truncated records do not print PASS.
 * On tdc_sys_error the stopped results remain available for debugger inspection.
 */
int tdc_ut_report_poll(void)
{
    int result;
    unsigned unit, vector, length;
    if (s_tdc_ut_stream.active) return tdc_ut_log_poll(&s_tdc_ut_stream) < 0 ? -1 : 0;
    if (s_tdc_ut_cursor == s_tdc_ut_count) return 1;
    if (s_tdc_ut_count - s_tdc_ut_cursor < 4U ||
        read_word(s_tdc_ut_actual, s_tdc_ut_cursor) != -999) return -1;
    unit = (unsigned) read_word(s_tdc_ut_actual, s_tdc_ut_cursor + 1U);
    vector = (unsigned) read_word(s_tdc_ut_actual, s_tdc_ut_cursor + 2U);
    length = (unsigned) read_word(s_tdc_ut_actual, s_tdc_ut_cursor + 3U);
    if (length == 0U || length > s_tdc_ut_count - s_tdc_ut_cursor - 4U) return -1;
    /* Check metadata too. A damaged expected header cannot align silently. */
    for (unsigned i = 0U; s_tdc_ut_expected != 0 && i < 4U; i++)
        if (read_word(s_tdc_ut_actual, s_tdc_ut_cursor + i) !=
            read_word(s_tdc_ut_expected, s_tdc_ut_cursor + i)) return -1;
    s_tdc_ut_offset = s_tdc_ut_cursor + 4U;
    result = tdc_ut_log_begin(&s_tdc_ut_stream, s_tdc_ut_core, unit, vector,
                              length, 0, read_actual, s_tdc_ut_expected ? read_expected : 0, write_line);
    if (result != 0) return -1;
    s_tdc_ut_cursor += 4U + length;
    return tdc_ut_log_poll(&s_tdc_ut_stream) < 0 ? -1 : 0;
}

int tdc_ut_report_cfx_core_begin(const char *core, unsigned actual_word, unsigned expected_word,
                           unsigned count, unsigned capacity, int done, int overflow)
{
    unsigned words = (CFX_XRAM3_LSB_UNSIGNED_TOP - CFX_XMEM_LSB_UNSIGNED_BASE + 1U) / 4U;
    unsigned first = (CFX_XRAM0_LSB_UNSIGNED_BASE - CFX_XMEM_LSB_UNSIGNED_BASE) / 4U;
    int result;
    /* Exclude XROM/register windows. Only result arrays in XRAM are valid. */
    if (core == 0 || (strcmp(core,"CFX") && strcmp(core,"HEAR")) ||
        capacity == 0U || capacity > words || actual_word < first || expected_word < first ||
        actual_word > words - capacity ||
        expected_word > words - capacity) return -1;
    result = tdc_ut_report_begin(core,
        (const int *) (CFX_XMEM_LSB_UNSIGNED_BASE + actual_word * 4U),
        (const int *) (CFX_XMEM_LSB_UNSIGNED_BASE + expected_word * 4U),
        count, capacity, done, overflow);
    if (result == 0) s_tdc_ut_cfx_words = 1;
    return result;
}

int tdc_ut_report_cfx_begin(unsigned actual_word, unsigned expected_word,
                           unsigned count, unsigned capacity, int done, int overflow)
{
    return tdc_ut_report_cfx_core_begin("CFX",actual_word,expected_word,count,capacity,done,overflow);
}

/* Keep record headers so several numeric cases retain their original IDs.
 * The separate vector stream carries the in-range verdict; this stream must
 * preserve actual values and bounds without falsely declaring them equal.
 */
int tdc_ut_report_cfx_observed_begin(const char *core, unsigned actual_word,
                                    unsigned count, unsigned capacity, int done, int overflow)
{
    unsigned words=(CFX_XRAM3_LSB_UNSIGNED_TOP-CFX_XMEM_LSB_UNSIGNED_BASE+1U)/4U;
    unsigned first=(CFX_XRAM0_LSB_UNSIGNED_BASE-CFX_XMEM_LSB_UNSIGNED_BASE)/4U;
    int result;
    if (core==0 || (strcmp(core,"CFX") && strcmp(core,"HEAR")) ||
        capacity==0U || capacity>words || actual_word<first || actual_word>words-capacity)
        return -1;
    result=tdc_ut_report_begin(core,
        (const int *)(CFX_XMEM_LSB_UNSIGNED_BASE+actual_word*4U),0,
        count,capacity,done,overflow);
    if (result==0) s_tdc_ut_cfx_words=1;
    return result;
}

/* Raw captured vectors, e.g. 256 HEAR vMag values. Observation never PASS. */
int tdc_ut_report_cfx_values_begin(const char *core, unsigned unit, unsigned vector,
                                  unsigned word, unsigned count, int done)
{
    unsigned words=(CFX_XRAM3_LSB_UNSIGNED_TOP-CFX_XMEM_LSB_UNSIGNED_BASE+1U)/4U;
    unsigned first=(CFX_XRAM0_LSB_UNSIGNED_BASE-CFX_XMEM_LSB_UNSIGNED_BASE)/4U;
    if (core==0 || (strcmp(core,"CFX") && strcmp(core,"HEAR")) || !done ||
        s_tdc_ut_stream.active || s_tdc_ut_cursor<s_tdc_ut_count ||
        count==0 || count>65535U || count>words || word<first || word>words-count) return -1;
    s_tdc_ut_actual=(const int *)(CFX_XMEM_LSB_UNSIGNED_BASE+word*4U);
    s_tdc_ut_cfx_words=1; s_tdc_ut_offset=0;
    return tdc_ut_log_begin(&s_tdc_ut_stream,core,unit,vector,count,0,read_actual,0,write_line);
}

/* Explicit operator cancellation after stopping the writer. Poll until 1
 * to terminate any partial line before a new begin. No completion is added.
 */
void tdc_ut_report_cancel(void)
{
    tdc_ut_log_cancel(&s_tdc_ut_stream);
    s_tdc_ut_count = s_tdc_ut_cursor = s_tdc_ut_offset = 0U;
    s_tdc_ut_actual = s_tdc_ut_expected = 0;
    s_tdc_ut_core = 0;
    s_tdc_ut_cfx_words = 0;
}
#endif
