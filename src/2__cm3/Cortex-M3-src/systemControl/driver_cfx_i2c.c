#include <hw.h>
#include <stdbool.h>

#include "cfx_cm3_sharedMemory.h"
#include "driver_i2c_state_cfx.h"
#if 1
#include "board.h"
#endif

#define df_i2c_CommandWrite 1
#define df_i2c_CommandRead  2

extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll;

bool cfx_i2c_done = true;

void CFX_1_IRQHandler(void)
{

    cfx_i2c_done = true;
}

bool isI2C_done(void)
{
    return cfx_i2c_done;
}

bool cfx_i2c_write(int slaveAddr, int *dataBuff, int dataSize)
{

    int k;

    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.SlaveAddr = slaveAddr;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.RW        = df_i2c_CommandWrite;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.dataSize  = dataSize;

    for (k = 0; k < dataSize; k++)
        cfx_cm3_sharedMemoryAll.cfx_i2c_interface.CM3_TxBuffer[k] = dataBuff[k];

    cfx_i2c_done                     = false;
    SYSCTRL_CFX_CMD->CFX_CMD_1_ALIAS = 1; // CFX에 i2c명령이 업데이트 되었음을 알려준다.

    __WFI();

    while (!cfx_i2c_done)
    {

        __WFI();
    }
    if (cfx_cm3_sharedMemoryAll.cfx_i2c_interface.i2c_State == df_I2C_State_Idle)
    {
        return true;
    }
    else
    {
        return false;
    }
    //
}

bool cfx_i2c_read(int slaveAddr, int *dataBuff, int dataSize)
{

    int k;

    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.SlaveAddr = slaveAddr;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.RW        = df_i2c_CommandRead;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.dataSize  = dataSize;

    cfx_i2c_done                     = false;
    SYSCTRL_CFX_CMD->CFX_CMD_1_ALIAS = 1; // CFX에 i2c명령이 업데이트 되었음을 알려준다.

    __WFI();

    while (!cfx_i2c_done)
    {

        __WFI();
    }
    if (cfx_cm3_sharedMemoryAll.cfx_i2c_interface.i2c_State == df_I2C_State_Idle)
    {
        for (k = 0; k < dataSize; k++)
            dataBuff[k] = cfx_cm3_sharedMemoryAll.cfx_i2c_interface.CM3_RxBuffer[k];

        return true;
    }
    else
    {
        return false;
    }
}
