/* Actual BOOT reporter with RAM results and a backpressured RTT sink. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/0__bootloader/source/test/tdc_ut_boot.h"
int g_tdc_ut_boot_actual[TDC_UT_BOOT_CAPACITY];
int g_tdc_ut_boot_expected[TDC_UT_BOOT_CAPACITY];
unsigned g_tdc_ut_boot_count;
int g_tdc_ut_boot_done, g_tdc_ut_boot_overflow;
static char s_output[8192];
static unsigned s_length, s_calls;
unsigned SEGGER_RTT_Write(unsigned channel,const void *data,unsigned bytes)
{
    assert(channel==0 && bytes<=60);
    if(s_calls++ % 3U == 0U) return 0;
    unsigned n=bytes>5 ? 5:bytes;
    assert(s_length+n<sizeof(s_output));
    memcpy(s_output+s_length,data,n); s_length+=n; s_output[s_length]=0;
    return n;
}
void tdc_ut_boot_release(void) {}
int tdc_ut_boot_run_unit(unsigned unit) { (void)unit; return -2; }
#include "../../src/0__bootloader/source/test/tdc_ut_boot_report.c"
static void drain(void)
{
    int rc=0;
    for(unsigned i=0;i<1000 && rc==0;i++) rc=tdc_ut_boot_report_poll();
    assert(rc==1);
}
int main(void)
{
    int a[]={-999,14,1,2,INT32_MIN,INT32_MAX,-999,11,6,1,0};
    int e[]={-999,14,1,2,INT32_MIN,INT32_MAX,-999,11,6,1,-1};
    memcpy(g_tdc_ut_boot_actual,a,sizeof(a)); memcpy(g_tdc_ut_boot_expected,e,sizeof(e));
    g_tdc_ut_boot_count=11;
    assert(tdc_ut_boot_report_begin()==-1); g_tdc_ut_boot_done=1;
    g_tdc_ut_boot_overflow=1; assert(tdc_ut_boot_report_begin()==-1); g_tdc_ut_boot_overflow=0;
    assert(tdc_ut_boot_report_begin()==0); assert(tdc_ut_boot_report_begin()==-1);
    drain(); assert(strstr(s_output,"C1 PASS N=2 F=0"));
    assert(strstr(s_output,"C6 I0 A=0 E=-1")); assert(strstr(s_output,"C6 FAIL N=1 F=1"));
    s_calls=s_length=0; s_output[0]=0;
    assert(tdc_ut_boot_report_begin()==0);
    assert(tdc_ut_boot_report_poll()==0); assert(tdc_ut_boot_report_poll()==0);
    assert(s_length==5); tdc_ut_boot_report_cancel();
    assert(tdc_ut_boot_report_begin()==-1); drain(); assert(!strstr(s_output,"PASS"));
    assert(tdc_ut_boot_report_begin()==0); drain();
    for(const char *p=s_output;*p;) {
        const char *end=strstr(p,"\r\n"); assert(end && end+2-p<=60); p=end+2;
    }
    g_tdc_ut_boot_expected[3]=3; assert(tdc_ut_boot_report_begin()==0);
    assert(tdc_ut_boot_report_poll()==-1); tdc_ut_boot_report_cancel();
    puts("BOOT_REPORT HOST_PASS: actual adapter, metadata, retry, cancel/restart, FAIL preserved");
    return 0;
}
