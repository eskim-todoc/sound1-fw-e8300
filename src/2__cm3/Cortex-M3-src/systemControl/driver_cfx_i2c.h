
#ifndef DEFINITION_Driver_CFX_I2C_H__
#define DEFINITION_Driver_CFX_I2C_H__

#define df_i2c_CommandWrite 1
#define df_i2c_CommandRead  2

bool isI2C_done(void);

bool cfx_i2c_write(int slaveAddr, int *dataBuff, int dataSize);

bool cfx_i2c_read(int slaveAddr, int *dataBuff, int dataSize);

#endif
