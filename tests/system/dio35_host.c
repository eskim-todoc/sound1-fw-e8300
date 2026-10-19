/* Host observations only. Real source function bodies are included by the
 * checker; peripheral calls below are event sinks, not a hardware simulator. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define DIO35 35
#define DIO_1X_DRIVE 0
#define DIO_LPF_DISABLE 0
#define DIO_NO_PULL 0
#define DIO_MODE_GPIO_OUT 3
#define df_True 1
#define DIO_PIN_INDEX_for_LED_color_R 8
#define DIO_PIN_INDEX_for_LED_color_G 15
#define DIO_PIN_INDEX_for_LED_color_B 12
#define DIO_PIN_INDEX_for_FPGA_SLEEP 4
#define NRF_SPI_CS_PIN 21
#define GPIO_PIN_ReadCommandForSPI_Master 20
#define DIO_PIN_INDEX_for_Accelerometer 16
#define DIO_PIN_INDEX_for_CM3_SCL 11
#define DIO_PIN_INDEX_for_CM3_SDA 9
#define DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP 0x1003
#define CM3_DIO_PIN_CFG_FOR_GPIO_OUPUT_NOPULLUP 0x2803
#define OTE_1_5_GEN_DIO_CFG_NORMAL_ACCEL_INT 1
#define I2C_PIN_CFG_VAL_forDIO 0x3800
#define I2C0 0

enum { PcmBitStream_Mode_Preamble, PcmBitStream_Mode_LiveStimulation,
       PcmBitStream_Mode_SepcificCommand, PcmBitStream_Mode_NopStandby,
       PcmBitStream_Mode_NopBacktel };
static struct { int pcm_out; } g_interrupt_flags;
static int g_pcm_mode;
static int qcc_done, dio35_configured, low_count, high_count, calls, saw_high;
static int expected_mode;
static uint64_t latch;

static void Sys_GPIO_Set_Low(int pin)
{
    if (pin == DIO35) {
        assert(qcc_done);
        ++low_count;
    }
    latch &= ~(UINT64_C(1) << pin);
}
static void Sys_GPIO_Set_High(int pin)
{
    if (pin == DIO35) {
        assert(dio35_configured);
        ++high_count;
    }
    latch |= UINT64_C(1) << pin;
}
static void Sys_DIO_Config(int pin, int config)
{
    if (pin == DIO35) {
        assert(qcc_done && low_count == 1);
        assert((latch & (UINT64_C(1) << DIO35)) == 0);
        assert(config == 3);
        dio35_configured = 1;
    }
}
static void Sys_I2C_DIOConfig(int bus, int config, int scl, int sda)
{
    (void)bus; (void)config; (void)scl; (void)sda;
}
static void snd_qcc_init(void) { qcc_done = 1; }
static void pcm_mode_call(int mode)
{
    assert(mode == expected_mode);
    ++calls;
    saw_high = !!(latch & (UINT64_C(1) << DIO35));
}
static void fn_PcmBitStream_Mode_Preamble(void) { pcm_mode_call(0); }
static void fn_PcmBitStream_Mode_LiveStimulation(void) { pcm_mode_call(1); }
static void fn_PcmBitStream_Mode_SepcificCommand(void) { pcm_mode_call(2); }
static void fn_PcmBitStream_Mode_NopStandby(void) { pcm_mode_call(3); }
static void fn_PcmBitStream_Mode_NopBacktel(void) { pcm_mode_call(4); }
static void fn_PcmBitStream_Mode_FillZero(void) { pcm_mode_call(5); }

#include "firmware_functions.inc"

int main(void)
{
    const uint64_t marker = UINT64_C(1) << DIO35;
    const uint64_t peer = UINT64_C(1) << 34;
    latch = marker | peer; /* Stale output latch must become Low before mode. */
    assert(ci_dio_configure_normal() == df_True);
    assert(qcc_done && (latch & peer));
    assert(dio35_configured == TDC_SYSTEM_TEST_DIO35_ENABLE);
    assert(low_count == TDC_SYSTEM_TEST_DIO35_ENABLE);
    assert(!!(latch & marker) == !TDC_SYSTEM_TEST_DIO35_ENABLE);
    latch &= ~marker;
    low_count = high_count = 0;
    for (int cycle = 0; cycle < 100; ++cycle) {
        for (int mode = 0; mode < 6; ++mode) {
            g_interrupt_flags.pcm_out = 0;
            g_pcm_mode = mode;
            expected_mode = mode;
            FIFO_4_ISR();
            assert(g_interrupt_flags.pcm_out == 1);
            assert(!!(latch & marker) == TDC_SYSTEM_TEST_DIO35_ENABLE);
            pcmDataOut();
            assert(saw_high == TDC_SYSTEM_TEST_DIO35_ENABLE);
            assert((latch & marker) == 0 && (latch & peer));
        }
    }
    assert(calls == 600);
    assert(high_count == 600 * TDC_SYSTEM_TEST_DIO35_ENABLE);
    assert(low_count == 600 * TDC_SYSTEM_TEST_DIO35_ENABLE);
    /* A repeated IRQ while High cannot encode a second rising edge. */
    FIFO_4_ISR();
    FIFO_4_ISR();
    assert(!!(latch & marker) == TDC_SYSTEM_TEST_DIO35_ENABLE);
    puts("HOST_PASS: init order, 600 mode cycles, peer bit, overlapping IRQ limitation");
    return 0;
}
