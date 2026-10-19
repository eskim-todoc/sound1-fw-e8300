/* PC 전용: 실제 상태 함수와 RAM 공유 함수를 링크한다.
 * 장치/파일 경계만 고정 응답이며 각 시나리오는 별도 프로세스에서 시작한다.
 * 지역 static을 강제로 바꾸거나 제품 소스/매크로를 재작성하지 않는다. */
#if !defined(TDC_UNIT_TEST) || !defined(TDC_UT_CM3_HOST)
#error "PC fixture only"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <cfx_cm3_sharedMemory.h>
#include <isd_interface.h>
#include <isd_interface_FPGA.h>
#include <isd_interface_init_FPGA.h>
#include <isd_interface_stimulationParaSetting.h>
#include <isd_interface_stimulationStandAlone.h>
#include <stimulationParaCal.h>
#include <indicatorByStimul.h>
#include <internalStimulationChip.h>
#include <driver_REN_ISL9122.h>
#include <dirver_PCM.h>
#include <error.h>
#include <tdc_gain_storage.h>
#include <tdc_ut_vectors_cm3.h>
#include <tdc_ut_coordination_cm3.h>
#include <electrodeMapping.h>
#include <dirver_i2c_for_ISD.h>

static unsigned s_tdc_checks, s_tdc_failed, s_tdc_known;
static unsigned s_tdc_case;
static int s_tdc_tick, s_tdc_final_tick, s_tdc_mode;
static int s_tdc_power = 190, s_tdc_power_writes, s_tdc_power_reads;
static EN_ISD_PowerState s_tdc_response = ISD_Power_HighStable;
static int s_tdc_response_reads, s_tdc_clear_calls;
static bool s_tdc_clear_ok = true, s_tdc_reset_ok = true, s_tdc_fpga_ok = true;
static bool s_tdc_final_fail, s_tdc_backtel_read_fail, s_tdc_backtel_mismatch;
static int s_tdc_width, s_tdc_backtel_config = 23;
static int s_tdc_file_calls, s_tdc_old_isd, s_tdc_copy_calls;
static int s_tdc_copy_attempts, s_tdc_skipped;
/* PCB 전극 계약에서 자극 1..32 / 기준 2..32,1을 칩 순서로 펼친 입력.
 * 실행 중 제품 electrodeMap이나 생성 패킷에서 기대값을 복사하지 않는다. */
static const int s_tdc_reference_electrode[32] = {
    2,0,4,1,6,3,8,5,10,7,13,9,11,15,29,17,
    31,18,20,16,22,19,24,21,26,23,28,25,30,27,14,12
};

static void check(unsigned unit, const char *field, int actual, int expected, bool known)
{
    ++s_tdc_checks;
    printf("RECORD {\"unit\":%u,\"case\":%u,\"field\":\"%s\",\"actual\":[%d],\"expected\":[%d],\"known_failure\":%s}\n",
           unit, ++s_tdc_case, field, actual, expected, known ? "true" : "false");
    if (actual != expected)
    {
        ++s_tdc_failed;
        if (known && actual==1 && expected==0) ++s_tdc_known;
    }
    else if (known)
    {
        fprintf(stderr, "Known finding changed: %s\n", field);
        ++s_tdc_failed;
    }
}

static void observe(unsigned unit, const char *field, int actual)
{
    printf("RECORD {\"unit\":%u,\"case\":%u,\"field\":\"%s\",\"actual\":[%d],\"expected\":null}\n",
           unit, ++s_tdc_case, field, actual);
}

/* RTT와 시각은 출력 경계다. 실제 레지스터/파일/전원에는 접근하지 않는다. */
int SEGGER_RTT_printf(unsigned buffer, const char *format, ...) {(void)buffer; (void)format; return 0;}
int ci_timer_get_tick(void) {return s_tdc_tick;}
void fn_write_userSettingParameters(int isd) {(void)isd; abort();}
int ci_fft_read_pass_bin(int bands) {(void)bands; return 0;}
int ci_fft_init_pass_bin(int bands) {(void)bands; abort();}

bool Reset_REN_ISL9122(void) {return s_tdc_reset_ok;}
bool write_change_TxPowerLevel(int value)
{
    assert(value >= ResetVoltageSetValue && value <= MaxVoltageControlValue);
    s_tdc_power = value; ++s_tdc_power_writes; return true;
}
bool read_txPowerLevel(int *value) {*value = s_tdc_power; ++s_tdc_power_reads; return true;}
EN_ISD_PowerState read_isd_Power_State(void) {++s_tdc_response_reads; return s_tdc_response;}
bool write_FPGA_clear_FIFO(void)
{
    ++s_tdc_clear_calls;
    return s_tdc_clear_ok && !(s_tdc_final_fail && s_tdc_tick == s_tdc_final_tick);
}
void reset_Fpga_variable(void) {}
bool write_FPGA_reset(void) {return true;}
bool read_FPGA_version(int *value) {*value = 0x28; return true;}
bool read_FPGA_systemResgister_1st(int *value)
{
    *value = s_tdc_tick == 11 ? FPGA_Status__Reset_value :
             s_tdc_fpga_ok ? FPGA_Status__OK_DisabledRF_value : FPGA_Status__Reset_value;
    return true;
}
bool check_FPGA_PCM_Error(bool *value) {*value = false; return true;}
bool check_FPGA_FIFO_empty(bool *value) {*value = true; return true;}
bool read_FPGA_systemError_Flag(int *value) {*value = 0; return true;}
bool read_FPGA_PulseWidth(int *value) {*value = s_tdc_width; return true;}
bool read_FPGA_backtelConfig(int *value) {*value = s_tdc_backtel_config; return true;}
void upadte_fpga_pulsePhaseWidth_written_Value(int value) {(void)value;}
void upadte_fpga_backtelConfig_written_Value(int value) {(void)value;}
void chang_PulseWidth(int index, int width) {(void)index; s_tdc_width = width;}
void chang_PulseWidth_minimum(int index) {chang_PulseWidth(index, FPGA_pulsePhaseWidth_minimum);}
int change_8BitBacktel_mode(int index) {(void)index; return s_tdc_backtel_config;}
int disable_Backtel(int index) {(void)index; return 0;}
bool read_FPGA_FIFO_counter(int *value)
{
    *value = s_tdc_mode == en__bipolar && s_tdc_tick == 29 ? 32 : 2; return true;
}
/* readback는 사전 fixture다. 생성한 명령을 다시 읽어 기대값으로 쓰지 않는다. */
bool read_ISD_by_CM3_I2C(int address, int *values, int count)
{
    assert(address==i2cAddr_FPGA_Backtel_FIFO);
    if (s_tdc_backtel_read_fail && s_tdc_tick == 9) return false;
    if (count == 32)
    {
        for (int i=0;i<32;++i) values[i] = s_tdc_reference_electrode[i];
    }
    else
    {
        assert(count == 2);
        values[0] = 44;
        /* 공유 헤더의 기준 전극 코드: body=1, referenceNA=0.
         * bit7=1, A/A=0, MP_CONFIG=1/0, STIM_MODE=0/1. */
        values[1] = s_tdc_mode == en__bipolar ? 0x81 : 0x84;
        if (s_tdc_backtel_mismatch && s_tdc_tick==9) values[0]=45;
    }
    return true;
}

static void file_read(int isd)
{
    assert(isd == 1 || isd == 2);
    assert(read_connected_ISD_Num() == s_tdc_old_isd); /* 모든 입력 준비 후 게시 */
    ++s_tdc_file_calls;
}
int ci_map_read_isd_info(int isd)
{
    file_read(isd);
    ST__CFX_CM3_SharedMemory_ISD_info *info = &cfx_cm3_sharedMemoryAll.cfx_ISD_info[isd-1];
    info->isd_year=19+isd; info->isd_month_model=isd; info->isd_serial=100+isd;
    info->isd_location_RL=isd;
    for (int i=0;i<25;++i) info->isd_userName[i]=100*isd+i;
    for (int i=0;i<4;++i) info->remocon_passkey[i]=1000*isd+i;
    return 0;
}
int ci_map_read_user_setting_value(int isd) {file_read(isd); return 0;}
int ci_map_read_map_stamp(int isd) {file_read(isd); return 0;}
int ci_map_read_map_data(int isd, int map) {file_read(isd); assert(map>=1 && map<=4); return 0;}
void fn_copy_MapInfo_toCM3(int isd)
{
    file_read(isd);
    memset(&cfx_cm3_sharedMemoryAll.connected_ISD_Map_info,0,sizeof(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info));
    cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.user_usableMapNum=isd==1 ? 2 : 1;
    cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.mapStamp.mapping_year=27-isd;
    for (int i=0;i<4;++i)
    {
        cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex[i]=isd==1 ? i<2 : i==2;
        cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[i].mapping_day=10*isd+i+1;
    }
}
void fn_copy_userSettingParameters_toCM3(int isd)
{
    file_read(isd);
    cfx_cm3_sharedMemoryAll.userSettingValue.mapNum=isd==1 ? 1 : 3;
}
int tdc_gain_storage_load(int isd, ST__TDC_GAIN_SETTING *out)
{
    file_read(isd); memset(out,0,sizeof(*out));
    out->gain_table_index_a=isd; out->gain_table_index_b=isd+2; return 0;
}
void fn_copy_MappingData_toCM3(int map, int isd)
{
    assert((isd==1 && (map==1 || map==2)) || (isd==2 && map==3));
    ++s_tdc_copy_calls;
}

static void prepare(void)
{
    memset(&cfx_cm3_sharedMemoryAll,0,sizeof(cfx_cm3_sharedMemoryAll));
    cfx_cm3_sharedMemoryAll.systemShare.system_opMode=en__mappingMode;
    tdc_ut_cm3_isd_state_t state={0};
    state.state.isd_controlState=en__isdStatus_stimul_10V_Ok;
    state.state.conneded_ISD=true; state.i2c_free=true;
    tdc_ut_cm3_isd_state_exchange(&state);
    g_tdc_ut_cm3_suppress_diagnostics=1;
    g_tdc_ut_cm3_suppress_uart_flag=1;
    clearAllErrorFlag();
    ST__CFX_CM3_SharedMemory_mapData *map=getPointerCurrentMapData();
    map->numFrequencyBand=32; map->stimulationPulsePhaseWidth=25;
    map->stimulationStrategy=df_stimulationStrategy_CIS;
    map->stimulationMode=en__monopolr_body;
    map->stimulationIndicatorChannelNum=1; map->stimulationIndicatorAmplitude_uA=300;
    for (int i=0;i<32;++i)
    {
        map->T_level_uA[i]=100; map->C_level_uA[i]=300;
        map->usableStimulationElectrodIndex[i]=i+1;
        map->usableReferenceElectrodIndex[i]=(i+1)%32+1;
    }
    *readStimulDAC_RegisterValue()=(ST_STIUL_DAC_REGISTER_VALUE){0,0,44};
}

static void selection(void)
{
    for (int step=0;step<3;++step)
    {
        int isd=step==1 ? 2 : 1;
        s_tdc_old_isd=read_connected_ISD_Num(); s_tdc_file_calls=0;
        changeConnected_isd_num_CFX(isd);
        check(21,"selected",read_connected_ISD_Num(),isd,false);
        check(21,"id",read_ISD_manufacture_ID(isd-1),isd==1 ? 0x14010065 : 0x15020066,false);
        check(21,"location",readConnected_ISD_Location(isd),isd,false);
        for (int i=0;i<25;++i) check(21,"synthetic_name",readConnected_ISD_userName(isd)[i],100*isd+i,false);
        for (int i=0;i<4;++i) check(21,"synthetic_passkey",read_recomcon_passkey_connected_ISD(isd)[i],1000*isd+i,false);
        check(21,"gain_a",cfx_cm3_sharedMemoryAll.gain_table_index_a,isd,false);
        check(21,"gain_b",cfx_cm3_sharedMemoryAll.gain_table_index_b,isd+2,false);
        check(21,"input_boundaries",s_tdc_file_calls,10,false);
        check(22,"count",readConnected_ISD_usableMapNum(),isd==1 ? 2 : 1,false);
        for (int i=0;i<4;++i) check(22,"usable",readConnected_ISD_usableMapIndex()[i],isd==1 ? i<2 : i==2,false);
        check(22,"stamp",readConnected_ISD_MapStamp()[0],27-isd,false);
        for (int i=1;i<=4;++i) check(22,"date",readConnected_ISD_MapDate(i)[2],10*isd+i,false);
        int before[4]; memcpy(before,readConnected_ISD_usableMapIndex(),sizeof(before));
        changeProgramMapNum(isd==1 ? 2 : 3);
        check(22,"program",readProgramMapNum(),isd==1 ? 2 : 3,false);
        check(22,"list_unchanged",memcmp(before,readConnected_ISD_usableMapIndex(),sizeof(before))==0,1,false);
    }
}

static void power_init(bool ok)
{
    s_tdc_reset_ok=ok; change_isd_state(en__isdStatus_PowerIC_Reset);
    for (s_tdc_tick=0;s_tdc_tick<200;++s_tdc_tick)
    {
        init_txPowerIC(s_tdc_tick==0);
        if (s_tdc_tick==0 || s_tdc_tick==3)
            check(31,"power_request",cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX,s_tdc_tick==0 ? 2 : 1,false);
        if (!ok && s_tdc_tick==10)
        {
            check(31,"reset_failure_not_complete",snd_isd_interface_get_state().isd_controlState,en__isdStatus_PowerIC_Reset,false);
            return; /* 이후 하위 드라이버 복구까지 모델링하지 않는다. */
        }
        if (snd_isd_interface_get_state().isd_controlState==en__isdStatus_PowerIC_OK) break;
        check(31,"incomplete",snd_isd_interface_get_state().isd_controlState,en__isdStatus_PowerIC_Reset,false);
    }
    check(31,"power_ready",snd_isd_interface_get_state().isd_controlState,en__isdStatus_PowerIC_OK,false);
    check(31,"power_upper_bound",s_tdc_power,MaxVoltageControlValue,false);
    check(31,"ramp_executed",s_tdc_power_writes>1,1,false);
}

static void fpga_init(bool ok)
{
    s_tdc_fpga_ok=ok; change_isd_state(en__isdStatus_PowerIC_OK);
    for (s_tdc_tick=0;s_tdc_tick<=20;++s_tdc_tick)
    {
        init_FPGA(s_tdc_tick==0);
        if (s_tdc_tick<20) check(31,"fpga_not_ready",snd_isd_interface_get_state().isd_controlState,en__isdStatus_PowerIC_OK,false);
    }
    check(31,"fpga_result",snd_isd_interface_get_state().isd_controlState,ok ? en__isdStatus_FPGA_Ok : en__isdStatus_PowerIC_OK,false);
}

static void link_check(bool mapping, const char *name)
{
    unsigned unit=mapping ? 33 : 32;
    if (strstr(name,"none")) s_tdc_response=NoBacktel;
    if (strstr(name,"many")) s_tdc_response=BackTelNumTooMuch;
    if (strstr(name,"low")) s_tdc_response=ISD_Power_LowStable;
    s_tdc_clear_ok=strstr(name,"clear-fail")==NULL;
    /* 송신 완료 전에는 읽지 않는다. 실제 공유 완료 플래그를 입력한다. */
    if (!mapping)
    {
        clearConnectionCheckPcmFiredFlag();
        update_isd_LinkConnection_byBacktel_withLiveStimulation();
        check(unit,"no_completion_no_read",s_tdc_response_reads,0,false);
        cfx_cm3_sharedMemoryAll.cfx_PCM_interface.conneded_ISDCheckPCM_state=BackelCircuitDisabled_readPcmFired_duringLiveStimulation;
    }
    for (int tick=0;tick<(mapping ? 7 : 3);++tick)
    {
        if (mapping) update_isd_LinkConnection_byBacktel_withMapping(tick);
        else update_isd_LinkConnection_byBacktel_withLiveStimulation();
        if (mapping) check(unit,"progress",isING_connectionCheckWithMapping(),tick<6,false);
    }
    check(unit,"received_once",s_tdc_response_reads,1,false);
    check(unit,"fifo_clear",s_tdc_clear_calls,1,false);
    check(unit,"i2c_released",is_i2c_free(),1,false);
    int expected_state=!s_tdc_clear_ok ? en__isdStatus_PowerIC_OK :
        (s_tdc_response==NoBacktel || s_tdc_response==BackTelNumTooMuch) ? en__isdStatus_FPGA_Ok : en__isdStatus_stimul_10V_Ok;
    check(unit,"recovery_state",snd_isd_interface_get_state().isd_controlState,expected_state,false);
    check(unit,"error",readErrorCode().ISD_ErrorFlag,s_tdc_response==NoBacktel ? en__BackTelCounterZero :
          s_tdc_response==BackTelNumTooMuch ? en__BackterDataLengthError : 0,false);
    if (!mapping) check(unit,"completion_cleared",readConnectionCheckPcmState(),BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation,false);
    if (s_tdc_response==ISD_Power_HighStable || s_tdc_response==ISD_Power_LowStable)
        check(unit,"power_adjustment",s_tdc_power,s_tdc_response==ISD_Power_HighStable ? 189 : 191,false);
    if (mapping)
    {
        update_isd_LinkConnection_byBacktel_withMapping(7);
        check(unit,"no_stale_progress",isING_connectionCheckWithMapping(),0,false);
        update_isd_LinkConnection_byBacktel_withMapping(0);
        check(unit,"restart_progress",isING_connectionCheckWithMapping(),1,false);
    }
}

static void setting(const char *name)
{
    s_tdc_mode=strcmp(name,"bipolar")==0 ? en__bipolar : en__monopolr_body;
    getPointerCurrentMapData()->stimulationMode=s_tdc_mode;
    s_tdc_final_tick=s_tdc_mode==en__bipolar ? 39 : 14;
    s_tdc_final_fail=strstr(name,"final-fail")!=NULL;
    s_tdc_backtel_read_fail=strstr(name,"read-fail")!=NULL;
    s_tdc_backtel_mismatch=strstr(name,"read-mismatch")!=NULL;
    clear_sitmulationParaSettingDone();
    for (s_tdc_tick=0;s_tdc_tick<=s_tdc_final_tick;++s_tdc_tick)
    {
        bool error=stimulationSetting(s_tdc_tick==0);
        check(41,"step_error",error,s_tdc_backtel_mismatch && s_tdc_tick==9,false);
        check(41,"completion",isStimulParaIsSettingDone(),
              s_tdc_tick==s_tdc_final_tick && !s_tdc_final_fail && !s_tdc_backtel_read_fail,
              false);
        if (s_tdc_tick==(s_tdc_mode==en__bipolar ? 30 : 5))
        {
            int offset=s_tdc_mode==en__bipolar ? 2 : 0;
            check(41,"dac_write",cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[offset],0x50b2c,false);
            check(41,"mode_write",cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[offset+1],s_tdc_mode==en__bipolar ? 0x50d81 : 0x50d84,false);
        }
        if (s_tdc_mode==en__bipolar && (s_tdc_tick==8 || s_tdc_tick==10))
        {
            int first=s_tdc_tick==8 ? 0 : 24;
            int count=s_tdc_tick==8 ? 24 : 8;
            /* 주소0x04, write=1, configuration header=0x50000. */
            for (int i=0;i<count;++i)
                check(41,"reference_write",cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[i],
                      0x50900 | s_tdc_reference_electrode[first+i],false);
        }
        if ((s_tdc_backtel_mismatch || s_tdc_backtel_read_fail) && s_tdc_tick==9)
        {
            check(41,"read_failure_recovery",snd_isd_interface_get_state().isd_controlState,
                  s_tdc_backtel_read_fail ? en__isdStatus_PowerIC_OK : en__isdStatus_FPGA_Ok,false);
            break; /* 제품 상위 상태가 초기화로 전환된 뒤 단계를 강제 진행하지 않는다. */
        }
    }
    if (s_tdc_final_fail) check(41,"final_failure_recovery",snd_isd_interface_get_state().isd_controlState,en__isdStatus_PowerIC_OK,false);
}

static int before_copy(int success)
{
    ++s_tdc_copy_attempts; s_tdc_skipped=!success; return !success;
}

static void caller(bool fail)
{
    g_tdc_ut_cm3_calculation_before_copy=before_copy;
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag=0;
    check(29,"not_loaded",stimulationStandAlone(),0,false);
    check(29,"not_loaded_no_calculation",s_tdc_copy_attempts,0,false);
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag=1;
    if (fail) for (int i=0;i<32;++i) getPointerCurrentMapData()->C_level_uA[i]=2500;
    s_tdc_mode=en__monopolr_body; s_tdc_final_tick=14;
    set_newMapLoadeFlagForStimulParaSetting();
    check(29,"first_setting_incomplete",stimulationStandAlone(),0,false);
    check(29,"copy_attempt",s_tdc_copy_attempts,1,false);
    check(29,"uninitialized_read_skipped",s_tdc_skipped,fail,false);
    check(29,"completion_publication",cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag,!fail,fail);
    check(29,"error",readErrorCode().dataProcessingErrorFlag,fail ? en__unusableMapData : 0,false);
    observe(29,"recalculation_request_after",isNewMapLoadeFlag());
    if (fail)
    {
        observe(29,"pcm_after_failed_calculation",readCurrentPcmOutputMode());
        return; /* 실패 후 T/C 보존/제로화 정책을 시험에서 정하지 않는다. */
    }
    for (s_tdc_tick=1;s_tdc_tick<=15;++s_tdc_tick)
    {
        bool complete=stimulationStandAlone();
        check(41,"caller_completion",complete,s_tdc_tick==15,false);
    }
    check(41,"live_after_setting",readCurrentPcmOutputMode(),PcmBitStream_Mode_LiveStimulation,false);
    check(41,"no_new_map_no_recalculation",s_tdc_copy_attempts,1,false);
    set_newMapLoadeFlagForStimulParaSetting();
    check(41,"new_map_request",isNewMapLoadeFlag(),1,false);
    (void)stimulationStandAlone();
    check(41,"new_map_consumed",isNewMapLoadeFlag(),0,false);
    check(41,"new_map_calculated",s_tdc_copy_attempts,2,false);
    check(41,"new_map_incomplete",isStimulParaIsSettingDone(),0,false);
}

int main(int argc, char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    assert(argc==2); prepare();
    const char *name=argv[1];
    if (!strcmp(name,"selection")) selection();
    else if (!strncmp(name,"power-",6)) power_init(!strcmp(name,"power-ok"));
    else if (!strncmp(name,"fpga-",5)) fpga_init(!strcmp(name,"fpga-ok"));
    else if (!strncmp(name,"live-",5)) link_check(false,name);
    else if (!strncmp(name,"mapping-",8)) link_check(true,name);
    else if (!strncmp(name,"caller-",7)) caller(strcmp(name,"caller-ok")!=0);
    else setting(name);
    printf("EXTRA_RESULT checks=%u failed=%u known=%u\n",s_tdc_checks,s_tdc_failed,s_tdc_known);
    return s_tdc_failed==s_tdc_known ? 0 : 1;
}
