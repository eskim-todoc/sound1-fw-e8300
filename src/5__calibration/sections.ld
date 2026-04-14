/**
 * @file sections.ld
 * @brief Application Linker Configuration File
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

/* ----------------------------------------------------------------------------
 * Memory linker description
 * ------------------------------------------------------------------------- */
MEMORY
{
    /* ARM Cortex-M3 PRAM0 to PRAM6
     * PRAM0 - 64K (when assigned)
     * PRAM1 - 64K (when assigned)
     * PRAM2 - 64K
     * PRAM3 - 64K
     * PRAM4 - 64K
     * PRAM5 - 32K
     * PRAM6 - 32K
     */
    PRAM  (xrw) : ORIGIN = 0x00010000, LENGTH = 384K

    /* LPDSP32 PRAM5 to PRAM0
     * PRAM0 - 8K
     * PRAM1 - 8K
     * PRAM2 - 16K
     * PRAM3 - 16K
     * PRAM4 - 16K
     * PRAM5 - 16K
     */
    LPDSP32_PRAM  (xrw) : ORIGIN = 0x00070000, LENGTH = 80K

    /* LPDSP32 BRAM0 to BRAM2
     * BRAM0 - 8K
     * BRAM1 - 8K
     * BRAM2 - 16K
     */
    LPDSP32_BRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 32K

    /* ARM Cortex-M3 DRAM0 to DRAM3  */
    DRAM  (xrw) : ORIGIN = 0x20008000, LENGTH = 96K

    /* LPDSP32 ARAM5 to PRAM0
     * ARAM0 - 32K
     * ARAM1 - 32K
     * ARAM2 - 64K
     * ARAM3 - 64K
     * ARAM4 - 8K
     * ARAM5 - 8K
     */
    LPDSP32_ARAM  (xrw) : ORIGIN = 0x20020000, LENGTH = 208K

    /* HEAR FIFO LSB aligned, sign extended  */
    HEAR_FIFO_LSB_SIGN_EXT  (xrw) : ORIGIN = 0x20100000, LENGTH = 256K

    /* HEAR FIFO MSB aligned, zero padded  */
    HEAR_FIFO_MSB_ZERO_PAD  (xrw) : ORIGIN = 0x20140000, LENGTH = 256K

    /* HEAR FIFO LSB aligned, zero extended  */
    HEAR_FIFO_MSB_ZERO_EXT  (xrw) : ORIGIN = 0x20180000, LENGTH = 256K

    /* HEAR FIFO 16-bit mode  */
    HEAR_FIFO_16  (xrw) : ORIGIN = 0x201C0000, LENGTH = 256K

    /* CFX PROM (via bridge)  */
    CFX_PROM  (xrw) : ORIGIN = 0x20200000, LENGTH = 8K

    /* Filter Engine PRAM LSB  */
    FENG_PRAM_LSB  (xrw) : ORIGIN = 0x20207000, LENGTH = 1K

    /* Filter Engine PRAM MSB  */
    FENG_PRAM_MSB  (xrw) : ORIGIN = 0x20207400, LENGTH = 1K

    /* HEAR Microcode PRAM0 to PRAM1
     * PRAM0 - 16k
     * PRAM1 - 16k
     */
    HEAR_PRAM  (xrw) : ORIGIN = 0x20208000, LENGTH = 32K

    /* CFX PRAM0 to PRAM1
     * PRAM0 - 64k
     * PRAM1 - 64k
     */
    CFX_PRAM  (xrw) : ORIGIN = 0x20220000, LENGTH = 128K

    /* CFX XMEM LSB aligned, sign extended  */
    CFX_XMEM_LSB_SIGN_EXT  (xrw) : ORIGIN = 0x20400000, LENGTH = 2048K

    /* CFX XMEM MSB aligned, zero padded  */
    CFX_XMEM_MSB_ZERO_PAD  (xrw) : ORIGIN = 0x20600000, LENGTH = 2048K

    /* CFX XMEM LSB aligned, zero extended  */
    CFX_XMEM_MSB_ZERO_EXT  (xrw) : ORIGIN = 0x20800000, LENGTH = 2048K

    /* CFX XMEM 16-bit mode  */
    CFX_XMEM_16  (xrw) : ORIGIN = 0x20A00000, LENGTH = 2048K

    /* CFX YMEM LSB aligned, sign extended  */
    CFX_YMEM_LSB_SIGN_EXT  (xrw) : ORIGIN = 0x20C00000, LENGTH = 64K

    /* CFX YMEM MSB aligned, zero padded  */
    CFX_YMEM_MSB_ZERO_PAD  (xrw) : ORIGIN = 0x20C20000, LENGTH = 128K

    /* CFX YMEM LSB aligned, zero extended  */
    CFX_YMEM_MSB_ZERO_EXT  (xrw) : ORIGIN = 0x20C40000, LENGTH = 128K

    /* CFX YMEM 16-bit mode  */
    CFX_YMEM_16  (xrw) : ORIGIN = 0x20C60000, LENGTH = 128K
}

/* ----------------------------------------------------------------------------
 * Stack related defines and provided variables
 * ------------------------------------------------------------------------- */
__stack = ORIGIN(DRAM) + LENGTH(DRAM);

PROVIDE ( __stack = __stack ) ;

/*
 * Default stack sizes.
 * These are used by the startup in order to allocate stacks
 * for the different modes.
 */

__Main_Stack_Size = 5120;
PROVIDE ( _Main_Stack_Size = __Main_Stack_Size ) ;

__Main_Stack_Limit = __stack  - __Main_Stack_Size ;
PROVIDE ( _Main_Stack_Limit = __Main_Stack_Limit ) ;

/* ----------------------------------------------------------------------------
 * Heap related defines and provided variables
 * ------------------------------------------------------------------------- */
PROVIDE ( __Heap_Begin__ = __noinit_end__ ) ;
PROVIDE ( __Heap_Limit__ = __stack - __Main_Stack_Size ) ;

/*
 * The entry point is informative, for debuggers and simulators,
 * since the Cortex-M vector points to it anyway.
 */
ENTRY(Reset_Handler)

/* 
 * As is the VTOR register, we refer to it in startup documentation
 */
__VTOR = 0xE000ED08;

/* ----------------------------------------------------------------------------
 * Section definitions
 * ------------------------------------------------------------------------- */
SECTIONS
{
    /*
     * For Cortex-M devices, the beginning of the startup code is stored in
     * the .interrupt_vector section, which goes to FLASH
     */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.interrupt_vector))

        /*
         * This section is here to store the startup code immediately after
         * the interrupt vectors, as required by the program ROM.
         */
        KEEP(*(.reset))

        /* Pre-initialization Code */
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start__ = .);

        /* System initialization and the platform initialization (if present)
         * should be first */
        KEEP(*(.preinit_array_sysinit .preinit_array_sysinit.*))
        KEEP(*(.preinit_array_platform .preinit_array_platform.*))

        /* Pre-initialization functions (to be executed before C++
         * constructors are run) */
        KEEP(*(.preinit_array .preinit_array.*))

        PROVIDE_HIDDEN (__preinit_array_end__ = .);

        /* Initialization Code */
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start__ = .);

        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))

        PROVIDE_HIDDEN (__init_array_end__ = .);

        /*
         * The program code is stored in the .text section,
         * which goes to PRAM.
         */
        . = ALIGN(4);

        *(.text .text.*)            /* all remaining code */
        *(.rodata .rodata.*)        /* read-only data (constants) */

        . = ALIGN(4);
        __dsp_start__ = . ;
        KEEP(*(.dsp .dsp.*))        /* all remaining DSP code */
        __dsp_end__ = . ;

        . = ALIGN(4);
    } >PRAM

    /*
     * This address is used by the startup code to
     * initialize the .data section.
     */
    . = ALIGN(4);
    __data_init__ = .;

    /*
     * The initialized data section. This area should be used sparingly and
     * carefully, as it must be loaded by the bootloader or debug tools.
     */
    .data  : AT ( __data_init__ )
    {
        . = ALIGN(4);

        /* This is used by the startup code to initialize the .data section */
        __data_start__ = . ;
        *(.data_begin .data_begin.*)
        *(.data .data.*)
        *(.data_end .data_end.*)
        . = ALIGN(4);

        /* This is used by the startup code to initialize the .data section */
        __data_end__ = . ;
    } >DRAM

    /*
     * The uninitialized data section. NOLOAD is used to avoid
     * the "section `.bss' type changed to PROGBITS" warning
     */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss_begin .bss_begin.*)

        *(.bss .bss.*)
        *(COMMON)

        *(.bss_end .bss_end.*)
        . = ALIGN(4);
        __bss_end__ = .;
    } >DRAM

    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        __noinit_start__ = .;

        *(.noinit .noinit.*)

         . = ALIGN(4) ;
        __noinit_end__ = .;
    } > DRAM
    
    /* Check if there is enough space to allocate the main stack */
    ._stack (NOLOAD) :
    {
        . = ALIGN(4);
        
        . = . + __Main_Stack_Size ;
        
        . = ALIGN(4);
    } >DRAM
    
    /* SEGGER RTT memory */
    .segger_rtt 0x20000000 : ALIGN(4)
    {
        KEEP(*(.segger_rtt))
    } > LPDSP32_BRAM
}
