#ifndef TDC_UT_BATCH_CFX_BRIDGE_H
#define TDC_UT_BATCH_CFX_BRIDGE_H
#if defined(TDC_UNIT_TEST)
#include "../../../1__cfx/test/tdc_ut_batch_cfx.h"
/* Debugger setup after CFX start: use the CURRENT map's XMEM word address of
 * g_tdc_ut_cfx_batch_mailbox, never the CM3 byte address or a prior build map.
 * Configure checks magic/version/token/RAM range without changing CFX state.
 * begin requires a strictly increasing session for this arm (1..0x7fffff).
 * poll_limit bounds waiting in polls, NOT milliseconds. Choose enough for
 * full FFT/vMag drain and the debugger's independent core scheduling.
 * One foreground caller; console must exclusively own RTT and the existing
 * report API during busy. No product hooks or automatic execution. */
int tdc_ut_cfx_bridge_configure(unsigned mailbox_word, unsigned token);
int tdc_ut_cfx_bridge_begin(unsigned group, unsigned session, unsigned poll_limit);
/* 0=pending, 1=all records accepted by RTT (not PASS), -error=terminal.
 * An error returns negative only after this bridge's pending line has been
 * terminated and its error line drained. A full RTT buffer can keep busy=1
 * indefinitely; never interleave console output while busy. END means every
 * result was emitted, NOT that the viewer saved it or every test passed. */
int tdc_ut_cfx_bridge_poll(void);
int tdc_ut_cfx_bridge_run(unsigned poll_budget);
void tdc_ut_cfx_bridge_abort(void);
int tdc_ut_cfx_bridge_busy(void);
unsigned tdc_ut_cfx_bridge_completed(void);
unsigned tdc_ut_cfx_bridge_error(void);
/* 1 only when producer reports no HEAR pending and no result lease. After a
 * cleanup timeout this can remain 0 even though RTT is fully drained/busy=0.
 * Continue CFX service and inspect/reconfigure after actual completion. */
int tdc_ut_cfx_bridge_restored(void);
/* CM3-only admission guard: 1 if never successfully configured, or if idle
 * with a valid current mailbox in READY/DONE/ABORTED, RESTORED=1 and CMD=0.
 * Invalid configured mailboxes, pending RTT lines and late HEAR cleanup
 * return 0. Read-only; does not configure, cancel or restore anything.
 * An unconfigured 1 is NOT proof of product/HEAR quiescence: the caller must
 * still enforce those independent preconditions and serialize all callers. */
int tdc_ut_cfx_bridge_ram_safe(void);
#endif
#endif
