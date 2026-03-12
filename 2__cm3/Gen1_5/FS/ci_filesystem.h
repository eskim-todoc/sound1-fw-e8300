/**
 * @file OTE_1_5_gen_FS.h
 */

#ifndef __OTE_1_5_gen_FS_h__
#define __OTE_1_5_gen_FS_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ff.h>
#include <nvmctrl.h>
#include <nvmlib.h>

#include <FPGA.h>
#include <cfx_cm3_sharedMemory.h>
#include <definitionsForAlgorithm.h>
#include <processorDirective.h>
#include <99_eeprom_address.h>

#include <ci_uart.h>
#include <ci_printf.h>
#include <aes.h>
#include <ci_crc.h>

// Pointer to the null-terminated string that specifies the logical drive.
// The string without drive number means the default drive.
// #define CI_FILESYSTEM_LOGICAL_DRIVE_NUM "0:"  // Default drive.
#define CI_FILESYSTEM_LOGICAL_DRIVE_NUM                 "1:"  // Default drive.
#define CI_FILESYSTEM_LOGICAL_DRIVE_NUM_FOR_BOOT_STATUS "0:"  // Default drive.

#define SND_FATFS_LDRV_NUM_BOOT      0
#define SND_FATFS_LDRV_NUM_USER_DATA 1

// 0: Do not mount now (to be mounted on the first access to the volume),
// 1: Force mounted the volume to check if it is ready to work.
#define SND_FATFS_MOUNT_OPTION 1

// FS의 FFT PASS BIN 베이스 주소
#define CI_FILESYSTEM_BASE_ADDR_FFT_PASS_BIN DSP_PRAM4_REMAP_BASE

// FS의 FFT WINDOW 베이스 주소
#define OTE_1_5_GEN_FS_HANN_WINDOW_COEFF_ADDR_OFFSET_BYTES 4096
#define OTE_1_5_GEN_FS_HANN_WINDOW_COEFF_BASE_ADDR         (CI_FILESYSTEM_BASE_ADDR_FFT_PASS_BIN + OTE_1_5_GEN_FS_HANN_WINDOW_COEFF_ADDR_OFFSET_BYTES)

// FS의 LOG 베이스 주소
#define CI_FILESYSTEM_OFFSET_BYTES_FOR_EVENT_LOG_OFFSET_BYTES (OTE_1_5_GEN_FS_HANN_WINDOW_COEFF_ADDR_OFFSET_BYTES + 4096)
#define CI_FILESYSTEM_BASE_ADDR_FOR_EVENT_LOG                 (CI_FILESYSTEM_BASE_ADDR_FFT_PASS_BIN + CI_FILESYSTEM_OFFSET_BYTES_FOR_EVENT_LOG_OFFSET_BYTES)

// FS의 맵 데이터 베이스 주소
#define CI_FILESYSTEM_BASE_ADDR_ENTIRE_MAP   DSP_PRAM3_REMAP_BASE
#define OTE_1_5_GEN_FS_MAP_DATA_OFFSET_WORDS 994
#define OTE_1_5_GEN_FS_MAP_DATA_OFFSET_BYTES 3976

#define ENABLE_DETAIL_MESSAGE_FOR_FILE_READ_WRITE 1

typedef struct
{
    ST__CFX_CM3_SharedMemory_ISD_info isd_info;                    // 132 바이트 (33 워드)
    uint32_t                          isd_info_crc_ccitt;          //   4 바이트 ( 1 워드) 중 2 바이트 사용
    uint32_t                          isd_info_aes128_padding[2];  //   8 바이트 ( 2 워드) 0 값으로 패딩
    // ISD 정보 : 132 + 4 + 8 = 144 바이트 (36 워드)

    ST__CFX_CM3_SharedMemory_userSettingValue user_setting_value;            // 28 바이트 (7 워드)
    uint32_t                                  user_setting_value_crc_ccitt;  //  4 바이트 (1 워드)
    // 사용자 설정 값 : 28 + 4 = 32 바이트 (8 워드)

    ST__MAPPING_DATE map_stamp;                    // 24 바이트 (6 워드)
    uint32_t         map_stamp_crc_ccitt;          //  4 바이트 (1 워드)
    uint32_t         map_stamp_aes128_padding[1];  //  4 바이트 (1 워드)
    // 맵 스탬프 : 24 + 4 + 4 = 32 바이트 (8 워드)

    ST__CFX_CM3_SharedMemory_mapData map_data[4];                    // 각 948 바이트 (237 워드)
    uint32_t                         map_data_crc_ccitt[4];          // 각   4 바이트 (  1 워드)
    uint32_t                         map_data_aes128_padding[4][2];  // 각   8 바이트 (  2 워드)
    // 개별 맵 데이터 : 948 + 4 + 8 =  960 바이트 (240 워드)
    // 전체 맵 데이터 : 960 * 4     = 3840 바이트 (960 워드)

    // 내부기 1EA에 대한 총 메모리 크기
    //
    //   ISD 정보       (144 바이트, 36 워드)
    // + 사용자 설정 값 (32 바이트, 8 워드)
    // + 맵 스탬프      (32 바이트, 8 워드)
    // + 전체 맵 데이터 (3840 바이트, 960 워드)
    //
    // = 4048 바이트, 1012 워드

} CI_FILESYSTEM_MAP_T;

typedef struct
{
    CI_FILESYSTEM_MAP_T map[MaxNumUser];
    // 각 맵 정보 4048 바이트, 1012 워드
    // 총 사용자 맵 정보는 16192 바이트, 4048 워드
    // LPDSP32 PRAM3 영역의 크기인 4096 워드를 거의 다 씀 (약 98.83% 사용)

} CI_FILESYSTEM_ENTIRE_MAP_T;

typedef struct
{
    int num_of_freq_band;
    int fft_pass_bin[Half_FFT_Size];
} CI_FILESYSTEM_FFT_PASS_BIN_T;

//
// extern system variables
//
extern CI_FILESYSTEM_FFT_PASS_BIN_T *g_ci_filesystem_ptr_pass_bin;
extern CI_FILESYSTEM_ENTIRE_MAP_T   *g_ci_filesystem_ptr_entire_map;
extern FATFS                         g_snd_fatfs_mount;
extern FIL                           g_snd_fatfs_ohdl;

//
// function headers
//

FIL *ci_filesystem_get_fp(void);

const NVMCTRL_Options_t *ci_filesystem_get_nvmctrl_option(void);

int ci_filesystem_nvm_init(void);
int ci_filesystem_nvm_reinit(void);

int snd_fatfs_init_mem_map(void);
int snd_fatfs_remount(int ldrv);
int snd_fatfs_mount(int ldrv);
int snd_fatfs_unmount(void);

int ci_filesystem_remount(void);
int ci_filesystem_mount(void);

int ci_filesystem_read_with_crc_and_aes128(char *p_name, uint8_t *p_data, int data_size, uint32_t *p_uint32_crc, uint32_t *p_uint32_aes128_padding, int aes128_padding_size, bool enable_crc, bool enable_aes);

int ci_filesystem_write_with_crc_and_aes128(char     *p_name,
                                            uint8_t  *p_data,                   // 평문 데이터(data_size)
                                            int       data_size,                // 예: 132
                                            uint32_t *p_uint32_crc,             // 4B (하위 16비트만 유효)
                                            uint32_t *p_uint32_aes128_padding,  // 패딩 버퍼(쓰기 전용, 0 채움 권장)
                                            int       aes128_padding_size,      // 예: 8  (remain+4+pad==16 충족)
                                            bool      enable_crc,
                                            bool      enable_aes);

int ci_filesystem_read(char* p_name, uint8_t* p_buf, int size);
int ci_filesystem_write(char *p_name, uint8_t *p_buf, int size);
int ci_filesystem_copy_isd_info_from_filesystem_to_shared_memory(void);

#endif // __OTE_1_5_gen_FS_h__
