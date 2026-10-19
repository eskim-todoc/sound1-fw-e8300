#ifndef TDC_UT_IO_CM3_HOST_HW_H
#define TDC_UT_IO_CM3_HOST_HW_H
#if !defined(TDC_UNIT_TEST) || !defined(TDC_UT_IO_CM3_HOST)
#error This hardware fixture is only for the isolated host executable.
#endif

/* SDK 선언/레지스터 정의는 설치된 원본을 사용한다. MMIO와 CPU 명령만 교체한다. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ff.h>
#include <sk5.h>
#undef SystemCoreClock

extern I2C_Type g_tdc_ut_io_cm3_i2c;
extern SPI_Type g_tdc_ut_io_cm3_spi;
extern SPI_STATUS_Type g_tdc_ut_io_cm3_spi_status;
extern DMA_Type g_tdc_ut_io_cm3_dma[2];
extern DMA_CNTS_Type g_tdc_ut_io_cm3_dma_count[2];
extern uint32_t SystemCoreClock;
void tdc_ut_io_cm3_host_delay(uint32_t cycles);
void tdc_ut_io_cm3_host_gpio(int pin, int value);
int tdc_ut_io_cm3_host_gpio_read(int pin);
void tdc_ut_io_cm3_host_i2c_start(int addr, int read);
void tdc_ut_io_cm3_host_i2c_reset(void);
void tdc_ut_io_cm3_host_reset_request(void);

#undef I2C0
#undef SPI1
#undef SPI1_STATUS
#undef DMA0
#undef DMA1
#undef DMA0_CNTS
#undef DMA1_CNTS
#define I2C0 (&g_tdc_ut_io_cm3_i2c)
#define SPI1 (&g_tdc_ut_io_cm3_spi)
#define SPI1_STATUS (&g_tdc_ut_io_cm3_spi_status)
#define DMA0 (&g_tdc_ut_io_cm3_dma[0])
#define DMA1 (&g_tdc_ut_io_cm3_dma[1])
#define DMA0_CNTS (&g_tdc_ut_io_cm3_dma_count[0])
#define DMA1_CNTS (&g_tdc_ut_io_cm3_dma_count[1])
#define Sys_I2C_StartRead(i2c, addr) tdc_ut_io_cm3_host_i2c_start((addr), 1)
#define Sys_I2C_StartWrite(i2c, addr) tdc_ut_io_cm3_host_i2c_start((addr), 0)
#define Sys_I2C_Reset(i2c) tdc_ut_io_cm3_host_i2c_reset()
#define Sys_I2C_Config(i2c, cfg) ((i2c)->CFG = (cfg))
#define Sys_GPIO_Set_High(pin) tdc_ut_io_cm3_host_gpio((pin), 1)
#define Sys_GPIO_Set_Low(pin) tdc_ut_io_cm3_host_gpio((pin), 0)
#define Sys_GPIO_Read(pin) tdc_ut_io_cm3_host_gpio_read(pin)
#define Sys_DIO_Config(pin, cfg) ((void)(pin), (void)(cfg))
#define Sys_Delay(cycles) tdc_ut_io_cm3_host_delay(cycles)
#define Sys_DMA_Mode_Enable(dma, mode) ((void)(dma), (void)(mode))
#define Sys_DMA_ChannelConfig(...) ((void)0)
#define Sys_DMA_Set_Ctrl(...) ((void)0)
#define Sys_DMA_Clear_Status(...) ((void)0)
#define Sys_SPI_TransferConfig(...) ((void)0)
#define Sys_SPI_DIOConfig(...) ((void)0)
#define Sys_SPI_Config(...) ((void)0)
#undef NVIC_ClearPendingIRQ
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef __WFE
#define NVIC_ClearPendingIRQ(...) ((void)0)
#define NVIC_EnableIRQ(...) ((void)0)
#define NVIC_DisableIRQ(...) ((void)0)
#define __WFE() tdc_ut_io_cm3_host_delay(0)
#undef SYS_WATCHDOG_REFRESH
#undef SYS_WATCHDOG_RESET
#define SYS_WATCHDOG_REFRESH() ((void)0)
#define SYS_WATCHDOG_RESET() tdc_ut_io_cm3_host_reset_request()
#endif
