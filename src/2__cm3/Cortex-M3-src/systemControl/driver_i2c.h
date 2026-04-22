#ifndef DEFINITION_DRIVER_I2C_H__
#define DEFINITION_DRIVER_I2C_H__

#include <hw.h>
#include <stdbool.h>

#include "board.h"  //ok

#define CM3_I2c_using_ISR

// SCL = SYSCLK / ((PRESCALE+1) × 3) — HW §18.4.5.1

// Run 모드 (SYSCLK=30.72 MHz) 용
// 현재 FPGA 통신이 100kHz 지원..
#define I2C_MASTER_PRESCALE_240 ((uint32_t) (0x4FU << I2C_CFG_MASTER_PRESCALE_Pos))  // 128    kHz
#define I2C_MASTER_PRESCALE_243 ((uint32_t) (0x50U << I2C_CFG_MASTER_PRESCALE_Pos))  // 126.42 kHz

// Sleep 모드 (SYSCLK=2.56 MHz) 용: 동일 PRESCALE 이면 SCL 이 12배 낮아져
// 10.67 kHz 로 떨어지므로, 분주비를 240→21 로 낮춰 SCL ≈ 122 kHz 유지.
#define I2C_MASTER_PRESCALE_21  ((uint32_t) (0x06U << I2C_CFG_MASTER_PRESCALE_Pos))  // 121.9  kHz @ 2.56 MHz

// clang-format off
#define CM3_I2C_CFG_VAL_AsMaster   ( I2C_MASTER_PRESCALE_240            \
                                   | I2C_STOP_INT_ENABLE                \
                                   | I2C_OVERRUN_INT_ENABLE             \
                                   | I2C_BUS_ERROR_INT_ENABLE           \
                                   | I2C_RX_INT_ENABLE                  \
                                   | I2C_TX_INT_ENABLE                  )

#define df__clearI2cHardwareStatus ( I2C_OVERRUN_CLEAR                  \
                                   | I2C_BUS_ERROR_CLEAR                \
                                   | I2C_STOP_DETECTED_CLEAR            \
                                   | I2C_REPEATED_START_DETECTED_CLEAR  )
// clang-format on

typedef struct
{
    uint32_t overrun_clear : 1;
    uint32_t bus_error_clear : 1;
    uint32_t stop_detected_clear : 1;
    uint32_t repeated_start_detected_clear : 1;
    uint32_t tx_req_set : 1;
    uint32_t reserved0 : 3;
    uint32_t overrun : 1;
    uint32_t ack : 1;
    uint32_t gen_call : 1;
    uint32_t read_write : 1;
    uint32_t addr_data : 1;
    uint32_t line_free : 1;
    uint32_t clk_stretch : 1;
    uint32_t rx_req : 1;
    uint32_t tx_req : 1;
    uint32_t data_event : 1;
    uint32_t stop_detected : 1;
    uint32_t master_mode : 1;
    uint32_t start_pending : 1;
    uint32_t busy : 1;
    uint32_t bus_error : 1;
    uint32_t reserved1 : 2;
    uint32_t repeated_start_detected : 1;
    uint32_t stop_or_repeated_start_detected : 1;
} OTE_1_5gen_I2C_STATUS_BIT_FIELD_T;

typedef union
{
    uint32_t                          status;
    OTE_1_5gen_I2C_STATUS_BIT_FIELD_T fields;
} OTE_1_5gen_I2C_STATUS_T;

typedef enum
{
    /*0x10       데이터 버스 사용가능,(리셋이후) */
    I2C_FREE_AF_RESET = 16,

    /*0x410      stop 검출, 데이터 버스 사용가능 -- 인터럽트 상태에설서 읽었을 경우. */
    I2C_Write_FREE_AF_DONE_1 = 1040,

    /*0x10       데이터 버스 사용가능, ACK를 받은 상태(쓰기 완료 후)  --- 인터럽트 종료 후 상태를 읽었을 경우 */
    I2C_Write_FREE_AF_DONE_2 = 16,

    /*0x1008     마스터 모드에서.. 데이터 레지스터에 슬레이브 주소가 들어있음     */
    I2C_Write_DATA_REGISTER_HODL_ADDR = 4104,

    /*0x1248    마스터 모드에서.. 데이터 레지스터에 슬레이브 주소가 들어있고,다음 전송할 데이터가 필요하고, buffer는 차있음    */
    I2C_Write_DATA_REGISTER_HODL_DATA = 4680,

    /*0x1020    마스터 모드에서.. 데이터 1byte 송신이 완료되고 다음 데이터 송신 대기하는 상태로,클럭이 연장되고 있다.(data레지스터에 값을 써야된다)    */
    I2C_Write_CLK_STRETCH = 4128,

    /*0x411     stop 검출, 데이터 버스 사용가능하고,  ACK 신호를 받지 못한 상태   */
    I2C_Write_NOT_ACK_1 = 1041,

    /*0x11      데이터 버스 사용가능하고,  ACK 신호를 받지 못한 상태    */
    I2C_Write_NOT_ACK_2 = 17,

    /*0x455      ACK 신호를 받지 못한 상태, 데이터 버스 사용이 가능하고, stop 검출     - 더미 데이터를 보내지 않을 경우 데이터 전송이 완료되고 인터럽트가 발생
     * 였을 때 읽은 값*/
    I2C_Read_NOT_ACK_1 = 1045,

    /*0x15      ACK 신호를 받지 못한 상태, 데이터 버스 사용이 가능 - 더미 데이터를 보내지 않을 경우 데이터 전송이 완료되고 인터럽트가 종료되고 상태가 변경된
     * 상태 였을 때 읽은 값*/
    I2C_Read_NOT_ACK_2 = 21,

    /*0x455     stop 검출, 버퍼가 차있으며, 데이터 버스 사용가능하고, ACK를 받지 못한 상태(읽기에서 마지막 더미(0xFF) 데이터를 수신한 상태.) */
    I2C_Read_NOT_ACK_3 = 1109,

    /*0x55       버퍼가 차있으며, 데이터 버스 사용가능하고, ACK를 받지 못한 상태(읽기에서 마지막 더미(0xFF) 데이터를 수신한 상태.) */
    I2C_Read_NOT_ACK_4 = 85,

    /*0x1045    버퍼가 차있으며, 데이터 버스 사용 중이며, ACK를 받지 못한 상태(읽기에서 마지막 더미(0xFF) 데이터를 수신한 상태.)  */
    I2C_Read_NOT_ACK_5 = 4165,

    /*0x100c     마스터 모드에서 데이터 레지스터에 슬레이브 주소가 들어있음   */
    I2C_Read_DATA_REGISTER_HODL_ADDR = 4108,

    /*0x120c    마스터 모드에서 데이터.. 레지스터에 슬레이브 주소가 들어있고, 다음 전송할 데이터가 필요하고, buffer는 비어있음  */
    I2C_Read_DATA_REGISTER_HODL_DATA = 4620,

    /*0x1224    마스터 모드에서 데이터.. 슬레이브 주소의  송신이  완료되고, 클럭이 연장되고 있다.(ack 신호를 주어야 되다.)   */
    I2C_Read_CLK_STRETCH_WAITING_ACK = 4644,

    /*0x1064    마스터 모드에서 데이터.. 데이터의 수신이 완료되어 버퍼가 차 있는 상태로,클럭이 연장되고 있다.(데이터 레지스터를 읽고 ACK 신호를 주어야 되다.) */
    I2C_Read_CLK_STRETCH_WAITING_READ = 4196,
} I2C_INTERFACE_STATUS;

typedef enum
{
    I2C_NO_ERROR                 = 0,
    I2C_SLAVE_DEVICE_NO_REACTION = 1,
} EN__I2C_ERROR_CODE;

typedef enum
{
    i2c_state_Idle = 0,
    i2c_state_WriteTriggered,
    i2c_state_WritingDone,
    i2c_state_ReadTriggerd,
    i2c_state_ReadingDone,
    i2c_state_Error,
} EN__I2C_DRIVER_STATE;

typedef struct
{
    EN__I2C_DRIVER_STATE i2c_diver_state;
    int                  i2cTxRx_RemaindedDataLength;
    int*                 p_i2cTx_Source;
    int *p_i2cRx_Destination;
	int slaveAddress;
	EN__I2C_ERROR_CODE i2c_Error_Code;
} ST__I2C_DRIVER;

void enableI2cInterface(bool isEnabled);
void init_I2c(void);

/* I2C 마스터 SCL 분주(MASTER_PRESCALE 필드)만 런타임 재설정.
 * 진행 중 트랜잭션이 있으면 완료될 때까지 대기한 후 CFG 레지스터 갱신.
 *   prescale_mask: I2C_MASTER_PRESCALE_* 매크로 중 하나 (시프트 완료된 값) */
void i2c_set_master_prescale(uint32_t prescale_mask);

EN__I2C_DRIVER_STATE get_i2cDriverStatus(void);
bool isI2cDriverStatusIdle(void);
uint32_t getI2cHardwareStatus(void);
void clearI2cDriverStatus(void);
void setI2cDriverStatusIdle(void);
void clearI2cHardwareStatus(void);
void i2c_startWriteData(const int slaveAddress, int *p_sourcedata, const int dataLength);
void i2c_startReadData(const int slaveAddress, int *p_destination, const int dataLength);
void I2C_0_IRQHandler(void);
void i2c_comm(void);

#endif
