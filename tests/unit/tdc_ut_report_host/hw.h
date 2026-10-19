#include <stdint.h>
extern uint32_t g_tdc_ut_host_xram[0x14000];
#define CFX_XMEM_LSB_UNSIGNED_BASE ((uintptr_t)g_tdc_ut_host_xram)
#define CFX_XRAM0_LSB_UNSIGNED_BASE (CFX_XMEM_LSB_UNSIGNED_BASE+0x10000U)
#define CFX_XRAM3_LSB_UNSIGNED_TOP (CFX_XMEM_LSB_UNSIGNED_BASE+0x4ffffU)
