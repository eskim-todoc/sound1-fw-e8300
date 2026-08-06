#ifndef __tdc_hal_spi_h__
#define __tdc_hal_spi_h__

#include <hw.h>
#include <stdbool.h>

#include <tdc_ble_protocol.h>    //ok
#include <processorDirective.h>  //ok

#define TDC_HAL_SPI_COMM_PACKET_SIZE (BLE_DataPacketSize + 1)  // 20 + 1 = 21

#define TDC_HAL_SPI_COMM_PAYLOAD_SIZE_INDEX (TDC_HAL_SPI_COMM_PACKET_SIZE - 1)  // 21 - 1 = 20

#ifdef CM3_SPI_USING_DMA

#define TDC_HAL_SPI_CONFIG                                                                                                                                     \
    (SPI_SELECT_SLAVE | SPI_WORD_SIZE_8 | SPI_UNDERRUN_INT_ENABLE | SPI_OVERRUN_INT_ENABLE | SPI_CS_RISE_INT_ENABLE | SPI_RX_DMA_ENABLE | SPI_TX_DMA_ENABLE)
#define TDC_HAL_SPI_STATUS       (SPI_UNDERRUN_CLEAR | SPI_OVERRUN_CLEAR | SPI_CS_RISE_CLEAR | SPI_TX_REQ_SET)
#define TDC_HAL_SPI_CTRL_ENABLE  (SPI_MODE_READ_WRITE | SPI_ENABLE)
#define TDC_HAL_SPI_CTRL_DISABLE (SPI_MODE_READ_WRITE | SPI_DISABLE)
#else
#error "CM3_SPI_USING_DMA is not defined."
#endif

typedef enum
{
    TDC_HAL_SPI_COMM_RESET = -1,
    TDC_HAL_SPI_COMM_IDLE  = 0,
    TDC_HAL_SPI_COMM_FETCH,
    SPI_CMMM_ERROR,
} tdc_hal_spi_comm_state_t;

#define TDC_HAL_SPI_TX_DUMMY_FOR_MASTER_READ 0

//////////////////////////////////////////////////////

//////////////////////////////////////////////////////

void SPI_1_RX_IRQHandler(void);
void SPI_1_TX_IRQHandler(void);
void SPI_1_COM_IRQHandler(void);

void                     tdc_hal_spi_enable_dma(void);
void                     tdc_hal_spi_init(void);
tdc_hal_spi_comm_state_t tdc_hal_spi_get_comm_state(void);
void                     tdc_hal_spi_set_comm_state_idle(void);
uint8_t                 *tdc_hal_spi_get_rx_packet_addr(void);
void                     tdc_hal_spi_write_tx_buffer(const uint8_t *source, int dataSize);

void tdc_hal_spi_enable_master_read_command(void);
void tdc_hal_spi_clear_master_read_command(void);
bool tdc_hal_spi_is_tx_buffer_empty(void);

void tdc_hal_spi_clear_tx_buffer(void);

/////////////////////////////////////////////////////
#endif
