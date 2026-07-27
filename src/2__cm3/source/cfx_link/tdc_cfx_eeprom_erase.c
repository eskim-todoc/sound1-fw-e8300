/**
 * @file OTE_1_5_gen_CFX_EEPROM_erase.c
 */

#include <tdc_fs.h>
#include <tdc_cfx_eeprom_erase.h>

void tdc_cfx_eeprom_erase_map_stamp_by_mapping(void)
{
    int                  isd_num;
    TDC_FS_MAP_T* p_isd;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    p_isd   = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1]);

    p_isd->map_stamp.mapping_year  = 0;  // 년
    p_isd->map_stamp.mapping_month = 0;  // 월
    p_isd->map_stamp.mapping_day   = 0;  // 일
    p_isd->map_stamp.mapping_hour  = 0;  // 시
    p_isd->map_stamp.mapping_min   = 0;  // 분
    p_isd->map_stamp.mapping_sec   = 0;  // 초

    tdc_fs_map_write_map_stamp(isd_num);  // 신규 코드
}

void tdc_cfx_eeprom_erase_user_setting_parameters_by_mapping(void)
{
    int                  isd_num;
    TDC_FS_MAP_T* p_isd;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    p_isd   = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1]);

    p_isd->user_setting_value.mapNum                = 1;  // 맵 번호
    p_isd->user_setting_value.stimulVolume          = 4;  // 자극 볼륨
    p_isd->user_setting_value.audioVolume           = 1;  // 오디오 볼륨
    p_isd->user_setting_value.indicatorLED_OnOff    = 1;  // LED On/Off (On == 1, Off == 2)
    p_isd->user_setting_value.indicatorStimul_OnOff = 1;  // 자극 알림 On/Off (On == 1, Off == 2)
    p_isd->user_setting_value.teleCoil_OnOff        = 2;  // 텔레코일 On/Off (On == 1, Off == 2)
    p_isd->user_setting_value.Ble_Onff              = 1;  // Ble On/Off 현재는 사용 안함

    tdc_fs_map_write_user_setting_value(isd_num);  // 신규 코드
}

void tdc_cfx_eeprom_erase_isd_info_by_mapping(void)
{
    int                  name_index;
    int                  isd_num;
    TDC_FS_MAP_T* p_isd;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    p_isd   = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1]);

    p_isd->isd_info.isd_year        = 0;  // 년
    p_isd->isd_info.isd_month_model = 0;  // 월, 모델 번호
    p_isd->isd_info.isd_serial      = 0;  // 시리얼
    p_isd->isd_info.isd_location_RL = 3;  // 수술부위 (1: 왼쪽, 2: 오른쪽, 3: NA)

    name_index = 0;  // 사용자 이름 최대 25 워드

    p_isd->isd_info.isd_userName[name_index++] = 'T';
    p_isd->isd_info.isd_userName[name_index++] = 'O';
    p_isd->isd_info.isd_userName[name_index++] = 'D';
    p_isd->isd_info.isd_userName[name_index++] = 'O';
    p_isd->isd_info.isd_userName[name_index++] = 'C';
    p_isd->isd_info.isd_userName[name_index++] = '_';
    p_isd->isd_info.isd_userName[name_index++] = 'O';
    p_isd->isd_info.isd_userName[name_index++] = 'T';
    p_isd->isd_info.isd_userName[name_index++] = 'E';

    for (; name_index < 25; name_index++)
    {
        p_isd->isd_info.isd_userName[name_index] = 0;
    }

    p_isd->isd_info.remocon_passkey[0] = '1';
    p_isd->isd_info.remocon_passkey[1] = '1';
    p_isd->isd_info.remocon_passkey[2] = '1';
    p_isd->isd_info.remocon_passkey[3] = '1';

    tdc_fs_map_write_isd_info(isd_num);  // 신규 코드
}

void tdc_cfx_eeprom_erase_map_data_by_mapping(void)
{
    int                  isd_num, map_num;
    int                  map_num_begin, map_num_end;
    TDC_FS_MAP_T* p_isd;

    isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;
    map_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;

    p_isd = &(g_tdc_fs_ptr_entire_map->map[isd_num - 1]);

    if (map_num == 0)  // map_num이 0이면 모든 맵 지우기
    {
        map_num_begin = 0;
        map_num_end   = MaxNumMap;
    }
    else
    {
        map_num_begin = map_num - 1;
        map_num_end   = map_num;
    }

    for (int i = map_num_begin; i < map_num_end; i++)
    {
        // mapping date (6 word)
        p_isd->map_data[i].mappingDate.mapping_year  = 0;  // mapping date (year)
        p_isd->map_data[i].mappingDate.mapping_month = 0;  // mapping date (month)
        p_isd->map_data[i].mappingDate.mapping_day   = 0;  // mapping date (day)
        p_isd->map_data[i].mappingDate.mapping_hour  = 0;  // mapping date (hour)
        p_isd->map_data[i].mappingDate.mapping_min   = 0;  // mapping date (min)
        p_isd->map_data[i].mappingDate.mapping_sec   = 0;  // mapping date (sec)

        // stimulation strategy (1 word)
        p_isd->map_data[i].stimulationStrategy = 1;  // CIS

        // first pulse phase (1 word)
        p_isd->map_data[i].firstPulsePhase = 0;  // negative first

        // stimulation mode (1 word)
        p_isd->map_data[i].stimulationMode = 2;  // monopolar-rod

        // stimulation pulse width (1 word)
        p_isd->map_data[i].stimulationPulsePhaseWidth = 25;

        // number of frequency band (1 word)
        p_isd->map_data[i].numFrequencyBand = 32;

        // tone signal output channel index (1 word)
        p_isd->map_data[i].stimulationIndicatorChannelNum = 32;

        // tone signal output amplitude (1 word)
        p_isd->map_data[i].stimulationIndicatorAmplitude_uA = 0;

        // stimulation channel assigned electrode index (32 word)
        for (int k = 0; k < df_MaxNumOfElectrode; k++)
        {
            p_isd->map_data[i].usableStimulationElectrodIndex[k] = 1 + k;  // 1 to 32
        }

        // reference channel assigned electrode index (32 word)
        for (int k = 0; k < df_MaxNumOfElectrode; k++)
        {
            p_isd->map_data[i].usableReferenceElectrodIndex[k] = 99;
        }

        // CIS frequency band order (32 word)
        for (int k = 0, m = 16; k < df_MaxNumOfElectrode / 2; k++, m++)
        {
            p_isd->map_data[i].CIS_FreqBandOrder[k] = 1 + k;  // 1 to 16
            p_isd->map_data[i].CIS_FreqBandOrder[m] = 1 + m;  // 17 to 32
        }

        // T level uA (32 word)
        for (int k = 0; k < df_MaxNumOfElectrode; k++)
        {
            p_isd->map_data[i].T_level_uA[k] = 0;
        }

        // C level uA (32 word)
        for (int k = 0; k < df_MaxNumOfElectrode; k++)
        {
            p_isd->map_data[i].C_level_uA[k] = 0;
        }

        // Audio min
        for (int k = 0; k < df_MaxNumOfElectrode; k++)
        {
        	p_isd->map_data[i].audio_input_x_mim[k] = df_minAudioForLogarithm;
        }

        // Audio max
        for (int k = 0; k < df_MaxNumOfElectrode; k++)
        {
            p_isd->map_data[i].audio_input_x_max[k] = df_maxAudioForLogarithm;
        }

        tdc_fs_map_write_map_data(isd_num, i + 1 /* map_num */);
    }
}

void tdc_cfx_eeprom_erase_mapdata_mapping_app(void)
{
    int map_num;

    map_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;

    if (map_num == 0)  // 맵 번호가 0이면 내부기 정보 및 사용자 설정 정보를 제거
    {
        tdc_cfx_eeprom_erase_isd_info_by_mapping();               // ISD 정보의 메타데이터 항목 제거
        tdc_cfx_eeprom_erase_user_setting_parameters_by_mapping();  // ISD 정보의 사용자 설정 값 제거
        tdc_cfx_eeprom_erase_map_stamp_by_mapping();               // ISD 정보의 맵 스탭프 제어
        tdc_cfx_eeprom_erase_map_data_by_mapping();                // ISD 정보의 모든 맵 데이터 제거
        tdc_cfx_eeprom_read_all_isd_info();                      // 모든 ISD 정보를 다시 불러오기
    }
    else
    {
        tdc_cfx_eeprom_erase_map_data_by_mapping();  // ISD 정보의 특정 맵 데이터 제거
    }

    cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 0; // 명령어 플래그 클리어
}
