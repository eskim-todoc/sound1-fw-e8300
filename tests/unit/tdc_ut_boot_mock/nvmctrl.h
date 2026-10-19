#ifndef TDC_UT_BOOT_MOCK_NVMCTRL_H
#define TDC_UT_BOOT_MOCK_NVMCTRL_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t config; } NVMCTRL_Options_t;
void NVMGetDefaultOptions(NVMCTRL_Options_t *);
int NVMInit(NVMCTRL_Options_t *);
void NVMReInitOptions(NVMCTRL_Options_t *);
int NVMReadBytes(uint32_t, uint32_t, uint32_t *);
bool NVMSync(void);
#endif
