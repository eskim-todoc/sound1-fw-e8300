/**
 * @file ci_from_cfx_eeprom_recover.h
 */

#ifndef __ci_from_cfx_eeprom_recover_h__
#define __ci_from_cfx_eeprom_recover_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>

#include <ci_map.h>
#include <fn_from_cfx_eeprom_erase.h>
#include <fn_from_cfx_eeprom_read.h>

void fn_recover_mapData_byMapping(void);
void fn_recover_Mapdata_mappingApp(void);

#endif // __ci_from_cfx_eeprom_recover_h__
