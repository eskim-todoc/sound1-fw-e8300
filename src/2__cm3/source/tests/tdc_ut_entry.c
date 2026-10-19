/* Keep debugger-callable entry points in an opt-in test image. Nothing here
 * executes at startup. The linker KEEP rule has no input in a normal build.
 */
#if defined(TDC_UNIT_TEST)
#include "tdc_ut_vectors_cm3.h"
#include "tdc_ut_io_cm3.h"
#include "tdc_ut_report.h"
#include "tdc_ut_batch_cm3.h"
typedef void (*tdc_ut_entry_t)(void);
__attribute__((used, section(".tdc_ut_entry")))
static const tdc_ut_entry_t s_tdc_ut_entries[] = {
    (tdc_ut_entry_t)tdc_ut_batch_cm3_begin,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_begin_capture,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_poll,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_run,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_cancel,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_active,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_selected,
    (tdc_ut_entry_t)tdc_ut_batch_cm3_completed,
    (tdc_ut_entry_t)tdc_ut_cm3_vectors_run_unit,
    (tdc_ut_entry_t)tdc_ut_cm3_vectors_release,
    (tdc_ut_entry_t)tdc_ut_cm3_vectors_expected,
    (tdc_ut_entry_t)tdc_ut_cm3_vectors_observe_calculation_failure,
    (tdc_ut_entry_t)tdc_ut_io_cm3_run_unit,
    (tdc_ut_entry_t)tdc_ut_io_cm3_release,
    (tdc_ut_entry_t)tdc_ut_io_cm3_observe_unit,
    (tdc_ut_entry_t)tdc_ut_io_cm3_i2c_snapshot,
    (tdc_ut_entry_t)tdc_ut_io_cm3_spi_snapshot,
    (tdc_ut_entry_t)tdc_ut_report_begin,
    (tdc_ut_entry_t)tdc_ut_report_cfx_begin,
    (tdc_ut_entry_t)tdc_ut_report_cfx_core_begin,
    (tdc_ut_entry_t)tdc_ut_report_cfx_observed_begin,
    (tdc_ut_entry_t)tdc_ut_report_cfx_values_begin,
    (tdc_ut_entry_t)tdc_ut_report_poll,
    (tdc_ut_entry_t)tdc_ut_report_cancel
};
#endif
