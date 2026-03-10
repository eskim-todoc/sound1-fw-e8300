#ifndef ISD_INTERFACE_INIT_FPGA_H__
#define ISD_INTERFACE_INIT_FPGA_H__

#include <stdbool.h>

#include "FPGA.h" //ok

#include <ci_printf.h>

void init_FPGA(bool isdControlStateChagedFlag);
void init_txPowerIC(bool isdControlStateChagedFlag);
void init_ISD(bool isdControlStateChagedFlag);

#endif
