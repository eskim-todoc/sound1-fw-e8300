#include <tdc_printf.h>

void tdc_printf_file_func_line(const char* file, const char* func, int line)
{

#if 0
    SEGGER_RTT_printf(0, "\r\n" RTT_CTRL_BG_CYAN RTT_CTRL_TEXT_MAGENTA "%s" RTT_CTRL_RESET "\r\n", file);
#else
    SEGGER_RTT_printf(0, "\r\n" RTT_CTRL_BG_CYAN RTT_CTRL_TEXT_MAGENTA "%s" RTT_CTRL_RESET " ", file);
#endif
    SEGGER_RTT_printf(0, RTT_CTRL_BG_CYAN RTT_CTRL_TEXT_BRIGHT_GREEN "%s()" RTT_CTRL_RESET " ", func);
    SEGGER_RTT_printf(0, RTT_CTRL_BG_CYAN RTT_CTRL_TEXT_BRIGHT_YELLOW "%d" RTT_CTRL_RESET "\r\n", line);
    // SEGGER_RTT_printf(0, "                              ");
}
