#include <board.h>
#if defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#ifndef __tdc_drv_isl9122_h__
#define __tdc_drv_isl9122_h__

#include <stdbool.h>

#define TDC_DRV_ISL9122_SLAVE_ADDR 0x18

#define TDC_DRV_ISL9122_REG_VOLTAGESET   0x11
#define TDC_DRV_ISL9122_REG_CONV_CFG     0x12
#define TDC_DRV_ISL9122_REG_INTFLAG_MASK 0x13

#define TDC_DRV_PMIC_DEFAULTVALUE_CONV_CFG    0x81
#define TDC_DRV_PMIC_DEFAULTVALUE_INTFLAG_MAS 0x00

#define TDC_DRV_PMIC_BITPOSITION_TYPE 0
#define TDC_DRV_PMIC_BITLENGTH__TYPE  1

#define TDC_DRV_PMIC_BITPOSITION_OC_FAULT_MODE 5
#define TDC_DRV_PMIC_BITLENGTH_OC_FAULT_MODE   2

#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE 214 // 0.025*214=5.35V
                                   //  #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE      190     // 0.025*200=4.75V
// #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE    180     // 0.025*180=4.5V
// #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE    168     // 0.025*168=4.2V
// #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE    160     // 0.025*160=4.0V
#define TDC_DRV_PMIC_VOLTAGE_CONTROL_STEP 5
#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE    164 // 0.025*160=4V
// #define TDC_DRV_PMIC_MIN_TX_POWER_VALUE       178     // 0.025*178=4.45
// #define TDC_DRV_PMIC_MIN_TX_POWER_VALUE   (TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE-1)          // 0.025*178=4.45

#define TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE 75 // 0.025*75=1.875V

bool tdc_drv_isl9122_write_register(int registerAddr, int value);
bool tdc_drv_isl9122_read_register(int registerAddr, int *read_value);
bool tdc_drv_isl9122_reset(void);

#endif

#endif
