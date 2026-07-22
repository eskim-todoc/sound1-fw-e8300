#ifndef internalStimulationChip_H__
#define internalStimulationChip_H__

#include <processorDirective.h>

#if defined(ISD_Ver_is_100)

#include <isd_ver1_0_0.h>

#elif defined(ISD_Ver_is_110)

#include <isd_ver1_1_0.h>

#elif defined(ISD_Ver_is_111)
#include <isd_ver1_1_1.h>

#elif defined(ISD_Ver_is_112)
#include <isd_ver1_1_2.h>

#else
#error Board is NOT selected.
#endif
#endif
