/* Legacy host vectors keep their original default-period boundary.
 * Actual LINKPARM storage/configuration faults run in tests/link_pmic/config_host.c.
 */
#include <tdc_link_config.h>
const tdc_link_config_t *tdc_link_config_get(void)
{
    static const tdc_link_config_t defaults = {140, 172, 3, 4, 1, 0, 0, 214};
    return &defaults;
}
