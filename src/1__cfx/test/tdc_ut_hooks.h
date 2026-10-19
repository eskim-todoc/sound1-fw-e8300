#ifndef TDC_UT_HOOKS_H
#define TDC_UT_HOOKS_H
#if defined(TDC_UNIT_TEST)
/* Input substitution isolates integer AGC branch tests from log-library
 * approximation. It is disabled by default and restored after every vector.
 * The observed goal is taken before attack/release, never used as expected.
 */
extern int g_tdc_ut_agc_override_enabled;
extern int g_tdc_ut_agc_input_db;
extern int g_tdc_ut_agc_goal_db;
extern int g_tdc_ut_agc_clamped_input;
extern int g_tdc_ut_power_override_enabled;
#endif
#endif
