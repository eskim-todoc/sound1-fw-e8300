/* Safe BOOT RAM batch: serial ownership of the existing result/reporter pair.
 * No heap, extra vector copies or device initialization. Test image only.
 */
#if defined(TDC_UNIT_TEST)
#if defined(TDC_UNIT_TEST_HOST) && !defined(TDC_UNIT_TEST_RAM_ONLY)
#error "BOOT batch host checks must use TDC_UNIT_TEST_RAM_ONLY"
#endif
#include <SEGGER_RTT.h>
#include <stdio.h>
#include "tdc_ut_boot.h"
#include "tdc_ut_boot_report.h"
#include "tdc_ut_batch_boot.h"

enum { PH_BEGIN, PH_UNIT, PH_REPORT, PH_END, PH_STOP, PH_STOP_MARKER };
static const unsigned char s_all[] = {8,9,10,11,12,13,14,19,20};
/* Current target RAM vectors, not HOST-only extensions. Missing/duplicated
 * vectors are an incomplete batch, even if the producer posted done. */
static const unsigned char s_case_counts[] = {6,5,13,14,11,10,4,10,7};
static const unsigned char s_file[] = {8,9,10,11,12,19,20};
static const unsigned char s_crypto[] = {13,14};
static const unsigned char *s_units;
static tdc_ut_boot_batch_status_t s_status = {0,0,0,0,TDC_UT_BOOT_BATCH_IDLE};
static unsigned s_phase, s_owns_result, s_owns_report;
static int s_stop_status;
/* Longest marker is under 40 bytes, including CRLF. Unit lines use the
 * existing 60-byte logger. Only one marker and one unit line may be pending. */
static char s_marker[40];
static unsigned s_bytes, s_sent;

static int marker_begin(const char *kind, unsigned count)
{
    int n = snprintf(s_marker, sizeof(s_marker),
                     "[UT-BATCH] CORE=BOOT %s N=%u\r\n", kind, count);
    if (n < 0 || (unsigned)n >= sizeof(s_marker)) return -1;
    s_bytes = (unsigned)n;
    s_sent = 0;
    return 0;
}

static int marker_poll(void)
{
    unsigned n;
    if (s_bytes == 0) return 1;
    n = SEGGER_RTT_Write(0, s_marker + s_sent, s_bytes - s_sent);
    if (n > s_bytes - s_sent) return -1;
    s_sent += n;
    if (s_sent < s_bytes) return 0;
    s_bytes = s_sent = 0;
    return 1;
}

/* Called once per stop transition: retain only the CRLF needed to finish
 * an accepted prefix. Repeated cancel must not restart partial CRLF drain. */
static void marker_cancel(void)
{
    if (s_sent == 0) { s_bytes = 0; return; }
    if (s_sent == s_bytes - 1U) s_marker[s_sent] = '\n';
    else {
        s_marker[s_sent] = '\r';
        s_marker[s_sent + 1U] = '\n';
        s_bytes = s_sent + 2U;
    }
}

static void stop_begin(int reason)
{
    /* A transport error takes precedence over a user cancellation. */
    if (s_phase == PH_STOP || s_phase == PH_STOP_MARKER) {
        if (reason == TDC_UT_BOOT_BATCH_ERROR) s_stop_status = reason;
        return;
    }
    s_stop_status = reason;
    marker_cancel();
    if (s_owns_report) tdc_ut_boot_report_cancel();
    s_phase = PH_STOP;
}

static void release_owned(void)
{
    if (!s_owns_result) return;
    /* release() clears private pending only; done is NOT a busy indication.
     * Clear our public metadata as well, only after report drain/cancel drain.
     * Never clear or release data when run_unit returned BUSY. */
    tdc_ut_boot_release();
    g_tdc_ut_boot_done = 0;
    g_tdc_ut_boot_count = 0;
    g_tdc_ut_boot_overflow = 0;
    s_owns_result = s_owns_report = 0;
}

/* Check all framing before report_begin; mismatching VALUES are deliberately
 * allowed so the existing reporter emits FAIL and subsequent units still run.
 * Wrong unit IDs, empty/truncated records or invalid case IDs stop the batch. */
static int result_valid(unsigned unit)
{
    unsigned pos = 0, cases = 0, planned = 0;
    unsigned first = unit == 9 || unit == 14 ? 1U : 101U;
    for (unsigned i=0; i<sizeof(s_all); ++i)
        if (s_all[i] == unit) planned=s_case_counts[i];
    if (!g_tdc_ut_boot_done || g_tdc_ut_boot_overflow ||
        !g_tdc_ut_boot_count || g_tdc_ut_boot_count > TDC_UT_BOOT_CAPACITY) return 0;
    while (pos < g_tdc_ut_boot_count) {
        unsigned i, length;
        if (g_tdc_ut_boot_count - pos < 5U || g_tdc_ut_boot_actual[pos] != -999 ||
            g_tdc_ut_boot_actual[pos+1] != (int)unit ||
            g_tdc_ut_boot_actual[pos+2] != (int)(first+cases) ||
            g_tdc_ut_boot_actual[pos+3] < 1) return 0;
        for (i=0; i<4; ++i)
            if (g_tdc_ut_boot_actual[pos+i] != g_tdc_ut_boot_expected[pos+i]) return 0;
        length = (unsigned)g_tdc_ut_boot_actual[pos+3];
        if (length > g_tdc_ut_boot_count - pos - 4U) return 0;
        pos += 4U + length;
        ++cases;
    }
    return planned != 0 && cases == planned;
}

int tdc_ut_boot_batch_busy(void)
{ return s_status.status == TDC_UT_BOOT_BATCH_RUNNING; }

const tdc_ut_boot_batch_status_t *tdc_ut_boot_batch_get_status(void)
{ return &s_status; }

int tdc_ut_boot_batch_begin(unsigned group, int ready)
{
    const unsigned char *units;
    unsigned count;
    if (tdc_ut_boot_batch_busy()) return TDC_UT_BOOT_BATCH_BUSY;
    if (ready != 1) return TDC_UT_BOOT_BATCH_NOT_READY;
    switch (group) {
    case TDC_UT_BOOT_BATCH_ALL: units=s_all; count=sizeof(s_all); break;
    case TDC_UT_BOOT_BATCH_FILE: units=s_file; count=sizeof(s_file); break;
    case TDC_UT_BOOT_BATCH_CRYPTO: units=s_crypto; count=sizeof(s_crypto); break;
    default: return TDC_UT_BOOT_BATCH_BAD_GROUP;
    }
    if (tdc_ut_boot_report_busy() || g_tdc_ut_boot_done ||
        g_tdc_ut_boot_count || g_tdc_ut_boot_overflow) return TDC_UT_BOOT_BATCH_STALE;
    if (marker_begin("BEGIN", count) != 0) return TDC_UT_BOOT_BATCH_ERROR;
    s_units = units;
    s_status.group=group; s_status.total=count;
    s_status.completed=s_status.unit=0;
    s_status.status=TDC_UT_BOOT_BATCH_RUNNING;
    s_phase=PH_BEGIN; s_owns_result=s_owns_report=0;
    s_stop_status=TDC_UT_BOOT_BATCH_CANCELLED;
    return 0;
}

int tdc_ut_boot_batch_poll(void)
{
    int rc;
    if (!tdc_ut_boot_batch_busy()) return s_status.status;
    switch (s_phase) {
    case PH_BEGIN:
        rc=marker_poll();
        if (rc < 0) stop_begin(TDC_UT_BOOT_BATCH_ERROR);
        else if (rc == 1) s_phase=PH_UNIT;
        break;
    case PH_UNIT:
        /* The result producer and RTT stream belong exclusively to this
         * foreground sequence. Refuse newly discovered outside activity. */
        if (tdc_ut_boot_report_busy() || g_tdc_ut_boot_done ||
            g_tdc_ut_boot_count || g_tdc_ut_boot_overflow) {
            stop_begin(TDC_UT_BOOT_BATCH_ERROR); break;
        }
        s_status.unit=s_units[s_status.completed];
        rc=tdc_ut_boot_run_unit(s_status.unit);
        /* An overflow return still owns a completed result in run_unit(). */
        s_owns_result=(rc == 0 || (rc == TDC_UT_BOOT_ERROR && g_tdc_ut_boot_done));
        if (rc != 0 || !result_valid(s_status.unit)) {
            stop_begin(TDC_UT_BOOT_BATCH_ERROR); break;
        }
        if (tdc_ut_boot_report_begin() != 0) {
            stop_begin(TDC_UT_BOOT_BATCH_ERROR); break;
        }
        s_owns_report=1; s_phase=PH_REPORT;
        break;
    case PH_REPORT:
        rc=tdc_ut_boot_report_poll();
        if (rc < 0) stop_begin(TDC_UT_BOOT_BATCH_ERROR);
        else if (rc == 1) {
            release_owned(); ++s_status.completed;
            if (s_status.completed == s_status.total) {
                if (marker_begin("END",s_status.total) != 0)
                    stop_begin(TDC_UT_BOOT_BATCH_ERROR);
                else s_phase=PH_END;
            } else s_phase=PH_UNIT;
        }
        break;
    case PH_END:
        rc=marker_poll();
        if (rc < 0) stop_begin(TDC_UT_BOOT_BATCH_ERROR);
        else if (rc == 1) s_status.status=TDC_UT_BOOT_BATCH_DONE;
        break;
    case PH_STOP:
        if (s_bytes) {
            rc=marker_poll();
            if (rc < 0) s_stop_status=TDC_UT_BOOT_BATCH_ERROR;
            break;
        }
        if (s_owns_report) {
            rc=tdc_ut_boot_report_poll();
            if (rc < 0) {
                s_stop_status=TDC_UT_BOOT_BATCH_ERROR;
                tdc_ut_boot_report_cancel();
            }
            if (rc != 1) break;
        }
        release_owned();
        if (marker_begin(s_stop_status == TDC_UT_BOOT_BATCH_ERROR ? "ERROR" : "CANCEL",
                         s_status.completed) == 0) s_phase=PH_STOP_MARKER;
        else s_status.status=TDC_UT_BOOT_BATCH_ERROR;
        break;
    case PH_STOP_MARKER:
        rc=marker_poll();
        if (rc < 0) {
            /* A broken writer is not completion. Finish its partial marker,
             * then retry an ERROR marker after the transport recovers. */
            marker_cancel(); s_stop_status=TDC_UT_BOOT_BATCH_ERROR; s_phase=PH_STOP;
        } else if (rc == 1) s_status.status=s_stop_status;
        break;
    default:
        stop_begin(TDC_UT_BOOT_BATCH_ERROR);
        break;
    }
    return s_status.status;
}

int tdc_ut_boot_batch_run(unsigned poll_budget)
{
    int rc=s_status.status;
    while (poll_budget-- && rc == TDC_UT_BOOT_BATCH_RUNNING)
        rc=tdc_ut_boot_batch_poll();
    return rc;
}

void tdc_ut_boot_batch_cancel(void)
{
    if (!tdc_ut_boot_batch_busy()) return;
    if (s_phase == PH_END && s_sent != 0) return; /* END already committed. */
    stop_begin(TDC_UT_BOOT_BATCH_CANCELLED);
}

/* Existing linker KEEP(.tdc_ut_entry) retains explicit debugger/console APIs.
 * No product source or linker changes and no automatic call are required. */
typedef void (*tdc_ut_batch_boot_entry_t)(void);
__attribute__((used,section(".tdc_ut_entry")))
static const tdc_ut_batch_boot_entry_t s_tdc_ut_batch_boot_entries[]={
    (tdc_ut_batch_boot_entry_t)tdc_ut_boot_batch_begin,
    (tdc_ut_batch_boot_entry_t)tdc_ut_boot_batch_poll,
    (tdc_ut_batch_boot_entry_t)tdc_ut_boot_batch_run,
    (tdc_ut_batch_boot_entry_t)tdc_ut_boot_batch_cancel,
    (tdc_ut_batch_boot_entry_t)tdc_ut_boot_batch_busy,
    (tdc_ut_batch_boot_entry_t)tdc_ut_boot_batch_get_status
};
#endif
