#include "driver_i2c.h"
#include "processorDirective.h"
#include <stdbool.h>
#include <stddef.h>

#define CM3_I2c_TestMode

static ST__I2C_DRIVER i2c_driver;

EN__I2C_DRIVER_STATE get_i2cDriverStatus(void)
{
    return i2c_driver.i2c_diver_state;
}

bool isI2cDriverStatusIdle(void)
{
    if (i2c_driver.i2c_diver_state == i2c_state_Idle)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void setI2cDriverStatusIdle(void)
{
    i2c_driver.i2c_diver_state = i2c_state_Idle;
}

void clearI2cDriverStatus(void)
{
    i2c_driver.i2c_diver_state             = i2c_state_Idle;
    i2c_driver.i2cTxRx_RemaindedDataLength = 0;
    i2c_driver.p_i2cTx_Source              = NULL;
    i2c_driver.p_i2cRx_Destination         = NULL;
    i2c_driver.slaveAddress                = 0;
    i2c_driver.i2c_Error_Code              = I2C_NO_ERROR;
}

uint32_t getI2cHardwareStatus(void)
{
    return I2C0->STATUS;
}

#ifdef CM3_I2c_TestMode
int I2C_statusRegister_dump[32];
int dumpIndex = 0;
#endif

void enableI2cInterface(bool isEnabled)
{
    // I2C_ENABLE과 I2C_DISABLE의 비트 인덱스가 다르다. 둘다 Write only 비트 필드이다.
    if (isEnabled)
    {
        I2C0->CTRL = I2C0->CTRL | I2C_ENABLE;
    }
    else
    {
        I2C0->CTRL = I2C0->CTRL | I2C_DISABLE;
    }
}

void clearI2cHardwareStatus(void)
{
    I2C0->STATUS = df__clearI2cHardwareStatus;
    NVIC_ClearPendingIRQ(I2C_0_IRQn);
}

void i2c_startWriteData(const int slaveAddress, int* p_sourcedata, const int dataLength)
{
    OTE_1_5gen_I2C_STATUS_T i2cStatus;

    i2c_driver.slaveAddress                = slaveAddress;
    i2c_driver.p_i2cTx_Source              = p_sourcedata;
    i2c_driver.i2cTxRx_RemaindedDataLength = dataLength;

    i2cStatus.status = getI2cHardwareStatus();

#ifdef CM3_I2c_TestMode
    I2C_statusRegister_dump[dumpIndex++] = (int) i2cStatus.status;
    if (dumpIndex >= 32)
        dumpIndex = 0;
#endif

    // 버스 Free, 라인 Free 그리고 TX 전송이 가능한 상태에서만 I2C 전송
    if ((i2cStatus.fields.busy == 0) && (i2cStatus.fields.tx_req == 1) && (i2cStatus.fields.line_free == 1))
    {
        i2c_driver.i2c_diver_state = i2c_state_WriteTriggered;

        clearI2cHardwareStatus();
        Sys_I2C_StartWrite(I2C0, i2c_driver.slaveAddress);
    }
    else
    {
        i2c_driver.i2c_diver_state = i2c_state_Error;
        i2c_driver.i2c_Error_Code  = I2C_SLAVE_DEVICE_NO_REACTION;
    }
}

void i2c_startReadData(const int slaveAddress, int* p_destination, const int dataLength)
{
    OTE_1_5gen_I2C_STATUS_T i2cStatus;

    i2c_driver.slaveAddress                = slaveAddress;
    i2c_driver.p_i2cRx_Destination         = p_destination;
    i2c_driver.i2cTxRx_RemaindedDataLength = dataLength;

    i2cStatus.status = getI2cHardwareStatus();

#ifdef CM3_I2c_TestMode
    I2C_statusRegister_dump[dumpIndex++] = (int) i2cStatus.status;
    if (dumpIndex >= 32)
        dumpIndex = 0;
#endif

    if ((i2cStatus.fields.busy == 0) && (i2cStatus.fields.tx_req == 1) && (i2cStatus.fields.line_free == 1))
    {
        i2c_driver.i2c_diver_state = i2c_state_ReadTriggerd;

        clearI2cHardwareStatus();
        Sys_I2C_StartRead(I2C0, i2c_driver.slaveAddress);
    }
    else
    {
        i2c_driver.i2c_diver_state = i2c_state_Error;
        i2c_driver.i2c_Error_Code  = I2C_SLAVE_DEVICE_NO_REACTION;
    }
}

void init_I2c(void)
{
    Sys_I2C_Reset(I2C0);
    Sys_I2C_Config(I2C0, CM3_I2C_CFG_VAL_AsMaster);
    clearI2cDriverStatus();
    enableI2cInterface(true);

#ifdef CM3_I2c_using_ISR
    NVIC_ClearPendingIRQ(I2C_0_IRQn);
    NVIC_EnableIRQ(I2C_0_IRQn);  // I2C0 인터럽트 활성화
#endif
}

void i2c_set_master_prescale(uint32_t prescale_mask)
{
    // 진행 중 트랜잭션 완료 대기.
    // 호출 시점이 Sleep 진입 직전이라 사실상 idle 이지만 방어적으로 spin.
    while (!isI2cDriverStatusIdle())
    {
        /* 무한 대기 시 워치독(3.28s)이 동작하므로 별도 타임아웃 불필요 */
    }

    // MASTER_PRESCALE 필드만 교체.
    enableI2cInterface(false);
    uint32_t cfg = I2C0->CFG;
    cfg = (cfg & ~I2C_CFG_MASTER_PRESCALE_Mask) | prescale_mask;
    I2C0->CFG = cfg;
    enableI2cInterface(true);
}

#ifdef CM3_I2c_using_ISR
void I2C_0_IRQHandler(void)
#else
void i2c_comm(void)
#endif
{
    static int              i = 0;
    OTE_1_5gen_I2C_STATUS_T i2cStatus;

    NVIC_ClearPendingIRQ(I2C_0_IRQn);

    i2cStatus.status = getI2cHardwareStatus();

#ifdef CM3_I2c_TestMode
    I2C_statusRegister_dump[dumpIndex++] = (int) i2cStatus.status;
    if (dumpIndex >= 32)
        dumpIndex = 0;
#endif

    switch (i2c_driver.i2c_diver_state)
    {
        case i2c_state_Idle:
        {
            i++;
        }
        break;

        case i2c_state_WriteTriggered:
        {
            // clang-format off
             if (   (i2cStatus.fields.bus_error      == 0)
                 && (i2cStatus.fields.busy           == 1)
                 && (i2cStatus.fields.stop_detected  == 0)
                 && (i2cStatus.fields.clk_stretch    == 1)
                 && (i2cStatus.fields.read_write     == 0)
                 && (i2cStatus.fields.ack            == 0)
                 && (i2cStatus.fields.tx_req         == 1))
             {
                 if (0 < i2c_driver.i2cTxRx_RemaindedDataLength)
                 {
                     I2C0->TX_DATA = *(i2c_driver.p_i2cTx_Source++);
                     i2c_driver.i2cTxRx_RemaindedDataLength--;
                 }

                 if (i2c_driver.i2cTxRx_RemaindedDataLength == 0)
                 {
                     I2C0->CTRL = I2C_LAST_DATA;
                 }
             }
             else if (i2cStatus.fields.stop_detected == 1)
             {
                 if ((i2cStatus.fields.bus_error == 1) || (i2c_driver.i2cTxRx_RemaindedDataLength != 0))
                 {
                     i2c_driver.i2c_diver_state = i2c_state_Error;
                     i2c_driver.i2c_Error_Code  = I2C_SLAVE_DEVICE_NO_REACTION;
                 }
                 else
                 {
                     i2c_driver.i2c_diver_state = i2c_state_WritingDone;
                     I2C0->STATUS = df__clearI2cHardwareStatus;
                 }
             }
             else
             {
                 i++;
             }
            // clang-format on
        }
        break;
#ifndef CM3_I2c_using_ISR
        case i2c_state_WritingDone:
        {
            ST__I2C_DRIVER.i2c_diver_state = i2c_state_Idle;
        }
        break;
#endif
        case i2c_state_ReadTriggerd:
        {
            // clang-format off

             // 처음에 주소에 대한 응답을 받았을 때
             if ((i2cStatus.fields.bus_error        == 0)
                 && (i2cStatus.fields.busy          == 1)
                 && (i2cStatus.fields.stop_detected == 0)
                 && (i2cStatus.fields.clk_stretch   == 1)
                 && (i2cStatus.fields.read_write    == 1)
                 && (i2cStatus.fields.rx_req        == 0)
                 && (i2cStatus.fields.ack           == 0))
             {
                 I2C0->CTRL = I2C_ACK;
             }
             // 데이터 수신했을 때
             else if ((i2cStatus.fields.bus_error        == 0)
                      && (i2cStatus.fields.busy          == 1)
                      && (i2cStatus.fields.stop_detected == 0)
                      && (i2cStatus.fields.clk_stretch   == 1)
                      && (i2cStatus.fields.read_write    == 1)
                      && (i2cStatus.fields.rx_req        == 1)
                      && (i2cStatus.fields.addr_data     == 0))
             {
                 if ((i2c_driver.i2cTxRx_RemaindedDataLength - 1) == 0)
                 {
                     I2C0->CTRL = I2C_NACK | I2C_STOP;
                 }
                 else
                 {
                     I2C0->CTRL = I2C_ACK;
                 }

                 *(i2c_driver.p_i2cRx_Destination) = I2C0->RX_DATA;
                 i2c_driver.p_i2cRx_Destination++;
                 i2c_driver.i2cTxRx_RemaindedDataLength--;
             }
             else if (i2cStatus.fields.stop_detected == 1)
             {
                 if ((i2cStatus.fields.bus_error == 1) || (i2c_driver.i2cTxRx_RemaindedDataLength != 0))
                 {
                     i2c_driver.i2c_diver_state = i2c_state_Error;
                     i2c_driver.i2c_Error_Code  = I2C_SLAVE_DEVICE_NO_REACTION;
                 }
                 else
                 {
                     i2c_driver.i2c_diver_state = i2c_state_ReadingDone;
                     I2C0->STATUS = df__clearI2cHardwareStatus;
                 }
             }
             else
             {
                 i++;
             }
            // clang-format on
        }
        break;
#ifndef CM3_I2c_using_ISR
        case i2c_state_ReadingDone:
        {
            ST__I2C_DRIVER.i2c_diver_state = i2c_state_Idle;
        }
        break;
#endif
        case i2c_state_Error:
        {
            init_I2c();
        }
        break;
        default:
        {
        }
        break;
    }
}
