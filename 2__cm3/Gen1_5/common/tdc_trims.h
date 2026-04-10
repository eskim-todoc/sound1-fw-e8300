
#ifndef COMMON_TDC_TRIMS_H_
#define COMMON_TDC_TRIMS_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <calibrate_power.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <definitionsForAlgorithm.h>

#include <ci_util.h>
#include <ci_filesystem.h>
#include <ci_printf.h>

#include <sk5_map_nvm.h>

void tdc_Trims_LoadManuTable(uint32_t *p_manu_table);
unsigned int tdc_Trims_SetVREGAndLSAD();
unsigned int tdc_Trims_SetVDDIF(unsigned int target);
unsigned int tdc_Trims_SetVDDA(unsigned int target);
unsigned int tdc_Trims_SetVDDC(unsigned int target);
unsigned int tdc_Trims_SetVDDC_CP(unsigned int target);
unsigned int tdc_Trims_SetVDDM(unsigned int target);
unsigned int tdc_Trims_SetVDDM_CP(unsigned int target);
unsigned int tdc_Trims_SetVDDOD(unsigned int target);
unsigned int tdc_Trims_SetVMIC(unsigned int target);
unsigned int tdc_Trims_SetOperatingFrequencyMult(unsigned int frequency_index, unsigned int multiplier);
unsigned int tdc_Trims_SetOperatingFrequency(unsigned int frequency_index);
unsigned int tdc_Trims_SetADCOffsets();

#endif /* COMMON_TDC_TRIMS_H_ */
