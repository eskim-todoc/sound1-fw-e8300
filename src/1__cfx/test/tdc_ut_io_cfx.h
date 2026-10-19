#ifndef TDC_UT_IO_CFX_H
#define TDC_UT_IO_CFX_H

#if defined(TDC_UNIT_TEST)
#include <hw.h>
#define TDC_UT_CFX_IO_CAPACITY 768
#define TDC_UT_IO_UNSUPPORTED (-1)
#define TDC_UT_IO_BUSY (-2)
#define TDC_UT_IO_OVERFLOW (-3)
#define TDC_UT_IO_TARGET_REQUIRED (-4)
#define TDC_UT_IO_TIMEOUT (-5)
#define TDC_UT_IO_WINDOW_NOT_READY (-6)

/* I2S_isStreaming()의 bool 대신 RESET 포함 원시 상태를 복원할 때 사용. */
int tdc_ut_i2s_raw_state(void);

/* 설정: 모든 오디오/PCM 생산자와 CM3 공유값 갱신을 멈춘 뒤 명시적으로 호출.
 * 입력: 지원 CFX ID 하나. 출력: actual/expected의 독립 flat 레코드.
 * 복원: 수정한 제품 상태를 정적 백업으로 복원한다. done은 PASS가 아니다.
 * release는 RTT가 모두 읽은 뒤만 호출. 미지원/오류는 음수이며 실행을 숨기지 않는다.
 * CFX-011 비양수 볼륨/유효 복귀와 CFX-033 작은 대역 수를 포함한다.
 * CFX-015 완료 플래그 및 CFX-033 sparse CIS의 알려진 불일치를 그대로 기록한다.
 * 맵의 ADC/DIO 재설정, 계산 완료의 HEAR RAM 접근, 전략 dispatch는 PC fixture 전용.
 * 기존 타깃 훅으로 격리되지 않는 접근을 이 진입점에서 자동 실행하지 않는다.
 */
int tdc_ut_cfx_io_run_unit(int unit);
int tdc_ut_cfx_io_release(void);
extern int _XMEM g_tdc_ut_cfx_io_actual[TDC_UT_CFX_IO_CAPACITY];
extern int _XMEM g_tdc_ut_cfx_io_expected[TDC_UT_CFX_IO_CAPACITY];
extern int g_tdc_ut_cfx_io_count, g_tdc_ut_cfx_io_failed;
extern int g_tdc_ut_cfx_io_overflow;
extern volatile int g_tdc_ut_cfx_io_done;

/* 0 기본값은 기존 HW 접근. 시험 중만 RAM FIFO/FS 경계로 전환한다. */
extern int g_tdc_ut_io_override_enabled;
extern int _XMEM g_tdc_ut_io_dmic_fifo[2][16];
extern int _IOMEM g_tdc_ut_io_fs_band_count;
extern int _IOMEM g_tdc_ut_io_fs_bins[256];
int _XMEM *tdc_ut_io_pcm_top(void);
void tdc_ut_io_pcm_write(int _XMEM *p_word, int value);
void tdc_ut_io_fft_write(int value);

/* HEAR는 실기 전용. 준비된 HEAR/IRQ/512점 Hanning 창을 사용한다.
 * hardware_ready=1은 호출자가 생산자를 정지하고 준비를 확인했다는 명시적 값.
 * start는 리셋/초기화하지 않는다. poll은 한 번 확인하고 반환한다.
 * TIMEOUT 후에도 pending을 유지한다. 실제 완료 전 메모리 복원/release 금지.
 * CMD0의 vAbsolute 중간값은 분석 대상으로 구분한다. Event0 후에는 MaxElement가
 * word0을 덮어쓴 최종값을 비교한다. microcode 실행 순서는 변경하지 않는다.
 */
int tdc_ut_hear_start(int unit, int vector, int hardware_ready);
int tdc_ut_hear_poll(void);
int tdc_ut_hear_release(void);
/* CFX와 HEAR는 동시에 실행하지 않으며 결과 저장소를 공유한다.
 * 브리지는 core 값만 구분한다. 기존 CM3 공유 ABI는 변경하지 않는다. */
#define g_tdc_ut_hear_actual g_tdc_ut_cfx_io_actual
#define g_tdc_ut_hear_expected g_tdc_ut_cfx_io_expected
extern int _XMEM g_tdc_ut_hear_vmag[256];
extern int _XMEM g_tdc_ut_hear_fft[512];
extern int g_tdc_ut_hear_fft_count, g_tdc_ut_hear_block_exp;
/* 실제 완료 후 OBSERVED 값. 독립 DFT의 허용 오차 비교와 함께 보존한다.
 * count=0은 캡처 없음. done 전에는 읽지 않고 drain/release 전에는 변경하지 않는다. */
extern int g_tdc_ut_hear_vmag_count;
extern int g_tdc_ut_hear_count, g_tdc_ut_hear_failed, g_tdc_ut_hear_overflow;
extern volatile int g_tdc_ut_hear_done, g_tdc_ut_hear_pending;
extern int g_tdc_ut_hear_target_needed, g_tdc_ut_hear_timed_out;
#define TDC_UT_HEAR_POLL_LIMIT 100000

#define TDC_UT_PCM_TOP() tdc_ut_io_pcm_top()
#define TDC_UT_PCM_WRITE(p, value) tdc_ut_io_pcm_write((p), (value))
#define TDC_UT_FFT_WRITE(value) tdc_ut_io_fft_write(value)
#else
#define TDC_UT_PCM_TOP() ((int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING)
#define TDC_UT_PCM_WRITE(p, value) (*(p) = (value))
#define TDC_UT_FFT_WRITE(value) (D_FIFO_A1_0->ACCESS = (value))
#endif

#endif
