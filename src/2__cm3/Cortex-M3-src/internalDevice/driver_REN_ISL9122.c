
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "board.h"

#if defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#include "driver_i2c.h"

#include "driver_REN_ISL9122.h"
#include "commonDataProcessing.h"
#if defined(TDC_UNIT_TEST)
#include "../test/tdc_ut_io_cm3.h"
int (*g_tdc_ut_io_cm3_pmic_bus_hook)(int read, int addr, int *data, int count) = 0;
#endif

bool write_REN_ISL9122_register_byCM3_I2C(int registerAddr, int value)
{
    EN__I2C_DRIVER_STATE i2cDriverState;
    int                  transferBuffer[2];
    bool                 PassFail = false;

    transferBuffer[0] = (int) registerAddr;
    transferBuffer[1] = (int) value;
#if defined(TDC_UNIT_TEST)
    if (g_tdc_ut_io_cm3_pmic_bus_hook != 0) return g_tdc_ut_io_cm3_pmic_bus_hook(0, REN_ISL9122_SlaveAddr, transferBuffer, 2) != 0;
#endif
    i2c_startWriteData(REN_ISL9122_SlaveAddr, transferBuffer, 2);

    while (1)
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
        __WFE();
    }

    return PassFail;
}

bool read_REN_ISL9122_register_byCM3_I2C(int registerAddr, int *read_value)
{
    EN__I2C_DRIVER_STATE i2cDriverState;
    bool                 PassFail = false;
    int                  transferBuffer[2];

    // 읽기 : 선행 명령 전송 (
    transferBuffer[0] = registerAddr;
#if defined(TDC_UNIT_TEST)
    if (g_tdc_ut_io_cm3_pmic_bus_hook != 0)
    {
        if (!g_tdc_ut_io_cm3_pmic_bus_hook(0, REN_ISL9122_SlaveAddr, transferBuffer, 1)) return false;
        if (!g_tdc_ut_io_cm3_pmic_bus_hook(1, REN_ISL9122_SlaveAddr, &transferBuffer[1], 1)) return false;
        *read_value = transferBuffer[1];
        return true;
    }
#endif
    i2c_startWriteData(REN_ISL9122_SlaveAddr, transferBuffer, 1);

    while (1)
    {
        i2cDriverState = get_i2cDriverStatus();

        if (i2cDriverState == i2c_state_WritingDone)
        {
            setI2cDriverStatusIdle();
            // 읽기 : 레지스터 값 읽어들임
            i2c_startReadData(REN_ISL9122_SlaveAddr, &transferBuffer[1], 1);
        }

        if (i2cDriverState == i2c_state_ReadingDone)
        {
            *read_value = transferBuffer[1];

            setI2cDriverStatusIdle();
            PassFail = true;
            break;
        }

        if (i2cDriverState == i2c_state_Error)
        {
            init_I2c();
            break;
        }

        __WFE();
    }

    return PassFail;
}

bool Reset_REN_ISL9122(void)
{
    int value;
    int readValue;
    int temp;

#if 0

    write_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_CONV_CFG,DefaultValue_CONV_CFG);

    temp=data_clearBit(DefaultValue_INTFLAG_MAS, BitPosition_OC_FAULT_MODE, BitLength_OC_FAULT_MODE);
    value=1;
    value=(value<<BitPosition_OC_FAULT_MODE)|temp;

    write_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_INTFLAG_MASK,value);

#else

    write_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_CONV_CFG, DefaultValue_CONV_CFG);
    // write_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_CONV_CFG,0x89);
    read_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_CONV_CFG, &readValue);
#endif

    value = ResetVoltageSetValue;

    write_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_VoltageSet, value);

    read_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_VoltageSet, &readValue);

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
