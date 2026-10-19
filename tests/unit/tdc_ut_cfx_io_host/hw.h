#ifndef TDC_UT_CFX_IO_HOST_HW_H
#define TDC_UT_CFX_IO_HOST_HW_H
#include <stdint.h>
#include <assert.h>
#define _XMEM
#define _IOMEM
#define chess_storage(x)
#define chess_loop_range(a,b)
#define chess_unroll_loop(x)
typedef struct { int CTRL, STATUS; } PCM_Type;
typedef struct { int ACCESS; } D_FIFO_Type;
extern D_FIFO_Type g_tdc_ut_host_fifo;
#define D_FIFO_A1_0 (&g_tdc_ut_host_fifo)
#define D_HEAR_A0MEM_BASE 0
#define D_HEAR_A1MEM_BASE 0
#define D_HEAR_C0MEM_BASE 0
#define D_HEAR_H4MEM_BASE 0
#define D_DSP_PRAM1_BASE 0
#define D_DSP_PRAM4_BASE 0
#define disable_interrupts() ((void)0)
#define enable_interrupts() ((void)0)
/* SDK 설정 기호를 기록한다. 실제 레지스터 비트와 클럭 동작은 검증하지 않는다. */
void tdc_ut_cfx_io_host_adc(int channel, const char *setting);
void tdc_ut_cfx_io_host_dio(const char *pin, const char *setting);
#define SYS_SET_ADC_DEC_CTRL(a,b,c) tdc_ut_cfx_io_host_adc((b), #c)
#define Sys_DIO_Config(a,b) tdc_ut_cfx_io_host_dio(#a, #b)
#endif
