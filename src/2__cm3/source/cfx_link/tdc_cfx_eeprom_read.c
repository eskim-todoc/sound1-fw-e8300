/**
 * @file EEPROM_read.c
 */

#include <tdc_fs.h>
#include <tdc_cfx_eeprom_read.h>

void tdc_cfx_eeprom_read_all_isd_info(void)
{
    for (int i = 0; i < MaxNumUser; i++)
    {
        // #1. File-system에서 ISD 전체 정보를 해당 ISD 공유 메모리에 로드한다.
        tdc_fs_map_read_isd_info(i + 1);

        // #2. 공유 메모리로 로드한 전체 ISD 정보 중에 info 항목만 CFX와의 공유 메모리로 복사한다.
        cfx_cm3_sharedMemoryAll.cfx_ISD_info[i] = g_tdc_fs_ptr_entire_map->map[i].isd_info;
    }
}

void tdc_cfx_eeprom_copy_map_info_to_cm3(int isd_num)
{
    TDC_FS_MAP_T *p_isd;
    int                 *p_map_date;
    int                  map_cnt;
    int                  sum;

    p_isd = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1]);

    // 맵 스탬프 정보(6-워드)를 공유 메모리로 복사
    cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.mapStamp = p_isd->map_stamp;

    // 최대 4개의 각 프로그램 매핑 일자 정보(6-워드)를 공유 메모리로 복사
    for (int i = 0; i < MaxNumMap; i++)
    {
        cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[i] = p_isd->map_data[i].mappingDate;
    }

    // 매핑 일자의 합이 0이면 사용 불가능한 맵으로 간주하며, 사용자가 사용 가능한 총 맵 (프로그램) 수를 감소시킴
    map_cnt = MaxNumMap;

    // 참고: 매핑 일자의 경우 fn_fromCFX 관련 코드에서
    //       Erase 진행 시 0으로 모두 초기화되며, Recover 진행 시 99로 모두 초기화 됨

    for (int i = 0; i < MaxNumMap; i++)
    {
        sum        = 0;
        p_map_date = (int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[i]);

        for (int k = 0; k < df_24bitWordLength_mappingDate; k++)  // 매핑 일자의 합 구하기
        {
            sum += p_map_date[k];
        }

        if (sum == 0)  // 합이 0이면 사용자가 사용 가능한 총 맵 (프로그램) 수를 1 감소
        {
            map_cnt--;
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex[i] = 0;  // 사용불가한 맵 (프로그램)
        }
        else
        {
            cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex[i] = 1;  // 사용가능한 맵 (프로그램)
        }
    }

    cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.user_usableMapNum = map_cnt;  // 사용자가 사용 가능한 총 맵 (프로그램) 수 업데이트
}

void tdc_cfx_eeprom_copy_user_setting_parameters_to_cm3(int isd_num)
{
    cfx_cm3_sharedMemoryAll.userSettingValue = g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value;
}

void tdc_cfx_eeprom_copy_mapping_data_to_cm3(int map_num, int isd_num)
{
    cfx_cm3_sharedMemoryAll.currentMapData = g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1];
}

void tdc_cfx_eeprom_copy_isd_info_to_repository(void)
{
    TDC_FS_MAP_T               *p_isd;
    ST__CFX_CM3_SharedMemory_ISD_info *p_src;
    ST__CFX_CM3_SharedMemory_ISD_info *p_dst;

    p_isd = &(g_tdc_fs_ptr_entire_map->map[cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index - 1]);
    p_src = &p_isd->isd_info;
    p_dst = &cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.ISD_info_mapData;

    *p_dst = *p_src;
}

void tdc_cfx_eeprom_copy_mapping_data_to_repository(void)
{
    int                               isd_num, map_num;
    ST__CFX_CM3_SharedMemory_mapData *p_src, *p_dst;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    map_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;

    p_src = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1]);
    p_dst = &(cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.readWritemapData);

    *p_dst = *p_src;
}

void tdc_cfx_eeprom_copy_user_setting_parameters_to_repository(void)
{
    ST__CFX_CM3_SharedMemory_userSettingValue *p_src, *p_dst;

    p_src = &(g_tdc_fs_ptr_entire_map->map[cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index - 1].user_setting_value);
    p_dst = &cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.userSettingValue_mapData;

    *p_dst = *p_src;
}

void tdc_cfx_eeprom_copy_map_stamp_to_repository(void)
{
    ST__MAPPING_DATE *p_src, *p_dst;

    p_src = &(g_tdc_fs_ptr_entire_map->map[cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index - 1].map_stamp);
    p_dst = &cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.mapStamp;

    *p_dst = *p_src;
}

void tdc_cfx_read_mapdata_mapping_app(void)
{
    tdc_cfx_eeprom_copy_isd_info_to_repository();
    tdc_cfx_eeprom_copy_user_setting_parameters_to_repository();
    tdc_cfx_eeprom_copy_map_stamp_to_repository();

    // 맵 번호가 0일 때는 읽지 맵 데이터를 읽지 않고, 내부기 제조 정보와 사용자 이름까지만 읽는다.
    if (cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index != 0)
    {
        tdc_cfx_eeprom_copy_mapping_data_to_repository();
    }

    cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 0; // 명령어 클리어
}
