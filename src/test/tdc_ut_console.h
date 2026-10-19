/* Explicit foreground test console. No product-loop hook or startup call. */
#ifndef TDC_UT_CONSOLE_H
#define TDC_UT_CONSOLE_H
#if defined(TDC_UNIT_TEST)
typedef struct
{
    const char *name;
    int (*begin)(unsigned group, unsigned step);
    int (*poll)(void);
    void (*cancel)(void);
} tdc_ut_console_command_t;
/* The table and names must outlive the open session. begin=0 accepts;
 * poll=0 pending, >0 complete, <0 aborted/error AFTER all output drains.
 * Only this console may own RTT DOWN0 and the active test reporter.
 */
int tdc_ut_console_open(const tdc_ut_console_command_t *commands, unsigned count);
int tdc_ut_console_poll(void);
int tdc_ut_console_run(unsigned budget);
extern volatile int g_tdc_ut_console_exit_requested;
extern volatile int g_tdc_ut_console_opened;
/* A writer contract failure latches the session. Do not resume product code;
 * inspect/reset from the debugger after making the test hardware safe. */
extern volatile int g_tdc_ut_console_transport_fault;
#endif
#endif
