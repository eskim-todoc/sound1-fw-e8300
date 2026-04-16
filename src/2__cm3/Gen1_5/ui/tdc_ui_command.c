#include "tdc_ui_command.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SEGGER_RTT.h>

#include "LedOutput.h"
#include "batteryNPowerControl.h"
#include "cfx_cm3_sharedMemory.h"
#include "error.h"
#include "ci_event_log.h"
#include "ci_map.h"

#include <ci_printf.h>

/* ======================================================================== */
/*  Defines                                                                 */
/* ======================================================================== */

#define SENTINEL      "--"
#define SENTINEL_LEN  2
#define MAX_LINE      128
#define MAX_ARGC      8
#define MAX_HANDLERS  32

/* ======================================================================== */
/*  Internal types                                                          */
/* ======================================================================== */

typedef int (*command_fn_t)(int argc, char *argv[]);

typedef struct {
    const char    *name;
    command_fn_t   fn;
    const char    *help;
} command_entry_t;

typedef struct {
    const char *name;
    int         val;
} str_map_t;

/* ======================================================================== */
/*  Override state                                                          */
/* ======================================================================== */

static bool    s_tdc_override_isd_active;
static bool    s_tdc_override_isd_value;

static bool    s_tdc_override_map_active;
static bool    s_tdc_override_map_value;

static bool    s_tdc_override_battery_active;
static uint8_t s_tdc_override_battery_percent;

/* --led req/clr 로 설정한 per-source LED override */
static bool    s_tdc_led_override[LED_SRC__MAX];

/* ======================================================================== */
/*  Override hook getters (public — called from main.c)                     */
/* ======================================================================== */

bool tdc_ui_command_override_isd_active(void)        { return s_tdc_override_isd_active; }
bool tdc_ui_command_override_isd_value(void)         { return s_tdc_override_isd_value; }

bool tdc_ui_command_override_map_active(void)        { return s_tdc_override_map_active; }
bool tdc_ui_command_override_map_value(void)         { return s_tdc_override_map_value; }

bool tdc_ui_command_override_battery_active(void)    { return s_tdc_override_battery_active; }
uint8_t tdc_ui_command_override_battery_percent(void){ return s_tdc_override_battery_percent; }

bool tdc_ui_command_is_led_override(int source)
{
    if (source < 0 || source >= LED_SRC__MAX)
        return false;
    return s_tdc_led_override[source];
}

/* ======================================================================== */
/*  Framework state                                                         */
/* ======================================================================== */

static command_entry_t s_tdc_table[MAX_HANDLERS];
static uint8_t         s_tdc_table_cnt;

static char    s_tdc_line[MAX_LINE];
static uint8_t s_tdc_line_len;

/* ======================================================================== */
/*  Low-level I/O                                                           */
/* ======================================================================== */

static void echo_char(char ch)
{
    SEGGER_RTT_Write(0, &ch, 1);
}

static void echo_string(const char *s)
{
    SEGGER_RTT_Write(0, s, (unsigned) strlen(s));
}

static void print_prompt(void)
{
    echo_string("> ");
}

static void output_printf(const char *fmt, ...)
{
    char    buf[256];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0)
    {
        if (n > (int) sizeof(buf) - 1)
            n = (int) sizeof(buf) - 1;
        SEGGER_RTT_Write(0, buf, (unsigned) n);
    }
}

/* ======================================================================== */
/*  String helpers                                                          */
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
/*  String <-> enum maps                                                    */
/* ======================================================================== */

static const str_map_t s_tdc_source_map[] = {
    {"power",   LED_SRC_POWER},
    {"error",   LED_SRC_ERROR},
    {"ble",     LED_SRC_BLE_IND},
    {"mapping", LED_SRC_MAPPING},
    {"battery", LED_SRC_BATTERY},
    {"isd",     LED_SRC_ISD},
    {NULL, 0}
};

static const str_map_t s_tdc_state_map[] = {
    {"none",       LED_ST_NONE},
    /* power */
    {"on",         LED_ST_POWER_ON},
    {"off",        LED_ST_POWER_OFF},
    /* battery */
    {"ready",      LED_ST_BATT_READY},
    {"mid",        LED_ST_BATT_MID},
    {"critical",   LED_ST_BATT_CRITICAL},
    /* isd */
    {"in_use",     LED_ST_IN_USE},
    /* mapping */
    {"no_isd",     LED_ST_MAPPING_NO_ISD},
    {"with_isd",   LED_ST_MAPPING_ISD},
    /* ble */
    {"pair",       LED_ST_PAIR},
    {"ota_qcc",    LED_ST_OTA_QCC},
    {"ota_ezairo", LED_ST_OTA_EZAIRO},
    /* error */
    {"map",        LED_ST_ERROR_MAP},
    {"mcu",        LED_ST_ERROR_MCU},
    {"accel",      LED_ST_ERROR_ACCEL},
    {"fpga",       LED_ST_ERROR_FPGA},
    {"pmic",       LED_ST_ERROR_PMIC},
    {NULL, 0}
};

/* ======================================================================== */
/*  Framework: register, print_help                                         */
/* ======================================================================== */

static bool register_command(const command_entry_t *entry)
{
    if (s_tdc_table_cnt >= MAX_HANDLERS)
        return false;

    s_tdc_table[s_tdc_table_cnt] = *entry;
    s_tdc_table_cnt++;
    return true;
}

static void print_help(void)
{
    output_printf("--- commands ---\r\n");
    for (int i = 0; i < s_tdc_table_cnt; i++)
    {
        output_printf("  --%s\t%s\r\n", s_tdc_table[i].name, s_tdc_table[i].help);
    }
    output_printf("----------------\r\n");
}

/* ======================================================================== */
/*  --help handler                                                          */
/* ======================================================================== */

static int handle_help(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    print_help();
    return 0;
}

/* ======================================================================== */
/*  --led handler                                                           */
/* ======================================================================== */

static int handle_led(int argc, char *argv[])
{
    if (argc < 2)
        return -1;

    /* --led show */
    if (ci_strcasecmp(argv[1], "show") == 0)
    {
        output_printf("--- LED Arbiter state ---\r\n");
        for (int src = 0; src < LED_SRC__MAX; src++)
        {
            led_state_t state = led_get_request((led_src_t) src);
            output_printf("  [%s] = %s%s\r\n",
                          val_to_str(s_tdc_source_map, src),
                          val_to_str(s_tdc_state_map, state),
                          s_tdc_led_override[src] ? " (override)" : "");
        }
        output_printf("  burst_in_progress = %d\r\n", led_is_power_burst_in_progress());
        output_printf("  user_led_off = %d\r\n", (readLED_indicatorOnOff() == 2) ? 1 : 0);
        output_printf("-------------------------\r\n");
        return 0;
    }

    /* --led req <src> <state> */
    if (ci_strcasecmp(argv[1], "req") == 0)
    {
        if (argc < 4) return -1;

        int src   = str_to_val(s_tdc_source_map, argv[2]);
        int state = str_to_val(s_tdc_state_map, argv[3]);
        if (src < 0 || state < 0)
        {
            output_printf("invalid src or state\r\n");
            return -1;
        }
        s_tdc_led_override[src] = true;
        led_request((led_src_t) src, (led_state_t) state);
        output_printf("OK: %s <- %s (override)\r\n", argv[2], argv[3]);
        return 0;
    }

    /* --led clr <src> */
    if (ci_strcasecmp(argv[1], "clr") == 0)
    {
        if (argc < 3) return -1;

        int src = str_to_val(s_tdc_source_map, argv[2]);
        if (src < 0)
        {
            output_printf("invalid src\r\n");
            return -1;
        }
        s_tdc_led_override[src] = false;
        led_request((led_src_t) src, LED_ST_NONE);
        output_printf("OK: %s cleared\r\n", argv[2]);
        return 0;
    }

    /* --led user on|off */
    if (ci_strcasecmp(argv[1], "user") == 0)
    {
        if (argc < 3) return -1;

        if (ci_strcasecmp(argv[2], "on") == 0)
        {
            changeLED_indicatorOnOff(en__PAYLOAD_ON);
            output_printf("OK: user LED on\r\n");
        }
        else if (ci_strcasecmp(argv[2], "off") == 0)
        {
            changeLED_indicatorOnOff(en__PAYLOAD_OFF);
            output_printf("OK: user LED off\r\n");
        }
        else
        {
            return -1;
        }
        return 0;
    }

    /* --led pair */
    if (ci_strcasecmp(argv[1], "pair") == 0)
    {
        led_request(LED_SRC_BLE_IND, LED_ST_PAIR);
        output_printf("OK: PAIR latch injected\r\n");
        return 0;
    }

    /* --led burst <on|off> */
    if (ci_strcasecmp(argv[1], "burst") == 0)
    {
        if (argc < 3) return -1;

        if (ci_strcasecmp(argv[2], "on") == 0)
        {
            output_printf("burst_in_progress = %d\r\n", led_is_power_burst_in_progress());
        }
        else if (ci_strcasecmp(argv[2], "off") == 0)
        {
            /* force-clear power source */
            led_request(LED_SRC_POWER, LED_ST_NONE);
            output_printf("OK: power burst force-cleared\r\n");
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
/*  --batt handler                                                          */
/* ======================================================================== */

static int handle_battery(int argc, char *argv[])
{
    if (argc < 2)
        return -1;

    /* --batt show */
    if (ci_strcasecmp(argv[1], "show") == 0)
    {
        output_printf("  real  = %d%%\r\n", snd_batt_get_percent());
        if (s_tdc_override_battery_active)
            output_printf("  ovr   = %d%% (active)\r\n", s_tdc_override_battery_percent);
        else
            output_printf("  ovr   = inactive\r\n");
        return 0;
    }

    /* --batt <percent> -- 0~100 */
    int percent = 0;
    const char *p = argv[1];
    while (*p >= '0' && *p <= '9')
    {
        percent = percent * 10 + (*p - '0');
        p++;
    }
    if (*p != '\0' || percent > 100)
    {
        output_printf("invalid percent (0~100)\r\n");
        return -1;
    }

    s_tdc_override_battery_active  = true;
    s_tdc_override_battery_percent = (uint8_t) percent;
    output_printf("OK: battery override = %d%%\r\n", percent);
    return 0;
}

/* ======================================================================== */
/*  --isd handler                                                           */
/* ======================================================================== */

static int handle_isd(int argc, char *argv[])
{
    if (argc < 2) return -1;

    bool val;
    if (ci_strcasecmp(argv[1], "on") == 0)
        val = true;
    else if (ci_strcasecmp(argv[1], "off") == 0)
        val = false;
    else
    {
        output_printf("invalid: on or off\r\n");
        return -1;
    }

    s_tdc_override_isd_active = true;
    s_tdc_override_isd_value  = val;
    output_printf("OK: ISD override = %s\r\n", val ? "on" : "off");
    return 0;
}

/* ======================================================================== */
/*  --map handler                                                           */
/* ======================================================================== */

static int handle_map(int argc, char *argv[])
{
    if (argc < 2) return -1;

    bool val;
    if (ci_strcasecmp(argv[1], "on") == 0)
        val = true;
    else if (ci_strcasecmp(argv[1], "off") == 0)
        val = false;
    else
    {
        output_printf("invalid: on or off\r\n");
        return -1;
    }

    s_tdc_override_map_active = true;
    s_tdc_override_map_value  = val;
    output_printf("OK: MAP override = %s\r\n", val ? "on" : "off");
    return 0;
}

/* ======================================================================== */
/*  --err handler                                                           */
/* ======================================================================== */

static int handle_error(int argc, char *argv[])
{
    if (argc < 2) return -1;

    if (ci_strcasecmp(argv[1], "clr") == 0)
    {
        clearAllErrorFlag();
        led_request(LED_SRC_ERROR, LED_ST_NONE);
        output_printf("OK: all error flags cleared\r\n");
        return 0;
    }

    if (ci_strcasecmp(argv[1], "data_logging") == 0)
    {
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_open, __LINE__);
        output_printf("OK: DATA_LOGGING error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "fpga") == 0)
    {
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__FPGA_ResetValueError, __LINE__);
        output_printf("OK: FPGA error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "acc") == 0)
    {
        errorCodeUpdate(en__ACCELEROMETER_ERROR, en__ACCELER_ResetValueError, __LINE__);
        output_printf("OK: ACC error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "pmic") == 0)
    {
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__NON_RESETTABLE, __LINE__);
        output_printf("OK: PMIC error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "mcu") == 0)
    {
        errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
        output_printf("OK: MCU error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "map") == 0)
    {
        errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
        output_printf("OK: MAP error injected\r\n");
    }
    else
    {
        output_printf("unknown type: data_logging|fpga|acc|pmic|mcu|map\r\n");
        return -1;
    }

    return 0;
}

/* ======================================================================== */
/*  --vol handler                                                           */
/* ======================================================================== */

static int handle_volume(int argc, char *argv[])
{
    if (argc < 2) return -1;

    int volume = 0;
    const char *p = argv[1];
    while (*p >= '0' && *p <= '9')
    {
        volume = volume * 10 + (*p - '0');
        p++;
    }

    if (*p != '\0' || volume < 1 || volume > 10)
    {
        output_printf("invalid: 1~10\r\n");
        return -1;
    }

    changeAudioVolume(volume);
    output_printf("OK: volume = %d\r\n", volume);
    return 0;
}

/* ======================================================================== */
/*  --init_all_map handler                                                  */
/* ======================================================================== */

static int handle_init_all_map(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ci_printi("[UI CMD] START TO ERASE ALL MAPS\r\n");
    ci_map_init_map_data_all(true);
    ci_printi("[UI CMD] FINISHED ERASING ALL MAPS\r\n");
    output_printf("OK: all maps erased\r\n");
    return 0;
}

/* ======================================================================== */
/*  --dump_log handler                                                      */
/* ======================================================================== */

static int handle_dump_log(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ci_printi("[UI CMD] START TO READ EVENT LOG\r\n");
    ci_event_log_read();
    ci_printi("[UI CMD] FINISHED TO READ EVENT LOG\r\n");
    output_printf("OK: event log dumped\r\n");
    return 0;
}

/* ======================================================================== */
/*  --write_integrity_err handler                                           */
/* ======================================================================== */

static int handle_write_integrity_error(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ci_event_log_write(CI_EVENT_LOG_TYPE_INTEGRITY_ERROR);
    output_printf("OK: integrity error logged\r\n");
    return 0;
}

/* ======================================================================== */
/*  Registration table                                                      */
/* ======================================================================== */

static const command_entry_t s_tdc_commands[] = {
    {"help",               handle_help,                   "--help"},
    {"led",                handle_led,                    "--led show|req|clr|user|pair|burst"},
    {"batt",               handle_battery,                "--batt show|<0~100>"},
    {"isd",                handle_isd,                    "--isd on|off"},
    {"map",                handle_map,                    "--map on|off"},
    {"err",                handle_error,                  "--err clr|data_logging|fpga|acc|pmic|mcu|map"},
    {"vol",                handle_volume,                 "--vol <1~10>"},
    {"init_all_map",       handle_init_all_map,           "--init_all_map"},
    {"dump_log",           handle_dump_log,               "--dump_log"},
    {"write_integrity_err",handle_write_integrity_error,  "--write_integrity_err"},
};

/* ======================================================================== */
/*  Tokenizer                                                               */
/* ======================================================================== */

static int tokenize(char *line, char *argv[], int max_argc)
{
    int argc = 0;
    char *p  = line;

    while (*p && argc < max_argc)
    {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;

        argv[argc++] = p;

        /* advance to next whitespace or end */
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }

    return argc;
}

/* ======================================================================== */
/*  Dispatch                                                                */
/* ======================================================================== */

static void dispatch(char *line)
{
    /* sentinel check: "--" */
    if (strncmp(line, SENTINEL, SENTINEL_LEN) != 0)
    {
        output_printf("unknown, type '--help'\r\n");
        return;
    }

    /* skip sentinel */
    char *command_line = &line[SENTINEL_LEN];

    char *argv[MAX_ARGC];
    int   argc = tokenize(command_line, argv, MAX_ARGC);

    if (argc == 0)
    {
        output_printf("unknown, type '--help'\r\n");
        return;
    }

    /* linear search */
    for (int i = 0; i < s_tdc_table_cnt; i++)
    {
        if (strcmp(argv[0], s_tdc_table[i].name) == 0)
        {
            int ret = s_tdc_table[i].fn(argc, argv);
            if (ret < 0)
            {
                output_printf("  usage: %s\r\n", s_tdc_table[i].help);
            }
            return;
        }
    }

    output_printf("unknown cmd '%s', type '--help'\r\n", argv[0]);
}

static void dispatch_literal(const char *literal)
{
    char buf[MAX_LINE];
    int  len = (int) strlen(literal);

    if (len >= MAX_LINE - SENTINEL_LEN - 1)
        len = MAX_LINE - SENTINEL_LEN - 1;

    memcpy(buf, SENTINEL, SENTINEL_LEN);
    memcpy(&buf[SENTINEL_LEN], literal, len);
    buf[SENTINEL_LEN + len] = '\0';

    dispatch(buf);
}

/* ======================================================================== */
/*  Public API                                                              */
/* ======================================================================== */

void tdc_ui_command_init(void)
{
    s_tdc_table_cnt = 0;
    s_tdc_line_len  = 0;

    for (int i = 0; i < (int)(sizeof(s_tdc_commands) / sizeof(s_tdc_commands[0])); i++)
    {
        register_command(&s_tdc_commands[i]);
    }

    output_printf("\r\n[UI CMD] ready. type '--help'\r\n");
    print_prompt();
}

void tdc_ui_command_poll(void)
{
    char ch;

    while (SEGGER_RTT_Read(0, &ch, 1) > 0)
    {
        if (ch == '\r' || ch == '\n')
        {
            echo_string("\r\n");

            if (s_tdc_line_len == 0)
            {
                /* 빈 라인 -> --led show 자동 */
                dispatch_literal("led show");
            }
            else
            {
                s_tdc_line[s_tdc_line_len] = '\0';
                dispatch(s_tdc_line);
                s_tdc_line_len = 0;
            }
            print_prompt();
        }
        else if (ch == 0x08 || ch == 0x7F) /* BS / DEL */
        {
            if (s_tdc_line_len > 0)
            {
                s_tdc_line_len--;
                echo_string("\b \b");
            }
        }
        else if (s_tdc_line_len < MAX_LINE - 1)
        {
            s_tdc_line[s_tdc_line_len++] = ch;
            echo_char(ch);
        }
    }
}
