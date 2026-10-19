/* Exact live/mapping caller bodies; injected read/feedback boundaries exercise cleanup. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <tdc_isd.h>
#include <tdc_isd_fpga.h>
#define conneded_ISDCheck_byISDPower
#define BackelCircuitDisabled_readPcmFired_duringLiveStimulation 1
#define en__mappingMode 1
#define FPGA_pulsePhaseWidth_minimum 13
#define PcmBitStream_Mode_NopStandby 0
#define PcmBitStream_Mode_SepcificCommand 1
#define pcm_Mold_NopStandby 1
#define pcm_Mold_configuration 2
#define pcm_Mold_NopBacktel 3
#define df_MaxNumTransferableChannel 24
#define TDC_PRINTF_V(...) ((void)printf(__VA_ARGS__))
static struct {EN__ISD_CONTROL_STATE isd_controlState;} s_isd_state;
static struct {struct {int system_opMode;} systemShare;} cfx_cm3_sharedMemoryAll;
static int s_tdc_live_link_flow;
static bool s_tdc_live_link_busy;
static bool isINGconnectionCheck_WithMapping;
static int s_tdc_fault,s_tdc_fired,s_tdc_busy,s_tdc_power_reads,s_tdc_level_reads,s_tdc_feedback,s_tdc_clears,s_tdc_releases;
static int s_tdc_order[12],s_tdc_order_count;
void tdc_isd_change_state(EN__ISD_CONTROL_STATE s){s_isd_state.isd_controlState=s;}
void tdc_isd_set_i2c_busy(void){s_tdc_busy=1;}
void tdc_isd_set_i2c_free(void){s_tdc_busy=0;++s_tdc_releases;s_tdc_order[s_tdc_order_count++]=3;}
int tdc_shm_read_connection_check_pcm_state(void){return s_tdc_fired;}
void tdc_shm_clear_connection_check_pcm_fired_flag(void){s_tdc_fired=0;}
EN_ISD_PowerState tdc_isd_fpga_read_isd_power_state(void){++s_tdc_power_reads;if(s_tdc_fault==1){tdc_isd_change_state(en__isdStatus_PowerIC_OK);return ISD_Power_NA;}return ISD_Power_LowStable;}
bool tdc_isd_fpga_read_tx_power_level(int *out){++s_tdc_level_reads;if(s_tdc_fault==2){tdc_isd_change_state(en__isdStatus_PowerIC_Reset);return false;}*out=180;return true;}
bool tdc_link_control_feedback(int p,int level,bool mapping,bool dedicated){(void)p;(void)level;assert(mapping==dedicated);++s_tdc_feedback;s_tdc_order[s_tdc_order_count++]=1;tdc_isd_change_state(s_tdc_fault==3?en__isdStatus_PowerIC_Reset:en__isdStatus_FPGA_Ok);return false;}
bool tdc_isd_fpga_write_clear_fifo(void){++s_tdc_clears;s_tdc_order[s_tdc_order_count++]=2;return s_tdc_fault!=5;}
void tdc_shm_change_pcm_output_mode(int m){(void)m;}
void tdc_shm_change_next_pcm_output_mode(int m){(void)m;}
void tdc_isd_fpga_change_pulse_width_minimum(int i){assert(i<24);}
void tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(int v){assert(v==13);}
void tdc_isd_fpga_change_8_bit_backtel_mode(int i){assert(i<24);}
void tdc_shm_fill_specific_command_buffer(int i,int v){(void)v;assert(i>=0&&i<24);}
#include "integration_actual.inc"
int main(void){
    for(int mapping=0;mapping<=1;mapping++)for(int fault=1;fault<=5;fault++){
        s_tdc_fault=fault;s_tdc_fired=1;s_tdc_busy=0;s_tdc_live_link_flow=0;s_tdc_live_link_busy=false;
        s_tdc_power_reads=s_tdc_level_reads=s_tdc_feedback=s_tdc_clears=s_tdc_releases=s_tdc_order_count=0;
        s_isd_state.isd_controlState=en__isdStatus_stimul_10V_Ok;
        for(int tick=0;tick<8;tick++){
            if(mapping){tdc_isd_update_link_by_backtel_mapping(tick);if(!isINGconnectionCheck_WithMapping)break;}
            else {tdc_isd_update_link_by_backtel_live();if(!s_tdc_fired)break;}
        }
        assert(s_tdc_busy==0&&s_tdc_releases==1);
        assert(mapping?!isINGconnectionCheck_WithMapping:!s_tdc_live_link_busy&&!s_tdc_fired);
        if(fault==1){assert(s_tdc_feedback==0&&s_tdc_clears==0&&s_isd_state.isd_controlState==en__isdStatus_PowerIC_OK);if(!mapping)assert(s_tdc_level_reads==0);}
        if(fault==2){assert(s_tdc_feedback==0&&s_tdc_clears==0&&s_isd_state.isd_controlState==en__isdStatus_PowerIC_Reset);if(mapping)assert(s_tdc_power_reads==0);}
        if(fault==3){assert(s_tdc_feedback==1&&s_tdc_clears==0&&s_isd_state.isd_controlState==en__isdStatus_PowerIC_Reset);assert(s_tdc_order_count==2&&s_tdc_order[0]==1&&s_tdc_order[1]==3);}
        if(fault>=4){assert(s_tdc_feedback==1&&s_tdc_clears==1);assert(s_tdc_order_count==3&&s_tdc_order[0]==1&&s_tdc_order[1]==2&&s_tdc_order[2]==3);assert(s_isd_state.isd_controlState==(fault==4?en__isdStatus_FPGA_Ok:en__isdStatus_PowerIC_OK));}
    }
    puts("INTEGRATION_HOST_PASS live/mapping x read/PMIC/disconnect/clear faults; cleanup ordering");return 0;
}
