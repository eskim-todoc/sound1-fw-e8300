/* CFX TD24A I/O 벡터. 명시적 디버거/시험 호출만 사용한다.
 * 설정: 오디오/PCM/CM3 갱신 정지, 정상 HW 경로는 사용하지 않는 RAM 경계로 캡처.
 * 입력/출력: 기준서 ID별 벡터와 독립 기대값, [-999,unit,case,length,actual...].
 * 복원: 정적 백업으로 제품 상태를 되돌리고 release 전 캡처를 유지한다.
 * IRQ 상태, RF 성공, 실제 FIFO 전송 타이밍은 이 시험의 판정 대상이 아니다.
 */
#if defined(TDC_UNIT_TEST)
#include <main.h>
#include <FrequencyAnalysis.h>
#include <nonlinearMapping.h>
#include "tdc_ut_hooks.h"
#include "tdc_ut_io_cfx.h"
#include "tdc_ut_vectors_cfx.h"
#include "tdc_ut_hear_reference.h"

int _XMEM g_tdc_ut_cfx_io_actual[TDC_UT_CFX_IO_CAPACITY];
int _XMEM g_tdc_ut_cfx_io_expected[TDC_UT_CFX_IO_CAPACITY];
int g_tdc_ut_cfx_io_count, g_tdc_ut_cfx_io_failed, g_tdc_ut_cfx_io_overflow;
volatile int g_tdc_ut_cfx_io_done;
int g_tdc_ut_io_override_enabled;
int _XMEM g_tdc_ut_io_dmic_fifo[2][16];
int _IOMEM g_tdc_ut_io_fs_band_count;
int _IOMEM g_tdc_ut_io_fs_bins[256];
static int _XMEM s_tdc_ut_pcm[64];
static int _XMEM s_tdc_ut_pcm_trace[128][4];
static int s_tdc_ut_pcm_writes;
static int _XMEM s_tdc_ut_fft_trace[16];
static int s_tdc_ut_fft_writes;
static int _XMEM s_tdc_ut_input[16];
/* 기댓값은 출력 버퍼에서 복사하지 않고 고정 입력의 비트 배치로 따로 만든다. */
static int _XMEM s_tdc_ut_packet_expected[32];
static int _XMEM s_tdc_ut_saved_electrode_map[32];
static int _XMEM s_tdc_ut_saved_dmic[2][LIB_AUDIO_IN_BUF_MAX_CNT][16];
static int _XMEM s_tdc_ut_saved_i2s[I2S_BUFFER_FULL_READY_CNT][16];
static int _XMEM s_tdc_ut_saved_agc[16], s_tdc_ut_saved_i2s_output[16];
static int _XMEM s_tdc_ut_saved_bins[256];
static int _XMEM s_tdc_ut_saved_arrays[13][32];
static int _XMEM s_tdc_ut_saved_date[6];
static ST__CFX_CM3_SharedMemory_mapData _XMEM s_tdc_ut_saved_map;
static ST__CFX_CM3_SharedMemory_userSettingValue _XMEM s_tdc_ut_saved_user;
static CFX_CM3_SharedMemory_PCM_interface _XMEM s_tdc_ut_saved_pcm;
static ST__CFX_CM3_SharedMemory_MapDataChange _XMEM s_tdc_ut_saved_flags;
static int _XMEM s_tdc_ut_saved_scalar[28];
static int _IOMEM s_tdc_ut_saved_uart[224];
extern int _XMEM m_current_isd, m_previous_isd;
extern volatile int _XMEM lib_g_i2s_buffer_copy_done;

int _XMEM *tdc_ut_io_pcm_top(void)
{
    if (g_tdc_ut_io_override_enabled) return &s_tdc_ut_pcm[47];
    return (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;
}

void tdc_ut_io_pcm_write(int _XMEM *p_word, int value)
{
    if (g_tdc_ut_io_override_enabled)
    {
        int offset = (int) (&s_tdc_ut_pcm[47] - p_word);
        if (offset < 0 || offset >= 24 || s_tdc_ut_pcm_writes >= 128)
        { g_tdc_ut_cfx_io_overflow = 1; return; }
        s_tdc_ut_pcm_trace[s_tdc_ut_pcm_writes][0] = offset;
        s_tdc_ut_pcm_trace[s_tdc_ut_pcm_writes][1] = value;
        s_tdc_ut_pcm_trace[s_tdc_ut_pcm_writes][2] = Addr_SharedMem->is_pcm_specific_command_reading;
        s_tdc_ut_pcm_trace[s_tdc_ut_pcm_writes][3] = Addr_SharedMem->pcm_specific_command_read_index;
        s_tdc_ut_pcm_writes++;
    }
    *p_word = value;
}

void tdc_ut_io_fft_write(int value)
{
    if (g_tdc_ut_io_override_enabled)
    {
        if (s_tdc_ut_fft_writes >= 16) { g_tdc_ut_cfx_io_overflow = 1; return; }
        s_tdc_ut_fft_trace[s_tdc_ut_fft_writes++] = value;
    }
    else D_FIFO_A1_0->ACCESS = value;
}

static void put(int actual, int expected)
{
    int idx = g_tdc_ut_cfx_io_count;
    if (idx >= TDC_UT_CFX_IO_CAPACITY) { g_tdc_ut_cfx_io_overflow = 1; return; }
    g_tdc_ut_cfx_io_actual[idx] = actual;
    g_tdc_ut_cfx_io_expected[idx] = expected;
    g_tdc_ut_cfx_io_count++;
    if (actual != expected) g_tdc_ut_cfx_io_failed++;
}
static void mark(int unit, int vector, int length)
{ put(-999,-999); put(unit,unit); put(vector,vector); put(length,length); }
static void reset_trace(void)
{
    s_tdc_ut_pcm_writes = s_tdc_ut_fft_writes = 0;
    for (int i = 0; i < 24; i++) s_tdc_ut_pcm[47-i] = -777;
}
static void capture_pcm(int unit, int vector, int start, int pulses, int stride)
{
    mark(unit, vector, 25);
    for (int i = 0; i < 24; i++)
    {
        int expected = pcm_Mold_NopStandby;
        if (i % stride == 0 && i / stride < pulses)
            expected = s_tdc_ut_packet_expected[(start + i / stride) % (unit == 33 ? 32 : 16)];
        put(s_tdc_ut_pcm[47-i], expected);
    }
    put(addr_transferred_index, (start + pulses) % (unit == 33 ? 32 : 16));
}

static void backup(int restore)
{
    int _XMEM *arrays[13] = {addr_MapProgramData_StimulusChannelAssignedElectrodIndex,
        addr_MapProgramData_ReferenceChannelAssignedElectrodIndex, addr_MapProgramData_CIS_FreqBandOrder,
        addr_MapProgramData_T_Level_uA, addr_MapProgramData_C_Level_uA, addr_MapProgramData_xMinLevel,
        addr_MapProgramData_xMaxLevel, g_mapping_stimulus_amplitude_T_level,
        g_mapping_stimulus_amplitude_C_level, g_max_limit_stimulus_amplitude_C_level,
        g_logaritmMapping_coeff_A_Q16_8, g_logaritmMapping_coeff_B_Q16_8, g_pcm_amplitude_level};
    int _XMEM *scalar[20] = {&addr_MapProgramData_stimulationStrategy, &addr_MapProgramData_firstPulsePhase,
        &addr_MapProgramData_StimulationMode, &addr_MapProgramData_StimulationPulsePhaseWidth,
        &addr_MapProgramData_FrequencyAnalysisBandNumbers, &addr_MapProgramData_indicatorStimulCannel_index,
        &addr_MapProgramData_indicatorStimulLevel_uA, &m_previous_audio_volume,
        &m_previous_stimulation_volume, &m_audio_volume, &m_current_isd, &m_previous_isd,
        &m_prev_agc_10db_gain_Q8_16, &m_link_connection_check_counter,
        &g_pcm_stimulation_packet_header, &g_pcmFrameNum_per_channel,
        &g_transferableChannelNum_per_1msec, &addr_transferred_index,
        &g_NofM_LastStimulus_BandIndex, &g_NofM_Phase};
    for (int i = 0; i < 32; i++)
    {
        for (int a = 0; a < 13; a++)
            if (restore) arrays[a][i] = s_tdc_ut_saved_arrays[a][i];
            else s_tdc_ut_saved_arrays[a][i] = arrays[a][i];
    }
    for (int i = 0; i < 20; i++)
        if (restore) *scalar[i] = s_tdc_ut_saved_scalar[i];
        else s_tdc_ut_saved_scalar[i] = *scalar[i];
    for (int i = 0; i < 16; i++)
    {
        for (int m = 0; m < 2; m++) for (int b = 0; b < LIB_AUDIO_IN_BUF_MAX_CNT; b++)
            if (restore) g_lib_dmic_in_buffers[m][b][i] = s_tdc_ut_saved_dmic[m][b][i];
            else s_tdc_ut_saved_dmic[m][b][i] = g_lib_dmic_in_buffers[m][b][i];
        for (int b = 0; b < I2S_BUFFER_FULL_READY_CNT; b++)
            if (restore) lib_g_i2s_buffers[b][i] = s_tdc_ut_saved_i2s[b][i];
            else s_tdc_ut_saved_i2s[b][i] = lib_g_i2s_buffers[b][i];
        if (restore) { m_agc_output_buffer[i] = s_tdc_ut_saved_agc[i]; lib_g_i2s_buffer_output[i] = s_tdc_ut_saved_i2s_output[i]; }
        else { s_tdc_ut_saved_agc[i] = m_agc_output_buffer[i]; s_tdc_ut_saved_i2s_output[i] = lib_g_i2s_buffer_output[i]; }
    }
    for (int i = 0; i < 256; i++)
        if (restore) g_pass_bin_index[i] = s_tdc_ut_saved_bins[i];
        else s_tdc_ut_saved_bins[i] = g_pass_bin_index[i];
    for (int i = 0; i < 6; i++)
        if (restore) addr_MapProgramData_mappingDate[i] = s_tdc_ut_saved_date[i];
        else s_tdc_ut_saved_date[i] = addr_MapProgramData_mappingDate[i];
    if (restore)
    {
        Addr_SharedMem->currentMapData = s_tdc_ut_saved_map;
        Addr_SharedMem->userSettingValue = s_tdc_ut_saved_user;
        Addr_SharedMem->cfx_PCM_interface = s_tdc_ut_saved_pcm;
        Addr_SharedMem->mapChangeFlag = s_tdc_ut_saved_flags;
        Addr_SharedMem->connected_ISD_num = s_tdc_ut_saved_scalar[20];
        Addr_SharedMem->userSettingValueLoadedFlag = s_tdc_ut_saved_scalar[21];
        Addr_SharedMem->isMappingProgramConneted = s_tdc_ut_saved_scalar[22];
        Addr_SharedMem->is_enabled_mute_stimulation_under_t_level = s_tdc_ut_saved_scalar[23];
        Addr_SharedMem->is_pcm_specific_command_reading = s_tdc_ut_saved_scalar[24];
        Addr_SharedMem->pcm_specific_command_read_index = s_tdc_ut_saved_scalar[25];
        lib_g_i2s_interrupt_flag = s_tdc_ut_saved_scalar[26];
        lib_g_i2s_buffer_copy_done = s_tdc_ut_saved_scalar[27];
    }
    else
    {
        s_tdc_ut_saved_map = Addr_SharedMem->currentMapData;
        s_tdc_ut_saved_user = Addr_SharedMem->userSettingValue;
        s_tdc_ut_saved_pcm = Addr_SharedMem->cfx_PCM_interface;
        s_tdc_ut_saved_flags = Addr_SharedMem->mapChangeFlag;
        s_tdc_ut_saved_scalar[20] = Addr_SharedMem->connected_ISD_num;
        s_tdc_ut_saved_scalar[21] = Addr_SharedMem->userSettingValueLoadedFlag;
        s_tdc_ut_saved_scalar[22] = Addr_SharedMem->isMappingProgramConneted;
        s_tdc_ut_saved_scalar[23] = Addr_SharedMem->is_enabled_mute_stimulation_under_t_level;
        s_tdc_ut_saved_scalar[24] = Addr_SharedMem->is_pcm_specific_command_reading;
        s_tdc_ut_saved_scalar[25] = Addr_SharedMem->pcm_specific_command_read_index;
        s_tdc_ut_saved_scalar[26] = lib_g_i2s_interrupt_flag;
        s_tdc_ut_saved_scalar[27] = lib_g_i2s_buffer_copy_done;
    }
}

static void map_copy_vector(int without_date)
{
    int _IOMEM *date = (int _IOMEM *) &Addr_SharedMem->currentMapData.mappingDate;
    int _XMEM *dest[7] = {addr_MapProgramData_StimulusChannelAssignedElectrodIndex,
        addr_MapProgramData_ReferenceChannelAssignedElectrodIndex, addr_MapProgramData_CIS_FreqBandOrder,
        addr_MapProgramData_T_Level_uA, addr_MapProgramData_C_Level_uA,
        addr_MapProgramData_xMinLevel, addr_MapProgramData_xMaxLevel};
    for (int i = 0; i < 6; i++) { date[i] = 10+i; addr_MapProgramData_mappingDate[i] = 70+i; }
    Addr_SharedMem->currentMapData.stimulationStrategy = 1;
    Addr_SharedMem->currentMapData.firstPulsePhase = 1;
    Addr_SharedMem->currentMapData.stimulationMode = en__bipolar;
    Addr_SharedMem->currentMapData.stimulationPulsePhaseWidth = 40;
    Addr_SharedMem->currentMapData.numFrequencyBand = 32;
    Addr_SharedMem->currentMapData.stimulationIndicatorChannelNum = 12;
    Addr_SharedMem->currentMapData.stimulationIndicatorAmplitude_uA = 90;
    for (int i = 0; i < 32; i++)
    {
        Addr_SharedMem->currentMapData.usableStimulationElectrodIndex[i] = i+1;
        Addr_SharedMem->currentMapData.usableReferenceElectrodIndex[i] = 32-i;
        Addr_SharedMem->currentMapData.CIS_FreqBandOrder[i] = i+1;
        Addr_SharedMem->currentMapData.T_level_uA[i] = 50+i;
        Addr_SharedMem->currentMapData.C_level_uA[i] = 150+i;
        Addr_SharedMem->currentMapData.audio_input_x_mim[i] = 300+i;
        Addr_SharedMem->currentMapData.audio_input_x_max[i] = 600+i;
    }
    if (without_date) copy_MappingData_without_mappingDate(); else copy_MappingData();
    mark(12, without_date+1, 237);
    for (int i = 0; i < 6; i++) put(addr_MapProgramData_mappingDate[i], (without_date ? 70 : 10)+i);
    put(addr_MapProgramData_stimulationStrategy,1); put(addr_MapProgramData_firstPulsePhase,1);
    put(addr_MapProgramData_StimulationMode,en__bipolar); put(addr_MapProgramData_StimulationPulsePhaseWidth,40);
    put(addr_MapProgramData_FrequencyAnalysisBandNumbers,32); put(addr_MapProgramData_indicatorStimulCannel_index,12);
    put(addr_MapProgramData_indicatorStimulLevel_uA,90);
    for (int a = 0; a < 7; a++) for (int i = 0; i < 32; i++)
    {
        int expected = a == 0 || a == 2 ? i+1 : a == 1 ? 32-i : a == 3 ? 50+i : a == 4 ? 150+i : a == 5 ? 300+i : 600+i;
        put(dest[a][i], expected);
    }
}

int tdc_ut_cfx_io_release(void)
{
    if (g_tdc_ut_io_override_enabled || g_tdc_ut_hear_pending) return TDC_UT_IO_BUSY;
    g_tdc_ut_cfx_io_done = 0;
    return 0;
}

int tdc_ut_cfx_io_run_unit(int unit)
{
    /* 소량의 포인터/스칼라만 스택에 둔다. 백업/결과/입력은 정적이다. */
    int saved_i2s[6] = {lib_g_i2s_buffer_in_pos, lib_g_i2s_buffer_out_pos,
        lib_g_i2s_buffer_copy_cnt, lib_g_i2s_buffer_state, lib_g_i2s_buffer_sub_state, tdc_ut_i2s_raw_state()};
    static int _XMEM saved_packet[32], saved_order[16];
    static const int order[16] = {0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15};
    static const int fade_in_first[16] = {0,1023,2047,3071,4095,5119,6143,7167,8191,9215,10239,11263,12287,13311,14335,15359};
    static const int fade_in_last[16] = {49151,50175,51199,52223,53247,54271,55295,56319,57343,58367,59391,60415,61439,62463,63487,65536};
    static const int fade_out_first[16] = {65536,63487,62463,61439,60415,59391,58367,57343,56319,55295,54271,53247,52223,51199,50175,49151};
    static const int fade_out_last[16] = {15359,14335,13311,12287,11263,10239,9215,8191,7167,6143,5119,4095,3071,2047,1023,0};
    if (g_tdc_ut_cfx_vector_done || g_tdc_ut_cfx_io_done || g_tdc_ut_hear_pending || g_tdc_ut_hear_done || g_tdc_ut_io_override_enabled || g_tdc_ut_power_override_enabled || g_tdc_ut_agc_override_enabled)
        return TDC_UT_IO_BUSY;
    switch (unit)
    {
        case 7: case 10: case 11: case 12: case 15: case 19: case 26: case 27:
        case 33: case 36: case 37: case 40: break;
        default: return TDC_UT_IO_UNSUPPORTED;
    }
    backup(0);
    if (unit == 11)
        for (int i=0; i<224; i++) s_tdc_ut_saved_uart[i]=FS_MEM_UART->buffer[i];
    for (int i=0; i<32; i++)
    { saved_packet[i] = addr_stimulationTempBuff[i]; s_tdc_ut_saved_electrode_map[i]=addr_electrodeMap[i]; }
    for (int i=0; i<16; i++) saved_order[i] = g_NofM_notAdjacent_BandIndex[i];
    g_tdc_ut_cfx_io_count = g_tdc_ut_cfx_io_failed = g_tdc_ut_cfx_io_overflow = 0;
    g_tdc_ut_io_override_enabled = 1;
    reset_trace();
    switch (unit)
    {
        case 7:
            for (int v=0; v<2; v++)
            {
                for (int m=0; m<2; m++) for (int i=0; i<16; i++) g_tdc_ut_io_dmic_fifo[m][i] = 100*v+1000*m+i;
                tdc_copy_DMIC_buffers();
            }
            mark(unit,1,64);
            for (int m=0; m<2; m++) for (int b=0; b<2; b++) for (int i=0; i<16; i++)
                put(g_lib_dmic_in_buffers[m][b][i], (b == 0 ? 100 : 0)+1000*m+i);
            break;
        case 10:
            Addr_SharedMem->mapChangeFlag.cm3Command_mapChange = 0;
            Addr_SharedMem->mapChangeFlag.cm3_audioParameterCalculationDone_Flag = 0;
            Addr_SharedMem->userSettingValue.stimulVolume = m_previous_stimulation_volume;
            Addr_SharedMem->userSettingValue.audioVolume = m_previous_audio_volume;
            Addr_SharedMem->isMappingProgramConneted = 0;
            m_previous_isd = 0; Addr_SharedMem->connected_ISD_num = 1;
            LB_Normal_PowerMode(); mark(unit,1,3);
            put(m_current_isd,1); put(m_previous_isd,1); put(Addr_SharedMem->userSettingValueLoadedFlag,1);
            Addr_SharedMem->connected_ISD_num = 0; m_prev_agc_10db_gain_Q8_16 = 999;
            LB_Normal_PowerMode(); mark(unit,2,5);
            put(m_current_isd,0); put(m_previous_isd,0); put(Addr_SharedMem->userSettingValueLoadedFlag,0);
            put(m_prev_agc_10db_gain_Q8_16,0); put(m_link_connection_check_counter,-df_connectionCheckPeriod_ms);
            break;
        case 11:
            /* 실제 이벤트 함수가 Cnew/계수/이전 볼륨을 갱신한다.
             * xmin=0은 실제 0 경로, xmax=2만 power 고정으로 근사를 제외한다. */
            for (int i=0; i<32; i++)
            { g_mapping_stimulus_amplitude_T_level[i]=50; g_mapping_stimulus_amplitude_C_level[i]=150;
              addr_MapProgramData_xMinLevel[i]=0; addr_MapProgramData_xMaxLevel[i]=2; }
            g_tdc_ut_power_override_enabled = 1;
            /* 끝점 동시 변경, 오디오만 변경, 자극 볼륨만 변경, 변경 없음. */
            for (int v=0; v<5; v++)
            {
                int stim = v == 0 || v == 3 || v == 4 ? 1 : 4;
                int audio = v == 0 ? 1 : 10;
                if (v < 2) { m_previous_stimulation_volume = -1; m_previous_audio_volume = -1; }
                Addr_SharedMem->userSettingValue.stimulVolume = stim;
                Addr_SharedMem->userSettingValue.audioVolume = audio;
                if (v == 2) /* 이전 4/10에서 오디오만 1로 바꾼다. */
                { audio=1; Addr_SharedMem->userSettingValue.audioVolume=1; }
                if (v == 3) /* 이전 4/1에서 자극만 1로 바꾼다. */
                { audio=1; Addr_SharedMem->userSettingValue.audioVolume=1; }
                if (v == 4) { audio=1; Addr_SharedMem->userSettingValue.audioVolume=1; }
                reset_trace();
                Normal_PowerMode_event_both_stimulation_audio_volumeChange();
                mark(unit,v+1,132);
                for (int i=0; i<32; i++) put(g_max_limit_stimulus_amplitude_C_level[i], stim==1 ? 119 : 150);
                for (int i=0; i<32; i++) put(g_mapping_stimulus_amplitude_T_level[i],50);
                for (int i=0; i<32; i++) put(g_logaritmMapping_coeff_A_Q16_8[i],stim==1 ? 8832 : 12800);
                for (int i=0; i<32; i++) put(g_logaritmMapping_coeff_B_Q16_8[i],12800);
                put(m_previous_stimulation_volume,stim); put(m_previous_audio_volume,audio);
                put(m_audio_volume,audio-1); put(s_tdc_ut_pcm_writes,v == 4 ? 0 : 24);
            }
            /* 비양수 요청은 기존 정상 값/계수를 보존하고 PCM에도 쓰지 않는다.
             * 양의 범위 초과(자극 5/오디오 11)는 제품의 배열 검사가 없어 호출하지 않는다. */
            for (int v=0; v<4; v++)
            {
                int unchanged=1;
                Addr_SharedMem->userSettingValue.stimulVolume=v<2 ? -v : 1;
                Addr_SharedMem->userSettingValue.audioVolume=v<2 ? 1 : 2-v;
                reset_trace(); Normal_PowerMode_event_both_stimulation_audio_volumeChange();
                for (int i=0; i<32; i++)
                    if (g_max_limit_stimulus_amplitude_C_level[i]!=119 ||
                        g_mapping_stimulus_amplitude_T_level[i]!=50 ||
                        g_logaritmMapping_coeff_A_Q16_8[i]!=8832 ||
                        g_logaritmMapping_coeff_B_Q16_8[i]!=12800) unchanged=0;
                mark(unit,20+v,6);
                put(unchanged,1); put(m_previous_stimulation_volume,1);
                put(m_previous_audio_volume,1); put(m_audio_volume,0);
                put(s_tdc_ut_pcm_writes,0); put(g_tdc_ut_cfx_io_overflow,0);
            }
            /* 거부 후 유효한 중간 단계 요청으로 복귀한다. Q2.22 비율 절삭 기준. */
            for (int v=0; v<2; v++)
            {
                Addr_SharedMem->userSettingValue.stimulVolume=2+v;
                Addr_SharedMem->userSettingValue.audioVolume=1;
                reset_trace(); Normal_PowerMode_event_both_stimulation_audio_volumeChange();
                mark(unit,30+v,13);
                for (int i=0; i<3; i++)
                {
                    int channel=i==0 ? 0 : i==1 ? 15 : 31;
                    put(g_max_limit_stimulus_amplitude_C_level[channel],v ? 139 : 129);
                    put(g_logaritmMapping_coeff_A_Q16_8[channel],v ? 11392 : 10112);
                    put(g_logaritmMapping_coeff_B_Q16_8[channel],12800);
                }
                put(m_previous_stimulation_volume,2+v); put(m_previous_audio_volume,1);
                put(m_audio_volume,0); put(s_tdc_ut_pcm_writes,24);
            }
            g_tdc_ut_power_override_enabled = 0;
            break;
        case 12: map_copy_vector(0); map_copy_vector(1); break;
        case 15:
            for (int i=0; i<16; i++) { s_tdc_ut_input[i]=100+i; lib_g_i2s_buffer_output[i]=55; }
            lib_g_i2s_buffer_in_pos = lib_g_i2s_buffer_copy_cnt = 0;
            lib_g_i2s_buffer_state = I2S_BUFFER_STATE_INIT;
            lib_g_i2s_interrupt_flag = 1; lib_g_i2s_buffer_copy_done = 0;
            lib_i2s_copy_data_from_fifo(s_tdc_ut_input); mark(unit,1,20);
            for (int i=0; i<16; i++) put(lib_g_i2s_buffers[0][i],100+i);
            put(lib_g_i2s_buffer_in_pos,1); put(lib_g_i2s_buffer_copy_cnt,1); put(lib_g_i2s_interrupt_flag,0);
            put(lib_i2s_is_buffer_copy_done(),1); /* 현재 구현 0이면 실제 불일치다. */
            lib_i2s_clear_buffer_copy_done(); lib_i2s_clear_data(); mark(unit,2,17);
            put(lib_i2s_is_buffer_copy_done(),0);
            for (int i=0; i<16; i++) put(lib_g_i2s_buffer_output[i],0);
            break;
        case 19:
            for (int v=0; v<5; v++)
            {
                int _XMEM *output;
                for (int b=0; b<I2S_BUFFER_FULL_READY_CNT; b++) for (int i=0; i<16; i++) lib_g_i2s_buffers[b][i]=65536;
                lib_g_i2s_buffer_out_pos=0;
                lib_g_i2s_buffer_sub_state = v==0 ? 0 : v==1 ? 3 : v==2 ? 6 : v==3 ? 9 : 10;
                output = v<2 ? I2S_get_buffer_with_fade_in_process() : I2S_get_buffer_with_fade_out_process();
                mark(unit,v+1,16);
                for (int i=0; i<16; i++) put(output[15-i],v==0 ? fade_in_first[i] : v==1 ? fade_in_last[i] : v==2 ? fade_out_first[i] : v==3 ? fade_out_last[i] : 0);
            }
            break;
        case 26:
            for (int i=0; i<16; i++) m_agc_output_buffer[i]=i;
            update_FFT_inputData(); mark(unit,1,17); put(s_tdc_ut_fft_writes,16);
            for (int i=0; i<16; i++) put(s_tdc_ut_fft_trace[i],15-i);
            break;
        case 27:
            addr_MapProgramData_FrequencyAnalysisBandNumbers=32;
            for (int i=0; i<256; i++) { g_tdc_ut_io_fs_bins[i]=i%32; g_pass_bin_index[i]=-1; }
            g_tdc_ut_io_fs_band_count=31; read_FFT_PassBin_index(); mark(unit,1,256);
            for (int i=0; i<256; i++) put(g_pass_bin_index[i],-1);
            g_tdc_ut_io_fs_band_count=32; read_FFT_PassBin_index(); mark(unit,2,256);
            for (int i=0; i<256; i++) put(g_pass_bin_index[i],i%32);
            break;
        case 33:
            addr_MapProgramData_FrequencyAnalysisBandNumbers=32;
            Addr_SharedMem->is_enabled_mute_stimulation_under_t_level=0;
            addr_MapProgramData_firstPulsePhase=0; prepare_pcmStimulationPacketHeader();
            g_pcmFrameNum_per_channel=2; g_transferableChannelNum_per_1msec=12; addr_transferred_index=0;
            for (int i=0; i<32; i++)
            { addr_MapProgramData_CIS_FreqBandOrder[i]=32-i;
              addr_MapProgramData_StimulusChannelAssignedElectrodIndex[i]=32-i;
              addr_electrodeMap[i]=(i*7)%32;
              g_pcm_amplitude_level[i]=i%4 == 0 ? 0 : i%4 == 1 ? 1 : i%4 == 2 ? 127 : 255;
              /* CIS 순서가 역순이므로 결과 위치 i의 입력 대역은 31-i. */
              int band=31-i, amp=band%4 == 0 ? 0 : band%4 == 1 ? 1 : band%4 == 2 ? 127 : 255;
              s_tdc_ut_packet_expected[i]=0x40000 | (((i*7)%32)<<10) | (amp<<2); }
            for (int v=0; v<3; v++)
            {
                reset_trace(); stimulationStrategy_CIS(); mark(unit,10+v,32);
                for (int i=0; i<32; i++) put(addr_stimulationTempBuff[i],s_tdc_ut_packet_expected[i]);
                capture_pcm(unit,v+1,(v*12)%32,12,2);
            }
            /* 채널 수가 전송 가능 수보다 작으면 각 채널을 한 번씩만 출력하고
             * 나머지는 NOP이다. sparse 분기의 누락/덮어쓰기를 기대값으로 숨기지 않는다. */
            for (int v=0; v<4; v++)
            {
                int bands=v==0 ? 1 : v==1 ? 3 : v==2 ? 11 : 12;
                int stride=v==1 ? 1 : 2;
                addr_MapProgramData_FrequencyAnalysisBandNumbers=bands;
                g_pcmFrameNum_per_channel=stride; g_transferableChannelNum_per_1msec=12;
                addr_transferred_index=0;
                for (int i=0; i<32; i++)
                { addr_MapProgramData_CIS_FreqBandOrder[i]=i+1;
                  addr_MapProgramData_StimulusChannelAssignedElectrodIndex[i]=i+1;
                  addr_electrodeMap[i]=i; g_pcm_amplitude_level[i]=i+1; }
                reset_trace(); stimulationStrategy_CIS(); mark(unit,20+v,26);
                for (int i=0; i<24; i++)
                {
                    int band=i/stride;
                    int expected=i%stride==0 && band<bands ? 0x40000 | (band<<10) | ((band+1)<<2) : 0x25555;
                    put(s_tdc_ut_pcm[47-i],expected);
                }
                put(addr_transferred_index,0); put(g_tdc_ut_cfx_io_overflow,0);
            }
            /* 기준서의 등진폭100/정순32채널 조건도 별도로 유지한다. */
            addr_MapProgramData_FrequencyAnalysisBandNumbers=32;
            g_pcmFrameNum_per_channel=2; addr_transferred_index=0;
            for (int i=0; i<32; i++) g_pcm_amplitude_level[i]=100;
            for (int v=0; v<3; v++)
            {
                reset_trace(); stimulationStrategy_CIS(); mark(unit,30+v,57);
                for (int i=0; i<32; i++) put(addr_stimulationTempBuff[i],0x40000 | (i<<10) | 400);
                for (int i=0; i<24; i++)
                {
                    int band=(v*12+i/2)%32;
                    put(s_tdc_ut_pcm[47-i],i%2 ? 0x25555 : 0x40000 | (band<<10) | 400);
                }
                put(addr_transferred_index,((v+1)*12)%32);
            }
            break;
        case 36:
            /* 가변 입력은 위상/전극/Amplitude. 고정 헤더 비트도 완성 단어로 확인한다. */
            for (int v=0; v<2; v++)
            {
                static const int electrodes[4]={1,2,17,32};
                static const int amplitudes[4]={0,1,127,255};
                addr_MapProgramData_firstPulsePhase=v; prepare_pcmStimulationPacketHeader();
                addr_MapProgramData_FrequencyAnalysisBandNumbers=4;
                Addr_SharedMem->is_enabled_mute_stimulation_under_t_level=0;
                g_pcmFrameNum_per_channel=2; g_transferableChannelNum_per_1msec=12; addr_transferred_index=0;
                for (int i=0; i<32; i++) addr_electrodeMap[i]=i;
                for (int i=0; i<4; i++)
                { addr_MapProgramData_CIS_FreqBandOrder[i]=i+1;
                  addr_MapProgramData_StimulusChannelAssignedElectrodIndex[i]=electrodes[i];
                  g_pcm_amplitude_level[i]=amplitudes[i]; }
                reset_trace(); stimulationStrategy_CIS(); mark(unit,v+1,25);
                put(g_pcm_stimulation_packet_header,v ? 0x48000 : 0x40000);
                for (int i=0; i<4; i++)
                {
                    int word=addr_stimulationTempBuff[i];
                    int expected=0x40000 | (v<<15) | ((electrodes[i]-1)<<10) | (amplitudes[i]<<2);
                    put(word,expected); put((word>>16)&15,4); put((word>>15)&1,v);
                    put((word>>10)&31,electrodes[i]-1); put((word>>2)&255,amplitudes[i]); put(word&3,0);
                }
            }
            break;
        case 37:
            for (int v=0; v<2; v++)
            {
                Addr_SharedMem->is_enabled_mute_stimulation_under_t_level=0;
                for (int i=0; i<32; i++)
                { addr_MapProgramData_StimulusChannelAssignedElectrodIndex[i]=32-i; addr_electrodeMap[i]=(i*7)%32;
                  g_pcm_amplitude_level[i]=i%4 == 0 ? 0 : i%4 == 1 ? 1 : i%4 == 2 ? 127 : 255; }
                for (int i=0; i<16; i++) g_NofM_notAdjacent_BandIndex[i]=order[i];
                g_NofM_LastStimulus_BandIndex = v ? 0 : 15;
                addr_MapProgramData_firstPulsePhase=v; prepare_pcmStimulationPacketHeader();
                for (int i=0; i<16; i++)
                { int band=v ? i==0 ? 15 : order[i-1] : order[i];
                  int amp=band%4 == 0 ? 0 : band%4 == 1 ? 1 : band%4 == 2 ? 127 : 255;
                  s_tdc_ut_packet_expected[i]=0x40000 | (v<<15) | ((((31-band)*7)%32)<<10) | (amp<<2); }
                stimulationStrategy_NofM_Packet(); mark(unit,10+v,17);
                for (int i=0; i<16; i++)
                    put(addr_stimulationTempBuff[i],s_tdc_ut_packet_expected[i]);
                put(g_NofM_LastStimulus_BandIndex,v ? 7 : 15);
                g_pcmFrameNum_per_channel=3; g_transferableChannelNum_per_1msec=8; addr_transferred_index=0;
                for (int p=0; p<2; p++) { reset_trace(); stimulationStrategy_NofM_PCM_Write(); capture_pcm(unit,v*2+p+1,p*8,8,3); }
            }
            break;
        case 40:
            for (int v=0; v<2; v++)
            {
                /* 길이 인자는 없다. 벡터1은 3데이터+21 NOP, 벡터2는 24개 서로 다른 단어.
                 * 읽기 중 플래그와 직전 인덱스, 완료 후 요청 모드 해제를 함께 확인한다. */
                Addr_SharedMem->is_pcm_specific_command_reading=0;
                Addr_SharedMem->pcm_specific_command_read_index=17;
                Addr_SharedMem->cfx_PCM_interface.PCM_mode=PcmBitStream_Mode_SepcificCommand;
                Addr_SharedMem->cfx_PCM_interface.PCM_mode_next=PcmBitStream_Mode_NopStandby;
                for (int i=0; i<24; i++)
                { int word=v ? 0x50000 | (i<<9) | (0x80+i) : i<3 ? 0x11*(i+1) : 0x25555;
                  Addr_SharedMem->cfx_PCM_interface.PCM_specificBuffer[i]=word;
                  s_tdc_ut_packet_expected[i]=word; }
                reset_trace(); fn_PcmBitStream_Mode_SepcificCommand(); mark(unit,v+1,124);
                put(s_tdc_ut_pcm_writes,24);
                for (int i=0; i<24; i++)
                { put(s_tdc_ut_pcm_trace[i][0],i); put(s_tdc_ut_pcm_trace[i][1],s_tdc_ut_packet_expected[i]);
                  put(s_tdc_ut_pcm_trace[i][2],1); put(s_tdc_ut_pcm_trace[i][3],i ? i-1 : 17); }
                put(Addr_SharedMem->is_pcm_specific_command_reading,0);
                put(Addr_SharedMem->pcm_specific_command_read_index,23);
                put(Addr_SharedMem->cfx_PCM_interface.PCM_mode,PcmBitStream_Mode_NopStandby);
                for (int i=0; i<24; i++) put(Addr_SharedMem->cfx_PCM_interface.PCM_specificBuffer[i],s_tdc_ut_packet_expected[i]);
            }
            break;
    }
    g_tdc_ut_io_override_enabled=0;
    backup(1);
    if (unit == 11)
        for (int i=0; i<224; i++) FS_MEM_UART->buffer[i]=s_tdc_ut_saved_uart[i];
    for (int i=0; i<32; i++)
    { addr_stimulationTempBuff[i]=saved_packet[i]; addr_electrodeMap[i]=s_tdc_ut_saved_electrode_map[i]; }
    for (int i=0; i<16; i++) g_NofM_notAdjacent_BandIndex[i]=saved_order[i];
    lib_g_i2s_buffer_in_pos=saved_i2s[0]; lib_g_i2s_buffer_out_pos=saved_i2s[1];
    lib_g_i2s_buffer_copy_cnt=saved_i2s[2]; lib_g_i2s_buffer_state=saved_i2s[3];
    lib_g_i2s_buffer_sub_state=saved_i2s[4]; tdc_i2s_set_streaming_state(saved_i2s[5]);
    g_tdc_ut_cfx_io_done=1;
    return g_tdc_ut_cfx_io_overflow ? TDC_UT_IO_OVERFLOW : 0;
}

/* HEAR는 CMD를 실제 장치에 요청한 후 ISR 완료를 poll로 관찰한다.
 * 호스트는 CMD를 소프트웨어로 계산하지 않고 TARGET_REQUIRED를 반환한다.
 * FFT scratch(H01/H23)는 체인 내부 작업 공간이다. 시험 후 정상 경로를 다시
 * 시작해야 하며 CMD 실행 중에는 복원하지 않는다. Hanning 창은 변경하지 않는다.
 */
int _XMEM g_tdc_ut_hear_vmag[256];
int g_tdc_ut_hear_vmag_count;
int _XMEM g_tdc_ut_hear_fft[512];
int g_tdc_ut_hear_fft_count, g_tdc_ut_hear_block_exp;
int g_tdc_ut_hear_count, g_tdc_ut_hear_failed, g_tdc_ut_hear_overflow;
volatile int g_tdc_ut_hear_done, g_tdc_ut_hear_pending;
int g_tdc_ut_hear_target_needed=1, g_tdc_ut_hear_timed_out;
static int s_tdc_ut_hear_unit, s_tdc_ut_hear_vector, s_tdc_ut_hear_polls;
static int s_tdc_ut_hear_saved_flags[2];
static int _XMEM s_tdc_ut_hear_saved_mix[16], s_tdc_ut_hear_saved_abs[16];
static int _XMEM s_tdc_ut_hear_saved_fft_input[512], s_tdc_ut_hear_saved_fft_output[512];
static int _XMEM s_tdc_ut_hear_saved_vmag[256];
static const int s_tdc_ut_sine16[16] = {0,25080,46341,60547,65536,60547,46341,25080,0,-25080,-46341,-60547,-65536,-60547,-46341,-25080};

/* 비교기는 하드웨어 실행을 대신하지 않는다. 블록 지수는 실제 완료 시 읽는다.
 * WinDFT 입력 1.23의 정수 영역으로 되돌려 비교하고, vMag의 2.22는 추가 x2.
 * SDK가 보장한 정확도 수치가 아니라 사전에 정한 시험 허용 오차다.
 * CFX long은 48비트다. raw 24비트와 지수 상한 검사 후 곱하므로 넘치지 않는다. */
static int hear_value_in_range(int raw, int expected, int exponent, int magnitude)
{
    long normalized, difference, tolerance;
    int divisor=magnitude ? TDC_UT_HEAR_VMAG_ERROR_DIVISOR : TDC_UT_HEAR_FFT_ERROR_DIVISOR;
    if (exponent < 0 || exponent > TDC_UT_HEAR_MAX_BLOCK_EXP || raw < -8388608 || raw > 8388607)
        return 0;
    if (magnitude && raw < 0) return 0;
    normalized=(long) raw * ((long) 1 << (exponent + magnitude));
    difference=normalized-(long) expected;
    if (difference < 0) difference=-difference;
    long size=(long) expected;
    if (size < 0) size=-size;
    tolerance=TDC_UT_HEAR_ERROR_FLOOR+(size+divisor-1)/divisor;
    return difference <= tolerance;
}

int tdc_ut_hear_start(int unit, int vector, int hardware_ready)
{
    if ((unit != 1 && unit != 2) || vector < 1 || vector > (unit == 1 ? 1 : TDC_UT_HEAR_REFERENCE_VECTORS))
        return TDC_UT_IO_UNSUPPORTED;
    if (g_tdc_ut_cfx_vector_done || g_tdc_ut_hear_pending || g_tdc_ut_hear_done || g_tdc_ut_cfx_io_done || g_tdc_ut_io_override_enabled)
        return TDC_UT_IO_BUSY;
    if (hardware_ready != 1) return TDC_UT_IO_TARGET_REQUIRED;
#if defined(TDC_UT_HOST)
    return TDC_UT_IO_TARGET_REQUIRED;
#else
    if (g_interrupt_flags.function_chain0 || g_interrupt_flags.function_chain1)
        return TDC_UT_IO_BUSY; /* 기존 완료 이벤트를 지우지 않는다. */
    if (unit == 2)
        for (int i=0; i<512; i++)
            if (((int _XMEM *) HEAR_ADDR_FFT_WINDOW)[i] != s_tdc_ut_hear_window_reference[0][i])
                return TDC_UT_IO_WINDOW_NOT_READY; /* 창을 임의 교체해서 PASS시키지 않는다. */
    s_tdc_ut_hear_saved_flags[0]=g_interrupt_flags.function_chain0;
    s_tdc_ut_hear_saved_flags[1]=g_interrupt_flags.function_chain1;
    if (unit == 1)
    {
        static const int input[5]={0,-32,17,-100,99};
        for (int i=0; i<16; i++)
        {
            s_tdc_ut_hear_saved_mix[i]=((int _XMEM *) HEAR_ADDR_AUDIO_MIX)[i];
            s_tdc_ut_hear_saved_abs[i]=((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS)[i];
            ((int _XMEM *) HEAR_ADDR_AUDIO_MIX)[i]=i<5 ? input[i] : 0;
        }
    }
    else
    {
        for (int i=0; i<512; i++)
        {
            s_tdc_ut_hear_saved_fft_input[i]=((int _XMEM *) HEAR_ADDR_FFT_INPUT)[i];
            s_tdc_ut_hear_saved_fft_output[i]=((int _XMEM *) HEAR_ADDR_FFT_OUTPUT)[i];
            int input=vector == 1 ? 0 : vector == 4 ? 32768 : vector == 5 ? i%2 ? -32768 : 32768 : s_tdc_ut_sine16[(i*(vector == 2 ? 1 : 2)) % 16];
            ((int _XMEM *) HEAR_ADDR_FFT_INPUT)[i]=input;
        }
        for (int i=0; i<256; i++) s_tdc_ut_hear_saved_vmag[i]=((int _XMEM *) HEAR_ADDR_VMAG_OUTPUT)[i];
    }
    s_tdc_ut_hear_unit=unit; s_tdc_ut_hear_vector=vector; s_tdc_ut_hear_polls=0;
    g_tdc_ut_hear_count=g_tdc_ut_hear_failed=g_tdc_ut_hear_overflow=0;
    g_tdc_ut_hear_vmag_count=g_tdc_ut_hear_fft_count=0; g_tdc_ut_hear_block_exp=-1;
    g_tdc_ut_cfx_io_count=g_tdc_ut_cfx_io_failed=g_tdc_ut_cfx_io_overflow=0;
    g_tdc_ut_hear_timed_out=0; g_tdc_ut_hear_pending=1;
    g_tdc_ut_hear_target_needed=1;
    if (unit == 1) { CALL_FUNCTION_CHAIN(HEAR_FC_agc_preprocessing); }
    else { CALL_FUNCTION_CHAIN(HEAR_FC_fft_vmag); }
    return 0;
#endif
}

int tdc_ut_hear_poll(void)
{
#if defined(TDC_UT_HOST)
    return TDC_UT_IO_TARGET_REQUIRED;
#else
    if (!g_tdc_ut_hear_pending) return TDC_UT_IO_UNSUPPORTED;
    if (!(s_tdc_ut_hear_unit == 1 ? g_interrupt_flags.function_chain0 : g_interrupt_flags.function_chain1))
    {
        if (s_tdc_ut_hear_polls < TDC_UT_HEAR_POLL_LIMIT) s_tdc_ut_hear_polls++;
        if (s_tdc_ut_hear_polls >= TDC_UT_HEAR_POLL_LIMIT)
        { g_tdc_ut_hear_timed_out=1; return TDC_UT_IO_TIMEOUT; }
        return 1; /* 진행 중. 호출자가 다음 poll 시점을 정한다. */
    }
    if (s_tdc_ut_hear_unit == 1)
    {
        /* vAbsolute 직후 word0=0은 분석용 중간값이다. Event0 뒤에는
         * MaxElement가 같은 word0에 최대값 100을 써 놓는다. */
        static const int expected[5]={100,32,17,100,99};
        mark(1,1,18);
        for (int i=0; i<16; i++) put(((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS)[i],i<5 ? expected[i] : 0);
        put(g_interrupt_flags.function_chain0,1); put(g_interrupt_flags.function_chain1,0);
        for (int i=0; i<16; i++)
        { ((int _XMEM *) HEAR_ADDR_AUDIO_MIX)[i]=s_tdc_ut_hear_saved_mix[i];
          ((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS)[i]=s_tdc_ut_hear_saved_abs[i]; }
    }
    else
    {
        int fft_any=0, vmag_any=0, peak_bin=0, peak_value=0;
        g_tdc_ut_hear_block_exp=(int) D_HEAR_BFP->BLOCK_EXP;
        for (int i=0; i<512; i++)
        { int value=((int _XMEM *) HEAR_ADDR_FFT_OUTPUT)[i]; g_tdc_ut_hear_fft[i]=value; fft_any |= value; }
        g_tdc_ut_hear_fft_count=512;
        for (int i=0; i<256; i++)
        {
            int value=((int _XMEM *) HEAR_ADDR_VMAG_OUTPUT)[i];
            g_tdc_ut_hear_vmag[i]=value; vmag_any |= value;
            if (value > peak_value) { peak_value=value; peak_bin=i; }
        }
        g_tdc_ut_hear_vmag_count=256;
        if (s_tdc_ut_hear_vector == 1)
        { mark(2,1,4); put(fft_any,0); put(vmag_any,0); }
        else
        {
            mark(2,s_tdc_ut_hear_vector,278);
            put(g_tdc_ut_hear_block_exp >= 0 && g_tdc_ut_hear_block_exp <= TDC_UT_HEAR_MAX_BLOCK_EXP,1);
            for (int j=0; j<TDC_UT_HEAR_FFT_REFERENCE_BINS; j++)
            {
                int k=s_tdc_ut_hear_reference_bins[j];
                for (int component=0; component<2; component++)
                    put(hear_value_in_range(g_tdc_ut_hear_fft[2*k+component],
                        s_tdc_ut_hear_fft_reference[s_tdc_ut_hear_vector-1][2*j+component],g_tdc_ut_hear_block_exp,0),1);
            }
            for (int i=0; i<256; i++)
                put(hear_value_in_range(g_tdc_ut_hear_vmag[i],
                    s_tdc_ut_hear_vmag_reference[s_tdc_ut_hear_vector-1][i],g_tdc_ut_hear_block_exp,1),1);
            put(peak_bin,s_tdc_ut_hear_vector == 2 ? 32 : s_tdc_ut_hear_vector == 3 ? 64 : 0);
        }
        put(g_interrupt_flags.function_chain0,0); put(g_interrupt_flags.function_chain1,1);
        for (int i=0; i<512; i++)
        { ((int _XMEM *) HEAR_ADDR_FFT_INPUT)[i]=s_tdc_ut_hear_saved_fft_input[i];
          ((int _XMEM *) HEAR_ADDR_FFT_OUTPUT)[i]=s_tdc_ut_hear_saved_fft_output[i]; }
        for (int i=0; i<256; i++) ((int _XMEM *) HEAR_ADDR_VMAG_OUTPUT)[i]=s_tdc_ut_hear_saved_vmag[i];
    }
    if (g_tdc_ut_hear_timed_out)
    { mark(s_tdc_ut_hear_unit,99,1); put(1,0); } /* 늦은 완료가 timeout을 PASS로 덮지 않는다. */
    g_interrupt_flags.function_chain0=s_tdc_ut_hear_saved_flags[0];
    g_interrupt_flags.function_chain1=s_tdc_ut_hear_saved_flags[1];
    g_tdc_ut_hear_count=g_tdc_ut_cfx_io_count;
    g_tdc_ut_hear_failed=g_tdc_ut_cfx_io_failed;
    g_tdc_ut_hear_overflow=g_tdc_ut_cfx_io_overflow;
    g_tdc_ut_hear_target_needed=0; g_tdc_ut_hear_pending=0; g_tdc_ut_hear_done=1;
    return g_tdc_ut_hear_overflow ? TDC_UT_IO_OVERFLOW : 0;
#endif
}

int tdc_ut_hear_release(void)
{
    if (g_tdc_ut_hear_pending) return TDC_UT_IO_BUSY;
    g_tdc_ut_hear_done=0;
    return 0;
}
#endif
