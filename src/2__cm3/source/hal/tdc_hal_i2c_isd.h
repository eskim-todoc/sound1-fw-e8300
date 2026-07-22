#ifndef __tdc_hal_i2c_isd_h__
#define __tdc_hal_i2c_isd_h__

#include <stdbool.h>

#include <tdc_hal_i2c.h>  //ok

bool tdc_hal_i2c_isd_write(int slaveAddr, int *dataBuff, int dataSize);
bool tdc_hal_i2c_isd_read(int slaveAddr, int *dataBuff, int dataSize);

#endif
