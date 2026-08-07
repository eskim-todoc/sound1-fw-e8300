
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <board.h>

#include <tdc_hal_i2c.h>

#include <tdc_drv_isl9122.h>
#include <tdc_stim_common.h>
#include <tdc_util.h>

/* 데이터시트 5.10 - Forced Bypass 와 규정 모드를 오갈 때 연속되는 모드 설정 I2C
 * 명령 사이에 1ms 이상을 두라고 요구한다. 여유를 조금 얹어 2ms 로 잡는다. */
#define TDC_DRV_ISL9122_MODE_SETTLE_MS 2

bool tdc_drv_isl9122_write_register(int registerAddr, int value)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    int                        transferBuffer[2];
    bool                       PassFail = false;

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
    bool                       PassFail = false;
    int                        transferBuffer[2];

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

    tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_CONV_CFG, TDC_DRV_PMIC_DEFAULTVALUE_CONV_CFG);
    // tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_CONV_CFG,0x89);
    tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_CONV_CFG, &readValue);

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

// 모드 이름. 열거형 바로 옆에 두어 값과 이름이 따로 놀지 않게 한다.
const char *tdc_drv_isl9122_mode_name(tdc_drv_isl9122_fmode_t mode)
{
    switch (mode)
    {
        case TDC_DRV_ISL9122_FMODE_NORMAL:
        {
            return "NORMAL (auto buck/bypass/boost)";
        }

        case TDC_DRV_ISL9122_FMODE_RESERVED:
        {
            return "RESERVED (datasheet says do not use)";
        }

        case TDC_DRV_ISL9122_FMODE_FORCED_PWM:
        {
            return "FORCED PWM (no PFM)";
        }

        case TDC_DRV_ISL9122_FMODE_FORCED_BYPASS:
        {
            return "FORCED BYPASS (no boost, no OCP)";
        }

        default:
        {
            return "UNKNOWN";
        }
    }
}

// 현재 동작 모드를 읽는다. CONV_CFG 를 읽어 FMODE 두 비트만 뽑는다.
bool tdc_drv_isl9122_read_mode(tdc_drv_isl9122_fmode_t *p_mode)
{
    int readValue;

    if (p_mode == NULL)
    {
        return false;
    }

    if (!tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_CONV_CFG, &readValue))
    {
        return false;
    }

    *p_mode = (tdc_drv_isl9122_fmode_t) ((readValue & TDC_DRV_ISL9122_CONV_CFG_FMODE_MASK) >> TDC_DRV_ISL9122_CONV_CFG_FMODE_SHIFT);

    return true;
}

// 동작 모드를 바꾼다. 성공 판정은 되읽은 값이 기대와 같은 것까지다.
//
// CONV_CFG 를 통째로 쓰면 EN_AND · DISCH · DVSRATE · TYPE1 을 덮어써서 변환기가
// 꺼지거나 오차증폭기가 바뀐다. 반드시 읽고-고쳐-쓴다.
bool tdc_drv_isl9122_write_mode(tdc_drv_isl9122_fmode_t mode)
{
    int requested = (int) mode;
    int currentValue;
    int newValue;
    int readBack;

    // 데이터시트 Table 5 가 0x1 을 금지한다. 범위 밖도 함께 막는다.
    if ((requested < TDC_DRV_ISL9122_FMODE_NORMAL) || (requested > TDC_DRV_ISL9122_FMODE_FORCED_BYPASS) || (requested == TDC_DRV_ISL9122_FMODE_RESERVED))
    {
        return false;
    }

    if (!tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_CONV_CFG, &currentValue))
    {
        return false;
    }

    newValue =
        (currentValue & ~TDC_DRV_ISL9122_CONV_CFG_FMODE_MASK) | ((requested << TDC_DRV_ISL9122_CONV_CFG_FMODE_SHIFT) & TDC_DRV_ISL9122_CONV_CFG_FMODE_MASK);

    if (!tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_CONV_CFG, newValue))
    {
        return false;
    }

    // 다음 모드 설정 명령까지의 간격을 여기서 미리 확보한다.
    tdc_util_delay_ms(TDC_DRV_ISL9122_MODE_SETTLE_MS);

    if (!tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_CONV_CFG, &readBack))
    {
        return false;
    }

    return (readBack == newValue);
}
