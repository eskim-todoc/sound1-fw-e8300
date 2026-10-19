#ifndef TDC_UT_CFX_BATCH_FIXTURE_HW_H
#define TDC_UT_CFX_BATCH_FIXTURE_HW_H
#include <stdint.h>
#include <string.h>
#define _XMEM
#define _IOMEM
extern uint32_t g_tdc_ut_batch_fixture_xram[0x14000];
#define CFX_XMEM_LSB_UNSIGNED_BASE ((uintptr_t)g_tdc_ut_batch_fixture_xram)
#define CFX_XRAM0_LSB_UNSIGNED_BASE (CFX_XMEM_LSB_UNSIGNED_BASE + 0x10000U)
#define CFX_XRAM3_LSB_UNSIGNED_TOP (CFX_XMEM_LSB_UNSIGNED_BASE + 0x4ffffU)
#endif
