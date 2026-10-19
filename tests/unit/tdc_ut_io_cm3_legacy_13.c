/* Error_ISL98608_ReadByte는 원본 헤더가 선택한다. 비활성 readback 분기는 시험하지 않는다. */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include <hw.h>
#include <board.h>
#include <driver_i2c.h>
#undef Board_is_OTE_VER_1_5
#define Board_is_OTE_VER_1_3
#include "../../src/2__cm3/Cortex-M3-src/internalDevice/driver_REN_ISL98608.c"
#endif
