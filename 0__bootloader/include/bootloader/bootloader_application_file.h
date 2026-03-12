/**
 * @file bootloader_application_file.h
 * @brief Header file with the structure definition for the application header
 *        file format.
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

#ifndef BOOTLOADER_APPLICATION_FILE_H
#define BOOTLOADER_APPLICATION_FILE_H

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

/** @addtogroup CM3BOOTLOADERg
 *  @{
 */

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/

/* Application file type magic numbers */
#define APPFILE_IDENT_APP               0x657A6169   /**< Application  file (not encrypted),         */
#define APPFILE_IDENT_APPENCRYPTED      0x455A4149   /**< Application  file (encrypted),             */
#define APPFILE_IDENT_MANIFEST          0x657A616D   /**< Manifest file (not encrypted),             */
#define APPFILE_IDENT_MANIFESTENCRYPTED 0x455A414D   /**< Manifest file (encrypted)                  */

/* Data section memory region instances */
#define SEC_CFX_X                       0x01        /**< Data destination is CFX X memory           */
#define SEC_CFX_Y                       0x02        /**< Data destination is CFX Y memory           */
#define SEC_CFX_P                       0x03        /**< Data destination is CFX P memory           */
#define SEC_CM3_P                       0x04        /**< Data destination is CM3 P memory           */
#define SEC_IOMEM                       0x05        /**< Data destination is IO memory. No boundary
                                                        checks are performed here, use it at your
                                                        own risk.                                   */
#define SEC_BOOT_CFX                    0x08        /**< CFX core boot section                      */
#define SEC_BOOT_CM3                    0x09        /**< CM3 core boot section                      */

/**
 The following table shows the permissible range for the section types defined
 above. The sum of the data offset and the data size must fit within the range.

 | Section          | Start Address                 |  End Address                  |
 |:-----------------|:-----------------------------:|:-----------------------------:|
 | SEC_CFX_X        | CFX_XRAM0_LSB_UNSIGNED_BASE   | CFX_XRAM3_LSB_UNSIGNED_TOP    |
 | SEC_CFX_Y        | CFX_YRAM0_LSB_UNSIGNED_BASE   | CFX_YRAM2_LSB_UNSIGNED_TOP    |
 | SEC_CFX_XY       |       -                       |       -                       |
 | SEC_CFX_P        | CFX_PRAM_BASE                 | CFX_PRAM_TOP                  |
 | SEC_CM3_P        | PRAM_BASE                     | PRAM_TOP                      |
 | SEC_IOMEM        | 0                             | 0xFFFFFFFF                    |
 */

/* Data section flags */
#define APPDATA_FLAGS_CLEARTEXT         0x00        /**< Section data is not encrypted              */
#define APPDATA_FLAGS_ENCRYPTED         0x80        /**< Section data is encrypted with the
                                                        Application Key                             */
#define APPDATA_FLAGS_CRC               0x00        /**< CRC will be verified for the section       */
#define APPDATA_FLAGS_VOLATILE          0x40        /**< CRC will not be verified (volatile section)*/

#define APPDATA_FLAGS_UNLOCKED          0x00        /**< Do not lock the PRAM segment.              */
#define APPDATA_FLAGS_LOCKED            0x20        /**< PRAM segment will be locked if it is PRAM
                                                         or PRAM1.*/

#define APPDATA_FLAGS_EMPTY             0x10        /**< This data section carries no data, it will
                                                         write `size` to `offset`. */

#define MANIFEST_MAXIMUM_SIZE           512         /**< Maximum size of a manifest file. In bytes. */
#define APP_HEADER_MAXIMUM_SIZE         512         /**< Maximum size of the
                                                         application_file_header. In bytes.         */

/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/

/**
 * Header for an Ezairo application file.
 *
 * For encrypted files, the encryption applies to the whole file with the
 * exception of the ident field in the file header. This provides some level of
 * authentication as the CRC fields will also be encrypted. The crc_table
 * member is followed by a CCITT CRC field stored in big-endian order. The CRC
 * field is calculated over the whole structure. If the number of entries in
 * the CRC tables is odd, there is a uint16_t word with zero as a padding
 * between the last entry of crc_table and the CRC field.
 */
struct application_file_header
{
    uint32_t    ident;              /**< Magic number indicating an Ezairo aplication file:
                                         'ezai': Application  file (not encrypted),
                                         'EZAI': Application  file (encrypted),
                                         'ezam': Manifest file (not encrypted),
                                         'EZAM': Manifest file (encrypted)                          */
    uint16_t    crc_table_size;     /**< The number of entries in the crc_table.                    */
    uint16_t    crc_table[];        /**< The CCITT CRC for the data sections that are part of this
                                         file. If the number of sections is odd, the last entry is
                                         a padding value.                                           */
};

/**
 * Defines the header for a block of data in the file.
 * @note The whole structure is 16 bytes long (an AES block).
 */
struct data_section_header
{
    uint32_t    offset;             /**< Offset (in words) from the beginning of the memory instance
                                         specified by the type field.                               */
    uint32_t    size;               /**< Section data size.
                                         The size only counts the number of words in the data, it
                                         does not count the header size.                            */
    uint16_t    type;               /**< Section type. One of the following:
                                         - #SEC_CFX_X
                                         - #SEC_CFX_Y
                                         - #SEC_CFX_P
                                         - #SEC_CM3_P
                                         - #SEC_IOMEM
                                         - #SEC_BOOT_CFX
                                         - #SEC_BOOT_CM3                                            */
    uint16_t    flags;              /**< A bitfield for flags:
                                         - Bit 7: encrypted section
                                             - 0 : Section data is not encrypted
                                             - 1 : Section data and CRC are encrypted with the
                                                 Application Key.
                                         - Bit 6: CRC check
                                             - 0: CRC will be verified for the section
                                             - 1: CRC will not be verified (volatile section)
                                         - Bit 5: Lock PRAM. Valid only if #type is #SEC_CFX_P or #SEC_CM3_P.
                                             - 0: Do not lock the PRAM segment.
                                             - 1: PRAM segment will be locked if it is PRAM or PRAM1.*/
    uint8_t     aes_pad_count;      /**< Number of AES padding bytes if the section is encrypted.   */
    uint8_t     _reserved;          /**< Zero-filled area */
    uint16_t    header_crc;         /**< The CCITT CRC for the header section. If the encryption
                                         flag is set, it should be encrypted with the same key as the
                                         data section. This is stored in big-endian order.          */
    uint32_t    data[];             /**< The section data                                           */
};

/**
 * Data layout for boot sections (SEC_BOOT_CFX or SEC_BOOT_CM3).
 *
 * @note If this data is packet into an encrypted section, the data must
 * be padded so it's a multiple of an AES block.
 */
struct boot_section_data
{
    uint32_t    stack_pointer;          /**< Stack pointer absolute address in the core's address space */
    uint32_t    init_pointer;           /**< Address the control will jump to                           */
};

/* ----------------------------------------------------------------------------
 * Function prototype definitions
 * --------------------------------------------------------------------------*/

/** @} *//* End of the CM3BOOTLOADERg group */

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif /* BOOTLOADER_APPLICATION_FILE_H */
