#ifndef TDC_LINK_TEST_FF_H
#define TDC_LINK_TEST_FF_H
#include <stdint.h>
typedef unsigned UINT;
typedef unsigned FRESULT;
typedef struct { unsigned position; } FIL;
#define FR_OK 0
#define FR_DISK_ERR 1
#define FR_NO_FILE 4
#define FA_READ 1
#define FA_WRITE 2
#define FA_OPEN_ALWAYS 16
FRESULT f_open(FIL *, const char *, unsigned);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_write(FIL *, const void *, UINT, UINT *);
FRESULT f_lseek(FIL *, unsigned);
unsigned f_tell(FIL *);
FRESULT f_sync(FIL *);
FRESULT f_close(FIL *);
#endif
