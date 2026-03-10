/**
 * @file OTE_1_5_gen_CFX_EEPROM_erase.h
 */

#ifndef __OTE_1_5_gen_CFX_EEPROM_erase_h__
#define __OTE_1_5_gen_CFX_EEPROM_erase_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>

#include <ci_map.h>
#include <fn_from_cfx_eeprom_read.h>

void fn_erase_mapStamp_byMapping(void);
void fn_erase_Mapdata_mappingApp(void);
void fn_erase_mapData_byMapping(void);
void fn_erase_ISD_info_byMapping(void);
void fn_erase_userSettingParameters_byMapping(void);

void fn_recover_mapData_byMapping(void);
void fn_recover_Mapdata_mappingApp(void);

#endif // __OTE_1_5_gen_CFX_EEPROM_erase_h__
