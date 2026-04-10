
#include <rtt_printf.h>

void rtt_printf(const char *p_fmt, ...)
{
    char    buf[64];
    int     len;
    va_list ap;

    va_start(ap, p_fmt);
    len = vsnprintf(buf, 64, p_fmt, ap);
    va_end(ap);

    SEGGER_RTT_Write(0, buf, len);
}

int rtt_getch(char *buf)
{
    return SEGGER_RTT_Read(0, buf, 1);
}
