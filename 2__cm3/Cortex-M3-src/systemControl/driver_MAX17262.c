/**
 * @file driver_MAX17262.c
 */

#include <driver_MAX17262.h>

static volatile uint32_t _g_mv = 0;

uint32_t max17262_get_mv(void)
{
    return _g_mv;
}

void max17262_update_mv(void)
{
    EN__I2C_DRIVER_STATE i2c_driver_state;
    int                  reg;
    int                  data[2];
    uint16_t             raw;
    uint32_t             mv;

    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return;
    }

    // Writing

    reg = MAX17262_REG_VCELL;

    i2c_startWriteData(MAX17262_SLAVE_ADDR, &reg, 1);

    while (1)
    {
        i2c_driver_state = get_i2cDriverStatus();

        if (i2c_driver_state == i2c_state_WritingDone)
        {
            setI2cDriverStatusIdle();
            break;
        }

        if (i2c_driver_state == i2c_state_Error)
        {
            init_I2c();
            ci_printe("[MAX17262] I2C WRITING ERROR. \r\n");
            return;
        }

        __WFE();
    }

    // Reading

    i2c_startReadData(MAX17262_SLAVE_ADDR, data, 2);

    while (1)
    {
        i2c_driver_state = get_i2cDriverStatus();

        if (i2c_driver_state == i2c_state_ReadingDone)
        {
            setI2cDriverStatusIdle();
            break;
        }

        if (i2c_driver_state == i2c_state_Error)
        {
            init_I2c();
            ci_printe("[MAX17262] I2C READING ERROR. \r\n");
            return;
        }

        __WFE();
    }

    raw = (uint16_t) (data[0] | (data[1] << 8));
    mv  = (((uint32_t) raw) * 78125) / 1000000;
}
