/*
 * tdc_drv_iqs323.h
 *
 * IQS323 터치센서 드라이버 헤더.
 */

#ifndef TDC_DRV_IQS323_H_
#define TDC_DRV_IQS323_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <driver_i2c.h>
#include <ci_printf.h>
#include <ci_timer.h>

#include <tdc_touch_config.h>

/* **********************************************************************
 * Common defines
 */
#define TDC_DRV_IQS323_SLAVE_ADDR        0x44
#define TDC_DRV_IQS323_RDY_PIN           DIO16
#define TDC_DRV_IQS323_RDY_WINDOW_OPENED 0
#define TDC_DRV_IQS323_RDY_WINDOW_CLOSED 1

#define TDC_DRV_IQS323_DEFAULT_DELAY_MS             (SystemCoreClock / 1000)  // 1msec
#define TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN  45
#define TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE 20
#define TDC_DRV_IQS323_MAX_WAIT_MS_FOR_ATI_DONE     500

/* MCLR 하드 리셋용 DIO 설정 */
#define TDC_DRV_IQS323_RDY_PIN_CFG_OUTPUT (DIO_2X_DRIVE | DIO_LPF_DISABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)
#define TDC_DRV_IQS323_RDY_PIN_CFG_INPUT  (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define TDC_DRV_IQS323_MCLR_HOLD_MS       1  /* MCLR LOW 유지 시간 (데이터시트: ≥250ns, 마진 확보) */
#define TDC_DRV_IQS323_BOOT_WAIT_MS       50 /* MCLR 해제 후 IQS323 부팅 대기 */

/* **********************************************************************
 * Register address
 */
#define TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS       0x10
#define TDC_DRV_IQS323_REG_ADDR_CH0_FILTERED_COUNTS 0x13 /* CH0 측정 카운트 (16bit) ? 터치 마진 환산용 */
#define TDC_DRV_IQS323_REG_ADDR_CH0_LTA             0x14 /* CH0 장기평균(LTA) (16bit) ? 터치 마진 환산용 */
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_SETUP       0x30
#define TDC_DRV_IQS323_REG_ADDR_SENSOR1_SETUP       0x40
#define TDC_DRV_IQS323_REG_ADDR_SENSOR2_SETUP       0x50
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP   0x36
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT    0x38
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP    0x39
#define TDC_DRV_IQS323_REG_ADDR_SENSOR1_ATI_SETUP   0x46 /* CH1 더미 ATI Disabled용 */
#define TDC_DRV_IQS323_REG_ADDR_CH0_PROX_SETTINGS   0x61
#define TDC_DRV_IQS323_REG_ADDR_CH0_TOUCH_SETTINGS  0x62
#define TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL      0xC0
#define TDC_DRV_IQS323_REG_ADDR_EVENTS_ENABLE       0xD3
#define TDC_DRV_IQS323_REG_ADDR_I2C_SETTINGS        0xE0

/* **********************************************************************
 * System Status (0x10) bit values
 */
#define TDC_DRV_IQS323_CH0_NOT_IN_TOUCH 0
#define TDC_DRV_IQS323_CH0_IN_TOUCH     1

#define TDC_DRV_IQS323_NO_RESET_EVENT 0
#define TDC_DRV_IQS323_RESET_EVENT    1

#define TDC_DRV_IQS323_NO_ATI_ERROR 0
#define TDC_DRV_IQS323_ATI_ERROR    1

#define TDC_DRV_IQS323_NO_ATI_EVENT 0
#define TDC_DRV_IQS323_ATI_EVENT    1

/* **********************************************************************
 * Sensor Setup (0x30, 0x40, 0x50) bit values
 */
#define TDC_DRV_IQS323_CTX0_DISABLE 0
#define TDC_DRV_IQS323_CTX0_ENABLE  1

#define TDC_DRV_IQS323_CHANNEL_DISABLE 0
#define TDC_DRV_IQS323_CHANNEL_ENABLE  1

/* Sensor Setup LSB ? 비활성 채널 CRx 핀 전기적 상태 (enable_channel=0 시 적용)
 * bits[3:2]=CRX1 state, bits[1:0]=CRX0 state  (2-bit per pin: 00=Float, 01=Bias, 10=VSS, 11=VREG)
 * 0x00=둘 다 Floating(기본), 0x0A=둘 다 VSS(GND), 0x05=Bias, 0x0F=VREG */
#define TDC_DRV_IQS323_INACTIVE_RXS_FLOATING 0x00
#define TDC_DRV_IQS323_INACTIVE_RXS_VSS      0x0A /* CRX0+CRX1 모두 VSS */
#define TDC_DRV_IQS323_INACTIVE_RXS_CRX0_VSS 0x02 /* CRX0만 VSS, CRX1 Floating */

/* **********************************************************************
 * Prox Input and Control (0x33/0x43/0x53) ? 수신 핀 선택
 * reset value 0x01CF (reserved bit7=1, bit1-0=11 강제 ? 임의 변경 시 IC 먹통).
 * CalCap 더미 채널에서는 이 레지스터를 쓰지 않고 reset 상태를 유지한다.
 */
#define TDC_DRV_IQS323_REG_ADDR_SENSOR1_PROX_INPUT 0x43

/* **********************************************************************
 * Pattern Definitions (0x34/0x44/0x54) ? CalCap 크기·Inactive Rxs
 * reset value 0x030A
 */
#define TDC_DRV_IQS323_REG_ADDR_SENSOR1_PATTERN_DEF 0x44
/* CalCap 더미 채널용 값 (reset 0x030A 기준 산출):
 *   LSB 0x2A = CalCap size(bit7-4)=2 → 1.0pF + Inactive Rxs(bit3-0)=VSS(0x0A)
 *   MSB 0x03 = Wav Pattern 0 (self-cap) 유지
 * 상세: docs/참고/touch/IQS323-CalCap-더미채널.md */
#define TDC_DRV_IQS323_PATTERN_CALCAP_SIZE_1PF_LSB 0x2A
#define TDC_DRV_IQS323_PATTERN_DEF_MSB             0x03

/* CH1 더미 ATI Mode=Disabled (0x46 reset 0x040C → bits[2:0]=000):
 * CalCap 부하에서 auto-ATI가 수렴 못 해 전역 ATI_ERROR(System Status bit6)가 SET되면
 * tdc_touch_get_state()가 CAL_ERROR로 단락되어 CH0 터치 판정이 막힌다. CH0(0x36)와 동일하게
 * ATI를 꺼서 §5.11 "ATI 실행 후 error check"를 스킵시킨다. 상세: IQS323-CalCap-더미채널.md */
#define TDC_DRV_IQS323_DUMMY_ATI_SETUP_LSB 0x08
#define TDC_DRV_IQS323_DUMMY_ATI_SETUP_MSB 0x04

/* **********************************************************************
 * Channel Setup (0x60/0x70/0x80) ? 채널 동작 모드
 */
#define TDC_DRV_IQS323_REG_ADDR_CHANNEL1_SETUP 0x70
/* bits[3:0]: Channel Mode */
#define TDC_DRV_IQS323_CH_MODE_INDEPENDENT 0x00 /* 독립 채널 (기본) */
#define TDC_DRV_IQS323_CH_MODE_REFERENCE   0x02 /* 환경 기준값 채널 ? LTA 드리프트 보정 */

/* **********************************************************************
 * Touch Settings (0x62, 0x72, 0x82) values
 */
#define TDC_DRV_IQS323_TOUCH_HYSTERESIS_80 80
#define TDC_DRV_IQS323_TOUCH_THRESHOLD_80  80

/* 노말 모드 운용값 */
#define TDC_DRV_IQS323_TOUCH_THRESHOLD  100
#define TDC_DRV_IQS323_TOUCH_HYSTERESIS 80

/* 절전 모드 운용값 ? 주변 소자 OFF + 저속 클럭 환경에서 counts delta 감소 대응 */
#define TDC_DRV_IQS323_SLEEP_TOUCH_THRESHOLD  100
#define TDC_DRV_IQS323_SLEEP_TOUCH_HYSTERESIS 80

/* **********************************************************************
 * Prox Settings (0x61, 0x71, 0x81) values
 * Prox Threshold 를 노터치 baseline delta 보다 크게 설정 → 노터치가 prox 로
 * 오판되어 LTA 가 freeze 되는 것을 방지한다. (reset 0 → 항상 prox → baseline drift)
 * prox event 는 events_enable 에서 비활성 — prox 판정은 LTA freeze 제어 용도로만 쓰인다.
 */
#define TDC_DRV_IQS323_PROX_THRESHOLD        120
#define TDC_DRV_IQS323_SLEEP_PROX_THRESHOLD  100

/* **********************************************************************
 * RTT 런타임 터치 파라미터 튜닝 (개발용)
 * 0: 비활성(기본), 1: 활성 — RTT UI로 MULT·COMP·THRESHOLD·HYSTERESIS 조정 가능.
 * 활성 시 부팅 기본 threshold/hysteresis=255 (터치 무반응 → set+apply 후 활성).
 */
#define TDC_TOUCH_RTT_TUNING 0

/* **********************************************************************
 * System Control (0xC0) bit values
 */
#define TDC_DRV_IQS323_ACK_RESET      1
#define TDC_DRV_IQS323_TRIGGER_RE_ATI 1

/* **********************************************************************
 * Events Enable (0xD3) bit values
 */
#define TDC_DRV_IQS323_EVENT_DISABLE 0
#define TDC_DRV_IQS323_EVENT_ENABLE  1

/* **********************************************************************
 * System Status (0x10) register
 */
typedef struct
{
    uint8_t ch0_prox           : 1;  // [8]
    uint8_t ch0_touch          : 1;  // [9]
    uint8_t ch1_prox           : 1;  // [10]
    uint8_t ch1_touch          : 1;  // [11]
    uint8_t ch2_prox           : 1;  // [12]
    uint8_t ch2_touch          : 1;  // [13]
    uint8_t current_power_mode : 2;  // [14:15]
} tdc_drv_iqs323_msb_system_status_t;

typedef struct
{
    uint8_t prox_event   : 1;  // [0]
    uint8_t touch_event  : 1;  // [1]
    uint8_t slider_event : 1;  // [2]
    uint8_t power_event  : 1;  // [3]
    uint8_t ati_event    : 1;  // [4]
    uint8_t ati_active   : 1;  // [5]
    uint8_t ati_error    : 1;  // [6]
    uint8_t reset_event  : 1;  // [7]
} tdc_drv_iqs323_lsb_system_status_t;

typedef struct
{
    uint8_t                            addr;
    tdc_drv_iqs323_lsb_system_status_t lsb;
    tdc_drv_iqs323_msb_system_status_t msb;
} tdc_drv_iqs323_element_system_status_t;

typedef union
{
    tdc_drv_iqs323_element_system_status_t elements;
    uint8_t                                bytes[3];
} tdc_drv_iqs323_reg_system_status_t;

/* **********************************************************************
 * Sensor Setup (0x30, 0x40, 0x50) register
 */
typedef struct
{
    uint8_t ctx0           : 1;  // [8]
    uint8_t ctx1           : 1;  // [9]
    uint8_t ctx2           : 1;  // [10]
    uint8_t tx_a           : 1;  // [11]
    uint8_t reserved_bit12 : 1;  // [12]
    uint8_t cal_cap_tx     : 1;  // [13]
    uint8_t cal_cap_rx     : 1;  // [14]
    uint8_t reserved_bit15 : 1;  // [15]
} tdc_drv_iqs323_msb_sensor_setup_t;

typedef struct
{
    uint8_t enable_channel             : 1;  // [0]
    uint8_t linearise_counts           : 1;  // [1]
    uint8_t dual_direction             : 1;  // [2]
    uint8_t invert                     : 1;  // [3]
    uint8_t vbias                      : 1;  // [4]
    uint8_t fosc_tx_frequency          : 1;  // [5]
    uint8_t release_movement_ui_enable : 1;  // [6]
    uint8_t reserved_bit7              : 1;  // [7]
} tdc_drv_iqs323_lsb_sensor_setup_t;

typedef struct
{
    uint8_t                           addr;
    tdc_drv_iqs323_lsb_sensor_setup_t lsb;
    tdc_drv_iqs323_msb_sensor_setup_t msb;
} tdc_drv_iqs323_element_sensor_setup_t;

typedef union
{
    tdc_drv_iqs323_element_sensor_setup_t elements;
    uint8_t                               bytes[3];
} tdc_drv_iqs323_reg_sensor_setup_t;

/* **********************************************************************
 * Touch Settings (0x62, 0x72, 0x82) register
 */
typedef struct
{
    uint8_t touch_hysteresis : 8;  // [8:15]
} tdc_drv_iqs323_msb_touch_settings_t;

typedef struct
{
    uint8_t touch_threshold : 8;  // [0:7]
} tdc_drv_iqs323_lsb_touch_settings_t;

typedef struct
{
    uint8_t                             addr;
    tdc_drv_iqs323_lsb_touch_settings_t lsb;
    tdc_drv_iqs323_msb_touch_settings_t msb;
} tdc_drv_iqs323_element_touch_settings_t;

typedef union
{
    tdc_drv_iqs323_element_touch_settings_t elements;
    uint8_t                                 bytes[3];
} tdc_drv_iqs323_reg_touch_settings_t;

/* **********************************************************************
 * System Control (0xC0) register
 */
typedef struct
{
    uint8_t ch0_timeout_disable  : 1;  // [8]
    uint8_t ch1_timeout_disable  : 1;  // [9]
    uint8_t ch2_timeout_disable  : 1;  // [10]
    uint8_t reserved_bit11_to_15 : 5;  // [11:15]
} tdc_drv_iqs323_msb_system_control_t;

typedef struct
{
    uint8_t ack_reset      : 1;  // [0]
    uint8_t soft_reset     : 1;  // [1]
    uint8_t re_ati         : 1;  // [2]
    uint8_t reseed         : 1;  // [3]
    uint8_t power_mode     : 3;  // [4:6]
    uint8_t interface_type : 1;  // [7]
} tdc_drv_iqs323_lsb_system_control_t;

typedef struct
{
    uint8_t                             addr;
    tdc_drv_iqs323_lsb_system_control_t lsb;
    tdc_drv_iqs323_msb_system_control_t msb;
} tdc_drv_iqs323_element_system_control_t;

typedef union
{
    tdc_drv_iqs323_element_system_control_t elements;
    uint8_t                                 bytes[3];
} tdc_drv_iqs323_reg_system_control_t;

/* **********************************************************************
 * Events Enable (0xD3) register
 */
typedef struct
{
    uint8_t reserved_bit8_to_15 : 8;  // [8:15]
} tdc_drv_iqs323_msb_events_enable_t;

typedef struct
{
    uint8_t prox_event    : 1;  // [0]
    uint8_t touch_event   : 1;  // [1]
    uint8_t slider_event  : 1;  // [2]
    uint8_t power_event   : 1;  // [3]
    uint8_t ati_event     : 1;  // [4]
    uint8_t reserved_bit5 : 1;  // [5]
    uint8_t ati_error     : 1;  // [6]
    uint8_t reserved_bit7 : 1;  // [7]
} tdc_drv_iqs323_lsb_events_enable_t;

typedef struct
{
    uint8_t                            addr;
    tdc_drv_iqs323_lsb_events_enable_t lsb;
    tdc_drv_iqs323_msb_events_enable_t msb;
} tdc_drv_iqs323_element_events_enable_t;

typedef union
{
    tdc_drv_iqs323_element_events_enable_t elements;
    uint8_t                                bytes[3];
} tdc_drv_iqs323_reg_events_enable_t;

/* **********************************************************************
 * I2C Settings (0xE0) register
 */
typedef struct
{
    uint8_t reserved_bit8_to_15 : 8;  // [8:15]
} tdc_drv_iqs323_msb_i2c_settings_t;

typedef struct
{
    uint8_t stop_bit_disable   : 1;  // [0]
    uint8_t rw_check_disable   : 1;  // [1]
    uint8_t reserved_bit2_to_7 : 6;  // [2:7]
} tdc_drv_iqs323_lsb_i2c_settings_t;

typedef struct
{
    uint8_t                           addr;
    tdc_drv_iqs323_lsb_i2c_settings_t lsb;
    tdc_drv_iqs323_msb_i2c_settings_t msb;
} tdc_drv_iqs323_element_i2c_settings_t;

typedef union
{
    tdc_drv_iqs323_element_i2c_settings_t elements;
    uint8_t                               bytes[3];
} tdc_drv_iqs323_reg_i2c_settings_t;

/* **********************************************************************
 * Public API ? 저수준 드라이버 인터페이스
 *
 * 기능 레이어(tdc_touch) 에서 상태머신 단계를 조립할 때 사용.
 * 상태머신·롱터치 판정 등 UX 로직은 tdc_touch 에 위치.
 */

/* MCLR 하드 리셋. ~55ms 블로킹. IQS323 POR 발생 → Auto-ATI 시작. */
void tdc_drv_iqs323_mclr_reset(void);

/* Auto-ATI 완료 여부 1회 read (논블로킹).
 * 반환: true = 완료 / false = 아직 진행 중 */
bool tdc_drv_iqs323_is_auto_ati_done(void);

/* ACK + Sensor/Touch/Events 설정 + 고정 ATI 보상값 + Reseed 일괄 적용.
 * Auto-ATI 완료 후 호출해야 한다. 덤프 모드 시에는 RE-ATI 후 덤프 출력. */
void tdc_drv_iqs323_apply_settings(void);

/* 현재 터치 여부 + 드라이버 에러 여부 1회 read.
 * 반환: true = read 성공. *p_pressed 에 터치 상태,
 *       *p_ati_error 에 ATI_ERROR 플래그. */
bool tdc_drv_iqs323_read_status(bool *p_pressed, bool *p_ati_error);

/* 현재 CH0 터치 마진을 threshold '계수' 단위로 역환산해 1회 read·로그.
 * self-cap 터치 조건 (LTA-Counts) > Touch Threshold(절대=계수×LTA/256) 에서
 * 양변을 ×256/LTA 하여, 현재 신호를 설정 threshold 계수와 같은 단위로 비교한다:
 *   current_coeff = (LTA-Counts)×256/LTA   (값이 클수록 터치 근접, threshold 계수 초과 시 터치)
 * 방전 전 신선한 LTA/Counts 로 호출해야 마진이 정확하다.
 * p_threshold_coeff / p_current_coeff 는 NULL 허용 (로그만 필요할 때).
 * 반환: true = read 성공. */
bool tdc_drv_iqs323_read_touch_margin(uint8_t *p_threshold_coeff, uint16_t *p_current_coeff);

/* **********************************************************************
 * ATI Calibration Mode ? TDC_TOUCH_ATI_CALIB_MODE 빌드 전용
 *
 * 조립 완제품에서 디버그 포트 없이 최적 ATI 보상값 후보를 LED 로 식별하는
 * 1회성 개발 도구. 프로덕션 빌드에 포함하지 않는다.
 */
#if TDC_TOUCH_ATI_CALIB_MODE

#define TDC_DRV_IQS323_CALIB_CANDIDATE_COUNT 10

typedef struct
{
    uint8_t mult_lsb;
    uint8_t mult_msb;
    uint8_t comp_lsb;
    uint8_t comp_msb;
} tdc_drv_iqs323_calib_candidate_t;

extern const tdc_drv_iqs323_calib_candidate_t tdc_drv_iqs323_calib_candidates[TDC_DRV_IQS323_CALIB_CANDIDATE_COUNT];

/* auto-ATI 완료 직후 호출. MULT/COMP 를 I2C 로 읽어 후보 테이블과 비교.
 * 반환: 최근접 후보 인덱스 (1~10). 0=I2C read 실패. */
uint8_t tdc_drv_iqs323_calib_find_candidate(void);

/* auto-ATI 완료 직후 MULT·COMP 16비트 값을 직접 읽어 반환.
 * 반환: true=성공. *p_mult=(MSB<<8)|LSB, *p_comp=(MSB<<8)|LSB. */
bool tdc_drv_iqs323_calib_read_ati(uint16_t *p_mult, uint16_t *p_comp);

/* RE-ATI 트리거 후 완료까지 블로킹 대기 (타임아웃 500ms).
 * 반환: true=정상 완료 / false=트리거 실패 또는 타임아웃. */
bool tdc_drv_iqs323_calib_re_ati(void);

#endif /* TDC_TOUCH_ATI_CALIB_MODE */

/* **********************************************************************
 * Sleep Measure Mode ? TDC_TOUCH_SLEEP_MEASURE_MODE 빌드 전용
 *
 * func_sleep 환경(주변장치 OFF, 저속 클럭)에서 re-ATI 후 ATI 레지스터를
 * RTT로 반복 출력하는 1회성 측정 도구.
 */
#if TDC_TOUCH_SLEEP_MEASURE_MODE

/* re-ATI 트리거 → 완료 대기 → ATI_SETUP/MULT/COMP 레지스터를 RTT로 출력. */
void tdc_drv_iqs323_sleep_measure_dump(void);

#endif /* TDC_TOUCH_SLEEP_MEASURE_MODE */

/* LTA 를 현재 counts 로 강제 재설정. */
void tdc_drv_iqs323_reseed(void);

/* ESD 방전 ? CH0 일시 비활성(CRX0=VSS) 후 복원. 누적 정전기 제거.
 * 반환: true = 성공, false = I2C 쓰기 실패.
 * 호출: tdc_touch_get_state() 에서 read_status() 직전 (TDC_TOUCH_CRX0_DISCHARGE_ENABLE=1 시). */
bool tdc_drv_iqs323_discharge_crx0(void);

/* 절전 레지스터 사전 적용 ? ci_power_sleep() 전 호출. THRESHOLD/HYSTERESIS/MULT/COMP/CH_TIMEOUT 기록.
 * RESEED 및 터치 해제 대기는 ci_power_sleep() 이후 호출자 책임.
 * 반환: true = 성공, false = I2C 쓰기 실패. */
bool tdc_drv_iqs323_apply_sleep_settings(void);

void tdc_set_iqs323_in_ulp_mode(void);
void tdc_clear_iqs323_in_ulp_mode(void);
bool tdc_is_iqs323_in_ulp_mode(void);

#if TDC_TOUCH_RTT_TUNING
typedef struct {
    uint8_t mult_lsb;
    uint8_t mult_msb;
    uint8_t comp_lsb;
    uint8_t comp_msb;
    uint8_t threshold;
    uint8_t hysteresis;
    bool    ati_valid;
} tdc_iqs323_tuning_t;

void tdc_drv_iqs323_set_normal_tuning(const tdc_iqs323_tuning_t *p);
void tdc_drv_iqs323_set_sleep_tuning(const tdc_iqs323_tuning_t *p);
void tdc_drv_iqs323_apply_tuning(void);
void tdc_drv_iqs323_clear_tuning(void);
void tdc_drv_iqs323_show_tuning(void);
#endif /* TDC_TOUCH_RTT_TUNING */

#endif /* TDC_DRV_IQS323_H_ */
