#ifndef __tdc_stim_indicator_h__
#define __tdc_stim_indicator_h__

#include <stdbool.h>

bool tdc_stim_indicator_is_triggered(void);

void tdc_stim_indicator_set_trigger(void);
void tdc_stim_indicator_out(int enableStimulationIndicator, bool stimulationTriggerLowPower, bool stimulationTriggerMapping);
void tdc_stim_indicator_set_level_255(void);

#endif
