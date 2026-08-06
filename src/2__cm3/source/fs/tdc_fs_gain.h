/*
 * tdc_fs_gain.h
 *
 * Gain Conversion Table 인덱스의 ISD별 영속화.
 *
 * ISD(내부기)는 최대 4개까지 등록되며, 게인은 착용자마다 다르므로 슬롯별로
 * 보관한다. 다만 슬롯당 데이터가 8바이트뿐이고 파일 하나가 4KB 클러스터를
 * 점유하므로, 파일을 4개로 나누지 않고 전체 슬롯을 파일 하나로 관리한다.
 *
 * 파일 구성 (총 40바이트, tdc_fs 의 CRC 래퍼로 기록):
 *   ident_begin(4) + setting[4] * 8 + ident_end(4) = 40
 *   -> (40 % 16) + 4(CRC) + 4(padding) = 16  : 쓰기 함수의 블록 정렬 불변식 충족
 *      (맵 스탬프 파일과 동일한 구성)
 *
 * 무결성은 2단으로 본다.
 *   1) 파일 전체 CRC + 매직넘버 -> 실패 시 전 슬롯 기본값
 *   2) 슬롯별 인덱스 범위        -> 위반한 슬롯만 기본값 (부분 복구)
 */

#ifndef __tdc_fs_gain_h__
#define __tdc_fs_gain_h__

#include <stddef.h>
#include <stdint.h>

#include <processorDirective.h>  // MaxNumUser

#define TDC_FS_GAIN_FILE_NAME        "/GAIN_TBL"
#define TDC_FS_GAIN_FILE_IDENT_BEGIN 0x6A1C0001
#define TDC_FS_GAIN_FILE_IDENT_END   0x1000C1A6

// 40 바이트 데이터 기준 정렬: (40 % 16) + 4 + 4 == 16
#define TDC_FS_GAIN_FILE_PADDING_LEN 4

// 게인 테이블 인덱스 (0 = 뮤트, 128 = 유니티(0 dB), 255 = 최대)
#define TDC_FS_GAIN_TABLE_INDEX_MIN       0
#define TDC_FS_GAIN_TABLE_INDEX_MAX       255
#define TDC_FS_GAIN_TABLE_INDEX_DEFAULT_A 128
#define TDC_FS_GAIN_TABLE_INDEX_DEFAULT_B 128

#define TDC_FS_GAIN_RET_OK   0
#define TDC_FS_GAIN_RET_FAIL (-1)

typedef struct
{
    int gain_table_index_a;  // 마이크 경로 게인
    int gain_table_index_b;  // I2S 크래들 마이크 경로 게인
} tdc_fs_gain_setting_t;     // 8 바이트

typedef struct
{
    uint32_t              ident_begin;
    tdc_fs_gain_setting_t setting[MaxNumUser];  // 슬롯 1~4 -> 인덱스 0~3
    uint32_t              ident_end;
} tdc_fs_gain_file_t;  // 40 바이트

/* 부팅 시 파일을 검사하고 손상되었으면 기본값으로 복구한다.
 * 파일시스템 마운트 이후, CFX iteration 개방 이전에 호출해야 한다. */
int tdc_fs_gain_init(void);

/* 해당 ISD 슬롯의 게인 설정을 읽는다.
 * 실패하더라도 p_out 에는 항상 유효한 값(기본값)이 채워진다. */
int tdc_fs_gain_load(int isd_num, tdc_fs_gain_setting_t *p_out);

/* 해당 ISD 슬롯의 게인 설정을 저장한다. 다른 슬롯 값은 보존된다. */
int tdc_fs_gain_save(int isd_num, const tdc_fs_gain_setting_t *p_in);

/* 해당 ISD 슬롯의 게인 설정을 기본값으로 되돌린다(공장 초기화 시 사용).
 * 다른 슬롯 값은 보존된다. */
int tdc_fs_gain_reset(int isd_num);

#endif /* __tdc_fs_gain_h__ */
