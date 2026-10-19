/* Real bridge lifecycle on synthetic mailbox memory. No HEAR execution.
 * Proves the exact busy/restored/tdc_sys_error contract consumed by the adapter tests.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/2__cm3/source/tests/tdc_ut_batch_cfx_bridge.h"
#include "../../src/2__cm3/source/tests/tdc_ut_report.h"
uint32_t g_console_adapter_xram[0x14000];
static char output[2048];
static unsigned output_count;
unsigned SEGGER_RTT_Write(unsigned channel,const void *data,unsigned n)
{
    assert(channel==0 && n<=60 && output_count+n<sizeof(output));
    memcpy(output+output_count,data,n);output_count+=n;output[output_count]=0;return n;
}
int tdc_ut_report_begin(const char *core,const int *actual,const int *expected,
                        unsigned count,unsigned capacity,int done,int overflow)
{
    assert(!strcmp(core,"CFX") && actual && expected && count==5 && capacity==5 && done && !overflow);
    return 0;
}
void tdc_ut_report_cancel(void){}
int tdc_ut_report_poll(void){assert(!"No result descriptor was produced");return -1;}
int tdc_ut_report_cfx_core_begin(const char *core,unsigned a,unsigned e,unsigned n,unsigned c,int d,int o)
{(void)core;(void)a;(void)e;(void)n;(void)c;(void)d;(void)o;assert(0);return -1;}
int tdc_ut_report_cfx_observed_begin(const char *core,unsigned a,unsigned n,unsigned c,int d,int o)
{(void)core;(void)a;(void)n;(void)c;(void)d;(void)o;assert(0);return -1;}
int tdc_ut_report_cfx_values_begin(const char *core,unsigned u,unsigned v,unsigned a,unsigned n,int d)
{(void)core;(void)u;(void)v;(void)a;(void)n;(void)d;assert(0);return -1;}
int main(void)
{
    uint32_t *mb=g_console_adapter_xram+0x4000;
    assert(!tdc_ut_cfx_bridge_busy() && !tdc_ut_cfx_bridge_restored() && !tdc_ut_cfx_bridge_error());
    assert(tdc_ut_cfx_bridge_ram_safe()); /* Never configured: CM3-only remains usable. */
    mb[UTB_MAGIC]=TDC_UT_BATCH_MAGIC;mb[UTB_VERSION]=TDC_UT_BATCH_VERSION;mb[UTB_WORDS]=UTB_WORD_COUNT;
    mb[UTB_TOKEN]=77;mb[UTB_READY_FLAGS]=TDC_UT_BATCH_RAM_READY|TDC_UT_BATCH_HEAR_READY;
    mb[UTB_STATE]=UTB_READY;mb[UTB_RESTORED]=1;
    assert(tdc_ut_cfx_bridge_configure(0x4000,77)==0 && tdc_ut_cfx_bridge_restored());
    assert(tdc_ut_cfx_bridge_ram_safe());
    assert(tdc_ut_cfx_bridge_begin(TDC_UT_BATCH_HEAR,1,4)==0);
    assert(!tdc_ut_cfx_bridge_ram_safe());
    assert(mb[UTB_CMD]==UTB_OPEN);
    /* Producer consumes OPEN and waits for a real HEAR event that never comes. */
    mb[UTB_CMD]=0;mb[UTB_SESSION]=1;mb[UTB_GROUP]=TDC_UT_BATCH_HEAR;
    mb[UTB_READY_ACK]=77;mb[UTB_TOTAL]=TDC_UT_BATCH_HEAR_COUNT;
    mb[UTB_STATE]=UTB_RUNNING;mb[UTB_RESTORED]=0;
    int result=0,saw_abort=0;
    for(unsigned i=0;i<50 && result==0;++i)
    {
        result=tdc_ut_cfx_bridge_poll();
        if(mb[UTB_CMD]==UTB_ABORT)
        {saw_abort=1;mb[UTB_CMD]=0;mb[UTB_STATE]=UTB_CLEANUP;}
    }
    assert(saw_abort && result==-UTB_TIMEOUT);
    assert(!tdc_ut_cfx_bridge_busy() && !tdc_ut_cfx_bridge_restored());
    assert(!tdc_ut_cfx_bridge_ram_safe());
    assert(tdc_ut_cfx_bridge_error()==UTB_TIMEOUT);
    assert(strstr(output,"ERROR E=5 RESTORED=0\r\n") && !strstr(output," END "));
    assert(tdc_ut_cfx_bridge_configure(0x4000,77)==-UTB_NOT_READY);
    /* Late completion clears producer leases, not the previous tdc_sys_error code. */
    mb[UTB_STATE]=UTB_ABORTED;mb[UTB_RESTORED]=1;
    assert(tdc_ut_cfx_bridge_restored() && tdc_ut_cfx_bridge_error()==UTB_TIMEOUT);
    assert(tdc_ut_cfx_bridge_ram_safe());
    mb[UTB_TOKEN]=78; /* A replaced/unknown mailbox is not restored evidence. */
    assert(!tdc_ut_cfx_bridge_ram_safe());mb[UTB_TOKEN]=77;
    puts("CONSOLE_ADAPTER_BRIDGE PASS: unconfigured=busy0/restored0/error0; timeout=busy0/restored0/error5; late-complete=restored1/error5");
    return 0;
}
