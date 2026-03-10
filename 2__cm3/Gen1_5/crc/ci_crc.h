#ifndef __ci_crc_h__
#define __ci_crc_h__

#include <stdint.h>
#include <hw.h>
#include <crc.h>

uint16_t ci_crc_ccitt_calc(void* buf, uint32_t size);

#endif /* __ci_crc_h__ */
