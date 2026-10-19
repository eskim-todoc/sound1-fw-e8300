"""Bounded source checks for six current SUVC analysis-method units.

This checks reviewed statements and call order, not C execution, a complete
preprocessor/AST, peripheral behavior, timing, or whole-unit PASS. Comments,
literals and known inactive #if 0 / TDC_UNIT_TEST branches cannot satisfy a
check. Unknown build-condition branches remain visible and are a stated limit.
Only the sibling coverage JSON is written. Source and criterion are read-only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
CRITERION = ROOT / (
    "docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/"
    "SUVC-FW-TD24A 소프트웨어 유닛 검증 기준서.xlsx"
)
CRITERION_SHA = "89967fa692c4f51e3be916c73270d75859b4f692c96928c2d73da0cc6c5096fd"
SHEET = "기준서_유닛 검증"
OUTPUT = Path(__file__).with_name("remaining_analysis_coverage.json")
SELECTED = {("CM3", n) for n in (1, 3, 5, 8, 15)} | {("CFX", 14)}
PATHS = {
    "init": "src/2__cm3/Cortex-M3-src/systemControl/initialize.c",
    "spi": "src/2__cm3/Cortex-M3-src/systemControl/driver_SPI.c",
    "spi_h": "src/2__cm3/Cortex-M3-src/systemControl/driver_SPI.h",
    "i2c": "src/2__cm3/Cortex-M3-src/systemControl/driver_i2c.c",
    "main": "src/2__cm3/Cortex-M3-src/main.c",
    "qcc": "src/2__cm3/Cortex-M3-src/QCC/snd_qcc.c",
    "qcc_h": "src/2__cm3/Cortex-M3-src/QCC/snd_qcc.h",
    "dio": "src/2__cm3/Gen1_5/common/ci_dio.c",
    "power": "src/2__cm3/Gen1_5/common/ci_power.c",
    "audio": "src/1__cfx/lib_cfx/lib_audio_in.c",
    "fifo_h": "src/1__cfx/hear/microcode.h",
    "cfx_main": "src/1__cfx/systemControl/main.c",
}
UNIT_PATH_KEYS = {
    ("CM3", 1): ["init", "main", "i2c", "spi", "spi_h"],
    ("CM3", 3): ["qcc", "qcc_h", "dio", "init"],
    ("CM3", 5): ["main"],
    ("CM3", 8): ["qcc"],
    ("CM3", 15): ["main", "init", "i2c", "power", "cfx_main"],
    ("CFX", 14): ["audio", "fifo_h"],
}
LEXICAL = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def mask_comments_literals(text: str) -> str:
    """Keep offsets and line numbers while hiding non-code tokens."""
    return LEXICAL.sub(lambda m: re.sub(r"[^\r\n]", " ", m.group()), text)


def normal_source(text: str) -> str:
    """Resolve only literal 0/1 and test-macro branches, not arbitrary macros."""
    text = mask_comments_literals(text)
    stack: list[list] = []
    enabled = True
    lines = []
    for line in text.splitlines(keepends=True):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if directive:
            kind, expr = directive.group(1), re.sub(r"\s+", "", directive.group(2))
            truth = {"0": False, "1": True, "defined(TDC_UNIT_TEST)": False,
                     "definedTDC_UNIT_TEST": False, "!defined(TDC_UNIT_TEST)": True}.get(expr)
            if kind in ("ifdef", "ifndef"):
                truth = (kind == "ifndef") if expr == "TDC_UNIT_TEST" else None
            if kind in ("if", "ifdef", "ifndef"):
                stack.append([enabled, truth, truth is True])
                enabled = enabled and truth is not False
            elif kind == "else":
                if not stack:
                    raise ValueError("unmatched #else")
                parent, known, taken = stack[-1]
                enabled = parent and (known is None or not taken)
                stack[-1][2] = True
            elif kind == "elif":
                if not stack:
                    raise ValueError("unmatched #elif")
                parent, known, taken = stack[-1]
                enabled = parent and (known is None or (not taken and truth is not False))
                if known is not None:
                    stack[-1][1] = truth
                    stack[-1][2] = taken or truth is True
            else:
                if not stack:
                    raise ValueError("unmatched #endif")
                enabled = stack.pop()[0]
            lines.append(re.sub(r"[^\r\n]", " ", line))
        else:
            lines.append(line if enabled else re.sub(r"[^\r\n]", " ", line))
    if stack:
        raise ValueError("unclosed conditional directive")
    return "".join(lines)


def closing(text: str, start: int, left: str, right: str) -> int:
    depth = 0
    for pos in range(start, len(text)):
        if text[pos] == left:
            depth += 1
        elif text[pos] == right:
            depth -= 1
            if depth == 0:
                return pos
    raise ValueError("unclosed " + left)


def function_span(text: str, name: str) -> tuple[str, int]:
    """Accept typedef/pointer return types and skip prototypes."""
    pattern = (r"(?m)^[ \t]*(?:(?:static|inline|extern)\s+)*"
               r"(?:[A-Za-z_]\w*(?:[ \t]+|[ \t]*\*+[ \t]*))+"
               + re.escape(name) + r"[ \t]*\(")
    for match in re.finditer(pattern, text):
        end_args = closing(text, match.end() - 1, "(", ")")
        body_start = end_args + 1
        while body_start < len(text) and text[body_start].isspace():
            body_start += 1
        if body_start < len(text) and text[body_start] == "{":
            return text[body_start:closing(text, body_start, "{", "}") + 1], body_start
    raise ValueError("missing definition: " + name)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


def ordered(text: str, tokens: list[str]) -> bool:
    cursor = 0
    for token in tokens:
        found = text.find(token, cursor)
        if found < 0:
            return False
        cursor = found + len(token)
    return True


def case_arms(body: str, variable: str) -> dict[str, str]:
    body = compact(body)
    match = re.search(r"switch\(" + re.escape(variable) + r"\)\{", body)
    if not match:
        raise ValueError("missing switch: " + variable)
    end = closing(body, match.end() - 1, "{", "}")
    switch = body[match.end():end]
    labels = list(re.finditer(r"case([A-Za-z_]\w*):|default:", switch))
    return {(m.group(1) or "default"): switch[m.end():labels[i + 1].start() if i + 1 < len(labels) else len(switch)]
            for i, m in enumerate(labels)}


def gpio_mapping(body: str, variable: str, high: str, low: str, pin: str) -> bool:
    arms = case_arms(body, variable)
    wanted = {high: "High", low: "Low", "default": "Low"}
    if set(arms) != set(wanted):
        return False
    for key, level in wanted.items():
        arm = arms[key]
        calls = re.findall(r"Sys_GPIO_Set_(High|Low)\(([^)]*)\);", arm)
        if calls != [(level, pin)] or "break;" not in arm:
            return False
    return True


def self_checks() -> dict:
    """Negative controls use strings in memory, never mutate product source."""
    sample = ('void x(void);\nstatic status_t x(void) {\n'
              '// fake(); }\nchar *s = "fake(); }";\n'
              '#if 0\nfake();\n#else\nreal();\n#endif\n'
              '#if defined(TDC_UNIT_TEST)\nfake();\n#endif\nreturn 0;\n}\n')
    source = normal_source(sample)
    body, _ = function_span(source, "x")
    checks = {
        "offsets_and_newlines_preserved": len(source) == len(sample) and source.count("\n") == sample.count("\n"),
        "prototype_skipped_and_typedef_return_supported": "return 0;" in body,
        "comments_literals_and_inactive_test_hidden": "fake();" not in body,
        "literal_else_selected": "real();" in body,
        "missing_order_token_rejected": not ordered("alpha();gamma();", ["alpha();", "beta();"]),
        "reversed_order_rejected": not ordered("beta();alpha();", ["alpha();", "beta();"]),
    }
    mapping = "switch(state){case YES:Sys_GPIO_Set_High(PIN);break;case NO:Sys_GPIO_Set_Low(PIN);break;default:Sys_GPIO_Set_Low(PIN);break;}"
    checks["correct_gpio_mapping_accepted"] = gpio_mapping(mapping, "state", "YES", "NO", "PIN")
    checks["wrong_default_gpio_rejected"] = not gpio_mapping(mapping.replace("default:Sys_GPIO_Set_Low", "default:Sys_GPIO_Set_High"), "state", "YES", "NO", "PIN")
    if not all(checks.values()):
        raise RuntimeError("analysis checker self-check failed: " + repr(checks))
    return checks


def read_criterion() -> tuple[dict, str]:
    import openpyxl
    digest = hashlib.sha256(CRITERION.read_bytes()).hexdigest()
    if digest != CRITERION_SHA:
        raise ValueError("criterion changed; review current six rows before updating this check")
    workbook = openpyxl.load_workbook(CRITERION, read_only=True, data_only=True)
    units = {}
    try:
        for row in workbook[SHEET].iter_rows():
            values = [cell.value for cell in row[:11]]
            match = re.fullmatch(r"SW-FW-UnitTest-(CM3|CFX)-ID-(\d+)", str(values[3]))
            if match:
                key = match.group(1), int(match.group(2))
                if key in SELECTED:
                    if key in units or values[5] != "A":
                        raise ValueError("duplicate unit or method no longer A: " + str(key))
                    units[key] = {
                        "core": key[0], "unit": key[1], "name": values[4], "method": values[5],
                        "test_id": values[3], "su_id": values[6], "row": row[0].row,
                        "criterion_range": f"B{row[0].row}:K{row[0].row}",
                        "input": values[7], "limitations": values[8],
                        "procedure": values[9], "expected": values[10],
                        "status": "ANALYSIS_SOURCE_CHECKS", "checks": [], "criterion_issues": [],
                        "device_execution": False, "whole_unit_pass": False,
                    }
    finally:
        workbook.close()
    if set(units) != SELECTED:
        raise ValueError("missing current criterion units: " + repr(SELECTED - set(units)))
    return units, digest


def run_checks(units: dict, clean: dict) -> None:
    def body(file: str, name: str) -> str:
        return compact(function_span(clean[file], name)[0])

    def check(core: str, unit: int, name: str, file: str, symbol: str,
              result: bool, expected: str, observed: str = "") -> None:
        line = 1 if symbol == "<file>" else clean[file][:function_span(clean[file], symbol)[1]].count("\n") + 1
        units[core, unit]["checks"].append({
            "name": name, "matched": bool(result), "path": PATHS[file], "symbol": symbol,
            "definition_line": line, "expected_source_fact": expected,
            "observed_source_fact": observed or (expected if result else "required source structure not found"),
        })

    init = body("init", "Initialize")
    boot = body("main", "tdc_normal_boot_sequence")
    spi = body("spi", "init_cm3_SPI")
    dma = body("spi", "ci_SPI_enable_DMA")
    check("CM3", 1, "initialization_before_QCC_request", "init", "Initialize",
          ordered(init, ["reset_DMA_disable();", "init_I2c();", "init_cm3_SPI();", "snd_qcc_set_mode(SND_QCC_MODE_NORMAL);"]),
          "DMA reset, I2C and SPI initialization precede QCC normal request")
    check("CM3", 1, "software_permission_after_Initialize", "main", "tdc_normal_boot_sequence",
          ordered(boot, ["tdc_wait_for_cfx_start();", "Initialize();", "is_enabled_CFX_iteration=1;"]),
          "CFX start wait and complete Initialize precede shared CFX iteration permission")
    check("CM3", 1, "IRQ_enable_separate_from_CFX_permission", "init", "Initialize",
          ordered(init, ["enable_CFX_trigger_for_iteration();", "init_cm3_SPI();"])
          and "is_enabled_CFX_iteration=1;" not in init,
          "CM3 CFX/FIFO IRQ enable is earlier than SPI init, but is not shared CFX iteration permission")
    check("CM3", 1, "I2C_reset_config_clear_before_enable", "i2c", "init_I2c",
          ordered(body("i2c", "init_I2c"), ["Sys_I2C_Reset(I2C0);", "Sys_I2C_Config(I2C0,CM3_I2C_CFG_VAL_AsMaster);", "clearI2cDriverStatus();", "enableI2cInterface(true);"]),
          "I2C reset/configuration/software state clear precede I2C enable")
    reset = body("init", "reset_DMA_disable")
    check("CM3", 1, "all_eight_DMA_channels_reset", "init", "reset_DMA_disable",
          all(f"Sys_DMA_Mode_Enable(DMA{n},DMA_DISABLE);" in reset and
              f"Sys_DMA_Clear_Status(DMA{n},(DMA_COMPLETE_INT_CLEAR|DMA_CNT_INT_CLEAR));" in reset for n in range(8)),
          "DMA0..7 are disabled and completion/count interrupt status cleared")
    check("CM3", 1, "SPI_buffer_DMA_before_enable", "spi", "init_cm3_SPI",
          ordered(spi, ["Sys_SPI_DIOConfig(SPI1,SPI_SELECT_SLAVE,SPI_DIO_PIN_CFG,NRF_SPI_CLK_PIN,NRF_SPI_CS_PIN,NRF_SPI_MOSI_PIN,NRF_SPI_MISO_PIN);", "Sys_SPI_TransferConfig(SPI1,DRIVER_SPI_CTRL_DISABLE);", "Sys_SPI_Config(SPI1,DRIVER_SPI_CONFIG);", "SPI_Tx_Buffer[i]=1+i;", "ci_SPI_enable_DMA();", "Sys_SPI_TransferConfig(SPI1,DRIVER_SPI_CTRL_ENABLE);"]),
          "SPI pins/disable/config/TX buffer initialization/DMA preparation precede SPI enable")
    dma_tokens = ["Sys_DMA_Mode_Enable(DMA0,DMA_DISABLE);", "Sys_DMA_ChannelConfig(DMA0,DRIVER_DMA0_CFG0,SPI_COMM_PACKET_SIZE,0,0x40000E10,((uint32_t)SPI_Rx_Buffer));", "Sys_DMA_Set_Ctrl(DMA0,DRIVER_DMA0_CTRL);", "Sys_DMA_Clear_Status(DMA0,DRIVER_DMA0_STATUS);", "Sys_DMA_Mode_Enable(DMA1,DMA_DISABLE);", "Sys_DMA_ChannelConfig(DMA1,DRIVER_DMA1_CFG0,SPI_COMM_PACKET_SIZE,0,((uint32_t)SPI_Tx_Buffer),0x40000E0C);", "Sys_DMA_Set_Ctrl(DMA1,DRIVER_DMA1_CTRL);", "Sys_DMA_Clear_Status(DMA1,DRIVER_DMA1_STATUS);", "Sys_DMA_Mode_Enable(DMA0,DMA_ENABLE);", "Sys_DMA_Mode_Enable(DMA1,DMA_ENABLE);"]
    check("CM3", 1, "DMA_direction_length_configuration_order", "spi", "ci_SPI_enable_DMA",
          ordered(dma, dma_tokens), "RX DMA0: SPI read register to RX buffer; TX DMA1: TX buffer to SPI write register, packet-sized, configured before enable")
    spi_header = compact(clean["spi_h"])
    check("CM3", 1, "SPI_DMA_and_interrupt_configuration_tokens", "spi_h", "<file>",
          all(token in spi_header for token in ["SPI_SELECT_SLAVE", "SPI_WORD_SIZE_8", "SPI_CS_RISE_INT_ENABLE", "SPI_RX_DMA_ENABLE", "SPI_TX_DMA_ENABLE", "#defineSPI_COMM_PACKET_SIZE(BLE_DataPacketSize+1)"]),
          "SPI header requests slave, 8-bit words, CS-rise interrupt, RX/TX DMA and packet length BLE payload + 1")
    irq = body("init", "enable_CFX_trigger_for_iteration")
    check("CM3", 1, "CFX_FIFO_IRQ_configuration_order", "init", "enable_CFX_trigger_for_iteration",
          all(ordered(irq, [f"NVIC_ClearPendingIRQ({name});", f"NVIC_SetPriority({name},3);", f"NVIC_EnableIRQ({name});"]) for name in ("CFX_0_IRQn", "FIFO_5_IRQn")),
          "CFX_0 and FIFO_5 IRQ pending clear and priority 3 precede enable")

    qcc_init = body("qcc", "snd_qcc_init")
    check("CM3", 3, "two_GPIO_outputs_before_initial_values", "qcc", "snd_qcc_init",
          ordered(qcc_init, ["Sys_DIO_Config(SND_QCC_DIO_QCC_CTRL,SND_QCC_DIO_CFG_GPIO_OUT);", "Sys_DIO_Config(SND_QCC_DIO_ISD_CHECK,SND_QCC_DIO_CFG_GPIO_OUT);", "snd_qcc_set_isd(SND_QCC_ISD_DISCONNECTED);", "snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);"]),
          "QCC_CTRL/ISD_CHECK configured as outputs before disconnected/shutdown setters")
    for unit in (3, 8):
        check("CM3", unit, "mode_GPIO_truth_table", "qcc", "snd_qcc_set_mode",
              gpio_mapping(body("qcc", "snd_qcc_set_mode"), "mode", "SND_QCC_MODE_NORMAL", "SND_QCC_MODE_SHUTDOWN", "SND_QCC_DIO_QCC_CTRL"),
              "QCC_CTRL High only for NORMAL, Low for SHUTDOWN and default")
        check("CM3", unit, "ISD_GPIO_truth_table", "qcc", "snd_qcc_set_isd",
              gpio_mapping(body("qcc", "snd_qcc_set_isd"), "state", "SND_QCC_ISD_CONNECTED", "SND_QCC_ISD_DISCONNECTED", "SND_QCC_DIO_ISD_CHECK"),
              "ISD_CHECK High only for CONNECTED, Low for DISCONNECTED and default")
    check("CM3", 3, "pin_definitions_and_initialization_reachable", "dio", "ci_dio_configure_normal",
          "snd_qcc_init();" in body("dio", "ci_dio_configure_normal") and "ci_dio_configure_normal();" in init
          and all(t in compact(clean["qcc_h"]) for t in ["#defineSND_QCC_DIO_QCC_CTRLDIO24", "#defineSND_QCC_DIO_ISD_CHECKDIO13", "DIO_MODE_GPIO_OUT"]),
          "Initialize calls normal DIO initialization, which calls snd_qcc_init; current pin definitions are DIO24/DIO13")

    normal = body("main", "func_normal")
    iteration = body("main", "tdc_normal_iteration")
    handle = body("main", "tdc_handle_events")
    check("CM3", 5, "tick_gate_and_consumption", "main", "func_normal",
          "if((iterationFlag==true)){tdc_normal_iteration(&ctx);}" in normal
          and normal.count("tdc_normal_iteration(") == 1,
          "normal iteration processing is called once at its iterationFlag==true gate")
    check("CM3", 5, "collect_before_handle_before_disable", "main", "tdc_normal_iteration",
          ordered(iteration, ["tdc_collect_events(&ev);", "tdc_handle_events(&ev,ctx);", "disable_iteration();"])
          and "iterationFlag=false;" in body("main", "disable_iteration"),
          "collect inputs, handle inputs, then consume iteration flag")
    check("CM3", 5, "control_map_ISD_BLE_order", "main", "tdc_handle_events",
          ordered(handle, ["ctx->systemState=systemControl(", "update_mapNum();", "ctx->isd_state=isd_interface(", "ctx->ble_state=bleCommunication("]),
          "systemControl, map update, isd_interface, bleCommunication in that order")
    collect = body("main", "tdc_collect_events")
    check("CM3", 5, "touch_and_timeout_inputs_collected_once", "main", "tdc_collect_events",
          all(collect.count(t) == 1 for t in ["readErrorCode()", "snd_charger_get_state()", "snd_batt_get_percent()", "tdc_touch_process()", "tdc_qcc_has_batt_level_rx_timed_out()"]),
          "each error/charger/battery/touch/timeout input read appears once per input collection")
    check("CM3", 5, "normal_exit_is_sleep_guarded", "main", "func_normal",
          "if(ctx.systemState.systemOff==true){if(tdc_can_enter_sleep(ctx.ble_state.mappingConnection)){led_force_fade_off();break;}ctx.systemState.systemOff=false;}" in normal
          and ordered(body("main", "main"), ["while(1)", "func_normal();", "func_sleep();"]),
          "accepted systemOff exits normal loop to sleep; rejected sleep request resets systemOff")

    sleep = body("main", "func_sleep")
    flag = "cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX"
    wait = "while(1){if(" + flag + "==0){ci_printd();break;}}"
    check("CM3", 15, "CFX_clear_wait_before_low_clock", "main", "func_sleep",
          ordered(sleep, [flag + "=1;", wait, "ci_power_sleep();", "i2c_set_master_prescale(I2C_MASTER_PRESCALE_6);", "ci_timer_init_prescaled(TDC_TOUCH_ULP_TIMER_PRESCALE,TDC_TOUCH_ULP_TIMER_TIMEOUT_VALUE);"]),
          "request=1; wait until CFX clears request to 0; only then lower clock and configure sleep I2C/timer")
    check("CM3", 15, "CFX_ack_after_cleanup", "cfx_main", "standby",
          ordered(body("cfx_main", "standby"), ["lib_disable_PCM(", "lib_i2s_disable(", "disable_DMIC(", "disable_FIFO_all(", "SYS_HEAR_PAUSE", "app_configure_interrupts(CFX_INT_STANDBY);", "enter_ULP_mode_Command_CM3_to_CFX=0;", "SYS_WAIT_FOR_INTERRUPT;"]),
          "CFX standby clears ULP request after input/output/FIFO/HEAR cleanup and standby interrupt configuration")
    check("CM3", 15, "sleep_clock_and_I2C_idle_boundary", "power", "ci_power_sleep",
          "tdc_Trims_SetOperatingFrequency(SYS_FREQ_2M56);" in body("power", "ci_power_sleep")
          and ordered(body("i2c", "i2c_set_master_prescale"), ["while(!isI2cDriverStatusIdle())", "enableI2cInterface(false);", "I2C0->CFG=cfg;", "enableI2cInterface(true);"]),
          "sleep selects SYS_FREQ_2M56; I2C prescale update waits for idle and disables/re-enables I2C")
    check("CM3", 15, "wake_monitor_and_touch_reboot", "main", "func_sleep",
          ordered(sleep, ["SYS_WAIT_FOR_INTERRUPT;", "tdc_touch_iqs323_read_status(&st);", "if(sleep_ignore)", "tdc_touch_sleep_handle_notouch_gate(", "else{tdc_touch_sleep_handle_touch_reboot("])
          and "if(ok&&state==TDC_TOUCH_STATE_TOUCH){(*touch_cnt)++;if(*touch_cnt>=TDC_TOUCH_ULP_REBOOT_TOUCH_CNT){" in body("main", "tdc_touch_sleep_handle_touch_reboot")
          and "SYS_WATCHDOG_RESET();" in body("main", "tdc_touch_sleep_handle_touch_reboot"),
          "sleep waits for interrupts, reads touch, gates initial no-touch state, then sustained valid touch triggers watchdog reset")
    check("CM3", 15, "normal_recovery_is_reinitialization_not_fallthrough", "main", "main",
          ordered(body("main", "main"), ["load_data_section();", "load_bss_section();", "func_normal();"])
          and "ci_util_assert(ci_power_normal());" in init and "init_I2c();" in init and "init_cm3_SPI();" in init,
          "wake reset re-enters main/normal Initialize, restoring power and communication configuration; func_sleep does not return normally")
    criterion_input = units["CM3", 15]["input"]
    criterion_expected = units["CM3", 15]["expected"]
    assert "enter_ULP_mode_Command_CM3_to_CFX" in criterion_input
    assert "1(요청/정리 대기)" in criterion_input and "0(CFX 정리 후 요청 해제)" in criterion_input
    assert "1인 동안" in criterion_expected and "0으로 해제" in criterion_expected

    clear = body("audio", "clear_FIFO")
    all_clear = body("audio", "clear_FIFO_all")
    fifos = [(f"A0_{i}", 64 if i == 4 else 32) for i in range(7)] + [("A1_0", 512)]
    check("CFX", 14, "selected_FIFO_bounded_zero_write", "audio", "clear_FIFO",
          "for(inti=0;i<len;i++)chess_loop_range(1,HEAR_LARGEST_FIFO_SIZE){p_fifo[i]=0;}" in clear
          and re.findall(r"[A-Za-z_]\w*\[[^]]*\]=[^;]*;", clear) == ["p_fifo[i]=0;"],
          "selected FIFO function writes zero only through p_fifo[0..len-1], with positive valid length/pointer as caller precondition")
    expected_calls = [("HCT_" + name, "HCT_FIFO_" + name + "_LENGTH") for name, _ in fifos]
    check("CFX", 14, "complete_FIFO_target_list", "audio", "clear_FIFO_all",
          re.findall(r"clear_FIFO\(([^,]+),([^)]*)\);", all_clear) == expected_calls,
          "all FIFO clear calls exactly A0_0..A0_6 and A1_0 with corresponding declared lengths")
    header = compact(clean["fifo_h"])
    check("CFX", 14, "declared_FIFO_lengths", "fifo_h", "<file>",
          all(f"#defineHCT_FIFO_{name}_LENGTH({length})" in header for name, length in fifos),
          "current FIFO lengths: 32/32/32/32/64/32/32/512 words")
    check("CFX", 14, "FIFO_zeroing_is_not_hardware_reset", "audio", "clear_FIFO",
          not any(t in clear + all_clear for t in ("Sys_FIFO_Configure(", "Sys_FIFO_Reset(", "D_FIFO_")),
          "clear_FIFO and clear_FIFO_all clear backing words, not FIFO hardware pointers/configuration")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stdout", action="store_true", help="print coverage JSON instead of updating sibling result")
    args = parser.parse_args()
    controls = self_checks()
    units, criterion_hash = read_criterion()
    raw = {key: (ROOT / path).read_bytes() for key, path in PATHS.items()}
    clean = {key: normal_source(data.decode("utf-8-sig")) for key, data in raw.items()}
    run_checks(units, clean)
    for key, unit in units.items():
        unit["scope"] = "Reviewed source statements/call order only; no C execution, runtime trace, timing or whole-unit/device PASS."
        unit["limits"] = [
            "This is not a full C preprocessor/AST or control-flow proof; unknown build-condition branches remain visible.",
            "Hardware state, register bit encoding, pin levels and interrupt races are not measured.",
            "Matching selected checks does not validate every condition in the criterion.",
        ]
        if key == ("CFX", 14):
            unit["limits"].append("No FIFO memory is written here. Zeroing [1,2,3,4] is source reasoning, not an executed FIFO reset; pointer/length validity, FIFO boundaries/overlap and concurrent access remain untested.")
        if key == ("CM3", 15):
            unit["limits"].append("Watchdog wake/reboot and normal configuration restoration are source paths only; no physical wake, clock/SCL measurement or complete reset-chain execution.")
        unit["mismatch_count"] = sum(not c["matched"] for c in unit["checks"])
        unit["host_mismatch_count"] = None  # No host C runner is involved.
        unit["status"] = "SOURCE_CHECKS_MATCHED" if unit["mismatch_count"] == 0 else "SOURCE_CHECK_MISMATCH"
        # A check may follow calls across files; retain every reviewed dependency,
        # not just the primary symbol displayed beside that check.
        unit["sources"] = sorted(PATHS[k] for k in UNIT_PATH_KEYS[key])
    stable = all(hashlib.sha256((ROOT / PATHS[key]).read_bytes()).hexdigest() == hashlib.sha256(data).hexdigest() for key, data in raw.items())
    criterion_stable = hashlib.sha256(CRITERION.read_bytes()).hexdigest() == criterion_hash
    records = [units[k] for k in sorted(units)]
    data = {
        "schema_version": 1, "kind": "bounded_source_analysis", "script": "tests/unit/check_remaining_analysis.py",
        "criterion": str(CRITERION.relative_to(ROOT)).replace("\\", "/"),
        "criterion_sha256": criterion_hash, "criterion_sheet": SHEET,
        "device_execution": False, "whole_unit_pass": False,
        "source_stable_during_run": stable, "criterion_stable_during_run": criterion_stable,
        "self_checks": controls, "units": records,
        "mismatch_count": sum(u["mismatch_count"] for u in records),
        "criterion_issue_count": sum(len(u["criterion_issues"]) for u in records),
        "sources": {PATHS[key]: hashlib.sha256(value).hexdigest() for key, value in raw.items()},
    }
    serialized = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    if args.stdout:
        print(serialized)
    else:
        OUTPUT.write_text(serialized, encoding="utf-8")
        for unit in records:
            print(f'{unit["core"]}-{unit["unit"]:03d}: {unit["status"]} checks={len(unit["checks"])} mismatches={unit["mismatch_count"]}')
        print(f'criterion_issues={data["criterion_issue_count"]}; device_execution=false; whole_unit_pass=false')
    return 0 if data["mismatch_count"] == 0 and stable and criterion_stable else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, RuntimeError, OSError) as exc:
        print("ANALYSIS_CHECK_ERROR: " + str(exc), file=sys.stderr)
        sys.exit(2)
