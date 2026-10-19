/* PC 전용: 실제 상태 함수와 RAM 공유 함수를 링크한다.
 * 장치/파일 경계만 고정 응답이며 각 시나리오는 별도 프로세스에서 시작한다.
 * 지역 static을 강제로 바꾸거나 제품 소스/매크로를 재작성하지 않는다. */
#if !defined(TDC_UNIT_TEST) || !defined(TDC_UT_CM3_HOST)
#tdc_sys_error "PC fixture only"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tdc_shm.h>
#include <tdc_isd.h>
#include <tdc_isd_fpga.h>
#include <tdc_isd_init_fpga.h>
#include <tdc_isd_stim_para_setting.h>
#include <tdc_isd_stim_standalone.h>
#include <tdc_stim_para_cal.h>
#include <tdc_stim_indicator.h>
#include <internalStimulationChip.h>
#include <tdc_drv_isl9122.h>
#include <tdc_isd_pcm.h>
#include <tdc_sys_error.h>
#include <tdc_fs_gain.h>
#include <tdc_ut_vectors_cm3.h>
#include <tdc_ut_coordination_cm3.h>
#include <electrodeMapping.h>
#include <tdc_hal_i2c_isd.h>

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
int tdc_hal_timer_get_tick(void) {return s_tdc_tick;}
void tdc_cfx_eeprom_write_user_setting_parameters(int isd) {(void)isd; abort();}
int tdc_fs_fft_read_pass_bin(int bands) {(void)bands; return 0;}
int tdc_fs_fft_init_pass_bin(int bands) {(void)bands; abort();}

bool tdc_drv_isl9122_reset(void) {return s_tdc_reset_ok;}
bool tdc_isd_fpga_write_change_tx_power_level(int value)
{
    assert(value >= TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE && value <= TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE);
    s_tdc_power = value; ++s_tdc_power_writes; return true;
}
bool tdc_isd_fpga_read_tx_power_level(int *value) {*value = s_tdc_power; ++s_tdc_power_reads; return true;}
EN_ISD_PowerState tdc_isd_fpga_read_isd_power_state(void) {++s_tdc_response_reads; return s_tdc_response;}
bool tdc_isd_fpga_write_clear_fifo(void)
{
    ++s_tdc_clear_calls;
    return s_tdc_clear_ok && !(s_tdc_final_fail && s_tdc_tick == s_tdc_final_tick);
}
void tdc_isd_fpga_reset_fpga_variable(void) {}
bool tdc_isd_fpga_write_reset(void) {return true;}
bool tdc_isd_fpga_read_version(int *value) {*value = 0x28; return true;}
bool tdc_isd_fpga_read_systemregister_1st(int *value)
{
    *value = s_tdc_tick == 11 ? FPGA_Status__Reset_value :
             s_tdc_fpga_ok ? FPGA_Status__OK_DisabledRF_value : FPGA_Status__Reset_value;
    return true;
}
bool tdc_isd_fpga_check_fpga_pcm_error(bool *value) {*value = false; return true;}
bool tdc_isd_fpga_check_fpga_fifo_empty(bool *value) {*value = true; return true;}
bool tdc_isd_fpga_read_system_error_flag(int *value) {*value = 0; return true;}
bool tdc_isd_fpga_read_pulse_width(int *value) {*value = s_tdc_width; return true;}
bool tdc_isd_fpga_read_backtel_config(int *value) {*value = s_tdc_backtel_config; return true;}
void tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(int value) {(void)value;}
void tdc_isd_fpga_update_fpga_backtel_config_written_value(int value) {(void)value;}
void tdc_isd_fpga_change_pulse_width(int index, int width) {(void)index; s_tdc_width = width;}
void tdc_isd_fpga_change_pulse_width_minimum(int index) {tdc_isd_fpga_change_pulse_width(index, FPGA_pulsePhaseWidth_minimum);}
int tdc_isd_fpga_change_8_bit_backtel_mode(int index) {(void)index; return s_tdc_backtel_config;}
int tdc_isd_fpga_disable_backtel(int index) {(void)index; return 0;}
bool tdc_isd_fpga_read_fifo_counter(int *value)
{
    *value = s_tdc_mode == en__bipolar && s_tdc_tick == 29 ? 32 : 2; return true;
}
/* readback는 사전 fixture다. 생성한 명령을 다시 읽어 기대값으로 쓰지 않는다. */
bool tdc_hal_i2c_isd_read(int address, int *values, int count)
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
    assert(tdc_shm_read_connected_isd_num() == s_tdc_old_isd); /* 모든 입력 준비 후 게시 */
    ++s_tdc_file_calls;
}
int tdc_fs_map_read_isd_info(int isd)
{
    file_read(isd);
    ST__CFX_CM3_SharedMemory_ISD_info *info = &cfx_cm3_sharedMemoryAll.cfx_ISD_info[isd-1];
    info->isd_year=19+isd; info->isd_month_model=isd; info->isd_serial=100+isd;
    info->isd_location_RL=isd;
    for (int i=0;i<25;++i) info->isd_userName[i]=100*isd+i;
    for (int i=0;i<4;++i) info->remocon_passkey[i]=1000*isd+i;
    return 0;
}
int tdc_fs_map_read_user_setting_value(int isd) {file_read(isd); return 0;}
int tdc_fs_map_read_map_stamp(int isd) {file_read(isd); return 0;}
int tdc_fs_map_read_map_data(int isd, int map) {file_read(isd); assert(map>=1 && map<=4); return 0;}
void tdc_cfx_eeprom_copy_map_info_to_cm3(int isd)
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
void tdc_cfx_eeprom_copy_user_setting_parameters_to_cm3(int isd)
{
    file_read(isd);
    cfx_cm3_sharedMemoryAll.userSettingValue.mapNum=isd==1 ? 1 : 3;
}
int tdc_gain_storage_load(int isd, ST__TDC_GAIN_SETTING *out)
{
    file_read(isd); memset(out,0,sizeof(*out));
    out->gain_table_index_a=isd; out->gain_table_index_b=isd+2; return 0;
}
void tdc_cfx_eeprom_copy_mapping_data_to_cm3(int map, int isd)
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
    tdc_sys_error_clear_all();
    ST__CFX_CM3_SharedMemory_mapData *map=tdc_shm_get_pointer_current_map_data();
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
    *tdc_stim_read_dac_register_value()=(ST_STIUL_DAC_REGISTER_VALUE){0,0,44};
}

static void selection(void)
{
    for (int step=0;step<3;++step)
    {
        int isd=step==1 ? 2 : 1;
        s_tdc_old_isd=tdc_shm_read_connected_isd_num(); s_tdc_file_calls=0;
        tdc_shm_change_connected_isd_num_cfx(isd);
        check(21,"selected",tdc_shm_read_connected_isd_num(),isd,false);
        check(21,"id",tdc_shm_read_isd_manufacture_id(isd-1),isd==1 ? 0x14010065 : 0x15020066,false);
        check(21,"location",tdc_shm_read_connected_isd_location(isd),isd,false);
        for (int i=0;i<25;++i) check(21,"synthetic_name",tdc_shm_read_connected_isd_user_name(isd)[i],100*isd+i,false);
        for (int i=0;i<4;++i) check(21,"synthetic_passkey",tdc_shm_read_remocon_passkey_connected_isd(isd)[i],1000*isd+i,false);
        check(21,"gain_a",cfx_cm3_sharedMemoryAll.gain_table_index_a,isd,false);
        check(21,"gain_b",cfx_cm3_sharedMemoryAll.gain_table_index_b,isd+2,false);
        check(21,"input_boundaries",s_tdc_file_calls,10,false);
        check(22,"count",tdc_shm_read_connected_isd_usable_map_num(),isd==1 ? 2 : 1,false);
        for (int i=0;i<4;++i) check(22,"usable",tdc_shm_read_connected_isd_usable_map_index()[i],isd==1 ? i<2 : i==2,false);
        check(22,"stamp",tdc_shm_read_connected_isd_map_stamp()[0],27-isd,false);
        for (int i=1;i<=4;++i) check(22,"date",tdc_shm_read_connected_isd_map_date(i)[2],10*isd+i,false);
        int before[4]; memcpy(before,tdc_shm_read_connected_isd_usable_map_index(),sizeof(before));
        tdc_shm_change_program_map_num(isd==1 ? 2 : 3);
        check(22,"program",tdc_shm_read_program_map_num(),isd==1 ? 2 : 3,false);
        check(22,"list_unchanged",memcmp(before,tdc_shm_read_connected_isd_usable_map_index(),sizeof(before))==0,1,false);
    }
}

static void power_init(bool ok)
{
    s_tdc_reset_ok=ok; tdc_isd_change_state(en__isdStatus_PowerIC_Reset);
    for (s_tdc_tick=0;s_tdc_tick<200;++s_tdc_tick)
    {
        tdc_isd_init_tx_power_ic(s_tdc_tick==0);
        if (s_tdc_tick==0 || s_tdc_tick==3)
            check(31,"power_request",cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX,s_tdc_tick==0 ? 2 : 1,false);
        if (!ok && s_tdc_tick==10)
        {
            check(31,"reset_failure_not_complete",tdc_isd_get_state().isd_controlState,en__isdStatus_PowerIC_Reset,false);
            return; /* 이후 하위 드라이버 복구까지 모델링하지 않는다. */
        }
        if (tdc_isd_get_state().isd_controlState==en__isdStatus_PowerIC_OK) break;
        check(31,"incomplete",tdc_isd_get_state().isd_controlState,en__isdStatus_PowerIC_Reset,false);
    }
    check(31,"power_ready",tdc_isd_get_state().isd_controlState,en__isdStatus_PowerIC_OK,false);
    check(31,"power_upper_bound",s_tdc_power,TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE,false);
    check(31,"ramp_executed",s_tdc_power_writes>1,1,false);
}

static void fpga_init(bool ok)
{
    s_tdc_fpga_ok=ok; tdc_isd_change_state(en__isdStatus_PowerIC_OK);
    for (s_tdc_tick=0;s_tdc_tick<=20;++s_tdc_tick)
    {
        tdc_isd_init_fpga(s_tdc_tick==0);
        if (s_tdc_tick<20) check(31,"fpga_not_ready",tdc_isd_get_state().isd_controlState,en__isdStatus_PowerIC_OK,false);
    }
    check(31,"fpga_result",tdc_isd_get_state().isd_controlState,ok ? en__isdStatus_FPGA_Ok : en__isdStatus_PowerIC_OK,false);
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
        tdc_shm_clear_connection_check_pcm_fired_flag();
        tdc_isd_update_link_by_backtel_live();
        check(unit,"no_completion_no_read",s_tdc_response_reads,0,false);
        cfx_cm3_sharedMemoryAll.cfx_PCM_interface.conneded_ISDCheckPCM_state=BackelCircuitDisabled_readPcmFired_duringLiveStimulation;
    }
    for (int tick=0;tick<(mapping ? 7 : 3);++tick)
    {
        if (mapping) tdc_isd_update_link_by_backtel_mapping(tick);
        else tdc_isd_update_link_by_backtel_live();
        if (mapping) check(unit,"progress",tdc_isd_is_connection_check_with_mapping(),tick<6,false);
    }
    check(unit,"received_once",s_tdc_response_reads,1,false);
    check(unit,"fifo_clear",s_tdc_clear_calls,1,false);
    check(unit,"i2c_released",tdc_isd_is_i2c_free(),1,false);
    int expected_state=!s_tdc_clear_ok ? en__isdStatus_PowerIC_OK :
        (s_tdc_response==NoBacktel || s_tdc_response==BackTelNumTooMuch) ? en__isdStatus_FPGA_Ok : en__isdStatus_stimul_10V_Ok;
    check(unit,"recovery_state",tdc_isd_get_state().isd_controlState,expected_state,false);
    check(unit,"tdc_sys_error",tdc_sys_error_read().ISD_ErrorFlag,s_tdc_response==NoBacktel ? en__BackTelCounterZero :
          s_tdc_response==BackTelNumTooMuch ? en__BackterDataLengthError : 0,false);
    if (!mapping) check(unit,"completion_cleared",tdc_shm_read_connection_check_pcm_state(),BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation,false);
    if (s_tdc_response==ISD_Power_HighStable || s_tdc_response==ISD_Power_LowStable)
        check(unit,"power_adjustment",s_tdc_power,s_tdc_response==ISD_Power_HighStable ? 189 : 191,false);
    if (mapping)
    {
        tdc_isd_update_link_by_backtel_mapping(7);
        check(unit,"no_stale_progress",tdc_isd_is_connection_check_with_mapping(),0,false);
        tdc_isd_update_link_by_backtel_mapping(0);
        check(unit,"restart_progress",tdc_isd_is_connection_check_with_mapping(),1,false);
    }
}

static void setting(const char *name)
{
    s_tdc_mode=strcmp(name,"bipolar")==0 ? en__bipolar : en__monopolr_body;
    tdc_shm_get_pointer_current_map_data()->stimulationMode=s_tdc_mode;
    s_tdc_final_tick=s_tdc_mode==en__bipolar ? 39 : 14;
    s_tdc_final_fail=strstr(name,"final-fail")!=NULL;
    s_tdc_backtel_read_fail=strstr(name,"read-fail")!=NULL;
    s_tdc_backtel_mismatch=strstr(name,"read-mismatch")!=NULL;
    tdc_isd_clear_stim_para_setting_done();
    for (s_tdc_tick=0;s_tdc_tick<=s_tdc_final_tick;++s_tdc_tick)
    {
        bool tdc_sys_error=tdc_isd_stim_setting_step(s_tdc_tick==0);
        check(41,"step_error",tdc_sys_error,s_tdc_backtel_mismatch && s_tdc_tick==9,false);
        check(41,"completion",tdc_isd_is_stim_para_setting_done(),
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
            check(41,"read_failure_recovery",tdc_isd_get_state().isd_controlState,
                  s_tdc_backtel_read_fail ? en__isdStatus_PowerIC_OK : en__isdStatus_FPGA_Ok,false);
            break; /* 제품 상위 상태가 초기화로 전환된 뒤 단계를 강제 진행하지 않는다. */
        }
    }
    if (s_tdc_final_fail) check(41,"final_failure_recovery",tdc_isd_get_state().isd_controlState,en__isdStatus_PowerIC_OK,false);
}

static int before_copy(int success)
{
    ++s_tdc_copy_attempts; s_tdc_skipped=!success; return !success;
}

static void caller(bool fail)
{
    g_tdc_ut_cm3_calculation_before_copy=before_copy;
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag=0;
    check(29,"not_loaded",tdc_isd_stim_standalone_step(),0,false);
    check(29,"not_loaded_no_calculation",s_tdc_copy_attempts,0,false);
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag=1;
    if (fail) for (int i=0;i<32;++i) tdc_shm_get_pointer_current_map_data()->C_level_uA[i]=2500;
    s_tdc_mode=en__monopolr_body; s_tdc_final_tick=14;
    tdc_isd_set_new_map_loaded_flag();
    check(29,"first_setting_incomplete",tdc_isd_stim_standalone_step(),0,false);
    check(29,"copy_attempt",s_tdc_copy_attempts,1,false);
    check(29,"uninitialized_read_skipped",s_tdc_skipped,fail,false);
    check(29,"completion_publication",cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag,!fail,fail);
    check(29,"tdc_sys_error",tdc_sys_error_read().dataProcessingErrorFlag,fail ? en__unusableMapData : 0,false);
    observe(29,"recalculation_request_after",tdc_isd_is_new_map_loaded_flag());
    if (fail)
    {
        observe(29,"pcm_after_failed_calculation",tdc_shm_read_current_pcm_output_mode());
        return; /* 실패 후 T/C 보존/제로화 정책을 시험에서 정하지 않는다. */
    }
    for (s_tdc_tick=1;s_tdc_tick<=15;++s_tdc_tick)
    {
        bool complete=tdc_isd_stim_standalone_step();
        check(41,"caller_completion",complete,s_tdc_tick==15,false);
    }
    check(41,"live_after_setting",tdc_shm_read_current_pcm_output_mode(),PcmBitStream_Mode_LiveStimulation,false);
    check(41,"no_new_map_no_recalculation",s_tdc_copy_attempts,1,false);
    tdc_isd_set_new_map_loaded_flag();
    check(41,"new_map_request",tdc_isd_is_new_map_loaded_flag(),1,false);
    (void)tdc_isd_stim_standalone_step();
    check(41,"new_map_consumed",tdc_isd_is_new_map_loaded_flag(),0,false);
    check(41,"new_map_calculated",s_tdc_copy_attempts,2,false);
    check(41,"new_map_incomplete",tdc_isd_is_stim_para_setting_done(),0,false);
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
