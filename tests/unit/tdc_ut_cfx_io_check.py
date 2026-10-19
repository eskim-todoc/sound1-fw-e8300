"""Run exact current CFX I/O function bodies with RAM hardware boundaries.

No HEAR emulation: start/poll must return TARGET_REQUIRED on the host. Known
criterion mismatch (15 completion flag) remains a failure; CIS keeps its original expected vectors.
Generated translation units are build artifacts; source SHA256s are recorded.
"""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from check_cfx_vectors import function

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/cfx-io"
PREFIX = r"""
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define TDC_UNIT_TEST 1
#define TDC_UT_HOST 1
#include <main.h>
#include <FrequencyAnalysis.h>
#include "tdc_ut_io_cfx.h"
#include "tdc_ut_vectors_cfx.h"
#include "tdc_ut_hear_reference.h"
ST__CFX_CM3_SharedMemory_ALL shared;
#define Addr_SharedMem (&shared)
/* 가속기 실행 없이 실제 CFX 소비 함수의 입력 메모리만 RAM으로 옮긴다. */
static int s_tdc_ut_host_vmag[256];
#undef HEAR_ADDR_VMAG_OUTPUT
#define HEAR_ADDR_VMAG_OUTPUT s_tdc_ut_host_vmag
FS_MEM_UART_T host_uart;
#undef FS_MEM_UART
#define FS_MEM_UART (&host_uart)
D_FIFO_Type g_tdc_ut_host_fifo;
volatile app_interrupt_flag_t g_interrupt_flags;
volatile int HCT_A0_0[32], HCT_A0_1[32];
int m_link_connection_check_counter;
int g_tdc_ut_agc_override_enabled, g_tdc_ut_power_override_enabled;
volatile int g_tdc_ut_cfx_vector_done;
typedef struct {long long v;} m8p16_t;
typedef struct {long long v;} frac48_t;
#define to_int(x) ((int)(x))
#define to_long(x) (x)
#define to_fix(x) (x)
#define to_lfix(x) (x)
static frac48_t cfx_as_frac48(long long x) {frac48_t f={x};return f;}
static frac48_t cfx_as_frac24(int x) {return cfx_as_frac48(x);}
/* 근사 라이브러리 호출은 실패 처리. override를 켠 정수 경로만 시험한다. */
static m8p16_t cfx_pwr_to_dB(frac48_t x) {(void)x;assert(0);m8p16_t m={0};return m;}
static frac48_t cfx_dB_to_pwr(m8p16_t x) {(void)x;assert(0);return cfx_as_frac48(0);}
#define cfx_as_long(x) ((x).v)
static long long cfx_pwr_to_dB_asm(long long x) {(void)x;assert(0);return 0;}
static long long cfx_dB_to_pwr_asm(long long x) {(void)x;assert(0);return 0;}
static long long cfx_div_frac24_frac24_m24p24(int a,int b) {assert(b);return ((long long)a*16777216)/b;}
"""

MAIN = r"""
static void check_record_format(void)
{
    int i=0;
    while (i<g_tdc_ut_cfx_io_count)
    {
        assert(g_tdc_ut_cfx_io_actual[i]==-999);
        assert(g_tdc_ut_cfx_io_expected[i]==-999);
        assert(!memcmp(&g_tdc_ut_cfx_io_actual[i],&g_tdc_ut_cfx_io_expected[i],4*sizeof(int)));
        int length=g_tdc_ut_cfx_io_actual[i+3];
        assert(length>0 && i+4+length<=g_tdc_ut_cfx_io_count);
        i+=4+length;
    }
    assert(i==g_tdc_ut_cfx_io_count);
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const int ids[]={7,10,11,12,15,19,26,27,33,36,37,40};
    for (int i=0; i<32; i++)
    { g_pcm_amplitude_level[i]=101+i; addr_stimulationTempBuff[i]=201+i;
      addr_electrodeMap[i]=31-i; }
    for (int i=0; i<16; i++)
    { g_NofM_notAdjacent_BandIndex[i]=i; m_agc_output_buffer[i]=301+i; }
    for (int i=0; i<256; i++) g_pass_bin_index[i]=i%32;
    lib_g_i2s_buffer_sub_state=8; lib_g_i2s_buffer_state=2;
    lib_g_i2s_buffer_in_pos=4; lib_g_i2s_buffer_out_pos=3;
    lib_g_i2s_buffer_copy_cnt=5; tdc_i2s_set_streaming_state(LIB_I2S_STATE_RESET);
    for (int b=0; b<8; b++) for (int i=0; i<16; i++) lib_g_i2s_buffers[b][i]=b*100+i;
    for (int m=0; m<2; m++) for (int b=0; b<2; b++) for (int i=0; i<16; i++) g_lib_dmic_in_buffers[m][b][i]=1000*m+100*b+i;
    shared.userSettingValue.audioVolume=2; shared.userSettingValue.stimulVolume=2;
    shared.connected_ISD_num=2; shared.is_pcm_specific_command_reading=0;
    shared.pcm_specific_command_read_index=17;
    host_uart.flag[0]=1;
    for (int i=0; i<512; i++) host_uart.buffer[i]=8000+i;
    static ST__CFX_CM3_SharedMemory_ALL before;
    before=shared;
    for (unsigned u=0; u<sizeof(ids)/sizeof(*ids); u++)
    {
        for (int repeat=0; repeat<2; repeat++)
        {
            assert(tdc_ut_cfx_io_run_unit(ids[u])==0);
            assert(g_tdc_ut_cfx_io_done && !g_tdc_ut_cfx_io_overflow);
            check_record_format();
            int known=ids[u]==15 ? 1 : 0;
            if (g_tdc_ut_cfx_io_failed!=known)
                for (int i=0; i<g_tdc_ut_cfx_io_count; i++)
                    if (g_tdc_ut_cfx_io_actual[i]!=g_tdc_ut_cfx_io_expected[i])
                        printf("CFX-%d unexpected element=%d actual=%d expected=%d\n",ids[u],i,g_tdc_ut_cfx_io_actual[i],g_tdc_ut_cfx_io_expected[i]);
            assert(g_tdc_ut_cfx_io_failed==known);
            assert(!memcmp(&before,&shared,sizeof(shared)));
            assert(!g_tdc_ut_io_override_enabled && !g_tdc_ut_power_override_enabled);
            assert(tdc_ut_i2s_raw_state()==LIB_I2S_STATE_RESET);
            assert(lib_g_i2s_buffer_in_pos==4 && lib_g_i2s_buffer_out_pos==3);
            assert(lib_g_i2s_buffer_copy_cnt==5 && lib_g_i2s_buffer_state==2 && lib_g_i2s_buffer_sub_state==8);
            for (int i=0; i<32; i++) { assert(g_pcm_amplitude_level[i]==101+i); assert(addr_stimulationTempBuff[i]==201+i); assert(addr_electrodeMap[i]==31-i); }
            for (int i=0; i<16; i++) { assert(g_NofM_notAdjacent_BandIndex[i]==i); assert(m_agc_output_buffer[i]==301+i); }
            for (int i=0; i<256; i++) assert(g_pass_bin_index[i]==i%32);
            for (int b=0; b<8; b++) for (int i=0; i<16; i++) assert(lib_g_i2s_buffers[b][i]==b*100+i);
            for (int m=0; m<2; m++) for (int b=0; b<2; b++) for (int i=0; i<16; i++) assert(g_lib_dmic_in_buffers[m][b][i]==1000*m+100*b+i);
            for (int i=0; i<512; i++) assert(host_uart.buffer[i]==8000+i);
            static int immutable[TDC_UT_CFX_IO_CAPACITY];
            memcpy(immutable,g_tdc_ut_cfx_io_actual,sizeof(immutable));
            assert(tdc_ut_cfx_io_run_unit(7)==TDC_UT_IO_BUSY);
            assert(tdc_ut_hear_start(1,1,1)==TDC_UT_IO_BUSY);
            assert(!memcmp(immutable,g_tdc_ut_cfx_io_actual,sizeof(immutable)));
            assert(tdc_ut_cfx_io_release()==0);
        }
        printf("CFX-%02d %s mismatches=%d values=%d\n",ids[u],ids[u]==15 ? "CRITERIA_MISMATCH" : "HOST_VECTOR_PASS",g_tdc_ut_cfx_io_failed,g_tdc_ut_cfx_io_count);
        for (int i=0; i<g_tdc_ut_cfx_io_count; i++)
            if (g_tdc_ut_cfx_io_actual[i]!=g_tdc_ut_cfx_io_expected[i])
                printf("  actual[%d]=%d expected=%d\n",i,g_tdc_ut_cfx_io_actual[i],g_tdc_ut_cfx_io_expected[i]);
        /* Host evidence only. Target output remains the parent's ASCII reporter. */
        printf("CFX_CAPTURE {\"unit\":%d,\"host_mismatch_count\":%d,\"word_count\":%d,\"actual\":[",ids[u],g_tdc_ut_cfx_io_failed,g_tdc_ut_cfx_io_count);
        for (int i=0; i<g_tdc_ut_cfx_io_count; i++) printf("%s%d",i ? "," : "",g_tdc_ut_cfx_io_actual[i]);
        printf("],\"expected\":[");
        for (int i=0; i<g_tdc_ut_cfx_io_count; i++) printf("%s%d",i ? "," : "",g_tdc_ut_cfx_io_expected[i]);
        puts("]}");
    }
    assert(tdc_ut_cfx_io_run_unit(14)==TDC_UT_IO_UNSUPPORTED);
    assert(tdc_ut_cfx_io_run_unit(99)==TDC_UT_IO_UNSUPPORTED);
    static int captured_actual[TDC_UT_CFX_IO_CAPACITY], captured_expected[TDC_UT_CFX_IO_CAPACITY];
    memcpy(captured_actual,g_tdc_ut_cfx_io_actual,sizeof(captured_actual));
    memcpy(captured_expected,g_tdc_ut_cfx_io_expected,sizeof(captured_expected));
    for (int state=0; state<3; state++)
    {
        g_tdc_ut_cfx_vector_done=state==0;
        g_tdc_ut_hear_pending=state==1;
        g_tdc_ut_hear_done=state==2;
        assert(tdc_ut_cfx_io_run_unit(7)==TDC_UT_IO_BUSY);
        assert(tdc_ut_hear_start(1,1,1)==TDC_UT_IO_BUSY);
        assert(!memcmp(captured_actual,g_tdc_ut_cfx_io_actual,sizeof(captured_actual)));
        assert(!memcmp(captured_expected,g_tdc_ut_cfx_io_expected,sizeof(captured_expected)));
        assert(!memcmp(&before,&shared,sizeof(shared)));
        if (state==1) assert(tdc_ut_cfx_io_release()==TDC_UT_IO_BUSY && tdc_ut_hear_release()==TDC_UT_IO_BUSY);
    }
    g_tdc_ut_cfx_vector_done=g_tdc_ut_hear_pending=g_tdc_ut_hear_done=0;
    assert(tdc_ut_hear_start(1,1,0)==TDC_UT_IO_TARGET_REQUIRED);
    assert(tdc_ut_hear_start(1,1,1)==TDC_UT_IO_TARGET_REQUIRED);
    assert(tdc_ut_hear_start(2,1,1)==TDC_UT_IO_TARGET_REQUIRED);
    assert(tdc_ut_hear_poll()==TDC_UT_IO_TARGET_REQUIRED);
    assert(!g_tdc_ut_hear_done && g_tdc_ut_hear_target_needed);
    assert(g_tdc_ut_hear_vmag_count==0);
    puts("HEAR-1/2 TARGET_REQUIRED; host did not fabricate completion");
    tdc_ut_cfx_io_host_cases();
    return 0;
}
"""


def pointer_function(text: str, name: str) -> str:
    """Preserve the complete pointer-returning product function body."""
    pattern = r"^int\s+_XMEM\s*\*" + re.escape(name) + r"\s*\([^;]*?\)\s*\{"
    found = re.search(pattern, text, re.M)
    if not found:
        raise ValueError(f"missing pointer function {name}")
    # Reuse the tokenizer; replace only the temporary signature for locating.
    selected = "int " + text[found.start():].split(name, 1)[1]
    located = function("int " + name + selected[4:], name)
    return text[found.start():found.start() + len(located) + len(found.group().split(name, 1)[0]) - 4]


def without_includes(text: str) -> str:
    return re.sub(r"^#include[^\n]*", "", text, flags=re.M)


def audit_candidate_criteria(captures: list[dict], criteria: dict) -> None:
    """Check separately calculated document candidates against exact captures.

    The candidate export is not generated from these actual output arrays.
    This catches divergence between frozen target vectors and workbook inputs.
    """
    units = {capture["unit"]: capture for capture in captures}

    def records(unit: int) -> dict[int, list[int]]:
        values = units[unit]["actual"]
        result = {}
        position = 0
        while position < len(values):
            marker, number, vector, length = values[position:position + 4]
            assert marker == -999 and number == unit and vector not in result
            result[vector] = values[position + 4:position + 4 + length]
            position += 4 + length
        assert position == len(values)
        return result

    headers = records(36)
    for case in criteria["CFX-036"]["cases"]:
        payload = headers[case["case"]]
        assert payload[0] == int(case["expected_header_hex"], 16)
        assert [payload[1 + 6 * i] for i in range(4)] == case["expected_words"]
    nofm = records(37)
    for case in criteria["CFX-037"]["cases"]:
        vector = case["case"] - 1
        assert nofm[10 + vector][:16] == case["expected_packet_words"]
        assert nofm[10 + vector][16] == case["expected_last_band_0_based"]
        for index, expected in enumerate(case["expected_pcm_blocks"]):
            assert nofm[vector * 2 + index + 1][:24] == expected
            assert nofm[vector * 2 + index + 1][24] == case["expected_indices"][index]
    cis = records(33)
    for i in range(3):
        assert cis[10 + i] == criteria["CFX-033"]["expected_packet_words"]
        assert cis[i + 1][:24] == criteria["CFX-033"]["expected_pcm_blocks"][i]
        assert cis[i + 1][24] == criteria["CFX-033"]["expected_indices"][i]
    specific = records(40)
    for case in criteria["CFX-040"]["cases"]:
        payload = specific[case["case"]]
        assert payload[0] == 24
        assert [payload[2 + 4 * i] for i in range(24)] == case["input_and_expected_words"]
        assert payload[-24:] == case["input_and_expected_words"]


def main() -> None:
    names = {"main": "systemControl/main.c", "system": "systemControl/system_control.c",
             "i2s": "lib_cfx/lib_i2s.c", "audio": "lib_cfx/lib_audio_in.c",
             "freq": "signalProcessing/FrequencyAnalysis.c", "mapping": "signalProcessing/nonlinearMapping.c",
             "strategy": "signalProcessing/stimulationStrategy.c", "agc": "signalProcessing/agc.c",
             "pcm": "internalDevice/driver_PCM.c", "runner": "test/tdc_ut_io_cfx.c"}
    paths = {k: ROOT / "src/1__cfx" / p for k, p in names.items()}
    paths["host_cases"] = ROOT / "tests/unit/tdc_ut_cfx_io_cases.c"
    paths["host_boundary"] = ROOT / "tests/unit/tdc_ut_cfx_io_host/hw.h"
    paths["runner_header"] = ROOT / "src/1__cfx/test/tdc_ut_io_cfx.h"
    source_bytes = {k: p.read_bytes() for k, p in paths.items()}
    texts = {k: data.decode("utf-8-sig") for k, data in source_bytes.items()}
    pieces = [PREFIX]
    for k, first in [("system", "void LB_Normal_PowerMode"), ("i2s", "void lib_init_I2S"),
                     ("mapping", "void logarithmMapping"), ("strategy", "void read_transferableChannelNum_fromCM3"),
                     ("agc", "void audio_agc"), ("audio", "static void lib_init_audio_in_path_common")]:
        pieces.append(without_includes(texts[k].split(first, 1)[0]))
    selection = {
        "main": ["tdc_copy_DMIC_buffers"],
        "system": ["LB_Normal_PowerMode", "Normal_PowerMode_isdConnected", "Normal_PowerMode_isdDisonnected",
                   "Normal_PowerMode_event_mapChange", "Normal_PowerMode_event_audioParameterCalculation",
                   "Normal_PowerMode_event_both_stimulation_audio_volumeChange", "copy_MappingData", "copy_MappingData_without_mappingDate"],
        "i2s": ["tdc_i2s_set_streaming_state", "tdc_ut_i2s_raw_state", "lib_i2s_copy_data_from_fifo",
                "I2S_isStreaming", "lib_i2s_is_buffer_copy_done", "lib_i2s_clear_buffer_copy_done", "lib_i2s_clear_data"],
        "audio": ["tdc_enable_1_DMIC", "tdc_enable_2_DMICs", "tdc_audio_set_front_mic"],
        "freq": ["read_FFT_PassBin_index", "update_FFT_inputData", "find_freq_rep_value"],
        "mapping": ["calculate_x_power_p", "calculate_newStimulation_maxLevel", "read_C_T_Level_fromCM3", "calculate_logaritmMapping_coeff_with_audioVolume", "logarithmMapping"],
        "strategy": ["read_transferableChannelNum_fromCM3", "prepare_pcmStimulationPacketHeader", "stimulationStrategy_CIS", "stimulationStrategy_NofM_Packet", "stimulationStrategy_NofM_PCM_Write", "stimulationStrategy_NofM_Init_Buffers", "stimulationStrategy_NofM_Peak_Pick", "stimulationStrategy_NofM_Interleaving", "stimulationStrategy_NofM", "stimulationStrategy"],
        "pcm": ["fn_PcmBitStream_Mode_SepcificCommand", "fn_PcmBitStream_Mode_NopStandby"],
    }
    pieces.append("int g_pass_bin_index[256], g_freq_rep_values[32], g_freq_rep_values_scaled[32];\n")
    pieces += [function(texts[k], name) for k, funcs in selection.items() for name in funcs]
    pieces += [pointer_function(texts["i2s"], f) for f in ["I2S_get_buffer_with_fade_in_process", "I2S_get_buffer_with_fade_out_process"]]
    pieces.append(without_includes(texts["runner"]))
    pieces.append(texts["host_cases"])
    pieces.append(MAIN)
    OUT.mkdir(parents=True, exist_ok=True)
    generated = OUT / "io.c"
    # CFX long has 48 bits. Windows long has 32 bits; selected bounded vectors
    # require widened host intermediates. Target ABI/saturation is not proven.
    source = PREFIX + re.sub(r"\blong\b", "long long", "\n".join(pieces[1:]))
    generated.write_text(source, encoding="utf-8")
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("gcc unavailable")
    includes = [ROOT / "tests/unit/tdc_ut_cfx_io_host"] + [ROOT / "src/1__cfx" / p for p in ["systemControl", "environment", "hear", "signalProcessing", "internalDevice", "lib_cfx", "OTE_1_5_gen", "test"]]
    command = [gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-Wno-unused-variable"]
    command += ["-I" + str(p) for p in includes]
    command += [str(generated), "-o", str(OUT / "io.exe")]
    built = subprocess.run(command, capture_output=True, text=True, timeout=60)
    if built.returncode:
        raise RuntimeError(built.stdout + built.stderr)
    ran = subprocess.run([str(OUT / "io.exe")], capture_output=True, text=True, timeout=10)
    captures = [json.loads(line.removeprefix("CFX_CAPTURE "))
                for line in ran.stdout.splitlines() if line.startswith("CFX_CAPTURE ")]
    extensions = [json.loads(line.removeprefix("CFX_EXTRA "))
                  for line in ran.stdout.splitlines() if line.startswith("CFX_EXTRA ")]
    summary = "\n".join(line for line in ran.stdout.splitlines()
                        if not line.startswith(("CFX_CAPTURE ", "CFX_EXTRA ")))
    print(summary)
    unit_names = {7: "마이크 블록 수집", 10: "내부기 연결 상태 반영", 11: "맵 및 볼륨 변경 처리",
                  12: "공유 맵 데이터 복사", 15: "I2S 수신 버퍼 관리", 19: "I2S 시작과 중단의 완만한 전환",
                  26: "주파수 분석 입력 갱신", 27: "FFT 대역 인덱스 읽기", 33: "CIS 전략과 전략 선택",
                  36: "자극 패킷 헤더 구성", 37: "NofM 패킷 구성과 분할 출력", 40: "지정 명령 PCM 출력"}
    for capture in captures:
        capture["name"] = unit_names[capture["unit"]]
        capture["status"] = "CRITERIA_MISMATCH" if capture["host_mismatch_count"] else "HOST_VECTOR_PASS"
        capture["executions"] = 2
        capture["whole_unit_pass_claimed"] = False
        capture["mismatches"] = [{"offset": i, "actual": a, "expected": e}
                                 for i, (a, e) in enumerate(zip(capture["actual"], capture["expected"]))
                                 if a != e]
    for case in extensions:
        case["scope"] = "host-only real product functions; RAM SDK/address boundaries"
        case["assessment"] = ("characterization; safe failure/output policy unresolved"
                              if case["case"] in (106, 110, 111) else "contract comparison")
    coverage_path = ROOT / "tests/unit/tdc_ut_cfx_io_coverage.json"
    coverage = json.loads(coverage_path.read_text(encoding="utf-8"))
    candidate_path = ROOT / "tests/unit/tdc_ut_cfx_io_criteria.json"
    candidates = json.loads(candidate_path.read_text(encoding="utf-8"))
    criteria = {u["unit"]: u for u in coverage["units"] if u["core"] == "CFX" and u["host_mismatch_count"] is not None}
    if not ran.returncode:
        audit_candidate_criteria(captures, candidates)
        assert len(extensions) == 24
        assert {(case["case"], case["repeat"]) for case in extensions} == {
            (case, repeat) for case in range(100, 112) for repeat in range(2)}
        for case in extensions:
            assert case["actual"] == case["expected"] and case["actual"]
        known_offsets = {15: [23]}
        assert len(captures) == len(criteria) == 12
        for capture in captures:
            criterion = criteria[capture["unit"]]
            assert capture["name"] == criterion["name"]
            assert capture["status"] == criterion["status"]
            assert capture["host_mismatch_count"] == criterion["host_mismatch_count"]
            assert capture["word_count"] == criterion["word_count"]
            assert capture["host_mismatch_count"] == len(capture["mismatches"])
            assert [item["offset"] for item in capture["mismatches"]] == known_offsets.get(capture["unit"], [])
    evidence = {"scope": "actual C function bodies; RAM hardware boundary; no HEAR execution",
                "build": command, "build_exit_code": built.returncode,
                "exit_code": ran.returncode, "stdout": summary, "units": captures,
                "criteria_failures": [15],
                "host_only_cases": extensions,
                "host_only_comparisons": sum(len(case["actual"]) for case in extensions),
                "coverage": str(coverage_path.relative_to(ROOT)),
                "coverage_status_verified": not ran.returncode,
                "candidate_criteria_verified": not ran.returncode,
                "candidate_criteria": str(candidate_path.relative_to(ROOT)),
                "candidate_criteria_sha256": hashlib.sha256(candidate_path.read_bytes()).hexdigest(),
                "sources": {str(paths[k].relative_to(ROOT)): hashlib.sha256(data).hexdigest()
                            for k, data in source_bytes.items()}}
    (OUT / "checks.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding="utf-8")
    if ran.returncode:
        raise RuntimeError(ran.stderr or "host assertion failed")


if __name__ == "__main__":
    main()
