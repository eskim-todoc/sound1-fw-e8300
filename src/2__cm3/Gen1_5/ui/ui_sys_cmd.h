#ifndef UI_SYS_CMD_H__
#define UI_SYS_CMD_H__

#include <stdbool.h>
#include <stdint.h>

void ui_sys_cmd_register(void);

/* ========================================================================
 *  Override hooks — main.c / systemControl.c 에서 호출
 * ======================================================================== */

bool    ui_sys_ovr_isd_active(void);
bool    ui_sys_ovr_isd_value(void);

bool    ui_sys_ovr_map_active(void);
bool    ui_sys_ovr_map_value(void);

bool    ui_sys_ovr_batt_active(void);
uint8_t ui_sys_ovr_batt_percent(void);

/* ui_led_cmd.c 에서 :batt <pct> 설정 시 호출 */
void    ui_sys_ovr_set_batt(uint8_t pct);
void    ui_sys_ovr_clear_batt(void);

#endif
