
#ifndef __tdc_hal_trims_h__
#define __tdc_hal_trims_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <calibrate_power.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <definitionsForAlgorithm.h>

#include <tdc_util.h>
#include <tdc_fs.h>
#include <tdc_printf.h>

#include <sk5_map_nvm.h>

void tdc_hal_trims_load_manu_table(uint32_t *p_manu_table);
unsigned int tdc_hal_trims_set_vreg_and_lsad();
unsigned int tdc_hal_trims_set_vddif(unsigned int target);
unsigned int tdc_hal_trims_set_vdda(unsigned int target);
unsigned int tdc_hal_trims_set_vddc(unsigned int target);
unsigned int tdc_hal_trims_set_vddc_cp(unsigned int target);
unsigned int tdc_hal_trims_set_vddm(unsigned int target);
unsigned int tdc_hal_trims_set_vddm_cp(unsigned int target);
unsigned int tdc_hal_trims_set_vddod(unsigned int target);
unsigned int tdc_hal_trims_set_vmic(unsigned int target);
unsigned int tdc_hal_trims_set_operating_frequency_mult(unsigned int frequency_index, unsigned int multiplier);
unsigned int tdc_hal_trims_set_operating_frequency(unsigned int frequency_index);
unsigned int tdc_hal_trims_set_adc_offsets();

#endif /* __tdc_hal_trims_h__ */
