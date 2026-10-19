/* Host-only test of the actual reporter, including 24-bit bridge decoding. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
uint32_t g_tdc_ut_host_xram[0x14000];
static char s_tdc_ut_capture[8192];
static unsigned s_tdc_ut_bytes, s_tdc_ut_calls;
unsigned SEGGER_RTT_Write(unsigned channel, const void *data, unsigned bytes)
{
    unsigned accepted;
    assert(channel == 0 && bytes <= 60);
    if ((s_tdc_ut_calls++ % 3) == 0) return 0;
    accepted = bytes > 5 ? 5 : bytes;
    assert(s_tdc_ut_bytes + accepted < sizeof(s_tdc_ut_capture));
    memcpy(s_tdc_ut_capture + s_tdc_ut_bytes, data, accepted);
    s_tdc_ut_bytes += accepted; s_tdc_ut_capture[s_tdc_ut_bytes] = 0;
    return accepted;
}
#include "../../src/2__cm3/Cortex-M3-src/test/tdc_ut_report.c"
static void drain(void)
{
    int rc=0;
    for (unsigned i=0; i<1000 && rc==0; i++) rc=tdc_ut_report_poll();
    assert(rc==1);
}
int main(void)
{
    int a[]={-999,27,1,3,0,127,255,-999,27,2,1,-20};
    int e[]={-999,27,1,3,0,127,255,-999,27,2,1,-21};
    assert(tdc_ut_report_begin(0,a,e,12,12,1,0)==-1);
    assert(tdc_ut_report_begin("CM3",a,e,12,12,0,0)==-1);
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,1)==-1);
    assert(tdc_ut_report_begin("CM3",a,e,13,12,1,0)==-1);
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==0);
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==-1);
    drain();
    assert(strstr(s_tdc_ut_capture,"C1 PASS N=3 F=0"));
    assert(strstr(s_tdc_ut_capture,"C2 I0 A=-20 E=-21"));
    assert(strstr(s_tdc_ut_capture,"C2 FAIL N=1 F=1"));
    tdc_ut_report_cancel();
    s_tdc_ut_bytes=0; s_tdc_ut_capture[0]=0;
    assert(tdc_ut_report_begin("CM3",a,0,12,12,1,0)==0); drain();
    assert(strstr(s_tdc_ut_capture,"OBSERVED")); assert(!strstr(s_tdc_ut_capture,"PASS"));
    tdc_ut_report_cancel();
    a[0]=0;
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==0);
    assert(tdc_ut_report_poll()==-1); tdc_ut_report_cancel();
    a[0]=-999; e[3]=4;
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==0);
    assert(tdc_ut_report_poll()==-1); tdc_ut_report_cancel(); e[3]=3;
    assert(tdc_ut_report_cfx_begin(0,0x4100,7,7,1,0)==-1);
    assert(tdc_ut_report_cfx_begin(0x13ffc,0x4100,7,7,1,0)==-1);
    const int v[]={-999,17,1,3,-8388608,-1,8388607};
    for(unsigned i=0;i<7;i++) {
        g_tdc_ut_host_xram[0x4000+i]=(uint32_t)v[i]&0xffffffU;
        g_tdc_ut_host_xram[0x4100+i]=(uint32_t)v[i]&0xffffffU;
    }
    s_tdc_ut_bytes=s_tdc_ut_calls=0; s_tdc_ut_capture[0]=0;
    assert(tdc_ut_report_cfx_begin(0x4000,0x4100,7,7,1,0)==0);
    drain(); assert(strstr(s_tdc_ut_capture,"-8388608,-1,8388607"));
    assert(strstr(s_tdc_ut_capture,"[UT-CFX-017] C1 PASS N=3 F=0"));
    s_tdc_ut_bytes=0; s_tdc_ut_capture[0]=0;
    assert(tdc_ut_report_cfx_core_begin("HEAR",0x4000,0x4100,7,7,1,0)==0);
    drain(); assert(strstr(s_tdc_ut_capture,"[UT-HEAR-017]"));
    tdc_ut_report_cancel(); s_tdc_ut_bytes=0; s_tdc_ut_capture[0]=0;
    assert(tdc_ut_report_cfx_observed_begin("CFX",0,7,7,1,0)==-1);
    assert(tdc_ut_report_cfx_observed_begin("CFX",0x13ffc,7,7,1,0)==-1);
    assert(tdc_ut_report_cfx_observed_begin("CFX",0x4000,7,7,0,0)==-1);
    assert(tdc_ut_report_cfx_observed_begin("CFX",0x4000,7,7,1,1)==-1);
    const int numeric[]={-999,23,10,3,-20,-21,-19,-999,23,11,3,40,39,41};
    for(unsigned i=0;i<14;i++) g_tdc_ut_host_xram[0x4000+i]=(uint32_t)numeric[i]&0xffffffU;
    assert(tdc_ut_report_cfx_observed_begin("CFX",0x4000,14,14,1,0)==0);
    assert(tdc_ut_report_cfx_observed_begin("CFX",0x4000,14,14,1,0)==-1);
    drain();
    assert(strstr(s_tdc_ut_capture,"C10 A[0:2]=-20,-21,-19"));
    assert(strstr(s_tdc_ut_capture,"C11 A[0:2]=40,39,41"));
    assert(strstr(s_tdc_ut_capture,"OBSERVED")); assert(!strstr(s_tdc_ut_capture,"PASS"));
    for(unsigned i=0;i<7;i++) g_tdc_ut_host_xram[0x4000+i]=(uint32_t)v[i]&0xffffffU;
    tdc_ut_report_cancel(); s_tdc_ut_bytes=0; s_tdc_ut_capture[0]=0;
    assert(tdc_ut_report_cfx_values_begin("HEAR",2,4,0x4004,3,1)==0);
    drain(); assert(strstr(s_tdc_ut_capture,"OBSERVED")); assert(!strstr(s_tdc_ut_capture,"PASS"));
    assert(strstr(s_tdc_ut_capture,"-8388608,-1,8388607"));
    tdc_ut_report_cancel(); s_tdc_ut_bytes=s_tdc_ut_calls=0; s_tdc_ut_capture[0]=0;
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==0);
    assert(tdc_ut_report_poll()==0); assert(tdc_ut_report_poll()==0);
    assert(s_tdc_ut_bytes==5); /* prefix only */
    tdc_ut_report_cancel();
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==-1);
    drain();
    assert(!strstr(s_tdc_ut_capture,"PASS"));
    assert(tdc_ut_report_begin("CM3",a,e,12,12,1,0)==0); drain();
    for(const char *p=s_tdc_ut_capture; *p;) {
        const char *end=strstr(p,"\r\n"); assert(end && end+2-p<=60); p=end+2;
    }
    puts("REPORT HOST_PASS: metadata, cancellation, 24bit sign, retry, FAIL/PASS");
    return 0;
}
