/* 호스트 관찰용 include fixture. 실제 초기화 FSM의 private 상태만 조회한다. */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include "../../src/2__cm3/Cortex-M3-src/systemControl/tdc_touch.c"
int tdc_ut_io_cm3_touch_ready(void) { return s_init_state == TDC_TOUCH_INIT_READY; }
#endif
