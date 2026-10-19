#ifndef TDC_UT_BOOT_MOCK_HW_H
#define TDC_UT_BOOT_MOCK_HW_H
#if !defined(TDC_UNIT_TEST) || !defined(TDC_UNIT_TEST_HOST)
#error Host boundary requires TDC_UNIT_TEST and TDC_UNIT_TEST_HOST
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* 주소/비트/구조는 설치된 SDK 원본을 사용한다. 레지스터 접근만 대체한다. */
#include <sk5_map.h>
#include <reg.h>
#include <sk5_hw_cid101.h>
#include <sk5_map_nvm.h>
extern CRC_Type g_tdc_boot_crc_regs;
extern D_CLK_Type g_tdc_boot_clock_regs;
#undef CRC
#define CRC (&g_tdc_boot_crc_regs)
#undef D_CLK
#define D_CLK (&g_tdc_boot_clock_regs)
#undef DMA0
#undef DMA1
#define DMA0 0U
#define DMA1 1U
#undef TIMER2
#define TIMER2 2U
#define DIO8 8U
#define DIO12 12U
#define DIO15 15U
#define DIO32 32U
extern uint32_t SystemCoreClock;
void Sys_Set_CRC_Config(CRC_Type *, uint32_t);
void Sys_CRC_CCITTInitValue(CRC_Type *);
void Sys_CRC_Add(CRC_Type *, uint32_t, uint32_t);
uint32_t Sys_CRC_GetFinalValue(const CRC_Type *);
void Sys_Memory_Enable(uint32_t, uint32_t);
void Sys_PRAMSec_SetLock(uint32_t);
void Sys_DMA_Mode_Enable(unsigned, uint32_t);
uint32_t Sys_DMA_Get_Status(unsigned);
void Sys_DMA_ChannelConfig(unsigned, uint32_t, uint32_t, uint32_t, uintptr_t, uintptr_t);
void Sys_DMA_Set_Ctrl(unsigned, uint32_t);
void Sys_GPIO_Write(unsigned, unsigned);
void Sys_GPIO_Set_Low(unsigned);
void Sys_DIO_Config(unsigned, uint32_t);
unsigned Sys_GPIO_Read(unsigned);
void Sys_Timer_Stop(unsigned);
void Sys_Timer_Start(unsigned);
void Sys_Timer_Config(unsigned, uint32_t, uint32_t, uint32_t);
void NVIC_ClearPendingIRQ(int);
void NVIC_SetPriority(int, unsigned);
void NVIC_EnableIRQ(int);
void NVIC_DisableIRQ(int);
void Sys_Delay(uint32_t);
void tdc_ut_boot_host_reset(void);
#define SYS_WATCHDOG_REFRESH() ((void)0)
#define SYS_WATCHDOG_RESET() tdc_ut_boot_host_reset()
#define TIMER_2_IRQn 14
#endif
