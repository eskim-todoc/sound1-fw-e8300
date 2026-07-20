/**
 * @file initialize.h
 */

#ifndef __initialize_h__
#define __initialize_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_util.h>

#include <isd_interface_init_FPGA.h>
#include <isd_interface_FPGA.h>

void Uninitialize(void);
void Initialize(void);
void ResetNRF(void);
void cm3MemorySetupCompleted(void);

#endif  // __initialize_h__
