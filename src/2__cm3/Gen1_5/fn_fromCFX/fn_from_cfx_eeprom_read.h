/**
 * @file EEPROM_read.h
 */

#ifndef __EEPROM_read_h__
#define __EEPROM_read_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <99_eeprom_address.h>

#include <cfx_cm3_sharedMemory.h>

#include <ci_map.h>

void fn_read_All_isd_info(void);
void fn_copy_MapInfo_toCM3(int isd_num);
void fn_copy_userSettingParameters_toCM3(int isd_num);
void fn_copy_MappingData_toCM3(int map_num, int isd_num);
void fn_copy_isd_info_to_Repository(void);
void fn_copy_MappingData_to_Repository(void);
void fn_copy_userSettingParameters_to_Repository(void);
void fn_copy_mapStamp_to_Repository(void);
void fn_Read_Mapdata_mappingApp(void);

#endif // __EEPROM_read_h__
