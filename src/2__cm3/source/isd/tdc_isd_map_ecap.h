#ifndef __tdc_isd_map_ecap_h__
#define __tdc_isd_map_ecap_h__

#include <stdbool.h>

void tdc_isd_map_ecap_step(bool startFlag);

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
