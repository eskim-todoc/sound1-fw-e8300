/**
 * @file tdc_drv_max17262.c
 */

#include <tdc_drv_max17262.h>

static volatile uint32_t _g_mv = 0;

uint32_t tdc_drv_max17262_get_mv(void)
{
    return _g_mv;
}

void tdc_drv_max17262_update_mv(void)
{
    tdc_hal_i2c_driver_state_t i2c_driver_state;
    int                  reg;
    int                  data[2];
    uint16_t             raw;
    uint32_t             mv;

    if (tdc_hal_i2c_get_driver_status() != i2c_state_Idle)
    {
        return;
    }

    // Writing

    reg = TDC_DRV_MAX17262_REG_VCELL;

    tdc_hal_i2c_start_write(TDC_DRV_MAX17262_SLAVE_ADDR, &reg, 1);

    while (1)
    {
        i2c_driver_state = tdc_hal_i2c_get_driver_status();

        if (i2c_driver_state == i2c_state_WritingDone)
        {
            tdc_hal_i2c_set_driver_status_idle();
            break;
        }

        if (i2c_driver_state == i2c_state_Error)
        {
            tdc_hal_i2c_init();
            TDC_PRINTF_E("[MAX17262] I2C WRITING ERROR. \r\n");
            return;
        }

        __WFE();
    }

    // Reading

    tdc_hal_i2c_start_read(TDC_DRV_MAX17262_SLAVE_ADDR, data, 2);

    while (1)
    {
        i2c_driver_state = tdc_hal_i2c_get_driver_status();

        if (i2c_driver_state == i2c_state_ReadingDone)
        {
            tdc_hal_i2c_set_driver_status_idle();
            break;
        }

        if (i2c_driver_state == i2c_state_Error)
        {
            tdc_hal_i2c_init();
            TDC_PRINTF_E("[MAX17262] I2C READING ERROR. \r\n");
            return;
        }

        __WFE();
    }

    raw = (uint16_t) (data[0] | (data[1] << 8));
    mv  = (((uint32_t) raw) * 78125) / 1000000;
}
