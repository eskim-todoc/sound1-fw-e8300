/**
 * @file OTE_1_5_gen_CFX_EEPROM_write.h
 */

#ifndef __OTE_1_5_gen_CFX_EEPROM_write_h__
#define __OTE_1_5_gen_CFX_EEPROM_write_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_fs_map.h>
#include <fn_from_cfx_eeprom_erase.h>
#include <fn_from_cfx_eeprom_read.h>

void fn_write_Mapdata_mappingApp(void);
void fn_write_mapData_byMapping(void);
void fn_write_ISD_info_byMapping(void);
void fn_write_userSettingParameters_byMapping(void);
void fn_write_userSettingParameters(int connected_ISD_num);
void fn_write_mapStmpParameters_byMapping(void);

#endif // __OTE_1_5_gen_CFX_EEPROM_write_h__
