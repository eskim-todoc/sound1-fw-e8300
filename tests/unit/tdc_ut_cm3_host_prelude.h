#ifndef TDC_UT_CM3_HOST_PRELUDE_H
#define TDC_UT_CM3_HOST_PRELUDE_H
#if !defined(TDC_UNIT_TEST) || !defined(TDC_UT_CM3_HOST)
#tdc_sys_error "CM3 host fixture is test-only"
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
/* Windows parameter annotations conflict with the SDK's FENG member names. */
#undef IN
#undef OUT
#undef interface
/* FatFs/NVM과 전체 main은 host에 링크하지 않는다. 제품형/시험 함수는 원본을 쓴다.
 * Windows intrinsic 충돌을 선행 include로 피하고 원래 제품 헤더를 사용한다. */
#define __main_h__
#define __tdc_fs_h__
#define __tdc_hal_dio_h__
int tdc_fs_map_read_isd_info(int isd_num);
int tdc_fs_map_read_user_setting_value(int isd_num);
int tdc_fs_map_read_map_stamp(int isd_num);
int tdc_fs_map_read_map_data(int isd_num, int map_num);
int tdc_fs_fft_read_pass_bin(int bands);
int tdc_fs_fft_init_pass_bin(int bands);
#endif
