/* Test session only: call from a stopped foreground debugger context.
 * This does not stop CFX/DMA/IRQs for the caller. ready=1 acknowledges that
 * preparation has already been completed; it is not a hardware check.
 */
#if defined(TDC_UNIT_TEST)
#include <hw.h>
#include <tdc_shm.h>
#include "tdc_ut_batch_cm3.h"
#include "tdc_ut_batch_cfx_bridge.h"
#define TDC_UT_CONSOLE_CAN_EXIT() tdc_ut_cfx_bridge_ram_safe()
#include "../../../test/tdc_ut_console.c"

static unsigned s_tdc_console_session;
static int s_tdc_auto_phase;
static int s_tdc_auto_cancelled;
static int bridge_start(unsigned group)
{
    if (s_tdc_console_session >= TDC_UT_BATCH_MAX_ID) return -1;
    return tdc_ut_cfx_bridge_begin(group, ++s_tdc_console_session,
                                   TDC_UT_BATCH_MAX_ID);
}
static int cm3_begin(unsigned group, unsigned step)
{
    if (step || tdc_ut_cfx_bridge_busy() || !tdc_ut_cfx_bridge_ram_safe()) return -1;
    return tdc_ut_batch_cm3_begin(group, 1);
}
static int capture_begin(unsigned group, unsigned step)
{
    if (!step || tdc_ut_cfx_bridge_busy() || !tdc_ut_cfx_bridge_ram_safe()) return -1;
    return tdc_ut_batch_cm3_begin_capture(group, step, 1);
}
static void cm3_cancel(void) { (void)tdc_ut_batch_cm3_cancel(); }
static int cfx_begin(unsigned group, unsigned step)
{
    if (group != 1 || step || tdc_ut_batch_cm3_active()) return -1;
    return bridge_start(TDC_UT_BATCH_CFX);
}
static int hear_begin(unsigned group, unsigned step)
{
    if (group != 1 || step || tdc_ut_batch_cm3_active()) return -1;
    return bridge_start(TDC_UT_BATCH_HEAR);
}
static int auto_begin(unsigned group, unsigned step)
{
    int result;
    if (group != 1 || step || tdc_ut_cfx_bridge_busy() ||
        !tdc_ut_cfx_bridge_restored()) return -1;
    result = tdc_ut_batch_cm3_begin(1, 1);
    if (!result) { s_tdc_auto_phase = 1; s_tdc_auto_cancelled = 0; }
    return result;
}
static int auto_poll(void)
{
    int result;
    if (s_tdc_auto_phase == 1)
    {
        result = tdc_ut_batch_cm3_poll();
        if (result != 1) return result;
        if (s_tdc_auto_cancelled) return TDC_UT_BATCH_CM3_CANCELLED;
        result = bridge_start(TDC_UT_BATCH_CFX);
        if (result) return result;
        s_tdc_auto_phase = 2;
        return 0;
    }
    return tdc_ut_cfx_bridge_poll();
}
static void auto_cancel(void)
{
    s_tdc_auto_cancelled = 1;
    if (s_tdc_auto_phase == 1) cm3_cancel();
    else tdc_ut_cfx_bridge_abort();
}
static const tdc_ut_console_command_t s_tdc_cm3_commands[] = {
    {"auto", auto_begin, auto_poll, auto_cancel},
    {"cm3", cm3_begin, tdc_ut_batch_cm3_poll, cm3_cancel},
    {"observe", capture_begin, tdc_ut_batch_cm3_poll, cm3_cancel},
    {"cfx", cfx_begin, tdc_ut_cfx_bridge_poll, tdc_ut_cfx_bridge_abort},
    {"hear", hear_begin, tdc_ut_cfx_bridge_poll, tdc_ut_cfx_bridge_abort}
};

int tdc_ut_cm3_console_open(int ready)
{
    if (ready != 1 || __get_IPSR() ||
        cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration ||
        tdc_ut_batch_cm3_active() || tdc_ut_cfx_bridge_busy() ||
        !tdc_ut_cfx_bridge_ram_safe()) return -1;
    return tdc_ut_console_open(s_tdc_cm3_commands,
        sizeof(s_tdc_cm3_commands) / sizeof(s_tdc_cm3_commands[0]));
}

/* Explicit interactive session, ended by "ut quit" (or the exit flag).
 * Refresh the existing watchdog exactly as the normal foreground loop does;
 * do not change its period/configuration. This session is NOT a watchdog test.
 * Each batch poll still has to return before the next refresh can occur.
 */
int tdc_ut_cm3_console_serve(int ready)
{
    int result;
    if (tdc_ut_cm3_console_open(ready)) return -1;
    do {
        SYS_WATCHDOG_REFRESH();
        result = tdc_ut_console_poll();
    } while (result == 0);
    return result;
}
typedef void (*tdc_ut_console_entry_t)(void);
__attribute__((used, section(".tdc_ut_entry")))
static const tdc_ut_console_entry_t s_tdc_console_entries[] = {
    (tdc_ut_console_entry_t)tdc_ut_cm3_console_open,
    (tdc_ut_console_entry_t)tdc_ut_cm3_console_serve,
    (tdc_ut_console_entry_t)tdc_ut_console_poll,
    (tdc_ut_console_entry_t)tdc_ut_console_run
};
#endif
