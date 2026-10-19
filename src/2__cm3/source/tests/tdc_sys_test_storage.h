/* 시스템 저장 시험의 전경 실행/RTT 배출. ISR에서 호출하지 않는다. */
#ifndef TDC_SYS_TEST_STORAGE_H
#define TDC_SYS_TEST_STORAGE_H
#include <stdbool.h>
#include <stdint.h>
#include <processorDirective.h>
#if TDC_SYS_TEST_STORAGE_ENABLE
int tdc_sys_storage_command(unsigned id, int argc, char *argv[], bool mapping_connected);
bool tdc_sys_storage_is_active(void);
bool tdc_sys_storage_pending(void);
bool tdc_sys_storage_command_allowed(const char *name);
void tdc_sys_storage_poll(void);
void tdc_sys_storage_log(const char *format, ...);
void tdc_sys_storage_hex(const char *label, const uint8_t *data, unsigned size);
#endif
#endif
