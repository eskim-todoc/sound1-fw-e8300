#ifndef DEFINITION_DRIVER_SPI_H__
#define DEFINITION_DRIVER_SPI_H__

#include <hw.h>
#include <stdbool.h>

#include "ble_commonProtocol.h" //ok
#include "processorDirective.h" //ok

#define SPI_COMM_PACKET_SIZE (BLE_DataPacketSize + 1) // 20 + 1 = 21

#define SPI_COMM_PayLodeSize_index (SPI_COMM_PACKET_SIZE - 1) // 21 - 1 = 20

#ifdef CM3_SPI_USING_DMA

#define DRIVER_SPI_CONFIG \
    (SPI_SELECT_SLAVE | SPI_WORD_SIZE_8 | SPI_UNDERRUN_INT_ENABLE | SPI_OVERRUN_INT_ENABLE | SPI_CS_RISE_INT_ENABLE | SPI_RX_DMA_ENABLE | SPI_TX_DMA_ENABLE)
#define DRIVER_SPI_STATUS       (SPI_UNDERRUN_CLEAR | SPI_OVERRUN_CLEAR | SPI_CS_RISE_CLEAR | SPI_TX_REQ_SET)
#define DRIVER_SPI_CTRL_ENABLE  (SPI_MODE_READ_WRITE | SPI_ENABLE)
#define DRIVER_SPI_CTRL_DISABLE (SPI_MODE_READ_WRITE | SPI_DISABLE)
#else
#error "CM3_SPI_USING_DMA is not defined."
#endif

typedef enum
{
    SPI_COMM_RESET = -1,
    SPI_COMM_IDLE  = 0,
    SPI_CMMM_FETCH,
    SPI_CMMM_ERROR,
} EN__SPI_COMMU_STATE;

#define SPI_TxDummyDataForMasterToRead 0

//////////////////////////////////////////////////////
int *get_rxBuffer_IRQ(void);
int *get_rxCheck_IRQ(void);
int  get_rxIndex_IRQ(void);
void set_rxIndex_IRQ(int index);

//////////////////////////////////////////////////////
int *getAddr_SPI_Rx_Buffer(void);
int *getAddr_SPI_Tx_Buffer(void);

void SPI_1_RX_IRQHandler(void);
void SPI_1_TX_IRQHandler(void);
void SPI_1_COM_IRQHandler(void);

void                ci_SPI_enable_DMA(void);
void                init_cm3_SPI(void);
EN__SPI_COMMU_STATE get_spi_commu_state(void);
void                set_spi_commu_state_IDLE(void);
int                *getAddr_SPI_Rx_DataPacket(void);
// void                writeDataToSpiTxBuff(const int source[], int dataSize);
void       writeDataToSpiTxBuff(int *source, int dataSize);
char       getCommandFromSPI_DataPacket(void);
char       getDataFromSPI_DataPacket(int index);
const int *getDataAddr_FromSPI_DataPacket(int index);
void       common_spi(void);

void set_spi_commu_state(EN__SPI_COMMU_STATE state);
void enable_ReadCommandForSPI_Master(void);
void clear_ReadCommandForSPI_Master(void);
bool isSpiTxBuffEmpty(void);

void clear_SPI_Tx_Buffer(void);

/////////////////////////////////////////////////////
#endif
