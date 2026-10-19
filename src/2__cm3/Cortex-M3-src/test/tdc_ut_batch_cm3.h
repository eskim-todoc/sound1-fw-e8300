/* Explicit, foreground-only test batches. No startup execution or device IO. */
#ifndef TDC_UT_BATCH_CM3_H
#define TDC_UT_BATCH_CM3_H
#if defined(TDC_UNIT_TEST)
#define TDC_UT_BATCH_CM3_INVALID (-1)
#define TDC_UT_BATCH_CM3_BUSY (-2)
#define TDC_UT_BATCH_CM3_UNSAFE (-3)
#define TDC_UT_BATCH_CM3_DATA (-4)
#define TDC_UT_BATCH_CM3_CANCELLED (-5)
#define TDC_UT_BATCH_CM3_REPORT (-6)
#define TDC_UT_BATCH_CM3_IDLE (-7)
#define TDC_UT_BATCH_CM3_MAX_POLLS 4096U

/* ready must be exactly 1: the caller has stopped the normal loop, every
 * producer/ISR and the other cores. This flag is an explicit acknowledgement,
 * not a command to stop them. CFX permission must already be 0. Automatic RAM
 * runs also require idle SPI/I2C and no installed manual observer gateway.
 * No concurrent direct runner/reporter calls are permitted until inactive.
 * Group 1: all 34 runs, group 2: 22 calculation/control runs, group 3: 12 IO.
 * CM3-029 has its normal vectors AND failure observation (two runs).
 * Existing PARTIAL/OBSERVED vectors retain their scope; END is not PASS.
 * begin=0 accepts without running a unit; negative rejects without clearing
 * somebody else's pending result/report. Each poll runs at most one existing
 * finite unit or one RTT write. It cannot preempt a defective unit function.
 */
int tdc_ut_batch_cm3_begin(unsigned group, int ready);

/* Input is applied by the operator BEFORE calling this function. Capture
 * group 1=all 24 observers, 2=device status (17), 3=communication/storage (7).
 * case_id=1..999 identifies the common input step. All selected snapshots are
 * copied during this one call, while producers remain stopped. No input is
 * applied, no peripheral transaction is started, no expected/PASS is emitted.
 * Busy SPI/I2C state is allowed here because these are read-only snapshots.
 * Uses a bounded 512-int private buffer; overflow rejects without an END.
 */
int tdc_ut_batch_cm3_begin_capture(unsigned group, unsigned case_id, int ready);

/* 0=pending, 1=all selected records and END drained, negative=terminal error.
 * Before first begin, returns IDLE. A terminal result persists until begin.
 * run requires 1..MAX_POLLS; it never waits for the RTT reader or hardware.
 * Continue polling while active after cancel/error to finish partial lines.
 */
int tdc_ut_batch_cm3_poll(void);
int tdc_ut_batch_cm3_run(unsigned poll_budget);
/* 0=accepted/idempotent while abort drains; negative=no active batch or ISR.
 * If END has already started transmission, BUSY means completion has committed:
 * finish poll, rather than truncating a truthful END after every unit drained.
 */
int tdc_ut_batch_cm3_cancel(void);
int tdc_ut_batch_cm3_active(void);
unsigned tdc_ut_batch_cm3_selected(void);
unsigned tdc_ut_batch_cm3_completed(void);
#endif
#endif
