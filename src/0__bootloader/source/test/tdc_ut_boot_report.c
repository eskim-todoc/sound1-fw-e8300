/* Foreground-only, nonblocking RTT adapter. Never prints from device ISRs. */
#if defined(TDC_UNIT_TEST)
#include <SEGGER_RTT.h>
#include "tdc_ut_boot.h"
#include "tdc_ut_boot_report.h"
#include "../../../test/tdc_ut_log.c"
static tdc_ut_stream_t s_tdc_ut_stream;
static unsigned s_tdc_ut_count, s_tdc_ut_cursor, s_tdc_ut_offset;
static int32_t actual(const void *ctx,unsigned i)
{ (void)ctx; return g_tdc_ut_boot_actual[s_tdc_ut_offset+i]; }
static int32_t expected(const void *ctx,unsigned i)
{ (void)ctx; return g_tdc_ut_boot_expected[s_tdc_ut_offset+i]; }
static unsigned write_line(const char *s,unsigned n)
{ return SEGGER_RTT_Write(0,s,n); }
int tdc_ut_boot_report_busy(void)
{ return s_tdc_ut_stream.active || s_tdc_ut_cursor<s_tdc_ut_count; }
int tdc_ut_boot_report_begin(void)
{
    if(s_tdc_ut_stream.active || s_tdc_ut_cursor<s_tdc_ut_count ||
       !g_tdc_ut_boot_done || g_tdc_ut_boot_overflow ||
       g_tdc_ut_boot_count==0 || g_tdc_ut_boot_count>TDC_UT_BOOT_CAPACITY) return -1;
    s_tdc_ut_count=g_tdc_ut_boot_count; s_tdc_ut_cursor=0;
    return 0;
}
int tdc_ut_boot_report_poll(void)
{
    if(s_tdc_ut_stream.active) return tdc_ut_log_poll(&s_tdc_ut_stream)<0 ? -1:0;
    if(s_tdc_ut_cursor==s_tdc_ut_count) return 1;
    if(s_tdc_ut_count-s_tdc_ut_cursor<4 || g_tdc_ut_boot_actual[s_tdc_ut_cursor]!=-999) return -1;
    for(unsigned i=0;i<4;i++)
        if(g_tdc_ut_boot_actual[s_tdc_ut_cursor+i]!=g_tdc_ut_boot_expected[s_tdc_ut_cursor+i]) return -1;
    unsigned unit=(unsigned)g_tdc_ut_boot_actual[s_tdc_ut_cursor+1];
    unsigned vector=(unsigned)g_tdc_ut_boot_actual[s_tdc_ut_cursor+2];
    unsigned n=(unsigned)g_tdc_ut_boot_actual[s_tdc_ut_cursor+3];
    if(n==0 || n>s_tdc_ut_count-s_tdc_ut_cursor-4) return -1;
    s_tdc_ut_offset=s_tdc_ut_cursor+4;
    if(tdc_ut_log_begin(&s_tdc_ut_stream,"BOOT",unit,vector,n,0,actual,expected,write_line)) return -1;
    s_tdc_ut_cursor+=4+n;
    return tdc_ut_log_poll(&s_tdc_ut_stream)<0 ? -1:0;
}
void tdc_ut_boot_report_cancel(void)
{ tdc_ut_log_cancel(&s_tdc_ut_stream); s_tdc_ut_count=s_tdc_ut_cursor=0; }
typedef void (*tdc_ut_entry_t)(void);
__attribute__((used,section(".tdc_ut_entry")))
static const tdc_ut_entry_t s_tdc_ut_entries[]={
    (tdc_ut_entry_t)tdc_ut_boot_run_unit,(tdc_ut_entry_t)tdc_ut_boot_release,
    (tdc_ut_entry_t)tdc_ut_boot_report_begin,(tdc_ut_entry_t)tdc_ut_boot_report_poll,
    (tdc_ut_entry_t)tdc_ut_boot_report_cancel
};
#endif
