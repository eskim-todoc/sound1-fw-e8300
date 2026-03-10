#ifndef isd_interface_mapping_SepcificStimulation_H___
#define isd_interface_mapping_SepcificStimulation_H___

#include <stdbool.h>

#include "ci_printf.h"

void clear_sitmulationParaSettingDone(void);
bool isStimulParaIsSettingDone(void);
bool settingStimulPara_monoPolarMode(bool isdControlStateChagedFlag);
bool settingStimulPara_biPolarMode(bool isdControlStateChagedFlag);
bool settingStimulPara_commonGround(bool isdControlStateChagedFlag);
bool stimulationSetting(bool startTrigger);

#endif
