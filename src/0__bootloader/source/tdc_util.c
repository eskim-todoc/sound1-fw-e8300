
#include <tdc_util.h>

void tdc_delay_ms(uint32_t ms)
{
    Sys_Delay((SystemCoreClock / 1000) * ms);
}

void tdc_delay_us(uint32_t us)
{
    Sys_Delay((SystemCoreClock / 1000000) * us);
}
