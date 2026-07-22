#include <hw.h>
#include <stdbool.h>

#include "tdc_shm.h"
#include "tdc_hal_i2c_state.h"
#if 1
#include "board.h"
#endif

#define TDC_HAL_I2C_CFX_CMD_WRITE 1
#define TDC_HAL_I2C_CFX_CMD_READ  2

extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll;

bool cfx_i2c_done = true;

void CFX_1_IRQHandler(void)
{

    cfx_i2c_done = true;
}

bool tdc_hal_i2c_cfx_write(int slaveAddr, int *dataBuff, int dataSize)
{

    int k;

    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.SlaveAddr = slaveAddr;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.RW        = TDC_HAL_I2C_CFX_CMD_WRITE;
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
    if (cfx_cm3_sharedMemoryAll.cfx_i2c_interface.i2c_State == TDC_HAL_I2C_STATE_IDLE)
    {
        return true;
    }
    else
    {
        return false;
    }
    //
}

bool tdc_hal_i2c_cfx_read(int slaveAddr, int *dataBuff, int dataSize)
{

    int k;

    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.SlaveAddr = slaveAddr;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.RW        = TDC_HAL_I2C_CFX_CMD_READ;
    cfx_cm3_sharedMemoryAll.cfx_i2c_interface.dataSize  = dataSize;

    cfx_i2c_done                     = false;
    SYSCTRL_CFX_CMD->CFX_CMD_1_ALIAS = 1; // CFX에 i2c명령이 업데이트 되었음을 알려준다.

    __WFI();

    while (!cfx_i2c_done)
    {

        __WFI();
    }
    if (cfx_cm3_sharedMemoryAll.cfx_i2c_interface.i2c_State == TDC_HAL_I2C_STATE_IDLE)
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
