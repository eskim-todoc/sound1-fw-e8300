
#ifndef ISD_INTERFACE_STIMULATIONSATNDALONE_H__
#define ISD_INTERFACE_STIMULATIONSATNDALONE_H__

#include <stdbool.h>

#include "tdc_printf.h"

bool stimulationStandAlone(void);
bool isNewMapLoadeFlag(void);
void set_newMapLoadeFlagForStimulParaSetting(void);
void clear_newMapLoadeFlag(void);

#endif
