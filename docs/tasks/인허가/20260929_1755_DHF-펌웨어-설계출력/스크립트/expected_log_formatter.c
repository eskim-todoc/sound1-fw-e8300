/* Documentation helper only: feed predeclared expected values to the existing
 * formatter. No product functions, device access, or test vectors run here.
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "tdc_ut_log.h"

static int32_t values[65535];
static int32_t expected_values[65535];
static int32_t read_value(const void *context, unsigned index)
{
    (void)context;
    return values[index];
}
static int32_t read_expected(const void *context, unsigned index)
{
    (void)context;
    return expected_values[index];
}
static unsigned write_value(const char *data, unsigned bytes)
{
    return (unsigned)fwrite(data, 1, bytes, stdout);
}
int main(void)
{
    char core[8];
    unsigned unit, vector, count, compare;
    tdc_ut_stream_t stream = {0};
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    while (scanf("%7s %u %u %u %u", core, &unit, &vector, &count, &compare) == 5)
    {
        if (!count || count > 65535) return 2;
        for (unsigned i = 0; i < count; ++i)
            if (scanf("%" SCNd32, &values[i]) != 1) return 3;
        for (unsigned i = 0; i < count; ++i)
            if (scanf("%" SCNd32, &expected_values[i]) != 1) return 3;
        if (tdc_ut_log_begin(&stream, core, unit, vector, count, 0,
                             read_value, compare ? read_expected : 0, write_value) != 0) return 4;
        int result;
        do {
            result = tdc_ut_log_poll(&stream);
            if (result < 0 || ferror(stdout)) return 5;
        } while (result != 1);
    }
    return ferror(stdin) || fflush(stdout) ? 6 : 0;
}
