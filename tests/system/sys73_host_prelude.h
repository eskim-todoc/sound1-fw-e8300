/* 실제 SDK 자료형을 사용하되 보드 워치독과 전체 main만 제외한다. */
#include "tdc_ut_cm3_host_prelude.h"
#undef __OTE_1_5_gen_FS_h__
#include <hw.h>
#undef SYS_WATCHDOG_REFRESH
#define SYS_WATCHDOG_REFRESH() ((void)0)
