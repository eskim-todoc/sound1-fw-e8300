/* Real target RAM branches, but any SPI WFE is a test failure, not auto-ACK. */
#include "../tdc_ut_io_cm3_host/hw.h"
#undef __WFE
void cm3_batch_forbidden_wait(void);
#define __WFE() cm3_batch_forbidden_wait()
#define __get_IPSR() 0U
