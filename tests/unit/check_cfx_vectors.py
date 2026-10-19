"""Exercise current CFX calculation vectors with the actual C function bodies.

Host adapters cover memory and DSP-library boundaries only. CFX long is widened
to host long long for these vectors. This is not a CFX ABI/SDK accuracy proof.
The source digests in the result make the tested version explicit.
"""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path
import cfx_numeric_reference

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/cfx-vectors"


def function(text, name):
    pattern = r"^(?:static\s+)?(?:int|void)\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{"
    match = re.search(pattern, text, re.M)
    if not match:
        raise ValueError("missing function: " + name)
    # These selected functions contain braces in comments. Tokenize comments
    # and literals so changes in comments cannot truncate the extracted body.
    token = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 1
    for t in token.finditer(text, match.end()):
        if t.group() == "{":
            depth += 1
        elif t.group() == "}":
            depth -= 1
            if depth == 0:
                return text[match.start():t.end()]
    raise ValueError("unclosed function: " + name)


PREFIX = r"""
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define TDC_UNIT_TEST 1
#define _XMEM
#define chess_storage(x)
#define chess_loop_range(a,b)
#define df_inputADC_DataBuffLength 16
#define df_MaxNumOfElectrode 32
#define df_MaxNum_NofM 16
#define INT24_MAX 8388607
#define INT24_MIN (-8388608)
#define HALF_FFT_SIZE 256
#define RIGHT_SHIFT_MAX_MAG_FREQ_SCALE 4
#define AUDIO_INPUT_RSHIFT 5
#define AUDIO_MIX_NORMALIZE_RSHIFT 1
#define TDC_GAIN_Q8_16_SHIFT 16
#define LIB_AUDIO_LOOPBACK_LEN 16
#define LIB_AUDIO_BIT_RSHIFT 6
#define LIB_AUDIO_FRONT_MIC_LEFT 1
#define LIB_AUDIO_FRONT_MIC_RIGHT 2
#define LIB_I2S_STATE_ENABLED 1
#define LIB_I2S_STATE_DISABLED 2
#define NORMALIZE_SHIFT_BIT_NUM_FOR_10DB_LIBRARY 5
int host_mix[16], host_vmag[256], host_max;
#define HEAR_ADDR_AUDIO_MIX host_mix
#define HEAR_ADDR_VMAG_OUTPUT host_vmag
#define HEAR_ADDR_AUDIO_MIX_ABS_MAX_VALUE (&host_max)
typedef struct {
 int T_level_255[32],C_level_255[32];
} host_tc_t;
typedef struct {
 int is_enabled_mute_stimulation_under_t_level,mute_stimulation_t_level_offset;
 int maxAudioInput,currentOutputStimulLevel_255[32];
 host_tc_t calculatedStimulPara_byCM3;
 struct {int indicatorStimulOutput_OnOff_Coltroled_byCM3,indicatorStimulLevel_255;} calculatedStimulationIndcator_byCM3;
 struct {int mapNum;} userSettingValue;
 struct {int stimulationIndicatorChannelNum;} currentMapData;
} host_shared_t;
host_shared_t shared;
#define Addr_SharedMem (&shared)
int g_mapping_stimulus_amplitude_T_level[32],g_mapping_stimulus_amplitude_C_level[32];
int addr_MapProgramData_xMinLevel[32],addr_MapProgramData_xMaxLevel[32];
int addr_MapProgramData_indicatorStimulCannel_index;
int g_pass_bin_index[256],g_freq_rep_values[32],g_freq_rep_values_scaled[32];
int s_lib_audio_front_mic,lib_g_i2s_state;
int g_tdc_ut_hear_pending,g_tdc_ut_hear_done,g_tdc_ut_cfx_io_done;
int tdc_ut_i2s_raw_state(void) {return lib_g_i2s_state;}
struct {int flag[1]; int buffer[600];} host_uart;
#define FS_MEM_UART (&host_uart)
typedef struct {long long v;} m8p16_t;
typedef struct {long long v;} frac48_t;
#define to_int(x) ((int)(x))
#define to_long(x) (x)
#define to_fix(x) (x)
#define to_lfix(x) (x)
static frac48_t cfx_as_frac48(long long x) {frac48_t f={x};return f;}
static frac48_t cfx_as_frac24(int x) {return cfx_as_frac48(x);}
/* Analytic host stand-ins exercise product control/math code, NOT the SDK.
 * Counters prove full cases reached these boundaries with overrides off.
 */
static int host_log_calls,host_exp_calls,host_div_calls;
static int host_full_log_calls,host_full_exp_calls;
extern int g_tdc_ut_agc_override_enabled,g_tdc_ut_power_override_enabled;
#ifndef HOST_NUMERIC_LOG_BIAS
#define HOST_NUMERIC_LOG_BIAS 0
#endif
#ifndef HOST_NUMERIC_EXP_PERCENT
#define HOST_NUMERIC_EXP_PERCENT 100
#endif
static long long cfx_pwr_to_dB_asm(long long x) {
    assert(x>0);host_log_calls++;
    if(!g_tdc_ut_agc_override_enabled && !g_tdc_ut_power_override_enabled) host_full_log_calls++;
    return (long long)floor(10.0*log10((double)x/140737488355328.0)*65536.0)+HOST_NUMERIC_LOG_BIAS;
}
static long long cfx_dB_to_pwr_asm(long long x) {
    double y=pow(10.0,(double)x/(10.0*65536.0))*140737488355328.0*HOST_NUMERIC_EXP_PERCENT/100.0;
    host_exp_calls++;assert(x<=0);
    if(!g_tdc_ut_agc_override_enabled && !g_tdc_ut_power_override_enabled) host_full_exp_calls++;
    return y>=140737488355327.0 ? 140737488355327LL : (long long)floor(y);
}
static m8p16_t cfx_pwr_to_dB(frac48_t x) {m8p16_t m={cfx_pwr_to_dB_asm(x.v)};return m;}
static frac48_t cfx_dB_to_pwr(m8p16_t x) {return cfx_as_frac48(cfx_dB_to_pwr_asm(x.v));}
#define cfx_as_long(x) ((x).v)
static long long cfx_div_frac24_frac24_m24p24(int a,int b) {assert(b);host_div_calls++;return ((long long)a<<24)/b;}
extern int g_tdc_ut_agc_override_enabled,g_tdc_ut_agc_input_db,g_tdc_ut_agc_goal_db,g_tdc_ut_agc_clamped_input;
extern int g_tdc_ut_power_override_enabled;
int calculate_x_power_p(int x);
int calculate_agc_gain(int x);
void apply_agc_gain(int x);
"""

MAIN = r"""
int main(void)
{
    const int units[] = {8,13,16,17,18,20,22,23,24,25,28,29,30,31,32,34,35};
    int *arrays[] = {host_mix,m_agc_output_buffer,g_pcm_amplitude_level,
      g_mapping_stimulus_amplitude_T_level,g_mapping_stimulus_amplitude_C_level,
      g_max_limit_stimulus_amplitude_C_level,g_logaritmMapping_coeff_A_Q16_8,
      g_logaritmMapping_coeff_B_Q16_8,g_freq_rep_values,g_freq_rep_values_scaled,
      addr_MapProgramData_xMinLevel,addr_MapProgramData_xMaxLevel,addr_stimulationTempBuff,
      g_NofM_adjacent_BandIndex,g_NofM_notAdjacent_BandIndex,g_pass_bin_index,host_vmag};
    int lengths[]={16,16,32,32,32,32,32,32,32,32,32,32,32,16,16,256,256};
    for(unsigned a=0;a<sizeof(arrays)/sizeof(*arrays);a++)
        for(int i=0;i<lengths[a];i++) arrays[a][i]=(int)a*100+i+1;
    for(int i=0;i<32;i++) {
        shared.currentOutputStimulLevel_255[i]=1000+i;
        shared.calculatedStimulPara_byCM3.T_level_255[i]=2000+i;
        shared.calculatedStimulPara_byCM3.C_level_255[i]=3000+i;
    }
    m_prev_agc_10db_gain_Q8_16=777; m_audio_volume=3; s_lib_audio_front_mic=2;
    host_max=333; shared.maxAudioInput=444;
    g_tdc_ut_agc_goal_db=101;g_tdc_ut_agc_clamped_input=202;g_tdc_ut_agc_input_db=303;
    shared.is_enabled_mute_stimulation_under_t_level=2;
    shared.mute_stimulation_t_level_offset=7;
    shared.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3=1;
    host_shared_t before=shared;
    lib_g_i2s_state=0; /* RESET, not equivalent to DISABLED. */
    int original_coeff[4]={m_attack_coeff_Q8_16,m_release_coeff_Q8_16,
      m_one_minus_attack_coeff_Q8_16,m_one_minus_release_coeff_Q8_16};
    for(unsigned u=0;u<sizeof(units)/sizeof(*units);u++)
    {
        for(int repeat=0;repeat<2;repeat++) {
            host_log_calls=host_exp_calls=host_div_calls=0;
            host_full_log_calls=host_full_exp_calls=0;
            assert(tdc_ut_cfx_run_unit(units[u])==0);
            assert(g_tdc_ut_cfx_vector_done && !g_tdc_ut_cfx_vector_overflow);
            if(g_tdc_ut_cfx_vector_failed) {
                for(int i=0;i<g_tdc_ut_cfx_vector_count;i++)
                    if(g_tdc_ut_cfx_vector_actual[i]!=g_tdc_ut_cfx_vector_expected[i])
                        printf("CFX%d element%d A=%d E=%d\n",units[u],i,g_tdc_ut_cfx_vector_actual[i],g_tdc_ut_cfx_vector_expected[i]);
            }
            assert(g_tdc_ut_cfx_vector_failed==0);
            if(units[u]==23 || units[u]==31 || units[u]==32) {
                int pos=0,seen=0;
                assert(host_full_log_calls>0 && host_full_exp_calls>0);
                if(units[u]!=23) assert(host_div_calls>0);
                assert(g_tdc_ut_cfx_numeric_count>0 && g_tdc_ut_cfx_numeric_count<=TDC_UT_CFX_NUMERIC_CAPACITY);
                while(pos<g_tdc_ut_cfx_numeric_count) {
                    assert(g_tdc_ut_cfx_numeric_actual[pos++]==-999);
                    assert(g_tdc_ut_cfx_numeric_actual[pos++]==units[u]);
                    int vector=g_tdc_ut_cfx_numeric_actual[pos++];
                    int count=g_tdc_ut_cfx_numeric_actual[pos++];
                    assert(vector>=10 && count%3==0 && pos+count<=g_tdc_ut_cfx_numeric_count);
                    for(int j=0;j<count;j+=3) {
                        int a=g_tdc_ut_cfx_numeric_actual[pos+j];
                        int lower=g_tdc_ut_cfx_numeric_actual[pos+j+1];
                        int upper=g_tdc_ut_cfx_numeric_actual[pos+j+2];
                        assert(lower<=a && a<=upper);seen++;
                    }
                    pos+=count;
                }
                assert(seen==(units[u]==23 ? 36 : units[u]==31 ? 64 : 128));
            } else assert(!g_tdc_ut_cfx_numeric_count);
            for(unsigned a=0;a<sizeof(arrays)/sizeof(*arrays);a++)
                for(int i=0;i<lengths[a];i++) assert(arrays[a][i]==(int)a*100+i+1);
            assert(!memcmp(&before,&shared,sizeof(shared)));
            assert(m_prev_agc_10db_gain_Q8_16==777 && m_audio_volume==3 && s_lib_audio_front_mic==2);
            assert(host_max==333 && !g_tdc_ut_agc_override_enabled && !g_tdc_ut_power_override_enabled);
            assert(g_tdc_ut_agc_goal_db==101 && g_tdc_ut_agc_clamped_input==202 && g_tdc_ut_agc_input_db==303);
            assert(lib_g_i2s_state==0);
            assert(m_attack_coeff_Q8_16==original_coeff[0] && m_release_coeff_Q8_16==original_coeff[1]);
            assert(m_one_minus_attack_coeff_Q8_16==original_coeff[2] && m_one_minus_release_coeff_Q8_16==original_coeff[3]);
            assert(tdc_ut_cfx_run_unit(16)==-2); /* immutable until drain */
            int result_count=g_tdc_ut_cfx_vector_count;
            int numeric_count=g_tdc_ut_cfx_numeric_count;
            g_tdc_ut_hear_pending=1;
            assert(tdc_ut_cfx_run_unit(units[u])==-2);
            assert(result_count==g_tdc_ut_cfx_vector_count && numeric_count==g_tdc_ut_cfx_numeric_count);
            g_tdc_ut_hear_pending=0;
            tdc_ut_cfx_vectors_release();
            /* Each external producer guard is tested with done cleared. */
            int *busy_flags[]={&g_tdc_ut_hear_pending,&g_tdc_ut_hear_done,&g_tdc_ut_cfx_io_done};
            for(int b=0;b<3;b++) {
                *busy_flags[b]=1;
                assert(tdc_ut_cfx_run_unit(units[u])==-2);
                assert(!g_tdc_ut_cfx_vector_done && result_count==g_tdc_ut_cfx_vector_count);
                assert(numeric_count==g_tdc_ut_cfx_numeric_count);
                *busy_flags[b]=0;
            }
        }
        printf("CFX-%03d HOST_PASS values=%d\n",units[u],g_tdc_ut_cfx_vector_count);
    }
    assert(tdc_ut_cfx_run_unit(99)==-1);
    g_tdc_ut_agc_override_enabled=1;
    assert(tdc_ut_cfx_run_unit(16)==-1);
    g_tdc_ut_agc_override_enabled=0;
    /* Range predicate regressions: a value outside either side must fail.
     * The raw measurement cannot be replaced with its expected center.
     */
    g_tdc_ut_cfx_vector_count=g_tdc_ut_cfx_vector_failed=g_tdc_ut_cfx_numeric_count=0;
    put_numeric(100,100,110);put_numeric(110,100,110);
    put_numeric(99,100,110);put_numeric(111,100,110);put_numeric(105,110,100);
    assert(g_tdc_ut_cfx_vector_failed==3);
    assert(g_tdc_ut_cfx_numeric_actual[6]==99 && g_tdc_ut_cfx_numeric_actual[9]==111);
    g_tdc_ut_cfx_vector_count=TDC_UT_CFX_VECTOR_CAPACITY;
    g_tdc_ut_cfx_numeric_count=TDC_UT_CFX_NUMERIC_CAPACITY;
    put_numeric(105,100,110);assert(g_tdc_ut_cfx_vector_overflow);
    return 0;
}
"""


def main():
    assert cfx_numeric_reference.HEADER.read_text(encoding="utf-8") == cfx_numeric_reference.render_header(cfx_numeric_reference.references())
    paths = {
        "agc": "src/1__cfx/signalProcessing/agc.c",
        "mapping": "src/1__cfx/signalProcessing/nonlinearMapping.c",
        "strategy": "src/1__cfx/signalProcessing/stimulationStrategy.c",
        "mixer": "src/1__cfx/systemControl/audioMixer.c",
        "loopback": "src/1__cfx/lib_cfx/lib_audio_mixer.c",
        "audio": "src/1__cfx/lib_cfx/lib_audio_in.c",
        "i2s": "src/1__cfx/lib_cfx/lib_i2s.c",
        "frequency": "src/1__cfx/signalProcessing/FrequencyAnalysis.c",
        "runner": "src/1__cfx/test/tdc_ut_vectors_cfx.c",
        "reference": "src/1__cfx/test/tdc_ut_numeric_reference_cfx.h",
        "header": "src/1__cfx/test/tdc_ut_vectors_cfx.h",
    }
    texts = {k: (ROOT / p).read_text(encoding="utf-8-sig") for k, p in paths.items()}
    pieces = [PREFIX]
    for name, first in [("agc", "audio_agc"), ("mapping", "logarithmMapping"), ("strategy", "read_transferableChannelNum_fromCM3")]:
        pre = texts[name].split("void " + first + "(", 1)[0]
        pieces.append(re.sub(r"^#include[^\n]*", "", pre, flags=re.M))
    selection = {
        "audio": ["tdc_audio_set_front_mic", "tdc_audio_get_front_mic"],
        "i2s": ["tdc_i2s_set_streaming_state", "I2S_isStreaming"],
        "mixer": ["tdc_audio_mix_1_buffer", "tdc_audio_mix_2_buffers", "tdc_audio_mix_2_buffers_for_beamforming", "tdc_audio_mix_2_buffers_with_gain"],
        "loopback": ["lib_loopback_AGC_out"],
        "agc": ["audio_agc_attack_release", "calculate_agc_gain", "apply_agc_gain", "audio_agc"],
        "mapping": ["calculate_x_power_p", "read_C_T_Level_fromCM3", "calculate_newStimulation_maxLevel", "calculate_logaritmMapping_coeff", "logarithmMapping"],
        "frequency": ["find_freq_rep_value"],
        "strategy": ["stimulationStrategy_NofM_Init_Buffers", "stimulationStrategy_NofM_Peak_Pick", "stimulationStrategy_NofM_Interleaving"],
    }
    pieces += [function(texts[k], name) for k, names in selection.items() for name in names]
    runner = re.sub(r"^#include[^\n]*", "", texts["runner"], flags=re.M)
    # The public capacity definition lives in a header included by target code.
    pieces.append(texts["header"] + "\n" + texts["reference"] + "\n" + runner)
    pieces.append(MAIN)
    source = re.sub(r"\blong\b", "long long", "\n".join(pieces[1:]))
    # Only target product bodies use 48-bit long. Do not widen host adapters.
    source = PREFIX + source
    OUT.mkdir(parents=True, exist_ok=True)
    cfile = OUT / "vectors.c"
    cfile.write_text(source, encoding="utf-8")
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc unavailable")
    exe = OUT / "vectors.exe"
    command = [gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", str(cfile), "-o", str(exe)]
    built = subprocess.run(command, capture_output=True, text=True)
    if built.returncode:
        raise RuntimeError(built.stdout + built.stderr)
    ran = subprocess.run([str(exe)], capture_output=True, text=True)
    print(ran.stdout)
    data = {"scope": "host calculation bodies, not target SDK approximation or hardware", "build": command,
            "exit_code": ran.returncode, "stdout": ran.stdout,
            "sources": {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths.values()}}
    (OUT / "checks.json").write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stderr or "vector assertion failed")


if __name__ == "__main__":
    main()
