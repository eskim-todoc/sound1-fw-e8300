#ifndef TDC_CM3_BATCH_HOST_HW_H
#define TDC_CM3_BATCH_HOST_HW_H
#include <stdint.h>
extern unsigned g_batch_ipsr;
extern uint32_t g_tdc_ut_host_xram[0x14000];
#define __get_IPSR() (g_batch_ipsr)
#define CFX_XMEM_LSB_UNSIGNED_BASE ((uintptr_t)g_tdc_ut_host_xram)
#define CFX_XRAM0_LSB_UNSIGNED_BASE (CFX_XMEM_LSB_UNSIGNED_BASE+0x10000U)
#define CFX_XRAM3_LSB_UNSIGNED_TOP (CFX_XMEM_LSB_UNSIGNED_BASE+0x4ffffU)
#endif
