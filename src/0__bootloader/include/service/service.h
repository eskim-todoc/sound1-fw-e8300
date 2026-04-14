
#ifndef __esp_inc_debugger_h__
#define __esp_inc_debugger_h__

#include <hw.h>
#include <bootloader.h>
#include <stdbool.h>

typedef struct
{
    uint8_t r        : 1;
    uint8_t g        : 1;
    uint8_t b        : 1;
    uint8_t reserved : 5;
} esp_debugger_led_element_t;

typedef union
{
    esp_debugger_led_element_t element;
    uint8_t                    raw;
} esp_debugger_led_t;

void service_main(void);

#endif /* __esp_inc_debugger_h__ */
