// 호스트 테스트용 shim - src/2__cm3/source/fs/tdc_fs_stim_mute.h 를 가린다.
//
// 왜 필요한가
//   진짜 헤더가 <tdc_fs.h> 를 include 하고, tdc_fs.h 가 SDK <ff.h> 와
//   <nvmctrl.h> 를 끌어온다. 상세 경위는 tests/unit/tdc_hal_timer.h 주석 참조
//   (요지 - MinGW 는 _WIN32 를 정의하므로 ff.h 가 <windows.h> 가지를 열고,
//    그 결과 STORAGE_DEVICE_DESCRIPTOR 충돌과 SPI_Type 미정의가 난다).
//
//   tdc_ble_remote.h 가 tdc_fs.h 에 닿는 경로는 셋이며 각각 shim 으로 끊었다.
//     :10 tdc_fs_event_log.h  :11 tdc_hal_timer.h  :13 tdc_fs_stim_mute.h
//
// 무엇을 담는가
//   ble/remote/tdc_ble_remote.c 가 실제로 쓰는 것만 담는다. 값과 시그니처는
//   진짜 헤더(fs/tdc_fs_stim_mute.h:26~44)에서 그대로 옮겼다.
//   진짜 헤더에만 있고 여기 없는 것: FILE_NAME · IDENT · RET_FALSE ·
//   OFFSET_DEFAULT · TDC_FS_STIM_MUTE_T · init. remote 가 쓰지 않는다.

#ifndef __tdc_fs_stim_mute_h__
#define __tdc_fs_stim_mute_h__

#include <stdbool.h>
#include <stdint.h>

#define TDC_FS_STIM_MUTE_UNDER_T_LEVEL_ENABLE  1
#define TDC_FS_STIM_MUTE_UNDER_T_LEVEL_DISABLE 2

#define TDC_FS_STIM_MUTE_T_LEVEL_OFFSET_MIN 0
#define TDC_FS_STIM_MUTE_T_LEVEL_OFFSET_MAX 255

#define TDC_FS_STIM_MUTE_RET_TRUE 0

int tdc_fs_stim_mute_update(uint32_t enable, uint32_t level);

#endif  // __tdc_fs_stim_mute_h__
