/**
 * @file OTE_1_5_gen_CFX_EEPROM_erase.h
 */

#ifndef __tdc_cfx_eeprom_erase_h__
#define __tdc_cfx_eeprom_erase_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>

#include <tdc_fs_map.h>
#include <tdc_cfx_eeprom_read.h>

void tdc_cfx_eeprom_erase_map_stamp_by_mapping(void);
void tdc_cfx_eeprom_erase_mapdata_mapping_app(void);
void tdc_cfx_eeprom_erase_map_data_by_mapping(void);
void tdc_cfx_eeprom_erase_isd_info_by_mapping(void);
void tdc_cfx_eeprom_erase_user_setting_parameters_by_mapping(void);

void tdc_cfx_eeprom_recover_map_data_by_mapping(void);
void tdc_cfx_eeprom_recover_mapdata_mapping_app(void);

#endif // __tdc_cfx_eeprom_erase_h__
