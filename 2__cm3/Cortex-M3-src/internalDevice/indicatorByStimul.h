#ifndef INDICATORBYSTIMULATION_H__
#define INDICATORBYSTIMULATION_H__

#include <stdbool.h>

bool is_Triggered_stimulationIndicator(void);

void set_stimulationIndicatorTrigger(void);
void stimulation_IndicatorOut(int enableStimulationIndicator, bool stimulationTriggerLowPower, bool stimulationTriggerMapping);
void setIndicatoStimlulLevel_255(void);

#endif
