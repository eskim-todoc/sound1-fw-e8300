/* Link actual IO vectors, drivers, reporter and batch. Reuse the existing
 * hardware-only fixture and its nonzero state-restore assertions unchanged. */
#define main cm3_batch_original_fixture_main
#include "tdc_ut_io_cm3_host.c"
#undef main
#include <tdc_ut_vectors_cm3.h>
#include <tdc_ut_batch_cm3.h>
int g_tdc_ut_cm3_vector_actual[TDC_UT_CM3_VECTOR_CAPACITY];
int g_tdc_ut_cm3_vector_expected[TDC_UT_CM3_VECTOR_CAPACITY];
unsigned g_tdc_ut_cm3_vector_count;
int g_tdc_ut_cm3_vector_overflow, g_tdc_ut_cm3_vector_observed;
const tdc_ut_cm3_observer_ops_t *g_tdc_ut_cm3_observer_ops;
int tdc_ut_cm3_vectors_run_unit(unsigned unit) {(void)unit;abort();}
int tdc_ut_cm3_vectors_observe_calculation_failure(void) {abort();}
void tdc_ut_cm3_vectors_release(void) {abort();}
const int *tdc_ut_cm3_vectors_expected(void) {abort();}
void cm3_batch_forbidden_wait(void) {assert(!"SPI must not wait for an IRQ in the RAM batch");}
static char s_batch_output[65536], s_batch_first[65536];
static unsigned s_batch_output_count, s_batch_writes;
unsigned SEGGER_RTT_Write(unsigned channel,const void *data,unsigned n)
{
    assert(channel==0 && n<=60);
    if (++s_batch_writes%3==1) return 0;
    unsigned accepted=n>7?7:n;
    assert(s_batch_output_count+accepted<sizeof(s_batch_output));
    memcpy(s_batch_output+s_batch_output_count,data,accepted);
    s_batch_output_count+=accepted;s_batch_output[s_batch_output_count]=0;
    return accepted;
}
static void batch_clear(void)
{s_batch_output_count=s_batch_writes=0;s_batch_output[0]=0;}
static void batch_drain(void)
{
    int rc=0;
    for(unsigned i=0;i<20000 && rc==0;++i) rc=tdc_ut_batch_cm3_poll();
    assert(rc==1 && !tdc_ut_batch_cm3_active());
}
int main(void)
{
    static const int units[]={10,25,26,34,35,36,37,40,45,47,50,51};
    static int reference_a[12][512],reference_e[12][512],reference_n[12];
    assert(cm3_batch_original_fixture_main()==0);
    tdc_ut_io_cm3_saved_ram_t before,after;
    capture_product_ram(&before);
    /* Same product inputs in forward and reverse order: fixture state must
     * not leak from a previous unit into the next unit's RAM vectors. */
    for(unsigned order=0;order<2;++order)
        for(unsigned j=0;j<12;++j)
        {
            unsigned i=order?11-j:j;
            assert(tdc_ut_io_cm3_run_unit(units[i])==0);
            unsigned bytes=(unsigned)g_tdc_ut_io_cm3_count*sizeof(int);
            if(!order)
            {
                reference_n[i]=g_tdc_ut_io_cm3_count;
                memcpy(reference_a[i],g_tdc_ut_io_cm3_actual,bytes);
                memcpy(reference_e[i],g_tdc_ut_io_cm3_expected,bytes);
            }
            else
            {
                assert(reference_n[i]==g_tdc_ut_io_cm3_count);
                assert(!memcmp(reference_a[i],g_tdc_ut_io_cm3_actual,bytes));
                assert(!memcmp(reference_e[i],g_tdc_ut_io_cm3_expected,bytes));
            }
            assert(tdc_ut_io_cm3_release()==0);
            capture_product_ram(&after);assert(!memcmp(&before,&after,sizeof(before)));
        }
    for(unsigned pass=0;pass<2;++pass)
    {
        batch_clear();assert(tdc_ut_batch_cm3_begin(3,1)==0);batch_drain();
        assert(tdc_ut_batch_cm3_completed()==12);
        assert(strstr(s_batch_output,"[UT-CM3-051] C17 I0 A=195 E=240\r\n"));
        assert(strstr(s_batch_output,"[UT-CM3-051] C17 I1 A=0 E=3\r\n"));
        assert(strstr(s_batch_output,"[UT-CM3-051] C18 I0 A=195 E=240\r\n"));
        assert(strstr(s_batch_output,"[UT-CM3-051] C18 I1 A=0 E=4\r\n"));
        assert(strstr(s_batch_output,"[UT-BATCH] CORE=CM3 END N=12\r\n"));
        capture_product_ram(&after);assert(!memcmp(&before,&after,sizeof(before)));
        if(pass==0) strcpy(s_batch_first,s_batch_output);else assert(!strcmp(s_batch_first,s_batch_output));
    }
    batch_clear();assert(tdc_ut_batch_cm3_begin_capture(1,27,1)==0);
    assert(s_batch_output_count==0);batch_drain();
    assert(tdc_ut_batch_cm3_completed()==24);
    assert(!strstr(s_batch_output,"PASS") && !strstr(s_batch_output,"FAIL"));
    assert(strstr(s_batch_output,"[UT-BATCH] CORE=CM3 END N=24\r\n"));
    for(const char *p=s_batch_output;*p;)
    {const char *e=strstr(p,"\r\n");assert(e && e+2-p<=60);p=e+2;}
    capture_product_ram(&after);assert(!memcmp(&before,&after,sizeof(before)));
    puts("CM3_BATCH REAL_IO PASS: 12 repeated/forward/reverse runs, 24 frozen snapshots, restored state, 4 known mismatch values retained, no WFE");
    return 0;
}
