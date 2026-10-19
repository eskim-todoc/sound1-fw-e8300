#ifndef SYS80_HOST_FF_H
#define SYS80_HOST_FF_H
#include <stdbool.h>
#include <stdint.h>
/* 호스트 전용 RAM 파일시스템 경계. SDK의 실제 저장장치/ABI 시험은 아니다. */
typedef unsigned UINT;
typedef uint32_t FSIZE_t;
typedef enum { FR_OK = 0, FR_DISK_ERR = 1, FR_NO_FILE = 4, FR_EXIST = 8,
               FR_INVALID_OBJECT = 9, FR_DENIED = 7 } FRESULT;
typedef struct { unsigned file, pos; uint8_t mode; bool open; } FIL;
#define FA_READ 0x01u
#define FA_WRITE 0x02u
#define FA_OPEN_EXISTING 0x00u
#define FA_CREATE_NEW 0x04u
#define FA_OPEN_ALWAYS 0x10u
FRESULT f_open(FIL *, const char *, uint8_t);
FRESULT f_lseek(FIL *, FSIZE_t);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_write(FIL *, const void *, UINT, UINT *);
FRESULT f_sync(FIL *);
FRESULT f_close(FIL *);
FRESULT f_truncate(FIL *);
FRESULT f_unlink(const char *);
FSIZE_t sys80_host_size(const FIL *);
#define f_size(fp) sys80_host_size(fp)
#define f_tell(fp) ((fp)->pos)
#endif
