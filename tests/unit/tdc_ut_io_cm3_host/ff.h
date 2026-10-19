#ifndef TDC_UT_IO_CM3_HOST_FF_H
#define TDC_UT_IO_CM3_HOST_FF_H
#if !defined(TDC_UNIT_TEST) || !defined(TDC_UT_IO_CM3_HOST)
#error This wrapper is only for isolated host tests.
#endif
#include <stdint.h>
/* SDK FatFs C99 원본 정의를 사용하고 Windows 전용 ABI 분기만 제외한다. */
#pragma push_macro("_WIN32")
#undef _WIN32
#include_next <ff.h>
#pragma pop_macro("_WIN32")
#endif
