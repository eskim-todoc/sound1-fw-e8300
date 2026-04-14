#include "board.h"
#if defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#ifndef REN_ISL9122_H__
#define REN_ISL9122_H__

#include <stdbool.h>

#define REN_ISL9122_SlaveAddr 0x18

#define REN_ISL9122_registerAddr_VoltageSet   0x11
#define REN_ISL9122_registerAddr_CONV_CFG     0x12
#define REN_ISL9122_registerAddr_INTFLAG_MASK 0x13

#define DefaultValue_CONV_CFG    0x81
#define DefaultValue_INTFLAG_MAS 0x00

#define BitPosition_Type 0
#define BitLength__Type  1

#define BitPosition_OC_FAULT_MODE 5
#define BitLength_OC_FAULT_MODE   2

#define MaxVoltageControlValue 214 // 0.025*214=5.35V
                                   //  #define MaxVoltageControlValue      190     // 0.025*200=4.75V
// #define MaxVoltageControlValue    180     // 0.025*180=4.5V
// #define MaxVoltageControlValue    168     // 0.025*168=4.2V
// #define MaxVoltageControlValue    160     // 0.025*160=4.0V
#define VoltageControlStep 5
#define MinTxPowerValue    164 // 0.025*160=4V
// #define MinTxPowerValue       178     // 0.025*178=4.45
// #define MinTxPowerValue   (MaxVoltageControlValue-1)          // 0.025*178=4.45

#define ResetVoltageSetValue 75 // 0.025*75=1.875V

bool write_REN_ISL9122_register_byCM3_I2C(int registerAddr, int value);
bool read_REN_ISL9122_register_byCM3_I2C(int registerAddr, int *read_value);
bool Reset_REN_ISL9122(void);

#endif

#endif
