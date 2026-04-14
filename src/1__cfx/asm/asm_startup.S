/**
 * @file app_start.S
 * @brief Application start up file prior to main() execution
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

#include <sk5_asm.h>

// ----------------------------------------------------------------------------
// Application Version
// ----------------------------------------------------------------------------
#define APP_VERSION_MAJOR               0x01
#define APP_VERSION_MINOR               0x00
#define APP_VERSION_REVISION            0x00

#define APP_VERSION                     (APP_VERSION_MAJOR << 12    | \
                                         APP_VERSION_MINOR << 8     | \
                                         APP_VERSION_REVISION)

/* External definitions */
.undef global text _main
.undef global text _second_asm_segment
.undef global text second_asm

/** @brief Define the default status register value
 *  @details
 *  Turn off flag updates so that arithmetic instructions do not set status flags.
 *  Turn on saturation so that stored double-precision results are saturated to [-1, 1).
 *  Enable biased rounding and disable interrupts during initialization.
 */
#define DEFAULT_SR_VAL                  (SR_FLAG_UPD_DISABLE        | \
                                         SR_SATURATE_ENABLE         | \
                                         SR_ROUND_ENABLE            | \
                                         SR_INT_DISABLE             | \
                                         SR_BIAS_ENABLE)
#if TARGET == ISS
/* ----------------------------------------------------------------------------
 * Simulator start region
 * --------------------------------------------------------------------------*/
.text global .at [0] _sim_start
    goto start_app               // Simulator always starts here, jump to the
                                 // application start address from here
#endif

// ----------------------------------------------------------------------------
// Put the application version in PMEM (placed at D_PMEM_APP_BASE by the linker)
// ----------------------------------------------------------------------------
.data global 0 App_Version PMEM
    APP_VERSION

/**
 * @fn _main_init
 * @brief Minimal starting point for CFX application.
 * @registermods   None
 * @assumptions    None
 * @clockcycles    6
 */
// ----------------------------------------------------------------------------
// Program Start (placed after App_Version by the linker)
// ----------------------------------------------------------------------------
.text global 0 _main_init
start_app:

    /* Clear status register on the CFX to disable interrupts, saturation,
     * rounding, etc. */
    load sr, DEFAULT_SR_VAL

    /* Start C code execution */
    call.1d _main
    Sys_CFX_InitStack(_sp_start_value_XMEM)   // [ds]
    
    /* Should not get here, if we do, an error must have occurred
     * start from the top */
    goto start_app
