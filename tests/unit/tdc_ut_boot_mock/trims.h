#ifndef TDC_UT_BOOT_MOCK_TRIMS_H
#define TDC_UT_BOOT_MOCK_TRIMS_H
#include <stdint.h>
void Sys_Trims_LoadManuTable(uint32_t *);
void Sys_Trims_SetVREGAndLSAD(void);
void Sys_Trims_SetVDDIF(unsigned);
void Sys_Trims_SetVDDA(unsigned);
void Sys_Trims_SetVDDC(unsigned);
void Sys_Trims_SetVDDC_CP(unsigned);
void Sys_Trims_SetVDDM(unsigned);
void Sys_Trims_SetVDDM_CP(unsigned);
void Sys_Trims_SetVDDOD(unsigned);
void Sys_Trims_SetVMIC(unsigned);
void Sys_Trims_SetADCOffsets(void);
void Sys_Trims_SetOperatingFrequency(unsigned);
#endif
