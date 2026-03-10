/**
 * @file bootloader.h
 * @brief Header file for the Arm Cortex-M3 core bootloader sample main file
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

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/* ----------------------------------------------------------------------------
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif    /* ifdef __cplusplus */

/* ----------------------------------------------------------------------------
 * Include files
 * --------------------------------------------------------------------------*/

#include <stdint.h>

/** @defgroup CM3BOOTLOADERg Arm Cortex-M3 Core Bootloader Reference
 *  Arm Cortex-M3 Core Bootloader
 *  @{
 */

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
 * Function prototype definitions

 * --------------------------------------------------------------------------*/

/**
 * Reads the manufacturing table from the NVM into RAM and then loads
 * it to a known location in PRAM. Tries to set the default trim values
 * if found in the table. Finds and sets the fastest system clock trim
 * value that can be used.
 * @param[in] boot_manu_off The manufacturing table offset to use when reading
 *                          the table from the NVM.
 * @return result -1 on error and 0 if the table was loaded successfully.
 */
int bootloader_load_manu_table(uint16_t boot_manu_off);

/**
 * Loads and processes an application file.
 * @param[in] file_name The name of the application file
 * @return 0 if successful, less than 0 otherwise.
 * @examplecode CM3-bootloader_examples.c bootloader_load_app_file_Example
 */
int bootloader_load_app_file(const char *file_name);

/**
 * Loads the application file specified in the NVM image Boot Information
 * sector. It might return if the Arm Cortex-M3 core is not booted.
 * @return 0 if successful, less than 0 otherwise.
 * @examplecode CM3-bootloader_examples.c bootloader_boot_Example
 */
int bootloader_boot(void);

/**
 * Loads the application file specified in the NVM image Boot Information
 * sector. Does not return.
 * @examplecode CM3-bootloader_examples.c bootloader_main_Example
 */
void bootloader_main(void);

/** @} *//* End of the CM3BOOTLOADERg group */

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif /* BOOTLOADER_H */
