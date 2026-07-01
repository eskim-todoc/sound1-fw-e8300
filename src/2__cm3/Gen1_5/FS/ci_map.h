/**
 * @file ci_map.h
 */

#ifndef __ci_map_h__
#define __ci_map_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>
#include <cfx_cm3_sharedMemory.h>
#include <definitionsForAlgorithm.h>

#include <ci_filesystem.h>

#define CI_MAP_NUM_1_INDEX 0
#define CI_MAP_NUM_2_INDEX 1
#define CI_MAP_NUM_3_INDEX 2
#define CI_MAP_NUM_4_INDEX 3

#define CI_ELEC_NUM_1_INDEX  0
#define CI_ELEC_NUM_2_INDEX  1
#define CI_ELEC_NUM_3_INDEX  2
#define CI_ELEC_NUM_4_INDEX  3
#define CI_ELEC_NUM_5_INDEX  4
#define CI_ELEC_NUM_6_INDEX  5
#define CI_ELEC_NUM_7_INDEX  6
#define CI_ELEC_NUM_8_INDEX  7
#define CI_ELEC_NUM_9_INDEX  8
#define CI_ELEC_NUM_10_INDEX 9
#define CI_ELEC_NUM_11_INDEX 10
#define CI_ELEC_NUM_12_INDEX 11
#define CI_ELEC_NUM_13_INDEX 12
#define CI_ELEC_NUM_14_INDEX 13
#define CI_ELEC_NUM_15_INDEX 14
#define CI_ELEC_NUM_16_INDEX 15
#define CI_ELEC_NUM_17_INDEX 16
#define CI_ELEC_NUM_18_INDEX 17
#define CI_ELEC_NUM_19_INDEX 18
#define CI_ELEC_NUM_20_INDEX 19
#define CI_ELEC_NUM_21_INDEX 20
#define CI_ELEC_NUM_22_INDEX 21
#define CI_ELEC_NUM_23_INDEX 22
#define CI_ELEC_NUM_24_INDEX 23
#define CI_ELEC_NUM_25_INDEX 24
#define CI_ELEC_NUM_26_INDEX 25
#define CI_ELEC_NUM_27_INDEX 26
#define CI_ELEC_NUM_28_INDEX 27
#define CI_ELEC_NUM_29_INDEX 28
#define CI_ELEC_NUM_30_INDEX 29
#define CI_ELEC_NUM_31_INDEX 30
#define CI_ELEC_NUM_32_INDEX 31

#define CI_MAP_FILE_INIT_NAME_ISD_INFO                                                                                                                                                                                                                                                                                         \
    { /* 1    2    3    4    5    6    7    8    9    10   11*/                                                                                                                                                                                                                                                                \
        '/', 'I', 'S', 'D', '*', '_', 'I', 'N', 'F', 'O', '\0'                                                                                                                                                                                                                                                                 \
    }

#define CI_MAP_FILE_INIT_NAME_USER_SETTING_VALUE                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                          \
        '/', 'I', 'S', 'D', '*', '_', 'U', 'S', 'E', 'R', '_', 'S', 'E', 'T', 'T', 'I', 'N', 'G', '\0'                                                                                                                                                                                                                         \
    }

#define CI_MAP_FILE_INIT_NAME_MAP_STAMP                                                                                                                                                                                                                                                                                        \
    {                                                                                                                                                                                                                                                                                                                          \
        '/', 'I', 'S', 'D', '*', 'S', 'T', 'A', 'M', 'P', '\0'                                                                                                                                                                                                                                                                 \
    }

#define CI_MAP_FILE_INIT_NAME_MAP_DATA                                                                                                                                                                                                                                                                                         \
    {                                                                                                                                                                                                                                                                                                                          \
        '/', 'I', 'S', 'D', '*', '_', 'M', 'A', 'P', '*', '\0'                                                                                                                                                                                                                                                                 \
    }

#define CI_MAP_FILE_NAME_LEN_ISD_INFO           11
#define CI_MAP_FILE_NAME_LEN_USER_SETTING_VALUE 19
#define CI_MAP_FILE_NAME_LEN_MAP_STAMP          11
#define CI_MAP_FILE_NAME_LEN_MAP_DATA           11

#define CI_MAP_FILE_INDEX_ISD_NUM 4
#define CI_MAP_FILE_INDEX_MAP_NUM 9

int ci_map_read_isd_info(int isd_num);
int ci_map_read_user_setting_value(int isd_num);
int ci_map_read_map_stamp(int isd_num);
int ci_map_read_map_data(int isd_num, int map_num);

int ci_map_write_isd_info(int isd_num);
int ci_map_write_user_setting_value(int isd_num);
int ci_map_write_map_stamp(int isd_num);
int ci_map_write_map_data(int isd_num, int map_num);

// int ci_map_init_map_data(int isd_num, bool force_init);
int ci_map_init_map_data(int isd_num, bool force_init, bool specific_RL, int val_RL);
int ci_map_init_map_data_all(bool force_init);

#endif  // __ci_map_h__
