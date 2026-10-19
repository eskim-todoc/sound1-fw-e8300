/* Batch control fault injection with the real reporter/logger. No device I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tdc_ut_boot.h"
#include "tdc_ut_boot_report.h"
#include "tdc_ut_batch_boot.h"

int g_tdc_ut_boot_actual[TDC_UT_BOOT_CAPACITY];
int g_tdc_ut_boot_expected[TDC_UT_BOOT_CAPACITY];
unsigned g_tdc_ut_boot_count;
int g_tdc_ut_boot_done, g_tdc_ut_boot_overflow;
static char output[32768];
static unsigned length, calls, limit=60, zero_count, over_count;
static unsigned pending, runs, releases, units[32], fault_unit, fault;
enum { FAIL_EXEC=1, OVERFLOW, WRONG_UNIT, WRONG_HEADER, EMPTY, UNFINISHED,
       BAD_LENGTH, BAD_CASE, SHORT_RECORD, MISSING_VECTOR, DUPLICATE_VECTOR };

unsigned SEGGER_RTT_Write(unsigned channel, const void *data, unsigned n)
{
    unsigned accepted;
    assert(channel == 0 && n <= 60 && n > 0);
    ++calls;
    if (over_count) { --over_count; return n+1; }
    if (zero_count) { --zero_count; return 0; }
    accepted=n < limit ? n : limit;
    assert(length+accepted < sizeof(output));
    memcpy(output+length,data,accepted); length+=accepted; output[length]=0;
    return accepted;
}
int tdc_ut_boot_run_unit(unsigned unit)
{
    const unsigned ids[]={8,9,10,11,12,13,14,19,20};
    const unsigned counts[]={6,5,13,14,11,10,4,10,7};
    unsigned count=0, first=unit==9 || unit==14 ? 1:101;
    if (pending) return TDC_UT_BOOT_BUSY;
    assert(!tdc_ut_boot_report_busy());
    assert(runs < sizeof(units)/sizeof(units[0]));
    units[runs++]=unit;
    if (unit == fault_unit && fault == FAIL_EXEC) return TDC_UT_BOOT_UNSUPPORTED;
    for(unsigned i=0; i<9; ++i)if(unit==ids[i])count=counts[i];
    assert(count>0);
    pending=1; g_tdc_ut_boot_done=1; g_tdc_ut_boot_count=count*5;
    g_tdc_ut_boot_overflow=0;
    for(unsigned i=0; i<count; ++i) {
        unsigned pos=5*i;
        int mismatch=unit==11 && first+i==108;
        g_tdc_ut_boot_actual[pos]=g_tdc_ut_boot_expected[pos]=-999;
        g_tdc_ut_boot_actual[pos+1]=g_tdc_ut_boot_expected[pos+1]=(int)unit;
        g_tdc_ut_boot_actual[pos+2]=g_tdc_ut_boot_expected[pos+2]=(int)(first+i);
        g_tdc_ut_boot_actual[pos+3]=g_tdc_ut_boot_expected[pos+3]=1;
        g_tdc_ut_boot_actual[pos+4]=mismatch ? 0:(int)unit;
        g_tdc_ut_boot_expected[pos+4]=mismatch ? -1:(int)unit;
    }
    if (unit == fault_unit) switch (fault) {
    case OVERFLOW: g_tdc_ut_boot_overflow=1; return TDC_UT_BOOT_ERROR;
    case WRONG_UNIT: g_tdc_ut_boot_actual[1]=g_tdc_ut_boot_expected[1]=14; break;
    case WRONG_HEADER: g_tdc_ut_boot_expected[0]=0; break;
    case EMPTY: g_tdc_ut_boot_count=0; break;
    case UNFINISHED: g_tdc_ut_boot_done=0; break;
    case BAD_LENGTH: g_tdc_ut_boot_actual[3]=g_tdc_ut_boot_expected[3]=128; break;
    case BAD_CASE: g_tdc_ut_boot_actual[2]=g_tdc_ut_boot_expected[2]=0; break;
    case SHORT_RECORD: g_tdc_ut_boot_count=3; break;
    case MISSING_VECTOR: g_tdc_ut_boot_count-=5; break;
    case DUPLICATE_VECTOR: g_tdc_ut_boot_actual[7]=g_tdc_ut_boot_expected[7]=(int)first; break;
    default: break;
    }
    return 0;
}
void tdc_ut_boot_release(void)
{
    assert(pending && !tdc_ut_boot_report_busy());
    pending=0; ++releases; /* Deliberately leave public done/count unchanged. */
}

static int poll_once(void)
{
    unsigned previous=calls;
    int ret=tdc_ut_boot_batch_poll();
    assert(calls-previous <= 1);
    assert(tdc_ut_boot_batch_busy() == (ret == 0));
    assert(tdc_ut_boot_batch_get_status()->status == ret);
    return ret;
}
static int drain(void)
{
    int ret=0;
    for (unsigned i=0; i<10000 && ret==0; ++i) ret=poll_once();
    assert(ret != 0);
    return ret;
}
static void check_lines(void)
{
    const char *p=output;
    while (*p) {
        const char *end=strstr(p,"\r\n");
        assert(end && end+2-p <= 60);
        for (const char *q=p; q<end; ++q) assert(*q>=32 && *q<=126);
        p=end+2;
    }
}
static void clear_capture(void)
{
    assert(!tdc_ut_boot_batch_busy() && !tdc_ut_boot_report_busy() && !pending);
    check_lines();
    output[0]=0; length=calls=runs=releases=0;
    limit=60; zero_count=over_count=0; fault=fault_unit=0;
}
static void clean_result(void)
{ g_tdc_ut_boot_count=0; g_tdc_ut_boot_done=0; g_tdc_ut_boot_overflow=0; }
static void start(unsigned group)
{
    assert(tdc_ut_boot_batch_begin(group,1)==0);
    assert(tdc_ut_boot_batch_busy());
    assert(tdc_ut_boot_batch_begin(group,1)==TDC_UT_BOOT_BATCH_BUSY);
    assert(tdc_ut_boot_batch_get_status()->group==group);
    assert(tdc_ut_boot_batch_run(0)==0 && calls==0);
}
static void expect_stop(int result, const char *kind)
{
    assert(drain()==result);
    assert(!strstr(output," CORE=BOOT END "));
    assert(strstr(output,kind));
    assert(!tdc_ut_boot_batch_busy() && !tdc_ut_boot_report_busy());
    unsigned previous=calls;
    assert(poll_once()==result && tdc_ut_boot_batch_run(20)==result);
    tdc_ut_boot_batch_cancel();
    assert(previous==calls);
    check_lines();
}
int main(void)
{
    const unsigned all[]={8,9,10,11,12,13,14,19,20};
    const unsigned file[]={8,9,10,11,12,19,20};
    assert(poll_once()==TDC_UT_BOOT_BATCH_IDLE);
    tdc_ut_boot_batch_cancel();
    assert(tdc_ut_boot_batch_run(10)==TDC_UT_BOOT_BATCH_IDLE && calls==0);
    for (int ready=-1; ready<=2; ++ready) if (ready!=1)
        assert(tdc_ut_boot_batch_begin(1,ready)==TDC_UT_BOOT_BATCH_NOT_READY);
    assert(tdc_ut_boot_batch_begin(0,1)==TDC_UT_BOOT_BATCH_BAD_GROUP);
    assert(tdc_ut_boot_batch_begin(4,1)==TDC_UT_BOOT_BATCH_BAD_GROUP);
    for (unsigned stale=0; stale<3; ++stale) {
        clean_result();
        if(stale==0)g_tdc_ut_boot_done=1;
        if(stale==1)g_tdc_ut_boot_count=1;
        if(stale==2)g_tdc_ut_boot_overflow=1;
        assert(tdc_ut_boot_batch_begin(1,1)==TDC_UT_BOOT_BATCH_STALE && releases==0);
    }
    clean_result();
    /* A full RTT buffer is not a terminal condition or a lost result. */
    start(1); limit=0;
    assert(tdc_ut_boot_batch_run(1000)==0 && runs==0 && length==0);
    tdc_ut_boot_batch_cancel();
    assert(tdc_ut_boot_batch_run(1000)==0 && tdc_ut_boot_batch_busy());
    limit=60;
    expect_stop(-4,"[UT-BATCH] CORE=BOOT CANCEL N=0\r\n");
    /* All nine, repeat, budget expiry/resume and mismatch must not stop N=9. */
    for (unsigned repeat=0; repeat<2; ++repeat) {
        clear_capture(); start(1); limit=3; zero_count=2;
        assert(tdc_ut_boot_batch_run(1)==0 && runs==0);
        assert(drain()==1 && runs==9 && releases==9);
        assert(memcmp(units,all,sizeof(all))==0);
        assert(strstr(output,"[UT-BATCH] CORE=BOOT BEGIN N=9\r\n"));
        assert(strstr(output,"[UT-BATCH] CORE=BOOT END N=9\r\n"));
        assert(strstr(output,"[UT-BOOT-011] C108 I0 A=0 E=-1\r\n"));
        assert(strstr(output,"[UT-BOOT-011] C108 FAIL N=1 F=1\r\n"));
        assert(!strstr(output,"[UT-BATCH] CORE=BOOT PASS"));
        assert(!g_tdc_ut_boot_done && !g_tdc_ut_boot_count);
        check_lines();
    }
    clear_capture(); start(2);
    assert(tdc_ut_boot_batch_run(10000)==1 && runs==7);
    assert(memcmp(units,file,sizeof(file))==0);
    clear_capture(); start(3);
    assert(drain()==1 && runs==2 && units[0]==13 && units[1]==14);

    /* Reject another unit's result; do not mistake done for release state. */
    clear_capture(); assert(tdc_ut_boot_run_unit(9)==0);
    assert(tdc_ut_boot_batch_begin(1,1)==TDC_UT_BOOT_BATCH_STALE);
    assert(pending && releases==0);
    assert(tdc_ut_boot_report_begin()==0);
    assert(tdc_ut_boot_report_busy());
    clean_result(); /* A misbehaving outside owner cannot hide an active drain. */
    assert(tdc_ut_boot_batch_begin(1,1)==TDC_UT_BOOT_BATCH_STALE);
    tdc_ut_boot_report_cancel();
    while(tdc_ut_boot_report_poll()!=1) {}
    tdc_ut_boot_release(); clean_result(); clear_capture();
    /* Private pending with cleared public flags: run_unit rejects; no steal. */
    pending=1; start(1);
    expect_stop(-1,"[UT-BATCH] CORE=BOOT ERROR N=0\r\n");
    assert(pending && releases==0 && runs==0);
    pending=0; clear_capture();

    /* Cancel before output, after a BEGIN prefix, at report prefixes and CR. */
    for (unsigned prefix=0; prefix<32; ++prefix) {
        clear_capture(); start(1); limit=1;
        for(unsigned i=0; i<prefix; ++i)assert(poll_once()==0);
        tdc_ut_boot_batch_cancel(); tdc_ut_boot_batch_cancel();
        zero_count=3; assert(tdc_ut_boot_batch_run(3)==0);
        limit=2;
        expect_stop(-4,"[UT-BATCH] CORE=BOOT CANCEL N=0\r\n");
        assert(runs==0 && releases==0);
    }
    for (unsigned prefix=1; prefix<strlen("[UT-BOOT-008] C101 A[0:0]=8\r\n"); ++prefix) {
        clear_capture(); start(1);
        assert(poll_once()==0 && poll_once()==0); /* BEGIN, unit. */
        assert(runs==1 && pending && tdc_ut_boot_report_busy());
        limit=1;
        for(unsigned i=0; i<prefix; ++i)assert(poll_once()==0);
        tdc_ut_boot_batch_cancel(); tdc_ut_boot_batch_cancel();
        zero_count=3; assert(tdc_ut_boot_batch_run(3)==0);
        assert(pending && releases==0); limit=3;
        expect_stop(-4,"[UT-BATCH] CORE=BOOT CANCEL N=0\r\n");
        assert(runs==1 && releases==1 && !pending);
    }
    /* Cancel between units, then rerun from the first unit. */
    clear_capture(); start(1);
    while(tdc_ut_boot_batch_get_status()->completed<1)assert(poll_once()==0);
    tdc_ut_boot_batch_cancel();
    expect_stop(-4,"[UT-BATCH] CORE=BOOT CANCEL N=1\r\n");
    assert(runs==1 && releases==1);
    clear_capture(); start(1); assert(drain()==1 && runs==9);

    /* END is only committed after every unit is drained; a cancel during an
     * accepted END prefix cannot retract it, so drain that valid END. */
    clear_capture(); start(3);
    while(tdc_ut_boot_batch_get_status()->completed<2)assert(poll_once()==0);
    limit=1; assert(poll_once()==0); tdc_ut_boot_batch_cancel();
    assert(drain()==1 && strstr(output,"[UT-BATCH] CORE=BOOT END N=2\r\n"));
    assert(!strstr(output,"CANCEL"));
    clear_capture(); start(3);
    while(tdc_ut_boot_batch_get_status()->completed<2)assert(poll_once()==0);
    tdc_ut_boot_batch_cancel();
    expect_stop(-4,"[UT-BATCH] CORE=BOOT CANCEL N=2\r\n");

    /* Execution/metadata faults stop instead of manufacturing an END. */
    for(unsigned f=FAIL_EXEC; f<=DUPLICATE_VECTOR; ++f) {
        clear_capture(); fault=f; fault_unit=9; start(1);
        expect_stop(-1,"[UT-BATCH] CORE=BOOT ERROR N=1\r\n");
        assert(runs==2 && releases==(f==FAIL_EXEC ? 1U:2U) && !pending);
    }
    /* Writer protocol failure with partial unit line. Error must stay busy
     * while its CRLF cannot be drained, then finish ERROR before return -1. */
    clear_capture(); start(1); assert(poll_once()==0 && poll_once()==0);
    limit=3; assert(poll_once()==0); over_count=1;
    assert(poll_once()==0 && pending);
    zero_count=4; assert(tdc_ut_boot_batch_run(4)==0 && pending);
    expect_stop(-1,"[UT-BATCH] CORE=BOOT ERROR N=0\r\n");
    assert(releases==1);
    clear_capture(); start(1); limit=3; assert(poll_once()==0); over_count=1;
    assert(poll_once()==0); expect_stop(-1,"[UT-BATCH] CORE=BOOT ERROR N=0\r\n");
    assert(runs==0);
    clear_capture(); start(1); tdc_ut_boot_batch_cancel();
    assert(poll_once()==0); over_count=1; assert(poll_once()==0);
    expect_stop(-1,"[UT-BATCH] CORE=BOOT ERROR N=0\r\n");
    clear_capture(); start(1); assert(drain()==1);
    check_lines();
    puts("BOOT_BATCH_STATE HOST_PASS: groups/ownership/retry/cancel/error/FAIL preserved");
    return 0;
}
