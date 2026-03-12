/**
 * @file asm_interrupt_vector_table.S
 */

#include <sk5_asm.h>

/* Undef any ISRs that we use in the table below and are provided externally */
.undef global text HEAR_0_ISR
.undef global text HEAR_1_ISR

.undef global text FIFO_0_ISR
.undef global text FIFO_1_ISR
.undef global text FIFO_2_ISR
.undef global text FIFO_3_ISR
.undef global text FIFO_4_ISR
.undef global text FIFO_5_ISR
.undef global text FIFO_6_ISR

.undef global text CM3_0_ISR
.undef global text CM3_1_ISR

// ----------------------------------------------------------------------------
// Interrupt Vector Table
// ----------------------------------------------------------------------------
.text global .at [D_INT_VECT_BASE] _ivt
    goto Watchdog_ISR            // Put our Watchdog ISR vector into the table.
    goto Empty_ISR               // ADC Monitor
    goto Empty_ISR               // Audio Sink 0
    goto Empty_ISR               // Audio Sink 1
    goto Empty_ISR               // Timer 0
    goto Empty_ISR               // Timer 1
    goto Empty_ISR               // Timer 2
    goto Empty_ISR               // Timer 3
    goto Empty_ISR               // DMA 0
    goto Empty_ISR               // DMA 1
    goto Empty_ISR               // DMA 2
    goto Empty_ISR               // DMA 3
    goto Empty_ISR               // DMA 4
    goto Empty_ISR               // DMA 5
    goto Empty_ISR               // DMA 6
    goto Empty_ISR               // DMA 7
    goto Empty_ISR               // DIO 0
    goto Empty_ISR               // DIO 1
    goto Empty_ISR               // DIO 2
    goto Empty_ISR               // DIO 3
    goto Empty_ISR               // DIO 4
    goto Empty_ISR               // DIO 5
    goto Empty_ISR               // DIO 6
    goto Empty_ISR               // DIO 7
    goto Empty_ISR               // I3C 0
    goto Empty_ISR               // I3C 1
    goto Empty_ISR               // I2C 0
    goto Empty_ISR               // I2C 1
    goto Empty_ISR               // CFX I2C
    goto Empty_ISR               // SPI 0 COM
    goto Empty_ISR               // SPI 0 RX
    goto Empty_ISR               // SPI 0 TX
    goto Empty_ISR               // SPI 1 COM
    goto Empty_ISR               // SPI 1 RX
    goto Empty_ISR               // SPI 1 TX
    goto Empty_ISR               // EMMC
    goto Empty_ISR               // UART ERROR
    goto Empty_ISR               // UART RX
    goto Empty_ISR               // UART TX
    goto Empty_ISR               // FENG
    goto Empty_ISR               // PCM 0 ERROR
    goto Empty_ISR               // PCM 0 RX TX
    goto Empty_ISR               // PCM 1 ERROR
    goto Empty_ISR               // PCM 1 RX TX
    goto Empty_ISR               // ASRC ERROR
    goto Empty_ISR               // ASRC IN
    goto Empty_ISR               // ASRC OUT
    goto Empty_ISR               // ASRC IDLE
    goto Empty_ISR               // G722 ENC IN RDY
    goto Empty_ISR               // G722 ENC OUT RDY
    goto Empty_ISR               // G722 DEC IN RDY
    goto Empty_ISR               // G722 DEC OUT RDY
    goto Empty_ISR               // RS ENC IN RDY
    goto Empty_ISR               // RS ENC OUT RDY
    goto Empty_ISR               // RS DEC IN RDY
    goto Empty_ISR               // RS DEC OUT RDY
    goto FIFO_0_ISR              // FIFO 0 (디지털 마이크1)
    goto FIFO_1_ISR              // FIFO 1 (디지털 마이크2)
    goto FIFO_2_ISR              // FIFO 2 (OD0)
    goto FIFO_3_ISR              // FIFO 3 (OD1)
    goto FIFO_4_ISR              // FIFO 4 (PCM)
    goto FIFO_5_ISR              // FIFO 5 (I2S in)
    goto FIFO_6_ISR              // FIFO 6 (I2S out)
    goto Empty_ISR               // FIFO 7
    goto HEAR_0_ISR              // HEAR 0
    goto HEAR_1_ISR              // HEAR 1
    goto Empty_ISR               // HEAR 2
    goto Empty_ISR               // HEAR 3
    goto Empty_ISR               // HEAR 4
    goto Empty_ISR               // HEAR 5
    goto Empty_ISR               // HEAR 6
    goto Empty_ISR               // HEAR 7
    goto Empty_ISR               // DSP 0
    goto Empty_ISR               // DSP 1
    goto Empty_ISR               // DSP 2
    goto Empty_ISR               // DSP 3
    goto Empty_ISR               // DSP 4
    goto Empty_ISR               // DSP 5
    goto Empty_ISR               // DSP 6
    goto Empty_ISR               // DSP 7
    goto CM3_0_ISR               // CM3 0
    goto CM3_1_ISR               // CM3 1
    goto Empty_ISR               // CM3 2
    goto Empty_ISR               // CM3 3
    goto Empty_ISR               // CM3 4
    goto Empty_ISR               // CM3 5
    goto Empty_ISR               // CM3 6
    goto Empty_ISR               // CM3 7
    goto Empty_ISR               // NNA
    goto Empty_ISR               // CLK DET
    goto Empty_ISR               // LSAD
    goto Empty_ISR               // MEM ERROR
    goto Empty_ISR               // G722 COM
    goto Empty_ISR               // RS COM


/**
 * @fn Empty_ISR
 * @brief Does nothing but return.
 * @registermods   None
 * @assumptions    None
 * @clockcycles    3
 */
.text weak 0 Empty_ISR

    returni

/**
 * @fn Watchdog_ISR
 * @brief Stop the CFX (if debugging) if the watchdog interrupt occurs.
 * @registermods   None
 * @assumptions    None
 * @clockcycles    4
 */
.text weak 0 Watchdog_ISR

    breakpoint

    returni
