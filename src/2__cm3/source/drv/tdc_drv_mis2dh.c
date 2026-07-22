#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <tdc_hal_i2c.h>

#include <tdc_drv_mis2dh.h>

#ifdef UART_isDedicated_CM3_DATA
#include <02_cfx_cm3_communication_Data_block.h>
#endif

bool write_MIS2DH_Register_byCM3_I2C(tdc_drv_mis2dh_register_t Register)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    int                  transferBuffer[2];
    bool                 PassFail = false;

    transferBuffer[0] = (int) Register.addrSubRegister;
    transferBuffer[1] = (int) Register.data;
    tdc_hal_i2c_start_write(TDC_DRV_MIS2DH_I2C_ADDR, transferBuffer, 2);

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

bool read_MIS2DH_Register_byCM3_I2C(tdc_drv_mis2dh_register_t *Register)
{
    tdc_hal_i2c_driver_state_t i2cDriverState;
    bool                 PassFail = false;
    int                  transferBuffer[2];

    // 읽기 : 선행 명령 전송
    transferBuffer[0] = (int) Register->addrSubRegister;
    tdc_hal_i2c_start_write(TDC_DRV_MIS2DH_I2C_ADDR, transferBuffer, 1);

    while (1)
    {
        i2cDriverState = tdc_hal_i2c_get_driver_status();
        if (i2cDriverState == i2c_state_WritingDone)
        {
            tdc_hal_i2c_set_driver_status_idle();

            // 읽기 : 레지스터 값 읽어들임
            tdc_hal_i2c_start_read(TDC_DRV_MIS2DH_I2C_ADDR, &transferBuffer[1], 1);
        }

        if (i2cDriverState == i2c_state_ReadingDone)
        {
            Register->data = (char) transferBuffer[1];

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

bool tdc_drv_mis2dh_reset(void)
{

    bool                  PassFail = true;
    tdc_drv_mis2dh_register_t Register;

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_STATUS_REG_AUX;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_OUT_TEMP_L;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_OUT_TEMP_H;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT_COUNTER_REG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TEMP_CFG_REG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG1;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG2;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG3;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG4;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG5;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG6;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_REF_DAT_CAP;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_STATUS_REG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_FIFO_CTRL_REG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_FIFO_SCR_REG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT1_CFG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT1_SRC;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT1_THS;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT1_DURATION;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT2_CFG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT2_SRC;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT2_THS;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT2_DURATION;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CLICK_CFG;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CLICK_THS;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TIME_LIMIT;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TIME_LATENCY;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TIME_WINDOW;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_ACT_THS;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_ACT_DUR;
        Register.data            = 0x00;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    return PassFail;
}

bool tdc_drv_mis2dh_configure_click_mode(int numActivation)
{
    //
    bool                  PassFail = true;
    tdc_drv_mis2dh_register_t Register;

    tdc_drv_mis2dh_reset();

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG1;
        Register.data            = 0x7C; // 400 Hz, Low-power mode, X/Y/Z enable;

        PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG2;
        Register.data            = 0x94; // HPF: Normal mode, HPF Cutoff: 0b01, FDS enable, HPF Click: enable
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG3; // 인터럽트 PAD 1설정
        Register.data            = 0x80;             // 0x80: 클릭인터럽트만 설정
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG6; // 인터럽트 PAD 2설정
        Register.data            = 0x80;             // 0x80: 클릭인터럽트만 설정
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail) //
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG4;
        Register.data            = 0x10; // 16g : b11  8g : b10
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_INT1_DURATION;
        Register.data            = 0x7f; //
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CLICK_CFG; // 클릭
        if (numActivation == 1)
        {
            Register.data = 0x10; // single click.( z축 싱글)
            PassFail      = write_MIS2DH_Register_byCM3_I2C(Register);
        }
        else if (numActivation == 2)
        {
            Register.data = 0x20; // double click.( z축 더블)
            PassFail      = write_MIS2DH_Register_byCM3_I2C(Register);
        }
        else
            PassFail = false;
    }

    if (PassFail)
    {

        Register.addrSubRegister = TDC_DRV_MIS2DH_CLICK_THS; // 클럭 문턱값
        Register.data            = 0x1F;             // 0~127
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TIME_LIMIT;
        Register.data            = 0x02; //
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TIME_LATENCY;
        Register.data            = 0x28; //
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_TIME_WINDOW;
        Register.data            = 0x78; //
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    return PassFail;
}

bool tdc_drv_mis2dh_configure_xyz_stream(void)
{

    bool                  PassFail = false;
    tdc_drv_mis2dh_register_t Register;

    Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG1;
    Register.data            = 0x5f; // 100 Hz, Low-power mode, X/Y/Z enable;

    PassFail = write_MIS2DH_Register_byCM3_I2C(Register);

#if 0
        // 하이패스필터를 거친 출력은 중력가속도 값이 없어진다.
        if (PassFail) {
            Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG2;
            Register.data = 0x08; // filtered Data to output Register , cut-off = 0.02*samplingFreq
            PassFail = write_MIS2DH_Register_byCM3_I2C(Register);
        }
#endif

    if (PassFail)
    {
        Register.addrSubRegister = TDC_DRV_MIS2DH_CTRL_REG4;
        Register.data            = 0x20; // 2g :0x00, 4g :0x10, 8g :0x20, 16g :0x30
        PassFail                 = write_MIS2DH_Register_byCM3_I2C(Register);
    }

    return PassFail;
}

static tdc_drv_mis2dh_stream_xyz_t accelerationValue;

bool tdc_drv_mis2dh_update_xyz_acceleration(void)
{
    bool PassFail = false;
    int  tempValue;

    tdc_drv_mis2dh_register_t Register_xH;
    tdc_drv_mis2dh_register_t Register_yH;
    tdc_drv_mis2dh_register_t Register_zH;

    Register_xH.addrSubRegister = TDC_DRV_MIS2DH_OUT_X_H;
    Register_yH.addrSubRegister = TDC_DRV_MIS2DH_OUT_Y_H;
    Register_zH.addrSubRegister = TDC_DRV_MIS2DH_OUT_Z_H;

    PassFail = read_MIS2DH_Register_byCM3_I2C(&Register_xH);
    if (PassFail)
    {
        PassFail = read_MIS2DH_Register_byCM3_I2C(&Register_yH);
    }
    if (PassFail)
    {
        PassFail = read_MIS2DH_Register_byCM3_I2C(&Register_zH);
    }

    if (PassFail)
    {

        tempValue                        = ((int) Register_xH.data) << 24; // char형 데이터를 int형으로 변환.
        accelerationValue.x_acceleration = tempValue >> 24;

        tempValue                        = ((int) Register_yH.data) << 24;
        accelerationValue.y_acceleration = tempValue >> 24;

        tempValue                        = ((int) Register_zH.data) << 24;
        accelerationValue.z_acceleration = tempValue >> 24;
    }
#if 1 // 에러가 발생했을 때 다시 센서를 초기화 한다?
    else
    {
        tdc_hal_i2c_init();
        tdc_drv_mis2dh_configure_xyz_stream();
    }
#endif

    return PassFail;
}

#if 0

char errorString[]="communication Error\r\n";
char singleClick[]="Single Click\r\n";
char doubleClick[]="Double Click\r\n";
char tripleClick[]="Triple Click\r\n";

#endif

#ifdef UART_isDedicated_CM3_DATA

void tdc_drv_mis2dh_transfer_acceleration_value(bool resultTrue)
{

    int i;

    char       StringBuff_For_UART[sharedBufferLengthForUartRx_fromCM3];
    char       tempString[10];
    int        index_UART_stringBuff = 0;
    static int sendingDataindex      = 1;

    if (resultTrue)
    {

        for (i = 0; i < 10; i++)
        {
            tempString[i] = 0;
        }

        sprintf(tempString, "%d", sendingDataindex); // 1.2msec
        sendingDataindex++;

        for (i = 0, index_UART_stringBuff = 0; i < 10; i++)
        {
            if (tempString[i] != 0)
            {
                StringBuff_For_UART[index_UART_stringBuff++] = tempString[i];
            }
            else
            {
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0D; // Carriage Return
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0A; // New Line
                break;
            }
        }

        for (i = 0; i < 10; i++)
            tempString[i] = 0;
        sprintf(tempString, "%d", accelerationValue.x_acceleration); // 1.2msec
        for (i = 0; i < 10; i++)
        {
            if (tempString[i] != 0)
            {
                StringBuff_For_UART[index_UART_stringBuff++] = tempString[i];
            }
            else
            {
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0D; // Carriage Return
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0A; // New Line
                break;
            }
        }

        for (i = 0; i < 10; i++)
            tempString[i] = 0;

        sprintf(tempString, "%d", accelerationValue.y_acceleration);
        for (i = 0; i < 10; i++)
        {
            if (tempString[i] != 0)
            {
                StringBuff_For_UART[index_UART_stringBuff++] = tempString[i];
            }
            else
            {
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0D; // Carriage Return
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0A; // New Line

                break;
            }
        }

        for (i = 0; i < 10; i++)
            tempString[i] = 0;

        sprintf(tempString, "%d", accelerationValue.z_acceleration);
        for (i = 0; i < 10; i++)
        {
            if (tempString[i] != 0)
            {
                StringBuff_For_UART[index_UART_stringBuff++] = tempString[i];
            }
            else
            {
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0D; // Carriage Return
                StringBuff_For_UART[index_UART_stringBuff++] = 0x0A; // New Line
                break;
            }
        }
    }
    else
    {
        index_UART_stringBuff                        = 0;
        StringBuff_For_UART[index_UART_stringBuff++] = 'S';
        StringBuff_For_UART[index_UART_stringBuff++] = 'e';
        StringBuff_For_UART[index_UART_stringBuff++] = 'n';
        StringBuff_For_UART[index_UART_stringBuff++] = 's';
        StringBuff_For_UART[index_UART_stringBuff++] = 'o';
        StringBuff_For_UART[index_UART_stringBuff++] = 'r';
        StringBuff_For_UART[index_UART_stringBuff++] = ' ';
        StringBuff_For_UART[index_UART_stringBuff++] = 'e';
        StringBuff_For_UART[index_UART_stringBuff++] = 'r';
        StringBuff_For_UART[index_UART_stringBuff++] = 'r';
        StringBuff_For_UART[index_UART_stringBuff++] = 'o';
        StringBuff_For_UART[index_UART_stringBuff++] = 'r';
        StringBuff_For_UART[index_UART_stringBuff++] = 0x0D; // Carriage Return
        StringBuff_For_UART[index_UART_stringBuff++] = 0x0A; // New Line
    }

    if (isUartBufferUsable())
    {
        write_SharedMem_cfx_cm3_UARTBuffer(StringBuff_For_UART, index_UART_stringBuff);
    }
    else
    {
    }
}

#endif
