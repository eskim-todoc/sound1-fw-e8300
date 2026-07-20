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

#include <tdc_hal_uart.h>
#include <SEGGER_RTT.h>

#define TDC_PRINTF_INTERFACE_UART       1
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

#if (TDC_PRINTF_INTERFACE == TDC_PRINTF_INTERFACE_UART)
#define TDC_PRINTF(...) tdc_hal_uart_printf(__VA_ARGS__)
#elif (TDC_PRINTF_INTERFACE == TDC_PRINTF_INTERFACE_SEGGER_RTT)

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
#if 1 // RTT 뷰어 사용 시
#if 0 // 상세 정보 표시 O
#define TDC_PRINTF(fmt, ...)                                         \
    SEGGER_RTT_printf(0,                                            \
                    "\r\n"                                          \
                    RTT_CTRL_BG_BLUE RTT_CTRL_TEXT_BRIGHT_BLUE      \
                    "%s"                                            \
                    RTT_CTRL_RESET                                  \
                    " "                                             \
                    RTT_CTRL_BG_CYAN RTT_CTRL_TEXT_BRIGHT_GREEN     \
                    "%s()"                                          \
                    RTT_CTRL_RESET                                  \
                    " "                                             \
                    RTT_CTRL_BG_CYAN RTT_CTRL_TEXT_BRIGHT_YELLOW    \
                    "%4d"                                           \
                    RTT_CTRL_RESET                                  \
                    "\r\n"                                          \
                    fmt,                                            \
                    __SHORT_FILE__,                                 \
                    __func__,                                       \
                    __LINE__,                                       \
                    ##__VA_ARGS__)
#elif 1
#define TDC_PRINTF(fmt, ...)                                         \
    /*tdc_printf_file_func_line(__SHORT_FILE__, __func__, __LINE__);   */ \
    SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)
#else // 상세 정보 표시 X
#define TDC_PRINTF(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#endif
#else // VT100 기반 터미널 사용 시
#define TDC_PRINTF(fmt, ...)                       \
    SEGGER_RTT_printf(0,                          \
                    "\033[4;36m"      "%s"        \
                    "\033[4;31m"      " / "       \
                    "\033[4;32m"      "%s()"      \
                    "\033[4;31m"      " / "       \
                    "\033[4;33m"      "%d"        \
                    "\033[4;31m"      " / "       \
					"\033[0m\033[37m"             \
                    fmt,                          \
                    __SHORT_FILE__,               \
                    __func__,                     \
                    __LINE__,                     \
                    ##__VA_ARGS__)
#endif
// clang-format on
#else
#define TDC_PRINTF(...)
#endif

#endif  // __tdc_printf_h__
