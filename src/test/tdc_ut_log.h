/* Test-only, foreground RTT stream. No heap, colours or ISR output.
 * A reader's storage must remain immutable until the stream finishes.
 */
#ifndef TDC_UT_LOG_H
#define TDC_UT_LOG_H
#if defined(TDC_UNIT_TEST)
#include <stdint.h>
#define TDC_UT_LINE_BYTES 60U /* Includes CRLF; excludes the C terminator. */
typedef int32_t (*tdc_ut_read_t)(const void *context, unsigned index);
typedef unsigned (*tdc_ut_write_t)(const char *data, unsigned bytes);
typedef struct
{
    char line[TDC_UT_LINE_BYTES + 1U];
    char core[5];
    const void *context;
    tdc_ut_read_t actual;
    tdc_ut_read_t expected;
    tdc_ut_write_t write;
    unsigned unit, vector, count, index, failed, phase;
    unsigned bytes, sent, active;
} tdc_ut_stream_t;

/* Returns -1 for invalid input or replacing a stream that is still active.
 * Null expected means observation only, never PASS.
 */
int tdc_ut_log_begin(tdc_ut_stream_t *stream, const char *core,
                     unsigned unit, unsigned vector, unsigned count,
                     const void *context, tdc_ut_read_t actual,
                     tdc_ut_read_t expected, tdc_ut_write_t write);
/* At most one write per call. 0=pending/retry, 1=finished, -1=invalid.
 * A zero or partial write retains the exact pending suffix. This also works
 * with RTT TRIM, though test builds should retain NO_BLOCK_SKIP.
 */
int tdc_ut_log_poll(tdc_ut_stream_t *stream);
/* Discard unsent records, but terminate an already partially written line.
 * Keep polling until 1 before starting another stream. No PASS is added.
 */
void tdc_ut_log_cancel(tdc_ut_stream_t *stream);
#endif
#endif
