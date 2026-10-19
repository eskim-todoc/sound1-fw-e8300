/* Error_ISL98608_ReadByte는 원본 헤더가 선택한다. 비활성 readback 분기는 시험하지 않는다. */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include <hw.h>
#include <board.h>
#include <tdc_hal_i2c.h>
#undef Board_is_OTE_VER_1_5
#define Board_is_OTE_VER_1_3
#include "legacy/tdc_drv_isl98608.inc"
#endif
