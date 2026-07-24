/**
 * @file tdc_printf.h
 */

#ifndef __tdc_printf_h__
#define __tdc_printf_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <SEGGER_RTT.h>

#define TDC_PRINTF_INTERFACE_SEGGER_RTT 2

#define TDC_PRINTF_INTERFACE TDC_PRINTF_INTERFACE_SEGGER_RTT

#define TDC_PRINTF_ENABLE_ERROR   1
#define TDC_PRINTF_ENABLE_WARN    1
#define TDC_PRINTF_ENABLE_INFO    1
#define TDC_PRINTF_ENABLE_DEBUG   1
#define TDC_PRINTF_ENABLE_VERBOSE 1

// 파일 이름만 추출하는 매크로
#define __SHORT_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))

void tdc_printf_file_func_line(const char *file, const char *func, int line);

#if (TDC_PRINTF_INTERFACE == TDC_PRINTF_INTERFACE_SEGGER_RTT)

#if TDC_PRINTF_ENABLE_ERROR
#define TDC_PRINTF_E(fmt, ...)                                                                                                                                                                                                                                                                                                    \
    tdc_printf_file_func_line(__SHORT_FILE__, __func__, __LINE__);                                                                                                                                                                                                                                                               \
    SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_RED fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define TDC_PRINTF_E(fmt, ...)
#endif

#if TDC_PRINTF_ENABLE_WARN
#define TDC_PRINTF_W(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_YELLOW fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define TDC_PRINTF_W(fmt, ...)
#endif

#if TDC_PRINTF_ENABLE_INFO
#define TDC_PRINTF_I(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_GREEN fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define TDC_PRINTF_I(fmt, ...)
#endif

#if TDC_PRINTF_ENABLE_DEBUG
#define TDC_PRINTF_D(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_RESET fmt, ##__VA_ARGS__)
#else
#define TDC_PRINTF_D(fmt, ...)
#endif

#if TDC_PRINTF_ENABLE_VERBOSE
#define TDC_PRINTF_V(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_BLACK fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define TDC_PRINTF_V(fmt, ...)
#endif

// clang-format off
#define TDC_PRINTF(fmt, ...)                                         \
    /*tdc_printf_file_func_line(__SHORT_FILE__, __func__, __LINE__);   */ \
    SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)
// clang-format on
#else
#define TDC_PRINTF(...)
#endif

#endif  // __tdc_printf_h__
