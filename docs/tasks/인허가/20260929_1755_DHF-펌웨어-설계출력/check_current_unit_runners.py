"""호스트에서 새 시험 경로의 배열, 판정, 복원과 비활성 빌드를 확인한다.

제품 함수 본문을 읽어 임시 호스트 C 파일에 넣는다. CFX long은 이 입력의
중간값을 담는 long long으로 대체하므로 타깃 ABI/하드웨어 검증이 아니다.
SDK/타깃 빌드나 실제 시험 결과로 사용하지 않는다.
"""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
QA = TASK / "qa/current-unit-runners"
RUNNERS = {
    "cfx": ROOT / "src/1__cfx/test/tdc_unit_test_cfx.c",
    "cm3": ROOT / "src/2__cm3/Cortex-M3-src/test/tdc_unit_test_cm3.c",
}


def function(text, name):
    match = re.search(r"^(?:static\s+)?(?:int|void)\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", text, re.M)
    if match is None:
        raise ValueError("function not found: " + name)
    depth = 1
    i = match.end()
    # 문자열 안에 중괄호가 없는, 아래에 지정한 계산 함수에만 사용한다.
    while depth:
        depth += (text[i] == "{") - (text[i] == "}")
        i += 1
    return text[match.start():i]


def compile_run(source, name, gcc, flags=()):
    cfile = QA / (name + ".c")
    exe = QA / (name + ".exe")
    cfile.write_text(source, encoding="utf-8")
    command = [gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", *flags, cfile.name, "-o", exe.name]
    build = subprocess.run(command, cwd=QA, text=True, encoding="utf-8", errors="replace", capture_output=True)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], text=True, encoding="utf-8", errors="replace", capture_output=True)
    if run.returncode:
        raise RuntimeError(name + ": " + run.stdout + run.stderr)
    return {"build": command, "exit_code": run.returncode, "stdout": run.stdout.strip()}


CFX_PREFIX = r"""
#include <assert.h>
#include <stdio.h>
#define _XMEM
#define chess_loop_range(a,b)
#define df_inputADC_DataBuffLength 16
#define df_MaxNumOfElectrode 32
#define df_MaxNum_NofM 16
#define INT24_MAX 8388607
#define INT24_MIN (-8388608)
int m_mix[16], m_agc_output_buffer[16], g_pcm_amplitude_level[32];
int addr_stimulationTempBuff[32], g_NofM_adjacent_BandIndex[16];
int g_NofM_notAdjacent_BandIndex[16];
int m_prev_agc_10db_gain_Q8_16;
int m_attack_coeff_Q8_16, m_release_coeff_Q8_16;
int m_one_minus_attack_coeff_Q8_16, m_one_minus_release_coeff_Q8_16;
#define HEAR_ADDR_AUDIO_MIX m_mix
"""

CFX_MAIN = r"""
int main(void) {
    int i, pass;
    m_prev_agc_10db_gain_Q8_16 = 777;
    m_attack_coeff_Q8_16 = 101; m_release_coeff_Q8_16 = 102;
    m_one_minus_attack_coeff_Q8_16 = 103; m_one_minus_release_coeff_Q8_16 = 104;
    for (i=0;i<16;i++) {
        m_mix[i]=1000+i; m_agc_output_buffer[i]=2000+i;
        g_NofM_adjacent_BandIndex[i]=3000+i;
        g_NofM_notAdjacent_BandIndex[i]=4000+i;
    }
    for(i=0;i<32;i++) {g_pcm_amplitude_level[i]=5000+i;addr_stimulationTempBuff[i]=6000+i;}
    for(pass=0;pass<2;pass++) {
        tdc_ut_cfx_run_current();
        assert(g_tdc_ut_cfx_current_done==1);
        assert(g_tdc_ut_cfx_current_count==115);
        assert(g_tdc_ut_cfx_current_failed==0);
        assert(g_tdc_ut_cfx_current_overflow==0);
        assert(m_prev_agc_10db_gain_Q8_16==777);
        assert(m_attack_coeff_Q8_16==101 && m_release_coeff_Q8_16==102);
        assert(m_one_minus_attack_coeff_Q8_16==103 && m_one_minus_release_coeff_Q8_16==104);
        for(i=0;i<115;i++) assert(g_tdc_ut_cfx_current_result[i]==g_tdc_ut_cfx_current_expected[i]);
        for(i=0;i<16;i++) {
            assert(m_mix[i]==1000+i && m_agc_output_buffer[i]==2000+i);
            assert(g_NofM_adjacent_BandIndex[i]==3000+i);
            assert(g_NofM_notAdjacent_BandIndex[i]==4000+i);
        }
        for(i=0;i<32;i++) {assert(g_pcm_amplitude_level[i]==5000+i);assert(addr_stimulationTempBuff[i]==6000+i);}
    }
    current_put(42,41); assert(g_tdc_ut_cfx_current_failed==1);
    g_tdc_ut_cfx_current_count=TDC_UT_CFX_CURRENT_MAX;
    current_put(42,41);
    assert(g_tdc_ut_cfx_current_count==TDC_UT_CFX_CURRENT_MAX);
    assert(g_tdc_ut_cfx_current_overflow==1);
    assert(g_tdc_ut_cfx_current_failed==1);
    puts("CFX: 4 units, vectors/results, repeated run, state restoration, mismatch and overflow checked on host");
    return 0;
}
"""

CM3_PREFIX = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define FPGA_interphaseGapTokenTime 4
#define FPGA_electrodAndStimulLevelTokenTime 10
#define FPGA_oneChannelDataTokenTime 41
#define df_MaxNumTransferableChannel 24
#define ci_printi printf
"""

CM3_MAIN = r"""
int main(void) {
    int i, pass;
    assert(tdc_ut_cm3_print_current(0)==-1);
    for(pass=0;pass<2;pass++) {
        tdc_ut_cm3_run_current();
        assert(g_tdc_ut_cm3_current_done==1);
        assert(g_tdc_ut_cm3_current_count==12);
        assert(g_tdc_ut_cm3_current_overflow==0);
#ifdef TDC_UT_HOST_FAULT
        assert(g_tdc_ut_cm3_current_failed==2);
#else
        assert(g_tdc_ut_cm3_current_failed==0);
        for(i=0;i<12;i++) assert(g_tdc_ut_cm3_current_result[i]==g_tdc_ut_cm3_current_expected[i]);
#endif
    }
    (void)i;
    assert(tdc_ut_cm3_print_current(-2)==-1);
    assert(tdc_ut_cm3_print_current(12)==-1);
    assert(tdc_ut_cm3_print_current(-1)==0);
    for(i=0;i<12;i++) assert(tdc_ut_cm3_print_current(i)==0);
    g_tdc_ut_cm3_current_failed=0;
    current_put(42,41); assert(g_tdc_ut_cm3_current_failed==1);
    g_tdc_ut_cm3_current_count=TDC_UT_CM3_CURRENT_MAX;
    current_put(42,41);
    assert(g_tdc_ut_cm3_current_count==TDC_UT_CM3_CURRENT_MAX);
    assert(g_tdc_ut_cm3_current_overflow==1);
    assert(g_tdc_ut_cm3_current_failed==1);
    puts("CM3: 1 unit, repeated run, mismatch, overflow and invalid-frame guard checked on host");
    return 0;
}
"""


def main():
    QA.mkdir(parents=True, exist_ok=True)
    gcc = shutil.which("gcc")
    if not gcc:
        raise RuntimeError("Existing host gcc is unavailable")
    agc_path = ROOT / "src/1__cfx/signalProcessing/agc.c"
    strategy_path = ROOT / "src/1__cfx/signalProcessing/stimulationStrategy.c"
    para_path = ROOT / "src/2__cm3/Cortex-M3-src/signalProcessing/stimulationParaCal.c"
    agc = agc_path.read_text(encoding="utf-8-sig")
    strategy = strategy_path.read_text(encoding="utf-8-sig")
    para = para_path.read_text(encoding="utf-8-sig")
    bodies = "\n".join(function(agc, n) for n in ["audio_agc_attack_release", "apply_agc_gain"])
    bodies = re.sub(r"\blong\b", "long long", bodies)
    bodies += "\n" + "\n".join(function(strategy, n) for n in [
        "stimulationStrategy_NofM_Init_Buffers", "stimulationStrategy_NofM_Peak_Pick", "stimulationStrategy_NofM_Interleaving"])
    current_cfx = RUNNERS["cfx"].read_text(encoding="utf-8-sig").split("#define TDC_UT_CFX_CURRENT_MAX", 1)[1]
    current_cfx = "#define TDC_UT_CFX_CURRENT_MAX" + current_cfx.rsplit("#endif", 1)[0]
    frame_body = function(para, "calculationNumFramePerOneChannle")
    transfer_body = function(para, "calculationTransferableChanneNum")
    # 원본의 비활성 #if 0 안에서는 사용하고 활성 경로에서는 쓰지 않는 인자다.
    transfer_body = transfer_body.replace("{", "{\n(void)usableElectrodNum;", 1)
    current_cm3 = RUNNERS["cm3"].read_text(encoding="utf-8-sig").split("#define TDC_UT_CM3_CURRENT_MAX", 1)[1]
    current_cm3 = "#define TDC_UT_CM3_CURRENT_MAX" + current_cm3.rsplit("#endif", 1)[0]
    cm3_source = CM3_PREFIX + frame_body + "\n" + transfer_body + "\n" + current_cm3 + CM3_MAIN
    results = {"cfx": compile_run(CFX_PREFIX + bodies + "\n" + current_cfx + CFX_MAIN, "cfx", gcc),
               "cm3": compile_run(cm3_source, "cm3", gcc, ["-Wno-unused-variable"])}
    fault_frame = frame_body.replace("calculationNumFramePerOneChannle", "real_num_frame", 1)
    fault_frame += "\nint calculationNumFramePerOneChannle(int pw) {return pw==13 ? 0 : real_num_frame(pw);}\n"
    results["cm3_invalid_frame"] = compile_run("\n".join([CM3_PREFIX, fault_frame, transfer_body, current_cm3, CM3_MAIN]),
                                               "cm3-invalid-frame", gcc, ["-DTDC_UT_HOST_FAULT", "-Wno-unused-variable"])
    legacy = RUNNERS["cfx"].read_text(encoding="utf-8-sig")
    capacity = re.search(r"^#define TDC_UT_RESULT_MAX\s+(\d+)", legacy, re.M).group(1)
    legacy_prefix = ("#include <assert.h>\n#include <stdio.h>\n#define _XMEM\n"
                     "#define TDC_UT_RESULT_MAX " + capacity + "\n"
                     "int tdc_ut_result[TDC_UT_RESULT_MAX],tdc_ut_count,g_tdc_ut_cfx_legacy_overflow;\n")
    legacy_main = r"""
int main(void) {
    int i;
    for(i=0;i<284;i++) tdc_ut_put(i);
    assert(tdc_ut_count==284 && g_tdc_ut_cfx_legacy_overflow==0);
    for(i=284;i<TDC_UT_RESULT_MAX;i++) tdc_ut_put(i);
    assert(tdc_ut_count==TDC_UT_RESULT_MAX);
    tdc_ut_put(12345);
    assert(tdc_ut_count==TDC_UT_RESULT_MAX && g_tdc_ut_cfx_legacy_overflow==1);
    assert(tdc_ut_result[TDC_UT_RESULT_MAX-1]==TDC_UT_RESULT_MAX-1);
    puts("Legacy result writer: 284 values fit; capacity boundary and overflow indicator checked on host");
    return 0;
}
"""
    results["legacy_capacity"] = compile_run("\n".join([legacy_prefix, function(legacy, "tdc_ut_put"), legacy_main]),
                                             "legacy-capacity", gcc)
    disabled = {}
    nm = shutil.which("nm")
    if not nm:
        raise RuntimeError("Existing host nm is unavailable")
    for core, path in RUNNERS.items():
        obj = QA / (core + "-disabled.o")
        subprocess.run([gcc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-c", str(path), "-o", obj.name], cwd=QA, check=True)
        symbols = subprocess.run([nm, "--defined-only", obj.name], cwd=QA, check=True, text=True,
                                 encoding="utf-8", errors="replace", capture_output=True).stdout
        if "tdc_" in symbols:
            raise AssertionError("test symbols present without TDC_UNIT_TEST: " + core)
        disabled[core] = {"symbols": symbols.strip(), "test_symbols": 0}
        path.read_text(encoding="utf-8-sig").encode("cp949")
    result = {"scope": "host logic/runner checks only; not target build or hardware execution",
              "runs": results, "disabled_build": disabled,
              "sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in [*RUNNERS.values(), agc_path, strategy_path, para_path]}}
    criteria_path = ROOT / "docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx"
    criteria_hash = hashlib.sha256(criteria_path.read_bytes()).hexdigest()
    if criteria_hash != "c5d89e3d8dc8a98fbe76940e1fff293108b5dc012d65c2db454f6a9873f6963a":
        raise AssertionError("Criteria were externally changed; recheck current vectors before claiming alignment")
    result["criteria_sha256"] = criteria_hash
    result["current_supported_units"] = ["CFX-024", "CFX-025", "CFX-034", "CFX-035", "CM3-027"]
    result["not_target_verified"] = True
    (QA / "checks.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
