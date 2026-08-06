

#ifndef BOARD_H__
#define BOARD_H__

// 동작 보드
#include <processorDirective.h>

/* 지원 보드: Board_is_OTE_VER_1_5 만. 구세대(ALASKA_3 · Bingley · Evaluation ·
 * OTE 1.0~1.4 · TD_DEV 1.4)는 Sullivan 1~1.5 세대 레거시로 제거했다(2026-07-22).
 * 필요 시 git 이력에서 복원. */
#if !defined(Board_is_OTE_VER_1_5)
#error Board is NOT selected or NOT supported. (supported: Board_is_OTE_VER_1_5)
#endif

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

#include <Board_OTE_ver1_5.h>

#endif  // BOARD_H__
