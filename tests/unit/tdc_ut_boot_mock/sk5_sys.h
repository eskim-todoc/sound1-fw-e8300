#ifndef TDC_UT_BOOT_MOCK_SYS_H
#define TDC_UT_BOOT_MOCK_SYS_H
#include <stdint.h>
extern uint32_t g_tdc_boot_manu[128], g_tdc_boot_key[4], g_tdc_boot_app_number;
#define SYSVAR_MANU_TABLE g_tdc_boot_manu
#define SYSVAR_KEY_START g_tdc_boot_key
#define SYSVAR_BOOT_APPN (&g_tdc_boot_app_number)
#endif
