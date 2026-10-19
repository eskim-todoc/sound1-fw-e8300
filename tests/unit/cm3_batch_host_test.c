/* Actual batch and reporter, simulated finite producers. Product behaviour is
 * separately checked by cm3 vectors and RAM IO fixtures, not by these stubs. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cm3_batch_host/tdc_shm.h"
#define TDC_UT_IO_CM3_H
#define TDC_UT_VECTORS_CM3_H
#define TDC_UT_IO_CM3_CAPACITY 512
#define TDC_UT_CM3_VECTOR_CAPACITY 256
int g_tdc_ut_cm3_vector_actual[256], g_tdc_ut_cm3_vector_expected[256];
unsigned g_tdc_ut_cm3_vector_count;
int g_tdc_ut_cm3_vector_overflow, g_tdc_ut_cm3_vector_observed;
volatile int g_tdc_ut_cm3_vector_done;
void *g_tdc_ut_cm3_observer_ops;
int g_tdc_ut_io_cm3_actual[512], g_tdc_ut_io_cm3_expected[512];
int g_tdc_ut_io_cm3_count, g_tdc_ut_io_cm3_overflow, g_tdc_ut_io_cm3_observed;
volatile int g_tdc_ut_io_cm3_done;
unsigned g_batch_ipsr;
uint32_t g_tdc_ut_host_xram[0x14000];
batch_shared_t cfx_cm3_sharedMemoryAll;
static int spi_idle=1, i2c_idle=1, fault, input_step=73;
static unsigned history[128], history_count, releases, snapshot_calls;
static char output[131072];
static unsigned output_count, writes, limit=7, blocked, pulse_block=1;
int tdc_hal_spi_is_tx_buffer_empty(void) { return spi_idle; }
int tdc_hal_i2c_is_driver_status_idle(void) { return i2c_idle; }
unsigned SEGGER_RTT_Write(unsigned channel,const void *data,unsigned n)
{
    assert(channel==0 && n<=60 && !g_batch_ipsr);
    ++writes;
    if (blocked || (pulse_block && writes%3==1)) return 0;
    unsigned accepted=n>limit?limit:n;
    assert(output_count+accepted<sizeof(output));
    memcpy(output+output_count,data,accepted);
    output_count+=accepted; output[output_count]=0;
    return accepted;
}
static void record(int *a,int *e,unsigned unit,unsigned vector,int observed)
{
    assert(!g_tdc_ut_cm3_vector_done && !g_tdc_ut_io_cm3_done);
    a[0]=-999; a[1]=(int)unit; a[2]=(int)vector; a[3]=1; a[4]=observed?input_step:7;
    memcpy(e,a,5*sizeof(int));
    if(unit==6) e[4]=8; /* A real mismatch must remain visible, not stop END. */
    if(fault==1) a[0]=0;
    if(fault==2) e[3]=2;
    history[history_count++]=unit;
    assert(history_count<=128);
}
int tdc_ut_cm3_vectors_run_unit(unsigned unit)
{
    record(g_tdc_ut_cm3_vector_actual,g_tdc_ut_cm3_vector_expected,unit,1,0);
    g_tdc_ut_cm3_vector_count=5; g_tdc_ut_cm3_vector_done=1;
    g_tdc_ut_cm3_vector_overflow=fault==3;
    g_tdc_ut_cm3_vector_observed=0;
    if(fault==4) g_tdc_ut_cm3_vector_done=0;
    return fault==5?-4:(unit==4?1:0);
}
int tdc_ut_cm3_vectors_observe_calculation_failure(void)
{
    int rc=tdc_ut_cm3_vectors_run_unit(29);
    g_tdc_ut_cm3_vector_actual[2]=g_tdc_ut_cm3_vector_expected[2]=91;
    g_tdc_ut_cm3_vector_observed=1; return rc<0?rc:2;
}
const int *tdc_ut_cm3_vectors_expected(void)
{return g_tdc_ut_cm3_vector_observed?0:g_tdc_ut_cm3_vector_expected;}
void tdc_ut_cm3_vectors_release(void) {g_tdc_ut_cm3_vector_done=0; ++releases;}
int tdc_ut_io_cm3_run_unit(int unit)
{
    record(g_tdc_ut_io_cm3_actual,g_tdc_ut_io_cm3_expected,(unsigned)unit,1,0);
    g_tdc_ut_io_cm3_count=5; g_tdc_ut_io_cm3_done=1;
    g_tdc_ut_io_cm3_overflow=0; g_tdc_ut_io_cm3_observed=0; return 0;
}
int tdc_ut_io_cm3_observe_unit(int unit,int case_id)
{
    int rc=tdc_ut_io_cm3_run_unit(unit); ++snapshot_calls;
    g_tdc_ut_io_cm3_actual[2]=case_id; g_tdc_ut_io_cm3_actual[4]=input_step;
    g_tdc_ut_io_cm3_observed=1;
    if(fault==6) {g_tdc_ut_io_cm3_count=512;g_tdc_ut_io_cm3_actual[3]=508;}
    return rc;
}
int tdc_ut_io_cm3_release(void) {g_tdc_ut_io_cm3_done=0;++releases;return 0;}
#include "../../src/2__cm3/source/tests/tdc_ut_report.c"
#include "../../src/2__cm3/source/tests/tdc_ut_batch_cm3.c"

static void clear_capture(void)
{
    assert(!tdc_ut_batch_cm3_active());
    output_count=writes=history_count=releases=snapshot_calls=0;
    output[0]=0; fault=0; limit=7; pulse_block=1; blocked=0;
}
static int drain(void)
{
    int rc=0;
    for(unsigned i=0;i<20000 && rc==0;++i) rc=tdc_ut_batch_cm3_poll();
    assert(rc!=0 && !tdc_ut_batch_cm3_active());
    assert(tdc_ut_batch_cm3_poll()==rc);
    return rc;
}
static void lines(void)
{
    for(const char *p=output;*p;)
    {
        const char *e=strstr(p,"\r\n"); assert(e && e+2-p<=60);
        for(const char *q=p;q<e;++q) assert(*q>=32 && *q<=126);
        p=e+2;
    }
}
static void all_runs(void)
{
    static const unsigned expected[]={4,6,7,9,11,12,13,16,17,18,19,20,21,22,23,24,27,28,29,30,44,29,10,25,26,34,35,36,37,40,45,47,50,51};
    static char first[131072];
    for(unsigned repeat=0;repeat<2;++repeat)
    {
        clear_capture();
        assert(tdc_ut_batch_cm3_begin(1,1)==0);
        assert(tdc_ut_batch_cm3_selected()==34);
        assert(tdc_ut_batch_cm3_begin(2,1)==-2);
        assert(tdc_ut_batch_cm3_run(0)==-1 && tdc_ut_batch_cm3_run(4097)==-1);
        blocked=1; assert(tdc_ut_batch_cm3_run(20)==0 && writes==20 && history_count==0);
        blocked=0; assert(drain()==1); lines();
        assert(history_count==34 && releases==34 && !memcmp(history,expected,sizeof(expected)));
        assert(strstr(output,"[UT-CM3-006] C1 I0 A=7 E=8\r\n"));
        assert(strstr(output,"[UT-CM3-006] C1 FAIL N=1 F=1\r\n"));
        assert(strstr(output,"[UT-CM3-029] C91 OBSERVED N=1 F=0\r\n"));
        assert(strstr(output,"[UT-BATCH] CORE=CM3 BEGIN N=34\r\n"));
        assert(strstr(output,"[UT-BATCH] CORE=CM3 END N=34\r\n"));
        if(!repeat) strcpy(first,output); else assert(!strcmp(first,output));
    }
    for(unsigned group=3;group>=2;--group)
    {
        clear_capture();assert(tdc_ut_batch_cm3_begin(group,1)==0);assert(drain()==1);
        unsigned count=group==3?12:22;
        assert(history_count==count && releases==count && tdc_ut_batch_cm3_completed()==count);
        assert(!memcmp(history,expected+(group==3?22:0),count*sizeof(unsigned)));
    }
}
static void rejection(void)
{
    int a[]={-999,80,1,1,2};
    clear_capture();
    assert(tdc_ut_batch_cm3_begin(0,1)==-1 && tdc_ut_batch_cm3_begin(4,1)==-1);
    assert(tdc_ut_batch_cm3_begin(1,0)==-3 && tdc_ut_batch_cm3_begin(1,2)==-3);
    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration=1;
    assert(tdc_ut_batch_cm3_begin(1,1)==-3);cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration=0;
    spi_idle=0;assert(tdc_ut_batch_cm3_begin(1,1)==-3);spi_idle=1;
    i2c_idle=0;assert(tdc_ut_batch_cm3_begin(1,1)==-3);i2c_idle=1;
    g_tdc_ut_cm3_observer_ops=(void*)1;assert(tdc_ut_batch_cm3_begin(1,1)==-3);g_tdc_ut_cm3_observer_ops=0;
    g_tdc_ut_cm3_vector_done=1;assert(tdc_ut_batch_cm3_begin(1,1)==-2);assert(g_tdc_ut_cm3_vector_done);g_tdc_ut_cm3_vector_done=0;
    g_tdc_ut_io_cm3_done=1;assert(tdc_ut_batch_cm3_begin(1,1)==-2);assert(g_tdc_ut_io_cm3_done);g_tdc_ut_io_cm3_done=0;
    assert(tdc_ut_report_begin("CM3",a,a,5,5,1,0)==0);
    assert(tdc_ut_batch_cm3_begin(1,1)==-2);
    while(tdc_ut_report_poll()!=1) {}
    assert(strstr(output,"[UT-CM3-080] C1 PASS"));tdc_ut_report_cancel();
    g_batch_ipsr=3;assert(tdc_ut_batch_cm3_begin(1,1)==-3);g_batch_ipsr=0;
    assert(history_count==0);
}
static void cancellation(void)
{
    for(unsigned cut=0;cut<145;++cut)
    {
        clear_capture();limit=1;pulse_block=0;
        assert(tdc_ut_batch_cm3_begin(2,1)==0);
        for(unsigned i=0;i<cut;++i) assert(tdc_ut_batch_cm3_poll()==0);
        assert(tdc_ut_batch_cm3_cancel()==0 && tdc_ut_batch_cm3_cancel()==0);
        assert(tdc_ut_batch_cm3_active());
        blocked=1;assert(tdc_ut_batch_cm3_run(7)==0 && tdc_ut_batch_cm3_active());blocked=0;
        assert(drain()==TDC_UT_BATCH_CM3_CANCELLED);lines();
        assert(strstr(output,"[UT-BATCH] CORE=CM3 ABORT N="));
        assert(!strstr(output,"[UT-BATCH] CORE=CM3 END"));
        assert(!g_tdc_ut_cm3_vector_done && !g_tdc_ut_io_cm3_done);
    }
    clear_capture();limit=60;pulse_block=0;
    assert(tdc_ut_batch_cm3_begin(3,1)==0);
    while(tdc_ut_batch_cm3_completed()!=12) assert(tdc_ut_batch_cm3_poll()==0);
    limit=1;assert(tdc_ut_batch_cm3_poll()==0);
    assert(tdc_ut_batch_cm3_cancel()==TDC_UT_BATCH_CM3_BUSY);
    assert(drain()==1);lines();
    assert(!strstr(output,"ABORT"));
}
static void errors(void)
{
    for(int f=1;f<=5;++f)
    {
        clear_capture();fault=f;
        assert(tdc_ut_batch_cm3_begin(2,1)==0);
        assert(drain()<0);lines();
        assert(!strstr(output,"END") && !strstr(output,"PASS"));
        assert(strstr(output,"[UT-BATCH] CORE=CM3 ERROR N=0"));
    }
    clear_capture();assert(tdc_ut_batch_cm3_begin(1,1)==0);
    while(tdc_ut_batch_cm3_completed()!=1) assert(tdc_ut_batch_cm3_poll()==0);
    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration=1;
    assert(drain()==-3);cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration=0;
    assert(strstr(output,"ERROR N=1") && !strstr(output,"END"));
    clear_capture();assert(tdc_ut_batch_cm3_begin(2,1)==0);
    g_batch_ipsr=1;unsigned before=writes;
    assert(tdc_ut_batch_cm3_poll()==-3 && tdc_ut_batch_cm3_cancel()==-3 && writes==before);
    assert(tdc_ut_batch_cm3_active());g_batch_ipsr=0;
    assert(tdc_ut_batch_cm3_cancel()==0);assert(drain()==-5);
}
static void captures(void)
{
    for(unsigned group=1;group<=3;++group)
    {
        clear_capture();input_step=73;spi_idle=i2c_idle=0;
        assert(tdc_ut_batch_cm3_begin_capture(group,14,1)==0);
        unsigned count=group==1?24:group==2?17:7;
        assert(snapshot_calls==count && releases==count && writes==0);
        input_step=99; /* Captured values must not follow later input changes. */
        assert(drain()==1);lines();
        assert(!strstr(output,"PASS") && !strstr(output,"FAIL"));
        assert(strstr(output,"C14 A[0:0]=73") && !strstr(output,"A[0:0]=99"));
        assert(tdc_ut_batch_cm3_completed()==count);
        spi_idle=i2c_idle=1;
    }
    clear_capture();assert(tdc_ut_batch_cm3_begin_capture(1,0,1)==-1);
    assert(tdc_ut_batch_cm3_begin_capture(1,1000,1)==-1);
    fault=6;assert(tdc_ut_batch_cm3_begin_capture(1,1,1)==-4);
    assert(!tdc_ut_batch_cm3_active() && writes==0 && !g_tdc_ut_io_cm3_done);
}
int main(void)
{
    assert(tdc_ut_batch_cm3_poll()==TDC_UT_BATCH_CM3_IDLE);
    rejection();all_runs();cancellation();errors();captures();
    puts("CM3_BATCH PASS: groups/order/repeat, 60B/partial/backpressure, cancel/tdc_sys_error, snapshots; no device execution");
    return 0;
}
