/* Test-only CFX/CM3 mailbox contract. Offsets are CFX XMEM WORDS, not bytes.
 * CM3 accesses the unsigned 32-bit view and uses the low 24 bits. No product
 * shared structure, ISR or startup hook is involved. Single foreground owner
 * per core; other test runners/reporters must remain idle until completion.
 */
#ifndef TDC_UT_BATCH_CFX_H
#define TDC_UT_BATCH_CFX_H
#if defined(TDC_UNIT_TEST)
#define TDC_UT_BATCH_MAGIC 0x554254U
#define TDC_UT_BATCH_VERSION 1U
#define TDC_UT_BATCH_MAX_ID 0x7fffffU
#define TDC_UT_BATCH_CFX 1U
#define TDC_UT_BATCH_HEAR 2U
#define TDC_UT_BATCH_RAM_READY 1U
#define TDC_UT_BATCH_HEAR_READY 2U
#define TDC_UT_BATCH_VECTOR_COUNT 17U
#define TDC_UT_BATCH_CFX_COUNT 29U
#define TDC_UT_BATCH_HEAR_COUNT 6U
#define TDC_UT_BATCH_VECTOR_IDS 8,13,16,17,18,20,22,23,24,25,28,29,30,31,32,34,35
#define TDC_UT_BATCH_IO_IDS 7,10,11,12,15,19,26,27,33,36,37,40

enum tdc_ut_batch_word
{
    UTB_MAGIC, UTB_VERSION, UTB_WORDS, UTB_TOKEN, UTB_READY_FLAGS, UTB_STATE,
    UTB_SESSION, UTB_GROUP, UTB_READY_ACK, UTB_SEQ, UTB_TOTAL,
    UTB_UNIT, UTB_VECTOR, UTB_ACTUAL, UTB_EXPECTED, UTB_COUNT, UTB_CAPACITY,
    UTB_NUMERIC, UTB_NUMERIC_COUNT, UTB_FFT, UTB_FFT_COUNT,
    UTB_VMAG, UTB_VMAG_COUNT, UTB_EXPONENT, UTB_FAILED, UTB_ERROR,
    UTB_COMPLETED,
    /* CM3 writes arguments first and CMD last. CFX clears CMD after reading.
     * No new command may replace a nonzero CMD. An ACK authorizes reuse only
     * after every compared/numeric/raw line has reached RTT. */
    UTB_CMD, UTB_CMD_TOKEN, UTB_CMD_SESSION, UTB_CMD_GROUP,
    UTB_CMD_SEQ, UTB_CMD_LIMIT, UTB_RESTORED, UTB_WORD_COUNT
};
enum tdc_ut_batch_state
{
    UTB_OFF, UTB_READY, UTB_RUNNING, UTB_DATA, UTB_DONE,
    UTB_FAULT, UTB_CLEANUP, UTB_ABORTED
};
enum tdc_ut_batch_command { UTB_NONE, UTB_OPEN, UTB_ACK, UTB_ABORT };
enum tdc_ut_batch_error
{
    UTB_OK, UTB_BAD_ARGUMENT, UTB_BUSY, UTB_NOT_READY, UTB_PROTOCOL,
    UTB_TIMEOUT, UTB_RUNNER_ERROR, UTB_CANCELLED, UTB_REPORT_ERROR
};

#if defined(CFX_CORE) || defined(TDC_UT_BATCH_HOST_CFX)
extern volatile unsigned int _XMEM g_tdc_ut_cfx_batch_mailbox[UTB_WORD_COUNT];
/* Setting this to 1 only exits the bounded service, NOT a safe cancellation.
 * Stop through the CM3 bridge first; keep polling HEAR cleanup if pending. */
extern volatile unsigned int g_tdc_ut_cfx_batch_service_stop;
#endif

/* Call after manually quiescing audio/PCM/shared writers and other tests.
 * ram_ready=1 confirms RAM producers stopped. hear_ready=1 additionally
 * confirms HEAR idle, correct loaded microcode/window and completion IRQs
 * enabled. Do not disable those IRQs for the HEAR group.
 * A new nonzero token is required on every re-arm; use the same current
 * token at CM3 configure. All token/session/limit values are 1..0x7fffff.
 * No pre-existing result is cleared by start. Return 0 or -error. */
int tdc_ut_cfx_batch_start(unsigned token, int ram_ready, int hear_ready);
/* 0=pending/ready, 1=group completed (not PASS), negative=terminal error.
 * CLEANUP returns 0 even with ERROR set: poll until HEAR actually completes.
 * Arrays remain leased after FAULT until CM3 finishes cancellation and ABORT.
 */
int tdc_ut_cfx_batch_poll(void);
int tdc_ut_cfx_batch_run(unsigned poll_budget);
/* One debugger invocation can serve several CM3 begin requests. Both loops
 * are bounded, no WFE/sleep, no watchdog feed. It keeps serving READY/DONE/
 * ABORTED and pending cleanup. Return 0 at budget/stop, negative if unarmed.
 * Choose finite bounds appropriate to the debugger session, then inspect
 * mailbox state before resuming product execution. Reboot after testing. */
int tdc_ut_cfx_batch_service(unsigned rounds, unsigned polls_per_round);
#endif
#endif
