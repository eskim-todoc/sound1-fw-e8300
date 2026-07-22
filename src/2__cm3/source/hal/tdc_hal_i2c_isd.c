

#include <tdc_hal_i2c_isd.h>
#include <tdc_printf.h>

bool tdc_hal_i2c_isd_write(int slaveAddr, int* dataBuff, int dataSize)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    bool                 PassFail = false;
    volatile int         wait_cnt;

    if (dataSize == 0)
    {
        return false;
    }
    else
    {
        tdc_hal_i2c_start_write(slaveAddr, dataBuff, dataSize);

        wait_cnt = 5000;  // total 5 sec

        while (0 <= wait_cnt)
        {
            i2cDriverState = tdc_hal_i2c_get_driver_status();
            if (i2cDriverState == i2c_state_WritingDone)
            {
                tdc_hal_i2c_set_driver_status_idle();
                PassFail = true;
                break;
            }

            if (i2cDriverState == i2c_state_Error)
            {
                tdc_hal_i2c_init();
                break;
            }

            //__WFE();
            Sys_Delay((SystemCoreClock / 1000));  // 1 msec
            wait_cnt--;
        }

        if (wait_cnt < 0)
        {
            PassFail = false;
            tdc_hal_i2c_init();
            TDC_PRINTF_E("[I2C] TIMEOUT FOR WRITING \r\n");
        }

        return PassFail;
    }
}

bool tdc_hal_i2c_isd_read(int slaveAddr, int* dataBuff, int dataSize)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    bool                 PassFail = false;
    volatile int         wait_cnt;

    if (dataSize == 0)
    {
        return false;
    }
    else
    {
        tdc_hal_i2c_start_read(slaveAddr, dataBuff, dataSize);

        wait_cnt = 5000;  // total 5 sec

        while (0 <= wait_cnt)
        {
            i2cDriverState = tdc_hal_i2c_get_driver_status();

            if (i2cDriverState == i2c_state_ReadingDone)
            {
                tdc_hal_i2c_set_driver_status_idle();
                PassFail = true;
                break;
            }

            if (i2cDriverState == i2c_state_Error)
            {
                tdc_hal_i2c_init();
                break;
            }

            //__WFE();
            Sys_Delay((SystemCoreClock / 1000));  // 1 msec
            wait_cnt--;
        }

        if (wait_cnt < 0)
        {
            PassFail = false;
            tdc_hal_i2c_init();
            TDC_PRINTF_E("[I2C] TIMEOUT FOR READING \r\n");
        }

        return PassFail;
    }
}
