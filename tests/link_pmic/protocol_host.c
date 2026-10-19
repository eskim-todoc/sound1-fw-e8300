/* Real config, policy and protocol modules. Faults are RAM FatFs boundaries. */
#define main config_fixture_main
#include "config_host.c"
#undef main
#include <tdc_link_control.h>
#include <tdc_isd.h>
#include <tdc_isd_fpga.h>
static int s_tdc_hw_writes;
static tdc_test_map_t s_tdc_map={268,2};
static tdc_test_para_t s_tdc_para={6};
tdc_test_map_t *getPointerCurrentMapData(void){return &s_tdc_map;}
tdc_test_para_t *getPointerCalculatedStimulPara_byCM3(void){return &s_tdc_para;}
bool write_change_TxPowerLevel(int level){++s_tdc_hw_writes;tdc_link_control_note_level(level);return true;}
void change_isd_state(EN__ISD_CONTROL_STATE state){(void)state;}
void errorCodeUpdate(int major,int minor,int line){(void)major;(void)minor;(void)line;}
#include "../../src/2__cm3/Cortex-M3-src/internalDevice/tdc_link_control.c"
#include "../../src/2__cm3/Cortex-M3-src/BleCommunication/tdc_ble_link_debug.c"
typedef struct { int command; int data[19]; } ST__REMOTECONTROL_PACKET;
static int s_tdc_other_routes;
static int gd_handle_touch_debug(const ST__REMOTECONTROL_PACKET *p,int *tx,int n,int o){(void)p;(void)tx;(void)o;++s_tdc_other_routes;return n;}
static int gd_handle_no_backtel(const ST__REMOTECONTROL_PACKET *p,int *tx,int n,int o){(void)p;(void)tx;(void)o;++s_tdc_other_routes;return n;}
static int gd_handle_map_init(const ST__REMOTECONTROL_PACKET *p,int *tx,int n,int o){(void)p;(void)tx;(void)o;++s_tdc_other_routes;return n;}
static int gd_handle_op_mode_setting(const ST__REMOTECONTROL_PACKET *p,int *tx,int n,int o){(void)p;(void)tx;(void)o;++s_tdc_other_routes;return n;}
int tdc_ble_reply_header(uint8_t *tx,int n,int command){tx[n]=(uint8_t)command;return n+1;}
#include "general_debug_route.inc"
static unsigned char s_tdc_tx[22];
static int call(int option,int access,int index,int value){
    int data[19]={option,access,index,value};memset(s_tdc_tx,0xA5,sizeof(s_tdc_tx));
    int n=tdc_ble_link_debug_handle(0x8F,data,s_tdc_tx+1);
    assert(n>=6 && n<=20 && s_tdc_tx[0]==0xA5 && s_tdc_tx[21]==0xA5);
    return n;
}
static void reject_case(int option,int access,int index,int value){
    tdc_link_config_t before=*tdc_link_config_get();int writes=s_tdc_writes;
    assert(call(option,access,index,value)==6 && s_tdc_tx[1]==0xF0 && s_tdc_tx[2]==0x8F);
    assert(!memcmp(&before,tdc_link_config_get(),sizeof(before)) && writes==s_tdc_writes && s_tdc_hw_writes==0);
}
int main(void){
    assert(tdc_link_config_init()==0);tdc_link_control_map_changed(0);
    assert(call(0x20,0,0,0)==20);const unsigned char expected[]={0x8F,0x20,0,0,15,0,3,140,172,4,0,1,0,0,0,3,12,6,0,2};
    assert(!memcmp(s_tdc_tx+1,expected,sizeof(expected)));
    assert(call(0x20,0,12,0)==7 && s_tdc_tx[6]==1 && s_tdc_tx[7]==12);
    assert(call(0x20,0,16,0)==6 && s_tdc_tx[6]==214);
    assert(call(0x20,0,17,0)==6 && s_tdc_tx[6]==0);
    assert(call(0x20,1,17,0)==6 && s_tdc_tx[1]==0x8F);
    reject_case(0x20,1,17,1);reject_case(0x20,1,17,3);
    reject_case(0x20,0,18,0);reject_case(0x20,0,119,0);reject_case(0x20,0,256,0);
    reject_case(0x20,2,3,140);reject_case(0x20,-1,3,140);reject_case(0x21,0,0,0);
    const int readonly[]={0,1,12,13,15};for(unsigned i=0;i<sizeof(readonly)/sizeof(readonly[0]);i++)reject_case(0x20,1,readonly[i],1);
    reject_case(0x20,1,3,111);reject_case(0x20,1,2,0);reject_case(0x20,1,2,256);reject_case(0x20,1,6,111);
    assert(call(0x20,1,6,180)==6 && tdc_link_control_get_force()==180 && s_tdc_hw_writes==0);
    assert(call(0x20,1,6,0)==6 && tdc_link_control_get_force()==0);
    assert(call(0x20,1,7,255)==6 && s_tdc_tx[6]==255 && tdc_link_control_get_infinite()==1);
    s_tdc_fault=1;tdc_link_config_t before=*tdc_link_config_get();
    assert(call(0x20,1,7,3)==6 && s_tdc_tx[1]==0xF0 && s_tdc_tx[3]==10 && tdc_link_control_get_infinite()==1);
    assert(!memcmp(&before,tdc_link_config_get(),sizeof(before)));
    s_tdc_fault=0;assert(call(0x20,1,7,3)==6 && tdc_link_control_get_infinite()==0 && tdc_link_config_get()->coin_count==3);
    assert(call(0xFF,0,0,0)==15);const unsigned char version[]={0x8F,0xFF,0,0,10,1,0,0x1E,0,0,0,1,0,0,0};assert(!memcmp(s_tdc_tx+1,version,sizeof(version)));
    reject_case(0xFF,1,0,0);reject_case(0xFF,0,1,0);
    ST__REMOTECONTROL_PACKET packet={0x8F,{0x20,0,12}};
    int routed[22];for(unsigned i=0;i<22;i++)routed[i]=0xA5;
    assert(tdc_remote_general_debug_handle(&packet,routed+1,0)==7);
    assert(routed[1]==0x8F && routed[6]==1 && routed[7]==12 && routed[0]==0xA5 && routed[21]==0xA5);
    packet.data[0]=0xFF;packet.data[2]=0;
    assert(tdc_remote_general_debug_handle(&packet,routed+1,0)==15 && routed[2]==0xFF);
    packet.data[0]=0x20;packet.data[1]=2;
    assert(tdc_remote_general_debug_handle(&packet,routed+1,0)==6 && routed[1]==0xF0);
    assert(s_tdc_other_routes==0);
    assert(s_tdc_hw_writes==0);puts("PROTOCOL_HOST_PASS bytes/ranges/FFoff/force_release/failed_save/capabilities");return 0;
}
