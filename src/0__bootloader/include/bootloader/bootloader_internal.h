/**
 * @file bootloader_internal.h
 * @brief Header file with private definitions
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

#ifndef BOOTLOADER_INTERNAL_H
#define BOOTLOADER_INTERNAL_H

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
#include <aes.h>
#include <bootloader_application_file.h>
#include <ff.h>
#include <stddef.h>

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/

/* Converts endianess in short words */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define htonl(a) (((a<<24)&(0xFFU<<24)) | ((a<<8)&0xFF0000U) | ((a>>8)&0xFF00U) | ((a>>24)&0xFFU))
#define ntohl(a) (((a<<24)&(0xFFU<<24)) | ((a<<8)&0xFF0000U) | ((a>>8)&0xFF00U) | ((a>>24)&0xFFU))
#define htons(a) ( ((a<<8)&0xFF00U) | ((a>>8)&0x00FF) )
#define ntohs(a) ( ((a<<8)&0xFF00U) | ((a>>8)&0x00FF) )
#else
#define htonl(a) ( a )
#define ntohl(a) ( a )
#define htons(a) ( a )
#define ntohs(a) ( a )
#endif /* __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ */

#if defined(__chess__)
#define BIT_TEST(reg, bit_num) (bittst(to_acc32(reg), bit_num))
#else
#define BIT_TEST(reg, bit_num) ((reg&(0x01U<<bit_num)) != 0)
#endif

/** Sets the Bootstrap_Exit_Status */
#define BOOTLOADER_EXIT_STATUS_x(code,line) (((((uint16_t)line)&0xFFFF) << 8)|(((int8_t)code)&0xFF))
#define BOOTLOADER_EXIT_STATUS_CODE(code) BOOTLOADER_EXIT_STATUS_x(code, __LINE__)

/** Boot speed values */
#define BOOT_SPEED_AUTO 0
#define BOOT_SPEED_SSPI 1
#define BOOT_SPEED_DSPI 2
#define BOOT_SPEED_QSPI 3

/* DMA Channels */
#define SECTION_TRANSFER_DMA DMA0
#define CRC_CALCULATION_DMA DMA1

/* ----------------------------------------------------------------------------
 * Global variables and types
 * --------------------------------------------------------------------------*/

/**
 * The boot-information block provides additional information about the
 * underlying memory, file system and communication interface. This information
 * starts immediately following the BPB at byte 512 in memory.
 */
typedef struct _bootloader_boot_information {
    uint16_t    boot_fs_ver;       /**< NVM File System version */
    uint8_t     boot_app_load[10]; /**< Name of application to load from root
                                         directory. Set to "APP000" by default.
                                         Application is loaded by the bootloader.
                                         Maximum length of string 8 - characters.
                                         Last two bytes padded with zero */
    uint8_t     boot_app_ext[4];   /**< Application extension name. Set to "fez"
                                         by default. Example: APP000.fez
                                         Maximum length of string 3 - characters.
                                         Last byte padded with zero */
    uint8_t     boot_media_id;     /**< ID of the boot media
                                        Bits [3:0] Reserved
                                        Bits [7:4] Data bus width
                                          0:   Automatic width selection
                                          1-8: Width of the data bus */
    uint8_t     boot_speed;        /**< Boot speed for reading of boot media
                                        0:     Automatic
                                        1:     Single SPI
                                        2:     Dual SPI
                                        3:     Quad SPI
                                        4-255: Reserved */
    uint16_t    boot_sec;          /**< Bits [6:0] Encrypted Boot loader key Index
                                        Bit [7] Prefered key. Used when bit 15
                                        is set, OTP:BOOT_KEY_REQUIRED is not set,
                                        OTP:BOOT_KEY_WRITTEN is set,
                                        and OTP:BOOT_KEY_DESTROYED is not set.
                                            0 : OTP key
                                            1 : Key ROM
                                        Bit [15]
                                            1 : Boot loader encrypted
                                            0 : Boot- loader not encrypted */
    uint32_t    boot_addr;         /**< Address in PRAM (as addressed on the CFX core)
                                        where the bootloader image should be loaded to.
                                        Default 0 */
    uint16_t    boot_manu_off;     /**< Byte offset from the beginning of
                                        reserved area for Manufacturing
                                        Information. */
    uint16_t    boot_manu_len;     /**< Length of the manufacturing area
                                        information in words. */
    uint8_t     _reserved0[2];     /**<  */
    uint16_t    boot_info_crc;     /**< CRC-CCITT of boot information area. */
} __attribute__((packed)) bootloader_boot_information;

typedef enum
{
    BOOTLOADER_LOAD_SECTIONS_RETURN_error       =-1,    /**< Error in function                      */
    BOOTLOADER_LOAD_SECTIONS_RETURN_ok          = 0,    /**< Data sections successfully loaded      */
    BOOTLOADER_LOAD_SECTIONS_RETURN_cfx_load    = 1,    /**< CFX boot section loaded                */
    BOOTLOADER_LOAD_SECTIONS_RETURN_cm3_load    = 2,    /**< CM3 boot section loaded                */
} BOOTLOADER_LOAD_SECTIONS_RETURN;

/* ----------------------------------------------------------------------------
 * Function prototype definitions
 * --------------------------------------------------------------------------*/

/**
 * Converts a regular char string to uint16_t
 * @param[in]   src Source buffer
 * @param[out]  dst Destination buffer
 * @return The number of characters copied
 */
int _strcpyW(const char *src, uint16_t *dst);

/**
 * Reads the Boot Information sector from the NVM device
 * @param buf   Buffer to store the data
 * @return 0 if successful, not zero otherwise
 */
int bootloader_read_nvm_boot_info(uint32_t *buf);

/**
 * Loads a SEC_BOOT_CFX or a SEC_BOOT_CM3 section
 *
 * @param[in]   section_header  Pointer to the section header data
 * @param[in]   file_desc       File descriptor of an open application file
 * @param[out]  boot_data       Boot data structure pointer to be filled if a
 *                              SEC_BOOT_* section is found in the file.
 * @param[in]   data_crc        CRC of the boot section data.
 * @return 0 if successful, less than 0 otherwise
 */
int bootloader_load_boot_section(struct data_section_header *section_header,
                                 FIL *file_desc,
                                 struct boot_section_data *boot_data,
                                 uint16_t data_crc);

/**
 * Loads all the data sections in an application file
 * @param[in]   file_desc       File descriptor of an open application file
 * @param[in]   app_header      Pointer to the file header structure (CRC data).
 * @param[out]  cfx_boot_data   Boot data structure pointer to be filled if a
 *                              SEC_BOOT_CFX section is found in the file.
 * @param[out]  cm3_boot_data   Boot data structure pointer to be filled if a
 *                              SEC_BOOT_CM3 section is found in the file.
 * @return BOOTLOADER_LOAD_SECTIONS_RETURN
 */
BOOTLOADER_LOAD_SECTIONS_RETURN bootloader_load_sections(
        FIL *file_desc,
        struct application_file_header *app_header,
        struct boot_section_data *cfx_boot_data,
        struct boot_section_data *cm3_boot_data);

/**
 * Returns a pointer to the Application Key Area that was loaded by the ROM.
 * @return Pointer to the Application Key Area
 */
uint32_t *bootloader_get_application_key(void);

/**
 * Verifies the CRC of a section header. If the section is encrypted, it will
 * use the Application Key to decrypt the section header.
 * @param[in]   section_header  Pointer to the section header data
 * @return 0 if CRC is correct, -1 if it is incorrect.
 */
int bootloader_check_header_crc(struct data_section_header *section_header);

/**
 * Calculates the CRC from a block of data in memory.
 * @param[in]   data    Pointer to data buffer
 * @param[in]   size    Size in octets of the data
 * @return The calculated CRC
 */
uint32_t bootloader_CRC_calc(uint8_t *data_in, uint32_t len);

/**
 * Transfers the data of a section to the correct memory location using DMA.
 *
 * @param[in]   section_header  The section header structure
 * @param[in]   file_desc       File descriptor of an open application file
 * @param[in]   data_crc        CRC of the data section
 * @return 0 if successful, less than 0 otherwise
 * @note This function assumes the header is correct and does not verifies the
 *       header CRC.
 */
int bootloader_transfer_section_data(struct data_section_header *section_header,
                                     FIL *file_desc,
                                     uint16_t data_crc);

/**
 * Merges app name and extension. Returns it in result.
 * @param[in]   boot_info  Bootloader boot information
 * @param[out]  result     App name merged with extension
 * @return 0 if successful, less than 0 otherwise
 */
int get_app_load_ext(bootloader_boot_information* boot_info, char *result);

/**
 * Yields execution to the loaded program
 * @param[in]   boot_data   Boot data structure pointer.
 * @return never returns
 * @note We don't pass a pointer to struct boot_section_data so we can test
 *       the argument values when this is called.
 */
int bootloader_boot_cm3 (uint32_t stack_pointer, uint32_t init_pointer);

/**
 * Starts an application on the CFX core
 * @param[in]   boot_data   Boot data structure pointer.
 * @return 0 if successful, less than 0 otherwise
 * @note We don't pass a pointer to struct boot_section_data so we can test
 *       the argument values when this is called.
 */
int bootloader_boot_cfx (uint32_t init_pointer);

/**
 * Decrypts in place a data buffer in ECB mode
 * @param[in]    aes_context Initialized AES context structure.
 * @param[inout] data        Pointer to the data to be decrypted
 * @param[in]    len         Length in words of the data to be decrypted.
 *                           Must be a multiple of 4 (128 bits).
 * @return 0 if successful, -1 otherwise
 */
int bootloader_decrypt_data(struct AES_ctx *aes_context, uint32_t *data, size_t len);

/**
 * Initialize the AES context structure with the given AES key.
 * @param[in] aes_context   AES context structure to be initialized
 * @param[in] key           Key to be used.
 * @return 0 if successful, -1 otherwise.
 */
int bootloader_initialize_aes_ctx(struct AES_ctx *aes_context, uint32_t *key);

/**
 * Read the file header from the disk and check the CRC. Will decrypt it if
 * necessary.
 * @param[in]   file_desc   File descriptor of an open application file
 * @param[out]  app_header  Pointer to the file header structure.
 * @param[in]   buf_size    Size, in bytes, of the app_header buffer.
 * @return Zero if successful, less than zero otherwise.
 */
int bootloader_read_and_check_file_header(FIL *file_desc,
                                          struct application_file_header *app_header,
                                          size_t buf_size);
/**
 * Sets the PMEM locking bits based on the data section destination and size.
 * @param[in]   section_header  Pointer to the section header data
 * @param[in]   dest_addr       Absolute section destination address.
 */
void bootloader_lock_pram(struct data_section_header *section_header,
                              uint32_t *dest_addr);

/**
 * Copies src to dst stopping at '\n' or '\0'
 * @param src Source buffer
 * @param dst Destination buffer
 * @param len Size of the destination buffer
 * @return The number of characters copied or -1 if there was an error.
 */
int get_manifest_files(char *src, char *dst, int len);

/**
 * Reads the manifest file, decrypting it if needed and opens the application files
 * in order to load their sections.
 * @param file_desc File descriptor of an open manifest file.
 * @param app_header Pointer to the file header structure.
 * @return 0 if the applications were opened and their sections were loaded successfully.
 *         -1 if there was an error.
 */
int bootloader_process_manifest(FIL *file_desc,
                                struct application_file_header *app_header);

/**
 * Calls the sections load function and sets flag variables if the CFX and/or
 * the CM3 should be booted.
 * @param file_desc File descriptor of an open application file.
 * @param app_header Pointer to the file header structure.
 * @return 0 if the sections were loaded successfully and the boot flags set.
 *         -1 if there was an error.
 */
int bootloader_process_sections(FIL *file_desc,
                                struct application_file_header *app_header);

/**
 * Opens the given app file which was listed in a manifest file and processes
 * the sections.
 * @param file_name App file name to open and process.
 * @return 0 if the file was opened and the sections were loaded successfully.
 *         -1 if there was an error.
 */
int bootloader_manifest_open_files(char *file_name);

#if defined(__arm__)
static inline void Nop() {
    asm("mov r0,r0");
}
#else
static inline void Nop() {
}
#endif

/* ----------------------------------------------------------------------------
 * Close the 'extern "C"' block
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif    /* ifdef __cplusplus */

#endif /* BOOTLOADER_INTERNAL_H */
