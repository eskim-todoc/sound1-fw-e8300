#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <board.h>


#if defined(Board_is_OTE_VER_1_2)


#include <tdc_hal_i2c.h>
#include <tdc_drv_isl91128.h>





bool tdc_drv_isl91128_write_register(int registerAddr, int value)
{
	tdc_hal_i2c_driver_state_t i2cDriverState;
	int transferBuffer[2];
	bool PassFail = false;

	transferBuffer[0] = (int) registerAddr;
	transferBuffer[1] = (int) value;
	tdc_hal_i2c_start_write(TDC_DRV_ISL91128_SLAVE_ADDR, transferBuffer, 2);

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


bool tdc_drv_isl91128_read_register(int registerAddr, int *read_value)
{
	tdc_hal_i2c_driver_state_t i2cDriverState;
	bool PassFail = false;
	int transferBuffer[2];

	// 읽기 : 선행 명령 전송
	transferBuffer[0] = registerAddr;
	tdc_hal_i2c_start_write(TDC_DRV_ISL91128_SLAVE_ADDR, transferBuffer, 1);

	while (1) {
		i2cDriverState = tdc_hal_i2c_get_driver_status();
		if (i2cDriverState == i2c_state_WritingDone)
		{
			tdc_hal_i2c_set_driver_status_idle();

			// 읽기 : 레지스터 값 읽어들임
			tdc_hal_i2c_start_read(TDC_DRV_ISL91128_SLAVE_ADDR, &transferBuffer[1], 1);

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


bool tdc_drv_isl91128_reset(void)
{


	int value;
	int readValue;

	// i2C로 설정한 DCDC값을 활성화한다.
	value=1<<TDC_DRV_ISL91128_ENALBE_I2C_CONTROL_BITPOSITION;

	//value=value|(0x3F<<TDC_DRV_ISL91128_VOLTAGECONTROL_BITPOSITION);
	value=value|(TDC_DRV_PMIC_RESET_VOLTAGE_CONTROL_VALUE<<TDC_DRV_ISL91128_VOLTAGECONTROL_BITPOSITION);





	tdc_drv_isl91128_write_register(TDC_DRV_ISL91128_REG_VOLTAGECONTROL,value);

	tdc_drv_isl91128_read_register(TDC_DRV_ISL91128_REG_VOLTAGECONTROL,&readValue);

	if(value==readValue)
		return true;
	else
		return false;


}


#endif
