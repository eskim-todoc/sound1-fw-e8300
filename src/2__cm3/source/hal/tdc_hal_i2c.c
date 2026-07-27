#include <tdc_hal_i2c.h>
#include <processorDirective.h>
#include <stdbool.h>
#include <stddef.h>

#define CM3_I2c_TestMode

static tdc_hal_i2c_driver_t i2c_driver;

tdc_hal_i2c_driver_state_t tdc_hal_i2c_get_driver_status(void)
{
    return i2c_driver.i2c_diver_state;
}

bool tdc_hal_i2c_is_driver_status_idle(void)
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

void tdc_hal_i2c_set_driver_status_idle(void)
{
    i2c_driver.i2c_diver_state = i2c_state_Idle;
}

void tdc_hal_i2c_clear_driver_status(void)
{
    i2c_driver.i2c_diver_state             = i2c_state_Idle;
    i2c_driver.i2cTxRx_RemaindedDataLength = 0;
    i2c_driver.p_i2cTx_Source              = NULL;
    i2c_driver.p_i2cRx_Destination         = NULL;
    i2c_driver.slaveAddress                = 0;
    i2c_driver.i2c_Error_Code              = TDC_HAL_I2C_NO_ERROR;
}

uint32_t tdc_hal_i2c_get_hardware_status(void)
{
    return I2C0->STATUS;
}

#ifdef CM3_I2c_TestMode
int I2C_statusRegister_dump[32];
int dumpIndex = 0;
#endif

void tdc_hal_i2c_enable_interface(bool isEnabled)
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

void tdc_hal_i2c_clear_hardware_status(void)
{
    I2C0->STATUS = TDC_HAL_I2C_CLEAR_HW_STATUS;
    NVIC_ClearPendingIRQ(I2C_0_IRQn);
}

void tdc_hal_i2c_start_write(const int slaveAddress, int *p_sourcedata, const int dataLength)
{
    tdc_hal_i2c_status_t i2cStatus;

    i2c_driver.slaveAddress                = slaveAddress;
    i2c_driver.p_i2cTx_Source              = p_sourcedata;
    i2c_driver.i2cTxRx_RemaindedDataLength = dataLength;

    i2cStatus.status = tdc_hal_i2c_get_hardware_status();

#ifdef CM3_I2c_TestMode
    I2C_statusRegister_dump[dumpIndex++] = (int) i2cStatus.status;
    if (dumpIndex >= 32)
        dumpIndex = 0;
#endif

    // 버스 Free, 라인 Free 그리고 TX 전송이 가능한 상태에서만 I2C 전송
    if ((i2cStatus.fields.busy == 0) && (i2cStatus.fields.tx_req == 1) && (i2cStatus.fields.line_free == 1))
    {
        i2c_driver.i2c_diver_state = i2c_state_WriteTriggered;

        tdc_hal_i2c_clear_hardware_status();
        Sys_I2C_StartWrite(I2C0, i2c_driver.slaveAddress);
    }
    else
    {
        i2c_driver.i2c_diver_state = i2c_state_Error;
        i2c_driver.i2c_Error_Code  = TDC_HAL_I2C_SLAVE_DEVICE_NO_REACTION;
    }
}

void tdc_hal_i2c_start_read(const int slaveAddress, int *p_destination, const int dataLength)
{
    tdc_hal_i2c_status_t i2cStatus;

    i2c_driver.slaveAddress                = slaveAddress;
    i2c_driver.p_i2cRx_Destination         = p_destination;
    i2c_driver.i2cTxRx_RemaindedDataLength = dataLength;

    i2cStatus.status = tdc_hal_i2c_get_hardware_status();

#ifdef CM3_I2c_TestMode
    I2C_statusRegister_dump[dumpIndex++] = (int) i2cStatus.status;
    if (dumpIndex >= 32)
        dumpIndex = 0;
#endif

    if ((i2cStatus.fields.busy == 0) && (i2cStatus.fields.tx_req == 1) && (i2cStatus.fields.line_free == 1))
    {
        i2c_driver.i2c_diver_state = i2c_state_ReadTriggerd;

        tdc_hal_i2c_clear_hardware_status();
        Sys_I2C_StartRead(I2C0, i2c_driver.slaveAddress);
    }
    else
    {
        i2c_driver.i2c_diver_state = i2c_state_Error;
        i2c_driver.i2c_Error_Code  = TDC_HAL_I2C_SLAVE_DEVICE_NO_REACTION;
    }
}

void tdc_hal_i2c_init(void)
{
    Sys_I2C_Reset(I2C0);
    Sys_I2C_Config(I2C0, TDC_HAL_I2C_CFG_MASTER);
    tdc_hal_i2c_clear_driver_status();
    tdc_hal_i2c_enable_interface(true);

#ifdef TDC_HAL_I2C_USING_ISR
    NVIC_ClearPendingIRQ(I2C_0_IRQn);
    NVIC_EnableIRQ(I2C_0_IRQn);  // I2C0 인터럽트 활성화
#endif
}

void tdc_hal_i2c_set_master_prescale(uint32_t prescale_mask)
{
    // 진행 중 트랜잭션 완료 대기.
    // 호출 시점이 Sleep 진입 직전이라 사실상 idle 이지만 방어적으로 spin.
    while (!tdc_hal_i2c_is_driver_status_idle())
    {
        /* 무한 대기 시 워치독(3.28s)이 동작하므로 별도 타임아웃 불필요 */
    }

    // MASTER_PRESCALE 필드만 교체.
    tdc_hal_i2c_enable_interface(false);
    uint32_t cfg = I2C0->CFG;
    cfg          = (cfg & ~I2C_CFG_MASTER_PRESCALE_Mask) | prescale_mask;
    I2C0->CFG    = cfg;
    tdc_hal_i2c_enable_interface(true);
}

/* TDC_HAL_I2C_USING_ISR 이 항상 정의돼 있어 아래는 늘 인터럽트 핸들러로 컴파일된다.
 * #else 쪽의 tdc_hal_i2c_comm() 은 정의될 수 없는 유령 함수여서 제거했다. */
void I2C_0_IRQHandler(void)
{
    static int              i = 0;
    tdc_hal_i2c_status_t i2cStatus;

    NVIC_ClearPendingIRQ(I2C_0_IRQn);

    i2cStatus.status = tdc_hal_i2c_get_hardware_status();

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
                     i2c_driver.i2c_Error_Code  = TDC_HAL_I2C_SLAVE_DEVICE_NO_REACTION;
                 }
                 else
                 {
                     i2c_driver.i2c_diver_state = i2c_state_WritingDone;
                     I2C0->STATUS = TDC_HAL_I2C_CLEAR_HW_STATUS;
                 }
             }
             else
             {
                 i++;
             }
            // clang-format on
        }
        break;
        /* 위와 같은 이유로 i2c_state_WritingDone 폴링 처리도 제거했다. */
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
                     i2c_driver.i2c_Error_Code  = TDC_HAL_I2C_SLAVE_DEVICE_NO_REACTION;
                 }
                 else
                 {
                     i2c_driver.i2c_diver_state = i2c_state_ReadingDone;
                     I2C0->STATUS = TDC_HAL_I2C_CLEAR_HW_STATUS;
                 }
             }
             else
             {
                 i++;
             }
            // clang-format on
        }
        break;
        /* TDC_HAL_I2C_USING_ISR 미정의 시의 폴링 처리(i2c_state_ReadingDone case)는
         * 제거했다. 이 매크로는 processorDirective.h 와 tdc_hal_i2c.h 양쪽에서 가드 없이
         * 정의돼 있어 #ifndef 가 영구 거짓이었다. */
        case i2c_state_Error:
        {
            tdc_hal_i2c_init();
        }
        break;
        default:
        {
        }
        break;
    }
}
