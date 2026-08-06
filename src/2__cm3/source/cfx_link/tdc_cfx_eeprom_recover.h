/**
 * @file tdc_cfx_eeprom_recover.h
 */

#ifndef __tdc_cfx_eeprom_recover_h__
#define __tdc_cfx_eeprom_recover_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>

#include <tdc_fs_map.h>
#include <tdc_cfx_eeprom_erase.h>
#include <tdc_cfx_eeprom_read.h>

void tdc_cfx_eeprom_recover_map_data_by_mapping(void);
void tdc_cfx_eeprom_recover_mapdata_mapping_app(void);

#endif  // __tdc_cfx_eeprom_recover_h__
