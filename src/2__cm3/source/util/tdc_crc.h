#ifndef __tdc_crc_h__
#define __tdc_crc_h__

#include <stdint.h>
#include <hw.h>
#include <crc.h>

uint16_t tdc_crc_ccitt_calc(void *buf, uint32_t size);

#endif /* __tdc_crc_h__ */
