#ifndef DRIVER_I2C_FOR_ISD_H__
#define DRIVER_I2C_FOR_ISD_H__

#include <stdbool.h>

#include "driver_i2c.h"  //ok

bool write_ISD_by_CM3_I2C(int slaveAddr, int *dataBuff, int dataSize);
bool read_ISD_by_CM3_I2C(int slaveAddr, int *dataBuff, int dataSize);

#endif
