#ifndef ISD_INTERFACE_MAPPING_READWRITE_MAPDADAH__
#define ISD_INTERFACE_MAPPING_READWRITE_MAPDADAH__

#include <stdbool.h>

#include "tdc_printf.h"

#define numPacket_readMapData_Original_ISDnSetting  2
#define numPacket_writeMapData_Original_ISDnSetting 2
#define numPacket_readMapData_ISDnSetting           3
#define numPacket_writeMapData_ISDnSetting          3
#define numPacket_readMapData_stimulPara            15
#define numPacket_writeMapData_stimulPara           15

void read_Original_isdInfo_N_userSetting_fromFlash(bool startFlag, int command);
void write_Original_isdInfo_N_userSetting_atFlash(bool startFlag, int command);
void read_isdInfo_N_userSetting_fromFlash(bool startFlag, int commnad, int slot_index);
void write_isdInfo_N_userSetting_atFlash(bool startFlag, int commnad, int slot_index);
void read_stimulPara_fromFlash(bool startFlag, int commnad, int slot_index, int map_index);
void write_stimulPara_atFlash(bool startFlag, int commnad, int slot_index, int map_index);
void reset_NVM_Selected_ISD_allData(bool startFlag, int commnad, int slot_index, EN__mapping_ReadWriteMap_command RecoverOrErase);
void reset_NVM_2to4_ISD_allData(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase);
bool reset_NVM_All_ISD_allData(bool startFlag, int command, EN__mapping_ReadWriteMap_command RecoverOrErase);
void reset_NVM_MapData(bool startFlag, int commnad, int slot_index, int map_index, EN__mapping_ReadWriteMap_command RecoverOrErase);

#endif
