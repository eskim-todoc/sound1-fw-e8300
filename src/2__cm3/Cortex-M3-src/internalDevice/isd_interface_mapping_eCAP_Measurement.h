#ifndef ISD_INTERFACE_MAPPING_ECAPMEASUREMENT_MASKING_H__
#define ISD_INTERFACE_MAPPING_ECAPMEASUREMENT_MASKING_H__

#include <stdbool.h>

void eCapMeasurement_masking(bool startFlag);

typedef enum
{
    en__probeAlone = 1,
    en__maskerNprobe,
    en__maskerAlone,
    en__switchingArtifact

} EN__eCAP_STIMUL_PATTERN;

typedef enum
{
    en__clearIndex = 1,
    en__fillForwardData,
    en__fillBackwardData

} EN__eCAP_Templete;

//  typedef enum
//  {
//
//      en__maskerNprobe=0,
//      en__probeAlone,
//      en__maskerAlone,
//      en__switchingArtifact
//
//  }EN__eCAP_STIMUL_PATTERN;

#define Max_eCAP_ReturnDataSize 8 // b

#endif
