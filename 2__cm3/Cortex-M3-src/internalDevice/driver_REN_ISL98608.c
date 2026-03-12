
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "board.h"

#if defined(Board_is_OTE_VER_1_3)

#include "driver_i2c.h"
#include "driver_REN_ISL98608.h"


bool write_REN_ISL98608_register_byCM3_I2C(int registerAddr, int value)
{
	EN__I2C_DRIVER_STATE i2cDriverState;
	int transferBuffer[2];
	bool PassFail = false;

	transferBuffer[0] = (int) registerAddr;
	transferBuffer[1] = (int) value;
	i2c_startWriteData(REN_ISL98608_SlaveAddr, transferBuffer, 2);

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


bool read_REN_ISL98608_register_byCM3_I2C(int registerAddr, int *read_value)
{
	EN__I2C_DRIVER_STATE i2cDriverState;
	bool PassFail = false;
	int transferBuffer[2];

	// 읽기 : 선행 명령 전송 (
	transferBuffer[0] = registerAddr;
	i2c_startWriteData(REN_ISL98608_SlaveAddr, transferBuffer, 1);

	while (1)
	{
		i2cDriverState = get_i2cDriverStatus();
		if (i2cDriverState == i2c_state_WritingDone)
		{
			setI2cDriverStatusIdle();

			// 읽기 : 레지스터 값 읽어들임
			i2c_startReadData(REN_ISL98608_SlaveAddr, &transferBuffer[1], 1);

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





bool Reset_REN_ISL98608(void)
{


	int value;
	int readValue;


	value=0x80;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,value);	// reset IC





#if defined(Error_ISL98608_ReadByte)


	value=0x80;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,value);	// reset IC


	return true;

#else

	value=0x80;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,value);	// reset IC


	read_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,&readValue)

	if(value==readValue)
		return true;
	else
		return false;


#endif

}


bool Reset_disableOutup_REN_ISL98608(void)
{


	int value;
	int readValue;



#if defined(Error_ISL98608_ReadByte)


	value=0x80;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,value);	// reset IC


	value=0x20;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,value);


	return true;

#else

	value=0x80;

	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,value);	// reset IC

	read_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_FaultStatus,&readValue)
	if(value==readValue)
	{
		value=0x20;
		write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,value);

		read_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,&readValue)

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

bool enable_VP_REN_ISL98608(void)
{


	int value;
	int readValue;






#if defined(Error_ISL98608_ReadByte)

	value=0x25;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,value);

	return true;

#else

	value=0x25;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,value);

	read_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,&readValue)

	if(value==readValue)
		return true;
	else
		return false;


#endif

}


bool disable_REN_ISL98608(void)
{


	int value;
	int readValue;







#if defined(Error_ISL98608_ReadByte)

	value=0x20;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,0x05);

	return true;

#else


	value=0x20;
	write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,0x05);
	read_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_Enable,&readValue)


	if(value==readValue)
		return true;
	else
		return false;


#endif

}
#endif
