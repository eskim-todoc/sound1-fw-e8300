/* TD24A vectors. Explicit debugger call only; never from an ISR/normal loop.
 * Stop audio, HEAR and PCM producers/consumers first. Restart the signal path
 * after testing. Static backups avoid a large CFX stack and restore touched
 * state. Each call stores one unit, so RTT can drain before the next unit.
 */
#if defined(TDC_UNIT_TEST)
#include <audioMixer.h>
#include <lib_audio_in.h>
#include <lib_i2s.h>
#include <lib_audio_mixer.h>
#include <agc.h>
#include <FrequencyAnalysis.h>
#include <nonlinearMapping.h>
#include <stimulationStrategy.h>
#include "tdc_ut_hooks.h"
#include "tdc_ut_vectors_cfx.h"
#include "tdc_ut_io_cfx.h"
#include "tdc_ut_numeric_reference_cfx.h"

int _XMEM g_tdc_ut_cfx_vector_actual[TDC_UT_CFX_VECTOR_CAPACITY];
int _XMEM g_tdc_ut_cfx_vector_expected[TDC_UT_CFX_VECTOR_CAPACITY];
int g_tdc_ut_cfx_vector_count, g_tdc_ut_cfx_vector_failed;
int g_tdc_ut_cfx_vector_overflow;
volatile int g_tdc_ut_cfx_vector_done;
int _XMEM g_tdc_ut_cfx_numeric_actual[TDC_UT_CFX_NUMERIC_CAPACITY];
int g_tdc_ut_cfx_numeric_count;
int g_tdc_ut_agc_override_enabled, g_tdc_ut_agc_input_db;
int g_tdc_ut_agc_goal_db, g_tdc_ut_agc_clamped_input;
int g_tdc_ut_power_override_enabled;
static int _XMEM s_tdc_ut_in[3][16];
static int _XMEM s_tdc_ut_saved[11][32];
static int _XMEM s_tdc_ut_saved_bins[256];
static int _XMEM s_tdc_ut_saved_vmag[256];
static int _XMEM s_tdc_ut_saved_mix[16], s_tdc_ut_saved_agc[16];
static int _XMEM s_tdc_ut_saved_adj[16], s_tdc_ut_saved_inter[16];
static int _XMEM s_tdc_ut_saved_output[32];

static void put(int actual, int expected)
{
    int idx = g_tdc_ut_cfx_vector_count;
    if (idx >= TDC_UT_CFX_VECTOR_CAPACITY)
    { g_tdc_ut_cfx_vector_overflow = 1; return; }
    g_tdc_ut_cfx_vector_actual[idx] = actual;
    g_tdc_ut_cfx_vector_expected[idx] = expected;
    g_tdc_ut_cfx_vector_count++;
    if (actual != expected) g_tdc_ut_cfx_vector_failed++;
}
static void mark(int unit, int vector, int count)
{ put(-999, -999); put(unit, unit); put(vector, vector); put(count, count); }
static void capture_numeric(int value)
{
    int idx = g_tdc_ut_cfx_numeric_count;
    if (idx >= TDC_UT_CFX_NUMERIC_CAPACITY)
    { g_tdc_ut_cfx_vector_overflow = 1; return; }
    g_tdc_ut_cfx_numeric_actual[idx] = value;
    g_tdc_ut_cfx_numeric_count++;
}
static void mark_numeric(int unit, int vector, int count)
{
    mark(unit, vector, count);
    capture_numeric(-999); capture_numeric(unit);
    capture_numeric(vector); capture_numeric(count * 3);
}
/* 기대값은 독립 수식/SDK 정밀도에서 미리 산출했다. 범위 판정만 표준
 * 비교기로 전달하고 계산값과 상하한은 별도 원본 캡처에 그대로 남긴다.
 */
static void put_numeric(int actual, int lower, int upper)
{
    capture_numeric(actual); capture_numeric(lower); capture_numeric(upper);
    put(lower <= upper && lower <= actual && actual <= upper, 1);
}
static void read_mix(int unit, int vector, int expected)
{
    int _XMEM *mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;
    mark(unit, vector, 16);
    for (int i = 0; i < 16; i++) put(mix[i], expected);
}

/* Returns -1 if not a supported direct-call vector, 0 after an execution.
 * done is not PASS. failed and overflow must also be checked. Other units
 * have separate hardware observations or analysis entries in the manifest.
 */
int tdc_ut_cfx_run_unit(int unit)
{
    int _XMEM *mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;
    int _XMEM *vmag = (int _XMEM *) HEAR_ADDR_VMAG_OUTPUT;
    int *arrays[11] = {g_pcm_amplitude_level, g_mapping_stimulus_amplitude_T_level,
        g_mapping_stimulus_amplitude_C_level, g_max_limit_stimulus_amplitude_C_level,
        g_logaritmMapping_coeff_A_Q16_8, g_logaritmMapping_coeff_B_Q16_8,
        g_freq_rep_values, g_freq_rep_values_scaled, addr_MapProgramData_xMinLevel,
        addr_MapProgramData_xMaxLevel, addr_stimulationTempBuff};
    int saved_prev = m_prev_agc_10db_gain_Q8_16, saved_volume = m_audio_volume;
    int saved_front = tdc_audio_get_front_mic(), saved_stream = tdc_ut_i2s_raw_state();
    int saved_coeff[4] = {m_attack_coeff_Q8_16, m_release_coeff_Q8_16,
        m_one_minus_attack_coeff_Q8_16, m_one_minus_release_coeff_Q8_16};
    int saved_mute = Addr_SharedMem->is_enabled_mute_stimulation_under_t_level;
    int saved_offset = Addr_SharedMem->mute_stimulation_t_level_offset;
    int saved_alert = Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3;
    int saved_max = Addr_SharedMem->maxAudioInput;
    int saved_goal = g_tdc_ut_agc_goal_db, saved_clamp = g_tdc_ut_agc_clamped_input;
    int saved_db = g_tdc_ut_agc_input_db;
    int saved_hear_max = *((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE);
    static const int inter[16] = {0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15};
    static const int boundary[6] = {-7700297,-7700296,-7700295,-6209353,-6209352,-6209351};
    static const int expected_gain[2][6] = {{-16,0,0,0,0,51},{973232,973249,973249,973249,973249,973249}};
    static const int volumes[4] = {119,129,139,150};
    /* Do not mask a forgotten override from a previous manual test. */
    if (g_tdc_ut_cfx_vector_done || g_tdc_ut_hear_pending ||
        g_tdc_ut_hear_done || g_tdc_ut_cfx_io_done) return -2;
    if (g_tdc_ut_agc_override_enabled || g_tdc_ut_power_override_enabled) return -1;
    switch (unit)
    {
        case 8: case 13: case 16: case 17: case 18: case 20: case 22:
        case 23: case 24: case 25: case 28: case 29: case 30: case 31:
        case 32: case 34: case 35: break;
        default: return -1;
    }
    g_tdc_ut_cfx_vector_done = 0;
    g_tdc_ut_cfx_vector_count = g_tdc_ut_cfx_vector_failed = g_tdc_ut_cfx_vector_overflow = 0;
    g_tdc_ut_cfx_numeric_count = 0;
    for (int i = 0; i < 32; i++)
    {
        for (int a = 0; a < 11; a++) s_tdc_ut_saved[a][i] = arrays[a][i];
        s_tdc_ut_saved_output[i] = Addr_SharedMem->currentOutputStimulLevel_255[i];
    }
    for (int i = 0; i < 16; i++)
    {
        s_tdc_ut_saved_mix[i] = mix[i]; s_tdc_ut_saved_agc[i] = m_agc_output_buffer[i];
        s_tdc_ut_saved_adj[i] = g_NofM_adjacent_BandIndex[i];
        s_tdc_ut_saved_inter[i] = g_NofM_notAdjacent_BandIndex[i];
        s_tdc_ut_in[0][i] = 320; s_tdc_ut_in[1][i] = -64; s_tdc_ut_in[2][i] = 0;
    }
    switch (unit)
    {
        case 8:
            mark(unit, 1, 3);
            for (int i = 0; i < 3; i++)
            { tdc_i2s_set_streaming_state(i == 1 ? LIB_I2S_STATE_ENABLED : LIB_I2S_STATE_DISABLED); put(I2S_isStreaming(), i == 1); }
            break;
        case 13:
            mark(unit, 1, 2);
            tdc_audio_set_front_mic(LIB_AUDIO_FRONT_MIC_LEFT); put(tdc_audio_get_front_mic(), LIB_AUDIO_FRONT_MIC_LEFT);
            tdc_audio_set_front_mic(LIB_AUDIO_FRONT_MIC_RIGHT); put(tdc_audio_get_front_mic(), LIB_AUDIO_FRONT_MIC_RIGHT);
            break;
        case 16:
            tdc_audio_mix_1_buffer(s_tdc_ut_in[0]); read_mix(unit, 1, 10);
            tdc_audio_mix_2_buffers(s_tdc_ut_in[0], s_tdc_ut_in[1]); read_mix(unit, 2, 8);
            break;
        case 17:
            for (int i = 0; i < 16; i++) s_tdc_ut_in[0][i] = 64 * i;
            s_tdc_ut_in[1][0] = 2048;
            tdc_audio_mix_2_buffers_for_beamforming(s_tdc_ut_in[0], s_tdc_ut_in[1], s_tdc_ut_in[2]);
            mark(unit, 1, 16);
            for (int i = 0; i < 16; i++) put(mix[i], i == 15 ? 32 : i + 1);
            break;
        case 18:
            tdc_audio_mix_2_buffers_with_gain(s_tdc_ut_in[0], s_tdc_ut_in[0], 65536, 65536);
            read_mix(unit, 1, 20);
            break;
        case 20:
            for (int i = 0; i < 16; i++) s_tdc_ut_in[0][i] = i;
            lib_loopback_AGC_out(s_tdc_ut_in[2], s_tdc_ut_in[0]);
            mark(unit, 1, 32);
            for (int i = 0; i < 16; i++) put(s_tdc_ut_in[2][i], i * 64);
            for (int i = 0; i < 16; i++) put(s_tdc_ut_in[0][i], i);
            { const int in[5] = {-131073,-131072,0,131071,131072};
              const int out[5] = {-8388608,-8388608,0,8388544,8388607};
              for (int i = 0; i < 16; i++) s_tdc_ut_in[0][i] = i < 5 ? in[i] : 0;
              lib_loopback_AGC_out(s_tdc_ut_in[2], s_tdc_ut_in[0]);
              mark(unit, 2, 16);
              for (int i = 0; i < 16; i++) put(s_tdc_ut_in[2][i], i < 5 ? out[i] : 0);
            }
            break;
        case 22:
            for (int i = 0; i < 2; i++)
            {
                *((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE) = i ? 100 : 0;
                audio_agc(); mark(unit, i + 1, 2);
                put(Addr_SharedMem->maxAudioInput, i ? 100 : 0);
                put(g_tdc_ut_agc_clamped_input, i ? 100 : 1);
            }
            break;
        case 23:
            g_tdc_ut_agc_override_enabled = 1;
            for (int v = 0; v < 2; v++)
            {
                mark(unit, v + 1, 6); m_audio_volume = v ? 9 : 0;
                for (int i = 0; i < 6; i++)
                {
                    m_prev_agc_10db_gain_Q8_16 = 0; g_tdc_ut_agc_input_db = boundary[i];
                    (void) calculate_agc_gain(1); put(g_tdc_ut_agc_goal_db, expected_gain[v][i]);
                }
            }
            g_tdc_ut_agc_override_enabled = 0;
            /* C10~C21: 입력부터 SDK log/exp와 attack/release를 전부 실행.
             * 기존 C1/C2의 중간 dB 경계 고정과 별도로 판정한다.
             */
            m_attack_coeff_Q8_16 = 62259; m_release_coeff_Q8_16 = 655;
            m_one_minus_attack_coeff_Q8_16 = 3276; m_one_minus_release_coeff_Q8_16 = 64880;
            for (int v = 0; v < 12; v++)
            {
                const int *ref = s_tdc_ut_agc_full[v];
                int gain;
                m_audio_volume = ref[1]; m_prev_agc_10db_gain_Q8_16 = 0;
                gain = calculate_agc_gain(ref[0]);
                mark_numeric(unit, v + 10, 3);
                put_numeric(g_tdc_ut_agc_goal_db, ref[2], ref[3]);
                put_numeric(m_prev_agc_10db_gain_Q8_16, ref[4], ref[5]);
                put_numeric(gain, ref[6], ref[7]);
            }
            break;
        case 24:
            /* Fixed input includes the four integer time-constant weights. */
            m_attack_coeff_Q8_16 = 62259; m_release_coeff_Q8_16 = 655;
            m_one_minus_attack_coeff_Q8_16 = 3276; m_one_minus_release_coeff_Q8_16 = 64880;
            mark(unit, 1, 1); m_prev_agc_10db_gain_Q8_16 = 0; put(audio_agc_attack_release(65536), 655);
            mark(unit, 2, 1); m_prev_agc_10db_gain_Q8_16 = 65536; put(audio_agc_attack_release(0), 3276);
            mark(unit, 3, 1); put(audio_agc_attack_release(0), 163);
            break;
        case 25:
            { const int in[5] = {-10,0,10,8388607,-8388608};
              const int twice[5] = {-20,0,20,8388607,-8388608};
              for (int v = 0; v < 2; v++)
              {
                  for (int i = 0; i < 16; i++) mix[i] = i < 5 ? in[i] : 0;
                  apply_agc_gain(v ? 8192 : 4096); mark(unit, v + 1, 16);
                  for (int i = 0; i < 16; i++) put(m_agc_output_buffer[i], i < 5 ? (v ? twice[i] : in[i]) : 0);
              }
            }
            break;
        case 28:
            for (int i = 0; i < 256; i++)
            {
                s_tdc_ut_saved_bins[i] = g_pass_bin_index[i]; s_tdc_ut_saved_vmag[i] = vmag[i];
                g_pass_bin_index[i] = -1; vmag[i] = 0;
            }
            g_pass_bin_index[0] = g_pass_bin_index[1] = 0; g_pass_bin_index[2] = 1;
            vmag[0] = 16; vmag[1] = 80; vmag[2] = 48; vmag[3] = 4096;
            find_freq_rep_value(); mark(unit, 1, 64);
            for (int i = 0; i < 32; i++) put(g_freq_rep_values[i], i == 0 ? 80 : i == 1 ? 48 : 0);
            for (int i = 0; i < 32; i++) put(g_freq_rep_values_scaled[i], i == 0 ? 5 : i == 1 ? 3 : 0);
            for (int i = 0; i < 256; i++) { g_pass_bin_index[i] = s_tdc_ut_saved_bins[i]; vmag[i] = s_tdc_ut_saved_vmag[i]; }
            break;
        case 29:
            for (int i = 0; i < 32; i++)
            {
                s_tdc_ut_saved_bins[i] = Addr_SharedMem->calculatedStimulPara_byCM3.T_level_255[i];
                s_tdc_ut_saved_vmag[i] = Addr_SharedMem->calculatedStimulPara_byCM3.C_level_255[i];
                Addr_SharedMem->calculatedStimulPara_byCM3.T_level_255[i] = i;
                Addr_SharedMem->calculatedStimulPara_byCM3.C_level_255[i] = 200 + i;
            }
            read_C_T_Level_fromCM3(); mark(unit, 1, 64);
            for (int i = 0; i < 32; i++) put(g_mapping_stimulus_amplitude_T_level[i], i);
            for (int i = 0; i < 32; i++) put(g_mapping_stimulus_amplitude_C_level[i], 200 + i);
            for (int i = 0; i < 32; i++)
            {
                Addr_SharedMem->calculatedStimulPara_byCM3.T_level_255[i] = s_tdc_ut_saved_bins[i];
                Addr_SharedMem->calculatedStimulPara_byCM3.C_level_255[i] = s_tdc_ut_saved_vmag[i];
            }
            break;
        case 30:
            for (int i = 0; i < 32; i++) { g_mapping_stimulus_amplitude_T_level[i] = 50; g_mapping_stimulus_amplitude_C_level[i] = 150; }
            for (int v = 1; v <= 4; v++)
            {
                calculate_newStimulation_maxLevel(v); mark(unit, v, 64);
                for (int i = 0; i < 32; i++) put(g_max_limit_stimulus_amplitude_C_level[i], volumes[v - 1]);
                for (int i = 0; i < 32; i++) put(g_mapping_stimulus_amplitude_T_level[i], 50);
            }
            break;
        case 31:
            g_tdc_ut_power_override_enabled = 1;
            for (int i = 0; i < 32; i++)
            { g_mapping_stimulus_amplitude_T_level[i] = 50; g_max_limit_stimulus_amplitude_C_level[i] = 150;
              addr_MapProgramData_xMinLevel[i] = 1; addr_MapProgramData_xMaxLevel[i] = 2; }
            calculate_logaritmMapping_coeff(); mark(unit, 1, 64);
            for (int i = 0; i < 32; i++) put(g_logaritmMapping_coeff_A_Q16_8[i], 25600);
            for (int i = 0; i < 32; i++) put(g_logaritmMapping_coeff_B_Q16_8[i], -12800);
            g_tdc_ut_power_override_enabled = 0;
            /* C10: 실제 x^p와 SDK division을 포함한 32채널 A/B 계수.
             * 채널마다 T/C를 구분하므로 한 값을 잘못 복제해도 검출한다.
             */
            for (int i = 0; i < 32; i++)
            {
                g_mapping_stimulus_amplitude_T_level[i] = 50 + i % 4;
                g_max_limit_stimulus_amplitude_C_level[i] = 150 + i % 4;
                addr_MapProgramData_xMinLevel[i] = 16; addr_MapProgramData_xMaxLevel[i] = 4096;
            }
            calculate_logaritmMapping_coeff(); mark_numeric(unit, 10, 64);
            for (int i = 0; i < 32; i++)
            {
                const int *ref = s_tdc_ut_coeff_full[i % 4];
                put_numeric(g_logaritmMapping_coeff_A_Q16_8[i], ref[0], ref[1]);
                put_numeric(g_logaritmMapping_coeff_B_Q16_8[i], ref[2], ref[3]);
            }
            break;
        case 32:
            /* A=0 fixes pre-limit y without replacing the limit logic. */
            Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 = 0;
            Addr_SharedMem->mute_stimulation_t_level_offset = 0;
            for (int mode = 1; mode <= 2; mode++)
            {
                const int y[4] = {40,50,100,160};
                const int out[2][4] = {{0,50,100,150},{50,50,100,150}};
                Addr_SharedMem->is_enabled_mute_stimulation_under_t_level = mode;
                for (int v = 0; v < 4; v++)
                {
                    for (int i = 0; i < 32; i++)
                    { g_logaritmMapping_coeff_A_Q16_8[i] = 0; g_logaritmMapping_coeff_B_Q16_8[i] = y[v] * 256;
                      g_mapping_stimulus_amplitude_T_level[i] = 50; g_max_limit_stimulus_amplitude_C_level[i] = 150; }
                    logarithmMapping(); mark(unit, (mode - 1) * 4 + v + 1, 32);
                    for (int i = 0; i < 32; i++) put(g_pcm_amplitude_level[i], out[mode - 1][v]);
                }
            }
            /* C10/C11: 0/정상/큰 입력을 실제 SDK x^p로 계산하고 앞에서
             * 실제 계산한 A/B를 사용한다. T/C 경계 분기는 C1~C8에서
             * 이미 분리한다. 소수 근사 오차는 사전 구간으로만 판정한다.
             */
            for (int i = 0; i < 32; i++)
            {
                g_mapping_stimulus_amplitude_T_level[i] = 50;
                g_max_limit_stimulus_amplitude_C_level[i] = 150;
                addr_MapProgramData_xMinLevel[i] = 16; addr_MapProgramData_xMaxLevel[i] = 4096;
                g_freq_rep_values_scaled[i] = s_tdc_ut_mapping_full[i % 8][0];
            }
            calculate_logaritmMapping_coeff();
            for (int mode = 1; mode <= 2; mode++)
            {
                Addr_SharedMem->is_enabled_mute_stimulation_under_t_level = mode;
                logarithmMapping(); mark_numeric(unit, mode + 9, 64);
                for (int i = 0; i < 32; i++)
                {
                    const int *ref = s_tdc_ut_mapping_full[i % 8];
                    put_numeric(g_pcm_amplitude_level[i], ref[mode * 2 - 1], ref[mode * 2]);
                    put_numeric(Addr_SharedMem->currentOutputStimulLevel_255[i], ref[mode * 2 - 1], ref[mode * 2]);
                }
            }
            break;
        case 34:
            for (int v = 0; v < 2; v++)
            {
                for (int i = 0; i < 32; i++) g_pcm_amplitude_level[i] = v ? i : 100;
                stimulationStrategy_NofM_Peak_Pick(); mark(unit, v + 1, 16);
                for (int i = 0; i < 16; i++) put(g_NofM_adjacent_BandIndex[i], v ? i + 16 : i);
            }
            break;
        case 35:
            for (int i = 0; i < 16; i++) g_NofM_adjacent_BandIndex[i] = i;
            stimulationStrategy_NofM_Interleaving(); mark(unit, 1, 16);
            for (int i = 0; i < 16; i++) put(g_NofM_notAdjacent_BandIndex[i], inter[i]);
            break;
    }
    for (int i = 0; i < 32; i++)
    {
        for (int a = 0; a < 11; a++) arrays[a][i] = s_tdc_ut_saved[a][i];
        Addr_SharedMem->currentOutputStimulLevel_255[i] = s_tdc_ut_saved_output[i];
    }
    for (int i = 0; i < 16; i++)
    { mix[i] = s_tdc_ut_saved_mix[i]; m_agc_output_buffer[i] = s_tdc_ut_saved_agc[i];
      g_NofM_adjacent_BandIndex[i] = s_tdc_ut_saved_adj[i]; g_NofM_notAdjacent_BandIndex[i] = s_tdc_ut_saved_inter[i]; }
    m_prev_agc_10db_gain_Q8_16 = saved_prev; m_audio_volume = saved_volume;
    tdc_audio_set_front_mic(saved_front);
    tdc_i2s_set_streaming_state(saved_stream);
    m_attack_coeff_Q8_16 = saved_coeff[0]; m_release_coeff_Q8_16 = saved_coeff[1];
    m_one_minus_attack_coeff_Q8_16 = saved_coeff[2]; m_one_minus_release_coeff_Q8_16 = saved_coeff[3];
    Addr_SharedMem->is_enabled_mute_stimulation_under_t_level = saved_mute;
    Addr_SharedMem->mute_stimulation_t_level_offset = saved_offset;
    Addr_SharedMem->calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 = saved_alert;
    Addr_SharedMem->maxAudioInput = saved_max;
    *((int _XMEM *) HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE) = saved_hear_max;
    g_tdc_ut_agc_goal_db = saved_goal; g_tdc_ut_agc_clamped_input = saved_clamp; g_tdc_ut_agc_input_db = saved_db;
    g_tdc_ut_cfx_vector_done = 1;
    return 0;
}

/* Call only after the reporter consumed all records or explicit cancellation. */
void tdc_ut_cfx_vectors_release(void)
{ g_tdc_ut_cfx_vector_done = 0; }
#endif
