/* Test-only XMEM relay. Descriptor DATA remains owned by CFX until ALL logs
 * (comparison, numeric triples, FFT, vMag and exponent) are accepted by RTT.
 * ACK is never a substitute for the actual test verdict or saved PC capture.
 */
#if defined(TDC_UNIT_TEST)
#include <hw.h>
#include <stdint.h>
#include <string.h>
#include <SEGGER_RTT.h>
#include "tdc_ut_report.h"
#include "tdc_ut_batch_cfx_bridge.h"

enum bridge_phase
{
    BR_IDLE, BR_OPEN, BR_BEGIN, BR_WAIT, BR_REPORT, BR_ACK, BR_END,
    BR_CANCEL, BR_ABORT_WAIT, BR_ERROR_LINE
};
static const unsigned s_tdc_ut_bridge_ids[TDC_UT_BATCH_CFX_COUNT] = {
    TDC_UT_BATCH_VECTOR_IDS, TDC_UT_BATCH_IO_IDS
};
static volatile uint32_t *s_tdc_ut_bridge_mb;
static unsigned s_tdc_ut_bridge_word, s_tdc_ut_bridge_token, s_tdc_ut_bridge_session;
static unsigned s_tdc_ut_bridge_group, s_tdc_ut_bridge_total, s_tdc_ut_bridge_completed;
static unsigned s_tdc_ut_bridge_limit, s_tdc_ut_bridge_wait, s_tdc_ut_bridge_error;
static unsigned s_tdc_ut_bridge_phase, s_tdc_ut_bridge_part, s_tdc_ut_bridge_report_active;
static unsigned s_tdc_ut_bridge_desc[UTB_COMPLETED + 1U];
static char s_tdc_ut_bridge_line[61];
static unsigned s_tdc_ut_bridge_bytes, s_tdc_ut_bridge_sent, s_tdc_ut_bridge_part_label;
static unsigned s_tdc_ut_bridge_abort_sent;
static const int s_tdc_ut_bridge_probe[5] = {-999, 1, 1, 1, 0};

static void barrier(void)
{
#if defined(__arm__) || defined(__thumb__)
    __asm volatile ("dmb" ::: "memory");
#endif
}
static uint32_t read_mb(unsigned index)
{ return s_tdc_ut_bridge_mb[index] & 0x00ffffffU; }
static void write_mb(unsigned index, unsigned value)
{ s_tdc_ut_bridge_mb[index] = value & 0x00ffffffU; }
static int in_ram(unsigned word, unsigned count)
{
    unsigned first = (CFX_XRAM0_LSB_UNSIGNED_BASE - CFX_XMEM_LSB_UNSIGNED_BASE) / 4U;
    unsigned end = (CFX_XRAM3_LSB_UNSIGNED_TOP - CFX_XMEM_LSB_UNSIGNED_BASE + 1U) / 4U;
    return count && word >= first && word < end && count <= end - word;
}
static int overlap(unsigned a, unsigned n, unsigned b, unsigned m)
{ return n && m && a < b + m && b < a + n; }
static int envelope(void)
{
    return s_tdc_ut_bridge_mb && read_mb(UTB_MAGIC) == TDC_UT_BATCH_MAGIC &&
        read_mb(UTB_VERSION) == TDC_UT_BATCH_VERSION && read_mb(UTB_WORDS) == UTB_WORD_COUNT &&
        read_mb(UTB_TOKEN) == s_tdc_ut_bridge_token &&
        (read_mb(UTB_READY_FLAGS) & TDC_UT_BATCH_RAM_READY) &&
        read_mb(UTB_READY_FLAGS) <= (TDC_UT_BATCH_RAM_READY | TDC_UT_BATCH_HEAR_READY);
}
static int identity(void)
{
    return envelope() && read_mb(UTB_SESSION) == s_tdc_ut_bridge_session &&
        read_mb(UTB_GROUP) == s_tdc_ut_bridge_group &&
        read_mb(UTB_READY_ACK) == s_tdc_ut_bridge_token &&
        read_mb(UTB_TOTAL) == s_tdc_ut_bridge_total;
}
static const char *core(void)
{ return s_tdc_ut_bridge_group == TDC_UT_BATCH_HEAR ? "HEAR" : "CFX"; }
static void append_text(const char *text)
{
    while (*text && s_tdc_ut_bridge_bytes < 58U)
        s_tdc_ut_bridge_line[s_tdc_ut_bridge_bytes++] = *text++;
}
static void append_uint(unsigned value)
{
    char digits[10]; unsigned n = 0U;
    do { digits[n++] = (char)('0' + value % 10U); value /= 10U; } while (value && n < sizeof(digits));
    while (n && s_tdc_ut_bridge_bytes < 58U) s_tdc_ut_bridge_line[s_tdc_ut_bridge_bytes++] = digits[--n];
}
static void line_prefix(void)
{
    s_tdc_ut_bridge_bytes = s_tdc_ut_bridge_sent = 0U;
    append_text("[UT-BATCH] CORE="); append_text(core());
}
static void line_finish(void)
{
    s_tdc_ut_bridge_line[s_tdc_ut_bridge_bytes++] = '\r';
    s_tdc_ut_bridge_line[s_tdc_ut_bridge_bytes++] = '\n';
    s_tdc_ut_bridge_line[s_tdc_ut_bridge_bytes] = 0;
}
static void boundary_line(const char *name)
{ line_prefix(); append_text(name); append_text(" N="); append_uint(s_tdc_ut_bridge_total); line_finish(); }
static int drain_line(void)
{
    unsigned remaining = s_tdc_ut_bridge_bytes - s_tdc_ut_bridge_sent;
    if (remaining)
    {
        unsigned written = SEGGER_RTT_Write(0, s_tdc_ut_bridge_line + s_tdc_ut_bridge_sent, remaining);
        if (written > remaining) written = remaining;
        s_tdc_ut_bridge_sent += written;
    }
    if (s_tdc_ut_bridge_sent != s_tdc_ut_bridge_bytes) return 0;
    s_tdc_ut_bridge_bytes = s_tdc_ut_bridge_sent = 0U;
    return 1;
}
static int command(unsigned cmd, unsigned seq)
{
    if (!envelope() || read_mb(UTB_CMD)) return 0;
    write_mb(UTB_CMD_TOKEN, s_tdc_ut_bridge_token);
    write_mb(UTB_CMD_SESSION, s_tdc_ut_bridge_session);
    write_mb(UTB_CMD_GROUP, s_tdc_ut_bridge_group);
    write_mb(UTB_CMD_SEQ, seq); write_mb(UTB_CMD_LIMIT, s_tdc_ut_bridge_limit);
    barrier(); write_mb(UTB_CMD, cmd); barrier();
    return 1;
}
static void fail(unsigned reason)
{
    if (s_tdc_ut_bridge_phase >= BR_CANCEL) return;
    s_tdc_ut_bridge_error = reason;
    /* An entirely unsent boundary line may be discarded. A partial line must
     * finish before cancellation/error output. Never append an END on error. */
    if (!s_tdc_ut_bridge_sent) s_tdc_ut_bridge_bytes = 0U;
    if (s_tdc_ut_bridge_report_active) tdc_ut_report_cancel();
    s_tdc_ut_bridge_abort_sent = 0U;
    s_tdc_ut_bridge_wait = 0U; s_tdc_ut_bridge_phase = BR_CANCEL;
}
int tdc_ut_cfx_bridge_configure(unsigned mailbox_word, unsigned token)
{
    volatile uint32_t *old_mb = s_tdc_ut_bridge_mb;
    unsigned old_token = s_tdc_ut_bridge_token;
    unsigned state;
    if (tdc_ut_cfx_bridge_busy()) return -UTB_BUSY;
    if (!token || token > TDC_UT_BATCH_MAX_ID || !in_ram(mailbox_word, UTB_WORD_COUNT))
        return -UTB_BAD_ARGUMENT;
    s_tdc_ut_bridge_mb = (volatile uint32_t *)(CFX_XMEM_LSB_UNSIGNED_BASE + mailbox_word * 4U);
    s_tdc_ut_bridge_token = token;
    state = read_mb(UTB_STATE);
    if (!envelope() || read_mb(UTB_CMD) ||
        (state != UTB_READY && state != UTB_DONE && state != UTB_ABORTED))
    {
        s_tdc_ut_bridge_mb = old_mb; s_tdc_ut_bridge_token = old_token;
        return -UTB_NOT_READY;
    }
    s_tdc_ut_bridge_word = mailbox_word;
    s_tdc_ut_bridge_error = s_tdc_ut_bridge_completed = 0U;
    return 0;
}
int tdc_ut_cfx_bridge_begin(unsigned group, unsigned session, unsigned poll_limit)
{
    unsigned state;
    if (tdc_ut_cfx_bridge_busy()) return -UTB_BUSY;
    if ((group != TDC_UT_BATCH_CFX && group != TDC_UT_BATCH_HEAR) ||
        !session || session > TDC_UT_BATCH_MAX_ID || !poll_limit || poll_limit > TDC_UT_BATCH_MAX_ID)
        return -UTB_BAD_ARGUMENT;
    if (!envelope()) return -UTB_NOT_READY;
    state = read_mb(UTB_STATE);
    if ((state != UTB_READY && state != UTB_DONE && state != UTB_ABORTED) || read_mb(UTB_CMD))
        return -UTB_BUSY;
    if (session <= read_mb(UTB_SESSION)) return -UTB_PROTOCOL;
    if (group == TDC_UT_BATCH_HEAR && !(read_mb(UTB_READY_FLAGS) & TDC_UT_BATCH_HEAR_READY))
        return -UTB_NOT_READY;
    /* The existing reporter has no busy getter. Reserve then cancel a valid
     * zero-output probe, without polling it. If somebody owns a partial line
     * or an unfinished record, begin rejects and their state stays untouched.
     * The caller must still serialize foreground access AFTER this probe. */
    if (tdc_ut_report_begin("CFX", s_tdc_ut_bridge_probe, s_tdc_ut_bridge_probe, 5U, 5U, 1, 0))
        return -UTB_BUSY;
    tdc_ut_report_cancel();
    s_tdc_ut_bridge_group = group; s_tdc_ut_bridge_session = session;
    s_tdc_ut_bridge_limit = poll_limit;
    s_tdc_ut_bridge_total = group == TDC_UT_BATCH_CFX ? TDC_UT_BATCH_CFX_COUNT : TDC_UT_BATCH_HEAR_COUNT;
    s_tdc_ut_bridge_completed = s_tdc_ut_bridge_error = s_tdc_ut_bridge_wait = 0U;
    s_tdc_ut_bridge_bytes = s_tdc_ut_bridge_sent = s_tdc_ut_bridge_report_active = 0U;
    s_tdc_ut_bridge_abort_sent = 0U;
    if (!command(UTB_OPEN, 0U)) return -UTB_BUSY;
    s_tdc_ut_bridge_phase = BR_OPEN;
    return 0;
}
static int32_t result_word(unsigned address)
{
    uint32_t raw = *(const volatile uint32_t *)(CFX_XMEM_LSB_UNSIGNED_BASE + address * 4U) & 0x00ffffffU;
    return raw & 0x00800000U ? (int32_t)(raw - 0x01000000U) : (int32_t)raw;
}
/* Validate the complete record stream BEFORE emitting any comparison. */
static int records(unsigned actual, unsigned expected, unsigned count, unsigned unit, unsigned vector)
{
    unsigned pos = 0U;
    while (pos < count)
    {
        int32_t length, test;
        unsigned i;
        if (count - pos < 5U || result_word(actual + pos) != -999 ||
            result_word(actual + pos + 1U) != (int32_t)unit) return 0;
        test = result_word(actual + pos + 2U); length = result_word(actual + pos + 3U);
        if (test < 1 || test > 999 || (vector && test != (int32_t)vector) ||
            length < 1 || (unsigned)length > count - pos - 4U) return 0;
        if (expected)
            for (i = 0U; i < 4U; ++i)
                if (result_word(actual + pos + i) != result_word(expected + pos + i)) return 0;
        pos += 4U + (unsigned)length;
    }
    return pos == count;
}
static int stable_descriptor(void)
{
    unsigned i;
    if (!identity() || read_mb(UTB_STATE) != UTB_DATA || read_mb(UTB_ERROR) ||
        read_mb(UTB_SEQ) != s_tdc_ut_bridge_completed + 1U ||
        read_mb(UTB_COMPLETED) != s_tdc_ut_bridge_completed) return 0;
    for (i = UTB_UNIT; i <= UTB_FAILED; ++i)
        if (read_mb(i) != s_tdc_ut_bridge_desc[i]) return 0;
    return 1;
}
static int descriptor(void)
{
    unsigned seq = s_tdc_ut_bridge_completed + 1U, unit, vector, i, j;
    unsigned address[5], size[5];
    unsigned *d = s_tdc_ut_bridge_desc;
    if (!identity() || read_mb(UTB_STATE) != UTB_DATA || read_mb(UTB_SEQ) != seq ||
        read_mb(UTB_ERROR) || read_mb(UTB_COMPLETED) != s_tdc_ut_bridge_completed || seq > s_tdc_ut_bridge_total)
        return 0;
    barrier();
    for (i = 0U; i <= UTB_COMPLETED; ++i) d[i] = read_mb(i);
    unit = s_tdc_ut_bridge_group == TDC_UT_BATCH_CFX ? s_tdc_ut_bridge_ids[seq - 1U] : seq == 1U ? 1U : 2U;
    vector = s_tdc_ut_bridge_group == TDC_UT_BATCH_CFX ? 0U : seq == 1U ? 1U : seq - 1U;
    if (d[UTB_UNIT] != unit || d[UTB_VECTOR] != vector || !d[UTB_COUNT] ||
        d[UTB_CAPACITY] != (s_tdc_ut_bridge_group == TDC_UT_BATCH_CFX && seq <= TDC_UT_BATCH_VECTOR_COUNT ? 528U : 768U) ||
        d[UTB_COUNT] > d[UTB_CAPACITY] || d[UTB_FAILED] > d[UTB_COUNT] ||
        d[UTB_NUMERIC_COUNT] > 400U || read_mb(UTB_RESTORED) != 1U)
        return 0;
    /* Numeric triples exist only for these real SDK calculation units. */
    if (s_tdc_ut_bridge_group == TDC_UT_BATCH_CFX && (unit == 23U || unit == 31U || unit == 32U))
    { if (!d[UTB_NUMERIC_COUNT]) return 0; }
    else if (d[UTB_NUMERIC] || d[UTB_NUMERIC_COUNT]) return 0;
    if (s_tdc_ut_bridge_group == TDC_UT_BATCH_HEAR && unit == 2U)
    {
        if (d[UTB_FFT_COUNT] != 512U || d[UTB_VMAG_COUNT] != 256U) return 0;
    }
    else if (d[UTB_FFT] || d[UTB_FFT_COUNT] || d[UTB_VMAG] || d[UTB_VMAG_COUNT] || d[UTB_EXPONENT]) return 0;
    address[0] = d[UTB_ACTUAL]; address[1] = d[UTB_EXPECTED];
    size[0] = size[1] = d[UTB_CAPACITY];
    address[2] = d[UTB_NUMERIC]; size[2] = d[UTB_NUMERIC_COUNT] ? 400U : 0U;
    address[3] = d[UTB_FFT]; size[3] = d[UTB_FFT_COUNT];
    address[4] = d[UTB_VMAG]; size[4] = d[UTB_VMAG_COUNT];
    for (i = 0U; i < 5U; ++i)
    {
        if (!size[i]) { if (address[i]) return 0; continue; }
        if (!in_ram(address[i], size[i]) || overlap(address[i], size[i], s_tdc_ut_bridge_word, UTB_WORD_COUNT)) return 0;
        for (j = 0U; j < i; ++j)
            if (overlap(address[i], size[i], address[j], size[j])) return 0;
    }
    if (!records(d[UTB_ACTUAL], d[UTB_EXPECTED], d[UTB_COUNT], unit, vector) ||
        (d[UTB_NUMERIC_COUNT] && !records(d[UTB_NUMERIC], 0U, d[UTB_NUMERIC_COUNT], unit, 0U))) return 0;
    barrier();
    return stable_descriptor();
}
static int start_part(void)
{
    unsigned *d = s_tdc_ut_bridge_desc;
    switch (s_tdc_ut_bridge_part)
    {
        case 0U: return tdc_ut_report_cfx_core_begin(core(), d[UTB_ACTUAL], d[UTB_EXPECTED],
                                                     d[UTB_COUNT], d[UTB_CAPACITY], 1, 0);
        case 1U: return tdc_ut_report_cfx_observed_begin(core(), d[UTB_NUMERIC], d[UTB_NUMERIC_COUNT], 400U, 1, 0);
        case 2U: return tdc_ut_report_cfx_values_begin(core(), d[UTB_UNIT], d[UTB_VECTOR], d[UTB_FFT], 512U, 1);
        case 3U: return tdc_ut_report_cfx_values_begin(core(), d[UTB_UNIT], d[UTB_VECTOR], d[UTB_VMAG], 256U, 1);
        default: return tdc_ut_report_cfx_values_begin(core(), d[UTB_UNIT], d[UTB_VECTOR],
                                                       s_tdc_ut_bridge_word + UTB_EXPONENT, 1U, 1);
    }
}
static void report_step(void)
{
    int result;
    if (!stable_descriptor()) { fail(UTB_PROTOCOL); return; }
    if (s_tdc_ut_bridge_report_active)
    {
        result = tdc_ut_report_poll();
        if (result < 0) { fail(UTB_REPORT_ERROR); return; }
        if (!result) return;
        s_tdc_ut_bridge_report_active = 0U; ++s_tdc_ut_bridge_part;
        s_tdc_ut_bridge_part_label = 0U; s_tdc_ut_bridge_wait = 0U;
    }
    if (s_tdc_ut_bridge_part == 1U && !s_tdc_ut_bridge_desc[UTB_NUMERIC_COUNT]) ++s_tdc_ut_bridge_part;
    if (s_tdc_ut_bridge_part >= 2U && !s_tdc_ut_bridge_desc[UTB_FFT_COUNT]) s_tdc_ut_bridge_part = 5U;
    if (s_tdc_ut_bridge_part == 5U)
    {
        if (command(UTB_ACK, s_tdc_ut_bridge_completed + 1U))
        { s_tdc_ut_bridge_phase = BR_ACK; s_tdc_ut_bridge_wait = 0U; }
        return;
    }
    if (s_tdc_ut_bridge_part && !s_tdc_ut_bridge_part_label)
    {
        line_prefix(); append_text(" RAW=");
        append_text(s_tdc_ut_bridge_part == 1U ? "NUMERIC" : s_tdc_ut_bridge_part == 2U ? "FFT" :
                    s_tdc_ut_bridge_part == 3U ? "VMAG" : "EXPONENT");
        append_text(" U="); append_uint(s_tdc_ut_bridge_desc[UTB_UNIT]);
        append_text(" C="); append_uint(s_tdc_ut_bridge_desc[UTB_VECTOR]); line_finish();
        s_tdc_ut_bridge_part_label = 1U;
    }
    if (!drain_line()) return;
    if (start_part() < 0) { fail(UTB_REPORT_ERROR); return; }
    s_tdc_ut_bridge_report_active = 1U;
}
static void error_line(void)
{
    line_prefix(); append_text(" ERROR E="); append_uint(s_tdc_ut_bridge_error);
    append_text(" RESTORED="); append_uint((unsigned)tdc_ut_cfx_bridge_restored()); line_finish();
    s_tdc_ut_bridge_phase = BR_ERROR_LINE;
}
static void cancel_step(void)
{
    if (!drain_line()) return;
    if (s_tdc_ut_bridge_report_active)
    {
        if (tdc_ut_report_poll() != 1) return;
        s_tdc_ut_bridge_report_active = 0U;
    }
    s_tdc_ut_bridge_wait = 0U; s_tdc_ut_bridge_phase = BR_ABORT_WAIT;
}
int tdc_ut_cfx_bridge_poll(void)
{
    unsigned state;
    if (s_tdc_ut_bridge_phase == BR_IDLE)
        return s_tdc_ut_bridge_error ? -(int)s_tdc_ut_bridge_error : 1;
    /* END is committed only after every ACK and log. Once committed, finish
     * that line even if the operator presses cancel; no live data is held. */
    if (s_tdc_ut_bridge_phase < BR_CANCEL && s_tdc_ut_bridge_phase != BR_END)
    {
        if (!envelope()) fail(UTB_PROTOCOL);
        /* OPEN may replace an ABORTED session. Its old error remains visible
         * until CFX publishes the entire new response and clears CMD last.
         * Envelope validity and the wait limit still apply while pending. */
        else if ((s_tdc_ut_bridge_phase != BR_OPEN || !read_mb(UTB_CMD)) && read_mb(UTB_ERROR))
            fail(read_mb(UTB_ERROR) <= UTB_REPORT_ERROR ? read_mb(UTB_ERROR) : UTB_PROTOCOL);
        else if (++s_tdc_ut_bridge_wait >= s_tdc_ut_bridge_limit) fail(UTB_TIMEOUT);
    }
    switch (s_tdc_ut_bridge_phase)
    {
        case BR_OPEN:
            if (!read_mb(UTB_CMD))
            {
                if (!identity() || (read_mb(UTB_STATE) != UTB_RUNNING && read_mb(UTB_STATE) != UTB_DATA))
                { fail(UTB_PROTOCOL); break; }
                boundary_line(" BEGIN"); s_tdc_ut_bridge_phase = BR_BEGIN; s_tdc_ut_bridge_wait = 0U;
            }
            break;
        case BR_BEGIN:
            if (drain_line()) { s_tdc_ut_bridge_phase = BR_WAIT; s_tdc_ut_bridge_wait = 0U; }
            break;
        case BR_WAIT:
            if (!identity()) { fail(UTB_PROTOCOL); break; }
            state = read_mb(UTB_STATE);
            if (state == UTB_DATA)
            {
                if (!descriptor()) { fail(UTB_PROTOCOL); break; }
                s_tdc_ut_bridge_part = s_tdc_ut_bridge_part_label = 0U;
                s_tdc_ut_bridge_phase = BR_REPORT; s_tdc_ut_bridge_wait = 0U;
            }
            else if (state == UTB_DONE)
            {
                if (s_tdc_ut_bridge_completed != s_tdc_ut_bridge_total || read_mb(UTB_SEQ) != s_tdc_ut_bridge_total ||
                    read_mb(UTB_COMPLETED) != s_tdc_ut_bridge_total || read_mb(UTB_CMD))
                { fail(UTB_PROTOCOL); break; }
                boundary_line(" END"); s_tdc_ut_bridge_phase = BR_END; s_tdc_ut_bridge_wait = 0U;
            }
            else if (state != UTB_RUNNING) fail(UTB_PROTOCOL);
            break;
        case BR_REPORT: report_step(); break;
        case BR_ACK:
            if (!identity()) { fail(UTB_PROTOCOL); break; }
            if (!read_mb(UTB_CMD))
            {
                if (read_mb(UTB_COMPLETED) != s_tdc_ut_bridge_completed + 1U) { fail(UTB_PROTOCOL); break; }
                ++s_tdc_ut_bridge_completed;
                s_tdc_ut_bridge_phase = BR_WAIT; s_tdc_ut_bridge_wait = 0U;
            }
            break;
        case BR_END:
            if (drain_line()) { s_tdc_ut_bridge_phase = BR_IDLE; return 1; }
            break;
        case BR_CANCEL: cancel_step(); break;
        case BR_ABORT_WAIT:
            /* Never write to a replaced image or a different session. */
            if (!envelope()) { error_line(); break; }
            if (!read_mb(UTB_CMD) && read_mb(UTB_SESSION) == s_tdc_ut_bridge_session &&
                read_mb(UTB_GROUP) == s_tdc_ut_bridge_group)
            {
                if (!s_tdc_ut_bridge_abort_sent)
                    s_tdc_ut_bridge_abort_sent = (unsigned)command(UTB_ABORT, read_mb(UTB_SEQ));
                else if (read_mb(UTB_STATE) == UTB_ABORTED && read_mb(UTB_RESTORED))
                { error_line(); break; }
            }
            if (++s_tdc_ut_bridge_wait >= s_tdc_ut_bridge_limit) error_line();
            break;
        case BR_ERROR_LINE:
            if (drain_line()) { s_tdc_ut_bridge_phase = BR_IDLE; return -(int)s_tdc_ut_bridge_error; }
            break;
        default: fail(UTB_PROTOCOL); break;
    }
    return 0;
}
int tdc_ut_cfx_bridge_run(unsigned poll_budget)
{
    unsigned i; int result = 0;
    if (!poll_budget || poll_budget > TDC_UT_BATCH_MAX_ID) return -UTB_BAD_ARGUMENT;
    for (i = 0U; i < poll_budget && !result; ++i) result = tdc_ut_cfx_bridge_poll();
    return result;
}
void tdc_ut_cfx_bridge_abort(void)
{ if (tdc_ut_cfx_bridge_busy() && s_tdc_ut_bridge_phase != BR_END) fail(UTB_CANCELLED); }
int tdc_ut_cfx_bridge_busy(void)
{ return s_tdc_ut_bridge_phase != BR_IDLE; }
unsigned tdc_ut_cfx_bridge_completed(void)
{ return s_tdc_ut_bridge_completed; }
unsigned tdc_ut_cfx_bridge_error(void)
{ return s_tdc_ut_bridge_error; }
int tdc_ut_cfx_bridge_restored(void)
{
    unsigned state;
    if (!envelope()) return 0;
    state = read_mb(UTB_STATE);
    return read_mb(UTB_RESTORED) == 1U &&
        (state == UTB_READY || state == UTB_DONE || state == UTB_ABORTED) && !read_mb(UTB_CMD);
}
int tdc_ut_cfx_bridge_ram_safe(void)
{
    /* An unused bridge must not block CM3-only tests. Once configured, a
     * missing/replaced mailbox is unsafe, as is a drained timeout with HEAR
     * still pending. Do not release the console while its last line drains. */
    if (!s_tdc_ut_bridge_mb) return 1;
    return !tdc_ut_cfx_bridge_busy() && tdc_ut_cfx_bridge_restored();
}

/* Separate KEEP input: the CM3 entry table belongs to another implementation.
 * Existing sections.ld already KEEP(*(.tdc_ut_entry)); no product linker edit. */
typedef void (*tdc_ut_cfx_bridge_entry_t)(void);
__attribute__((used, section(".tdc_ut_entry")))
static const tdc_ut_cfx_bridge_entry_t s_tdc_ut_cfx_bridge_entries[] = {
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_configure,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_begin,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_poll,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_run,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_abort,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_busy,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_completed,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_error,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_restored,
    (tdc_ut_cfx_bridge_entry_t)tdc_ut_cfx_bridge_ram_safe
};
#endif
