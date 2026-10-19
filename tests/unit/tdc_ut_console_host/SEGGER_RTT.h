#ifndef TDC_UT_CONSOLE_HOST_RTT_H
#define TDC_UT_CONSOLE_HOST_RTT_H
unsigned SEGGER_RTT_Write(unsigned channel, const void *data, unsigned count);
unsigned SEGGER_RTT_Read(unsigned channel, void *data, unsigned count);
#endif
