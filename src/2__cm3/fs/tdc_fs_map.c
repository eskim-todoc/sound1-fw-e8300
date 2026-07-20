/**
 * @file tdc_fs_map.c
 */

#include <tdc_fs_map.h>

int tdc_fs_map_read_isd_info(int isd_num)
{
    static char name[TDC_FS_MAP_FILE_NAME_LEN_ISD_INFO] = TDC_FS_MAP_FILE_INIT_NAME_ISD_INFO;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                 = (char) ('0' + isd_num);

#if 1
    return tdc_fs_read_with_crc_and_aes128(
        name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info, sizeof(ST__CFX_CM3_SharedMemory_ISD_info), &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info_crc_ccitt, g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info_aes128_padding, 8, true, true);
#else
    return tdc_fs_read(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info, sizeof(ST__CFX_CM3_SharedMemory_ISD_info));
#endif
}

int tdc_fs_map_read_user_setting_value(int isd_num)
{
    int         ret;
    static char name[TDC_FS_MAP_FILE_NAME_LEN_USER_SETTING_VALUE] = TDC_FS_MAP_FILE_INIT_NAME_USER_SETTING_VALUE;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                           = (char) ('0' + isd_num);

#if 1
    return tdc_fs_read_with_crc_and_aes128(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value, sizeof(ST__CFX_CM3_SharedMemory_userSettingValue), &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value_crc_ccitt, NULL, 0, true, false);
#else
    return tdc_fs_read(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value, sizeof(ST__CFX_CM3_SharedMemory_userSettingValue));
#endif
}

int tdc_fs_map_read_map_stamp(int isd_num)
{
    int         ret;
    static char name[TDC_FS_MAP_FILE_NAME_LEN_MAP_STAMP] = TDC_FS_MAP_FILE_INIT_NAME_MAP_STAMP;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                  = (char) ('0' + isd_num);
#if 1
    return tdc_fs_read_with_crc_and_aes128(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp, sizeof(ST__MAPPING_DATE), &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp_crc_ccitt, g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp_aes128_padding, 4, true, false);
#else
    return tdc_fs_read(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp, sizeof(ST__MAPPING_DATE));
#endif
}

int tdc_fs_map_read_map_data(int isd_num, int map_num)
{
    int         ret;
    static char name[TDC_FS_MAP_FILE_NAME_LEN_MAP_DATA] = TDC_FS_MAP_FILE_INIT_NAME_MAP_DATA;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                 = (char) ('0' + isd_num);
    name[TDC_FS_MAP_FILE_INDEX_MAP_NUM]                 = (char) ('0' + map_num);

#if 1
    return tdc_fs_read_with_crc_and_aes128(name,
                                                  (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1],
                                                  sizeof(ST__CFX_CM3_SharedMemory_mapData),
                                                  &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data_crc_ccitt[map_num - 1],
                                                  &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data_aes128_padding[map_num - 1][0],
                                                  8,
                                                  true,
                                                  false);
#else
    return tdc_fs_read(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1], sizeof(ST__CFX_CM3_SharedMemory_mapData));
#endif
}

int tdc_fs_map_write_isd_info(int isd_num)
{
    static char name[TDC_FS_MAP_FILE_NAME_LEN_ISD_INFO] = TDC_FS_MAP_FILE_INIT_NAME_ISD_INFO;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                 = (char) ('0' + isd_num);

    TDC_PRINTF_D("[MAP] TRY WRITING, FILE : '%s' \r\n", name);
#if 1
    return tdc_fs_write_with_crc_and_aes128(
        name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info, sizeof(ST__CFX_CM3_SharedMemory_ISD_info), &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info_crc_ccitt, g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info_aes128_padding, 8, true, true);
#else
    return tdc_fs_write(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info, sizeof(ST__CFX_CM3_SharedMemory_ISD_info));
#endif
}

int tdc_fs_map_write_user_setting_value(int isd_num)
{
    int         ret;
    static char name[TDC_FS_MAP_FILE_NAME_LEN_USER_SETTING_VALUE] = TDC_FS_MAP_FILE_INIT_NAME_USER_SETTING_VALUE;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                           = (char) ('0' + isd_num);

    TDC_PRINTF_D("[MAP] TRY WRITING, FILE : '%s' \r\n", name);

#if 1
    return tdc_fs_write_with_crc_and_aes128(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value, sizeof(ST__CFX_CM3_SharedMemory_userSettingValue), &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value_crc_ccitt, NULL, 0, true, false);
#else
    return tdc_fs_write(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value, sizeof(ST__CFX_CM3_SharedMemory_userSettingValue));
#endif
}

int tdc_fs_map_write_map_stamp(int isd_num)
{
    int         ret;
    static char name[TDC_FS_MAP_FILE_NAME_LEN_MAP_STAMP] = TDC_FS_MAP_FILE_INIT_NAME_MAP_STAMP;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                  = (char) ('0' + isd_num);

    TDC_PRINTF_D("[MAP] TRY WRITING, FILE : '%s' \r\n", name);

#if 1
    return tdc_fs_write_with_crc_and_aes128(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp, sizeof(ST__MAPPING_DATE), &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp_crc_ccitt, g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp_aes128_padding, 4, true, false);
#else
    return tdc_fs_write(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp, sizeof(ST__MAPPING_DATE));
#endif
}

int tdc_fs_map_write_map_data(int isd_num, int map_num)
{
    int         ret;
    static char name[TDC_FS_MAP_FILE_NAME_LEN_MAP_DATA] = TDC_FS_MAP_FILE_INIT_NAME_MAP_DATA;
    name[TDC_FS_MAP_FILE_INDEX_ISD_NUM]                 = (char) ('0' + isd_num);
    name[TDC_FS_MAP_FILE_INDEX_MAP_NUM]                 = (char) ('0' + map_num);

    TDC_PRINTF_D("[MAP] TRY WRITING, FILE : '%s' \r\n", name);

#if 1
    return tdc_fs_write_with_crc_and_aes128(name,
                                                   (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1],
                                                   sizeof(ST__CFX_CM3_SharedMemory_mapData),
                                                   &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data_crc_ccitt[map_num - 1],
                                                   &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data_aes128_padding[map_num - 1][0],
                                                   8,
                                                   true,
                                                   false);
#else
    return tdc_fs_write(name, (uint8_t *) &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[map_num - 1], sizeof(ST__CFX_CM3_SharedMemory_mapData));
#endif
}

int tdc_fs_map_init_map_data(int isd_num, bool force_init, bool specific_RL, int val_RL)
{
    ST__CFX_CM3_SharedMemory_ISD_info         *p_info;
    ST__CFX_CM3_SharedMemory_userSettingValue *p_user_setting;
    ST__MAPPING_DATE                          *p_map_stamp;
    ST__CFX_CM3_SharedMemory_mapData          *p_map_data[MaxNumMap];

    int validate_isd_info;
    int validate_user_setting;
    int validate_map_stamp;
    int validate_map_data[MaxNumMap];
    int validate_all;

    // Initialize pointers
    p_info         = &g_tdc_fs_ptr_entire_map->map[isd_num - 1].isd_info;
    p_user_setting = &g_tdc_fs_ptr_entire_map->map[isd_num - 1].user_setting_value;
    p_map_stamp    = &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_stamp;

    for (int i = 0; i < MaxNumMap; i++)
    {
        p_map_data[i] = &g_tdc_fs_ptr_entire_map->map[isd_num - 1].map_data[i];
    }

    validate_isd_info     = tdc_fs_map_read_isd_info(isd_num);
    validate_user_setting = tdc_fs_map_read_user_setting_value(isd_num);
    validate_map_stamp    = tdc_fs_map_read_map_stamp(isd_num);
    validate_map_data[0]  = tdc_fs_map_read_map_data(isd_num, 1);
    validate_map_data[1]  = tdc_fs_map_read_map_data(isd_num, 2);
    validate_map_data[2]  = tdc_fs_map_read_map_data(isd_num, 3);
    validate_map_data[3]  = tdc_fs_map_read_map_data(isd_num, 4);

#if 0
    TDC_PRINTF_V("\r\n\n[MAP] * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  \r\n\n");

    TDC_PRINTF_W("[MAP] ENTIRE ISD '%d' MAP DATA INFO \r\n", isd_num);

    char print_name[25];

    for (int name_i = 0; name_i < 25; name_i++)
    {
        print_name[name_i] = 0;
    }

    for (int name_i = 0; name_i < 25; name_i++)
    {
        print_name[name_i] = p_info->isd_userName[name_i];
    }

    TDC_PRINTF_D("\r\n[MAP] ");
    TDC_PRINTF_I("ISD '%d' INFO ", isd_num);
    TDC_PRINTF_D("YEAR = %d, MONTH = %d, MODEL = %d, SERIAL = 0x%04X, (0x%08X) L/R = %c, NAME = %s, PASSKEY = %c%c%c%c \r\n",  //
              p_info->isd_year,
              (p_info->isd_month_model >> 4) & 0x0F,
              p_info->isd_month_model & 0x0F,
              p_info->isd_serial,
              ((p_info->isd_year << 24) | (p_info->isd_month_model << 8) | p_info->isd_serial),
              p_info->isd_location_RL == Left_Ear    ? 'L'
              : p_info->isd_location_RL == Right_Ear ? 'R'
                                                     : 'X',
              print_name,
              p_info->remocon_passkey[0],
              p_info->remocon_passkey[1],
              p_info->remocon_passkey[2],
              p_info->remocon_passkey[3]);

    TDC_PRINTF_D("\r\n[MAP] ");
    TDC_PRINTF_I("ISD '%d' USER SETTING ", isd_num);
    TDC_PRINTF_D("MAP NUM = %d, STIM VOL = %d, AUDIO VOL = %d, LED = %d, ALARM = %d, TELECOIL = %d, BLE = %d \r\n",  //
              p_user_setting->mapNum,
              p_user_setting->stimulVolume,
              p_user_setting->audioVolume,
              p_user_setting->indicatorLED_OnOff,
              p_user_setting->indicatorStimul_OnOff,
              p_user_setting->teleCoil_OnOff,
              p_user_setting->Ble_Onff);

    TDC_PRINTF_D("\r\n[MAP] ");
    TDC_PRINTF_I("ISD '%d' MAP STAMP ", isd_num);
    TDC_PRINTF_D("YEAR = %d, MONTH = %d, DAY = %d, HOUR = %d, MIN = %d, SEC = %d \r\n",  //
              p_map_stamp->mapping_year,
              p_map_stamp->mapping_month,
              p_map_stamp->mapping_day,
              p_map_stamp->mapping_hour,
              p_map_stamp->mapping_min,
              p_map_stamp->mapping_sec);

    for (int loop_i = 0; loop_i < 4; loop_i++)
    {
        TDC_PRINTF_D("\r\n[MAP] ");
        TDC_PRINTF_I("ISD '%d' MAP '%d' ", isd_num, loop_i + 1);

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("MAPPING DATE : YEAR = %d, MONTH = %d, DAY = %d, HOUR = %d, MIN = %d, SEC = %d \r\n",
                  p_map_data[loop_i]->mappingDate.mapping_year,  //
                  p_map_data[loop_i]->mappingDate.mapping_month,
                  p_map_data[loop_i]->mappingDate.mapping_day,
                  p_map_data[loop_i]->mappingDate.mapping_hour,
                  p_map_data[loop_i]->mappingDate.mapping_min,
                  p_map_data[loop_i]->mappingDate.mapping_sec);

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("STRATEGY = %d (%s), FIRST PHASE = %d (%s), STIM MODE = %d (%s) \r\n",
                  p_map_data[loop_i]->stimulationStrategy,  //
                  p_map_data[loop_i]->stimulationStrategy == 1   ? "CIS"
                  : p_map_data[loop_i]->stimulationStrategy == 2 ? "N-OF-M"
                  : p_map_data[loop_i]->stimulationStrategy == 3 ? "NEW"
                                                                 : "INVALID",
                  p_map_data[loop_i]->firstPulsePhase,
                  p_map_data[loop_i]->firstPulsePhase == 0   ? "NEG"
                  : p_map_data[loop_i]->firstPulsePhase == 1 ? "POS"
                                                             : "INVALID",
                  p_map_data[loop_i]->stimulationMode,
                  p_map_data[loop_i]->stimulationMode == 1   ? "MONO-B"
                  : p_map_data[loop_i]->stimulationMode == 2 ? "MONO_R"
                  : p_map_data[loop_i]->stimulationMode == 3 ? "MONO-RB"
                  : p_map_data[loop_i]->stimulationMode == 4 ? "BP"
                  : p_map_data[loop_i]->stimulationMode == 5 ? "CG"
                  : p_map_data[loop_i]->stimulationMode == 6 ? "SEMI"
                                                             : "INVALID");

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("PULSE WIDTH = %d, FREQ BAND  = %d, ALARM CH = %d, ALARM UA = %d \r\n",
                  p_map_data[loop_i]->stimulationPulsePhaseWidth,  //
                  p_map_data[loop_i]->numFrequencyBand,
                  p_map_data[loop_i]->stimulationIndicatorChannelNum,
                  p_map_data[loop_i]->stimulationIndicatorAmplitude_uA);

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("USABLE STIM ELEC NUM : ");
        for (int loop_k = 0; loop_k < df_MaxNumOfElectrode; loop_k++)
        {
            TDC_PRINTF_D("%2d    ", p_map_data[loop_i]->usableStimulationElectrodIndex[loop_k]);
            if (((loop_k + 1) % 8) == 0)
            {
                TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
                TDC_PRINTF_D("                       ");
            }
        }

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("USABLE REF  ELEC NUM : ");
        for (int loop_k = 0; loop_k < df_MaxNumOfElectrode; loop_k++)
        {
            TDC_PRINTF_D("%2d    ", p_map_data[loop_i]->usableReferenceElectrodIndex[loop_k]);
            if (((loop_k + 1) % 8) == 0)
            {
                TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
                TDC_PRINTF_D("                       ");
            }
        }

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("CIS FREQ BAND ORDER  : ");
        for (int loop_k = 0; loop_k < df_MaxNumOfElectrode; loop_k++)
        {
            TDC_PRINTF_D("%2d    ", p_map_data[loop_i]->CIS_FreqBandOrder[loop_k]);
            if (((loop_k + 1) % 8) == 0)
            {
                TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
                TDC_PRINTF_D("                       ");
            }
        }

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("T LEVEL : ");
        for (int loop_k = 0; loop_k < df_MaxNumOfElectrode; loop_k++)
        {
            TDC_PRINTF_D("%4d    ", p_map_data[loop_i]->T_level_uA[loop_k]);
            if (((loop_k + 1) % 8) == 0)
            {
                TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
                TDC_PRINTF_D("                       ");
            }
        }

        TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
        TDC_PRINTF_D("C LEVEL : ");
        for (int loop_k = 0; loop_k < df_MaxNumOfElectrode; loop_k++)
        {
            TDC_PRINTF_D("%4d    ", p_map_data[loop_i]->C_level_uA[loop_k]);
            if (((loop_k + 1) % 8) == 0)
            {
                TDC_PRINTF_D("\r\n      ");  // "\r\n[MAP] " 자리 만큼
                TDC_PRINTF_D("                       ");
            }
        }
    }
#endif

    SYS_WATCHDOG_REFRESH();

    // ISD info + user name + remocon passkey + user setting value + map stamp + program[0] to program[3]
    // => total 994 word : 3976 bytes

    validate_all = validate_isd_info + validate_user_setting + validate_map_stamp  //
                   + validate_map_data[0] + validate_map_data[1] + validate_map_data[2] + validate_map_data[3];

    // 디버깅 메시지 구문
    if (validate_all < 0)
    {
        TDC_PRINTF_E("[MAP] ISD '%d' IS NOT VALID \r\n", isd_num);
    }
    else
    {
        if (force_init)
        {
            TDC_PRINTF_W("[MAP] ISD '%d' IS VALID BUT, FORCE INIT IS ENABLED \r\n", isd_num);
        }
        else
        {
            TDC_PRINTF_D("[MAP] ISD '%d' IS VALID \r\n", isd_num);
        }
    }

    // ISD information
    if ((validate_isd_info < 0) || (force_init))
    {
#if 1  // 기본 코드
        if (isd_num == 1)
        {
            // ISD info (4 word)
            p_info->isd_year        = 0x18;    //  ISD info (year)
            p_info->isd_month_model = 0x91;    //  ISD info (month, model)
            p_info->isd_serial      = 0x0000;  //  ISD info (serial)

            /* 양이 사용자에 대한 경우 */
            // 왼쪽
#if 1
            p_info->isd_location_RL = 1;  //  ISD info (left/right)

            // 오른쪽
#else
            p_info->isd_location_RL = 2;  //  ISD info (left/right)
#endif
            /* 양이 사용자 끝. */

            if (specific_RL)
            {
                p_info->isd_location_RL = val_RL;
            }
        }

        else
        {
            // ISD info (4 word)
            p_info->isd_year        = 0x12;    //  ISD info (year)
            p_info->isd_month_model = 0x34;    //  ISD info (month, model)
            p_info->isd_serial      = 0x5678;  //  ISD info (serial)
            p_info->isd_location_RL = 1;       //  ISD info (left/right)
        }

        // user name (25 word)
        p_info->isd_userName[0]  = 'T';   // user name 1
        p_info->isd_userName[1]  = 'O';   // user name 2
        p_info->isd_userName[2]  = 'D';   // user name 3
        p_info->isd_userName[3]  = 'O';   // user name 4
        p_info->isd_userName[4]  = 'C';   // user name 5
        p_info->isd_userName[5]  = '_';   // user name 6
        p_info->isd_userName[6]  = 'O';   // user name 7
        p_info->isd_userName[7]  = 'T';   // user name 8
        p_info->isd_userName[8]  = 'E';   // user name 9
        p_info->isd_userName[9]  = 0;     // user name 10
        p_info->isd_userName[10] = 0;     // user name 11
        p_info->isd_userName[11] = 0;     // user name 12
        p_info->isd_userName[12] = 0;     // user name 13
        p_info->isd_userName[13] = 0;     // user name 14
        p_info->isd_userName[14] = 0;     // user name 15
        p_info->isd_userName[15] = 0;     // user name 16
        p_info->isd_userName[16] = 0;     // user name 17
        p_info->isd_userName[17] = 0;     // user name 18
        p_info->isd_userName[18] = 0;     // user name 19
        p_info->isd_userName[19] = 0;     // user name 20
        p_info->isd_userName[20] = 0;     // user name 21
        p_info->isd_userName[21] = 0;     // user name 22
        p_info->isd_userName[22] = 0;     // user name 23
        p_info->isd_userName[23] = 0;     // user name 24
        p_info->isd_userName[24] = 0xFB;  // user name 25
#else                                     // 임의로 내부기 2개 할당 (테스트 코드)
        if (isd_num == 1)
        {
            // ISD info (4 word)
            p_info->isd_year        = 0x18;    //  ISD info (year)
            p_info->isd_month_model = 0x91;    //  ISD info (month, model)
            p_info->isd_serial      = 0x0022;  //  ISD info (serial)
            p_info->isd_location_RL = 2;       //  ISD info (left/right)

            p_info->isd_userName[0] = 'K';  // user name 1
            p_info->isd_userName[1] = 'E';  // user name 2
            p_info->isd_userName[2] = 'S';  // user name 3
            p_info->isd_userName[3] = '_';  // user name 4
            p_info->isd_userName[4] = 'R';  // user name 5
            p_info->isd_userName[5] = 'I';  // user name 6
            p_info->isd_userName[6] = 'G';  // user name 7
            p_info->isd_userName[7] = 'H';  // user name 8
            p_info->isd_userName[8] = 'T';  // user name 9
        }
        else if (isd_num == 2)
        {
            // ISD info (4 word)
            p_info->isd_year        = 0x00;    //  ISD info (year)
            p_info->isd_month_model = 0x00;    //  ISD info (month, model)
            p_info->isd_serial      = 0x00FA;  //  ISD info (serial)
            p_info->isd_location_RL = 1;       //  ISD info (left/right)

            p_info->isd_userName[0] = 'K';  // user name 1
            p_info->isd_userName[1] = 'E';  // user name 2
            p_info->isd_userName[2] = 'S';  // user name 3
            p_info->isd_userName[3] = '_';  // user name 4
            p_info->isd_userName[4] = 'L';  // user name 5
            p_info->isd_userName[5] = 'E';  // user name 6
            p_info->isd_userName[6] = 'F';  // user name 7
            p_info->isd_userName[7] = 'T';  // user name 8
            p_info->isd_userName[8] = 0;    // user name 9
        }
        else
        {
            // ISD info (4 word)
            p_info->isd_year        = 0x12;    //  ISD info (year)
            p_info->isd_month_model = 0x34;    //  ISD info (month, model)
            p_info->isd_serial      = 0x5678;  //  ISD info (serial)
            p_info->isd_location_RL = 1;       //  ISD info (left/right)

            p_info->isd_userName[0] = 'T';  // user name 1
            p_info->isd_userName[1] = 'O';  // user name 2
            p_info->isd_userName[2] = 'D';  // user name 3
            p_info->isd_userName[3] = 'O';  // user name 4
            p_info->isd_userName[4] = 'C';  // user name 5
            p_info->isd_userName[5] = '_';  // user name 6
            p_info->isd_userName[6] = 'O';  // user name 7
            p_info->isd_userName[7] = 'T';  // user name 8
            p_info->isd_userName[8] = 'E';  // user name 9
        }

        // user name (25 word)
        p_info->isd_userName[9]  = 0;     // user name 10
        p_info->isd_userName[10] = 0;     // user name 11
        p_info->isd_userName[11] = 0;     // user name 12
        p_info->isd_userName[12] = 0;     // user name 13
        p_info->isd_userName[13] = 0;     // user name 14
        p_info->isd_userName[14] = 0;     // user name 15
        p_info->isd_userName[15] = 0;     // user name 16
        p_info->isd_userName[16] = 0;     // user name 17
        p_info->isd_userName[17] = 0;     // user name 18
        p_info->isd_userName[18] = 0;     // user name 19
        p_info->isd_userName[19] = 0;     // user name 20
        p_info->isd_userName[20] = 0;     // user name 21
        p_info->isd_userName[21] = 0;     // user name 22
        p_info->isd_userName[22] = 0;     // user name 23
        p_info->isd_userName[23] = 0;     // user name 24
        p_info->isd_userName[24] = 0xFB;  // user name 25
#endif
        // remocon passkey (4 word)
        p_info->remocon_passkey[0] = '1';  // remocon passkey 1
        p_info->remocon_passkey[1] = '1';  // remocon passkey 2
        p_info->remocon_passkey[2] = '1';  // remocon passkey 3
        p_info->remocon_passkey[3] = '1';  // remocon passkey 4

        // TDC_PRINTF_D("[MAP] BEFORE WRITE ISD INFO \r\n");
        tdc_fs_map_write_isd_info(isd_num);
        TDC_PRINTF_D("[MAP] ISD '%d' HAS BEEN INIT \r\n", isd_num);
    }

    SYS_WATCHDOG_REFRESH();

    // User setting values (7 word)
    if ((validate_user_setting < 0) || (force_init))
    {
        p_user_setting->mapNum                = 1;  // map number
        p_user_setting->stimulVolume          = 4;  // stimulation volume
        p_user_setting->audioVolume           = 1;  // audio volume
        p_user_setting->indicatorLED_OnOff    = 1;  // LED on/off
        p_user_setting->indicatorStimul_OnOff = 1;  // stimulation indicator on/off
        p_user_setting->teleCoil_OnOff        = 2;  // telecoil on/off
        p_user_setting->Ble_Onff              = 1;  // BLE on/off

        tdc_fs_map_write_user_setting_value(isd_num);
        TDC_PRINTF_D("[MAP] USER SETTING '%d' HAS BEEN INIT \r\n", isd_num);
    }

    SYS_WATCHDOG_REFRESH();

    // Map stamp (6 word)
    if ((validate_map_stamp < 0) || (force_init))
    {
        p_map_stamp->mapping_year  = 11;  // map stamp year
        p_map_stamp->mapping_month = 11;  // map stamp month
        p_map_stamp->mapping_day   = 11;  // map stamp day
        p_map_stamp->mapping_hour  = 11;  // map stamp hour
        p_map_stamp->mapping_min   = 11;  // map stamp minute
        p_map_stamp->mapping_sec   = 11;  // map stamp second

        tdc_fs_map_write_map_stamp(isd_num);
        TDC_PRINTF_D("[MAP] MAP STAMP '%d' HAS BEEN INIT \r\n", isd_num);
    }

    SYS_WATCHDOG_REFRESH();

    // 1 program = 237 word, total 4 program (progrma[0] to program [3]) = 948 word

    for (int i = 0; i < MaxNumMap; i++)
    {
        if ((validate_map_data[i] < 0) || (force_init))
        {
            // mapping date (6 word)
            p_map_data[i]->mappingDate.mapping_year  = 99;  // mapping date (year)
            p_map_data[i]->mappingDate.mapping_month = 99;  // mapping date (month)
            p_map_data[i]->mappingDate.mapping_day   = 99;  // mapping date (day)
            p_map_data[i]->mappingDate.mapping_hour  = 99;  // mapping date (hour)
            p_map_data[i]->mappingDate.mapping_min   = 99;  // mapping date (min)
            p_map_data[i]->mappingDate.mapping_sec   = 99;  // mapping date (sec)

            // stimulation strategy (1 word)
            p_map_data[i]->stimulationStrategy = 1;  // 1: CIS, 2: nOFm

            /* NOTE: NofM 테스트를 위한 코드. 테스트 후 삭제할 것. */
#if 1
            if (i == TDC_FS_MAP_NUM_3_INDEX)
            {
                p_map_data[i]->stimulationStrategy = 2;  // 1: CIS, 2: nOFm
            }
#endif

            // first pulse phase (1 word)
            if (isd_num == 1)
            {
                switch (i)
                {
                    case TDC_FS_MAP_NUM_3_INDEX:
#if TDC_MAP_TEST_NOFM_USAGE_TIME
                        p_map_data[i]->firstPulsePhase = 0; // negative first
#else
                        p_map_data[i]->firstPulsePhase = 1; // positive first
#endif
                        break;

                    default:
                        p_map_data[i]->firstPulsePhase = 0;  // negative first
                        break;
                }
            }
            else  // isd_num == 2~4
            {
                p_map_data[i]->firstPulsePhase = 0;  // negative first
            }

            // stimulation mode (1 word)
            p_map_data[i]->stimulationMode = 2;  // monopolar-rod

            // stimulation pulse width (1 word)
            if (isd_num == 1)
            {
                switch (i)
                {
                    case TDC_FS_MAP_NUM_2_INDEX:
                        p_map_data[i]->stimulationPulsePhaseWidth = 13;
                        break;

                    case TDC_FS_MAP_NUM_3_INDEX:
#if TDC_MAP_TEST_NOFM_USAGE_TIME
                        p_map_data[i]->stimulationPulsePhaseWidth = 13;
#else
                        p_map_data[i]->stimulationPulsePhaseWidth = 14;
#endif
                        break;

                    case TDC_FS_MAP_NUM_4_INDEX:
                        p_map_data[i]->stimulationPulsePhaseWidth = 255;
                        break;

                    default:
#if TDC_MAP_TEST_NOFM_USAGE_TIME
                        p_map_data[i]->stimulationPulsePhaseWidth = 13;
#else
                        p_map_data[i]->stimulationPulsePhaseWidth = FPGA_pulsePhaseWidth_minimum + 37;  // = 13 + 37
#endif
                        break;
                }
            }
            else
            {
                p_map_data[i]->stimulationPulsePhaseWidth = FPGA_pulsePhaseWidth_minimum + 37;  // =13
            }

            // number of frequency band (1 word)
            if (isd_num == 1)
            {
                switch (i)
                {
                    case TDC_FS_MAP_NUM_2_INDEX:
                        p_map_data[i]->numFrequencyBand = 24;
                        break;

                    default:
                        p_map_data[i]->numFrequencyBand = 32;
                        break;
                }
            }
            else
            {
                p_map_data[i]->numFrequencyBand = 32;
            }

            // tone signal output channel index (1 word)
            p_map_data[i]->stimulationIndicatorChannelNum = 17;

            // tone signal output amplitude (1 word)
            p_map_data[i]->stimulationIndicatorAmplitude_uA = 1500;

            // stimulation channel assigned electrode index (32 word)
            for (int k = 0; k < df_MaxNumOfElectrode; k++)
            {
                p_map_data[i]->usableStimulationElectrodIndex[k] = k + 1;  // 1 to 32
            }

            // reference channel assigned electrode index (32 word)
            for (int k = 0; k < df_MaxNumOfElectrode; k++)
            {
                p_map_data[i]->usableReferenceElectrodIndex[k] = 99;
            }

            // CIS frequency band order (32 word)
            for (int k = 0; k < df_MaxNumOfElectrode; k++)
            {
                p_map_data[i]->CIS_FreqBandOrder[k] = k + 1;  // 1 to 32
            }

            // T level uA (32 word)
            if (isd_num == 1)
            {
                for (int k = 0; k < df_MaxNumOfElectrode; k++)
                {
                    switch (i)
                    {
                        case TDC_FS_MAP_NUM_2_INDEX:
                            if (k == CI_ELEC_NUM_16_INDEX)
                            {
                                p_map_data[i]->T_level_uA[k] = 1500;
                            }
                            else if (k <= CI_ELEC_NUM_24_INDEX)
                            {
                                p_map_data[i]->T_level_uA[k] = 500 + ((1000 * k) / 23);
                            }
                            else  // ELEC NUM 25~32
                            {
                                p_map_data[i]->T_level_uA[k] = 0;
                            }
                            break;

                        default:
#if TDC_MAP_TEST_NOFM_USAGE_TIME
                            p_map_data[i]->T_level_uA[k] = 1200;
#else
                            p_map_data[i]->T_level_uA[k] = (k == CI_ELEC_NUM_16_INDEX) ? 1500 : 500;
#endif
                            break;
                    }
                }
            }
            else
            {
                for (int k = 0; k < df_MaxNumOfElectrode; k++)
                {
                    p_map_data[i]->T_level_uA[k] = 500;
                }
            }

            // C level uA (32 word)
            if (isd_num == 1)
            {
                for (int k = 0; k < df_MaxNumOfElectrode; k++)
                {
                    switch (i)
                    {
                        case TDC_FS_MAP_NUM_2_INDEX:
                            if (k == CI_ELEC_NUM_16_INDEX)
                            {
                                p_map_data[i]->C_level_uA[k] = 1500;
                            }
                            else if (k <= CI_ELEC_NUM_24_INDEX)
                            {
                                p_map_data[i]->C_level_uA[k] = 500 + ((1000 * k) / 23);
                            }
                            else  // ELEC NUM 25~32
                            {
                                p_map_data[i]->C_level_uA[k] = 0;
                            }
                            break;

                        default:
#if TDC_MAP_TEST_NOFM_USAGE_TIME
                            p_map_data[i]->C_level_uA[k] = 1200;
#else
                            p_map_data[i]->C_level_uA[k] = 1500;
#endif
                            break;
                    }
                }
            }
            else
            {
                for (int k = 0; k < df_MaxNumOfElectrode; k++)
                {
                    p_map_data[i]->C_level_uA[k] = 1500;
                }
            }

            // X min level (32 word)
            for (int k = 0; k < df_MaxNumOfElectrode; k++)
            {
#if 1
                p_map_data[i]->audio_input_x_mim[k] = df_minAudioForLogarithm;
#else
                //  NOTE: 디버깅을 위해서 넣은 구문이므로, 테스트 후 df_minAudioForLogarithm 하나만 남기면 됨
                if (i == 0)
                {
                    p_map_data[i]->audio_input_x_mim[k] = df_minAudioForLogarithm;
                }
                else if (i == 1)
                {
                    p_map_data[i]->audio_input_x_mim[k] = df_minAudioForLogarithm + 1;
                }
                else if (i == 2)
                {
                    p_map_data[i]->audio_input_x_mim[k] = df_minAudioForLogarithm + 2;
                }
                else if (i == 3)
                {
                    p_map_data[i]->audio_input_x_mim[k] = df_minAudioForLogarithm + 3;
                }
#endif
            }

            // X max level (32 word)
            for (int k = 0; k < df_MaxNumOfElectrode; k++)
            {
                p_map_data[i]->audio_input_x_max[k] = df_maxAudioForLogarithm;
            }

            tdc_fs_map_write_map_data(isd_num, 1 + i);
            TDC_PRINTF_D("[MAP] ISD '%d' PROGRAM '%d' HAS BEEN INIT \r\n", isd_num, 1 + i);
        }

        SYS_WATCHDOG_REFRESH();
    }  // end for

    SYS_WATCHDOG_REFRESH();

    return df_True;
}

int tdc_fs_map_init_map_data_all(bool force_init)
{
    for (int isd_num = 1; isd_num <= MaxNumUser; isd_num++)
    {
        // tdc_fs_map_init_map_data(isd_num, force_init);
        tdc_fs_map_init_map_data(isd_num, force_init, false, 1);
        SYS_WATCHDOG_REFRESH();
    }

    return df_True;
}
