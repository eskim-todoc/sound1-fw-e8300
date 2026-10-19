

#include "dirver_i2c_for_ISD.h"
#include <ci_printf.h>
#if defined(TDC_UNIT_TEST)
#include "../test/tdc_ut_io_cm3.h"
int (*g_tdc_ut_io_cm3_isd_bus_hook)(int read, int addr, int *data, int count) = 0;
#endif

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
#if defined(TDC_UNIT_TEST)
        if (g_tdc_ut_io_cm3_isd_bus_hook != 0) return g_tdc_ut_io_cm3_isd_bus_hook(0, slaveAddr, dataBuff, dataSize) != 0;
#endif
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
#if defined(TDC_UNIT_TEST)
        if (g_tdc_ut_io_cm3_isd_bus_hook != 0) return g_tdc_ut_io_cm3_isd_bus_hook(1, slaveAddr, dataBuff, dataSize) != 0;
#endif
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
