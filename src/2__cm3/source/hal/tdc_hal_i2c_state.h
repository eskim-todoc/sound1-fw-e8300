#ifndef __tdc_hal_i2c_state_h__
#define __tdc_hal_i2c_state_h__

#include "board.h"              //ok
#include "processorDirective.h" //ok

/////////////////////////////////////////////////////
// I2C Rx/Tx 상태
#define TDC_HAL_I2C_ERR_ADDR_OR_CONNECTION -4 // I2C 통신하는 슬레이브 장치가 없는 상황(슬레이브 주소가 다르거나, 통신 라인이 물리적으로 끊어진 상황)
#define TDC_HAL_I2C_ERR_INTERFACE              -3 // I2C 통신 중 에러가 발생되어 복구가 되지 않는 상황
#define TDC_HAL_I2C_ERR_TXRX                  -2 // I2C 통신 중 에러가 발생되었지만, Reset을 통하여 I2C 인터페이스는 정상으로 돌아온 상태
#define TDC_HAL_I2C_ERR_LINE_HELD                   -1 // I2C line이 다른 장치에서 잡혀있어서 반복을 통신을 시작할 수 없는 상태
#define TDC_HAL_I2C_STATE_IDLE                        0
#define TDC_HAL_I2C_STATE_TX_TRIGGERED                 1
#define TDC_HAL_I2C_STATE_TX_ONGOING                      2
#define TDC_HAL_I2C_STATE_TX_DONE                     3
#define TDC_HAL_I2C_STATE_RX_TRIGGERED                 4
#define TDC_HAL_I2C_STATE_RX_ONGOING                      5
#define TDC_HAL_I2C_STATE_RX_DONE                     6

/////////////////////////////////////////////////////
// I2C Line Free가 활성화될 때가지 대기하는 시간
#define TDC_HAL_I2C_WAITING_TIME_MS 50
        

#endif
