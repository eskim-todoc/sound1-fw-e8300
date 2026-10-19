#ifndef SYS80_HOST_CRC_H
#define SYS80_HOST_CRC_H
#include <stdint.h>
/* 실제 ci_crc.c의 바이트 투입 경로를 실행하고 MMIO만 CCITT 모델로 치환한다. */
#define CRC 1u
#define CRC_LITTLE_ENDIAN 1u
#define CRC_CCITT 2u
#define CRC_BIT_ORDER_STANDARD 0u
#define CRC_FINAL_XOR_STANDARD 0u
void Sys_Set_CRC_Config(unsigned, unsigned);
void Sys_CRC_CCITTInitValue(unsigned);
void Sys_CRC_Add(unsigned, uint32_t, unsigned);
uint32_t Sys_CRC_GetFinalValue(unsigned);
uint16_t ci_crc_ccitt_calc(void *, uint32_t);
#endif
