#ifndef __tdc_isd_map_data_h__
#define __tdc_isd_map_data_h__

#include <stdbool.h>

#include <tdc_printf.h>

#define numPacket_readMapData_Original_ISDnSetting  2
#define numPacket_writeMapData_Original_ISDnSetting 2
#define numPacket_readMapData_ISDnSetting           3
#define numPacket_writeMapData_ISDnSetting          3
#define numPacket_readMapData_stimulPara            15
#define numPacket_writeMapData_stimulPara           15

void tdc_isd_map_read_original_info_setting(bool startFlag, int command);
void tdc_isd_map_write_original_info_setting(bool startFlag, int command);
void tdc_isd_map_read_info_setting(bool startFlag, int commnad, int slot_index);
void tdc_isd_map_write_info_setting(bool startFlag, int commnad, int slot_index);
void tdc_isd_map_read_stim_para(bool startFlag, int commnad, int slot_index, int map_index);
void tdc_isd_map_write_stim_para(bool startFlag, int commnad, int slot_index, int map_index);
void tdc_isd_map_reset_nvm_selected(bool startFlag, int commnad, int slot_index, EN__mapping_ReadWriteMap_command RecoverOrErase);
void tdc_isd_map_reset_nvm_2to4(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase);
bool tdc_isd_map_reset_nvm_all(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase);
void tdc_isd_map_reset_nvm_map_data(bool startFlag, int commnad, int slot_index, int map_index, EN__mapping_ReadWriteMap_command RecoverOrErase);

#endif
