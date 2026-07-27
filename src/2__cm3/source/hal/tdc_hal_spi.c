
#include <hw.h>
#include <stdbool.h>

#include <tdc_hal_dma.h>
#include <tdc_hal_spi.h>
#include <tdc_ble_mapping.h>
#include <processorDirective.h>
#include <tdc_ble_remote.h>

#include <board.h>  // 디버깅용

#include <tdc_printf.h>
#include <tdc_hal_timer.h>

/***********************************************************************
 * GLOBAL VARIABLES
 ***********************************************************************/

/* DMA 가 직접 읽고 쓰는 버퍼는 워드 크기와 1:1 로 맞춘다 (2026-07-27 8비트 전환).
 * DMA 워드 크기를 WORD_SIZE_8BITS_TO_8BITS 로 바꿨으므로 (tdc_hal_dma.h) 원소당 1바이트다.
 * aligned(4) 는 하드웨어 레퍼런스가 §19.4.2.2 에서 "주소는 워드 정렬 필요" 라 하고
 * §19.4.2.6 에서 "8비트 워드는 바이트 주소 지정 가능" 이라 하여 서술이 엇갈리므로,
 * 어느 쪽이 맞든 안전하도록 붙여 둔 것이다. 비용은 패딩 최대 3바이트뿐이다. */
static uint8_t SPI_Rx_Buffer[TDC_HAL_SPI_COMM_PACKET_SIZE] __attribute__((aligned(4))) = {0};
static uint8_t SPI_Tx_Buffer[TDC_HAL_SPI_COMM_PACKET_SIZE] __attribute__((aligned(4))) = {21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

/* Rx_DataPacket 은 DMA 가 건드리지 않는 사본이라 int 를 유지한다.
 * 외부 API tdc_hal_spi_get_rx_packet_addr() 가 int* 를 반환하므로
 * 이 타입을 유지하면 호출부를 하나도 고치지 않아도 된다.
 * SPI_Rx_Buffer(uint8_t) 에서 복사할 때 확대 변환이라 값 손실이 없다. */
static int  Rx_DataPacket[BLE_DataPacketSize];
static bool TxBufferEmpty = true;

void tdc_hal_spi_enable_master_read_command(void)
{
    TxBufferEmpty = false;
    Sys_GPIO_Set_High(GPIO_PIN_ReadCommandForSPI_Master);
    // 마스터 장치에...  slave 장치에서 데이터가 준비되었으니깐 읽어 갈 수 있음을 알려준다.
}

void tdc_hal_spi_clear_master_read_command(void)
{
    TxBufferEmpty = true;
    Sys_GPIO_Set_Low(GPIO_PIN_ReadCommandForSPI_Master);
    // 마스터 장치에서 데이터를 다 읽어 갔다.
}

bool tdc_hal_spi_is_tx_buffer_empty(void)
{
    return TxBufferEmpty;
}

int *tdc_hal_spi_get_rx_packet_addr(void)
{
    return &Rx_DataPacket[0];
}

static tdc_hal_spi_comm_state_t spi_CommuState = TDC_HAL_SPI_COMM_IDLE;

void SPI1_COM_IRQHandler(void)
{
    if (SPI1_STATUS->OVERRUN_ALIAS)
    {
        spi_CommuState = SPI_CMMM_ERROR;
        TDC_PRINTF_E("[SPI] OVERRUN \r\n");
    }

    if (SPI1_STATUS->UNDERRUN_ALIAS)
    {
        spi_CommuState = SPI_CMMM_ERROR;
        TDC_PRINTF_E("[SPI] UNDERRUN \r\n");
    }

    if (SPI1_STATUS->CS_RISE_ALIAS)
    {
        int  dma0_cnt      = DMA0_CNTS->TRANSFER_WORD_CNT_SHORT;
        int  dma1_cnt      = DMA1_CNTS->TRANSFER_WORD_CNT_SHORT;
        bool size_mismatch = false;

        if (dma0_cnt != TDC_HAL_SPI_COMM_PACKET_SIZE)
        {
            spi_CommuState = SPI_CMMM_ERROR;
            TDC_PRINTF_E("[SPI] DMA0_CNTS->TRANSFER_WORD_CNT_SHORT (%d) \r\n", dma0_cnt);
            size_mismatch = true;
        }

        if (dma1_cnt != TDC_HAL_SPI_COMM_PACKET_SIZE)
        {
            spi_CommuState = SPI_CMMM_ERROR;
            TDC_PRINTF_E("[SPI] DMA1_CNTS->TRANSFER_WORD_CNT_SHORT (%d) \r\n", dma1_cnt);
            size_mismatch = true;
        }

        /* (디버그) size mismatch 시 RX/TX 버퍼 내용 dump - 받은/송신한 크기만큼 */
        if (size_mismatch)
        {
            int rx_n = (dma0_cnt > 0 && dma0_cnt <= TDC_HAL_SPI_COMM_PACKET_SIZE) ? dma0_cnt : TDC_HAL_SPI_COMM_PACKET_SIZE;
            int tx_n = (dma1_cnt > 0 && dma1_cnt <= TDC_HAL_SPI_COMM_PACKET_SIZE) ? dma1_cnt : TDC_HAL_SPI_COMM_PACKET_SIZE;

            TDC_PRINTF_W("[SPI] DMA-MISMATCH t3=%d ms / RX_CNT=%d / TX_CNT=%d \r\n",
                      tdc_hal_timer_get_t3_tick(), dma0_cnt, dma1_cnt);

            TDC_PRINTF_W("[SPI] RX_BUFF (LSB->):");
            for (int i = 0; i < rx_n; i++)
            {
                TDC_PRINTF_W(" %02X", (unsigned int)(SPI_Rx_Buffer[i] & 0xFF));
            }
            TDC_PRINTF_W(" \r\n");

            TDC_PRINTF_W("[SPI] TX_BUFF (LSB->):");
            for (int i = 0; i < tx_n; i++)
            {
                TDC_PRINTF_W(" %02X", (unsigned int)(SPI_Tx_Buffer[i] & 0xFF));
            }
            TDC_PRINTF_W(" \r\n");
        }

        tdc_hal_spi_clear_tx_buffer();

        // Disable the SPI before configure the SPI port
        Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_DISABLE);

        // Clear flags
        SPI1->STATUS = TDC_HAL_SPI_STATUS;

        tdc_hal_spi_enable_dma();

        // Enable SPI
        Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_ENABLE);

        tdc_hal_spi_clear_master_read_command();  // 송신이 완료되었음을 CM3 및 NRF에서 알수 있도록 한다.
    }
}

// DMA0: SPI 수신
void DMA0_IRQHandler(void)  // DMA0은 SPI Rx에서 Memory로 패킷 단위의 데이터 완료되었을 때, 또는 에러가 발생하였을 때
                            // 발생된다.. (명령 수신에 사용)
{
    if ((DMA0->STATUS & DMA_COMPLETE_INT_TRUE) != DMA_COMPLETE_INT_TRUE)
    {
        spi_CommuState = SPI_CMMM_ERROR;         // 에러 상황 (DMA0이 완료되지 않았는데 인터럽트가 발생)
        Sys_DMA_Mode_Enable(DMA0, DMA_DISABLE);  // DMA0 끄기

#if 1
        {
            int dma0_cnt = DMA0_CNTS->TRANSFER_WORD_CNT_SHORT;
            int rx_n     = (dma0_cnt > 0 && dma0_cnt <= TDC_HAL_SPI_COMM_PACKET_SIZE) ? dma0_cnt : TDC_HAL_SPI_COMM_PACKET_SIZE;

            TDC_PRINTF_W("[DMA] SPI RX ERROR t3=%d ms / RX_CNT=%d \r\n",
                      tdc_hal_timer_get_t3_tick(), dma0_cnt);
            TDC_PRINTF_W("[DMA] RX_BUFF (LSB->):");
            for (int i = 0; i < rx_n; i++)
            {
                TDC_PRINTF_W(" %02X", (unsigned int)(SPI_Rx_Buffer[i] & 0xFF));
            }
            TDC_PRINTF_W(" \r\n");
        }
#endif
    }
    else
    {
        if (SPI_Rx_Buffer[0] == TDC_HAL_SPI_TX_DUMMY_FOR_MASTER_READ)  // 마스터에서 읽기를 할때는 수신되는 데이터에 더미 데이터(0)이 들어 있다.
        {
            spi_CommuState = TDC_HAL_SPI_COMM_IDLE;
        }
        else  // 마스터에서 쓰기를 할때는 수신되는 데이터의 첫 바이트가 명령 데이터가 들어 있다.
        {
            for (int i = 0; i < BLE_DataPacketSize; i++)
            {
                Rx_DataPacket[i] = SPI_Rx_Buffer[i];
            }

            spi_CommuState = TDC_HAL_SPI_COMM_FETCH;
        }
    }
}

// DMA1: SPI 송신
void DMA1_IRQHandler(void)  // DMA1은 Memory에서 SPI Tx로 패킷 단위의 데이터 송신이 완료되었을 때, 또는 에러가 발생하였을
                            // 때 발생된다.. (명령에 대한 응답에 사용)
{
    if ((DMA1->STATUS & DMA_COMPLETE_INT_TRUE) != DMA_COMPLETE_INT_TRUE)
    {
        spi_CommuState = SPI_CMMM_ERROR;         // 에러 상황 (DMA1이 완료 되지 않았는데 인터럽트가 발생)
        Sys_DMA_Mode_Enable(DMA1, DMA_DISABLE);  // DMA1 끄기
#if 1
        {
            int dma1_cnt = DMA1_CNTS->TRANSFER_WORD_CNT_SHORT;
            int tx_n     = (dma1_cnt > 0 && dma1_cnt <= TDC_HAL_SPI_COMM_PACKET_SIZE) ? dma1_cnt : TDC_HAL_SPI_COMM_PACKET_SIZE;

            TDC_PRINTF_W("[DMA] SPI TX ERROR t3=%d ms / TX_CNT=%d \r\n",
                      tdc_hal_timer_get_t3_tick(), dma1_cnt);
            TDC_PRINTF_W("[DMA] TX_BUFF (LSB->):");
            for (int i = 0; i < tx_n; i++)
            {
                TDC_PRINTF_W(" %02X", (unsigned int)(SPI_Tx_Buffer[i] & 0xFF));
            }
            TDC_PRINTF_W(" \r\n");
        }
#endif
    }
    else
    {
    }
}

tdc_hal_spi_comm_state_t tdc_hal_spi_get_comm_state(void)
{
    return spi_CommuState;
}

void tdc_hal_spi_set_comm_state_idle(void)
{
    spi_CommuState = TDC_HAL_SPI_COMM_IDLE;
}

void tdc_hal_spi_clear_tx_buffer(void)
{
    for (int i = 0; i < TDC_HAL_SPI_COMM_PACKET_SIZE; i++)
    {
        SPI_Tx_Buffer[i] = 0;
    }
}

void tdc_hal_spi_enable_dma(void)
{
    //
    // DMA0: RX (nRF → Cortex-M3) 설정
    //
    Sys_DMA_Mode_Enable(DMA0, DMA_DISABLE);  // DMA0 끄기
    // 입력 순서: dma, cfg, transferLength, counterInt, srcAddr, destAddr
    Sys_DMA_ChannelConfig(DMA0, TDC_HAL_DMA0_CFG0, TDC_HAL_SPI_COMM_PACKET_SIZE, 0, 0x40000E10, ((uint32_t) SPI_Rx_Buffer));
    Sys_DMA_Set_Ctrl(DMA0, TDC_HAL_DMA0_CTRL);
    Sys_DMA_Clear_Status(DMA0, TDC_HAL_DMA0_STATUS);

    //
    // DMA1: RX (Cortex-M3 → nRF) 설정
    //
    Sys_DMA_Mode_Enable(DMA1, DMA_DISABLE);  // DMA1 끄기
    // 입력 순서: dma, cfg, transferLength, counterInt, srcAddr, destAddr
    Sys_DMA_ChannelConfig(DMA1, TDC_HAL_DMA1_CFG0, TDC_HAL_SPI_COMM_PACKET_SIZE, 0, ((uint32_t) SPI_Tx_Buffer), 0x40000E0C);
    Sys_DMA_Set_Ctrl(DMA1, TDC_HAL_DMA1_CTRL);
    Sys_DMA_Clear_Status(DMA1, TDC_HAL_DMA1_STATUS);

    // __KIM: DMA의 MODE_ENABLE 설정 시 DMA_ENABLE로 하면 전송 종료 후 자동으로 DMA가 비활성화 상태로 변경된다.
    Sys_DMA_Mode_Enable(DMA0, DMA_ENABLE);  // DMA0 켜기
    Sys_DMA_Mode_Enable(DMA1, DMA_ENABLE);  // DMA1 켜기
}

void tdc_hal_spi_init(void)
{
    tdc_hal_spi_set_comm_state_idle();  // spi CommuState 초기화

    // SPI pin setting: Cortex-M3 ↔ nRF
    Sys_SPI_DIOConfig(SPI1, SPI_SELECT_SLAVE, SPI_DIO_PIN_CFG, NRF_SPI_CLK_PIN, NRF_SPI_CS_PIN, NRF_SPI_MOSI_PIN, NRF_SPI_MISO_PIN);

    // Disable the SPI before configure the SPI port
    Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_DISABLE);

    // Configure the SPI port
    Sys_SPI_Config(SPI1, TDC_HAL_SPI_CONFIG);

    // NOTE: DMA 활성화 전에 버퍼를 초기화 해야 하는 것으로 보인다.
    //     : DMA 활성화 시점에 TX_DMA 신호가 발생되어 SPI TX 레지스터에 값이 써지는 것으로 보인다.
    //     : 아닐수도 있는데, 일단은 이 순서를 지키는 것으로 마무리 하겠다.

    // DMA 송신 버퍼 초기화 (배열 초기값 설정하여 정의해도 0으로 빌드되는 현상 있음)
    for (int i = 0; i < TDC_HAL_SPI_COMM_PACKET_SIZE; i++)
    {
        SPI_Tx_Buffer[i] = (uint8_t) (1 + i);  // 1~21, uint8_t 범위 내
    }

#if 1
    // Clear flags
    SPI1->STATUS = TDC_HAL_SPI_STATUS;

    tdc_hal_spi_enable_dma();  // DMA0, DMA1 설정

    // Enable SPI
    Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_ENABLE);
#endif

    NVIC_ClearPendingIRQ(SPI1_COM_IRQn);
    NVIC_ClearPendingIRQ(DMA0_IRQn);
    NVIC_ClearPendingIRQ(DMA1_IRQn);

    NVIC_EnableIRQ(SPI1_COM_IRQn);  // SPI communication interrupt (OVERRUN, UNDERRUN, CS RISE)
    NVIC_EnableIRQ(DMA0_IRQn);
    NVIC_EnableIRQ(DMA1_IRQn);

    // NRF에 전송할 데이터가 없음
    tdc_hal_spi_clear_master_read_command();  // Sys_GPIO_Set_Low(GPIO_PIN_ReadCommandForSPI_Master);
}

// void tdc_hal_spi_write_tx_buffer(const int source[], int dataSize)
void tdc_hal_spi_write_tx_buffer(int *source, int dataSize)
{
    // TDC_PRINTF_W("[TX] ENTER empty=%d t3=%d ms\r\n", (int)tdc_hal_spi_is_tx_buffer_empty(), tdc_hal_timer_get_t3_tick());

    while (1)
    {
        if (tdc_hal_spi_is_tx_buffer_empty())
        {
            /* int -> uint8_t 축소 변환 지점 (2026-07-27 8비트 전환).
             * 캐스팅은 "하위 8비트만 쓴다" 는 의도를 드러내려고 명시한 것이다.
             * 동작은 전환 전과 같다. SPI1 이 SPI_WORD_SIZE_8 이라 전환 전에도
             * 상위 24비트는 송신되지 않고 버려졌다. */
            for (int i = 0; i < dataSize; i++)
            {
                SPI_Tx_Buffer[i] = (uint8_t) source[i];
            }

            for (int i = dataSize; i < TDC_HAL_SPI_COMM_PACKET_SIZE; i++)
            {
                SPI_Tx_Buffer[i] = 0;
            }

            SPI_Tx_Buffer[TDC_HAL_SPI_COMM_PACKET_SIZE - 1] = (uint8_t) dataSize;  // 21 - 1 = 20, 20 인덱스에 dataSize 기록

#if 1  // nRF SPI 디버깅
       // 라이브모드의 실시간 전류 값을 제외하고 출력 (데이터 양이 너무 많음)
            bool print_allowed = true;

            if (SPI_Tx_Buffer[0] == 0x66)
            {
                if (SPI_Tx_Buffer[1] == 0x06)
                {
                    print_allowed = false;
                }
            }

            if (print_allowed)
            {
                // TDC_PRINTF_W("[TX] PRINTV-BEFORE\r\n");

                TDC_PRINTF_V("[SPI TX] (LSB) 0x%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
                          "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (MSB) \r\n",
                          SPI_Tx_Buffer[0],
                          SPI_Tx_Buffer[1],
                          SPI_Tx_Buffer[2],
                          SPI_Tx_Buffer[3],
                          SPI_Tx_Buffer[4],
                          SPI_Tx_Buffer[5],
                          SPI_Tx_Buffer[6],
                          SPI_Tx_Buffer[7],
                          SPI_Tx_Buffer[8],
                          SPI_Tx_Buffer[9],
                          SPI_Tx_Buffer[10],
                          SPI_Tx_Buffer[11],
                          SPI_Tx_Buffer[12],
                          SPI_Tx_Buffer[13],
                          SPI_Tx_Buffer[14],
                          SPI_Tx_Buffer[15],
                          SPI_Tx_Buffer[16],
                          SPI_Tx_Buffer[17],
                          SPI_Tx_Buffer[18],
                          SPI_Tx_Buffer[19],
                          SPI_Tx_Buffer[20]);
            }
#endif

            // TDC_PRINTF_W("[TX] DMA-CYCLE-START\r\n");

            Sys_DMA_Mode_Enable(DMA0, DMA_DISABLE);  // DMA0 끄기
            Sys_DMA_Mode_Enable(DMA1, DMA_DISABLE);  // DMA1 끄기

            // Disable the SPI before configure the SPI port
            Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_DISABLE);

            // Clear flags
            SPI1->STATUS = TDC_HAL_SPI_STATUS;

            tdc_hal_spi_enable_dma();

            // Enable SPI
            Sys_SPI_TransferConfig(SPI1, TDC_HAL_SPI_CTRL_ENABLE);

            tdc_hal_spi_enable_master_read_command();
            // TDC_PRINTF_W("[TX] DONE-EXIT t3=%d ms\r\n", tdc_hal_timer_get_t3_tick());
            break;
        }
        else
        {
            // TDC_PRINTF_W("[TX] WFE-ENTER t3=%d ms\r\n", tdc_hal_timer_get_t3_tick());
            __WFE();
            // TDC_PRINTF_W("[TX] WFE-WAKE t3=%d ms empty=%d\r\n",
               //       tdc_hal_timer_get_t3_tick(), (int)tdc_hal_spi_is_tx_buffer_empty());
        }
    }
}
