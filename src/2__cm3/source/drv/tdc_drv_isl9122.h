#include <board.h>
#ifndef __tdc_drv_isl9122_h__
#define __tdc_drv_isl9122_h__

#include <stdbool.h>

#define TDC_DRV_ISL9122_SLAVE_ADDR 0x18

#define TDC_DRV_ISL9122_REG_VOLTAGESET 0x11
#define TDC_DRV_ISL9122_REG_CONV_CFG   0x12

#define TDC_DRV_PMIC_DEFAULTVALUE_CONV_CFG 0x81

/* INTFLAG_MASK 레지스터·OC_FAULT_MODE 비트필드 매크로 6종은 2차 리팩토링에서 제거했다.
 * 이들을 쓰던 tdc_drv_isl9122_reset() 안의 구버전 설정 경로가 #if 0 죽은 블록이었고,
 * 그 블록을 정리하면서 매크로가 고아가 됐다. 현재는 CONV_CFG 를 쓰고 되읽어 확인하는
 * 방식만 살아 있다. */

#define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE 214 // 0.025*214=5.35V
                                   //  #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE      190     // 0.025*200=4.75V
// #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE    180     // 0.025*180=4.5V
// #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE    168     // 0.025*168=4.2V
// #define TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE    160     // 0.025*160=4.0V
#define TDC_DRV_PMIC_VOLTAGE_CONTROL_STEP 5

//#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE    164 // 0.025*160=4V
#define TDC_DRV_PMIC_MIN_TX_POWER_VALUE    140 // 0.025*140=3.5V

// #define TDC_DRV_PMIC_MIN_TX_POWER_VALUE       178     // 0.025*178=4.45
// #define TDC_DRV_PMIC_MIN_TX_POWER_VALUE   (TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE-1)          // 0.025*178=4.45

#define TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE 75 // 0.025*75=1.875V

bool tdc_drv_isl9122_write_register(int registerAddr, int value);
bool tdc_drv_isl9122_read_register(int registerAddr, int *read_value);
bool tdc_drv_isl9122_reset(void);

#endif

