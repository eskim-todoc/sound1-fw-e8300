
#ifndef __tdc_hal_i2c_cfx_h__
#define __tdc_hal_i2c_cfx_h__

#define TDC_HAL_I2C_CFX_CMD_WRITE 1
#define TDC_HAL_I2C_CFX_CMD_READ  2


bool tdc_hal_i2c_cfx_write(int slaveAddr, int *dataBuff, int dataSize);

bool tdc_hal_i2c_cfx_read(int slaveAddr, int *dataBuff, int dataSize);

#endif
