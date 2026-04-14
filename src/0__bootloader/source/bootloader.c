/**
 * @file bootloader.c
 * @brief Arm Cortex-M3 core bootloader sample main file
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
#include <aes.h>
#include <bootloader_application_file.h>
#include <bootloader_internal.h>
#include <calibrate_power.h>

#include <ff.h>
#include <nvmctrl.h>

#include <sk5_map.h>
#include <sk5_map_nvm.h>
#include <sk5_sys.h>
#include <sk5_sys_calib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <trims.h>

#include <ci_boot.h>
#include <ci_initialize.h>
#include <rtt_printf.h>
#include <tdc_uart.h>

#define SHOULD_BOOT 0xA5

uint32_t bootloader_error __attribute__((section(".sysvars")));

/* Size in words.
 * This has to be multiple of 3, 4, 8 and 16 (AES block) bytes so the transfers
 * align correctly: 4080/4 = 1020 */
#define _DATA_TEMP_BUFFER_SIZE 1020
static uint32_t _data_temp_buffer[_DATA_TEMP_BUFFER_SIZE];
static FIL      ohdl;
static FATFS    fsmount;
static DIR      dir;

int bootloader_initialize_aes_ctx(struct AES_ctx *aes_context, uint32_t *key)
{
    uint32_t aes_key_scratchpad[4];
    int      i;

    if (aes_context == NULL || key == NULL)
    {
        return -1;
    }

    for (i = 0; i < 4; i++)
    {
        aes_key_scratchpad[i] = htonl(key[i]);
    }

    AES_init_ctx(aes_context, (unsigned char *) aes_key_scratchpad);

    return 0;
}

int bootloader_decrypt_data(struct AES_ctx *aes_context, uint32_t *data, size_t len)
{
    uint32_t  i;
    uint32_t *p;

    /* len is not an integer number of AES blocks. */
    if ((len & 0x03) != 0)
    {
        return -1;
    }

    for (i = 0; i < len; i++)
    {
        data[i] = htonl(data[i]);
    }

    p = data;
    for (i = 0; i < (len >> 2); i++)
    {
        AES_ECB_decrypt(aes_context, (unsigned char *) p);
        p += 4;
    }

    for (i = 0; i < len; i++)
    {
        data[i] = htonl(data[i]);
    }
    return 0;
}

void bootloader_lock_pram(struct data_section_header *section_header, uint32_t *dest_addr)
{
    uint32_t lock_value;

    lock_value = 0;

    if (section_header->type == SEC_CFX_P)
    {
        /* Check starting bank */
        if (dest_addr < (uint32_t *) CFX_PRAM1_BASE)
        {
            lock_value = CFX_PRAM0_LOCK;
        }
        else
        {
            lock_value = CFX_PRAM1_LOCK;
        }

        /* Check if it'll go to PRAM1 */
        if ((dest_addr + section_header->size) > (uint32_t *) CFX_PRAM1_BASE)
        {
            lock_value |= CFX_PRAM1_LOCK;
        }
    }
    else if (section_header->type == SEC_CM3_P)
    {
        /* Check starting bank */
        if (dest_addr < (uint32_t *) PRAM1_BASE)
        {
            lock_value = CM3_PRAM0_LOCK;
        }
        else
        {
            lock_value = CM3_PRAM1_LOCK;
        }

        /* Check if it'll go to PRAM1 */
        if ((dest_addr + section_header->size) > (uint32_t *) PRAM1_BASE)
        {
            lock_value |= CM3_PRAM1_LOCK;
        }
    }
    Sys_PRAMSec_SetLock(lock_value);
}

typedef struct
{
    uint32_t start_addr;
    uint32_t end_addr;
    uint32_t cfg_val;
    bool     is_cfg_0;
} IOMEM_map_t;

#define NUM_IOMEM_MAPPINGS 20

static const IOMEM_map_t iomem_map[NUM_IOMEM_MAPPINGS] = {
    {PRAM_BASE, PRAM_TOP, CM3_PRAM0_POWER_ENABLE | CM3_PRAM1_POWER_ENABLE | CM3_PRAM2_POWER_ENABLE | CM3_PRAM3_POWER_ENABLE | CM3_PRAM4_POWER_ENABLE | CM3_PRAM5_POWER_ENABLE | CM3_PRAM6_POWER_ENABLE, true},

    {DSP_PRAM5_REMAP_BASE, DSP_PRAM_TOP, DSP_PRAM5_POWER_ENABLE | DSP_PRAM4_POWER_ENABLE | DSP_PRAM3_POWER_ENABLE | DSP_PRAM2_POWER_ENABLE | DSP_PRAM1_POWER_ENABLE | DSP_PRAM0_POWER_ENABLE, true},

    {DSP_BRAM0_REMAP_BASE, DSP_BRAM2_REMAP_TOP, DSP_BRAM0_POWER_ENABLE | DSP_BRAM1_POWER_ENABLE | DSP_BRAM2_POWER_ENABLE, true},

    {DRAM_BASE, DRAM_TOP, CM3_DRAM0_POWER_ENABLE | CM3_DRAM1_POWER_ENABLE | CM3_DRAM2_POWER_ENABLE | CM3_DRAM3_POWER_ENABLE, true},

    {DSP_ARAM5_REMAP_BASE, DSP_ARAM0_REMAP_TOP, DSP_ARAM5_POWER_ENABLE | DSP_ARAM4_POWER_ENABLE | DSP_ARAM3_POWER_ENABLE | DSP_ARAM2_POWER_ENABLE | DSP_ARAM1_POWER_ENABLE | DSP_ARAM0_POWER_ENABLE, true},

    {EMMC_BUFFER_BASE, EMMC_BUFFER_TOP, CM3_EMMC_RAM_POWER_ENABLE, true},

    {FENG_STATE_BASE, FENG_STATE_TMP_TOP, FENG_STATE_POWER_ENABLE, false},

    {FENG_COEFFICIENT_BASE, FENG_COEFFICIENT_TOP, FENG_COEF_POWER_ENABLE, false},

    {FENG_PRAM_LSB_BASE, FENG_PRAM_MSB_TOP, FENG_PRAM_POWER_ENABLE, false},

    {HEAR_PRAM_BASE, HEAR_PRAM_TOP, HEAR_PRAM0_POWER_ENABLE | HEAR_PRAM1_POWER_ENABLE, false},

    {CFX_PRAM_BASE, CFX_PRAM_TOP, CFX_PRAM0_POWER_ENABLE | CFX_PRAM1_POWER_ENABLE, false},

    {PRAM0_REMAP_BASE, PRAM1_REMAP_TOP, CM3_PRAM0_POWER_ENABLE | CM3_PRAM1_POWER_ENABLE, true},

    {DSP_ARAM_BASE, DSP_ARAM_TOP, DSP_ARAM0_POWER_ENABLE | DSP_ARAM1_POWER_ENABLE | DSP_ARAM2_POWER_ENABLE | DSP_ARAM3_POWER_ENABLE | DSP_ARAM4_POWER_ENABLE | DSP_ARAM5_POWER_ENABLE, true},

    {DSP_BRAM01_ARAM_REMAP_BASE, DSP_BRAM01_ARAM_REMAP_TOP, DSP_ARAM0_POWER_ENABLE | DSP_ARAM1_POWER_ENABLE | DSP_ARAM2_POWER_ENABLE | DSP_ARAM3_POWER_ENABLE | DSP_ARAM4_POWER_ENABLE | DSP_ARAM5_POWER_ENABLE | DSP_BRAM0_POWER_ENABLE | DSP_BRAM1_POWER_ENABLE, true},

    {DSP_PRAM45_ARAM_REMAP_BASE, DSP_PRAM45_ARAM_REMAP_TOP, DSP_ARAM0_POWER_ENABLE | DSP_ARAM1_POWER_ENABLE | DSP_ARAM2_POWER_ENABLE | DSP_ARAM3_POWER_ENABLE | DSP_ARAM4_POWER_ENABLE | DSP_ARAM5_POWER_ENABLE | DSP_PRAM4_POWER_ENABLE | DSP_PRAM5_POWER_ENABLE, true},

    {PRAM56_ARAM_REMAP_BASE, PRAM56_ARAM_REMAP_TOP, DSP_ARAM0_POWER_ENABLE | DSP_ARAM1_POWER_ENABLE | DSP_ARAM2_POWER_ENABLE | DSP_ARAM3_POWER_ENABLE | DSP_ARAM4_POWER_ENABLE | DSP_ARAM5_POWER_ENABLE | CM3_PRAM5_POWER_ENABLE | CM3_PRAM6_POWER_ENABLE, true},

    {DRAM23_ARAM_REMAP_BASE, DRAM23_ARAM_REMAP_TOP, DSP_ARAM0_POWER_ENABLE | DSP_ARAM1_POWER_ENABLE | DSP_ARAM2_POWER_ENABLE | DSP_ARAM3_POWER_ENABLE | DSP_ARAM4_POWER_ENABLE | DSP_ARAM5_POWER_ENABLE | CM3_DRAM2_POWER_ENABLE | CM3_DRAM3_POWER_ENABLE, true},

    {DSP_BRAM0_BASE, DSP_BRAM2_TOP, DSP_BRAM0_POWER_ENABLE | DSP_BRAM1_POWER_ENABLE | DSP_BRAM2_POWER_ENABLE, true},

    {DRAM3_BRAM_REMAP_BASE, DRAM3_BRAM_REMAP_TOP, DSP_BRAM0_POWER_ENABLE | DSP_BRAM1_POWER_ENABLE | DSP_BRAM2_POWER_ENABLE | CM3_DRAM3_POWER_ENABLE, true},

    {DRAM2_BRAM_REMAP_BASE, DRAM2_BRAM_REMAP_TOP, DSP_BRAM0_POWER_ENABLE | DSP_BRAM1_POWER_ENABLE | DSP_BRAM2_POWER_ENABLE | CM3_DRAM2_POWER_ENABLE, true},
};

/**
 * @brief Check if the address being checked is inside the given memory and update cfg if needed.
 * @param[in] mem_map_info Current memory information
 * @param[in, out] check_addr Start or end address of the range being checked
 * @param[in] end_inp_addr End address of the memory range to check
 * @param[out] cfg Enable config value for the SYSCTRL_MEM_POWER_CFG0 or CFG1 register
 * @return exit 1 if we have found the last memory being used and 0 to continue looping
 */
static int step_addr_cfg(const IOMEM_map_t mem_map_info, uint32_t *check_addr, const uint32_t end_inp_addr, uint32_t *cfg)
{
    /* If address is inside this memory */
    if (mem_map_info.start_addr <= *check_addr && *check_addr <= mem_map_info.end_addr)
    {
        *cfg |= mem_map_info.cfg_val;
        if (*check_addr == end_inp_addr)
        {
            /* If we are checking the end address, this memory is the last memory being used.
             * Exit this function and the caller function */
            return 1;
        }
        else
        {
            /* If we are checking the start address, this memory is the first memory being used.
             * Set check_addr to the end address to find the last memory being used */
            *check_addr = end_inp_addr;
        }
    }
    else if (*check_addr == end_inp_addr && *check_addr > mem_map_info.end_addr)
    {
        /* If we are checking the end address and during this step the end address was not inside
         * the memory, we are using the entire memory and a bit or the entirety of the next memory.
         * Update cfg to enable this memory */
        *cfg |= mem_map_info.cfg_val;
    }

    return 0;
}

/**
 * @brief Loops through an array of IOMEM memories and updates the cfg values used to enable
 * memories if it is inside the range of the given addresses.
 * @param[in] start_addr Start address
 * @param[in] start_addr End address
 * @param[out] cfg_0 Enable config value for the SYSCTRL_MEM_POWER_CFG0 register
 * @param[out] cfg_1 Enable config value for the SYSCTRL_MEM_POWER_CFG1 register
 */
static void get_mem_enable_cfg(uint32_t start_addr, uint32_t end_addr, uint32_t *cfg_0, uint32_t *cfg_1)
{
    *cfg_0              = 0;
    *cfg_1              = 0;
    uint32_t check_addr = start_addr;
    for (size_t i = 0; i < NUM_IOMEM_MAPPINGS; i++)
    {
        if (step_addr_cfg(iomem_map[i], &check_addr, end_addr, (iomem_map[i].is_cfg_0) ? cfg_0 : cfg_1) != 0)
        {
            return;
        }
    }
}

int bootloader_transfer_section_data(struct data_section_header *section_header, FIL *file_desc, uint16_t data_crc)
{
    uint32_t *dest_addr;
    uint32_t  offset;
    uint32_t  remainder;
    size_t    read_size;
    size_t    bytes_read;
    /* In dest width words */
    uint32_t       dest_inc;
    int32_t        ret;
    uint32_t       word_size, dma_len_mul;
    uint32_t       region_size;
    uint32_t      *aes_key;
    struct AES_ctx aes_context;
    uint32_t      *lock_addr;
    uint32_t       dma_src_inc;

    word_size   = WORD_SIZE_32BITS_TO_32BITS;
    dest_inc    = _DATA_TEMP_BUFFER_SIZE;
    dma_len_mul = 0;
    dma_src_inc = DMA_SRC_ADDR_INCR_1;

    if ((section_header->flags & APPDATA_FLAGS_ENCRYPTED) != 0)
    {
        /* Check if the data size is a multiple of an AES block (4 words) */
        if ((section_header->size & 0x03) != 0)
        {
            return -1;
        }

        aes_key = bootloader_get_application_key();

        ret = bootloader_initialize_aes_ctx(&aes_context, aes_key);
        if (ret != 0)
        {
            return ret;
        }
    }

    /* We're comparing addresses here, so this offset is in bytes */
    offset = section_header->offset << 2;
    switch (section_header->type)
    {
        case SEC_CFX_X:
            region_size = HEAR_BD1MEM_LSB_UNSIGNED_TOP - CFX_XMEM_LSB_UNSIGNED_BASE + 1;
            if (offset >= region_size || (offset + ((section_header->size << 2) - section_header->aes_pad_count)) > region_size)
            {
                return -1;
            }
            Sys_Memory_Enable(0, CFX_XRAM0_POWER_ENABLE | CFX_XRAM1_POWER_ENABLE | CFX_XRAM2_POWER_ENABLE | CFX_XRAM3_POWER_ENABLE);
            dest_addr   = (uint32_t *) CFX_XMEM_LSB_UNSIGNED_BASE;
            word_size   = WORD_SIZE_8BITS_TO_24BITS;
            dest_inc    = (_DATA_TEMP_BUFFER_SIZE * 4) / 3;
            dma_len_mul = 2;
            break;
        case SEC_CFX_Y:
            region_size = CFX_YRAM2_LSB_UNSIGNED_TOP - CFX_YRAM0_LSB_UNSIGNED_BASE + 1;
            if (offset >= region_size || (offset + ((section_header->size << 2) - section_header->aes_pad_count)) > region_size)
            {
                return -1;
            }
            Sys_Memory_Enable(0, CFX_YRAM0_POWER_ENABLE | CFX_YRAM1_POWER_ENABLE | CFX_YRAM2_POWER_ENABLE);
            dest_addr   = (uint32_t *) CFX_YRAM0_LSB_UNSIGNED_BASE;
            word_size   = WORD_SIZE_8BITS_TO_24BITS;
            dest_inc    = (_DATA_TEMP_BUFFER_SIZE * 4) / 3;
            dma_len_mul = 2;
            break;
        case SEC_CFX_P:
            region_size = CFX_PRAM_TOP - CFX_PRAM_BASE + 1;
            if (offset >= region_size || (offset + ((section_header->size << 2) - section_header->aes_pad_count)) > region_size)
            {
                return -1;
            }
            dest_addr = (uint32_t *) CFX_PRAM_BASE;
            Sys_Memory_Enable(0, CFX_PRAM0_POWER_ENABLE | CFX_PRAM1_POWER_ENABLE);
            Sys_PRAMSec_SetLock(CFX_PRAM0_NOT_LOCKED | CFX_PRAM1_NOT_LOCKED);
            break;
        case SEC_CM3_P:
            region_size = PRAM_TOP - PRAM_BASE + 1;
            if (offset >= region_size || (offset + ((section_header->size << 2) - section_header->aes_pad_count)) > region_size)
            {
                return -1;
            }
            Sys_Memory_Enable(CM3_PRAM0_POWER_ENABLE | CM3_PRAM1_POWER_ENABLE | CM3_PRAM2_POWER_ENABLE | CM3_PRAM3_POWER_ENABLE | CM3_PRAM4_POWER_ENABLE | CM3_PRAM5_POWER_ENABLE | CM3_PRAM6_POWER_ENABLE, 0);
            dest_addr = (uint32_t *) PRAM_BASE;
            Sys_PRAMSec_SetLock(CM3_PRAM0_NOT_LOCKED | CM3_PRAM1_NOT_LOCKED);
            break;
        case SEC_IOMEM:
            dest_addr = 0;
            uint32_t mem_cfg_0, mem_cfg_1;
            get_mem_enable_cfg(offset, offset + (section_header->size << 2) - 1, &mem_cfg_0, &mem_cfg_1);
            Sys_Memory_Enable(mem_cfg_0, mem_cfg_1);
            break;
        default:
            return -1;
    }

    /* dest_addr is a uint32_t pointer, so this is already in words */
    dest_addr += section_header->offset;
    lock_addr = dest_addr;

    /* In words */
    remainder = section_header->size;

    /* Removes warning below. */
    read_size = remainder;

    /* APPDATA_FLAGS_EMPTY not set */
    if ((section_header->flags & APPDATA_FLAGS_EMPTY) == 0)
    {
        Sys_Set_CRC_Config(CRC, CRC_LITTLE_ENDIAN | 0 | CRC_BIT_ORDER_STANDARD | CRC_FINAL_XOR_STANDARD);
        Sys_CRC_CCITTInitValue(CRC);
    }
    else /* Empty sections don't carry data, they're used to support
     .bss sections */
    {
        dma_src_inc          = DMA_SRC_ADDR_STATIC;
        read_size            = remainder;
        _data_temp_buffer[0] = 0x00;
    }

    while (remainder > 0)
    {
        if ((section_header->flags & APPDATA_FLAGS_EMPTY) == 0)
        {
            if (remainder < _DATA_TEMP_BUFFER_SIZE)
            {
                read_size = remainder;
            }
            else
            {
                read_size = _DATA_TEMP_BUFFER_SIZE;
            }

            /* Read is in bytes */
            ret = f_read(file_desc, (uint8_t *) _data_temp_buffer, read_size << 2, &bytes_read);
            if (bytes_read != (read_size << 2) || ret != FR_OK)
            {
                return -1;
            }

            if ((section_header->flags & APPDATA_FLAGS_ENCRYPTED) != 0)
            {
                if (bootloader_decrypt_data(&aes_context, _data_temp_buffer, read_size) < 0)
                {
                    return -1;
                }
            }

            /* Calculate CRC while data is copied to memory. */
            Sys_DMA_Mode_Enable(CRC_CALCULATION_DMA, DMA_DISABLE);
            if (BIT_TEST(Sys_DMA_Get_Status(CRC_CALCULATION_DMA), DMA_STATUS_ACTIVE_Pos))
            {
                return -1;
            }
            Sys_DMA_ChannelConfig(CRC_CALCULATION_DMA,
                                  DMA_COMPLETE_INT_DISABLE | DMA_CNT_INT_DISABLE | DMA_DEST_ADDR_LSB_TOGGLE_DISABLE | DMA_SRC_ADDR_LSB_TOGGLE_DISABLE | DMA_DEST_ADDR_STATIC | DMA_SRC_ADDR_INCR_1 | WORD_SIZE_32BITS_TO_32BITS | DMA_DEST_ALWAYS_ON | DMA_SRC_ALWAYS_ON | DMA_PRIORITY_0 | SRC_TRANS_LENGTH_SEL
                                      | DMA_LITTLE_ENDIAN,
                                  read_size,
                                  0,
                                  (uint32_t) _data_temp_buffer,
                                  (uint32_t) & (CRC->ADD_32));
            Sys_DMA_Set_Ctrl(CRC_CALCULATION_DMA, DMA_CLEAR_CNTS | DMA_CLEAR_BUFFER);
            Sys_DMA_Mode_Enable(CRC_CALCULATION_DMA, DMA_ENABLE);

            /* If AES padding is added, for the last transfer, use read_size
             * and remainder as bytes, and copy the last data bytes excluding the
             * padding. */
            if (((remainder << 2) - section_header->aes_pad_count) < (_DATA_TEMP_BUFFER_SIZE << 2) && section_header->aes_pad_count != 0)
            {
                read_size   = (remainder << 2) - section_header->aes_pad_count;
                remainder   = read_size;
                dma_len_mul = 0;
                if (word_size == WORD_SIZE_32BITS_TO_32BITS)
                {
                    word_size = WORD_SIZE_8BITS_TO_32BITS;
                }
            }
        }

        Sys_DMA_Mode_Enable(SECTION_TRANSFER_DMA, DMA_DISABLE);
        if (BIT_TEST(Sys_DMA_Get_Status(SECTION_TRANSFER_DMA), DMA_STATUS_ACTIVE_Pos))
        {
            return -1;
        }
        Sys_DMA_ChannelConfig(SECTION_TRANSFER_DMA,
                              DMA_COMPLETE_INT_DISABLE | DMA_CNT_INT_DISABLE | DMA_DEST_ADDR_LSB_TOGGLE_DISABLE | DMA_SRC_ADDR_LSB_TOGGLE_DISABLE | DMA_DEST_ADDR_INCR_1 | dma_src_inc | word_size | DMA_DEST_ALWAYS_ON | DMA_SRC_ALWAYS_ON | DMA_PRIORITY_0 | SRC_TRANS_LENGTH_SEL | DMA_LITTLE_ENDIAN,
                              read_size << dma_len_mul,
                              0,
                              (uint32_t) _data_temp_buffer,
                              (uint32_t) dest_addr);
        Sys_DMA_Set_Ctrl(SECTION_TRANSFER_DMA, DMA_CLEAR_CNTS | DMA_CLEAR_BUFFER);
        Sys_DMA_Mode_Enable(SECTION_TRANSFER_DMA, DMA_ENABLE);

        Nop();
        /* Wait for data transfer and CRC calculation to complete. */
        while (BIT_TEST(Sys_DMA_Get_Status(SECTION_TRANSFER_DMA), DMA_STATUS_ACTIVE_Pos) || BIT_TEST(Sys_DMA_Get_Status(CRC_CALCULATION_DMA), DMA_STATUS_ACTIVE_Pos))
        {
            ;
        }

        /* Decrease remainder */
        remainder -= read_size;
        /* Increase write pointer */
        dest_addr += dest_inc;
    }

    Sys_DMA_Mode_Enable(SECTION_TRANSFER_DMA, DMA_DISABLE);
    Sys_DMA_Mode_Enable(CRC_CALCULATION_DMA, DMA_DISABLE);

    if ((section_header->flags & APPDATA_FLAGS_EMPTY) == 0)
    {
        Sys_CRC_Add(CRC, data_crc, 32);

        ret = Sys_CRC_GetFinalValue(CRC);

        if ((section_header->flags & APPDATA_FLAGS_VOLATILE) != 0 && ret != 0)
        {
            /* TODO: Fix error reporting */
            ret = 0;
        }

        /* Lock regardless if the transfer succeeded or not, as to not expose data
         * that might have been decrypted. */
        if ((section_header->flags & APPDATA_FLAGS_LOCKED) != 0)
        {
            bootloader_lock_pram(section_header, lock_addr);
        }

        return (ret == 0 ? 0 : -1);
    }
    else
    {
        return 0;
    }
}

int bootloader_read_nvm_boot_info(uint32_t *buf)
{
    int                          ret;
    bootloader_boot_information *boot_info;
    uint16_t                     calc_crc;

    if (buf == NULL)
    {
        return -1;
    }

    ret = NVMReadBytes(512, 512, buf);
    if (ret < 0)
    {
        return ret;
    }

    boot_info = (bootloader_boot_information *) buf;

    calc_crc = bootloader_CRC_calc((uint8_t *) buf, sizeof(bootloader_boot_information) - 2);

    return (calc_crc == boot_info->boot_info_crc ? 0 : -1);
}

int bootloader_load_manu_table(uint16_t boot_manu_off)
{
    int              ret;
    uint16_t         calc_crc;
    MANU_TABLE_Type *manuf_table;
    int              max_target  = 0;
    int              curr_target = 0;
    bool             ones_flag   = true;
    bool             zeroes_flag = true;

    ret = NVMReadBytes(boot_manu_off, 256, _data_temp_buffer);
    if (ret < 0)
    {
        return ret;
    }

    manuf_table = (MANU_TABLE_Type *) (_data_temp_buffer + ((boot_manu_off & 0x1FF) >> 2));

    calc_crc = bootloader_CRC_calc((uint8_t *) manuf_table, BOOTSTRAP_MANU_CRC_OFFSET);

    if (calc_crc != manuf_table->MANU_CRC_CCITT.upper)
    {
        return -1;
    }

    /* Make sure the manufacturing table is not just ones or zeroes. */
    for (int i = 0; (i < MANU_TABLE_SIZE - 1); i++)
    {
        if (_data_temp_buffer[i] != 0xffffffff)
        {
            ones_flag = false;
            break;
        }
    }

    for (int i = 0; (i < MANU_TABLE_SIZE - 1); i++)
    {
        if (_data_temp_buffer[i] != 0x00000000)
        {
            zeroes_flag = false;
            break;
        }
    }

    if (ones_flag || zeroes_flag)
    {
        return -1;
    }

#if 1
    tdc_uart_printf("manuf_table = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) manuf_table)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
#endif

#if 1
    tdc_uart_printf("\r\n\nbefore, LoadManuTable \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif

    /* Copy the manufacturing table to the preset location in CM3
     * PRAM using the function in HAL. */
    Sys_Trims_LoadManuTable((uint32_t *) manuf_table);

#if 1
    tdc_uart_printf("\r\n\nafter, LoadManuTable \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif

    /* Try to set the default values if they are in the manufacturing table. */
    Sys_Trims_SetVREGAndLSAD();
    Sys_Trims_SetVDDIF((VDDIF_ACTIVE_TARGET / 10));
    Sys_Trims_SetVDDA((VDDA_ACTIVE_TARGET / 10));
    Sys_Trims_SetVDDC((VDDC_ACTIVE_TARGET / 10));
    Sys_Trims_SetVDDC_CP(VDDCM_MIN_CP_DELTA_TARGET);
    Sys_Trims_SetVDDM((VDDM_ACTIVE_TARGET / 10));
    Sys_Trims_SetVDDM_CP(VDDCM_MIN_CP_DELTA_TARGET);
    Sys_Trims_SetVDDOD((VDDOD_ACTIVE_TARGET / 10));
    Sys_Trims_SetVMIC((VMIC_ACTIVE_TARGET / 10));
    Sys_Trims_SetADCOffsets();

    for (int i = 0; i < MANU_CLK_SIZE; i++)
    {
        curr_target = (manuf_table->MANU_CLK[i] & 0xFF);

        if (curr_target > max_target)
        {
            max_target = curr_target;
        }
    }

    /* If a calibrated frequency is found, update the system clock. */
    if (max_target > 0)
    {
        Sys_Trims_SetOperatingFrequency(max_target);
    }

    return 0;
}

int get_app_load_ext(bootloader_boot_information *boot_info, char *result)
{
    uint8_t len_app_load;
    uint8_t len_app_ext;
    if (!boot_info || !result)
    {
        return -1;
    }

    len_app_load = strlen((char *) boot_info->boot_app_load);
    len_app_ext  = strlen((char *) boot_info->boot_app_ext);

    if (len_app_load == 0 || len_app_ext == 0)
    {
        return -1;
    }

    strcpy(result, (char *) boot_info->boot_app_load);
    result[len_app_load] = '.';
    strcpy(&result[len_app_load + 1], (char *) boot_info->boot_app_ext);

    return len_app_load + len_app_ext + 1;
}

int bootloader_load_boot_section(struct data_section_header *section_header, FIL *file_desc, struct boot_section_data *boot_data, uint16_t data_crc)
{
    int32_t        ret;
    uint32_t       i;
    size_t         read_size;
    size_t         bytes_read;
    struct AES_ctx aes_context;
    uint32_t      *aes_key;

    if (section_header == NULL || boot_data == NULL)
    {
        return -1;
    }

    read_size = sizeof(struct boot_section_data);

    /* Section is encrypted, read a whole AES block. */
    if ((section_header->flags & APPDATA_FLAGS_ENCRYPTED) != 0)
    {
        read_size += 2 * sizeof(uint32_t);
    }

    /* Read structure and the CRC */
    ret = f_read(file_desc, (uint8_t *) _data_temp_buffer, read_size, &bytes_read);
    if (bytes_read != read_size || ret != FR_OK)
    {
        return -1;
    }

    /* Section is encrypted, read a whole AES block. */
    if ((section_header->flags & APPDATA_FLAGS_ENCRYPTED) != 0)
    {
        aes_key = bootloader_get_application_key();
        ret     = bootloader_initialize_aes_ctx(&aes_context, aes_key);
        if (ret != 0)
        {
            return ret;
        }
        ret = bootloader_decrypt_data(&aes_context, _data_temp_buffer, read_size >> 2);
        if (ret != 0)
        {
            return ret;
        }
    }

    Sys_Set_CRC_Config(CRC, CRC_LITTLE_ENDIAN | 0 | CRC_BIT_ORDER_STANDARD | CRC_FINAL_XOR_STANDARD);
    Sys_CRC_CCITTInitValue(CRC);

    for (i = 0; i < (read_size >> 2); i++)
    {
        Sys_CRC_Add(CRC, _data_temp_buffer[i], 32);
    }

    Sys_CRC_Add(CRC, data_crc, 32);

    ret = Sys_CRC_GetFinalValue(CRC);
    if (ret != 0)
    {
        return -1;
    }

    memcpy(boot_data, _data_temp_buffer, sizeof(struct boot_section_data));

    return 0;
}

uint32_t *bootloader_get_application_key(void)
{
    return (uint32_t *) SYSVAR_KEY_START; /* Cast to the correct type */
}

int bootloader_check_header_crc(struct data_section_header *section_header)
{
    uint16_t       calc_crc;
    uint32_t      *aes_key;
    struct AES_ctx aes_context;
    int            ret;

    if (section_header == NULL)
    {
        return -1;
    }
    /* Verify header CRC */
    calc_crc = bootloader_CRC_calc((uint8_t *) section_header, sizeof(struct data_section_header));
    if (calc_crc == 0)
    {
        /* If the CRC matches, there's nothing else to do, just return. */
        return 0;
    }

    /* CRC doesn't match, the header might be encrypted */
    aes_key = bootloader_get_application_key();
    ret     = bootloader_initialize_aes_ctx(&aes_context, aes_key);
    if (ret != 0)
    {
        return ret;
    }

    ret = bootloader_decrypt_data(&aes_context, (uint32_t *) section_header, sizeof(struct data_section_header) >> 2);
    if (ret != 0)
    {
        return ret;
    }

    calc_crc = bootloader_CRC_calc((uint8_t *) section_header, sizeof(struct data_section_header));
    if (calc_crc != 0)
    {
        return -1;
    }

    /* CRC matches, check for flag, otherwise we'll fail when decrypting the data. */
    if ((section_header->flags & APPDATA_FLAGS_ENCRYPTED) == 0)
    {
        return -1;
    }

    return 0;
}

BOOTLOADER_LOAD_SECTIONS_RETURN bootloader_load_sections(FIL *file_desc, struct application_file_header *app_header, struct boot_section_data *cfx_boot_data, struct boot_section_data *cm3_boot_data)
{
    int32_t                    ret;
    struct data_section_header section_header;
    uint8_t                    boot_flags;
    uint32_t                   section_number;
    size_t                     bytes_read;

    boot_flags     = 0;
    section_number = 0;

    if (app_header == NULL)
    {
        return BOOTLOADER_LOAD_SECTIONS_RETURN_error;
    }

    /* Read first header */
    ret = f_read(file_desc, (uint8_t *) &section_header, sizeof(struct data_section_header), &bytes_read);
    while (bytes_read > 0 && section_number < app_header->crc_table_size)
    {
        /* Verify header CRC */
        if (bootloader_check_header_crc(&section_header) != 0)
        {
            return BOOTLOADER_LOAD_SECTIONS_RETURN_error;
        }

        /* Process sections */
        if (section_header.type == SEC_BOOT_CFX)
        {
            ret = bootloader_load_boot_section(&section_header, file_desc, cfx_boot_data, app_header->crc_table[section_number]);
            boot_flags |= BOOTLOADER_LOAD_SECTIONS_RETURN_cfx_load;
        }
        else if (section_header.type == SEC_BOOT_CM3)
        {
            ret = bootloader_load_boot_section(&section_header, file_desc, cm3_boot_data, app_header->crc_table[section_number]);
            boot_flags |= BOOTLOADER_LOAD_SECTIONS_RETURN_cm3_load;
        }
        else
        {
            ret = bootloader_transfer_section_data(&section_header, file_desc, app_header->crc_table[section_number]);
        }
        if (ret < 0)
        {
            return BOOTLOADER_LOAD_SECTIONS_RETURN_error;
        }
        /* Read next header */
        ret = f_read(file_desc, (uint8_t *) &section_header, sizeof(struct data_section_header), &bytes_read);
        section_number++;
    }

    if (bytes_read < 0)
    {
        return BOOTLOADER_LOAD_SECTIONS_RETURN_error;
    }

    return (BOOTLOADER_LOAD_SECTIONS_RETURN_ok | boot_flags);
}

int bootloader_read_and_check_file_header(FIL *file_desc, struct application_file_header *app_header, size_t buf_size)
{
    int32_t        ret;
    uint16_t       calc_crc;
    uint32_t      *aes_key;
    struct AES_ctx aes_context;
    size_t         data_size, read_size;
    uint32_t      *p;
    size_t         bytes_read;

    memset(app_header, '\0', buf_size);

    data_size = offsetof(struct application_file_header, crc_table) + sizeof(uint16_t);

    /* Read the first 8 bytes of the file header. If the file header happens to
     * be encrypted, this will read the ident member and the first 4 bytes of
     * the first AES block.*/
    ret = f_read(file_desc, (uint8_t *) app_header, data_size, &read_size);
    if (read_size != data_size || ret != FR_OK)
    {
        return -1;
    }

    buf_size -= read_size;

    /* Check if the file is encrypted or not */
    if (app_header->ident == APPFILE_IDENT_APPENCRYPTED || app_header->ident == APPFILE_IDENT_MANIFESTENCRYPTED)
    {
        p = (uint32_t *) (((uint8_t *) app_header) + offsetof(struct application_file_header, crc_table_size));

        /* Encrypted file. Read the last 12 bytes of the first AES block. The
         * first 4 bytes were read in the previous read. */
        ret = f_read(file_desc, ((uint8_t *) app_header) + read_size, 12, &bytes_read);
        if (bytes_read != 12 || ret != FR_OK)
        {
            return -1;
        }
        read_size += bytes_read;

        aes_key = bootloader_get_application_key();
        ret     = bootloader_initialize_aes_ctx(&aes_context, aes_key);
        if (ret != 0)
        {
            return ret;
        }

        /* Decrypt the first AES block. It contains crc_table_size */
        if (bootloader_decrypt_data(&aes_context, p, 4) < 0)
        {
            return -1;
        }
    }

    /* Read the rest of the CRC table */
    data_size = sizeof(app_header->crc_table[0]) * app_header->crc_table_size;
    /* Check for padding */
    if ((app_header->crc_table_size & 0x01) != 0)
    {
        data_size += sizeof(uint16_t);
    }

    /* If the header is encrypted, we already read at least 5 crc_table entries */
    if (app_header->ident == APPFILE_IDENT_APPENCRYPTED || app_header->ident == APPFILE_IDENT_MANIFESTENCRYPTED)
    {
        /* Subtract how many bytes we already read. */
        if (data_size <= 12)
        {
            data_size = 0;
        }
        else
        {
            data_size -= 12;
            /* Make the encrypted part a multiple of an AES block */
            if ((data_size & 0x0F) != 0)
            {
                data_size += 16 - (data_size & 0x0F);
            }
        }
    }

    if (data_size > 0)
    {
        if (buf_size < data_size)
        {
            return -1;
        }

        ret = f_read(file_desc, ((uint8_t *) app_header) + read_size, data_size, &bytes_read);
        if (bytes_read != data_size || ret != FR_OK)
        {
            return -1;
        }

        if (app_header->ident == APPFILE_IDENT_APPENCRYPTED || app_header->ident == APPFILE_IDENT_MANIFESTENCRYPTED)
        {
            if (bootloader_decrypt_data(&aes_context, (uint32_t *) (((uint8_t *) app_header) + read_size), data_size >> 2) < 0)
            {
                return -1;
            }
        }
    }
    data_size = sizeof(uint16_t) * app_header->crc_table_size;
    if ((app_header->crc_table_size & 0x01) != 0)
    {
        data_size += sizeof(uint16_t);
    }
    data_size += offsetof(struct application_file_header, crc_table) + sizeof(uint16_t);

    calc_crc = bootloader_CRC_calc((uint8_t *) app_header, data_size);

    if (calc_crc != 0)
    {
        return -1;
    }

    return 0;
}

struct boot_section_data cfx_boot_data;
uint8_t                  cfx_boot_flag;
struct boot_section_data cm3_boot_data;
uint8_t                  cm3_boot_flag;

int bootloader_process_sections(FIL *file_desc, struct application_file_header *app_header)
{
    int ret;

    ret = bootloader_load_sections(file_desc, app_header, &cfx_boot_data, &cm3_boot_data);
    if (ret < BOOTLOADER_LOAD_SECTIONS_RETURN_ok)
    {
        return ret;
    }

    /* If we loaded a CFX boot section, boot the other core */
    if ((ret & BOOTLOADER_LOAD_SECTIONS_RETURN_cfx_load) != 0)
    {
        cfx_boot_flag = SHOULD_BOOT;
    }

    /* If we loaded a Cortex-M3 boot section, boot to it */
    if ((ret & BOOTLOADER_LOAD_SECTIONS_RETURN_cm3_load) != 0)
    {
        cm3_boot_flag = SHOULD_BOOT;
    }
    return 0;
}

int bootloader_manifest_open_files(char *file_name)
{
    int                             ret;
    uint8_t                         tmp[MANIFEST_MAXIMUM_SIZE];
    struct application_file_header *app_header;
    char                            file_path[20] = "/";

#if 1  // kes0481@to-doc.com
    uint8_t slot_num = tdc_boot_get_slot_num();

    if (0 < slot_num && slot_num < 5)
    {
        file_path[0] = '/';
        file_path[1] = '0' + slot_num;
        file_path[2] = '/';
        file_path[3] = 0;
    }
    else  // when, 255 (factory reset slot)
    {
        file_path[0] = '/';
        file_path[1] = 0;
    }

    tdc_uart_printf("File path : '%s' \r\n", file_path);

    ret = f_chdir(file_path);

    if (ret != FR_OK)
    {
        tdc_uart_printf("Fail : f_chdir() \r\n");
        return -1;
    }
#endif

    // strcat(file_path, file_name);
    strcpy(file_path, file_name);
    tdc_uart_printf("f_open : '%s' \r\n", file_path);

    app_header = (struct application_file_header *) tmp;

    /* Instead of simply calling bootloader_load_app_file, we open and read
     * the file header here so we don't have recursive calls. If a manifest
     * files listed itself, the system would crash. */
    ret = f_open(&ohdl, file_name, FA_OPEN_EXISTING | FA_READ);
    if (ret != FR_OK)
    {
        return -1;
    }

    ret = bootloader_read_and_check_file_header(&ohdl, app_header, sizeof(tmp));
    if (ret < 0)
    {
        f_close(&ohdl);
        return ret;
    }

    ret = -1;
    switch (app_header->ident)
    {
        case APPFILE_IDENT_APP:
        case APPFILE_IDENT_APPENCRYPTED:
            tdc_uart_printf("app_header->ident : app... \r\n");
            ret = bootloader_process_sections(&ohdl, app_header);
            f_close(&ohdl);
            break;
        case APPFILE_IDENT_MANIFEST:
            f_close(&ohdl);
            return -1;
        case APPFILE_IDENT_MANIFESTENCRYPTED:
            f_close(&ohdl);
            return -1;
        default:
            f_close(&ohdl);
            return -1;
    }

    return ret;
}

int get_manifest_files(char *src, char *dst, int len)
{
    int i;
    i = 0;
    if (src == NULL || dst == NULL)
    {
        return -1;
    }
    if (len <= 0)
    {
        return 0;
    }

    memset(dst, '\0', len);
    while (i < len)
    {
        if (src[i] == '\0' || src[i] == '\n')
        {
            dst[i] = '\0';
            break;
        }
        else
        {
            dst[i] = src[i];
        }
        i++;
    }
    return i;
}

int bootloader_process_manifest(FIL *file_desc, struct application_file_header *app_header)
{
    int            ret, pos;
    size_t         read_len;
    char           manifest_contents[513];
    char           file_name[16];
    uint16_t       calc_crc;
    uint32_t       i;
    struct AES_ctx aes_context;
    uint32_t      *aes_key;

    if (app_header == NULL || app_header->crc_table_size != 1)
    {
        return -1;
    }

    memset(manifest_contents, '\0', sizeof(manifest_contents));
    ret = f_read(file_desc, (uint8_t *) manifest_contents, sizeof(manifest_contents) - 1, &read_len);
    if (ret != FR_OK)
    {
        return -1;
    }

    f_close(file_desc);  // 김은수 추가

#if 1
    tdc_uart_printf("\r\n\nafter, read manifest_contents \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif

    while (read_len > 0)
    {
        tdc_uart_printf("manifest_contents read len : %d \r\n", read_len);

        if (app_header->ident == APPFILE_IDENT_MANIFESTENCRYPTED)
        {
            aes_key = bootloader_get_application_key();
            ret     = bootloader_initialize_aes_ctx(&aes_context, aes_key);
            if (ret != 0)
            {
                return ret;
            }

            /* Decrypt the first AES block. It contains crc_table_size */
            if (bootloader_decrypt_data(&aes_context, (uint32_t *) manifest_contents, read_len) < 0)
            {
                return -1;
            }
            read_len = strlen(manifest_contents) + 1;
        }

        Sys_Set_CRC_Config(CRC, CRC_LITTLE_ENDIAN | 0 | CRC_BIT_ORDER_STANDARD | CRC_FINAL_XOR_STANDARD);

        Sys_CRC_CCITTInitValue(CRC);

        for (i = 0; i < read_len; i++)
        {
            Sys_CRC_Add(CRC, manifest_contents[i], 8);
        }
        Sys_CRC_Add(CRC, app_header->crc_table[0], 16);
        calc_crc = Sys_CRC_GetFinalValue(CRC);
        if (calc_crc != 0)
        {
            return -1;
        }

        pos = 0;
        i   = get_manifest_files(manifest_contents, file_name, sizeof(file_name));

        tdc_uart_printf("i = %u \r\n", i);

        while (i != 0)
        {
            tdc_uart_printf("file_name : '%s' \r\n", file_name);

            ret = bootloader_manifest_open_files(file_name);
            if (ret < 0)
            {
                return ret;
            }
#if 1
            tdc_uart_printf("\r\n\nafter, load file: '%s' \r\n", file_name);
            tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
            for (int i = 0; i < MANU_TABLE_SIZE; i++)
            {
                tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
                if (((i + 1) % 4) == 0)
                {
                    tdc_uart_printf("\r\n");
                }
            }
            tdc_uart_printf("\r\n");
            tdc_uart_printf("==================================================\r\n");
#endif

            pos += i + 1;
            i = get_manifest_files(manifest_contents + pos, file_name, sizeof(file_name));

            tdc_uart_printf("i = %u \r\n", i);
        }
        ret = f_read(file_desc, (uint8_t *) manifest_contents, sizeof(manifest_contents), &read_len);
    }
    return 0;
}

int bootloader_load_app_file(const char *file_name)
{
    int32_t                         ret;
    char                            file_path[20] = "/";
    struct application_file_header *app_header;
    uint32_t                        tmp[128];

    if (file_name == NULL)
    {
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(-1);
        return -1;
    }

    cfx_boot_flag = 0;
    cm3_boot_flag = 0;

#if 1  // kes0481@to-doc.com
    uint8_t slot_num = tdc_boot_get_slot_num();

    tdc_uart_printf("\r\nslot_num = %u \r\n", slot_num);

    if (0 < slot_num && slot_num < 5)
    {
        file_path[0] = '/';
        file_path[1] = '0' + slot_num;
        file_path[2] = '/';
        file_path[3] = 0;
    }
    else  // when, 255 (factory reset slot)
    {
        file_path[0] = '/';
        file_path[1] = 0;
    }
    tdc_uart_printf("file path = '%s' \r\n", file_path);

    ret = f_chdir(file_path);

    if (ret != FR_OK)
    {
        tdc_uart_printf("Fail : f_chdir() \r\n");
        return -1;
    }
#endif

    // strcat(file_path, file_name);
    strcpy(file_path, file_name);

    tdc_uart_printf("f_open : '%s' \r\n", file_path);

    ret = f_open(&ohdl, file_path, FA_OPEN_EXISTING | FA_READ);
    if (ret != FR_OK)
    {
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(-1);
        return -1;
    }

    app_header = (struct application_file_header *) tmp;

    ret = bootloader_read_and_check_file_header(&ohdl, app_header, sizeof(tmp));
    if (ret < 0)
    {
        f_close(&ohdl);
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
        return ret;
    }

    switch (app_header->ident)
    {
        case APPFILE_IDENT_APP:
        case APPFILE_IDENT_APPENCRYPTED:
            tdc_uart_printf("app_header->ident : APP... \r\n");
            /* File is closed in bootloader_process_sections in case we
             * encounter a SEC_BOOT_CM3 section, otherwise close it here. */
            ret = bootloader_process_sections(&ohdl, app_header);
            break;
        case APPFILE_IDENT_MANIFEST:
        case APPFILE_IDENT_MANIFESTENCRYPTED:
            tdc_uart_printf("app_header->ident : MANIFEST... \r\n");
            ret = bootloader_process_manifest(&ohdl, app_header);
            break;
        default:
            ret = -1;
            break;
    }
    f_close(&ohdl);
    if (ret < 0)
    {
        tdc_uart_printf("Fail : f_close() \r\n");
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
        return ret;
    }

    if (!NVMSync())
    {
        tdc_uart_printf("Fail : NVMSync(), before boot \r\n");
        return -1;
    }

#if 0
    tdc_uart_printf("\r\n\nafter fw image copy \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif

    ret = f_unmount("0:");
    if (ret == FR_OK)
    {
        tdc_uart_printf("Success : f_unmount(\"0:\"); \r\n");
    }
    else
    {
        tdc_uart_printf("Failed : f_unmount(\"0:\"); \r\n");
    }

    SYS_WATCHDOG_REFRESH();

#if 0
    tdc_uart_printf("before, boot each cores \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");

    tdc_uart_printf("==================================================\r\n");
#endif

    if (cfx_boot_flag == SHOULD_BOOT)
    {
        ret = bootloader_boot_cfx(cfx_boot_data.init_pointer);
        if (ret < 0)
        {
            bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
            return ret;
        }
    }
    if (cm3_boot_flag == SHOULD_BOOT)
    {
        /* Doesn't return */
        bootloader_boot_cm3(cm3_boot_data.stack_pointer, cm3_boot_data.init_pointer);
    }

    return ret;
}

int bootloader_boot(void)
{
    int                          ret;
    bootloader_boot_information *boot_info;
    NVMCTRL_Options_t            options;
    uint32_t                     buf[128];
    char                         app_name[20];

    /* If the bootloader succeeded, this should be the only value in bootloader_error */
    bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(0);

    NVMGetDefaultOptions(&options);
    ret = NVMInit(&options);
    if (ret != 0)
    {
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
        return ret;
    }

    ret = bootloader_read_nvm_boot_info(buf);
    if (ret != 0)
    {
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
        return ret;
    }

    boot_info = (bootloader_boot_information *) buf;

    if (boot_info->boot_speed < BOOT_SPEED_AUTO || boot_info->boot_speed > BOOT_SPEED_QSPI)
    {
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(boot_info->boot_speed);
        return -1;
    }

    switch (boot_info->boot_speed)
    {
        case BOOT_SPEED_AUTO:
            /* Try using QSPI and read boot_info again. */
            options.config = (options.config & ~SPI_CFG_MODE_Mask) | SPI_MODE_QSPI;
            NVMReInitOptions(&options);
            ret = bootloader_read_nvm_boot_info(buf);
            if (ret == 0)
            {
                break;
            }
            /* QSPI failed, try using DSPI and read boot_info again. */
            options.config = (options.config & ~SPI_CFG_MODE_Mask) | SPI_MODE_DSPI;
            NVMReInitOptions(&options);
            ret = bootloader_read_nvm_boot_info(buf);
            if (ret == 0)
            {
                break;
            }
            /* DSPI failed, go back to the default config. */
            NVMReInitOptions(NULL);
            break;
        case BOOT_SPEED_SSPI:
            break;
        case BOOT_SPEED_DSPI:
            options.config = (options.config & ~SPI_CFG_MODE_Mask) | SPI_MODE_DSPI;
            NVMReInitOptions(&options);
            ret = bootloader_read_nvm_boot_info(buf);
            if (ret != 0)
            {
                /* DSPI failed, fail boot. */
                bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
                return ret;
            }
            break;
        case BOOT_SPEED_QSPI:
            options.config = (options.config & ~SPI_CFG_MODE_Mask) | SPI_MODE_QSPI;
            NVMReInitOptions(&options);
            ret = bootloader_read_nvm_boot_info(buf);
            if (ret != 0)
            {
                /* QSPI failed, fail boot. */
                bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
                return ret;
            }
            break;
        default:
            break;
    }

    uint32_t app_number = *((uint32_t *) SYSVAR_BOOT_APPN);

    tdc_uart_printf("app_number = %u \r\n", app_number);

    if (app_number == 0)
    {
        ret = get_app_load_ext(boot_info, app_name);

        tdc_uart_printf("app_name = '%s' \r\n", app_name);

        if (ret <= 0)
        {
            bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(-1);
            return -1;
        }
    }
    else
    {
        int      count           = 0;
        uint32_t app_number_temp = app_number;
        do
        {
            app_number_temp /= 10;
            ++count;
        } while (app_number_temp != 0);
        strcpy(app_name, "MANIFEST_");
        itoa(app_number, app_name + 9, 10);
        strcpy(app_name + 9 + count, ".TXT");
    }

    tdc_uart_printf("boot_manu_off = %u \r\n", boot_info->boot_manu_off);

    bootloader_load_manu_table(boot_info->boot_manu_off);

    tdc_uart_init();  // 재 초기화

#if 0
    tdc_uart_printf("\r\n\nafter all of power, clock setting \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif

    ret = f_mount(&fsmount, "0:", 1);  // default: "0"
    if (ret != FR_OK)
    {
        bootloader_error = BOOTLOADER_EXIT_STATUS_CODE(ret);
        return ret;
    }

    ret = f_chdrive("0:");
    if (ret != FR_OK)
    {
        tdc_uart_printf("Fail : f_chdrive() \r\n");
        return ret;
    }

    ret = f_opendir(&dir, "0:");
    if (ret != FR_OK)
    {
        tdc_uart_printf("Fail : f_opendir() \r\n");
        return ret;
    }

    ret = f_closedir(&dir);
    if (ret != FR_OK)
    {
        tdc_uart_printf("Fail : f_closedir() \r\n");
        return ret;
    }

    if (!NVMSync())
    {
        tdc_uart_printf("Fail : NVMSync() \r\n");
        return -1;
    }

#if 0
    tdc_uart_printf("\r\n\nafter, moount \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: OTA : kes0481@to-doc.com
////////////////////////////////////////////////////////////////////////////////////////////////////
#if 1
    tdc_uart_printf("\r\n");
    tdc_uart_printf("system core clock : %u (hz) \r\n", SystemCoreClock);

    snd_boot_set_fp(&ohdl);
    snd_boot_handle_file();
    tdc_boot_print_boot_file();  // for debugging
    tdc_boot_debug_mode();
#endif

#if 0
    tdc_uart_printf("\r\n\nafter, handle boot file \r\n");
    tdc_uart_printf("SYSVAR_MANU_TABLE = \r\n");
    for (int i = 0; i < MANU_TABLE_SIZE; i++)
    {
        tdc_uart_printf("0x%08X ", ((uint32_t *) SYSVAR_MANU_TABLE)[i]);
        if (((i + 1) % 4) == 0)
        {
            tdc_uart_printf("\r\n");
        }
    }
    tdc_uart_printf("\r\n");
    tdc_uart_printf("==================================================\r\n");
#endif
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    return bootloader_load_app_file(app_name);
}

void bootloader_main(void)
{
    bootloader_boot();
    while (1)
    {
    }
}
