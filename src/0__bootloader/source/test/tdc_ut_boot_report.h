#ifndef TDC_UT_BOOT_REPORT_H
#define TDC_UT_BOOT_REPORT_H
#if defined(TDC_UNIT_TEST)
/* Results are immutable until drain returns 1. -1 is a damaged record. */
int tdc_ut_boot_report_begin(void);
int tdc_ut_boot_report_poll(void);
/* Read-only foreground query, including a cancelled partial line still draining.
 * This is reporter ownership, not the result producer's private pending flag. */
int tdc_ut_boot_report_busy(void);
/* After cancel, poll until 1 before begin/release if a partial line exists. */
void tdc_ut_boot_report_cancel(void);
#endif
#endif
