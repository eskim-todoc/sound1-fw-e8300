#ifndef SYS80_HOST_FILESYSTEM_H
#define SYS80_HOST_FILESYSTEM_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ff.h>
#include <ci_crc.h>
#include <processorDirective.h>
#include "product_observer_types.inc"
extern FIL g_ci_filesystem_ohdl;
/* 제품 reader 로그/AES는 시험하지 않는다. AES 경로 진입은 즉시 실패시킨다. */
#define ci_printd(...) ((void)0)
#define ci_printe(...) ((void)0)
#define ci_printv(...) ((void)0)
void ci_aes_decrypt(uint8_t *);
int ci_filesystem_read_with_crc_and_aes128(char *, uint8_t *, int, uint32_t *,
                                          uint32_t *, int, bool, bool);
#endif
