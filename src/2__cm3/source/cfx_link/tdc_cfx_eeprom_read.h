/**
 * @file EEPROM_read.h
 */

#ifndef __tdc_cfx_eeprom_read_h__
#define __tdc_cfx_eeprom_read_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <99_eeprom_address.h>

#include <tdc_shm.h>

#include <tdc_fs_map.h>

void tdc_cfx_eeprom_read_all_isd_info(void);
void tdc_cfx_eeprom_copy_map_info_to_cm3(int isd_num);
void tdc_cfx_eeprom_copy_user_setting_parameters_to_cm3(int isd_num);
void tdc_cfx_eeprom_copy_mapping_data_to_cm3(int map_num, int isd_num);
void tdc_cfx_eeprom_copy_isd_info_to_repository(void);
void tdc_cfx_eeprom_copy_mapping_data_to_repository(void);
void tdc_cfx_eeprom_copy_user_setting_parameters_to_repository(void);
void tdc_cfx_eeprom_copy_map_stamp_to_repository(void);
void tdc_cfx_read_mapdata_mapping_app(void);

#endif  // __tdc_cfx_eeprom_read_h__
