
#ifndef __initialize_h__
#define __initialize_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define ENABLE_UART       1
#define ENABLE_SEGGER_RTT 1

void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

void ci_initialize(void);
void initialize_late(void);

#endif  // __initialize_h__
