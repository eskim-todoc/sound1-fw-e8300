/**
 * @file start.c
 * @brief Cortex-M3 Start Up function
 *
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

#include <hw.h>

/* ----------------------------------------------------------------------------
 * Application entry point
 * ------------------------------------------------------------------------- */
extern long ISR_Vector_Table;
extern int main(void);

/* ----------------------------------------------------------------------------
 * Linker provided defines for memory initialization
 * ------------------------------------------------------------------------- */
extern uint32_t __data_init__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;

/**
 * Initialize the application data and start execution with main. Should be
 * called from the reset vector. This function is different from the regular
 * Ezairo 8300 `_start` function in that `.data` is initialized here.
 * @note The symbols __data_init__, __data_start__, __data_end__, __bss_start__,
 *       and __bss_end__ are defined when the application is linked.
 */
void __attribute__ ((noreturn)) _start (void)
{
    uint32_t *src, *dest, *end;
    uint32_t i;

    /* Copy .data from PRAM to DRAM */
    src = &__data_init__;
    dest = &__data_start__;
    end = &__data_end__;

    while(dest < end) {
      *dest++ = *src++;
    }

    /* bss section already in PRAM */

    /* Initialize the bss sections */
    dest = &__bss_start__;
    while (dest < &__bss_end__)
    {
        *dest++ = 0;
    }
    /* Initialize the heap */
    _sbrk(0);

    /* Execute the pre-initialization code */
    for (i = 0; i < (__preinit_array_end__ - __preinit_array_start__); i++)
    {
        __preinit_array_start__[i] ();
    }

    /* Execute the initialization code */
    for (i = 0; i < (__init_array_end__ - __init_array_start__); i++)
    {
        __init_array_start__[i] ();
    }

    /* Call the main entry point. */
    main();

    /* Wait for the watchdog reset to trigger since main returned
     * and it should not have returned. */
    while (1);
}
