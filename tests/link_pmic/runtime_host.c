/* Real policy and real PMIC read/write bodies; only register I/O is modeled. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tdc_link_config.h>
#include <tdc_link_control.h>
#include <tdc_shm.h>
#include <tdc_isd.h>
#include <tdc_isd_fpga.h>
#include <driver_REN_ISL9122.h>
#include <tdc_sys_error.h>
tdc_test_shm_t cfx_cm3_sharedMemoryAll;
static tdc_link_config_t s_tdc_config={140,172,3,4,1,0,0,214};
static int s_tdc_writes,s_tdc_requested,s_tdc_bus_fail,s_tdc_read_value,s_tdc_error,s_tdc_state;
static int s_tdc_fifo_count,s_tdc_fifo_fail,s_tdc_fifo_reads,s_tdc_fifo_value;
bool read_FPGA_FIFO_counter(int *value){if(s_tdc_fifo_fail==1)return false;*value=s_tdc_fifo_count;return true;}
bool read_FPGA_backtel_FIFO(int *value,int count){assert(count==1);++s_tdc_fifo_reads;if(s_tdc_fifo_fail==2)return false;*value=s_tdc_fifo_value;return true;}
int data_ExtractionAndRigthShift(int value,int position,int length){return (value>>position)&((1<<length)-1);}
const tdc_link_config_t *tdc_link_config_get(void){return &s_tdc_config;}
void change_isd_state(EN__ISD_CONTROL_STATE state){s_tdc_state=state;if(state==en__isdStatus_PowerIC_Reset)tdc_link_control_invalidate_level();}
void errorCodeUpdate(int major,int minor,int line){(void)major;(void)line;s_tdc_error=minor;}
bool write_REN_ISL9122_register_byCM3_I2C(int reg,int value){assert(reg==0x11);++s_tdc_writes;s_tdc_requested=value;return !s_tdc_bus_fail;}
bool read_REN_ISL9122_register_byCM3_I2C(int reg,int *value){assert(reg==0x11);if(s_tdc_bus_fail)return false;*value=s_tdc_read_value;return true;}
#define TDC_ISD_DEBOUNCE_FAIL(count,a,b,c,major,minor) do { ++(count);(void)(count);errorCodeUpdate((major),(minor),__LINE__); } while(0)
#include "pmic_actual.inc"
#include "../../src/2__cm3/Cortex-M3-src/internalDevice/tdc_link_control.c"
static void reset(void){s_tdc_bus_fail=0;s_tdc_error=0;s_tdc_state=0;s_tdc_writes=0;tdc_link_control_set_force(0);tdc_link_control_set_infinite(0);tdc_link_control_reset_feedback();}
int main(void){
    int n,value=999;
    /* A failing boundary leaves its out parameter untouched on purpose. */
    s_tdc_fifo_fail=1;assert(read_isd_Power_State()==ISD_Power_NA && s_tdc_fifo_reads==0);
    s_tdc_fifo_count=1;s_tdc_fifo_fail=2;assert(read_isd_Power_State()==ISD_Power_NA && s_tdc_fifo_reads==1);
    s_tdc_fifo_fail=0;s_tdc_fifo_count=0;assert(read_isd_Power_State()==NoBacktel);
    s_tdc_fifo_count=2;assert(read_isd_Power_State()==BackTelNumTooMuch);
    s_tdc_fifo_count=1;for(int state=0;state<=3;state++){s_tdc_fifo_value=state;assert((int)read_isd_Power_State()==state);}
    /* Reset75 remains legal; operating force has floor112. Failed I/O never publishes. */
    assert(write_change_TxPowerLevel(75));assert(tdc_link_control_get_level()==75);
    n=s_tdc_writes;assert(!write_change_TxPowerLevel(74));assert(!write_change_TxPowerLevel(215));assert(n==s_tdc_writes);
    s_tdc_bus_fail=1;assert(!write_change_TxPowerLevel(180));assert(s_tdc_actual_level==75 && tdc_link_control_get_level()==0 && s_tdc_state==en__isdStatus_PowerIC_Reset);
    assert(!read_txPowerLevel(&value));assert(value==999 && s_tdc_actual_level==75 && tdc_link_control_get_level()==0);
    s_tdc_bus_fail=0;s_tdc_read_value=150;assert(read_txPowerLevel(&value));assert(value==150 && tdc_link_control_get_level()==150);
    /* Both low/high stable variants, floor and upper clamp. */
    reset();assert(tdc_link_control_feedback(ISD_Power_LowStable,212,0,0));assert(s_tdc_requested==214);
    assert(tdc_link_control_feedback(ISD_Power_LowUnstable,150,0,0));assert(s_tdc_requested==154);
    assert(tdc_link_control_feedback(ISD_Power_HighStable,150,0,0));assert(s_tdc_requested==149);
    n=s_tdc_writes;assert(tdc_link_control_feedback(ISD_Power_HighUnstable,140,0,0));assert(n==s_tdc_writes);
    /* Mapping gets172; returning normal declines one code, not immediately140. */
    assert(tdc_link_control_feedback(ISD_Power_HighStable,140,1,0));assert(tdc_link_control_get_level()==172);
    assert(tdc_link_control_feedback(ISD_Power_HighStable,172,0,0));assert(s_tdc_requested==171);
    /* One normal/Live retry coin, recharge on valid backtel. Dedicated has none. */
    reset();assert(tdc_link_control_feedback(ISD_Power_HighStable,180,0,0));
    assert(tdc_link_control_feedback(NoBacktel,180,0,0));assert(s_tdc_requested==184);
    assert(!tdc_link_control_feedback(NoBacktel,184,0,0));assert(s_tdc_state==en__isdStatus_FPGA_Ok);
    assert(tdc_link_control_feedback(ISD_Power_LowStable,180,1,0));assert(tdc_link_control_feedback(NoBacktel,184,1,0));
    assert(!tdc_link_control_feedback(NoBacktel,188,1,0));
    reset();assert(!tdc_link_control_feedback(NoBacktel,180,1,1));assert(s_tdc_writes==0);
    reset();assert(!tdc_link_control_feedback(BackTelNumTooMuch,180,1,1));assert(s_tdc_writes==0);
    /* Failed PMIC requests retain cache and PowerIC_Reset recovery. */
    reset();tdc_link_control_note_level(180);s_tdc_bus_fail=1;
    assert(!tdc_link_control_feedback(ISD_Power_LowStable,180,0,0));assert(s_tdc_actual_level==180 && tdc_link_control_get_level()==0 && s_tdc_state==en__isdStatus_PowerIC_Reset);
    /* Mode1 acceleration independently expected +4,+4,+8,+8,+12,+12,+16. */
    reset();s_tdc_config.mode=1;s_tdc_config.accel=1;
    const int expected[]={144,144,148,148,152,152,156};
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++){assert(tdc_link_control_feedback(NoBacktel,140,0,0));assert(s_tdc_requested==expected[i]);}
    n=s_tdc_writes;assert(tdc_link_control_feedback(BackTelNumTooMuch,140,0,0));assert(n==s_tdc_writes);
    assert(tdc_link_control_feedback(NoBacktel,140,0,0));assert(s_tdc_requested==144);
    assert(tdc_link_control_feedback(NoBacktel,213,0,0));assert(s_tdc_requested==214);
    assert(!tdc_link_control_feedback(NoBacktel,214,0,0));
    /* Force validation+release, infinite no automatic write, NOP map reset. */
    reset();assert(!tdc_link_control_set_force(111));assert(!tdc_link_control_set_force(215));assert(tdc_link_control_set_force(180));
    assert(tdc_link_control_feedback(ISD_Power_LowStable,140,0,0));assert(s_tdc_requested==180);
    assert(tdc_link_control_set_force(0));assert(tdc_link_control_feedback(ISD_Power_LowStable,180,0,0));assert(s_tdc_requested==179);
    reset();tdc_link_control_set_infinite(1);n=s_tdc_writes;assert(tdc_link_control_feedback(NoBacktel,214,0,0));assert(n==s_tdc_writes);
    assert(tdc_link_control_set_nop(19));assert(!tdc_link_control_set_nop(20));assert(tdc_link_control_get_nop()==19);
    tdc_link_control_map_changed(6);assert(tdc_link_control_get_nop()==6 && tdc_link_control_get_override()==0);
    s_tdc_config.max_level=200;assert(tdc_link_control_effective_max()==214);tdc_link_control_connected();assert(tdc_link_control_effective_max()==200);
    assert(tdc_link_control_period_ms()==300);
    puts("RUNTIME_HOST_PASS PMIC/cache/floors/coins/mapping/mode1/force/infinite/NOP");return 0;
}
