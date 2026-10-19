/* 호스트에서만 구형 보드 TU를 선택한다. 제품 board.h와 빌드 설정은 변경하지 않는다. */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include <hw.h>
#include <board.h>
#include <driver_i2c.h>
#undef Board_is_OTE_VER_1_5
#define Board_is_OTE_VER_1_2
#include "../../src/2__cm3/Cortex-M3-src/internalDevice/driver_REN_ISL91128.c"
#endif
