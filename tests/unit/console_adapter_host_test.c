/* Compile the actual image-specific wrapper and its actual included console.
 * Only batch lifecycle, MMIO guard and RTT transport are simulated here.
 * Deliberate vector FAIL text must not be mistaken for a failed lifecycle.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "console_adapter_host/cfx_cm3_sharedMemory.h"
#include "../../src/test/tdc_ut_console.h"
#include "../../src/2__cm3/Cortex-M3-src/test/tdc_ut_batch_cm3.h"
#include "../../src/2__cm3/Cortex-M3-src/test/tdc_ut_batch_cfx_bridge.h"
#include "../../src/0__bootloader/source/test/tdc_ut_batch_boot.h"

unsigned g_console_adapter_ipsr;
console_adapter_shared_t cfx_cm3_sharedMemoryAll;
static char output[131072], events[256];
static unsigned output_n, event_n, write_calls, read_calls, watchdog_calls;
static unsigned write_limit=5, block_writes, intermittent=1;
static int invalid_write_count;
static const char *input="";
static int bridge_restored=1;

typedef struct {
    int busy, phase, allow_complete, cancelling, terminal_error, observed;
    int end_committed;
    unsigned starts, cancels, polls, group, step, session;
    char core[5], line[61];
    unsigned line_n,line_sent;
} fake_batch_t;
static fake_batch_t cm3,bridge,boot;

unsigned SEGGER_RTT_Write(unsigned channel,const void *data,unsigned n)
{
    assert(channel==0 && n<=60);++write_calls;
    if(invalid_write_count)return n+1;
    if(block_writes || (intermittent && write_calls%4==1)) return 0;
    unsigned accepted=n>write_limit?write_limit:n;
    assert(output_n+accepted<sizeof(output));
    memcpy(output+output_n,data,accepted);output_n+=accepted;output[output_n]=0;
    return accepted;
}
unsigned SEGGER_RTT_Read(unsigned channel,void *data,unsigned n)
{
    assert(channel==0 && n==1);++read_calls;
    if(!*input)return 0;
    *(char*)data=*input++;return 1;
}
void console_adapter_watchdog(void)
{
    assert(++watchdog_calls<20000); /* Bound serve even if quit is broken. */
}
static void event(char value)
{assert(event_n+1<sizeof(events));events[event_n++]=value;events[event_n]=0;}
static void line(fake_batch_t *b,const char *what)
{
    int n=snprintf(b->line,sizeof(b->line),"[UT-BATCH] CORE=%s %s N=1\r\n",b->core,what);
    assert(n>0 && n<=60);b->line_n=(unsigned)n;b->line_sent=0;
}
static int begin_fake(fake_batch_t *b,unsigned group,unsigned step,const char *core,char tag)
{
    if(b->busy || group<1 || group>3)return -2;
    b->busy=1;b->phase=0;b->allow_complete=0;b->cancelling=0;b->terminal_error=0;
    b->end_committed=0;b->observed=step!=0;b->group=group;b->step=step;
    b->line_n=b->line_sent=0;strcpy(b->core,core);++b->starts;event(tag);
    return 0;
}
static int send_fake(fake_batch_t *b)
{
    unsigned left=b->line_n-b->line_sent;
    unsigned n=SEGGER_RTT_Write(0,b->line+b->line_sent,left);
    assert(n<=left);b->line_sent+=n;
    if(b->phase==4 && n)b->end_committed=1;
    return b->line_sent==b->line_n;
}
static int poll_fake(fake_batch_t *b)
{
    ++b->polls;
    if(!b->busy)return b->terminal_error?b->terminal_error:1;
    /* Cancel finishes any current line before ABORT. Production batch cancel
     * may truncate a line; either way a terminal result owns a full boundary. */
    if(b->line_sent<b->line_n)
    {
        if(!send_fake(b))return 0;
        b->line_n=b->line_sent=0;
        if(b->phase==4 || b->phase==5)
        {
            int result=b->phase==5?-5:(b->terminal_error?b->terminal_error:1);
            b->busy=0;event(b==&cm3?'c':b==&bridge?'f':'b');return result;
        }
    }
    if(b->cancelling){line(b,"ABORT");b->phase=5;return 0;}
    if(b->phase==0){line(b,"BEGIN");b->phase=1;return 0;}
    if(b->phase==1)
    {
        const char *result=b->observed?"[UT-CM3-045] C1 OBSERVED N=1 F=0\r\n":
                                         "[UT-CM3-006] C1 FAIL N=1 F=1\r\n";
        strcpy(b->line,result);b->line_n=(unsigned)strlen(result);b->line_sent=0;b->phase=2;return 0;
    }
    if(!b->allow_complete)return 0;
    line(b,b->terminal_error?"ERROR":"END");b->phase=4;return 0;
}
static void cancel_fake(fake_batch_t *b)
{
    ++b->cancels;
    if(!b->end_committed){b->cancelling=1;b->allow_complete=1;}
}

/* Actual wrapper calls these symbols. Keep signatures from its real headers. */
int tdc_ut_batch_cm3_begin(unsigned group,int ready)
{assert(ready==1);return begin_fake(&cm3,group,0,"CM3",'C');}
int tdc_ut_batch_cm3_begin_capture(unsigned group,unsigned step,int ready)
{assert(ready==1 && step);return begin_fake(&cm3,group,step,"CM3",'O');}
int tdc_ut_batch_cm3_poll(void){return poll_fake(&cm3);}
int tdc_ut_batch_cm3_cancel(void)
{cancel_fake(&cm3);return cm3.end_committed?TDC_UT_BATCH_CM3_BUSY:0;}
int tdc_ut_batch_cm3_active(void){return cm3.busy;}
int tdc_ut_cfx_bridge_begin(unsigned group,unsigned session,unsigned polls)
{
    assert(group==TDC_UT_BATCH_CFX || group==TDC_UT_BATCH_HEAR);
    assert(session>bridge.session && session<=TDC_UT_BATCH_MAX_ID && polls==TDC_UT_BATCH_MAX_ID);
    bridge.session=session;
    if(!bridge_restored)return -1;
    return begin_fake(&bridge,group,0,group==TDC_UT_BATCH_CFX?"CFX":"HEAR",group==TDC_UT_BATCH_CFX?'F':'H');
}
int tdc_ut_cfx_bridge_poll(void){return poll_fake(&bridge);}
void tdc_ut_cfx_bridge_abort(void){cancel_fake(&bridge);}
int tdc_ut_cfx_bridge_busy(void){return bridge.busy;}
int tdc_ut_cfx_bridge_restored(void){return bridge_restored;}
int tdc_ut_cfx_bridge_ram_safe(void)
{return !bridge.busy && (bridge.starts==0 || bridge_restored);}
unsigned tdc_ut_cfx_bridge_error(void)
{return bridge.terminal_error<0?(unsigned)-bridge.terminal_error:0;}
int tdc_ut_boot_batch_begin(unsigned group,int ready)
{assert(ready==1);return begin_fake(&boot,group,0,"BOOT",'B');}
int tdc_ut_boot_batch_poll(void){return poll_fake(&boot);}
void tdc_ut_boot_batch_cancel(void){cancel_fake(&boot);}
int tdc_ut_boot_batch_busy(void){return boot.busy;}

#if defined(CONSOLE_ADAPTER_BOOT)
int tdc_ut_boot_console_open(int ready);
int tdc_ut_boot_console_serve(int ready);
#define console_open tdc_ut_boot_console_open
#define console_serve tdc_ut_boot_console_serve
#else
int tdc_ut_cm3_console_open(int ready);
int tdc_ut_cm3_console_serve(int ready);
#define console_open tdc_ut_cm3_console_open
#define console_serve tdc_ut_cm3_console_serve
#endif

static void pump(unsigned count)
{for(unsigned i=0;i<count;++i)assert(tdc_ut_console_poll()>=0);}
static void feed(const char *text)
{
    assert(!*input);input=text;
    for(unsigned i=0;i<4000 && *input;++i)assert(tdc_ut_console_poll()==0);
    assert(!*input); /* Do not accidentally auto-complete the active batch. */
}
static void close_console(void)
{
    feed("ut quit\n");
    for(unsigned i=0;i<4000 && g_tdc_ut_console_opened;++i)assert(tdc_ut_console_poll()>=0);
    assert(!g_tdc_ut_console_opened && !cm3.busy && !bridge.busy && !boot.busy);
}
static void check_lines(void)
{
    for(const char *p=output;*p;)
    {
        const char *e=strstr(p,"\r\n");assert(e && e-p+2<=60);
        for(const char *c=p;c<e;++c)assert(*c>=32 && *c<=126);
        p=e+2;
    }
}
static void reset_output(void)
{
    assert(!cm3.busy && !bridge.busy && !boot.busy && !*input);
    output_n=event_n=0;output[0]=events[0]=0;
}
static void rejection_and_reopen(void)
{
    unsigned before=write_calls;
    assert(console_open(0)==-1 && console_open(2)==-1);
    g_console_adapter_ipsr=1;assert(console_open(1)==-1);g_console_adapter_ipsr=0;
#if !defined(CONSOLE_ADAPTER_BOOT)
    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration=1;
    assert(console_open(1)==-1);cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration=0;
    cm3.busy=1;assert(console_open(1)==-1);cm3.busy=0;
    bridge.busy=1;assert(console_open(1)==-1);bridge.busy=0;
#else
    boot.busy=1;assert(console_open(1)==-1);boot.busy=0;
#endif
    assert(write_calls==before && watchdog_calls==0);
    for(unsigned i=0;i<3;++i)
    {
        assert(console_open(1)==0 && console_open(1)==-1);
        close_console();assert(tdc_ut_console_poll()==1 && watchdog_calls==0);
    }
    check_lines();
}

#if !defined(CONSOLE_ADAPTER_BOOT)
static void auto_sequence(void)
{
    reset_output();assert(console_open(1)==0);pump(40);feed("ut auto 1\n");
    assert(!strcmp(events,"C") && cm3.group==1 && cm3.busy && !bridge.busy);
    pump(80);assert(strstr(output,"FAIL") && !bridge.busy);
    cm3.allow_complete=1;
    for(unsigned i=0;i<2000 && !bridge.busy;++i)assert(tdc_ut_console_poll()==0);
    assert(bridge.busy && !cm3.busy && !strcmp(events,"CcF"));
    assert(bridge.group==TDC_UT_BATCH_CFX && bridge.session>0);
    bridge.allow_complete=1;pump(200);
    assert(!bridge.busy && !strcmp(events,"CcFf"));
    const char *end=strstr(output,"CORE=CM3 END"),*start=strstr(output,"CORE=CFX BEGIN");
    assert(end && start && end<start && strstr(output,"CORE=CFX END"));
    close_console();check_lines();
}
static void stop_auto(int quit,int committed)
{
    reset_output();assert(console_open(1)==0);pump(40);feed("ut auto 1\n");
    unsigned started=bridge.starts,cancels=cm3.cancels;
    if(committed)
    {
        cm3.allow_complete=1;write_limit=1;intermittent=0;
        for(unsigned i=0;i<4000 && !cm3.end_committed;++i)assert(tdc_ut_console_poll()==0);
        assert(cm3.end_committed && cm3.busy);block_writes=1;
    }
    feed(quit?"ut quit\n":"ut stop\n");
    assert(cm3.cancels==cancels+1);
    if(committed){pump(30);assert(cm3.busy && g_tdc_ut_console_opened);block_writes=0;}
    pump(300);assert(!cm3.busy && bridge.starts==started);
    assert(strstr(output,"STOPPED_OR_ERROR"));
    if(quit)assert(!g_tdc_ut_console_opened);else{assert(g_tdc_ut_console_opened);close_console();}
    check_lines();write_limit=5;intermittent=1;
}
static void routing(void)
{
    reset_output();assert(console_open(1)==0);pump(40);
    unsigned c=cm3.starts,f=bridge.starts;
    feed("ut boot 1\nut auto 2\nut auto 1 1\nut cfx 2\nut hear 3\nut cm3 2 3\nut observe 1\nut observe 1 0\n");pump(80);
    assert(cm3.starts==c && bridge.starts==f);
    bridge_restored=0;feed("ut auto 1\n");pump(30);assert(cm3.starts==c);bridge_restored=1;
    feed("ut cm3 3\n");assert(cm3.group==3);cm3.allow_complete=1;pump(200);
    feed("ut observe 2 19\n");assert(cm3.group==2 && cm3.step==19);cm3.allow_complete=1;pump(200);
    assert(strstr(output,"OBSERVED"));
    unsigned session=bridge.session;
    feed("ut cfx 1\n");assert(bridge.group==TDC_UT_BATCH_CFX && bridge.session==session+1);
    bridge.allow_complete=1;pump(200);
    feed("ut hear 1\n");assert(bridge.group==TDC_UT_BATCH_HEAR && bridge.session==session+2);
    bridge.allow_complete=1;pump(200);close_console();
    assert(console_open(1)==0);pump(40);feed("ut cfx 1\n");
    assert(bridge.session==session+3);bridge.allow_complete=1;pump(200);close_console();check_lines();
}
static void execution_error(void)
{
    reset_output();assert(console_open(1)==0);pump(40);feed("ut auto 1\n");
    unsigned count=bridge.starts;cm3.terminal_error=-4;cm3.allow_complete=1;pump(200);
    assert(bridge.starts==count && strstr(output,"CORE=CM3 ERROR") && strstr(output,"STOPPED_OR_ERROR"));
    close_console();check_lines();
}
static int late_cleanup_guard(const char *scenario)
{
    /* Actual bridge semantics are separately reproduced by the companion
     * mailbox fixture. Here the wrapper must distinguish this terminal state
     * from a never-configured bridge, for which restored is also zero. */
    reset_output();
    if(!strcmp(scenario,"late-open"))
    {
        /* Console is closed, but a separately started bridge already reached
         * the same terminal cleanup timeout as the real mailbox fixture. */
        bridge.starts=1;bridge_restored=0;bridge.terminal_error=-UTB_TIMEOUT;
    }
    else
    {
        assert(console_open(1)==0);pump(40);feed("ut hear 1\n");assert(bridge.busy);
        bridge_restored=0;bridge.terminal_error=-UTB_TIMEOUT;bridge.allow_complete=1;pump(200);
    }
    assert(!bridge.busy && !tdc_ut_cfx_bridge_restored() && tdc_ut_cfx_bridge_error()==UTB_TIMEOUT);
    assert(!cm3.busy);
    unsigned before=cm3.starts;
    int accepted=0;
    if(!strcmp(scenario,"late-open"))
    {
        accepted=console_open(1)==0;
    }
    else
    {
        feed(!strcmp(scenario,"late-cm3")?"ut cm3 1\n":"ut observe 1 17\n");
        accepted=cm3.starts!=before;
    }
    if(accepted)
    {
        printf("CONSOLE_ADAPTER_GUARD FAIL: %s accepted with bridge busy=0 restored=0 error=%u; late HEAR cleanup pending\n",
               scenario,tdc_ut_cfx_bridge_error());
        return 1;
    }
    /* A late completion changes restored to 1 even though the previous error
     * code is retained. Recovery must not be permanently blocked by error!=0. */
    bridge_restored=1;
    if(!strcmp(scenario,"late-open"))assert(console_open(1)==0);
    feed(!strcmp(scenario,"late-observe")?"ut observe 1 18\n":"ut cm3 1\n");
    assert(cm3.starts==before+1);cm3.allow_complete=1;pump(200);close_console();
    printf("CONSOLE_ADAPTER_GUARD PASS: %s blocks unresolved cleanup and permits actual restoration\n",scenario);
    return 0;
}
static int late_cleanup_exit(int external)
{
    assert(console_open(1)==0);pump(40);feed("ut hear 1\n");
    bridge_restored=0;bridge.terminal_error=-UTB_TIMEOUT;bridge.allow_complete=1;pump(200);
    assert(!bridge.busy && g_tdc_ut_console_opened && !tdc_ut_cfx_bridge_ram_safe());
    if(external)g_tdc_ut_console_exit_requested=1;else feed("ut quit\n");
    pump(200);
    assert(g_tdc_ut_console_opened);
    const char *notice=strstr(output,"[UT-CONSOLE] WAITING_RESTORE\r\n");
    assert(notice && !strstr(notice+strlen("[UT-CONSOLE] WAITING_RESTORE\r\n"),"WAITING_RESTORE"));
    unsigned writes_before=write_calls,reads_before=read_calls;
    pump(200);assert(write_calls==writes_before && read_calls==reads_before);
    bridge_restored=1;pump(20);assert(!g_tdc_ut_console_opened && tdc_ut_console_poll()==1);
    assert(console_open(1)==0);pump(40);close_console();check_lines();
    printf("CONSOLE_ADAPTER_GUARD PASS: %s waits for actual late restoration, one notice, then closes/reopens\n",external?"external-exit":"quit");
    return 0;
}
static int unconfigured_cm3(void)
{
    bridge_restored=0;
    assert(!tdc_ut_cfx_bridge_busy() && tdc_ut_cfx_bridge_error()==0);
    if(console_open(1)!=0)
    {puts("CONSOLE_ADAPTER_GUARD FAIL: unconfigured bridge blocks standalone CM3 console");return 1;}
    pump(40);feed("ut cm3 1\n");
    if(cm3.starts!=1)
    {puts("CONSOLE_ADAPTER_GUARD FAIL: unconfigured bridge blocks standalone CM3 RAM run");return 1;}
    cm3.allow_complete=1;pump(200);feed("ut observe 1 1\n");
    if(cm3.starts!=2)
    {puts("CONSOLE_ADAPTER_GUARD FAIL: unconfigured bridge blocks standalone CM3 snapshot");return 1;}
    cm3.allow_complete=1;pump(200);close_console();
    puts("CONSOLE_ADAPTER_GUARD PASS: unconfigured CM3-only open/run/capture remain available");
    return 0;
}
#else
static void boot_routing(void)
{
    reset_output();assert(console_open(1)==0);pump(40);
    feed("ut auto 1\nut cm3 1\nut cfx 1\nut hear 1\nut observe 1 1\nut boot 1 2\n");pump(40);
    assert(boot.starts==0 && cm3.starts==0 && bridge.starts==0);
    for(unsigned group=1;group<=3;++group)
    {
        char command[24];snprintf(command,sizeof(command),"ut boot %u\n",group);feed(command);
        assert(boot.busy && boot.group==group);boot.allow_complete=1;pump(200);
    }
    assert(boot.starts==3 && !strcmp(events,"BbBbBb"));
    feed("ut boot 1\n");feed("ut stop\n");pump(200);
    assert(boot.cancels==1 && !boot.busy && strstr(output,"CORE=BOOT ABORT"));
    close_console();check_lines();
}
#endif

static void serve_watchdog(void)
{
    reset_output();unsigned count=watchdog_calls;
    assert(console_serve(0)==-1 && watchdog_calls==count);
#if defined(CONSOLE_ADAPTER_BOOT)
    input="ut boot 1\nut quit\n";
#else
    unsigned f=bridge.starts;
    input="ut auto 1\nut quit\n";
#endif
    assert(console_serve(1)==1 && !*input && !g_tdc_ut_console_opened);
    assert(watchdog_calls>count && !boot.busy && !cm3.busy && !bridge.busy);
#if !defined(CONSOLE_ADAPTER_BOOT)
    assert(bridge.starts==f);
#endif
    count=watchdog_calls;pump(10);assert(watchdog_calls==count);check_lines();
}
static int transport_fault(void)
{
    assert(console_open(1)==0);invalid_write_count=1;
    assert(tdc_ut_console_poll()==0 && g_tdc_ut_console_transport_fault && g_tdc_ut_console_opened);
    unsigned writes_before=write_calls,reads_before=read_calls;
    invalid_write_count=0;input="ut quit\n";g_tdc_ut_console_exit_requested=1;
    pump(200);
    assert(write_calls==writes_before && read_calls==reads_before && *input);
    assert(g_tdc_ut_console_opened && g_tdc_ut_console_transport_fault && console_open(1)==-1);
    puts("CONSOLE_ADAPTER_GUARD PASS: impossible RTT count latches fault, retains ownership, no further read/write/exit");
    return 0;
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"transport-fault"))return transport_fault();
#if !defined(CONSOLE_ADAPTER_BOOT)
    if(argc==2)
    {
        if(!strcmp(argv[1],"unconfigured"))return unconfigured_cm3();
        if(!strcmp(argv[1],"late-quit"))return late_cleanup_exit(0);
        if(!strcmp(argv[1],"late-external-exit"))return late_cleanup_exit(1);
        if(!strcmp(argv[1],"late-open") || !strcmp(argv[1],"late-cm3") || !strcmp(argv[1],"late-observe"))
            return late_cleanup_guard(argv[1]);
        return 2;
    }
#else
    (void)argc;(void)argv;
#endif
    rejection_and_reopen();
#if defined(CONSOLE_ADAPTER_BOOT)
    boot_routing();
#else
    auto_sequence();stop_auto(0,0);stop_auto(1,0);stop_auto(0,1);stop_auto(1,1);
    routing();execution_error();
#endif
    serve_watchdog();
#if defined(CONSOLE_ADAPTER_BOOT)
    puts("CONSOLE_ADAPTER_BOOT PASS: separate-image routing, guards, cancel/reopen, bounded serve watchdog");
#else
    puts("CONSOLE_ADAPTER_CM3 PASS: auto sequence/FAIL continuation, stop/quit including committed END, cfx/hear sessions, capture/guards/reopen, bounded serve watchdog");
#endif
    return 0;
}
