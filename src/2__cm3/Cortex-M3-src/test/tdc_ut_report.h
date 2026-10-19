#ifndef TDC_UT_REPORT_H
#define TDC_UT_REPORT_H
#if defined(TDC_UNIT_TEST)
/* expected=NULL reports OBSERVED only, never a verification PASS. */
int tdc_ut_report_begin(const char *core, const int *actual, const int *expected,
                        unsigned count, unsigned capacity, int done, int overflow);
int tdc_ut_report_cfx_begin(unsigned actual_word, unsigned expected_word,
                           unsigned count, unsigned capacity, int done, int overflow);
int tdc_ut_report_cfx_core_begin(const char *core, unsigned actual_word, unsigned expected_word,
                                unsigned count, unsigned capacity, int done, int overflow);
/* Header-framed raw numeric records, e.g. [actual, lower, upper] triples.
 * No expected buffer is read; OBSERVED only. Drain before releasing CFX data.
 */
int tdc_ut_report_cfx_observed_begin(const char *core, unsigned actual_word,
                                    unsigned count, unsigned capacity, int done, int overflow);
/* Raw XRAM capture, without record headers or prepared expectations. */
int tdc_ut_report_cfx_values_begin(const char *core, unsigned unit, unsigned vector,
                                  unsigned word, unsigned count, int done);
int tdc_ut_report_poll(void);
/* Cancel unfinished output, then poll until 1 to close any partial line.
 * This is not a successful report. begin rejects while the line is pending.
 */
void tdc_ut_report_cancel(void);
#endif
#endif
