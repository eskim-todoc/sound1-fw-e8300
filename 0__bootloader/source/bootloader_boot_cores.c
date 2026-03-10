/**
 * @file bootloader_boot_cores.c
 * @brief Support functions for booting the Cortex-M3 and the CFX core
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

#include <sk5_map.h>
#include <hw.h>
#include <coreboot.h>

int bootloader_boot_cm3 (uint32_t stack_pointer, uint32_t init_pointer)
{
    asm("MOV    SP, %[stack_pointer]" :: [stack_pointer] "r" (stack_pointer));
    asm("BX     %[res]" :: [res] "r" (init_pointer));
    return 0;
}

/**
 * Starts an application on the CFX core
 * @param[in]   init_pointer   Application start address pointer (CFX perspective).
 * @return 0 if successful, less than 0 otherwise
 */
int bootloader_boot_cfx (uint32_t init_pointer)
{
    volatile uint32_t *CFX_Start_Addr;
    volatile uint32_t *CFX_Start_Result;
    const unsigned int PRAM_sections[4] = {0xC000, 0x10000, 0x14000, 0x18000};
    unsigned int targeted_section = 0;

    CFX_Start_Addr = (uint32_t*) (APP_START_ADDR);
    CFX_Start_Result = CFX_Start_Addr + 1;

    /* Return -1 if the CFX core is not waiting for the address or if the
     * address is invalid. */
    if((*CFX_Start_Result != PCREQ) || (init_pointer < 0x8000) ||
       (init_pointer > PRAM_sections[3]))
    {
        return -1;
    }

    /* Loop through the sections to find out the targeted memory section */
    while (init_pointer >= PRAM_sections[targeted_section++])
    {
        if (targeted_section == 4)
        {
            break;
        }
    }

    /* Handle memory access and assignment depending on the targeted CFX or CM3
     * PRAM section. Ensure the targeted CM3 memory sections are not in use
     * as this section will re-assign those to the CFX core. */
    switch(targeted_section)
    {
        case 1:
            SYSCTRL->MEM_ACCESS_CFG1 |= CFX_PRAM0_ACCESS_ENABLE;
            break;
        case 2:
            SYSCTRL->MEM_ACCESS_CFG1 |= CFX_PRAM1_ACCESS_ENABLE;
            break;
        case 3:
            SYSCTRL->MEM_ACCESS_CFG0 |= CM3_PRAM0_ACCESS_ENABLE;
            SYSCTRL->PRAM_CFG |= PRAM0_CFX;;
            break;
        case 4:
            SYSCTRL->MEM_ACCESS_CFG0 |= CM3_PRAM1_ACCESS_ENABLE;
            SYSCTRL->PRAM_CFG |= PRAM1_CFX;
            break;
    }

    /* Write start address and XOR value to CFX PRAM */
    *CFX_Start_Addr = init_pointer;
    *CFX_Start_Result = PCKEY;

    /* Send CFX_0 interrupt to CFX */
    SYSCTRL->CFX_CMD = CFX_CMD_0;

    /* Success */
    return 0;
}
