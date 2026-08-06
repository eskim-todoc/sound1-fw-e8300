/**
 * @file OTE_1_5_gen_CFX_EEPROM_write.h
 */

#ifndef __tdc_cfx_eeprom_write_h__
#define __tdc_cfx_eeprom_write_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_fs_map.h>
#include <tdc_cfx_eeprom_erase.h>
#include <tdc_cfx_eeprom_read.h>

void tdc_cfx_eeprom_write_mapdata_mapping_app(void);
void tdc_cfx_eeprom_write_map_data_by_mapping(void);
void tdc_cfx_eeprom_write_isd_info_by_mapping(void);
void tdc_cfx_eeprom_write_user_setting_parameters_by_mapping(void);
void tdc_cfx_eeprom_write_user_setting_parameters(int connected_ISD_num);
void tdc_cfx_eeprom_write_map_stamp_parameters_by_mapping(void);

#endif  // __tdc_cfx_eeprom_write_h__
