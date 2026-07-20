#ifndef stimulationParameterCalculation_H___
#define stimulationParameterCalculation_H___

#include <stdbool.h>

#include "definitionsForAlgorithm.h"

#include "tdc_printf.h"

typedef struct
{
    int DAC_Slope_register; // cm3에서 계산함
    int DAC_offsetSlope_register;
    int DAC_offsetLevel_register; // cm3에서 변경함.

} ST_STIUL_DAC_REGISTER_VALUE;

bool calculationStimulParaN_cfxShare(void);

int calculationNumFramePerOneChannle(int pulsewidth);
int calculationTransferableChanneNum(int numFramePerChannel, int usableElectrodNum);
// const ST_STIUL_DAC_REGISTER_VALUE *readStimulDAC_RegisterValue(void);
ST_STIUL_DAC_REGISTER_VALUE *readStimulDAC_RegisterValue(void);
const int                    readDAC_offsetValue(void);
bool                         setting_stimulationRange(int pulseWidth, int *T_level_uA, int *C_level_uA, int NumCh, int *T_level_255, int *C_level_255);

#endif
