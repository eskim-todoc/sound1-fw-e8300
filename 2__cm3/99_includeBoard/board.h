

#ifndef BOARD_H__
#define BOARD_H__

// 동작 보드
#include "processorDirective.h"

#if defined(Board_is_OTE_VER_1_5)
// EEPROM
#define EEPROM_SPI_CLK_POS  0
#define EEPROM_SPI_CS_POS   1
#define EEPROM_SPI_SERI_POS 3
#define EEPROM_SPI_SERO_POS 2

#define DIO_NUM_NRF_SWDIO_NRESET                 // NOT USED ANY MORE
#define NRF_SPI_CS_PIN                    DIO21  // v0.35, NET: QCC_SPI_CS
#define NRF_SPI_CLK_PIN                   DIO28  // v0.35, NET: QCC_SPI_CLK
#define NRF_SPI_MOSI_PIN                  DIO26  // v0.35, NET: QCC_SPI_MOSI
#define NRF_SPI_MISO_PIN                  DIO18  // v0.35, NET: QCC_SPI_MISO
#define GPIO_PIN_ReadCommandForSPI_Master DIO20  // v0.35, NET: QCC_SPI_FLAG
#define DIO_NUM_NRF_ON_OFF_COMMAND               // NOT USED ANY MODE

#else
// 보드랑 관계없이 공통적인 핀
#define EEPROM_SPI_CLK_POS  0
#define EEPROM_SPI_CS_POS   1
#define EEPROM_SPI_SERI_POS 2
#define EEPROM_SPI_SERO_POS 3

// NRF와 연결된 핀은 10~19 이며 아래 값은 수정 가능하다.(아래값은 ezairo 7150 datasheet를 보고 맞춘 것이며 소프트웨어가 다르기때문에 수정해도 상관 없다.)
//			CM3	 핀번호        ---------------			     NRF 핀번호
//
//			DIO 10          ---------------         SWDIO_RESET
//			DIO 11          ---------------          P 012
//			DIO 12          ---------------          P 013
//			DIO 13          ---------------          P 009
//			DIO 14          ---------------          P 008
//			DIO 15          ---------------          P 010
//			DIO 16          ---------------          P 015
//			DIO 17          ---------------          P 027
//			DIO 18          ---------------          P 011
//			DIO 19          ---------------          P 014

#define DIO_NUM_NRF_SWDIO_NRESET          10  // nRF51822 reset
#define NRF_SPI_CS_PIN                    11  // SPI Chip Select
#define NRF_SPI_CLK_PIN                   12  // SPI Clock
#define NRF_SPI_MOSI_PIN                  13  // SPI Master Out Slave In
#define NRF_SPI_MISO_PIN                  14  // SPI Master In Slave Out
#define GPIO_PIN_ReadCommandForSPI_Master 15  // ReadCommandForSPI_Master
#define DIO_NUM_NRF_ON_OFF_COMMAND        16  // NRF 칩의  BLE
#define ENABLE_NRF_ADV_LowPower           17  // NRF 광고 전력 모드
#endif

#if defined(Board_is_TD_DEV_ver_1_4)
#include "Board_TD_DEV_ver_1_4.h"
#elif defined(Board_is_OTE_VER_1_3)
#include "Board_OTE_ver1_3.h"
#elif defined(Board_is_OTE_VER_1_5)
#include <Board_OTE_ver1_5.h>
#else
#error Board is NOT selected.
#endif

#endif // BOARD_H__

