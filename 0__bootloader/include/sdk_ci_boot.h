/**
 * @file sdk_ci_boot.h
 */

#ifndef __sdk_ci_boot_h__
#define __sdk_ci_boot_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#define SND_BOOT_FILE_NAME "/BOOT_STATUS.TXT"

#define SND_BOOT_VER_MAJOR 0
#define SND_BOOT_VER_MINOR 1

#define SND_BOOT_STATE_BOOT     1
#define SND_BOOT_STATE_ALT_BOOT 2

#define SND_BOOT_SUB_STATE_IDLE          1
#define SND_BOOT_SUB_STATE_BOOT_TRY      2
#define SND_BOOT_SUB_STATE_BOOT_TRY_DONE 3
#define SND_BOOT_SUB_STATE_UNKNOWN       4

#define SND_BOOT_ALT_BOOT_RESULT_NONE    1
#define SND_BOOT_ALT_BOOT_RESULT_SUCCESS 2
#define SND_BOOT_ALT_BOOT_RESULT_FAIL    3

#define SND_BOOT_SLOT_STATE_NONE   1
#define SND_BOOT_SLOT_STATE_USABLE 2

#define SND_BOOT_ALT_BOOT_TRY_MAX 3

// slot 0은 '/' 디렉토리의 이미지를 사용
// slot 1 이상은 '/1/' 과 같이 slot 번호에 해당하는 디렉토리의 이미지를 사용
// (slot 2는 '/2/', slot 3은 '/3/')

typedef struct __attribute__((packed))
{
    uint8_t  major_ver;           //  1 byte  sum:  1
    uint8_t  minor_ver;           //  1 byte  sum:  2
    uint8_t  state;               //  1 byte  sum:  3
    uint8_t  sub_state;           //  1 byte  sum:  4
    uint8_t  boot_slot_num;       //  1 byte  sum:  5
    uint8_t  last_boot_slot_num;  //  1 byte  sum:  6
    uint8_t  alt_boot_slot_num;   //  1 byte  sum:  7
    uint8_t  alt_boot_try_count;  //  1 byte  sum:  8
    uint8_t  alt_boot_result;     //  1 byte  sum:  9
    uint8_t  slot_1_state;        //  1 byte  sum: 10
    uint8_t  slot_2_state;        //  1 byte  sum: 11
    uint8_t  slot_3_state;        //  1 byte  sum: 12
    uint8_t  slot_4_state;        //  1 byte  sum: 13
    uint8_t  reserved[47];        // 47 bytes sum: 60
    uint32_t crc32;               //  4 bytes sum: 64
                                  //    total sum: 64 bytes
} snd_boot_status_t;

#endif // __sdk_ci_boot_h__
