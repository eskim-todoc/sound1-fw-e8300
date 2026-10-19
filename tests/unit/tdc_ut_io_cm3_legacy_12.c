/* 호스트에서만 구형 보드 TU를 선택한다. 제품 board.h와 빌드 설정은 변경하지 않는다. */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include <hw.h>
#include <board.h>
#include <tdc_hal_i2c.h>
#undef Board_is_OTE_VER_1_5
#define Board_is_OTE_VER_1_2
#include "legacy/tdc_drv_isl91128.inc"
#endif
