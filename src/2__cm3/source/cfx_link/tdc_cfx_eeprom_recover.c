/**
 * @file tdc_cfx_eeprom_recover.c
 */

#include <tdc_fs.h>
#include <tdc_cfx_eeprom_recover.h>
#include <tdc_fs_gain.h>  // 공장 초기화 시 게인 설정도 되돌린다

void tdc_cfx_eeprom_recover_map_data_by_mapping(void)
{
    int           isd_num, map_num;
    int           map_num_begin, map_num_end;
    TDC_FS_MAP_T *p_isd;

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
        p_isd->map_data[i].mappingDate.mapping_year  = 99;  // mapping date (year)
        p_isd->map_data[i].mappingDate.mapping_month = 99;  // mapping date (month)
        p_isd->map_data[i].mappingDate.mapping_day   = 99;  // mapping date (day)
        p_isd->map_data[i].mappingDate.mapping_hour  = 99;  // mapping date (hour)
        p_isd->map_data[i].mappingDate.mapping_min   = 99;  // mapping date (min)
        p_isd->map_data[i].mappingDate.mapping_sec   = 99;  // mapping date (sec)

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
            p_isd->map_data[i].usableStimulationElectrodIndex[k] = k + 1;  // 1 to 32
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

void tdc_cfx_eeprom_recover_mapdata_mapping_app(void)
{
    int isd_num, map_num;

    map_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index;

    if (map_num == 0)  // 맵 번호가 0이면 내부기 정보 및 사용자 설정 정보를 초기화
    {
        isd_num = cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index;

        tdc_cfx_eeprom_erase_isd_info_by_mapping();                 // ISD 정보의 메타데이터 항목 제거
        tdc_cfx_eeprom_erase_user_setting_parameters_by_mapping();  // ISD 정보의 사용자 설정 값 제거
        tdc_cfx_eeprom_erase_map_stamp_by_mapping();                // ISD 정보의 맵 스탭프 제어
        tdc_cfx_eeprom_recover_map_data_by_mapping();               // ISD 정보의 모든 맵 데이터 초기화
        tdc_fs_gain_reset(isd_num);                                 // ISD 정보의 게인 설정 초기화
        tdc_cfx_eeprom_read_all_isd_info();                         // 모든 ISD 정보를 다시 불러오기
    }
    else
    {
        tdc_cfx_eeprom_recover_map_data_by_mapping();  // ISD 정보의 모든 맵 데이터 초기화
    }

    cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 0;  // 명령어 플래그 클리어
}
