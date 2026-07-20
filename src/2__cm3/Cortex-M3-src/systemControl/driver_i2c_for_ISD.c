

#include "driver_i2c_for_ISD.h"
#include <ci_printf.h>

bool write_ISD_by_CM3_I2C(int slaveAddr, int* dataBuff, int dataSize)
{
    EN__I2C_DRIVER_STATE i2cDriverState;
    bool                 PassFail = false;
    volatile int         wait_cnt;

    if (dataSize == 0)
    {
        return false;
    }
    else
    {
        i2c_startWriteData(slaveAddr, dataBuff, dataSize);

        wait_cnt = 5000;  // total 5 sec

        while (0 <= wait_cnt)
        {
            i2cDriverState = get_i2cDriverStatus();
            if (i2cDriverState == i2c_state_WritingDone)
            {
                setI2cDriverStatusIdle();
                PassFail = true;
                break;
            }

            if (i2cDriverState == i2c_state_Error)
            {
                init_I2c();
                break;
            }

            //__WFE();
            Sys_Delay((SystemCoreClock / 1000));  // 1 msec
            wait_cnt--;
        }

        if (wait_cnt < 0)
        {
            PassFail = false;
            init_I2c();
            ci_printe("[I2C] TIMEOUT FOR WRITING \r\n");
        }

        return PassFail;
    }
}

bool read_ISD_by_CM3_I2C(int slaveAddr, int* dataBuff, int dataSize)
{
    EN__I2C_DRIVER_STATE i2cDriverState;
    bool                 PassFail = false;
    volatile int         wait_cnt;

    if (dataSize == 0)
    {
        return false;
    }
    else
    {
        i2c_startReadData(slaveAddr, dataBuff, dataSize);

        wait_cnt = 5000;  // total 5 sec

        while (0 <= wait_cnt)
        {
            i2cDriverState = get_i2cDriverStatus();

            if (i2cDriverState == i2c_state_ReadingDone)
            {
                setI2cDriverStatusIdle();
                PassFail = true;
                break;
            }

            if (i2cDriverState == i2c_state_Error)
            {
                init_I2c();
                break;
            }

            //__WFE();
            Sys_Delay((SystemCoreClock / 1000));  // 1 msec
            wait_cnt--;
        }

        if (wait_cnt < 0)
        {
            PassFail = false;
            init_I2c();
            ci_printe("[I2C] TIMEOUT FOR READING \r\n");
        }

        return PassFail;
    }
}
