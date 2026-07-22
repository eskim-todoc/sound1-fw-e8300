
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <board.h>

#if defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#include <tdc_hal_i2c.h>

#include <tdc_drv_isl9122.h>
#include <tdc_stim_common.h>

bool tdc_drv_isl9122_write_register(int registerAddr, int value)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    int                  transferBuffer[2];
    bool                 PassFail = false;

    transferBuffer[0] = (int) registerAddr;
    transferBuffer[1] = (int) value;
    tdc_hal_i2c_start_write(TDC_DRV_ISL9122_SLAVE_ADDR, transferBuffer, 2);

    while (1)
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
        __WFE();
    }

    return PassFail;
}

bool tdc_drv_isl9122_read_register(int registerAddr, int *read_value)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    bool                 PassFail = false;
    int                  transferBuffer[2];

    // 읽기 : 선행 명령 전송 (
    transferBuffer[0] = registerAddr;
    tdc_hal_i2c_start_write(TDC_DRV_ISL9122_SLAVE_ADDR, transferBuffer, 1);

    while (1)
    {
        i2cDriverState = tdc_hal_i2c_get_driver_status();

        if (i2cDriverState == i2c_state_WritingDone)
        {
            tdc_hal_i2c_set_driver_status_idle();
            // 읽기 : 레지스터 값 읽어들임
            tdc_hal_i2c_start_read(TDC_DRV_ISL9122_SLAVE_ADDR, &transferBuffer[1], 1);
        }

        if (i2cDriverState == i2c_state_ReadingDone)
        {
            *read_value = transferBuffer[1];

            tdc_hal_i2c_set_driver_status_idle();
            PassFail = true;
            break;
        }

        if (i2cDriverState == i2c_state_Error)
        {
            tdc_hal_i2c_init();
            break;
        }

        __WFE();
    }

    return PassFail;
}

bool tdc_drv_isl9122_reset(void)
{
    int value;
    int readValue;
    int temp;

#if 0

    tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_CONV_CFG,TDC_DRV_PMIC_DEFAULTVALUE_CONV_CFG);

    temp=tdc_stim_data_clear_bit(TDC_DRV_PMIC_DEFAULTVALUE_INTFLAG_MAS, TDC_DRV_PMIC_BITPOSITION_OC_FAULT_MODE, TDC_DRV_PMIC_BITLENGTH_OC_FAULT_MODE);
    value=1;
    value=(value<<TDC_DRV_PMIC_BITPOSITION_OC_FAULT_MODE)|temp;

    tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_INTFLAG_MASK,value);

#else

    tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_CONV_CFG, TDC_DRV_PMIC_DEFAULTVALUE_CONV_CFG);
    // tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_CONV_CFG,0x89);
    tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_CONV_CFG, &readValue);
#endif

    value = TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE;

    tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_VOLTAGESET, value);

    tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_VOLTAGESET, &readValue);

    if (value == readValue)
    {
        return true;
    }
    else
    {
        return false;
    }
}

#endif
