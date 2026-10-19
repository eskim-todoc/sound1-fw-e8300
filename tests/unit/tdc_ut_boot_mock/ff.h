#ifndef TDC_UT_BOOT_MOCK_FF_H
#define TDC_UT_BOOT_MOCK_FF_H
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
/* FatFs 파일 경계: 호스트 byte count는 size_t. 영속 상태 구조는 제품 헤더다. */
typedef size_t UINT;
typedef size_t FSIZE_t;
typedef int FRESULT;
typedef struct { FILE *file; struct { uint32_t sclust; } obj; } FIL;
typedef struct { unsigned unused; } FATFS;
typedef struct { unsigned unused; } DIR;
typedef struct { size_t fsize; char fname[256]; } FILINFO;
#define FR_OK 0
#define FR_DISK_ERR 1
#define FR_NO_FILE 4
#define FR_NO_PATH 5
#define FR_INVALID_OBJECT 9
#define FA_READ 1
#define FA_WRITE 2
#define FA_OPEN_EXISTING 0
#define FA_OPEN_ALWAYS 0x10
FRESULT f_open(FIL *, const char *, unsigned);
FRESULT f_close(FIL *);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_write(FIL *, const void *, UINT, UINT *);
FRESULT f_lseek(FIL *, size_t);
FRESULT f_sync(FIL *);
FRESULT f_stat(const char *, FILINFO *);
FRESULT f_chdir(const char *);
FRESULT f_mount(FATFS *, const char *, unsigned);
FRESULT f_chdrive(const char *);
FRESULT f_opendir(DIR *, const char *);
FRESULT f_closedir(DIR *);
FRESULT f_unmount(const char *);
#endif
