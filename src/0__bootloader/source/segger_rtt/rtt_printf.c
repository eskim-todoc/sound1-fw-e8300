
#include <rtt_printf.h>

void rtt_printf(const char *p_fmt, ...)
{
    char    buf[64];
    int     len;
    va_list ap;

    va_start(ap, p_fmt);
    len = vsnprintf(buf, 64, p_fmt, ap);
    va_end(ap);

    /* vsnprintf returns the required length, not the stored length. Never
     * expose bytes past buf when a message is truncated or formatting fails.
     * This legacy API is best-effort; unit-test results use the retry logger.
     */
    if (len > 0)
    {
        unsigned stored = (unsigned) len;
        if (stored >= sizeof(buf))
        {
            stored = sizeof(buf) - 1U;
        }
        SEGGER_RTT_Write(0, buf, stored);
    }
}

int rtt_getch(char *buf)
{
    return SEGGER_RTT_Read(0, buf, 1);
}
