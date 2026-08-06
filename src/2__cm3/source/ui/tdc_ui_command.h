#ifndef TDC_UI_COMMAND_H__
#define TDC_UI_COMMAND_H__

#include <stdbool.h>
#include <stdint.h>

void tdc_ui_command_init(void);
void tdc_ui_command_poll(void);

/* Override hooks - main.c 에서 호출 */
bool tdc_ui_command_override_isd_active(void);
bool tdc_ui_command_override_isd_value(void);

bool tdc_ui_command_override_map_active(void);
bool tdc_ui_command_override_map_value(void);

bool    tdc_ui_command_override_battery_active(void);
uint8_t tdc_ui_command_override_battery_percent(void);

/* 실 상태 입력 hook - main.c 매 cycle 호출 */
void tdc_ui_command_set_mapping_connected(bool connected);

/* --led req 로 직접 설정한 소스를 main loop 이 덮어쓰지 않도록 가드 */
bool tdc_ui_command_is_led_override(int source);

#endif
