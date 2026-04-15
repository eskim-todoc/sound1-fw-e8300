#include "ui_led_cmd.h"
#include "ui_cmd.h"
#include "ui_sys_cmd.h"

#include <string.h>

#include "LedOutput.h"
#include "batteryNPowerControl.h"
#include "cfx_cm3_sharedMemory.h"

/* ======================================================================== */
/*  String ↔ enum helpers (case-insensitive)                                */
/* ======================================================================== */

static int ci_strcasecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char) *a - (unsigned char) *b;
}

typedef struct {
    const char *name;
    int         val;
} str_map_t;

static const str_map_t k_src_map[] = {
    {"POWER",   LED_SRC_POWER},
    {"ERROR",   LED_SRC_ERROR},
    {"BLE_IND", LED_SRC_BLE_IND},
    {"MAPPING", LED_SRC_MAPPING},
    {"BATTERY", LED_SRC_BATTERY},
    {"ISD",     LED_SRC_ISD},
    {NULL, 0}
};

static const str_map_t k_state_map[] = {
    {"NONE",           LED_ST_NONE},
    {"READY",          LED_ST_READY},
    {"IN_USE",         LED_ST_IN_USE},
    {"BATT_MID",       LED_ST_BATT_MID},
    {"BATT_CRITICAL",  LED_ST_BATT_CRITICAL},
    {"MAPPING_NO_ISD", LED_ST_MAPPING_NO_ISD},
    {"MAPPING_ISD",    LED_ST_MAPPING_ISD},
    {"PAIR",           LED_ST_PAIR},
    {"OTA_QCC",        LED_ST_OTA_QCC},
    {"OTA_EZAIRO",     LED_ST_OTA_EZAIRO},
    {"ERROR_MAP",      LED_ST_ERROR_MAP},
    {"ERROR_MCU",      LED_ST_ERROR_MCU},
    {"ERROR_ACCEL",    LED_ST_ERROR_ACCEL},
    {"ERROR_FPGA",     LED_ST_ERROR_FPGA},
    {"ERROR_PMIC",     LED_ST_ERROR_PMIC},
    {"POWER_ON",       LED_ST_POWER_ON},
    {"POWER_OFF",      LED_ST_POWER_OFF},
    {NULL, 0}
};

static int str_to_val(const str_map_t *map, const char *tok)
{
    for (int i = 0; map[i].name; i++)
    {
        if (ci_strcasecmp(tok, map[i].name) == 0)
            return map[i].val;
    }
    return -1;
}

static const char *val_to_str(const str_map_t *map, int val)
{
    for (int i = 0; map[i].name; i++)
    {
        if (map[i].val == val)
            return map[i].name;
    }
    return "?";
}

/* ======================================================================== */
/*  :led handler                                                            */
/* ======================================================================== */

static int cmd_led(int argc, char *argv[])
{
    if (argc < 2)
        return -1;

    /* :led show */
    if (ci_strcasecmp(argv[1], "show") == 0)
    {
        ui_cmd_printf("--- LED Arbiter state ---\r\n");
        for (int src = 0; src < LED_SRC__MAX; src++)
        {
            led_state_t st = led_get_request((led_src_t) src);
            ui_cmd_printf("  [%s] = %s\r\n",
                          val_to_str(k_src_map, src),
                          val_to_str(k_state_map, st));
        }
        ui_cmd_printf("  burst_in_progress = %d\r\n", led_is_power_burst_in_progress());
        ui_cmd_printf("  user_led_off = %d\r\n", (readLED_indicatorOnOff() == 2) ? 1 : 0);
        ui_cmd_printf("-------------------------\r\n");
        return 0;
    }

    /* :led req <src> <state> */
    if (ci_strcasecmp(argv[1], "req") == 0)
    {
        if (argc < 4) return -1;

        int src = str_to_val(k_src_map, argv[2]);
        int st  = str_to_val(k_state_map, argv[3]);
        if (src < 0 || st < 0)
        {
            ui_cmd_printf("invalid src or state\r\n");
            return -1;
        }
        led_request((led_src_t) src, (led_state_t) st);
        ui_cmd_printf("OK: %s <- %s\r\n", argv[2], argv[3]);
        return 0;
    }

    /* :led clr <src> */
    if (ci_strcasecmp(argv[1], "clr") == 0)
    {
        if (argc < 3) return -1;

        int src = str_to_val(k_src_map, argv[2]);
        if (src < 0)
        {
            ui_cmd_printf("invalid src\r\n");
            return -1;
        }
        led_request((led_src_t) src, LED_ST_NONE);
        ui_cmd_printf("OK: %s cleared\r\n", argv[2]);
        return 0;
    }

    /* :led user <0|1> */
    if (ci_strcasecmp(argv[1], "user") == 0)
    {
        if (argc < 3) return -1;

        int val = argv[2][0] - '0';
        /* 1=Off → indicatorLED_OnOff=2, 0=On → indicatorLED_OnOff=1 */
        changeLED_indicatorOnOff(val ? en__PAYLOAD_OFF : en__PAYLOAD_ON);
        ui_cmd_printf("OK: user LED %s\r\n", val ? "OFF" : "ON");
        return 0;
    }

    /* :led pair */
    if (ci_strcasecmp(argv[1], "pair") == 0)
    {
        led_request(LED_SRC_BLE_IND, LED_ST_PAIR);
        ui_cmd_printf("OK: PAIR latch injected\r\n");
        return 0;
    }

    /* :led burst <on|off> */
    if (ci_strcasecmp(argv[1], "burst") == 0)
    {
        if (argc < 3) return -1;

        if (ci_strcasecmp(argv[2], "on") == 0)
        {
            ui_cmd_printf("burst_in_progress = %d\r\n", led_is_power_burst_in_progress());
        }
        else if (ci_strcasecmp(argv[2], "off") == 0)
        {
            /* force-clear power source */
            led_request(LED_SRC_POWER, LED_ST_NONE);
            ui_cmd_printf("OK: power burst force-cleared\r\n");
        }
        else
        {
            return -1;
        }
        return 0;
    }

    return -1;
}

/* ======================================================================== */
/*  :batt handler                                                           */
/* ======================================================================== */

static int cmd_batt(int argc, char *argv[])
{
    if (argc < 2)
        return -1;

    /* :batt show */
    if (ci_strcasecmp(argv[1], "show") == 0)
    {
        ui_cmd_printf("  real  = %d%%\r\n", snd_batt_get_percent());
        if (ui_sys_ovr_batt_active())
            ui_cmd_printf("  ovr   = %d%% (active)\r\n", ui_sys_ovr_batt_percent());
        else
            ui_cmd_printf("  ovr   = inactive\r\n");
        return 0;
    }

    /* :batt <pct> — 0~100 */
    int pct = 0;
    const char *p = argv[1];
    while (*p >= '0' && *p <= '9')
    {
        pct = pct * 10 + (*p - '0');
        p++;
    }
    if (*p != '\0' || pct > 100)
    {
        ui_cmd_printf("invalid pct (0~100)\r\n");
        return -1;
    }

    ui_sys_ovr_set_batt((uint8_t) pct);
    ui_cmd_printf("OK: batt override = %d%%\r\n", pct);
    return 0;
}

/* ======================================================================== */
/*  Registration                                                            */
/* ======================================================================== */

static const ui_cmd_entry_t k_entries[] = {
    {"led",  cmd_led,  ":led show|req|clr|user|pair|burst"},
    {"batt", cmd_batt, ":batt <0~100>|show"},
};

void ui_led_cmd_register(void)
{
    for (int i = 0; i < (int)(sizeof(k_entries) / sizeof(k_entries[0])); i++)
    {
        ui_cmd_register(&k_entries[i]);
    }
}
