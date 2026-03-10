#ifndef INTERFACE_MODUEL_I2C_H__
#define INTERFACE_MODUEL_I2C_H__

#include "board.h"              //ok
#include "processorDirective.h" //ok

/////////////////////////////////////////////////////
// I2C Rx/Tx 상태
#define df_I2C_AddressDifferentOrConnectionError -4 // I2C 통신하는 슬레이브 장치가 없는 상황(슬레이브 주소가 다르거나, 통신 라인이 물리적으로 끊어진 상황)
#define df_I2C_State_InterfaceError              -3 // I2C 통신 중 에러가 발생되어 복구가 되지 않는 상황
#define df_I2C_State_TxRx_Error                  -2 // I2C 통신 중 에러가 발생되었지만, Reset을 통하여 I2C 인터페이스는 정상으로 돌아온 상태
#define df_I2C_State_Line_Held                   -1 // I2C line이 다른 장치에서 잡혀있어서 반복을 통신을 시작할 수 없는 상태
#define df_I2C_State_Idle                        0
#define df_I2C_State_Tx_Trigered                 1
#define df_I2C_State_Tx_ing                      2
#define df_I2C_State_Tx_done                     3
#define df_I2C_State_Rx_Trigered                 4
#define df_I2C_State_Rx_ing                      5
#define df_I2C_State_Rx_done                     6

/////////////////////////////////////////////////////
// I2C Line Free가 활성화될 때가지 대기하는 시간
#define df_I2C_watingTime_ms 50
        

#endif
