#ifndef TDC_CONSOLE_ADAPTER_HOST_HW_H
#define TDC_CONSOLE_ADAPTER_HOST_HW_H
#include <stdint.h>
extern uint32_t g_console_adapter_xram[0x14000];
#define CFX_XMEM_LSB_UNSIGNED_BASE ((uintptr_t)g_console_adapter_xram)
#define CFX_XRAM0_LSB_UNSIGNED_BASE (CFX_XMEM_LSB_UNSIGNED_BASE+0x10000U)
#define CFX_XRAM3_LSB_UNSIGNED_TOP (CFX_XMEM_LSB_UNSIGNED_BASE+0x4ffffU)
extern unsigned g_console_adapter_ipsr;
void console_adapter_watchdog(void);
#define __get_IPSR() g_console_adapter_ipsr
#define SYS_WATCHDOG_REFRESH() console_adapter_watchdog()
#endif
