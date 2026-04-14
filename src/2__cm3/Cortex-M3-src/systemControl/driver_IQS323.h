/*
 * driver_IQS323.h
 */

#ifndef DRIVER_IQS323_H_
#define DRIVER_IQS323_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>
#include <driver_i2c.h>
#include <ci_printf.h>
#include <ci_timer.h>

/* **********************************************************************
 * FSM
 */
#define IQS323_FSM_RESET           0
#define IQS323_FSM_INIT            1
#define IQS323_FSM_WAIT_ANY_EVENT  2
#define IQS323_FSM_TOUCH_TRIGGERED 3
#define IQS323_FSM_KEEP_TOUCHING   4
#define IQS323_FSM_CONFIRM_TOUCH   5
#define IQS323_FSM_WAIT_RELEASE    6
#define IQS323_FSM_KEEP_RELEASING  7
#define IQS323_FSM_CONFIRM_RELEASE 8
#define IQS323_FSM_ATI_ERROR       9

/* **********************************************************************
 * Common defines
 */
#define IQS323_SLAVE_ADDR        0x44
#define IQS323_RDY_PIN           DIO16
#define IQS323_RDY_WINDOW_OPENED 0
#define IQS323_RDY_WINDOW_CLOSED 1

#define IQS323_DEFAULT_DELAY_MS             (SystemCoreClock / 1000)  // 1msec
#define IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN  45
#define IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE 20
#define IQS323_MAX_WAIT_MS_FOR_ATI_DONE     500

#define IQS323_TOUCH_STATE_RESET     0
#define IQS323_TOUCH_STATE_TOUCH     1
#define IQS323_TOUCH_STATE_NOT_TOUCH 2
#define IQS323_TOUCH_STATE_ATI_ERROR 3

/* **********************************************************************
 * Register address
 */
#define IQS323_REG_ADDR_SYSTEM_STATUS      0x10
#define IQS323_REG_ADDR_SENSOR0_SETUP      0x30
#define IQS323_REG_ADDR_SENSOR1_SETUP      0x40
#define IQS323_REG_ADDR_SENSOR2_SETUP      0x50
#define IQS323_REG_ADDR_CH0_TOUCH_SETTINGS 0x62
#define IQS323_REG_ADDR_SYSTEM_CONTROL     0xC0
#define IQS323_REG_ADDR_EVENTS_ENABLE      0xD3
#define IQS323_REG_ADDR_I2C_SETTINGS       0xE0

/* **********************************************************************
 * System Status (0x10) parameters (reset default : -)
 */
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CURRENT_POWER_MODE_NORMAL 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CURRENT_POWER_MODE_LP     1
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CURRENT_POWER_MODE_ULP    2
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CURRENT_POWER_MODE_HALT   3

#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH2_NOT_IN_TOUCH 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH2_IN_TOUCH     1

#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH2_NOT_IN_PROX 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH2_IN_PROX     1

#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH1_NOT_IN_TOUCH 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH1_IN_TOUCH     1

#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH1_NOT_IN_PROX 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH1_IN_PROX     1

#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH0_NOT_IN_TOUCH 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH0_IN_TOUCH     1

#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH0_NOT_IN_PROX 0
#define IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH0_IN_PROX     1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_RESET_EVENT 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_RESET_EVENT    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_ATI_ERROR 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_ATI_ERROR    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_ATI_ACTIVE 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_ATI_ACTIVE    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_ATI_EVENT 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_ATI_EVENT    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_POWER_EVENT 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_POWER_EVENT    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_SLIDER_EVENT 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_SLIDER_EVENT    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_TOUCH_EVENT 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_TOUCH_EVENT    1

#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_PROX_EVENT 0
#define IQS323_REG_VAL_SYSTEM_STATUS_LSB_PROX_EVENT    1

/* **********************************************************************
 * Sensor Setup (0x30, 0x40, 0x50) setting parameters (reset default : 0x0101)
 */
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_RESERVED_BIT15 0  // reset default

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CAL_CAP_RX_NOT_SELECT 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CAL_CAP_RX_SELECT     1

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CAL_CAP_TX_NOT_SELECT 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CAL_CAP_TX_SELECT     1

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_RESERVED_BIT12 0  // reset default

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_TX_A_DISABLE 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_TX_A_ENABLE  1

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX2_DISABLE 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX2_ENABLE  1

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX1_DISABLE 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX1_ENABLE  1

#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX0_DISABLE 0
#define IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX0_ENABLE  1  // reset default

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_RESERVED_BIT7 0

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_RELEASE_MOVEMENT_UI_DISABLE 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_RELEASE_MOVEMENT_UI_ENABLE  1

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_FOSC_TX_FREQ_PERIOD_AND_FRAC 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_FOSC_TX_FREQ_14MHZ           1

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_VBIAS_DISABLE 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_VBIAS_ON_CX2  1

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_DO_NOT_INVERT_CH_LOGIC 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_INVERT_CH_LOGIC        1

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_SINGLE_DIRECTION_THRESHOLD 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_DUAL_DIRECTION_THRESHOLD   1

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_DO_NOT_LINEARISE_COUNTS 0  // reset default
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_LINEARISE_COUNTS        1

#define IQS323_REG_VAL_SENSOR_SETUP_LSB_CHANNEL_DISABLE 0
#define IQS323_REG_VAL_SENSOR_SETUP_LSB_CHANNEL_ENABLE  1  // reset default

/* **********************************************************************
 * Touch Settings (0x62, 0x72, 0x82) setting parameters (reset default : 0x0000)
 */
#define IQS323_REG_VAL_TOUCH_SETTINGS_MSB_HYSTERESIS    0  // reset default = 0
#define IQS323_REG_VAL_TOUCH_SETTINGS_MSB_HYSTERESIS_80 80

#define IQS323_REG_VAL_TOUCH_SETTINGS_LSB_THRESHOLD    0  // reset default = 0
#define IQS323_REG_VAL_TOUCH_SETTINGS_LSB_THRESHOLD_80 80

/* **********************************************************************
 * System Control (0xC0) setting parameters (reset default : 0x0000)
 */
#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_RESERVED_BIT11_TO_15 0  // reset default

#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_CH2_TIMEOUT_ENABLE  0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_CH2_TIMEOUT_DISABLE 1

#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_CH1_TIMEOUT_ENABLE  0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_CH1_TIMEOUT_DISABLE 1

#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_CH0_TIMEOUT_ENABLE  0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_MSB_CH0_TIMEOUT_DISABLE 1

#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_INTERFACE_SELECTION_I2C_STREAMING 0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_INTERFACE_SELECTION_I2C_EVENT     1

#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_POWER_MODE_NORMAL      0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_POWER_MODE_LP          1
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_POWER_MODE_ULP         2
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_POWER_MODE_HALT        3
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_POWER_MODE_AUTO        4
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_POWER_MODE_AUTO_NO_ULP 5

#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_NO_RESEED      0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_TRIGGER_RESEED 1

#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_NO_RE_ATI      0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_TRIGGER_RE_ATI 1

#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_NO_SOFT_RESET      0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_TRIGGER_SOFT_RESET 1

#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_NO_ACK_RESET 0  // reset default
#define IQS323_REG_VAL_SYSTEM_CONTROL_LSB_ACK_RESET    1

/* **********************************************************************
 * Events Enable (0xD3) setting parameters (reset default : 0x0000)
 */
#define IQS323_REG_VAL_EVENTS_ENABLE_MSB_RESERVED_BIT8_TO_15 0  // reset default

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_RESERVED_BIT7 0  // reset default

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_ATI_ERROR_DISABLE 0  // reset default
#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_ATI_ERROR_ENABLE  1

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_RESERVED_BIT5 0  // reset default

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_ATI_EVENT_DISABLE 0  // reset default
#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_ATI_EVENT_ENABLE  1

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_POWER_EVENT_DISABLE 0  // reset default
#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_POWER_EVENT_ENABLE  1

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_SLIDER_EVENT_DISABLE 0  // reset default
#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_SLIDER_EVENT_ENABLE  1

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_TOUCH_EVENT_DISABLE 0  // reset default
#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_TOUCH_EVENT_ENABLE  1

#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_PROX_EVENT_DISABLE 0  // reset default
#define IQS323_REG_VAL_EVENTS_ENABLE_LSB_PORX_EVENT_ENABLE  1

/* **********************************************************************
 * I2C Settings (0xE0) setting parameters (reset default : 0x0000)
 */
#define IQS323_REG_VAL_I2C_SETTINGS_MSB_RESERVED_BIT8_TO_15 0  // reset default

#define IQS323_REG_VAL_I2C_SETTINGS_LSB_RESERVED_BIT2_TO_7 0  // reset default

#define IQS323_REG_VAL_I2C_SETTINGS_LSB_RW_CHECK_ENABLE  0  // reset default
#define IQS323_REG_VAL_I2C_SETTINGS_LSB_RW_CHECK_DISABLE 1

#define IQS323_REG_VAL_I2C_SETTINGS_LSB_STOP_BIT_ENABLE  0  // reset default
#define IQS323_REG_VAL_I2C_SETTINGS_LSB_STOP_BIT_DISABLE 1

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
} IQS323_MSB_SYSTEM_STATUS_T;

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
} IQS323_LSB_SYSTEM_STATUS_T;

typedef struct
{
    uint8_t                    addr;
    IQS323_LSB_SYSTEM_STATUS_T lsb;
    IQS323_MSB_SYSTEM_STATUS_T msb;
} IQS323_ELEMENT_SYSTEM_STATUS_T;

typedef union
{
    IQS323_ELEMENT_SYSTEM_STATUS_T elements;
    uint8_t                        bytes[3];
} IQS323_REG_SYSTEM_STATUS_T;

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
} IQS323_MSB_SENSOR_SETUP_T;

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
} IQS323_LSB_SENSOR_SETUP_T;

typedef struct
{
    uint8_t                   addr;
    IQS323_LSB_SENSOR_SETUP_T lsb;
    IQS323_MSB_SENSOR_SETUP_T msb;
} IQS323_ELEMENT_SENSOR_SETUP_T;

typedef union
{
    IQS323_ELEMENT_SENSOR_SETUP_T elements;
    uint8_t                       bytes[3];
} IQS323_REG_SENSOR_SETUP_T;

/* **********************************************************************
 * Touch Settings (0x62, 0x72, 0x82) register
 */
typedef struct
{
    uint8_t touch_hysteresis : 8;  // [8:15]
} IQS323_MSB_TOUCH_SETTINGS_T;

typedef struct
{
    uint8_t touch_threshold : 8;  // [0:7]
} IQS323_LSB_TOUCH_SETTINGS_T;

typedef struct
{
    uint8_t                     addr;
    IQS323_LSB_TOUCH_SETTINGS_T lsb;
    IQS323_MSB_TOUCH_SETTINGS_T msb;
} IQS323_ELEMENT_TOUCH_SETTINGS_T;

typedef union
{
    IQS323_ELEMENT_TOUCH_SETTINGS_T elements;
    uint8_t                         bytes[3];
} IQS323_REG_TOUCH_SETTINGS_T;

/* **********************************************************************
 * System Control (0xC0) register
 */
typedef struct
{
    uint8_t ch0_timeout_disable  : 1;  // [8]
    uint8_t ch1_timeout_disable  : 1;  // [9]
    uint8_t ch2_timeout_disable  : 1;  // [10]
    uint8_t reserved_bit11_to_15 : 5;  // [11:15]
} IQS323_MSB_SYSTEM_CONTROL_T;

typedef struct
{
    uint8_t ack_reset      : 1;  // [0]
    uint8_t soft_reset     : 1;  // [1]
    uint8_t re_ati         : 1;  // [2]
    uint8_t reseed         : 1;  // [3]
    uint8_t power_mode     : 3;  // [4:6]
    uint8_t interface_type : 1;  // [7]
} IQS323_LSB_SYSTEM_CONTROL_T;

typedef struct
{
    uint8_t                     addr;
    IQS323_LSB_SYSTEM_CONTROL_T lsb;
    IQS323_MSB_SYSTEM_CONTROL_T msb;
} IQS323_ELEMENT_SYSTEM_CONTROL_T;

typedef union
{
    IQS323_ELEMENT_SYSTEM_CONTROL_T elements;
    uint8_t                         bytes[3];
} IQS323_REG_SYSTEM_CONTROL_T;

/* **********************************************************************
 * Events Enable (0xD3) register
 */
typedef struct
{
    uint8_t reserved_bit8_to_15 : 8;  // [8:15]
} IQS323_MSB_EVENTS_ENABLE_T;

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
} IQS323_LSB_EVENTS_ENABLE_T;

typedef struct
{
    uint8_t                    addr;
    IQS323_LSB_EVENTS_ENABLE_T lsb;
    IQS323_MSB_EVENTS_ENABLE_T msb;
} IQS323_ELEMENT_EVENTS_ENABLE_T;

typedef union
{
    IQS323_ELEMENT_EVENTS_ENABLE_T elements;
    uint8_t                        bytes[3];
} IQS323_REG_EVENTS_ENABLE_T;

/* **********************************************************************
 * I2C Settings (0xE0) register
 */
typedef struct
{
    uint8_t reserved_bit8_to_15 : 8;  // [8:15]
} IQS323_MSB_I2C_SETTINGS_T;

typedef struct
{
    uint8_t stop_bit_disable   : 1;  // [0]
    uint8_t rw_check_disable   : 1;  // [1]
    uint8_t reserved_bit2_to_7 : 6;  // [2:7]
} IQS323_LSB_I2C_SETTINGS_T;

typedef struct
{
    uint8_t                   addr;
    IQS323_LSB_I2C_SETTINGS_T lsb;
    IQS323_MSB_I2C_SETTINGS_T msb;
} IQS323_ELEMENT_I2C_SETTINGS_T;

typedef union
{
    IQS323_ELEMENT_I2C_SETTINGS_T elements;
    uint8_t                       bytes[3];
} IQS323_REG_I2C_SETTINGS_T;

/* **********************************************************************
 * Function headers
 */
void iqs323_update_tick(int tick);
int  iqs323_get_tick(void);
void iqs323_update_state(int state);
int  iqs323_get_state(void);
bool iqs323_i2c_write(uint8_t *p_buf, int len);
bool iqs323_i2c_read(uint8_t *p_buf, int len);
bool iqs323_is_rdy_window_opened(void);
bool iqs323_wait_rdy_window_closed(int wait_max_msec);
bool iqs323_rdy_window_open(void);
bool iqs323_rdy_window_close(void);
bool iqs323_ack_reset_event(void);
bool iqs323_confirm_reset_event(void);
bool iqs323_events_enable(void);
bool iqs323_sensor_setup(void);
bool iqs323_touch_settings(void);
bool iqs323_re_ati_trigger(void);
bool iqs323_wait_re_ati_done(void);
bool iqs323_get_touch_state(int *p_state);

bool iqs323_process(void);

#endif /* DRIVER_IQS323_H_ */
