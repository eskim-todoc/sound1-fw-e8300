/**
 * @file OTE_1P5_power_manager.h
 */

#ifndef __OTE_1P5_power_manager_h__
#define __OTE_1P5_power_manager_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <sk5_map_nvm.h>
#include <calibrate_power.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <definitionsForAlgorithm.h>

#include <ci_util.h>
#include <ci_filesystem.h>
#include <ci_printf.h>

#include <tdc_trims.h>

#define SM_POWER_NORMAL  1
#define SM_POWER_STANDBY 2
#define SM_POWER_PREHEAT 3

#define CI_MANUF_TABLE_FILE "/MANUF_TABLE"

typedef struct
{
    unsigned int lsad_cfg;
    unsigned int vdda_ctrl;
    unsigned int analog_cp_vddif_ctrl;
    unsigned int vddc_trim;
    unsigned int vddm_trim;
    unsigned int osc_ctrl_1;
    unsigned int clk_cfg_0;
    bool         avs_enabled;
    bool         vddc_cp_enabled;
    bool         vddm_cp_enabled;
} SM_power_backup_t;

/**
 * The boot-information block provides additional information about the
 * underlying memory, file system and communication interface. This information
 * starts immediately following the BPB at byte 512 in memory.
 */
typedef struct _bootloader_boot_information
{
    uint16_t boot_fs_ver;       /**< NVM File System version */
    uint8_t  boot_app_load[10]; /**< Name of application to load from root
                                      directory. Set to "APP000"? by default.
                                      Application is loaded by the bootloader.
                                      Maximum length of string 8 - characters.
                                      Last two bytes padded with zero */
    uint8_t boot_app_ext[4];    /**< Application extension name. Set to "fez"
                                      by default. Example: APP000.fez
                                      Maximum length of string 3 - characters.
                                      Last byte padded with zero */
    uint8_t boot_media_id;      /**< ID of the boot media
                                     Bits [3:0] Reserved
                                     Bits [7:4] Data bus width
                                       0:   Automatic width selection
                                       1-8: Width of the data bus */
    uint8_t boot_speed;         /**< Boot speed for reading of boot media
                                     0:     Automatic
                                     1:     Single SPI
                                     2:     Dual SPI
                                     3:     Quad SPI
                                     4-255: Reserved */
    uint16_t boot_sec;          /**< Bits [6:0] Encrypted Boot loader key Index
                                     Bit [7] Prefered key. Used when bit 15
                                     is set, OTP:BOOT_KEY_REQUIRED is not set,
                                     OTP:BOOT_KEY_WRITTEN is set,
                                     and OTP:BOOT_KEY_DESTROYED is not set.
                                         0 : OTP key
                                         1 : Key ROM
                                     Bit [15]
                                         1 : Boot loader encrypted
                                         0 : Boot- loader not encrypted */
    uint32_t boot_addr;         /**< Address in PRAM (as addressed on the CFX core)
                                     where the bootloader image should be loaded to.
                                     Default 0 */
    uint16_t boot_manu_off;     /**< Byte offset from the beginning of
                                     reserved area for Manufacturing
                                     Information. */
    uint16_t boot_manu_len;     /**< Length of the manufacturing area
                                     information in words. */
    uint8_t  _reserved0[2];     /**<  */
    uint16_t boot_info_crc;     /**< CRC-CCITT of boot information area. */
} __attribute__((packed)) bootloader_boot_information;

int ci_power_normal(void);
int ci_power_sleep(void);

#endif  // __OTE_1P5_power_manager_h__
