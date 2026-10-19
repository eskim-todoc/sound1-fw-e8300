/* One owner, one existing result buffer, one RTT writer. Product state is
 * restored inside each existing RAM runner before its immutable log drains.
 * The batch never substitutes expected values for observations or failures.
 */
#if defined(TDC_UNIT_TEST)
#include <hw.h>
#include <SEGGER_RTT.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>
#include <stdio.h>
#include <string.h>
#include "tdc_ut_batch_cm3.h"
#include "tdc_ut_vectors_cm3.h"
#include "tdc_ut_io_cm3.h"
#include "tdc_ut_report.h"

#define TDC_BATCH_CAPTURE_CAPACITY 512U
#define TDC_BATCH_LINE_CAPACITY 60U
#define TDC_BATCH_FAILURE_OBSERVATION 129U
static const unsigned char s_tdc_calc[] = {
    4,6,7,9,11,12,13,16,17,18,19,20,21,22,23,24,27,28,29,30,44,
    TDC_BATCH_FAILURE_OBSERVATION
};
static const unsigned char s_tdc_io[] = {10,25,26,34,35,36,37,40,45,47,50,51};
static const unsigned char s_tdc_observe_device[] = {
    2,10,25,26,31,32,33,34,35,36,37,38,39,40,41,42,43
};
static const unsigned char s_tdc_observe_communication[] = {45,46,47,48,49,50,51};
/* Reserve the existing reporter without printing or changing its API. A
 * failed begin leaves another report untouched. This record is never polled.
 */
static const int s_tdc_reservation[] = {-999,1,1,1,0};
static int s_tdc_capture[TDC_BATCH_CAPTURE_CAPACITY];
static unsigned s_tdc_capture_count;
static unsigned s_tdc_group, s_tdc_selected, s_tdc_completed, s_tdc_index;
static unsigned s_tdc_marker_length, s_tdc_marker_sent;
static char s_tdc_marker[TDC_BATCH_LINE_CAPACITY + 1U];
static int s_tdc_capture_mode, s_tdc_result_owner, s_tdc_report_owner;
static int s_tdc_terminal = TDC_UT_BATCH_CM3_IDLE;
typedef enum {
    TDC_BATCH_IDLE, TDC_BATCH_BEGIN, TDC_BATCH_NEXT, TDC_BATCH_REPORTING,
    TDC_BATCH_END, TDC_BATCH_STOP_DRAIN, TDC_BATCH_STOP_MARK
} tdc_batch_phase_t;
static tdc_batch_phase_t s_tdc_phase;

static int foreground(void)
{
    return __get_IPSR() == 0U;
}

static int quiescent(int capture)
{
    if (cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration) return 0;
    return capture || (tdc_hal_spi_is_tx_buffer_empty() && tdc_hal_i2c_is_driver_status_idle() &&
                       g_tdc_ut_cm3_observer_ops == 0);
}

static int reserve_report(void)
{
    if (tdc_ut_report_begin("CM3", s_tdc_reservation, 0, 5, 5, 1, 0) != 0)
        return TDC_UT_BATCH_CM3_BUSY;
    s_tdc_report_owner = 1;
    return 0;
}

static int release_result(void)
{
    if (s_tdc_result_owner == 1) tdc_ut_cm3_vectors_release();
    if (s_tdc_result_owner == 2 && tdc_ut_io_cm3_release() != 0)
        return TDC_UT_BATCH_CM3_DATA;
    s_tdc_result_owner = 0;
    return 0;
}

static void marker(const char *event, unsigned count)
{
    int n = snprintf(s_tdc_marker, sizeof(s_tdc_marker),
                     "[UT-BATCH] CORE=CM3 %s N=%u\r\n", event, count);
    /* All arguments are fixed internal tokens and counts <=34. */
    s_tdc_marker_length = n > 0 && (unsigned)n <= TDC_BATCH_LINE_CAPACITY ? (unsigned)n : 0;
    s_tdc_marker_sent = 0;
}

static int marker_poll(void)
{
    unsigned left, sent;
    if (s_tdc_marker_sent == s_tdc_marker_length) return 1;
    left = s_tdc_marker_length - s_tdc_marker_sent;
    sent = SEGGER_RTT_Write(0, s_tdc_marker + s_tdc_marker_sent, left);
    if (sent > left) return TDC_UT_BATCH_CM3_REPORT;
    s_tdc_marker_sent += sent;
    return s_tdc_marker_sent == s_tdc_marker_length ? 1 : 0;
}

static void truncate_marker(void)
{
    /* Keep already accepted bytes, then terminate, including CR-only writes. */
    if (s_tdc_marker_sent == 0 || s_tdc_marker_sent == s_tdc_marker_length)
        s_tdc_marker_length = s_tdc_marker_sent = 0;
    else
    {
        s_tdc_marker_length = s_tdc_marker_sent;
        if (s_tdc_marker[s_tdc_marker_sent - 1U] != '\r')
            s_tdc_marker[s_tdc_marker_length++] = '\r';
        s_tdc_marker[s_tdc_marker_length++] = '\n';
    }
}

static void stop_batch(int tdc_sys_error)
{
    s_tdc_terminal = tdc_sys_error;
    truncate_marker();
    if (s_tdc_report_owner) tdc_ut_report_cancel();
    s_tdc_phase = TDC_BATCH_STOP_DRAIN;
}

static unsigned group_count(unsigned group, int capture)
{
    unsigned a = capture ? sizeof(s_tdc_observe_device) : sizeof(s_tdc_calc);
    unsigned b = capture ? sizeof(s_tdc_observe_communication) : sizeof(s_tdc_io);
    return group == 1 ? a + b : group == 2 ? a : b;
}

static unsigned group_unit(unsigned index, int *io)
{
    const unsigned char *a = s_tdc_capture_mode ? s_tdc_observe_device : s_tdc_calc;
    const unsigned char *b = s_tdc_capture_mode ? s_tdc_observe_communication : s_tdc_io;
    unsigned n = s_tdc_capture_mode ? sizeof(s_tdc_observe_device) : sizeof(s_tdc_calc);
    if (s_tdc_group == 3) { *io = 1; return b[index]; }
    if (index < n) { *io = 0; return a[index]; }
    *io = 1; return b[index - n];
}

static int begin(unsigned group, int ready, int capture)
{
    if (!foreground()) return TDC_UT_BATCH_CM3_UNSAFE;
    if (s_tdc_phase != TDC_BATCH_IDLE) return TDC_UT_BATCH_CM3_BUSY;
    if (group < 1 || group > 3) return TDC_UT_BATCH_CM3_INVALID;
    if (ready != 1 || !quiescent(capture)) return TDC_UT_BATCH_CM3_UNSAFE;
    if (g_tdc_ut_cm3_vector_done || g_tdc_ut_io_cm3_done) return TDC_UT_BATCH_CM3_BUSY;
    if (reserve_report() != 0) return TDC_UT_BATCH_CM3_BUSY;
    s_tdc_group = group; s_tdc_capture_mode = capture;
    s_tdc_selected = group_count(group, capture);
    s_tdc_completed = s_tdc_index = s_tdc_capture_count = 0;
    s_tdc_result_owner = 0; s_tdc_terminal = 0;
    marker("BEGIN", s_tdc_selected);
    s_tdc_phase = TDC_BATCH_BEGIN;
    return 0;
}

int tdc_ut_batch_cm3_begin(unsigned group, int ready)
{
    return begin(group, ready, 0);
}

int tdc_ut_batch_cm3_begin_capture(unsigned group, unsigned case_id, int ready)
{
    int rc;
    if (case_id < 1 || case_id > 999) return TDC_UT_BATCH_CM3_INVALID;
    rc = begin(group, ready, 1);
    if (rc != 0) return rc;
    for (unsigned i = 0; i < s_tdc_selected; ++i)
    {
        int io, count;
        unsigned unit = group_unit(i, &io);
        rc = tdc_ut_io_cm3_observe_unit((int)unit, (int)case_id);
        s_tdc_result_owner = g_tdc_ut_io_cm3_done ? 2 : 0;
        count = g_tdc_ut_io_cm3_count;
        if (rc != 0 || !g_tdc_ut_io_cm3_done || !g_tdc_ut_io_cm3_observed ||
            g_tdc_ut_io_cm3_overflow || count < 5 || count > TDC_UT_IO_CM3_CAPACITY ||
            (unsigned)count > TDC_BATCH_CAPTURE_CAPACITY - s_tdc_capture_count ||
            g_tdc_ut_io_cm3_actual[0] != -999 || g_tdc_ut_io_cm3_actual[1] != (int)unit ||
            g_tdc_ut_io_cm3_actual[2] != (int)case_id || g_tdc_ut_io_cm3_actual[3] != count - 4)
        {
            /* Capture failed before BEGIN was sent. Preserve no partial report. */
            tdc_ut_report_cancel(); s_tdc_report_owner = 0;
            (void)release_result(); s_tdc_capture_count = 0; s_tdc_phase = TDC_BATCH_IDLE;
            s_tdc_terminal = TDC_UT_BATCH_CM3_DATA;
            return s_tdc_terminal;
        }
        memcpy(s_tdc_capture + s_tdc_capture_count, g_tdc_ut_io_cm3_actual,
               (unsigned)count * sizeof(int));
        s_tdc_capture_count += (unsigned)count;
        if (release_result() != 0)
        {
            tdc_ut_report_cancel(); s_tdc_report_owner = 0;
            s_tdc_capture_count = 0; s_tdc_phase = TDC_BATCH_IDLE;
            s_tdc_terminal = TDC_UT_BATCH_CM3_DATA;
            return s_tdc_terminal;
        }
    }
    return 0;
}

static int start_unit(void)
{
    int rc, io;
    unsigned unit;
    if (!quiescent(s_tdc_capture_mode)) return TDC_UT_BATCH_CM3_UNSAFE;
    if (g_tdc_ut_cm3_vector_done || g_tdc_ut_io_cm3_done) return TDC_UT_BATCH_CM3_BUSY;
    if (s_tdc_capture_mode)
    {
        tdc_ut_report_cancel();
        return tdc_ut_report_begin("CM3", s_tdc_capture, 0, s_tdc_capture_count,
                                    TDC_BATCH_CAPTURE_CAPACITY, 1, 0) == 0 ? 0 : TDC_UT_BATCH_CM3_REPORT;
    }
    unit = group_unit(s_tdc_index, &io);
    if (io)
    {
        rc = tdc_ut_io_cm3_run_unit((int)unit);
        s_tdc_result_owner = g_tdc_ut_io_cm3_done ? 2 : 0;
        if (rc != 0) return TDC_UT_BATCH_CM3_DATA;
        tdc_ut_report_cancel();
        rc = tdc_ut_report_begin("CM3", g_tdc_ut_io_cm3_actual,
            g_tdc_ut_io_cm3_observed ? 0 : g_tdc_ut_io_cm3_expected,
            (unsigned)g_tdc_ut_io_cm3_count, TDC_UT_IO_CM3_CAPACITY,
            g_tdc_ut_io_cm3_done, g_tdc_ut_io_cm3_overflow);
    }
    else
    {
        rc = unit == TDC_BATCH_FAILURE_OBSERVATION ?
            tdc_ut_cm3_vectors_observe_calculation_failure() : tdc_ut_cm3_vectors_run_unit(unit);
        s_tdc_result_owner = g_tdc_ut_cm3_vector_done ? 1 : 0;
        if (rc < 0) return TDC_UT_BATCH_CM3_DATA;
        tdc_ut_report_cancel();
        rc = tdc_ut_report_begin("CM3", g_tdc_ut_cm3_vector_actual, tdc_ut_cm3_vectors_expected(),
            g_tdc_ut_cm3_vector_count, TDC_UT_CM3_VECTOR_CAPACITY,
            g_tdc_ut_cm3_vector_done, g_tdc_ut_cm3_vector_overflow);
    }
    return rc == 0 ? 0 : TDC_UT_BATCH_CM3_REPORT;
}

int tdc_ut_batch_cm3_poll(void)
{
    int rc;
    if (!foreground()) return TDC_UT_BATCH_CM3_UNSAFE;
    switch (s_tdc_phase)
    {
        case TDC_BATCH_IDLE: return s_tdc_terminal;
        case TDC_BATCH_BEGIN:
            rc = marker_poll();
            if (rc < 0) stop_batch(rc);
            else if (rc == 1) s_tdc_phase = TDC_BATCH_NEXT;
            return 0;
        case TDC_BATCH_NEXT:
            rc = start_unit();
            if (rc < 0) stop_batch(rc);
            else s_tdc_phase = TDC_BATCH_REPORTING;
            return 0;
        case TDC_BATCH_REPORTING:
            rc = tdc_ut_report_poll();
            if (rc < 0) stop_batch(TDC_UT_BATCH_CM3_DATA);
            else if (rc == 1)
            {
                if (release_result() != 0) { stop_batch(TDC_UT_BATCH_CM3_DATA); return 0; }
                s_tdc_report_owner = 0;
                s_tdc_completed += s_tdc_capture_mode ? s_tdc_selected : 1U;
                ++s_tdc_index;
                if (s_tdc_completed == s_tdc_selected)
                { marker("END", s_tdc_completed); s_tdc_phase = TDC_BATCH_END; }
                else if (reserve_report() != 0) stop_batch(TDC_UT_BATCH_CM3_REPORT);
                else s_tdc_phase = TDC_BATCH_NEXT;
            }
            return 0;
        case TDC_BATCH_END:
            rc = marker_poll();
            if (rc < 0) { stop_batch(rc); return 0; }
            if (rc == 1) { s_tdc_phase = TDC_BATCH_IDLE; s_tdc_terminal = 1; }
            return rc;
        case TDC_BATCH_STOP_DRAIN:
            if (s_tdc_marker_sent < s_tdc_marker_length)
            { (void)marker_poll(); return 0; }
            if (s_tdc_report_owner)
            {
                rc = tdc_ut_report_poll();
                if (rc == 0) return 0;
                if (rc < 0) { s_tdc_terminal = TDC_UT_BATCH_CM3_REPORT; return 0; }
                s_tdc_report_owner = 0;
            }
            if (release_result() != 0) return 0;
            marker(s_tdc_terminal == TDC_UT_BATCH_CM3_CANCELLED ? "ABORT" : "ERROR", s_tdc_completed);
            s_tdc_phase = TDC_BATCH_STOP_MARK;
            return 0;
        case TDC_BATCH_STOP_MARK:
            rc = marker_poll();
            if (rc == 1) { s_tdc_phase = TDC_BATCH_IDLE; return s_tdc_terminal; }
            return 0;
        default: return TDC_UT_BATCH_CM3_DATA;
    }
}

int tdc_ut_batch_cm3_run(unsigned poll_budget)
{
    if (poll_budget == 0 || poll_budget > TDC_UT_BATCH_CM3_MAX_POLLS) return TDC_UT_BATCH_CM3_INVALID;
    for (unsigned i = 0; i < poll_budget; ++i)
    {
        int rc = tdc_ut_batch_cm3_poll();
        if (rc != 0) return rc;
    }
    return 0;
}

int tdc_ut_batch_cm3_cancel(void)
{
    if (!foreground()) return TDC_UT_BATCH_CM3_UNSAFE;
    if (s_tdc_phase == TDC_BATCH_IDLE) return TDC_UT_BATCH_CM3_IDLE;
    if (s_tdc_phase == TDC_BATCH_STOP_DRAIN || s_tdc_phase == TDC_BATCH_STOP_MARK) return 0;
    if (s_tdc_phase == TDC_BATCH_END && s_tdc_marker_sent != 0) return TDC_UT_BATCH_CM3_BUSY;
    stop_batch(TDC_UT_BATCH_CM3_CANCELLED);
    return 0;
}
int tdc_ut_batch_cm3_active(void) { return s_tdc_phase != TDC_BATCH_IDLE; }
unsigned tdc_ut_batch_cm3_selected(void) { return s_tdc_selected; }
unsigned tdc_ut_batch_cm3_completed(void) { return s_tdc_completed; }
#endif
