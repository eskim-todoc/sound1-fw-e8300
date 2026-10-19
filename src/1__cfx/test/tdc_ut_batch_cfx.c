/* Foreground test worker: run one immutable result, wait for exact ACK, then
 * reuse the EXISTING result arrays. The RAM and HEAR groups are independent.
 * No product function is automatically called on boot or from an ISR.
 */
#if defined(TDC_UNIT_TEST)
#include <hw.h>
#include "tdc_ut_batch_cfx.h"
#include "tdc_ut_vectors_cfx.h"
#include "tdc_ut_io_cfx.h"
#include "tdc_ut_hooks.h"

volatile unsigned int _XMEM g_tdc_ut_cfx_batch_mailbox[UTB_WORD_COUNT];
volatile unsigned int g_tdc_ut_cfx_batch_service_stop;
static const int s_tdc_ut_batch_ids[TDC_UT_BATCH_CFX_COUNT] = {
    TDC_UT_BATCH_VECTOR_IDS, TDC_UT_BATCH_IO_IDS
};
static unsigned s_tdc_ut_batch_token, s_tdc_ut_batch_session;
static unsigned s_tdc_ut_batch_group, s_tdc_ut_batch_seq, s_tdc_ut_batch_limit;
static unsigned s_tdc_ut_batch_wait, s_tdc_ut_batch_error, s_tdc_ut_batch_state;
static unsigned s_tdc_ut_batch_flags, s_tdc_ut_batch_abort;
/* Lease: 0=none, 1=vector/numeric, 2=IO, 3=HEAR (pending or completed). */
static unsigned s_tdc_ut_batch_lease;
#define MB g_tdc_ut_cfx_batch_mailbox

#if defined(TDC_UT_BATCH_HOST_CFX)
/* Only the host fixture supplies the address translation. The target cast
 * preserves the CFX word address; it must never be multiplied by four here. */
extern unsigned tdc_ut_batch_host_word(const void *p);
extern void tdc_ut_batch_host_command_latched(unsigned command);
extern void tdc_ut_batch_host_response_write(unsigned word);
#define WORD(p) tdc_ut_batch_host_word(p)
#define RESPONSE(word, value) do { MB[word] = (value); tdc_ut_batch_host_response_write(word); } while (0)
#else
#define WORD(p) ((unsigned int)(p))
#define RESPONSE(word, value) do { MB[word] = (value); } while (0)
#endif

static void set_state(unsigned state)
{
    s_tdc_ut_batch_state = state;
    /* Volatile commit is last. XMEM is uncached, descriptors are single-writer
     * and never changed while DATA is outstanding. Result-producing calls
     * finish before this commit; CM3 reads the uncached unsigned window. */
    RESPONSE(UTB_STATE, state);
}
static int result_busy(void)
{
    return g_tdc_ut_cfx_vector_done || g_tdc_ut_cfx_io_done ||
        g_tdc_ut_hear_pending || g_tdc_ut_hear_done ||
        g_tdc_ut_agc_override_enabled || g_tdc_ut_power_override_enabled ||
        g_tdc_ut_io_override_enabled;
}
static void fault(unsigned error)
{
    if (!s_tdc_ut_batch_error) s_tdc_ut_batch_error = error;
    RESPONSE(UTB_ERROR, s_tdc_ut_batch_error);
    set_state(g_tdc_ut_hear_pending ? UTB_CLEANUP : UTB_FAULT);
}
static int release_lease(void)
{
    int result = 0;
    if (s_tdc_ut_batch_lease == 1U) tdc_ut_cfx_vectors_release();
    else if (s_tdc_ut_batch_lease == 2U) result = tdc_ut_cfx_io_release();
    else if (s_tdc_ut_batch_lease == 3U) result = tdc_ut_hear_release();
    if (!result) s_tdc_ut_batch_lease = 0U;
    return result;
}
static void finish_abort(void)
{
    if (g_tdc_ut_hear_pending) { set_state(UTB_CLEANUP); return; }
    if (release_lease()) { fault(UTB_RUNNER_ERROR); return; }
    RESPONSE(UTB_RESTORED, 1U);
    set_state(UTB_ABORTED);
}
int tdc_ut_cfx_batch_start(unsigned token, int ram_ready, int hear_ready)
{
    unsigned i;
    if (!token || token > TDC_UT_BATCH_MAX_ID || token == s_tdc_ut_batch_token ||
        ram_ready != 1 || (hear_ready != 0 && hear_ready != 1)) return -UTB_BAD_ARGUMENT;
    if (result_busy() || s_tdc_ut_batch_lease || s_tdc_ut_batch_state == UTB_RUNNING ||
        s_tdc_ut_batch_state == UTB_DATA || s_tdc_ut_batch_state == UTB_CLEANUP ||
        (s_tdc_ut_batch_state == UTB_READY && MB[UTB_CMD])) return -UTB_BUSY;
    MB[UTB_MAGIC] = 0U;
    for (i = 1U; i < UTB_WORD_COUNT; ++i) MB[i] = 0U;
    s_tdc_ut_batch_token = token; s_tdc_ut_batch_session = 0U;
    s_tdc_ut_batch_group = s_tdc_ut_batch_seq = s_tdc_ut_batch_wait = 0U;
    s_tdc_ut_batch_error = s_tdc_ut_batch_abort = 0U;
    s_tdc_ut_batch_flags = TDC_UT_BATCH_RAM_READY | (hear_ready ? TDC_UT_BATCH_HEAR_READY : 0U);
    g_tdc_ut_cfx_batch_service_stop = 0U;
    MB[UTB_VERSION] = TDC_UT_BATCH_VERSION; MB[UTB_WORDS] = UTB_WORD_COUNT;
    MB[UTB_TOKEN] = token; MB[UTB_READY_FLAGS] = s_tdc_ut_batch_flags;
    MB[UTB_RESTORED] = 1U;
    set_state(UTB_READY);
    MB[UTB_MAGIC] = TDC_UT_BATCH_MAGIC;
    return 0;
}
/* Apply every success/error response while CMD still belongs to the sender.
 * Clearing CMD is the response commit, NOT an early "request read" receipt.
 * In particular OPEN identity/state and ACK completed/state must be visible
 * together before CM3 is allowed to observe CMD=NONE. */
static void apply_command(unsigned command, unsigned token, unsigned session,
                          unsigned group, unsigned seq, unsigned limit)
{
    if (token != s_tdc_ut_batch_token || !session || session > TDC_UT_BATCH_MAX_ID)
    { fault(UTB_PROTOCOL); return; }
    if (command == UTB_OPEN)
    {
        if ((s_tdc_ut_batch_state != UTB_READY && s_tdc_ut_batch_state != UTB_DONE &&
             s_tdc_ut_batch_state != UTB_ABORTED) || result_busy() || s_tdc_ut_batch_lease ||
            session <= s_tdc_ut_batch_session || seq != 0U ||
            (group != TDC_UT_BATCH_CFX && group != TDC_UT_BATCH_HEAR) ||
            !limit || limit > TDC_UT_BATCH_MAX_ID)
        { fault(UTB_PROTOCOL); return; }
        if (group == TDC_UT_BATCH_HEAR && !(s_tdc_ut_batch_flags & TDC_UT_BATCH_HEAR_READY))
        { fault(UTB_NOT_READY); return; }
        s_tdc_ut_batch_session = session; s_tdc_ut_batch_group = group;
        s_tdc_ut_batch_limit = limit; s_tdc_ut_batch_wait = s_tdc_ut_batch_seq = 0U;
        s_tdc_ut_batch_error = s_tdc_ut_batch_abort = 0U;
        RESPONSE(UTB_SESSION, session); RESPONSE(UTB_GROUP, group);
        RESPONSE(UTB_SEQ, 0U); RESPONSE(UTB_COMPLETED, 0U); RESPONSE(UTB_ERROR, 0U);
        RESPONSE(UTB_TOTAL, group == TDC_UT_BATCH_CFX ? TDC_UT_BATCH_CFX_COUNT : TDC_UT_BATCH_HEAR_COUNT);
        RESPONSE(UTB_READY_ACK, token);
        set_state(UTB_RUNNING);
        return;
    }
    if (session != s_tdc_ut_batch_session || group != s_tdc_ut_batch_group)
    { fault(UTB_PROTOCOL); return; }
    if (command == UTB_ABORT)
    {
        /* seq is deliberately ignored: cancellation may race the next DATA.
         * Session/token still must match. CM3 must close its partial line and
         * stop reading all arrays BEFORE issuing this command. */
        s_tdc_ut_batch_abort = 1U;
        if (!s_tdc_ut_batch_error) s_tdc_ut_batch_error = UTB_CANCELLED;
        RESPONSE(UTB_ERROR, s_tdc_ut_batch_error);
        if (g_tdc_ut_hear_pending) g_tdc_ut_hear_timed_out = 1;
        finish_abort();
        return;
    }
    if (command != UTB_ACK || s_tdc_ut_batch_state != UTB_DATA ||
        seq != s_tdc_ut_batch_seq || !seq || s_tdc_ut_batch_error)
    { fault(UTB_PROTOCOL); return; }
    if (release_lease()) { fault(UTB_RUNNER_ERROR); return; }
    RESPONSE(UTB_COMPLETED, s_tdc_ut_batch_seq);
    s_tdc_ut_batch_wait = 0U;
    set_state(s_tdc_ut_batch_seq == MB[UTB_TOTAL] ? UTB_DONE : UTB_RUNNING);
}
static void receive_command(void)
{
    unsigned command = MB[UTB_CMD];
    unsigned session, group, seq, limit, token;
    if (!command) return;
    token = MB[UTB_CMD_TOKEN]; session = MB[UTB_CMD_SESSION];
    group = MB[UTB_CMD_GROUP]; seq = MB[UTB_CMD_SEQ]; limit = MB[UTB_CMD_LIMIT];
#if defined(TDC_UT_BATCH_HOST_CFX)
    tdc_ut_batch_host_command_latched(command);
#endif
    apply_command(command, token, session, group, seq, limit);
    /* All response fields above are volatile XMEM writes. This final volatile
     * write releases the command slot; nothing from the response is published
     * after it. An invalid command commits its error/state in the same order. */
    RESPONSE(UTB_CMD, UTB_NONE);
}
static void publish_result(void)
{
    int count, overflow, numeric = 0, failed;
    unsigned capacity, actual, expected;
    if (s_tdc_ut_batch_lease == 1U)
    {
        count = g_tdc_ut_cfx_vector_count; overflow = g_tdc_ut_cfx_vector_overflow;
        failed = g_tdc_ut_cfx_vector_failed; numeric = g_tdc_ut_cfx_numeric_count;
        if (!g_tdc_ut_cfx_vector_done) { fault(UTB_RUNNER_ERROR); return; }
        capacity = TDC_UT_CFX_VECTOR_CAPACITY;
        actual = WORD(g_tdc_ut_cfx_vector_actual); expected = WORD(g_tdc_ut_cfx_vector_expected);
    }
    else
    {
        int hear = s_tdc_ut_batch_lease == 3U;
        count = hear ? g_tdc_ut_hear_count : g_tdc_ut_cfx_io_count;
        overflow = hear ? g_tdc_ut_hear_overflow : g_tdc_ut_cfx_io_overflow;
        failed = hear ? g_tdc_ut_hear_failed : g_tdc_ut_cfx_io_failed;
        if (!(hear ? g_tdc_ut_hear_done : g_tdc_ut_cfx_io_done))
        { fault(UTB_RUNNER_ERROR); return; }
        capacity = TDC_UT_CFX_IO_CAPACITY;
        actual = WORD(g_tdc_ut_cfx_io_actual); expected = WORD(g_tdc_ut_cfx_io_expected);
    }
    if (overflow || count < 1 || (unsigned)count > capacity || failed < 0 ||
        numeric < 0 || numeric > TDC_UT_CFX_NUMERIC_CAPACITY)
    { fault(UTB_RUNNER_ERROR); return; }
    MB[UTB_ACTUAL] = actual; MB[UTB_EXPECTED] = expected;
    MB[UTB_COUNT] = (unsigned)count; MB[UTB_CAPACITY] = capacity;
    MB[UTB_NUMERIC] = numeric ? WORD(g_tdc_ut_cfx_numeric_actual) : 0U;
    MB[UTB_NUMERIC_COUNT] = (unsigned)numeric;
    MB[UTB_FFT] = MB[UTB_FFT_COUNT] = MB[UTB_VMAG] = MB[UTB_VMAG_COUNT] = MB[UTB_EXPONENT] = 0U;
    if (s_tdc_ut_batch_group == TDC_UT_BATCH_HEAR && MB[UTB_UNIT] == 2U)
    {
        if (g_tdc_ut_hear_fft_count != 512 || g_tdc_ut_hear_vmag_count != 256)
        { fault(UTB_RUNNER_ERROR); return; }
        MB[UTB_FFT] = WORD(g_tdc_ut_hear_fft); MB[UTB_FFT_COUNT] = 512U;
        MB[UTB_VMAG] = WORD(g_tdc_ut_hear_vmag); MB[UTB_VMAG_COUNT] = 256U;
        /* Preserve the raw 24-bit exponent even when the existing reference
         * comparison rejects it. A transport must not hide a numerical FAIL. */
        MB[UTB_EXPONENT] = (unsigned)g_tdc_ut_hear_block_exp;
    }
    MB[UTB_FAILED] = (unsigned)failed;
    MB[UTB_SEQ] = ++s_tdc_ut_batch_seq;
    MB[UTB_RESTORED] = 1U;
    s_tdc_ut_batch_wait = 0U;
    set_state(UTB_DATA);
}
static void execute_next(void)
{
    int result, unit;
    if (s_tdc_ut_batch_group == TDC_UT_BATCH_HEAR)
    {
        unit = s_tdc_ut_batch_seq ? 2 : 1;
        MB[UTB_UNIT] = (unsigned)unit;
        MB[UTB_VECTOR] = s_tdc_ut_batch_seq ? s_tdc_ut_batch_seq : 1U;
        result = tdc_ut_hear_start(unit, (int)MB[UTB_VECTOR], 1);
        if (result) { fault(UTB_RUNNER_ERROR); return; }
        s_tdc_ut_batch_lease = 3U; MB[UTB_RESTORED] = 0U;
        s_tdc_ut_batch_wait = 0U;
        return;
    }
    unit = s_tdc_ut_batch_ids[s_tdc_ut_batch_seq];
    MB[UTB_UNIT] = (unsigned)unit; MB[UTB_VECTOR] = 0U;
    if (s_tdc_ut_batch_seq < TDC_UT_BATCH_VECTOR_COUNT)
    {
        result = tdc_ut_cfx_run_unit(unit);
        if (g_tdc_ut_cfx_vector_done) s_tdc_ut_batch_lease = 1U;
    }
    else
    {
        result = tdc_ut_cfx_io_run_unit(unit);
        if (g_tdc_ut_cfx_io_done) s_tdc_ut_batch_lease = 2U;
    }
    if (result) { fault(UTB_RUNNER_ERROR); return; }
    publish_result();
}
int tdc_ut_cfx_batch_poll(void)
{
    int result;
    if (!s_tdc_ut_batch_token) return -UTB_NOT_READY;
    if (MB[UTB_MAGIC] != TDC_UT_BATCH_MAGIC || MB[UTB_VERSION] != TDC_UT_BATCH_VERSION ||
        MB[UTB_WORDS] != UTB_WORD_COUNT || MB[UTB_TOKEN] != s_tdc_ut_batch_token ||
        MB[UTB_READY_FLAGS] != s_tdc_ut_batch_flags)
        fault(UTB_PROTOCOL);
    else receive_command();
    /* Abort/timeout cannot stop a launched HEAR chain. Only the real event
     * permits its existing runner to restore RAM and completion flags. */
    if (s_tdc_ut_batch_lease == 3U && g_tdc_ut_hear_pending)
    {
        result = tdc_ut_hear_poll();
        if (result == TDC_UT_IO_TIMEOUT ||
            (g_tdc_ut_hear_pending && ++s_tdc_ut_batch_wait >= s_tdc_ut_batch_limit))
        { g_tdc_ut_hear_timed_out = 1; fault(UTB_TIMEOUT); }
        else if (result < 0) fault(UTB_RUNNER_ERROR);
        if (!g_tdc_ut_hear_pending)
        {
            MB[UTB_RESTORED] = 1U;
            if (s_tdc_ut_batch_abort) finish_abort();
            else if (s_tdc_ut_batch_error) set_state(UTB_FAULT);
            else publish_result();
        }
        return s_tdc_ut_batch_state == UTB_FAULT ? -(int)s_tdc_ut_batch_error : 0;
    }
    if (s_tdc_ut_batch_state == UTB_RUNNING) execute_next();
    else if (s_tdc_ut_batch_state == UTB_DATA && ++s_tdc_ut_batch_wait >= s_tdc_ut_batch_limit)
        fault(UTB_TIMEOUT); /* Retain result lease until explicit CM3 ABORT. */
    if (s_tdc_ut_batch_state == UTB_DONE) return 1;
    if (s_tdc_ut_batch_state == UTB_FAULT || s_tdc_ut_batch_state == UTB_ABORTED)
        return -(int)s_tdc_ut_batch_error;
    return 0;
}
int tdc_ut_cfx_batch_run(unsigned poll_budget)
{
    unsigned i; int result = 0;
    if (!poll_budget || poll_budget > TDC_UT_BATCH_MAX_ID) return -UTB_BAD_ARGUMENT;
    for (i = 0U; i < poll_budget && !result; ++i) result = tdc_ut_cfx_batch_poll();
    return result;
}
int tdc_ut_cfx_batch_service(unsigned rounds, unsigned polls_per_round)
{
    unsigned round, poll;
    if (!s_tdc_ut_batch_token) return -UTB_NOT_READY;
    if (!rounds || !polls_per_round || rounds > TDC_UT_BATCH_MAX_ID ||
        polls_per_round > TDC_UT_BATCH_MAX_ID) return -UTB_BAD_ARGUMENT;
    for (round = 0U; round < rounds && !g_tdc_ut_cfx_batch_service_stop; ++round)
        for (poll = 0U; poll < polls_per_round && !g_tdc_ut_cfx_batch_service_stop; ++poll)
            (void)tdc_ut_cfx_batch_poll();
    return 0;
}
#undef MB
#undef WORD
#undef RESPONSE
#endif
