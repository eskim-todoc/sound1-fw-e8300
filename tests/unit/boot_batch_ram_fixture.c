/* Reuse the proven RAM device boundaries and restoration sentinels without
 * modifying their shared fixture. Product functions and batch are linked
 * separately; the original main is retained for independent single-unit runs.
 */
#define main boot_batch_original_main
#include "tdc_ut_boot_host.c"
#undef main
#include "tdc_ut_batch_boot.h"
#include "tdc_ut_boot_report.h"
#include <assert.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

static char s_batch_output[65536]; /* Host capture only, not target code. */
static unsigned s_batch_length, s_batch_calls, s_batch_limit=7;

unsigned SEGGER_RTT_Write(unsigned channel, const void *data, unsigned bytes)
{
    unsigned n;
    assert(channel==0 && bytes<=60 && bytes>0);
    if(s_batch_calls++ % 7U == 0U) return 0;
    n=bytes < s_batch_limit ? bytes:s_batch_limit;
    assert(s_batch_length+n<sizeof(s_batch_output));
    memcpy(s_batch_output+s_batch_length,data,n);
    s_batch_length+=n; s_batch_output[s_batch_length]=0;
    return n;
}

static void batch_capture(unsigned group, int cancel)
{
    int rc=0;
    unsigned previous=0, cancelled=0;
    s_batch_length=s_batch_calls=0; s_batch_output[0]=0;
    assert(tdc_ut_boot_batch_begin(group,1)==0);
    assert(tdc_ut_boot_batch_begin(group,1)==TDC_UT_BOOT_BATCH_BUSY);
    for(unsigned i=0; i<30000 && rc==0; ++i) {
        unsigned calls=s_batch_calls;
        rc=tdc_ut_boot_batch_run(1);
        assert(s_batch_calls-calls<=1);
        const tdc_ut_boot_batch_status_t *status=tdc_ut_boot_batch_get_status();
        if(status->completed!=previous) { ram_state_verify(); previous=status->completed; }
        if(cancel && !cancelled && status->unit==11 && status->completed==3 &&
           s_batch_length && s_batch_output[s_batch_length-1]!='\n') {
            tdc_ut_boot_batch_cancel(); cancelled=1;
        }
    }
    assert(rc==(cancel ? TDC_UT_BOOT_BATCH_CANCELLED:TDC_UT_BOOT_BATCH_DONE));
    assert(!tdc_ut_boot_batch_busy() && !tdc_ut_boot_report_busy());
    assert(!g_tdc_ut_boot_done && !g_tdc_ut_boot_count && !g_tdc_ut_boot_overflow);
    ram_state_verify();
    fputs(s_batch_output,stdout);
    printf("HOST_STATUS G=%u RC=%d COMPLETED=%u\n",group,rc,
           tdc_ut_boot_batch_get_status()->completed);
}

int main(int argc,char **argv)
{
#if defined(_WIN32)
    /* RTT already supplies CRLF; host stdio must not add a second CR. */
    assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
    if(argc==3 && strcmp(argv[1],"direct")==0) {
        char *args[]={argv[0],"",argv[2]};
        return boot_batch_original_main(3,args);
    }
    if(argc!=2)return 2;
    ram_state_prepare();
    if(strcmp(argv[1],"repeat")==0) { batch_capture(1,0); batch_capture(1,0); }
    else if(strcmp(argv[1],"groups")==0) { batch_capture(2,0); batch_capture(3,0); }
    else if(strcmp(argv[1],"cancel")==0) { batch_capture(1,1); batch_capture(1,0); }
    else return 2;
    ram_state_verify();
    return 0;
}
