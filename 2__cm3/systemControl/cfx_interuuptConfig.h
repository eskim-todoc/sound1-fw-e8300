#ifndef CFX_InterruptConfig_H__
#define CFX_InterruptConfig_H__

#include "processorDirective.h" //ok

///////////////
// 필터엔진 인터럽트 설정 값
//////////////

// Disable all Filter Engine interrupt sources
#define FENG_INT_DISABLE_VAL 0

// Acknowledge all Filter Engine interrupt sources
#define FENG_INT_ACK_ALL_VAL 0xFFFFFFF

#define FENG_INT_ENABLE_VAL (INT_EBL_FENG_OVERLOAD | INT_EBL_FENG_OUT0)

///////////////
// CFX 인터럽트 설정 값
//////////////

// Configure interrupt multiplexing. Specify UART or HEAR_5; Filter Engine or
// HEAR_6; SDU or HEAR_7.
// In this sample application we choose Filter Engine interrupt instead of
// HEAR_6, although the Filter Engine interrupt is not enabled.
#ifdef CFX_UART_USING_ISR
#define INT_MUX_VAL (INT_SEL_UART | INT_SEL_HEAR_6 | INT_SEL_HEAR_7)
#else
#define INT_MUX_VAL (INT_SEL_HEAR_5 | INT_SEL_HEAR_6 | INT_SEL_HEAR_7)
#endif

#ifdef CFX_I2c_using_ISR

#ifdef CFX_UART_USING_ISR

// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_NormalPowerMode (INT_EBL_MASTER | INT_EBL_WATCHDOG | INT_EBL_I2C | INT_EBL_FIFO_0 | INT_EBL_FIFO_5 | INT_EBL_HEAR_0 | INT_EBL_HEAR_1 | INT_EBL_HEAR_2 | INT_EBL_HEAR_5_UART | INT_EBL_GPIO | INT_EBL_CM3_0 | INT_EBL_CM3_1 | INT_EBL_CM3_2 | INT_EBL_CM3_3)

#else
// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_NormalPowerMode (INT_EBL_MASTER | INT_EBL_WATCHDOG | INT_EBL_I2C | INT_EBL_FIFO_0 | INT_EBL_FIFO_5 | INT_EBL_HEAR_0 | INT_EBL_HEAR_1 | INT_EBL_HEAR_2 | INT_EBL_CM3_0 | INT_EBL_CM3_1 | INT_EBL_CM3_2 | INT_EBL_CM3_3)
#endif

#else

#ifdef CFX_UART_USING_ISR

// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_NormalPowerMode (INT_EBL_MASTER | INT_EBL_WATCHDOG | INT_EBL_FIFO_0 | INT_EBL_FIFO_5 | INT_EBL_HEAR_0 | INT_EBL_HEAR_1 | INT_EBL_HEAR_2 | INT_EBL_HEAR_5_UART | INT_EBL_CM3_0 | INT_EBL_CM3_1 | INT_EBL_CM3_2 | INT_EBL_CM3_3)
#else
// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_NormalPowerMode (INT_EBL_MASTER | INT_EBL_WATCHDOG | INT_EBL_FIFO_0 | INT_EBL_FIFO_5 | INT_EBL_HEAR_0 | INT_EBL_HEAR_1 | INT_EBL_HEAR_2 | INT_EBL_CM3_0 | INT_EBL_CM3_1 | INT_EBL_CM3_2 | INT_EBL_CM3_3)
#endif

#endif

// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_cm3_0 (INT_EBL_MASTER | INT_EBL_CM3_0)

// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_Master_Only (INT_EBL_MASTER)

// 인터럽트 컨틀롤러에서 받아들일 인터럽트 소스 선택
#define INT_ENABLE_VAL_LowPowerMode (INT_EBL_MASTER | INT_EBL_WATCHDOG | INT_EBL_GPIO | INT_EBL_TIMER_1)

// Define the interrupt status value for acknowledging all interrupts during
// initialization
#define INT_STATUS_VAL                  INT_ACK_ALL_INTS

// All interrupts have normal priority
#define INT_PRIORITY_VAL                0






#endif
