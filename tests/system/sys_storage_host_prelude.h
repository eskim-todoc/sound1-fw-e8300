/* PC 전용 경계. 실제 파일/NVM/MMIO를 열지 않고 제품 파일/AES 함수는 링크한다. */
#include "../unit/tdc_ut_cm3_host_prelude.h"
#undef __OTE_1_5_gen_FS_h__
#include <hw.h>
#undef SYS_WATCHDOG_REFRESH
#define SYS_WATCHDOG_REFRESH() ((void)0)
