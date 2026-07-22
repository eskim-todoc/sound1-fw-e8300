#ifndef __tdc_isd_stim_para_setting_h__
#define __tdc_isd_stim_para_setting_h__

#include <stdbool.h>

#include <tdc_printf.h>

void tdc_isd_clear_stim_para_setting_done(void);
bool tdc_isd_is_stim_para_setting_done(void);
bool tdc_isd_set_stim_para_monopolar(bool isdControlStateChagedFlag);
bool tdc_isd_set_stim_para_bipolar(bool isdControlStateChagedFlag);
bool tdc_isd_set_stim_para_common_ground(bool isdControlStateChagedFlag);
bool tdc_isd_stim_setting_step(bool startTrigger);

#endif
