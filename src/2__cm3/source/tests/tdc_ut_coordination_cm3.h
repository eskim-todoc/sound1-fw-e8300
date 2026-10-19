#ifndef TDC_UT_COORDINATION_CM3_H
#define TDC_UT_COORDINATION_CM3_H
#if defined(TDC_UNIT_TEST)
/* 시험 소유자는 CM3 루프/ISR/CFX를 먼저 정지한다. 기본 NULL은 제품 경로다.
 * 경계 callback은 준비 입력만 제공한다. 실행 허용과 맵 변경 판단은 실제 함수다.
 * 시험에서는 장치 초기화, 제품 파일 읽기/쓰기, 자극을 실행하지 않는다. */
typedef struct
{
    void (*wait_for_cfx)(void);
    void (*tdc_sys_init)(void);
    void (*post_initialize)(void);
} tdc_ut_cm3_boot_ops_t;
typedef struct
{
    void (*copy_map)(int map, int isd);
    int (*read_fft)(int bands);
    int (*init_fft)(int bands);
} tdc_ut_cm3_map_ops_t;
extern const tdc_ut_cm3_boot_ops_t *g_tdc_ut_cm3_boot_ops;
extern const tdc_ut_cm3_map_ops_t *g_tdc_ut_cm3_map_ops;
void tdc_ut_cm3_boot_sequence(void);
int tdc_ut_cm3_map_flag_exchange(int value);
/* Output: [-999, unit, case, length, values...] actual/expected 별도.
 * capacity 미달/actual/expected/count 겹침은 상태 변경 전에 거부한다.
 * count 겹침이면 count도 쓰지 않는다. shared/private/boundary 포인터 복원.
 * 4는 하드웨어 준비 경계를 대신 받는 순서 시험이며 CFX 실제 조회는 별도다.
 * 6은 실제 tdc_shm_change_program_map_num 함수다. 동일 맵 재적재 불일치를 그대로 남긴다. */
int tdc_ut_cm3_coordination_run(unsigned unit, int *actual, int *expected,
                                unsigned capacity, unsigned *count);
#endif
#endif
