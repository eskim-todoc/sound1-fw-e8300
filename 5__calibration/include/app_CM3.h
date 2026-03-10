/**
 * @file app.h
 * @brief Calibration sample application header file.
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

#ifndef APP_H_
#define APP_H_

#include <calibrate.h>
#include <sk5_map_nvm.h>
#include <sk5_sys_calib.h>
#include <trims.h>

#include <nvmctrl.h>
#include <ff.h>
#include <nvmlib.h>

// clang-format off

/* ----------------------------------------------------------------------------
 * Defines
 * --------------------------------------------------------------------------*/

#define DEC_FILTER_CFG  ( BAND_SELECT_ADC_0K_8K | ADC_INTEGER_DELAY_0       | ADC_UNMUTE                               \
                        | ADC_DEC_ENABLE        | ADC_DC_REMOVE_CUTOFF_20HZ | 0x665 )

/** SPI base config */
#define APP_SPI_DEFAULT_CFG ( SPI_TX_DMA_DISABLE      | SPI_RX_DMA_DISABLE       | SPI_TX_START_INT_DISABLE                \
						| SPI_TX_END_INT_DISABLE  | SPI_RX_INT_DISABLE       | SPI_CS_RISE_INT_DISABLE                 \
						| SPI_OVERRUN_INT_DISABLE | SPI_UNDERRUN_INT_DISABLE | SPI_MODE_SPI                            \
						| SPI_WORD_SIZE_8         | SPI_PRESCALE_2           | SPI_CLK_POLARITY_NORMAL                 \
						| SPI_SELECT_MASTER )
// clang-format on

#define SPI0_SCLK_PIN DIO0
#define SPI0_SSEL_PIN DIO1
#define SPI0_MOSI_PIN DIO2
#define SPI0_MISO_PIN DIO3

#define SPI_PIN_CFG (DIO_2X_DRIVE | DIO_LPF_DISABLE | DIO_60K_PULL_UP)

#define LSAD_DIO_INPUT LSAD_INPUT_DIO23

#define REF_CLK_DIO DIO19

#define CI_LED_DIO_GREEN DIO22
#define CI_LED_DIO_BLUE  DIO29
#define CI_LED_DIO_RED   DIO24

#define PIN_CFG_LED (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT)

/**
 * @brief Initializes the system and gets it in a ready state for calibration
 *        and for accessing NVM for loading and storing trimmings.
 */
void Initialize(void);

/**
 * Initializes the NVM library
 * @return ARM_DRIVER_OK if successful, ARM_DRIVER_ERROR otherwise
 */
int NVMInit_local(void);

/**
 * Toggles GPIO.
 * @param dio The GPIO to toggle [0-35]
 * @param n Number of times to toggle
 * @param delay_s delay between each toggle
 */
void ToggleGPIO(uint8_t dio, uint8_t n, uint32_t delay_s);

/**
 * Calls the appropriate delay function depending on the core.
 * @param delay_s delay in seconds
 */
void Delay(uint32_t delay_ms);
/**
 * Exit loop. Refreshes watchdog timer and calls ToggleGPIO endlessly.
 * @param status
 */
void ExitApp(int status);

/**
 * @brief Main CFX entry point
 * @return Zero but main function is generally not expected to return
 */
int main(void);

#endif /* APP_H_ */
