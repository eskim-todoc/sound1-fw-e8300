#ifndef __tdc_isd_init_fpga_h__
#define __tdc_isd_init_fpga_h__

#include <stdbool.h>

#include <FPGA.h>  //ok

#include <tdc_printf.h>

void tdc_isd_init_fpga(bool isdControlStateChagedFlag);
void tdc_isd_init_tx_power_ic(bool isdControlStateChagedFlag);
void tdc_isd_init_device(bool isdControlStateChagedFlag);

#endif
