
#ifndef __tdc_isd_stim_standalone_h__
#define __tdc_isd_stim_standalone_h__

#include <stdbool.h>

#include "tdc_printf.h"

bool tdc_isd_stim_standalone_step(void);
bool tdc_isd_is_new_map_loaded_flag(void);
void tdc_isd_set_new_map_loaded_flag(void);
void tdc_isd_clear_new_map_loaded_flag(void);

#endif
