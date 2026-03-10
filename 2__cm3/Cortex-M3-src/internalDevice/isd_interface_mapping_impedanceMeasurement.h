

#ifndef ISD_INTERFACE_MAPPING_IMPEDANCEMEASUREMENT_H__
#define ISD_INTERFACE_MAPPING_IMPEDANCEMEASUREMENT_H__

#include <stdbool.h>

typedef enum
{
    en__MonoPolar_impedance = 1,
    en_BiPolar_impedance
} EN__IMPEDANCE_MEASUREMENT_METHOD;

typedef enum
{
    en__startPulse_saving_index = 0,
    en__endPulse_saving_index
} EN__IMPEDANCE_SAVING_INDEX;

// 임피던스 측정시 최대 반복 횟수
#define df_maxIterationNum_impedance 255

void impedanceMeasurement(bool startFlag);

#endif

