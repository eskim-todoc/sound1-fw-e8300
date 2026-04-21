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
#define TDC_DRV_IQS323_RDY_PIN_CFG_INPUT  (DIO_1X_DRIVE | DIO_LPF_ENABLE  | DIO_NO_PULL | DIO_MODE_GPIO_IN)
#define TDC_DRV_IQS323_MCLR_HOLD_MS       1   /* MCLR LOW 유지 시간 (데이터시트: ≥250ns, 마진 확보) */
#define TDC_DRV_IQS323_BOOT_WAIT_MS       50  /* MCLR 해제 후 IQS323 부팅 대기 */

/* **********************************************************************
 * Register address
 */
#define TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS      0x10
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_SETUP      0x30
#define TDC_DRV_IQS323_REG_ADDR_SENSOR1_SETUP      0x40
#define TDC_DRV_IQS323_REG_ADDR_SENSOR2_SETUP      0x50
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP   0x36
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT   0x38
#define TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP   0x39
#define TDC_DRV_IQS323_REG_ADDR_CH0_TOUCH_SETTINGS 0x62
#define TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL     0xC0
#define TDC_DRV_IQS323_REG_ADDR_EVENTS_ENABLE      0xD3
#define TDC_DRV_IQS323_REG_ADDR_I2C_SETTINGS       0xE0

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

/* **********************************************************************
 * Touch Settings (0x62, 0x72, 0x82) values
 */
#define TDC_DRV_IQS323_TOUCH_HYSTERESIS_80 80
#define TDC_DRV_IQS323_TOUCH_THRESHOLD_80  80

/* **********************************************************************
 * System Control (0xC0) bit values
 */
#define TDC_DRV_IQS323_ACK_RESET    1
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
    uint8_t                        addr;
    tdc_drv_iqs323_lsb_system_status_t lsb;
    tdc_drv_iqs323_msb_system_status_t msb;
} tdc_drv_iqs323_element_system_status_t;

typedef union
{
    tdc_drv_iqs323_element_system_status_t elements;
    uint8_t                            bytes[3];
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
    uint8_t                       addr;
    tdc_drv_iqs323_lsb_sensor_setup_t lsb;
    tdc_drv_iqs323_msb_sensor_setup_t msb;
} tdc_drv_iqs323_element_sensor_setup_t;

typedef union
{
    tdc_drv_iqs323_element_sensor_setup_t elements;
    uint8_t                           bytes[3];
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
    uint8_t                         addr;
    tdc_drv_iqs323_lsb_touch_settings_t lsb;
    tdc_drv_iqs323_msb_touch_settings_t msb;
} tdc_drv_iqs323_element_touch_settings_t;

typedef union
{
    tdc_drv_iqs323_element_touch_settings_t elements;
    uint8_t                             bytes[3];
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
    uint8_t                         addr;
    tdc_drv_iqs323_lsb_system_control_t lsb;
    tdc_drv_iqs323_msb_system_control_t msb;
} tdc_drv_iqs323_element_system_control_t;

typedef union
{
    tdc_drv_iqs323_element_system_control_t elements;
    uint8_t                             bytes[3];
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
    uint8_t                        addr;
    tdc_drv_iqs323_lsb_events_enable_t lsb;
    tdc_drv_iqs323_msb_events_enable_t msb;
} tdc_drv_iqs323_element_events_enable_t;

typedef union
{
    tdc_drv_iqs323_element_events_enable_t elements;
    uint8_t                            bytes[3];
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
    uint8_t                       addr;
    tdc_drv_iqs323_lsb_i2c_settings_t lsb;
    tdc_drv_iqs323_msb_i2c_settings_t msb;
} tdc_drv_iqs323_element_i2c_settings_t;

typedef union
{
    tdc_drv_iqs323_element_i2c_settings_t elements;
    uint8_t                           bytes[3];
} tdc_drv_iqs323_reg_i2c_settings_t;

/* **********************************************************************
 * Public API — 저수준 드라이버 인터페이스
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

#endif /* TDC_DRV_IQS323_H_ */
