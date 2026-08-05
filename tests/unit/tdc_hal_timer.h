// 호스트 테스트용 shim - src/2__cm3/source/hal/tdc_hal_timer.h 를 가린다.
//
// 왜 필요한가
//   진짜 헤더는 <main.h> 를 include 하고, main.h -> tdc_fs.h -> SDK <ff.h> 로
//   이어진다. ff.h 는 _WIN32 가 정의돼 있을 때만 <windows.h> 를 끌어오는데
//   (SDK ff.h:38, 주석에 "Windows VC++ (for development only)"), MinGW 가
//   _WIN32 를 정의하므로 호스트 빌드에서만 이 가지가 열린다. 그러면
//     - Windows 의 STORAGE_DEVICE_DESCRIPTOR 와 SDK NVMLIB.h 의 것이 충돌하고
//     - tdc_fs.h 가 함께 끌어오는 nvmctrl.h -> NVMLIB.h -> spi_driver.h 의
//       SPI_Type(ARM CMSIS 타입)이 호스트에 없어
//   컴파일이 무너진다. ARM 빌드에서는 _WIN32 가 없어 발생하지 않는다.
//
//   이 shim 이 main.h 유입을 끊는다. tests/unit 이 첫 include 경로다.
//   src/ 는 한 줄도 건드리지 않는다 (tests/unit 의 src/ 무변경 원칙).
//
// 영향 범위
//   현재 러너가 컴파일하는 src/ 파일 5개(reply · map_measure · map_flash ·
//   cmd_0x65_specific · cmd_0x66_live)는 tdc_hal_timer.h 도 main.h 도
//   include 하지 않는다(실측 0건). 따라서 기존 테스트에 영향이 없다.
//
// 무엇을 담는가
//   ble/remote/tdc_ble_remote.c 가 실제로 쓰는 것만 담는다. 정의는
//   진짜 헤더(hal/tdc_hal_timer.h:37~68)에서 그대로 옮겼다.
//   시그니처가 어긋나면 링크 단계에서 드러난다.

#ifndef __tdc_hal_timer_h__
#define __tdc_hal_timer_h__

#include <stdbool.h>
#include <stdint.h>

#define TDC_HAL_TIMER_BASE_YEAR 2000

typedef struct
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} tdc_hal_timer_time_t;

int  tdc_hal_timer_get_tick(void);
void tdc_hal_timer_update_reference_time(tdc_hal_timer_time_t *p_time, uint32_t count_init_value);

#endif  // __tdc_hal_timer_h__
