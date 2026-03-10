
#ifndef ISD_INTERFACE_MAPPING_LIVE_H__
#define ISD_INTERFACE_MAPPING_LIVE_H__

#include <stdbool.h>

typedef enum
{
    en__Standby = 0,
    en__allParameter,             // 1
    en__Start,                    // 2
    en__StimulationVolumeAdjust,  // 3
    en__MicSensitivityAdjust,     // 4
    en__mapping_Stimul_indicator, // 5
    en__readEqualizer,            // 6
    en__readDeviceStatus,         // 7
    en__Stop,                     // 8
    en__HoldOn                    // 9
} EN__LIVE_STIMULATION_SUB_COMMAND;

#define Live_AllParameter_payloadNum 15

bool liveStimulation(ST__ISD_STATUS ISD_state);

#endif
