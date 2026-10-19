#ifndef TDC_UT_IO_CM3_H
#define TDC_UT_IO_CM3_H

#if defined(TDC_UNIT_TEST)
#include <tdc_hal_i2c.h>
#include <tdc_dfu_ble_ota.h>
#include <tdc_boot.h>
#define TDC_UT_IO_CM3_CAPACITY 512
#define TDC_UT_IO_CM3_OK 0
#define TDC_UT_IO_CM3_UNSUPPORTED (-1)
#define TDC_UT_IO_CM3_INVALID (-2)
#define TDC_UT_IO_CM3_BUSY (-3)

extern int g_tdc_ut_io_cm3_actual[TDC_UT_IO_CM3_CAPACITY];
extern int g_tdc_ut_io_cm3_expected[TDC_UT_IO_CM3_CAPACITY];
extern int g_tdc_ut_io_cm3_count;
extern int g_tdc_ut_io_cm3_overflow;
extern volatile int g_tdc_ut_io_cm3_done;
extern int g_tdc_ut_io_cm3_observed;

/* 설정: CFX 실행 금지, SPI/I2C idle, 모든 다른 코어/ISR 사용자를 정지한다.
 * 입력: 기준서의 CM3 유닛 번호. RAM fixture 가능 여부는 coverage JSON 참조.
 * 출력: [-999,unit,case,count,actual...]와 별도 expected, done은 종료만 뜻한다.
 * 복원: 공유 상태/시험 훅/송신 데이터/센서 ULP 상태를 복원한다.
 * done 이후 RTT drain을 마친 호출자가 release해야 다음 실행이 허용된다.
 * 미지원은 UNSUPPORTED, 사용 중은 BUSY다. 장치 쓰기를 자동 실행하지 않는다.
 */
int tdc_ut_io_cm3_run_unit(int unit);
int tdc_ut_io_cm3_release(void);
/* 시험자가 기준 입력과 실제 함수를 별도로 실행한 뒤 quiescent 시점에 호출한다.
 * 입력: 유닛/양수 case. 출력: 상태 snapshot만; expected를 사용하지 않는다.
 * reporter의 observed_begin 또는 expected=NULL로 전송한다. PASS 의미가 없다.
 * NVM read/write/delete, 장치 루프, reset은 호출하지 않고 제품 상태도 복원/변경하지 않는다.
 */
int tdc_ut_io_cm3_observe_unit(int unit, int case_id);

/* 0 초기값은 기존 드라이버 경로를 유지한다. 시험의 단일 스레드에서만 설정한다. */
extern void (*g_tdc_ut_io_cm3_i2c_poll_hook)(void);
/* I2C의 MMIO/IRQ 경계만 대체한다. 실제 driver/ISR 상태 전이는 그대로 실행한다.
 * action 1=status clear, 2=write start, 3=read start, 4=TX, 5=CTRL, 6=reset.
 * NULL은 기존 하드웨어 경로이며 활성 fixture는 status/rx/action 전부 제공한다.
 */
typedef struct
{
    uint32_t (*status)(void);
    uint32_t (*rx)(void);
    void (*action)(int action, uint32_t value);
} tdc_ut_io_cm3_i2c_ops_t;
extern const tdc_ut_io_cm3_i2c_ops_t *g_tdc_ut_io_cm3_i2c_ops;
/* trace 8개, dump index 1개, dump 32개를 포함한 private RAM 복원. */
void tdc_ut_io_cm3_i2c_exchange(tdc_hal_i2c_driver_t *state, int cache[41]);
/* SPI phase 0=준비 해제, 1=준비 설정, 2=DMA/핀 설정. nonzero는 RAM 경계 사용.
 * 저장 훅은 실제 게인 유효성/모드/슬롯 판정 이후에만 호출된다.
 */
extern int (*g_tdc_ut_io_cm3_spi_hw_hook)(int phase);
extern int (*g_tdc_ut_io_cm3_gain_save_hook)(int slot, int gain_a, int gain_b);
/* 실제 레지스터 조립/상태 해석 함수 아래의 I2C 전송 경계. 0 초기값은 실기다. */
extern int (*g_tdc_ut_io_cm3_isd_bus_hook)(int read, int addr, int *data, int count);
extern int (*g_tdc_ut_io_cm3_pmic_bus_hook)(int read, int addr, int *data, int count);
extern int (*g_tdc_ut_io_cm3_touch_bus_hook)(int read, unsigned char *data, int count);

/* OTA의 슬롯/파일 종류/순서/오류 판정은 실제 함수를 사용한다.
 * 아래 포인터가 NULL이면 기존 FatFs/부트 상태 함수를 그대로 호출한다.
 * RAM 시험 중에는 모든 필드가 설정된 fixture만 사용하며 실제 저장소에 접근하지 않는다.
 * callback 반환값은 파일 경계 응답일 뿐 제품 처리 결과가 아니다.
 */
typedef struct
{
    EN__BOOT_RET (*boot_status)(tdc_boot_status_t *out);
    FIL *(*file_pointer)(void);
    int (*remount)(int drive);
    FRESULT (*stat)(const TCHAR *path, FILINFO *info);
    FRESULT (*mkdir)(const TCHAR *path);
    FRESULT (*open)(FIL *file, const TCHAR *path, BYTE mode);
    FRESULT (*seek)(FIL *file, FSIZE_t offset);
    FRESULT (*close)(FIL *file);
    FRESULT (*write)(FIL *file, const void *data, UINT count, UINT *written);
} tdc_ut_io_cm3_ota_ops_t;
extern const tdc_ut_io_cm3_ota_ops_t *g_tdc_ut_io_cm3_ota_ops;

/* RAM 시험이 사용한 private 상태를 보관/복원한다. 호출자는 공유 RAM/ISR을 정지한다.
 * exchange의 인자는 현재 값을 받으면서 입력 값으로 교환된다. NULL은 사용하지 않는다.
 */
void tdc_ut_io_cm3_ota_exchange(ST__OTA_FILE_WRITE_INFO *state);
int tdc_ut_io_cm3_ble_exchange(int *packet, int capacity);

/* 쓰기/읽기 상태 관측만 한다. 버퍼/IRQ 상태를 바꾸지 않는다. */
void tdc_ut_io_cm3_i2c_snapshot(tdc_hal_i2c_driver_t *out);
int tdc_ut_io_cm3_spi_snapshot(int *out, int capacity);
int tdc_ut_io_cm3_spi_inject(const int *input, int count);
int tdc_ut_io_cm3_spi_restore(const int *input, int count, int empty);
int tdc_ut_io_cm3_fpga_cache(int *values, int restore);
int tdc_ut_io_cm3_i2c_trace(int *values, int capacity);
int tdc_ut_io_cm3_ble_snapshot(int *values, int capacity);
int tdc_ut_io_cm3_mapping_snapshot(int *values, int capacity);
int tdc_ut_io_cm3_ota_snapshot(int *values, int capacity);
#endif
#endif
