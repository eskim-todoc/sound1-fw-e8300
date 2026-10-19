#ifndef TDC_UT_IO_CM3_HOST_FIXTURE_H
#define TDC_UT_IO_CM3_HOST_FIXTURE_H
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST)
#include <tdc_ut_io_cm3.h>
#include <tdc_boot.h>

typedef struct
{
    int addr;
    int read;
    int count;
    int data[64];
} tdc_ut_io_cm3_transfer_t;

extern tdc_ut_io_cm3_transfer_t g_tdc_ut_io_cm3_transfer[128];
extern int g_tdc_ut_io_cm3_transfer_count;
extern int g_tdc_ut_io_cm3_gpio[32];
extern int g_tdc_ut_io_cm3_bus_error;
extern int g_tdc_ut_io_cm3_bus_error_transfer;
extern int g_tdc_ut_io_cm3_error_code;
extern int g_tdc_ut_io_cm3_error_count;
extern int g_tdc_ut_io_cm3_reset_count;
extern int g_tdc_ut_io_cm3_fs_error;
extern int g_tdc_ut_io_cm3_fs_short;
extern int g_tdc_ut_io_cm3_fs_drive;
extern int g_tdc_ut_io_cm3_fs_close_count;
extern char g_tdc_ut_io_cm3_fs_path[32];
extern unsigned char g_tdc_ut_io_cm3_fs_bytes[256];
extern unsigned g_tdc_ut_io_cm3_fs_length;
extern tdc_boot_status_t g_tdc_ut_io_cm3_boot_status;
extern int g_tdc_ut_io_cm3_boot_error;
void tdc_ut_io_cm3_host_setup(void);
void tdc_ut_io_cm3_host_restore(void);
void tdc_ut_io_cm3_host_rx(const int *data, int count);
void tdc_ut_io_cm3_host_pump(void);
void tdc_ut_io_cm3_host_ack(void);
int tdc_ut_io_cm3_host_tick(void);
#if !defined(TDC_UT_IO_CM3_RAM_ONLY)
int tdc_ut_io_cm3_extended_run(void);
int tdc_ut_io_cm3_touch_ready(void);
void tdc_ut_io_cm3_host_advance(int milliseconds);
int tdc_ut_io_cm3_store_read(const char *name, void *data, int count, bool crc, bool aes);
int tdc_ut_io_cm3_store_write(const char *name, const void *data, int count, bool crc, bool aes);
#endif
#endif
#endif
