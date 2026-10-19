/* PC 전용: checker가 원본 제품 함수체 뒤에 포함한다. 제품 파일/훅은 바꾸지 않는다.
 * ADC/DIO SDK 호출과 HEAR vMag 주소만 RAM 경계로 관찰한다. 실제 가속기 시험 아님.
 * 각 입력은 두 번 독립 실행하고 공유값/지역 정적 상태까지 저장/복원한다.
 */
static int s_tdc_ut_adc_delay[3], s_tdc_ut_adc_writes, s_tdc_ut_dio_writes;

void tdc_ut_cfx_io_host_adc(int channel, const char *setting)
{
    assert(channel==0 || channel==2);
    assert(!strcmp(setting,"LIB_ADC_DEC_CTRL_VAL") ||
           !strcmp(setting,"LIB_ADC_DEC_CTRL_VAL_0_0250"));
    s_tdc_ut_adc_delay[channel]=!strcmp(setting,"LIB_ADC_DEC_CTRL_VAL_0_0250");
    s_tdc_ut_adc_writes++;
}

void tdc_ut_cfx_io_host_dio(const char *pin, const char *setting)
{
    assert(!strcmp(pin,"DIO22") || !strcmp(pin,"DIO10"));
    assert(strstr(setting,"DIO_MODE_ADCCLK") || strstr(setting,"DIO_MODE_DISABLE"));
    s_tdc_ut_dio_writes++;
}

/* 호스트에서 실제로 접근하는 제품 상태 전체. padding은 0 초기화하여 비교한다. */
typedef struct {
    ST__CFX_CM3_SharedMemory_ALL shared;
    FS_MEM_UART_T uart;
    int words[4096];
} tdc_ut_cfx_io_host_state_t;

static void host_state(tdc_ut_cfx_io_host_state_t *state, int restore)
{
    int *arrays[]={addr_MapProgramData_mappingDate,
        addr_MapProgramData_StimulusChannelAssignedElectrodIndex,
        addr_MapProgramData_ReferenceChannelAssignedElectrodIndex,addr_MapProgramData_CIS_FreqBandOrder,
        addr_MapProgramData_T_Level_uA,addr_MapProgramData_C_Level_uA,
        addr_MapProgramData_xMinLevel,addr_MapProgramData_xMaxLevel,
        g_mapping_stimulus_amplitude_T_level,g_mapping_stimulus_amplitude_C_level,
        g_max_limit_stimulus_amplitude_C_level,g_logaritmMapping_coeff_A_Q16_8,
        g_logaritmMapping_coeff_B_Q16_8,g_pcm_amplitude_level,
        addr_electrodeMap,addr_stimulationTempBuff,addr_stimulationData,
        g_NofM_adjacent_BandIndex,g_NofM_notAdjacent_BandIndex,
        g_freq_rep_values,g_freq_rep_values_scaled,g_pass_bin_index,s_tdc_ut_host_vmag,
        m_agc_output_buffer,lib_g_i2s_buffer_output,
        &lib_g_i2s_buffers[0][0],&g_lib_dmic_in_buffers[0][0][0]};
    const int lengths[]={6,32,32,32,32,32,32,32,32,32,32,32,32,32,
        32,32,32,16,16,32,32,256,256,16,16,128,64};
    volatile int *scalars[]={&addr_MapProgramData_stimulationStrategy,&addr_MapProgramData_firstPulsePhase,
        &addr_MapProgramData_StimulationMode,&addr_MapProgramData_StimulationPulsePhaseWidth,
        &addr_MapProgramData_FrequencyAnalysisBandNumbers,&addr_MapProgramData_indicatorStimulCannel_index,
        &addr_MapProgramData_indicatorStimulLevel_uA,&m_previous_audio_volume,
        &m_previous_stimulation_volume,&m_audio_volume,&m_current_isd,&m_previous_isd,
        &m_prev_agc_10db_gain_Q8_16,&m_link_connection_check_counter,&g_pcm_stimulation_packet_header,
        &g_pcmFrameNum_per_channel,&g_transferableChannelNum_per_1msec,&addr_transferred_index,
        &g_NofM_LastStimulus_BandIndex,&g_NofM_Phase,&s_lib_audio_front_mic,&s_enabled_dmic_cnt,
        &lib_g_i2s_buffer_in_pos,&lib_g_i2s_buffer_out_pos,&lib_g_i2s_buffer_copy_cnt,
        &lib_g_i2s_buffer_state,&lib_g_i2s_buffer_sub_state,&lib_g_i2s_state,
        &g_tdc_ut_io_override_enabled,&g_tdc_ut_power_override_enabled,&g_tdc_ut_agc_override_enabled};
    int pos=0;
    if (!restore) { memset(state,0,sizeof(*state)); state->shared=shared; state->uart=host_uart; }
    else { shared=state->shared; host_uart=state->uart; }
    for (unsigned a=0; a<sizeof(arrays)/sizeof(*arrays); a++)
        for (int i=0; i<lengths[a]; i++,pos++) {
            assert(pos<4096);
            if (restore) arrays[a][i]=state->words[pos]; else state->words[pos]=arrays[a][i];
        }
    for (unsigned i=0; i<sizeof(scalars)/sizeof(*scalars); i++,pos++)
        if (restore) *scalars[i]=state->words[pos]; else state->words[pos]=*scalars[i];
    if (restore) {
        lib_g_i2s_interrupt_flag=state->words[pos++];
        lib_g_i2s_buffer_copy_done=state->words[pos++];
    } else {
        state->words[pos++]=lib_g_i2s_interrupt_flag;
        state->words[pos++]=lib_g_i2s_buffer_copy_done;
    }
    assert(pos<4096);
}

static void host_begin(void)
{
    g_tdc_ut_cfx_io_count=g_tdc_ut_cfx_io_failed=g_tdc_ut_cfx_io_overflow=0;
    g_tdc_ut_io_override_enabled=1;
    s_tdc_ut_adc_writes=s_tdc_ut_dio_writes=0;
    s_tdc_ut_adc_delay[0]=s_tdc_ut_adc_delay[2]=-1;
    reset_trace();
}

static void host_finish(int unit, int vector, int repeat)
{
    assert(!g_tdc_ut_cfx_io_overflow);
    if (g_tdc_ut_cfx_io_failed)
        for (int i=0; i<g_tdc_ut_cfx_io_count; i++)
            if (g_tdc_ut_cfx_io_actual[i]!=g_tdc_ut_cfx_io_expected[i])
                printf("EXT CFX-%d case=%d offset=%d A=%d E=%d\n",unit,vector,i,
                    g_tdc_ut_cfx_io_actual[i],g_tdc_ut_cfx_io_expected[i]);
    assert(!g_tdc_ut_cfx_io_failed);
    printf("CFX_EXTRA {\"unit\":%d,\"case\":%d,\"repeat\":%d,\"actual\":[",unit,vector,repeat);
    for (int i=0; i<g_tdc_ut_cfx_io_count; i++) printf("%s%d",i ? "," : "",g_tdc_ut_cfx_io_actual[i]);
    printf("],\"expected\":[");
    for (int i=0; i<g_tdc_ut_cfx_io_count; i++) printf("%s%d",i ? "," : "",g_tdc_ut_cfx_io_expected[i]);
    puts("]}");
}

static void host_map_case(int vector)
{
    /* 정상 맵의 좌/우/미지정, 스트리밍, Live 날짜 보존, 요청 없음, bin 불일치. */
    int live=vector==4, idle=vector==5, mismatch=vector==6;
    int expected_front=vector==0 ? LIB_AUDIO_FRONT_MIC_LEFT :
        vector==1 || vector==3 ? LIB_AUDIO_FRONT_MIC_RIGHT : LIB_AUDIO_FRONT_MIC_NONE;
    host_begin();
    shared.userSettingValue.mapNum=live ? -1 : 0;
    m_current_isd=1; s_lib_audio_front_mic=LIB_AUDIO_FRONT_MIC_NONE;
    s_enabled_dmic_cnt=2;
    shared.cfx_ISD_info[0].isd_location_RL=vector==0 ? Left_Ear :
        vector==1 || vector==3 ? Right_Ear : 0;
    tdc_i2s_set_streaming_state(vector==3 ? LIB_I2S_STATE_ENABLED : LIB_I2S_STATE_DISABLED);
    shared.mapChangeFlag.cm3Command_mapChange=idle ? 0 : 1;
    shared.mapChangeFlag.cfx_Reloaded_MapdataFlag=0;
    shared.mapChangeFlag.cm3_audioParameterCalculationDone_Flag=0;
    shared.currentMapData.stimulationStrategy=vector==1 ? df_stimulationStrategy_CIS : df_stimulationStrategy_NofM;
    shared.currentMapData.firstPulsePhase=1;
    shared.currentMapData.numFrequencyBand=32;
    shared.currentMapData.stimulationMode=en__bipolar;
    shared.currentMapData.stimulationPulsePhaseWidth=40;
    shared.currentMapData.stimulationIndicatorChannelNum=12;
    shared.currentMapData.stimulationIndicatorAmplitude_uA=90;
    int *source_date=(int *)&shared.currentMapData.mappingDate;
    for (int i=0; i<6; i++) { source_date[i]=10+i; addr_MapProgramData_mappingDate[i]=70+i; }
    int *dest[]={addr_MapProgramData_StimulusChannelAssignedElectrodIndex,
        addr_MapProgramData_ReferenceChannelAssignedElectrodIndex,addr_MapProgramData_CIS_FreqBandOrder,
        addr_MapProgramData_T_Level_uA,addr_MapProgramData_C_Level_uA,
        addr_MapProgramData_xMinLevel,addr_MapProgramData_xMaxLevel};
    for (int i=0; i<32; i++) {
        shared.currentMapData.usableStimulationElectrodIndex[i]=i+1;
        shared.currentMapData.usableReferenceElectrodIndex[i]=32-i;
        shared.currentMapData.CIS_FreqBandOrder[i]=i+1;
        shared.currentMapData.T_level_uA[i]=50+i; shared.currentMapData.C_level_uA[i]=150+i;
        shared.currentMapData.audio_input_x_mim[i]=300+i; shared.currentMapData.audio_input_x_max[i]=600+i;
        for (int a=0; a<7; a++) dest[a][i]=-100-a;
    }
    addr_MapProgramData_FrequencyAnalysisBandNumbers=1;
    g_tdc_ut_io_fs_band_count=mismatch ? 31 : 32;
    for (int i=0; i<256; i++) { g_tdc_ut_io_fs_bins[i]=i%32; g_pass_bin_index[i]=-1; }
    g_NofM_Phase=1; g_pcm_stimulation_packet_header=123;
    Normal_PowerMode_event_mapChange();
    put(shared.mapChangeFlag.cm3Command_mapChange,0);
    /* bin 불일치에서도 ACK=1은 현재 동작의 관찰이다. 안전한 실패 계약으로 해석하지 않는다. */
    put(shared.mapChangeFlag.cfx_Reloaded_MapdataFlag,!idle);
    put(shared.mapChangeFlag.cm3_audioParameterCalculationDone_Flag,0);
    put(g_NofM_Phase,idle || vector==1 ? 1 : 0);
    put(g_pcm_stimulation_packet_header,idle ? 123 : 0x48000);
    put(s_enabled_dmic_cnt,live || vector==3 ? 1 : 2);
    put(s_lib_audio_front_mic,expected_front);
    put(s_tdc_ut_adc_writes,live || idle ? 0 : 2);
    put(s_tdc_ut_dio_writes,idle ? 0 : 2);
    put(s_tdc_ut_adc_delay[0],live || idle ? -1 : expected_front==LIB_AUDIO_FRONT_MIC_RIGHT);
    put(s_tdc_ut_adc_delay[2],live || idle ? -1 : expected_front==LIB_AUDIO_FRONT_MIC_LEFT);
    put(s_tdc_ut_pcm_writes,idle ? 0 : 24);
    for (int i=0; i<24; i++) put(s_tdc_ut_pcm[47-i],idle ? -777 : 0x25555);
    for (int i=0; i<6; i++) put(addr_MapProgramData_mappingDate[i],(live || idle ? 70 : 10)+i);
    for (int a=0; a<7; a++) for (int i=0; i<32; i++)
        put(dest[a][i],idle ? -100-a : a==0 || a==2 ? i+1 : a==1 ? 32-i : a==3 ? 50+i : a==4 ? 150+i : a==5 ? 300+i : 600+i);
    for (int i=0; i<256; i++) put(g_pass_bin_index[i],idle || mismatch ? -1 : i%32);
    /* 완료한 명령은 재실행하지 않는다. */
    reset_trace(); Normal_PowerMode_event_mapChange(); put(s_tdc_ut_pcm_writes,0);
}

static void host_calculation_case(void)
{
    host_begin();
    shared.mapChangeFlag.cm3_audioParameterCalculationDone_Flag=1;
    shared.calculatedStimulPara_byCM3.frameNumPerChannel=3;
    shared.calculatedStimulPara_byCM3.transferableFrameNum=8;
    for (int i=0; i<32; i++) {
        shared.calculatedStimulPara_byCM3.T_level_255[i]=50+i;
        shared.calculatedStimulPara_byCM3.C_level_255[i]=150+i;
        g_mapping_stimulus_amplitude_T_level[i]=-1; g_mapping_stimulus_amplitude_C_level[i]=-1;
        addr_MapProgramData_xMinLevel[i]=0; addr_MapProgramData_xMaxLevel[i]=2;
    }
    for (int i=0; i<256; i++) s_tdc_ut_host_vmag[i]=i+1;
    Normal_PowerMode_event_audioParameterCalculation();
    put(g_pcmFrameNum_per_channel,3); put(g_transferableChannelNum_per_1msec,8);
    put(m_previous_audio_volume,-1); put(m_previous_stimulation_volume,-1);
    put(shared.mapChangeFlag.cm3_audioParameterCalculationDone_Flag,0);
    for (int i=0; i<32; i++) { put(g_mapping_stimulus_amplitude_T_level[i],50+i); put(g_mapping_stimulus_amplitude_C_level[i],150+i); }
    for (int i=0; i<256; i++) put(s_tdc_ut_host_vmag[i],0);
    /* 계산 완료 후 요청된 볼륨을 반영하면 새 T/C로 계수가 만들어져야 한다. */
    shared.userSettingValue.stimulVolume=4; shared.userSettingValue.audioVolume=1;
    g_tdc_ut_power_override_enabled=1;
    Normal_PowerMode_event_both_stimulation_audio_volumeChange();
    for (int i=0; i<32; i++) {
        put(g_max_limit_stimulus_amplitude_C_level[i],150+i);
        put(g_logaritmMapping_coeff_A_Q16_8[i],12800);
        put(g_logaritmMapping_coeff_B_Q16_8[i],(50+i)*256);
    }
    s_tdc_ut_host_vmag[0]=77;
    Normal_PowerMode_event_audioParameterCalculation();
    put(s_tdc_ut_host_vmag[0],77); put(m_previous_stimulation_volume,4); put(m_previous_audio_volume,1);
}

static void host_strategy_case(int vector)
{
    static const int order[16]={0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15};
    host_begin();
    addr_MapProgramData_stimulationStrategy=vector==0 ? df_stimulationStrategy_CIS :
        vector==1 ? df_stimulationStrategy_NofM : vector==2 ? df_stimulationStrategy_medium : 99;
    addr_MapProgramData_FrequencyAnalysisBandNumbers=32;
    addr_MapProgramData_firstPulsePhase=0; prepare_pcmStimulationPacketHeader();
    shared.is_enabled_mute_stimulation_under_t_level=2;
    shared.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3=0;
    g_tdc_ut_power_override_enabled=1;
    for (int i=0; i<256; i++) { g_pass_bin_index[i]=i%32; s_tdc_ut_host_vmag[i]=32; }
    for (int i=0; i<32; i++) {
        g_logaritmMapping_coeff_A_Q16_8[i]=256; g_logaritmMapping_coeff_B_Q16_8[i]=(100-i)*256;
        g_max_limit_stimulus_amplitude_C_level[i]=255;
        g_mapping_stimulus_amplitude_T_level[i]=0;
        g_pcm_amplitude_level[i]=-55; shared.currentOutputStimulLevel_255[i]=-66;
        g_freq_rep_values[i]=-77; g_freq_rep_values_scaled[i]=-88;
        addr_MapProgramData_CIS_FreqBandOrder[i]=i+1;
        addr_MapProgramData_StimulusChannelAssignedElectrodIndex[i]=i+1; addr_electrodeMap[i]=i;
        addr_stimulationTempBuff[i]=-99;
    }
    g_NofM_Phase=0; g_NofM_LastStimulus_BandIndex=15; addr_transferred_index=0;
    g_pcmFrameNum_per_channel=vector==1 ? 3 : 2;
    g_transferableChannelNum_per_1msec=vector==1 ? 8 : 12;
    for (int block=0; block<3; block++) {
        reset_trace(); stimulationStrategy();
        int active=vector<2;
        /* NofM은 24 NOP 초기화 뒤 자극 위치를 덮어쓴다. 최종 24워드를 아래서 비교한다. */
        put(s_tdc_ut_pcm_writes>0,active);
        put(addr_transferred_index,vector==0 ? (block+1)*12%32 : vector==1 ? (block%2 ? 0 : 8) : 0);
        put(g_NofM_Phase,vector==1 ? (block%2 ? 0 : 1) : 0);
        for (int i=0; i<32; i++) {
            put(g_pcm_amplitude_level[i],active ? 102-i : -55);
            put(shared.currentOutputStimulLevel_255[i],active ? 102-i : -66);
            put(g_freq_rep_values[i],active ? 32 : -77); put(g_freq_rep_values_scaled[i],active ? 2 : -88);
        }
        for (int i=0; i<24; i++) {
            int stride=vector==1 ? 3 : 2;
            int index=vector==1 ? order[(block%2)*8+i/stride] : (block*12+i/stride)%32;
            int expected=active ? (i%stride ? 0x25555 : 0x40000 | (index<<10) | ((102-index)<<2)) : -777;
            put(s_tdc_ut_pcm[47-i],expected);
        }
        if (vector==1) for (int i=0; i<16; i++) {
            put(g_NofM_adjacent_BandIndex[i],i); put(g_NofM_notAdjacent_BandIndex[i],order[i]);
        }
        /* Phase 1은 입력을 다시 계산하지 않아야 한다. 다음 phase 0 전에는 원복. */
        if (vector==1) for (int i=0; i<256; i++) s_tdc_ut_host_vmag[i]=block==0 ? 0 : 32;
    }
}

static void tdc_ut_cfx_io_host_cases(void)
{
    static tdc_ut_cfx_io_host_state_t before, after;
    /* 모든 변경 대상에 서로 다른 값을 둬 0 복원을 정상 복원으로 오인하지 않는다. */
    for (int i=0; i<32; i++) {
        addr_MapProgramData_T_Level_uA[i]=301+i; g_mapping_stimulus_amplitude_T_level[i]=401+i;
        g_logaritmMapping_coeff_A_Q16_8[i]=501+i; g_logaritmMapping_coeff_B_Q16_8[i]=601+i;
        g_mapping_stimulus_amplitude_C_level[i]=701+i; g_max_limit_stimulus_amplitude_C_level[i]=801+i;
    }
    const int units[]={11,33};
    for (unsigned u=0; u<2; u++) for (int repeat=0; repeat<2; repeat++) {
        host_state(&before,0);
        assert(tdc_ut_cfx_io_run_unit(units[u])==0);
        host_state(&after,0); assert(!memcmp(&before,&after,sizeof(before)));
        assert(tdc_ut_cfx_io_release()==0);
    }
    for (int repeat=0; repeat<2; repeat++) for (int vector=0; vector<12; vector++) {
        host_state(&before,0);
        if (vector<7) host_map_case(vector);
        else if (vector==7) host_calculation_case();
        else host_strategy_case(vector-8);
        host_finish(vector<8 ? 11 : 33,100+vector,repeat);
        host_state(&before,1); host_state(&after,0);
        assert(!memcmp(&before,&after,sizeof(before)));
    }
    /* 미지원/점유 중 호출은 캡처와 제품 상태를 모두 보존해야 한다. */
    static int actual[TDC_UT_CFX_IO_CAPACITY], expected[TDC_UT_CFX_IO_CAPACITY];
    memcpy(actual,g_tdc_ut_cfx_io_actual,sizeof(actual)); memcpy(expected,g_tdc_ut_cfx_io_expected,sizeof(expected));
    int *busy[]={&g_tdc_ut_io_override_enabled,&g_tdc_ut_power_override_enabled,&g_tdc_ut_agc_override_enabled};
    for (int i=0; i<3; i++) {
        *busy[i]=1; host_state(&before,0);
        assert(tdc_ut_cfx_io_run_unit(11)==TDC_UT_IO_BUSY);
        assert(tdc_ut_cfx_io_run_unit(33)==TDC_UT_IO_BUSY);
        host_state(&after,0); assert(!memcmp(&before,&after,sizeof(before)));
        assert(!memcmp(actual,g_tdc_ut_cfx_io_actual,sizeof(actual)));
        assert(!memcmp(expected,g_tdc_ut_cfx_io_expected,sizeof(expected))); *busy[i]=0;
    }
    /* 경계 초과에서 쓰기/결과를 거부한다. 실제 잘못된 제품 배열 접근은 실행하지 않는다. */
    host_begin(); int saved=s_tdc_ut_pcm[48];
    tdc_ut_io_pcm_write(&s_tdc_ut_pcm[48],123);
    assert(g_tdc_ut_cfx_io_overflow && !s_tdc_ut_pcm_writes && s_tdc_ut_pcm[48]==saved);
    g_tdc_ut_cfx_io_overflow=0; s_tdc_ut_pcm_writes=128; saved=s_tdc_ut_pcm[47];
    tdc_ut_io_pcm_write(&s_tdc_ut_pcm[47],123);
    assert(g_tdc_ut_cfx_io_overflow && s_tdc_ut_pcm_writes==128 && s_tdc_ut_pcm[47]==saved);
    g_tdc_ut_cfx_io_overflow=0; g_tdc_ut_cfx_io_count=TDC_UT_CFX_IO_CAPACITY;
    put(123,123); assert(g_tdc_ut_cfx_io_overflow && g_tdc_ut_cfx_io_count==TDC_UT_CFX_IO_CAPACITY);
    g_tdc_ut_io_override_enabled=0;
    puts("CFX host-only map/calculation/dispatch: 12 cases twice; full state restore and guard checks passed");
}
