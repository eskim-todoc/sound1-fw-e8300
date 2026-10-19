/* BOOT has a separate image/session from the CM3 application. */
#if defined(TDC_UNIT_TEST)
#include <hw.h>
#include "tdc_ut_batch_boot.h"
#include "../../../test/tdc_ut_console.c"
static int boot_begin(unsigned group, unsigned step)
{ return step ? -1 : tdc_ut_boot_batch_begin(group, 1); }
static const tdc_ut_console_command_t s_tdc_boot_commands[] = {
    {"boot", boot_begin, tdc_ut_boot_batch_poll, tdc_ut_boot_batch_cancel}
};
int tdc_ut_boot_console_open(int ready)
{
    if (ready != 1 || __get_IPSR() || tdc_ut_boot_batch_busy()) return -1;
    return tdc_ut_console_open(s_tdc_boot_commands, 1);
}
/* Caller guarantees initialized RTT/UART, idle boot/file/CRC/scratch/BRAM,
 * stopped other cores, and foreground ownership for the complete session.
 * Only the existing watchdog refresh operation is used; no reset or timing
 * configuration is changed. End through ut quit; reboot after the test.
 */
int tdc_ut_boot_console_serve(int ready)
{
    int result;
    if (tdc_ut_boot_console_open(ready)) return -1;
    do {
        SYS_WATCHDOG_REFRESH();
        result = tdc_ut_console_poll();
    } while (result == 0);
    return result;
}
typedef void (*tdc_ut_console_entry_t)(void);
__attribute__((used, section(".tdc_ut_entry")))
static const tdc_ut_console_entry_t s_tdc_console_entries[] = {
    (tdc_ut_console_entry_t)tdc_ut_boot_console_open,
    (tdc_ut_console_entry_t)tdc_ut_boot_console_serve,
    (tdc_ut_console_entry_t)tdc_ut_console_poll,
    (tdc_ut_console_entry_t)tdc_ut_console_run
};
#endif
