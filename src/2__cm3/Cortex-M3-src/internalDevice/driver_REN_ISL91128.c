#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "board.h"


#if defined(Board_is_OTE_VER_1_2)


#include "driver_i2c.h"
#include "driver_REN_ISL91128.h"





bool write_REN_ISL91128_register_byCM3_I2C(int registerAddr, int value)
{
	EN__I2C_DRIVER_STATE i2cDriverState;
	int transferBuffer[2];
	bool PassFail = false;

	transferBuffer[0] = (int) registerAddr;
	transferBuffer[1] = (int) value;
	i2c_startWriteData(REN_ISL91128_SlaveAddr, transferBuffer, 2);

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


bool read_REN_ISL91128_register_byCM3_I2C(int registerAddr, int *read_value)
{
	EN__I2C_DRIVER_STATE i2cDriverState;
	bool PassFail = false;
	int transferBuffer[2];

	// 읽기 : 선행 명령 전송
	transferBuffer[0] = registerAddr;
	i2c_startWriteData(REN_ISL91128_SlaveAddr, transferBuffer, 1);

	while (1) {
		i2cDriverState = get_i2cDriverStatus();
		if (i2cDriverState == i2c_state_WritingDone)
		{
			setI2cDriverStatusIdle();

			// 읽기 : 레지스터 값 읽어들임
			i2c_startReadData(REN_ISL91128_SlaveAddr, &transferBuffer[1], 1);

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


bool Reset_REN_ISL91128(void)
{


	int value;
	int readValue;

	// i2C로 설정한 DCDC값을 활성화한다.
	value=1<<en__enalbe_I2C_Control_BitPosition;

	//value=value|(0x3F<<en__voltageControl_BitPosition);
	value=value|(ResetVoltageControlValue<<en__voltageControl_BitPosition);





	write_REN_ISL91128_register_byCM3_I2C(REN_ISL91128_registerAddr_voltageControl,value);

	read_REN_ISL91128_register_byCM3_I2C(REN_ISL91128_registerAddr_voltageControl,&readValue);

	if(value==readValue)
		return true;
	else
		return false;


}


#endif
