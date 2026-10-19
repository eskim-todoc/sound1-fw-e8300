"""Reproducible source checks for analysis-only CFX units.

These are source/order checks, not emulation of peripherals or event timing.
The original review inputs/expectations remain in the current unit criterion.
"""
import hashlib
import json
from pathlib import Path
import re
from check_cfx_vectors import function

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/unit-tests/cfx-analysis"
PATHS = {
    "main": "src/1__cfx/systemControl/main.c",
    "interrupts": "src/1__cfx/systemControl/interrupt_service_routine.c",
    "audio": "src/1__cfx/lib_cfx/lib_audio_in.c",
    "pcm_init": "src/1__cfx/lib_cfx/lib_PCM.c",
    "i2s": "src/1__cfx/lib_cfx/lib_i2s.c",
    "pcm": "src/1__cfx/internalDevice/driver_PCM.c",
    "backtel": "src/1__cfx/internalDevice/driver_PCM_liveStimulation.c",
    "strategy": "src/1__cfx/signalProcessing/stimulationStrategy.c",
    "hear": "src/3__hear/microcode.hct",
}


def main():
    texts = {k: (ROOT / p).read_text(encoding="utf-8-sig") for k, p in PATHS.items()}
    # Strip comments before testing tokens, so obsolete comments cannot pass.
    clean = {k: re.sub(r"/\*.*?\*/|//[^\n]*", "", t, flags=re.S) for k, t in texts.items()}
    records = []
    def body(file, name):
        return function(clean[file], name)
    def ordered(text, *tokens):
        positions = [text.index(token) for token in tokens]
        assert positions == sorted(positions) and len(set(positions)) == len(positions), tokens
    def record(unit, sources, fact):
        records.append({"unit": unit, "sources": [PATHS[k] for k in sources], "checked_fact": fact,
                        "scope": "source tokens/order only; not whole-unit/device PASS"})
    ordered(body("main", "main"), "set_saturation_mode(1)", "is_enabled_CFX_iteration == 1", "normal();")
    assert "function_chain0 = 0" in body("interrupts", "app_clear_all_interrupt_flags")
    record(1, ["main", "interrupts"], "CM3 permission gates normal path; interrupt clear and per-event clear paths exist")
    ordered(body("main", "normal_init"), "SYS_HEAR_PAUSE", "SYS_HEAR_RESET", "SYS_HEAR_FUNCTIONCHAIN_ENABLE", "SYS_HEAR_START")
    ordered(clean["hear"], "StartFunctionChain(CMD0", "vAbsolute", "MaxElement", "Event(0)", "StartFunctionChain(CMD1", "Win_DFT_R_Forward", "vMag", "Event(1)")
    record(2, ["main", "hear"], "HEAR reset/config/start and CMD0/1 event assignments")
    assert "s_enabled_dmic_cnt = 1" in body("audio", "tdc_enable_1_DMIC")
    assert "s_enabled_dmic_cnt = 2" in body("audio", "tdc_enable_2_DMICs")
    record(3, ["audio", "main"], "SINGLE/DUAL bookkeeping and separate DIO configuration")
    ordered(body("pcm_init", "lib_init_PCM"), "PCM_RESET", "LIB_PCM_DIOConfig", "Sys_PCM_Config")
    ordered(body("main", "normal_init"), "lib_init_PCM", "lib_enable_PCM")
    record(4, ["pcm_init", "main"], "PCM reset/pin/config precedes output enable")
    ordered(body("i2s", "lib_init_I2S"), "PCM_RESET", "Sys_PCM_Config", "lib_g_i2s_state = LIB_I2S_STATE_DISABLED")
    ordered(body("main", "normal_init"), "lib_init_I2S", "lib_enable_I2S")
    record(5, ["i2s", "main"], "I2S dedicated initialization precedes enable")
    assert "is_pcm_specific_command_reading = 0" in body("main", "normal_init")
    assert "pcm_specific_command_read_index = 0" in body("main", "normal_init")
    record(6, ["main"], "Saturation and two command-state initial values set before normal processing")
    ordered(body("main", "standby"), "lib_disable_PCM", "SYS_SET_OUTPUT_CTRL", "lib_i2s_disable", "disable_DMIC", "disable_FIFO_all", "SYS_HEAR_PAUSE", "enter_ULP_mode_Command_CM3_to_CFX = 0", "SYS_WAIT_FOR_INTERRUPT")
    record(9, ["main"], "Output/input cleanup precedes completion clear and standby wait")
    dispatch = body("main", "PCM_LiveStimulation_Mode")
    for token in ["I2S_isStreaming()", "isMappingProgramConneted != 1", "is_i2s_source_cradle == 1", "LIB_AUDIO_DMIC_COUNT_SINGLE", "LIB_AUDIO_DMIC_COUNT_DUAL", "LIB_AUDIO_FRONT_MIC_LEFT", "LIB_AUDIO_FRONT_MIC_RIGHT"]:
        assert token in dispatch
    ordered(dispatch, "tdc_audio_mix_2_buffers_with_gain", "CALL_FUNCTION_CHAIN")
    record(21, ["main"], "Mapping/cradle/streaming/SINGLE/DUAL/left/right branches and post-mix CMD0")
    mode = body("pcm", "pcmDataOut")
    for token in ["PcmBitStream_Mode_Preamble", "PcmBitStream_Mode_LiveStimulation", "PcmBitStream_Mode_SepcificCommand", "PcmBitStream_Mode_NopStandby", "PcmBitStream_Mode_NopBacktel", "fn_PcmBitStream_Mode_FillZero"]:
        assert token in mode
    ordered(body("pcm", "fn_PcmBitStream_Mode_Preamble"), "PCM_PREAMBLE_STATE_FILL_ZERO", "PCM_PREAMBLE_STATE_TOGGLE", "pcm_Mold_Preamble", "PcmBitStream_Mode_NopStandby")
    record(38, ["pcm"], "Mode dispatch and two-step preamble state")
    ordered(body("main", "HEAR_LiveStimulation_Mode"), "function_chain0 == 1", "function_chain0 = 0", "audio_agc()", "update_FFT_inputData()", "CALL_FUNCTION_CHAIN", "function_chain1 == 1", "function_chain1 = 0", "pcmDataOut()")
    ordered(body("strategy", "stimulationStrategy"), "find_freq_rep_value();", "logarithmMapping();", "stimulationStrategy_CIS();")
    record(39, ["main", "strategy", "pcm"], "CMD0/AGC/FFT/CMD1 wait precedes strategy output; CIS band/mapping order")
    assert "pcm_Mold_NopStandby" in body("pcm", "fn_PcmBitStream_Mode_NopStandby")
    assert "pcm_Mold_NopBacktel" in body("pcm", "fn_PcmBitStream_Mode_NopBacktel")
    assert "TDC_UT_PCM_WRITE(p_pcmBuffer, 0)" in body("pcm", "fn_PcmBitStream_Mode_FillZero")
    assert "Reset_PCM = 0" in body("pcm", "fn_reset_PCM")
    record(41, ["pcm"], "Distinct NOP/backtel/zero patterns and reset request clear")
    ordered(body("backtel", "check_link_connection_state"), "case 0:", "Enable_backtelCircuit", "case 1:", "backtelPcmOut", "case 2:", "Disable_backtelCircuit", "case 4:", "shareConnectionCheckFired_DiableBacktel")
    assert "12 < g_pcmFrameNum_per_channel" in body("backtel", "shareConnectionCheckFired_DiableBacktel")
    record(42, ["backtel"], "Enable/transmit/disable/delayed completion; >12-frame branch excludes sent success")
    OUT.mkdir(parents=True, exist_ok=True)
    data = {"device_execution": False, "whole_unit_pass": False, "records": records,
            "sources": {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in PATHS.values()}}
    (OUT / "checks.json").write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    print("CFX ANALYSIS_SOURCE_CHECKS: " + ",".join(str(r["unit"]) for r in records))


if __name__ == "__main__":
    main()
