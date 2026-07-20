/**
 * @file OTE_1_5_gen_CFX_EEPROM_write.c
 */

#include <tdc_fs.h>
#include <fn_from_cfx_eeprom_write.h>

void fn_write_userSettingParameters(int connected_ISD_num)
{
    if (0 < cfx_cm3_sharedMemoryAll.userSettingValue.mapNum)  // 초기화 과정이 다 끝나고, 맵 번호가 0이 아닐 때만 수행할 수 있게 한다.
    {
        g_tdc_fs_ptr_entire_map->map[connected_ISD_num - 1].user_setting_value = cfx_cm3_sharedMemoryAll.userSettingValue;

        tdc_fs_map_write_user_setting_value(connected_ISD_num);  // 신규 코드
    }
}

void fn_write_userSettingParameters_byMapping(void)
{
    int                                                     isd_num;
    ST__CFX_CM3_SharedMemory_RepositoryForReadWriteMapData* p_repo_for_rw_map_data;

    isd_num                = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    p_repo_for_rw_map_data = &cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData;

    g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value = p_repo_for_rw_map_data->userSettingValue_mapData;

    tdc_fs_map_write_user_setting_value(isd_num);  // 신규 코드

    // g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp          = p_repo_for_rw_map_data->readWritemapData.mappingDate;
    // g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp          = p_repo_for_rw_map_data->mapStamp;

    // tdc_fs_map_write_map_stamp(isd_num);          // 신규 코드
}

void fn_write_mapStmpParameters_byMapping(void)
{
    int                                                     isd_num;
    ST__CFX_CM3_SharedMemory_RepositoryForReadWriteMapData* p_repo_for_rw_map_data;

    isd_num                = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    p_repo_for_rw_map_data = &cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData;

    g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp = p_repo_for_rw_map_data->mapStamp;

    tdc_fs_map_write_map_stamp(isd_num);  // 신규 코드
}

void fn_write_ISD_info_byMapping(void)
{
    int isd_num;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;

    if ((0 < isd_num) && (isd_num <= MaxNumUser))
    {
        g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info = cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.ISD_info_mapData;

        tdc_fs_map_write_isd_info(isd_num);  // 신규 코드
    }
}

void fn_write_mapData_byMapping(void)
{
    int                               isd_num, map_num;
    ST__CFX_CM3_SharedMemory_mapData *p_src_map_data, *p_dst_map_data;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    map_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;

    p_src_map_data = &cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.readWritemapData;
    p_dst_map_data = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1]);

    *p_dst_map_data = *p_src_map_data;

    tdc_fs_map_write_map_data(isd_num, map_num);  // 신규 코드
}

void fn_write_Mapdata_mappingApp(void)
{
    int map_num;

    map_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;

    if (map_num == 0)  // 맵 번호가 0이면 내부기 정보 및 사용자 설정 정보를 쓰기
    {
        fn_write_ISD_info_byMapping();
        fn_write_userSettingParameters_byMapping();
        fn_write_mapStmpParameters_byMapping();
    }
    else
    {
        fn_write_mapData_byMapping();
    }

    fn_read_All_isd_info();
    cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 0;
}
