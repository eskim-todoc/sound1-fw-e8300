#ifndef TDC_UT_BOOT_HOST_H
#define TDC_UT_BOOT_HOST_H
#if defined(TDC_UNIT_TEST) && defined(TDC_UNIT_TEST_HOST)
#include <stdint.h>
#include <stddef.h>
#include <ci_boot.h>
void tdc_ut_boot_record(unsigned unit, unsigned vector, const int *actual,
                        const int *expected, unsigned length);
int tdc_ut_boot_host_run_unit(unsigned unit);
int tdc_ut_boot_host_service_continue(void);
void tdc_ut_boot_slot_snapshot(snd_boot_status_t *, uint8_t *, FIL **);
void tdc_ut_boot_slot_restore(const snd_boot_status_t *, uint8_t, FIL *);
void tdc_ut_boot_service_snapshot(uint32_t *, uint8_t *);
void tdc_ut_boot_service_restore(uint32_t, uint8_t);
#endif
#endif
