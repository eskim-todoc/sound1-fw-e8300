/**
 * @file ci_printf.h
 */

#ifndef __ci_printf_h__
#define __ci_printf_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <ci_uart.h>
#include <SEGGER_RTT.h>

#define CI_PRINTF_INTERFACE_UART       1
#define CI_PRINTF_INTERFACE_SEGGER_RTT 2

#define CI_PRINTF_INTERFACE CI_PRINTF_INTERFACE_SEGGER_RTT

#define CI_PRINT_EABLE_ERROR   1
#define CI_PRINT_EABLE_WARN    1
#define CI_PRINT_EABLE_INFO    1
#define CI_PRINT_EABLE_DEBUG   1
#define CI_PRINT_EABLE_VERBOSE 1

// 파일 이름만 추출하는 매크로
#define __SHORT_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))

void ci_print_file_func_line(const char *file, const char *func, int line);

#if (CI_PRINTF_INTERFACE == CI_PRINTF_INTERFACE_UART)
#define ci_printf(...) ci_uart_printf(__VA_ARGS__)
#elif (CI_PRINTF_INTERFACE == CI_PRINTF_INTERFACE_SEGGER_RTT)

#if CI_PRINT_EABLE_ERROR
#define ci_printe(fmt, ...)                                                                                                                                                                                                                                                                                                    \
    ci_print_file_func_line(__SHORT_FILE__, __func__, __LINE__);                                                                                                                                                                                                                                                               \
    SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_RED fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define ci_printe(fmt, ...)
#endif

#if CI_PRINT_EABLE_WARN
#define ci_printw(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_YELLOW fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define ci_printw(fmt, ...)
#endif

#if CI_PRINT_EABLE_INFO
#define ci_printi(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_GREEN fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define ci_printi(fmt, ...)
#endif

#if CI_PRINT_EABLE_DEBUG
#define ci_printd(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_RESET fmt, ##__VA_ARGS__)
#else
#define ci_printd(fmt, ...)
#endif

#if CI_PRINT_EABLE_VERBOSE
#define ci_printv(fmt, ...) SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_BLACK fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#else
#define ci_printv(fmt, ...)
#endif

// clang-format off
#define ci_error_printf(fmt, ...)                                       \
        ci_print_file_func_line(__SHORT_FILE__, __func__, __LINE__);    \
        SEGGER_RTT_printf(0, RTT_CTRL_TEXT_BRIGHT_RED fmt RTT_CTRL_RESET, ##__VA_ARGS__)
#if 1 // RTT 뷰어 사용 시
#if 0 // 상세 정보 표시 O
#define ci_printf(fmt, ...)                                         \
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
#define ci_printf(fmt, ...)                                         \
    /*ci_print_file_func_line(__SHORT_FILE__, __func__, __LINE__);   */ \
    SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)
#else // 상세 정보 표시 X
#define ci_printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#endif
#else // VT100 기반 터미널 사용 시
#define ci_printf(fmt, ...)                       \
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
#define ci_printf(...)
#endif

#endif  // __ci_printf_h__
