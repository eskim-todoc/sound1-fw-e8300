
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "board.h"

#if defined(Board_is_OTE_VER_1_3)

#include "tdc_hal_i2c.h"
#include "tdc_drv_isl98608.h"


bool tdc_drv_isl98608_write_register(int registerAddr, int value)
{
	tdc_hal_i2c_driver_state_t i2cDriverState;
	int transferBuffer[2];
	bool PassFail = false;

	transferBuffer[0] = (int) registerAddr;
	transferBuffer[1] = (int) value;
	tdc_hal_i2c_start_write(TDC_DRV_ISL98608_SLAVE_ADDR, transferBuffer, 2);

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


bool tdc_drv_isl98608_read_register(int registerAddr, int *read_value)
{
	tdc_hal_i2c_driver_state_t i2cDriverState;
	bool PassFail = false;
	int transferBuffer[2];

	// 읽기 : 선행 명령 전송 (
	transferBuffer[0] = registerAddr;
	tdc_hal_i2c_start_write(TDC_DRV_ISL98608_SLAVE_ADDR, transferBuffer, 1);

	while (1)
	{
		i2cDriverState = tdc_hal_i2c_get_driver_status();
		if (i2cDriverState == i2c_state_WritingDone)
		{
			tdc_hal_i2c_set_driver_status_idle();

			// 읽기 : 레지스터 값 읽어들임
			tdc_hal_i2c_start_read(TDC_DRV_ISL98608_SLAVE_ADDR, &transferBuffer[1], 1);

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





bool tdc_drv_isl98608_reset(void)
{


	int value;
	int readValue;


	value=0x80;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,value);	// reset IC





#if defined(TDC_DRV_ISL98608_ERR_READ_BYTE)


	value=0x80;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,value);	// reset IC


	return true;

#else

	value=0x80;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,value);	// reset IC


	tdc_drv_isl98608_read_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,&readValue)

	if(value==readValue)
		return true;
	else
		return false;


#endif

}


bool tdc_drv_isl98608_reset_disable_output(void)
{


	int value;
	int readValue;



#if defined(TDC_DRV_ISL98608_ERR_READ_BYTE)


	value=0x80;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,value);	// reset IC


	value=0x20;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_ENABLE,value);


	return true;

#else

	value=0x80;

	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,value);	// reset IC

	tdc_drv_isl98608_read_register(TDC_DRV_ISL98608_REG_FAULTSTATUS,&readValue)
	if(value==readValue)
	{
		value=0x20;
		tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_ENABLE,value);

		tdc_drv_isl98608_read_register(TDC_DRV_ISL98608_REG_ENABLE,&readValue)

		if(value==readValue)
			return true;
		else
			return false;


	}
	else
	{
		return false;
	}



	if(value==readValue)
		return true;
	else
		return false;


#endif

}

bool tdc_drv_isl98608_enable_vp(void)
{


	int value;
	int readValue;






#if defined(TDC_DRV_ISL98608_ERR_READ_BYTE)

	value=0x25;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_ENABLE,value);

	return true;

#else

	value=0x25;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_ENABLE,value);

	tdc_drv_isl98608_read_register(TDC_DRV_ISL98608_REG_ENABLE,&readValue)

	if(value==readValue)
		return true;
	else
		return false;


#endif

}


bool tdc_drv_isl98608_disable(void)
{


	int value;
	int readValue;







#if defined(TDC_DRV_ISL98608_ERR_READ_BYTE)

	value=0x20;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_ENABLE,0x05);

	return true;

#else


	value=0x20;
	tdc_drv_isl98608_write_register(TDC_DRV_ISL98608_REG_ENABLE,0x05);
	tdc_drv_isl98608_read_register(TDC_DRV_ISL98608_REG_ENABLE,&readValue)


	if(value==readValue)
		return true;
	else
		return false;


#endif

}
#endif
