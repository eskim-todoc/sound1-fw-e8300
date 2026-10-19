/* Legacy host boundary only; actual configuration storage tested in link_pmic. */
#include <tdc_link_config.h>
const tdc_link_config_t *tdc_link_config_get(void)
{
    static const tdc_link_config_t defaults = {140, 172, 3, 4, 1, 0, 0, 214};
    return &defaults;
}
