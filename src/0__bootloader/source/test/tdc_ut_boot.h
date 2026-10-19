#ifndef TDC_UT_BOOT_H
#define TDC_UT_BOOT_H
#if defined(TDC_UNIT_TEST)
#include <stdint.h>
#include <ff.h>
#include <sdk_ci_boot.h>

#define TDC_UT_BOOT_CAPACITY 128U
#define TDC_UT_BOOT_UNSUPPORTED (-2)
#define TDC_UT_BOOT_BUSY (-3)
#define TDC_UT_BOOT_ERROR (-1)

extern int g_tdc_ut_boot_actual[TDC_UT_BOOT_CAPACITY];
extern int g_tdc_ut_boot_expected[TDC_UT_BOOT_CAPACITY];
extern unsigned g_tdc_ut_boot_count;
extern int g_tdc_ut_boot_done;
extern int g_tdc_ut_boot_overflow;

/* 다른 부트 처리/파일/CRC 사용이 멈춘 전경에서만 호출한다. unit=1..20.
 * 기존 부트 UART가 준비되어 있어야 한다. CRC 레지스터 접근과 기존 슬롯
 * 처리의 watchdog refresh는 실제 제품 함수를 그대로 사용한다.
 * 0은 실행 완료, -2는 미지원이다.
 * 실제값/기대값은 [-999,unit,case,length,values...] 같은 메타데이터다.
 * done은 PASS가 아니다. RTT drain 후 release 전까지 배열을 바꾸지 않는다.
 * 타깃: 9/14 전체 벡터, 8/10/11/12/13/19/20 RAM 하위 벡터.
 * 나머지 파일/장치 시험은 타깃에서 -2다. CRC 시험은 전경 독점 사용한다. */
int tdc_ut_boot_run_unit(unsigned unit);
void tdc_ut_boot_release(void);

/* 타깃에서 응용 키 대신 공개 시험 키를 쓴다. 0 초기화는 override 없음이다. */
void tdc_ut_boot_set_public_key(uint32_t *key);
uint32_t *tdc_ut_boot_public_key(void);

typedef FRESULT (*tdc_ut_boot_file_read_t)(void *context, FIL *file, void *out,
                                          UINT bytes, UINT *read);
typedef struct {
    tdc_ut_boot_file_read_t read;
    void *context;
} tdc_ut_boot_file_hook_t;
/* 기본값 모두 0. 설정/조회는 복사이고 RAM fixture 수명은 호출자가 소유한다.
 * 정상 함수들은 hook이 없으면 SDK f_read를 그대로 사용한다. */
void tdc_ut_boot_file_hook_get(tdc_ut_boot_file_hook_t *hook);
void tdc_ut_boot_file_hook_set(const tdc_ut_boot_file_hook_t *hook);
FRESULT tdc_ut_boot_read(FIL *file, void *out, UINT bytes, UINT *read);

/* 파일 존재/읽기/갱신의 논리만 RAM에서 실행한다. 물리 NVM, 내구성,
 * 전원 차단 후 보존을 대신하지 않는다. 모든 콜백의 기본값은 NULL이다.
 * 파일 구조와 슬롯 상태를 다른 코드가 사용하는 동안 설치하지 않는다. */
typedef struct {
    FRESULT (*open)(void *context, FIL *file, const char *name, unsigned mode);
    FRESULT (*close)(void *context, FIL *file);
    FRESULT (*stat)(void *context, const char *name, FILINFO *info);
    FRESULT (*write)(void *context, FIL *file, const void *in, UINT bytes, UINT *written);
    FRESULT (*seek)(void *context, FIL *file, FSIZE_t offset);
    FRESULT (*sync)(void *context, FIL *file);
    FRESULT (*chdir)(void *context, const char *name);
    void *context;
} tdc_ut_boot_file_ops_t;
void tdc_ut_boot_file_ops_get(tdc_ut_boot_file_ops_t *ops);
void tdc_ut_boot_file_ops_set(const tdc_ut_boot_file_ops_t *ops);
FRESULT tdc_ut_boot_open(FIL *file, const char *name, unsigned mode);
FRESULT tdc_ut_boot_close(FIL *file);
FRESULT tdc_ut_boot_stat(const char *name, FILINFO *info);
FRESULT tdc_ut_boot_write(FIL *file, const void *in, UINT bytes, UINT *written);
FRESULT tdc_ut_boot_seek(FIL *file, FSIZE_t offset);
FRESULT tdc_ut_boot_sync(FIL *file);
FRESULT tdc_ut_boot_chdir(const char *name);
int tdc_ut_boot_read_override_active(void);
#define TDC_UT_BOOT_SCRATCH_WORDS 1020U
/* scratch RAM만 교환한다. 시험 전/후 같은 버퍼로 호출하여 원래 값을 복원한다. */
void tdc_ut_boot_scratch_exchange(uint32_t data[TDC_UT_BOOT_SCRATCH_WORDS]);

typedef int (*tdc_ut_boot_nvm_read_t)(void *context, uint32_t offset,
                                    uint32_t bytes, uint32_t *out);
typedef struct {
    tdc_ut_boot_nvm_read_t read;
    void *context;
} tdc_ut_boot_nvm_hook_t;
/* 공개한 RAM 시험 부트 정보만 넣는다. 제조 보정/전압/클럭 적용은 하지 않는다. */
void tdc_ut_boot_nvm_hook_get(tdc_ut_boot_nvm_hook_t *hook);
void tdc_ut_boot_nvm_hook_set(const tdc_ut_boot_nvm_hook_t *hook);
int tdc_ut_boot_nvm_read(uint32_t offset, uint32_t bytes, uint32_t *out);

/* 실제 private 슬롯 상태를 저장/복원한다. 파일 및 전송에는 접근하지 않는다. */
void tdc_ut_boot_slot_snapshot(snd_boot_status_t *status, uint8_t *slot, FIL **fp);
void tdc_ut_boot_slot_restore(const snd_boot_status_t *status, uint8_t slot, FIL *fp);

enum {
    TDC_UT_BOOT_OBSERVE_TRANSFER = 1,
    TDC_UT_BOOT_OBSERVE_LOCK = 2,
    TDC_UT_BOOT_OBSERVE_LED = 3,
    TDC_UT_BOOT_OBSERVE_SERVICE_LED = 4,
    TDC_UT_BOOT_OBSERVE_REBOOT = 5
};
typedef void (*tdc_ut_boot_observer_t)(unsigned event, const uint32_t *values,
                                      unsigned count);
/* 기본값 0. 명시적 장치 시험에서만 사용한다. observer는 값을 즉시 복사해야
 * 하며 IRQ에서는 출력/블로킹을 하지 않는다. 관찰만으로 PASS를 만들지 않는다.
 * get/set은 인터럽트 중지 등 호출자가 보장한 정지 상태에서 수행한다. */
tdc_ut_boot_observer_t tdc_ut_boot_observer_get(void);
void tdc_ut_boot_observer_set(tdc_ut_boot_observer_t observer);
void tdc_ut_boot_observe(unsigned event, const uint32_t *values, unsigned count);
#endif
#endif
