/* Protocol test, not a simulation of DSP math or a HEAR device pass.
 * The real worker, bridge, record reporter and RTT formatter are linked.
 * Runners are strict fixtures: one lease, immutable captures, actual-event
 * restoration, and intentional mismatch preserved in the output. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <hw.h>
#include "../../../src/1__cfx/test/tdc_ut_batch_cfx.h"
#include "../../../src/1__cfx/test/tdc_ut_vectors_cfx.h"
#include "../../../src/1__cfx/test/tdc_ut_io_cfx.h"
#include "../../../src/2__cm3/Cortex-M3-src/test/tdc_ut_batch_cfx_bridge.h"
#include "../../../src/2__cm3/Cortex-M3-src/test/tdc_ut_report.h"

uint32_t g_tdc_ut_batch_fixture_xram[0x14000];
int g_tdc_ut_cfx_vector_actual[528], g_tdc_ut_cfx_vector_expected[528];
int g_tdc_ut_cfx_numeric_actual[400];
int g_tdc_ut_cfx_vector_count, g_tdc_ut_cfx_vector_failed, g_tdc_ut_cfx_vector_overflow;
int g_tdc_ut_cfx_numeric_count;
volatile int g_tdc_ut_cfx_vector_done;
int g_tdc_ut_cfx_io_actual[768], g_tdc_ut_cfx_io_expected[768];
int g_tdc_ut_cfx_io_count, g_tdc_ut_cfx_io_failed, g_tdc_ut_cfx_io_overflow;
volatile int g_tdc_ut_cfx_io_done;
int g_tdc_ut_hear_fft[512], g_tdc_ut_hear_vmag[256];
int g_tdc_ut_hear_fft_count, g_tdc_ut_hear_vmag_count, g_tdc_ut_hear_block_exp;
int g_tdc_ut_hear_count, g_tdc_ut_hear_failed, g_tdc_ut_hear_overflow;
volatile int g_tdc_ut_hear_done, g_tdc_ut_hear_pending;
int g_tdc_ut_hear_target_needed, g_tdc_ut_hear_timed_out;
int g_tdc_ut_agc_override_enabled, g_tdc_ut_power_override_enabled, g_tdc_ut_io_override_enabled;
static unsigned executed, released, restored, hear_polls, hear_delay = 3, forced_error;
static unsigned active_unit, active_case, active_group;
static int hold_hear, stall_rtt;
static unsigned tx_calls, tx_bytes, max_write = 5U;
static char output[400000];
static const unsigned mailbox_word = 0x4100U;
static unsigned session = 0U, token = 71U;
static unsigned publication_command, publication_session, publication_group, publication_seq;
static unsigned publication_open, publication_ack, publication_abort, publication_fault, publication_samples;
static unsigned publication_schedule;
#define M g_tdc_ut_cfx_batch_mailbox
#define X (g_tdc_ut_batch_fixture_xram + mailbox_word)
static void sync_from_worker(void);
static void sync_to_worker(void);

void tdc_ut_batch_host_command_latched(unsigned command)
{
    assert(!publication_command && command != UTB_NONE && M[UTB_CMD] == command);
    publication_command = command;
    publication_session = M[UTB_CMD_SESSION]; publication_group = M[UTB_CMD_GROUP];
    publication_seq = M[UTB_CMD_SEQ];
    /* Schedule the receiver BEFORE any response field has changed too. */
    if (publication_schedule)
    {
        sync_from_worker();
        assert(tdc_ut_cfx_bridge_poll() == 0);
        assert(!tdc_ut_cfx_bridge_error()); sync_to_worker(); ++publication_samples;
    }
}
void tdc_ut_batch_host_response_write(unsigned word)
{
    if (!publication_command) return; /* Ordinary publication/start/cleanup. */
    if (word != UTB_CMD)
        assert(M[UTB_CMD] == publication_command); /* The old early-clear fails here. */
    else
    {
        assert(M[UTB_CMD] == UTB_NONE);
        if (M[UTB_ERROR])
        {
            assert(M[UTB_STATE] == UTB_FAULT || M[UTB_STATE] == UTB_CLEANUP || M[UTB_STATE] == UTB_ABORTED);
            if (publication_command == UTB_ABORT) ++publication_abort;
            else ++publication_fault;
        }
        else if (publication_command == UTB_OPEN)
        {
            assert(M[UTB_SESSION] == publication_session && M[UTB_GROUP] == publication_group);
            assert(M[UTB_READY_ACK] == M[UTB_TOKEN] && M[UTB_STATE] == UTB_RUNNING);
            assert(M[UTB_SEQ] == 0U && M[UTB_COMPLETED] == 0U);
            assert(M[UTB_TOTAL] == (publication_group == TDC_UT_BATCH_CFX ? 29U : 6U));
            ++publication_open;
        }
        else
        {
            assert(publication_command == UTB_ACK);
            assert(M[UTB_COMPLETED] == publication_seq);
            assert(M[UTB_STATE] == (publication_seq == M[UTB_TOTAL] ? UTB_DONE : UTB_RUNNING));
            assert(!g_tdc_ut_cfx_vector_done && !g_tdc_ut_cfx_io_done && !g_tdc_ut_hear_done);
            ++publication_ack;
        }
    }
    /* Model independent cores by polling the REAL CM3 bridge after EACH
     * volatile response write, including the final command-slot release.
     * OPEN/ACK must remain pending until the coherent response is committed. */
    if (publication_schedule)
    {
        unsigned before = tdc_ut_cfx_bridge_completed();
        sync_from_worker(); assert(tdc_ut_cfx_bridge_poll() == 0);
        assert(!tdc_ut_cfx_bridge_error());
        if (word != UTB_CMD) assert(tdc_ut_cfx_bridge_completed() == before);
        sync_to_worker(); ++publication_samples;
    }
    if (word == UTB_CMD) publication_command = UTB_NONE;
}

static void check_release_log(void)
{
    char suffix[96];
    if (M[UTB_ERROR]) return; /* Explicit abort may discard unsent records. */
    assert(M[UTB_STATE] == UTB_DATA);
    if (active_group == 1U && (active_unit == 23U || active_unit == 31U || active_unit == 32U))
    {
        snprintf(suffix,sizeof(suffix),"[UT-CFX-%03u] C10 OBSERVED N=3 F=0\r\n",active_unit);
        assert(strstr(output,suffix));
    }
    else if (active_group == 2U && active_unit == 2U)
    {
        snprintf(suffix,sizeof(suffix),"[UT-HEAR-002] C%u OBSERVED N=512 F=0\r\n",active_case);
        assert(strstr(output,suffix));
        snprintf(suffix,sizeof(suffix),"[UT-HEAR-002] C%u OBSERVED N=256 F=0\r\n",active_case);
        assert(strstr(output,suffix));
        snprintf(suffix,sizeof(suffix),"[UT-HEAR-002] C%u OBSERVED N=1 F=0\r\n",active_case);
        assert(strstr(output,suffix));
    }
    else
    {
        snprintf(suffix,sizeof(suffix),"[UT-%s-%03u] C1 %s N=3 F=%u\r\n",
            active_group == 2U ? "HEAR" : "CFX",active_unit,
            active_unit == 15U ? "FAIL" : "PASS",active_unit == 15U ? 1U : 0U);
        assert(strstr(output,suffix));
    }
}

unsigned tdc_ut_batch_host_word(const void *p)
{
    if (p == g_tdc_ut_cfx_vector_actual) return 0x5000;
    if (p == g_tdc_ut_cfx_vector_expected) return 0x5300;
    if (p == g_tdc_ut_cfx_numeric_actual) return 0x5600;
    if (p == g_tdc_ut_cfx_io_actual) return 0x6000;
    if (p == g_tdc_ut_cfx_io_expected) return 0x6400;
    if (p == g_tdc_ut_hear_fft) return 0x6800;
    if (p == g_tdc_ut_hear_vmag) return 0x6c00;
    assert(!"unknown fixture pointer"); return 0;
}
unsigned SEGGER_RTT_Write(unsigned channel, const void *data, unsigned bytes)
{
    unsigned n;
    assert(channel == 0 && bytes <= 60U);
    if (stall_rtt || (tx_calls++ % 4U) == 0U) return 0U;
    n = bytes > max_write ? max_write : bytes;
    assert(tx_bytes + n < sizeof(output));
    memcpy(output + tx_bytes, data, n); tx_bytes += n; output[tx_bytes] = 0;
    return n;
}
static void no_lease(void)
{
    assert(!g_tdc_ut_cfx_vector_done && !g_tdc_ut_cfx_io_done &&
        !g_tdc_ut_hear_pending && !g_tdc_ut_hear_done);
}
static void fill(int *a, int *e, unsigned unit, unsigned vector)
{
    a[0] = e[0] = -999; a[1] = e[1] = (int)unit;
    a[2] = e[2] = (int)vector; a[3] = e[3] = 3;
    a[4] = e[4] = -8388608; a[5] = e[5] = -1; a[6] = e[6] = 8388607;
    if (unit == 15) a[5] = 0; /* A known mismatch must not stop the batch. */
}
int tdc_ut_cfx_run_unit(int unit)
{
    no_lease(); ++executed; active_unit = (unsigned)unit; active_group = 1U;
    if (forced_error) return -1;
    fill(g_tdc_ut_cfx_vector_actual, g_tdc_ut_cfx_vector_expected, (unsigned)unit, 1U);
    g_tdc_ut_cfx_vector_count = 7; g_tdc_ut_cfx_vector_failed = g_tdc_ut_cfx_vector_overflow = 0;
    g_tdc_ut_cfx_numeric_count = 0;
    if (unit == 23 || unit == 31 || unit == 32)
    {
        const int raw[] = {-999,unit,10,3,-20,-21,-19};
        memcpy(g_tdc_ut_cfx_numeric_actual, raw, sizeof(raw)); g_tdc_ut_cfx_numeric_count = 7;
    }
    g_tdc_ut_cfx_vector_done = 1; return 0;
}
void tdc_ut_cfx_vectors_release(void)
{
    check_release_log();
    assert(g_tdc_ut_cfx_vector_done); g_tdc_ut_cfx_vector_done = 0; ++released;
}
int tdc_ut_cfx_io_run_unit(int unit)
{
    no_lease(); ++executed; active_unit = (unsigned)unit; active_group = 1U;
    fill(g_tdc_ut_cfx_io_actual, g_tdc_ut_cfx_io_expected, (unsigned)unit, 1U);
    g_tdc_ut_cfx_io_count = 7; g_tdc_ut_cfx_io_failed = unit == 15;
    g_tdc_ut_cfx_io_overflow = 0; g_tdc_ut_cfx_io_done = 1; return 0;
}
int tdc_ut_cfx_io_release(void)
{
    check_release_log();
    assert(g_tdc_ut_cfx_io_done); g_tdc_ut_cfx_io_done = 0; ++released; return 0;
}
int tdc_ut_hear_start(int unit, int vector, int hardware_ready)
{
    no_lease(); assert(hardware_ready == 1); ++executed;
    active_unit = (unsigned)unit; active_case = (unsigned)vector; active_group = 2U;
    hear_polls = 0U; g_tdc_ut_hear_pending = 1; g_tdc_ut_hear_timed_out = 0;
    return 0;
}
int tdc_ut_hear_poll(void)
{
    assert(g_tdc_ut_hear_pending);
    if (hold_hear || ++hear_polls < hear_delay) return 1;
    fill(g_tdc_ut_cfx_io_actual, g_tdc_ut_cfx_io_expected, active_unit, active_case);
    g_tdc_ut_hear_count = 7; g_tdc_ut_hear_failed = g_tdc_ut_hear_overflow = 0;
    g_tdc_ut_hear_fft_count = active_unit == 2U ? 512 : 0;
    g_tdc_ut_hear_vmag_count = active_unit == 2U ? 256 : 0;
    g_tdc_ut_hear_block_exp = 4;
    for (int i = 0; i < 512; ++i) g_tdc_ut_hear_fft[i] = i == 511 ? -8388608 : i - 255;
    for (int i = 0; i < 256; ++i) g_tdc_ut_hear_vmag[i] = i;
    g_tdc_ut_hear_pending = 0; g_tdc_ut_hear_done = 1; ++restored;
    return 0;
}
int tdc_ut_hear_release(void)
{
    check_release_log();
    assert(!g_tdc_ut_hear_pending && g_tdc_ut_hear_done);
    g_tdc_ut_hear_done = 0; ++released; return 0;
}
static void copy_words(unsigned word, const int *p, unsigned n)
{ for (unsigned i = 0; i < n; ++i) g_tdc_ut_batch_fixture_xram[word + i] = (uint32_t)p[i] & 0xffffffU; }
static void sync_from_worker(void)
{
    for (unsigned i = 0; i < UTB_WORD_COUNT; ++i) X[i] = M[i];
    copy_words(0x5000,g_tdc_ut_cfx_vector_actual,528); copy_words(0x5300,g_tdc_ut_cfx_vector_expected,528);
    copy_words(0x5600,g_tdc_ut_cfx_numeric_actual,400);
    copy_words(0x6000,g_tdc_ut_cfx_io_actual,768); copy_words(0x6400,g_tdc_ut_cfx_io_expected,768);
    copy_words(0x6800,g_tdc_ut_hear_fft,512); copy_words(0x6c00,g_tdc_ut_hear_vmag,256);
}
static void sync_to_worker(void)
{ for (unsigned i = UTB_CMD; i <= UTB_CMD_LIMIT; ++i) M[i] = X[i]; }
static int step(void)
{
    (void)tdc_ut_cfx_batch_poll(); sync_from_worker();
    int result = tdc_ut_cfx_bridge_poll(); sync_to_worker(); return result;
}
static int finish(void)
{
    int result = 0;
    for (unsigned i = 0; i < 200000U && !result; ++i) result = step();
    assert(result && !tdc_ut_cfx_bridge_busy());
    if (result == 1) assert(tdc_ut_cfx_bridge_ram_safe());
    return result;
}
static void new_arm(int hear_ready)
{
    assert(!tdc_ut_cfx_bridge_busy());
    assert(tdc_ut_cfx_batch_start(++token, 1, hear_ready) == 0); sync_from_worker();
    assert(tdc_ut_cfx_bridge_configure(mailbox_word, token) == 0);
    assert(tdc_ut_cfx_bridge_ram_safe());
    session = 0U;
}
static void begin(unsigned group, unsigned limit)
{
    tx_bytes = tx_calls = 0U; output[0] = 0;
    assert(tdc_ut_cfx_bridge_begin(group, ++session, limit) == 0); sync_to_worker();
    assert(!tdc_ut_cfx_bridge_ram_safe());
}
static void test_ram_safe_configured(void)
{
    static const unsigned states[] = {
        UTB_OFF, UTB_READY, UTB_RUNNING, UTB_DATA, UTB_DONE,
        UTB_FAULT, UTB_CLEANUP, UTB_ABORTED, 99U
    };
    static const unsigned invalid_fields[][2] = {
        {UTB_MAGIC,0U}, {UTB_VERSION,99U}, {UTB_WORDS,33U},
        {UTB_TOKEN,0U}, {UTB_READY_FLAGS,0U}, {UTB_READY_FLAGS,7U},
        {UTB_RESTORED,0U}, {UTB_RESTORED,2U}, {UTB_CMD,UTB_OPEN}
    };
    new_arm(1);
    for (unsigned i = 0; i < sizeof(states)/sizeof(states[0]); ++i)
    {
        X[UTB_STATE] = states[i];
        assert(tdc_ut_cfx_bridge_ram_safe() ==
            (states[i] == UTB_READY || states[i] == UTB_DONE || states[i] == UTB_ABORTED));
        sync_from_worker();
    }
    for (unsigned i = 0; i < sizeof(invalid_fields)/sizeof(invalid_fields[0]); ++i)
    {
        X[invalid_fields[i][0]] = invalid_fields[i][1];
        assert(!tdc_ut_cfx_bridge_ram_safe());
        /* Failed reconfiguration must not erase the previous configured
         * state and accidentally reopen the unconfigured CM3-only path. */
        assert(tdc_ut_cfx_bridge_configure(0U,token) == -UTB_BAD_ARGUMENT);
        assert(!tdc_ut_cfx_bridge_ram_safe());
        sync_from_worker(); assert(tdc_ut_cfx_bridge_ram_safe());
    }
    X[UTB_TOKEN] = token + 1U;
    assert(!tdc_ut_cfx_bridge_ram_safe());
    assert(tdc_ut_cfx_bridge_configure(mailbox_word,token) == -UTB_NOT_READY);
    assert(!tdc_ut_cfx_bridge_ram_safe()); sync_from_worker();
}
static unsigned occurrences(const char *needle)
{
    unsigned n = 0U; const char *p = output;
    while ((p = strstr(p, needle))) { ++n; p += strlen(needle); }
    return n;
}
static void check_lines(void)
{
    const char *p = output;
    while (*p)
    {
        const char *end = strstr(p, "\r\n"); assert(end && end + 2 - p <= 60);
        for (const char *q = p; q < end; ++q) assert((unsigned char)*q < 128 && *q >= 32);
        p = end + 2;
    }
}
static void request_abort(void)
{
    tdc_ut_cfx_bridge_abort(); assert(tdc_ut_cfx_bridge_busy());
    assert(finish() < 0); check_lines(); assert(!strstr(output," END N="));
    assert(strstr(output," ERROR E="));
}
static void test_full_groups(void)
{
    unsigned before = executed, releases = released;
    new_arm(1);
    publication_schedule = 1U;
    assert(tdc_ut_cfx_bridge_begin(1,0,9999) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_bridge_begin(3,1,9999) == -UTB_BAD_ARGUMENT);
    begin(TDC_UT_BATCH_CFX,1000000U);
    assert(tdc_ut_cfx_bridge_begin(1,2,9999) == -UTB_BUSY);
    assert(tdc_ut_cfx_bridge_configure(mailbox_word,token) == -UTB_BUSY);
    assert(tdc_ut_cfx_batch_start(token+1U,1,1) == -UTB_BUSY);
    assert(finish() == 1);
    assert(executed-before == 29U && released-releases == 29U);
    assert(tdc_ut_cfx_bridge_completed() == 29U && tdc_ut_cfx_bridge_restored());
    assert(occurrences("CORE=CFX BEGIN N=29") == 1U && occurrences("CORE=CFX END N=29") == 1U);
    assert(strstr(output,"[UT-CFX-015] C1 FAIL N=3 F=1"));
    assert(occurrences(" RAW=NUMERIC ") == 3U && occurrences("C10 OBSERVED") == 3U);
    assert(strstr(output,"-8388608,-1,8388607")); check_lines();
    assert(tdc_ut_cfx_bridge_begin(1,session,1000) == -UTB_PROTOCOL);
    before = executed; releases = released;
    begin(TDC_UT_BATCH_HEAR,1000000U); assert(finish() == 1);
    assert(executed-before == 6U && released-releases == 6U);
    assert(tdc_ut_cfx_bridge_completed() == 6U && tdc_ut_cfx_bridge_restored());
    assert(occurrences(" RAW=FFT ") == 5U && occurrences(" RAW=VMAG ") == 5U && occurrences(" RAW=EXPONENT ") == 5U);
    assert(occurrences("OBSERVED N=512") == 5U && occurrences("OBSERVED N=256") == 5U);
    assert(occurrences("OBSERVED N=1") == 5U);
    assert(strstr(output,"CORE=HEAR END N=6")); check_lines();
    /* Same service/arm supports another RAM group without CFX start. */
    begin(TDC_UT_BATCH_CFX,1000000U); assert(finish() == 1);
    assert(tdc_ut_cfx_batch_service(2U,3U) == 0);
    publication_schedule = 0U;
    assert(publication_open == 3U && publication_ack == 64U && publication_samples > 200U);
}
static void test_preflight(void)
{
    static const int report[] = {-999,1,1,1,0};
    new_arm(0);
    assert(tdc_ut_report_begin("CM3",report,report,5U,5U,1,0) == 0);
    assert(tdc_ut_cfx_bridge_begin(1,1,9999) == -UTB_BUSY);
    assert(!tdc_ut_cfx_bridge_busy() && M[UTB_CMD] == 0U);
    tdc_ut_report_cancel(); assert(tdc_ut_report_poll() == 1);
    assert(tdc_ut_cfx_bridge_configure(0,token) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_bridge_configure(0x13fff,token) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_bridge_configure(mailbox_word,token+1) == -UTB_NOT_READY);
    assert(tdc_ut_cfx_bridge_begin(2,1,9999) == -UTB_NOT_READY);
    X[UTB_VERSION] = 9;
    assert(tdc_ut_cfx_bridge_begin(1,1,9999) == -UTB_NOT_READY); sync_from_worker();
    assert(tdc_ut_cfx_batch_start(token,1,0) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_batch_start(token+1,0,0) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_batch_run(0) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_bridge_run(0) == -UTB_BAD_ARGUMENT);
    g_tdc_ut_cfx_vector_done = 1;
    assert(tdc_ut_cfx_batch_start(token+1,1,0) == -UTB_BUSY);
    g_tdc_ut_cfx_vector_done = 0;
}
static void test_restart_after_abort_publication(void)
{
    new_arm(1); begin(1,1000000U); (void)step();
    request_abort();
    assert(M[UTB_STATE] == UTB_ABORTED && M[UTB_ERROR] == UTB_CANCELLED);
    assert(tdc_ut_cfx_bridge_ram_safe());
    begin(1,1000000U);
    /* CM3 may poll repeatedly before CFX has consumed the new OPEN. The
     * old session's CANCELLED is not a response to this committed request. */
    for (unsigned i = 0; i < 10U; ++i)
    {
        assert(tdc_ut_cfx_bridge_poll() == 0);
        assert(!tdc_ut_cfx_bridge_error());
        assert(X[UTB_CMD] == UTB_OPEN && X[UTB_ERROR] == UTB_CANCELLED);
    }
    publication_schedule = 1U;
    assert(finish() == 1);
    publication_schedule = 0U;
    assert(occurrences("CORE=CFX BEGIN N=29") == 1U);
    assert(occurrences("CORE=CFX END N=29") == 1U);
    assert(!strstr(output," ERROR E=")); check_lines();
}
static void test_lease_cancel(void)
{
    new_arm(1); begin(1,1000000U);
    (void)step(); assert(M[UTB_STATE] == UTB_DATA);
    unsigned releases = released, calls = executed;
    stall_rtt = 1;
    for (unsigned i = 0; i < 50; ++i) assert(step() == 0);
    assert(executed == calls && released == releases && M[UTB_SEQ] == 1U);
    assert(g_tdc_ut_cfx_vector_done && tdc_ut_cfx_bridge_busy());
    tdc_ut_cfx_bridge_abort();
    for (unsigned i = 0; i < 25; ++i) assert(step() == 0);
    /* No terminal error while the error line is stalled. */
    assert(tdc_ut_cfx_bridge_busy() && tdc_ut_cfx_bridge_restored());
    assert(!tdc_ut_cfx_bridge_ram_safe()); stall_rtt = 0;
    assert(finish() == -UTB_CANCELLED); check_lines();
    assert(tdc_ut_cfx_bridge_ram_safe());
    assert(released == releases+1U && !strstr(output," END N="));
    begin(1,1000000U);
    while (tx_bytes < 8U) assert(step() == 0); /* partial BEGIN */
    request_abort(); assert(tdc_ut_cfx_bridge_restored());
    begin(1,1000000U);
    while (!strstr(output,"[UT-CFX-008]")) assert(step() == 0); /* partial record */
    request_abort(); assert(tdc_ut_cfx_bridge_restored());
}
static void test_bad_descriptor(unsigned index, unsigned value)
{
    new_arm(1); begin(1,1000000U);
    (void)step(); assert(M[UTB_STATE] == UTB_DATA);
    unsigned releases = released;
    M[index] = value;
    assert(finish() < 0 && released == releases+1U);
    assert(!strstr(output," END N=")); check_lines();
}
static void test_stale_ack(void)
{
    new_arm(1); begin(1,1000000U); (void)step();
    unsigned releases = released;
    M[UTB_CMD_TOKEN] = token; M[UTB_CMD_SESSION] = session-1;
    M[UTB_CMD_GROUP] = 1; M[UTB_CMD_SEQ] = 1; M[UTB_CMD] = UTB_ACK;
    assert(tdc_ut_cfx_batch_poll() == -UTB_PROTOCOL);
    assert(released == releases && g_tdc_ut_cfx_vector_done);
    sync_from_worker(); assert(finish() == -UTB_PROTOCOL); check_lines();
}
static void test_timeout_late_hear(void)
{
    new_arm(1); hold_hear = 1; begin(2,1000U);
    unsigned before = restored, releases = released;
    for (unsigned i = 0; i < 1100U; ++i) (void)step();
    assert(g_tdc_ut_hear_pending && M[UTB_STATE] == UTB_CLEANUP);
    assert(restored == before && released == releases && !tdc_ut_cfx_bridge_restored());
    assert(tdc_ut_cfx_batch_start(token+1,1,1) == -UTB_BUSY);
    /* Late actual event restores RAM, then abort can release, never END. */
    hold_hear = 0; assert(finish() < 0);
    assert(restored == before+1U && released == releases+1U && tdc_ut_cfx_bridge_restored());
    assert(!strstr(output," END N=")); check_lines();
    begin(2,1000000U); hold_hear = 1; (void)step();
    tdc_ut_cfx_bridge_abort();
    for (unsigned i = 0; i < 100U; ++i) assert(step() == 0);
    assert(g_tdc_ut_hear_pending); hold_hear = 0; assert(finish() == -UTB_CANCELLED);
}
static void test_ack_timeout(void)
{
    new_arm(1); begin(1,1000U); (void)step();
    unsigned releases = released;
    /* CM3 halted: CFX alone must time out but never reuse the result. */
    for (unsigned i = 0; i < 1100U; ++i) (void)tdc_ut_cfx_batch_poll();
    assert(M[UTB_STATE] == UTB_FAULT && released == releases && g_tdc_ut_cfx_vector_done);
    assert(finish() == -UTB_TIMEOUT && released == releases+1U);
    check_lines();
}
static void test_missing_hear_and_late_recovery(void)
{
    new_arm(1); begin(2,100U); hold_hear = 1;
    unsigned before = released, saved = restored;
    assert(finish() == -UTB_TIMEOUT);
    assert(!tdc_ut_cfx_bridge_busy() && !tdc_ut_cfx_bridge_restored());
    assert(!tdc_ut_cfx_bridge_ram_safe());
    assert(g_tdc_ut_hear_pending && released == before && restored == saved);
    assert(strstr(output,"RESTORED=0\r\n") && !strstr(output," END N=")); check_lines();
    assert(tdc_ut_cfx_bridge_configure(mailbox_word,token) == -UTB_NOT_READY);
    assert(!tdc_ut_cfx_bridge_ram_safe());
    hold_hear = 0;
    assert(tdc_ut_cfx_batch_service(2U,5U) == 0); sync_from_worker();
    assert(tdc_ut_cfx_bridge_restored() && released == before+1U && restored == saved+1U);
    assert(tdc_ut_cfx_bridge_ram_safe());
    assert(tdc_ut_cfx_bridge_configure(mailbox_word,token) == 0);
    begin(1,1000000U); assert(finish() == 1);
}
static void test_bad_record(void)
{
    new_arm(1); begin(1,1000000U); (void)step();
    g_tdc_ut_cfx_vector_actual[3] = 1000;
    assert(finish() == -UTB_PROTOCOL);
    assert(!strstr(output,"PASS") && !strstr(output," END N=")); check_lines();
}
static void test_runner_failure(void)
{
    new_arm(1); forced_error = 1; begin(1,1000000U);
    assert(finish() == -UTB_RUNNER_ERROR);
    forced_error = 0;
    assert(!strstr(output," END N=")); check_lines(); no_lease();
}
int main(void)
{
    /* No successful configuration yet: allow independent CM3-only work. */
    assert(!tdc_ut_cfx_bridge_restored() && tdc_ut_cfx_bridge_ram_safe());
    assert(tdc_ut_cfx_bridge_configure(0U,token) == -UTB_BAD_ARGUMENT);
    assert(tdc_ut_cfx_bridge_ram_safe());
    test_ram_safe_configured();
    test_full_groups(); test_preflight(); test_restart_after_abort_publication(); test_lease_cancel();
    test_bad_descriptor(UTB_ACTUAL,0U);
    test_bad_descriptor(UTB_ACTUAL,0x13fffU);
    test_bad_descriptor(UTB_EXPECTED,0x5000U);
    test_bad_descriptor(UTB_COUNT,529U);
    test_bad_descriptor(UTB_UNIT,9U);
    test_bad_descriptor(UTB_SEQ,2U);
    test_bad_descriptor(UTB_NUMERIC_COUNT,401U);
    test_bad_descriptor(UTB_FFT_COUNT,512U);
    test_stale_ack(); test_timeout_late_hear(); test_ack_timeout();
    test_missing_hear_and_late_recovery(); test_bad_record(); test_runner_failure();
    no_lease();
    assert(active_group && active_unit);
    assert(publication_abort && publication_fault && publication_command == UTB_NONE);
    assert(UTB_WORD_COUNT == 34 && sizeof(g_tdc_ut_cfx_batch_mailbox) / sizeof(g_tdc_ut_cfx_batch_mailbox[0]) == 34U);
    puts("CFX_BATCH HOST_PASS: 29+6, response publication interleavings, RAM safety guard, repeat, lease, raw, partial, abort, stale, bounds, late HEAR");
    return 0;
}
