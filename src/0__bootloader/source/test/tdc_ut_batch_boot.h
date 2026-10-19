#ifndef TDC_UT_BATCH_BOOT_H
#define TDC_UT_BATCH_BOOT_H
#if defined(TDC_UNIT_TEST)

/* Explicit foreground entry only. No startup hook or RTT DOWN parser here.
 * group 1 (default): 8,9,10,11,12,13,14,19,20
 * group 2 (file):    8,9,10,11,12,19,20
 * group 3 (crypto):  13,14
 * These reuse the existing target RAM vectors, not full device tests.
 */
enum {
    TDC_UT_BOOT_BATCH_ALL = 1,
    TDC_UT_BOOT_BATCH_FILE = 2,
    TDC_UT_BOOT_BATCH_CRYPTO = 3
};
enum {
    TDC_UT_BOOT_BATCH_ERROR = -1,
    TDC_UT_BOOT_BATCH_NOT_READY = -2,
    TDC_UT_BOOT_BATCH_BUSY = -3,
    TDC_UT_BOOT_BATCH_CANCELLED = -4,
    TDC_UT_BOOT_BATCH_BAD_GROUP = -5,
    TDC_UT_BOOT_BATCH_STALE = -6,
    TDC_UT_BOOT_BATCH_RUNNING = 0,
    TDC_UT_BOOT_BATCH_DONE = 1,
    TDC_UT_BOOT_BATCH_IDLE = 2
};
typedef struct {
    unsigned group, total, completed, unit;
    int status;
} tdc_ut_boot_batch_status_t;

/* ready MUST be 1: caller certifies boot/file/CRC/scratch/BRAM users and other
 * test callers are quiescent for the whole batch. RTT channel 0 and the boot
 * UART must already be initialized. CRC hardware must be clocked/exclusive;
 * slot vectors retain their existing SYS_WATCHDOG_REFRESH dependency.
 * The caller must service its existing watchdog policy between bounded polls.
 * No NVM write, image transfer, core jump, reset or watchdog setup is added.
 * A poll can synchronously execute ONE existing unit, so its WCET still applies.
 *
 * 0=accepted; negative=not started, existing results untouched. Never calls
 * release to steal another run. Unconsumed/stale done/count/overflow or an
 * active reporter rejects begin. To switch from manual unit calls, that owner
 * must finish/cancel+drain its report, release its result and explicitly clear
 * its done/count/overflow metadata. This batch clears ONLY results it owns.
 */
int tdc_ut_boot_batch_begin(unsigned group, int ready);

/* At most one RTT write per poll, no wait loop. 0=keep polling, 1=all selected
 * units were drained (NOT PASS), 2=never started/idle, -4=cancel drained,
 * -1=execution/report/transport error drained. Terminal calls are idempotent.
 * Zero/partial RTT acceptance retries the exact suffix. While busy, caller
 * must not call the unit/reporter/release APIs or print into this RTT stream.
 */
int tdc_ut_boot_batch_poll(void);
/* Poll at most poll_budget times. Budget 0 only reads status. Budget expiry
 * returns 0 and preserves progress; run()/poll() can resume without begin(). */
int tdc_ut_boot_batch_run(unsigned poll_budget);
/* Foreground only; no writes here. Cancel an active unit report, terminate its
 * partial line, then emit CANCEL. Keep polling while busy before reuse.
 * Once END transmission has started, all units are already drained/released:
 * cancellation is too late and END is finished normally (status DONE).
 */
void tdc_ut_boot_batch_cancel(void);
int tdc_ut_boot_batch_busy(void);
/* Borrowed read-only status, valid for program lifetime. No ISR/concurrent use. */
const tdc_ut_boot_batch_status_t *tdc_ut_boot_batch_get_status(void);
#endif
#endif
