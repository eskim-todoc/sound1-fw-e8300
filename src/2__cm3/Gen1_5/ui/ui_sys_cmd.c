#include "ui_sys_cmd.h"
#include "ui_cmd.h"

#include <string.h>

#include "LedOutput.h"
#include "cfx_cm3_sharedMemory.h"
#include "error.h"
#include "ci_event_log.h"
#include "ci_map.h"

#include <ci_printf.h>

/* ======================================================================== */
/*  Override state                                                          */
/* ======================================================================== */

static bool    s_ovr_isd_active;
static bool    s_ovr_isd_value;

static bool    s_ovr_map_active;
static bool    s_ovr_map_value;

static bool    s_ovr_batt_active;
static uint8_t s_ovr_batt_pct;

/* ======================================================================== */
/*  Override hook getters (called from main.c / systemControl.c)            */
/* ======================================================================== */

bool ui_sys_ovr_isd_active(void)     { return s_ovr_isd_active; }
bool ui_sys_ovr_isd_value(void)      { return s_ovr_isd_value; }

bool ui_sys_ovr_map_active(void)     { return s_ovr_map_active; }
bool ui_sys_ovr_map_value(void)      { return s_ovr_map_value; }

bool ui_sys_ovr_batt_active(void)    { return s_ovr_batt_active; }
uint8_t ui_sys_ovr_batt_percent(void){ return s_ovr_batt_pct; }

/* setter (called from ui_led_cmd.c :batt handler) */
void ui_sys_ovr_set_batt(uint8_t pct)
{
    s_ovr_batt_active = true;
    s_ovr_batt_pct    = pct;
}

void ui_sys_ovr_clear_batt(void)
{
    s_ovr_batt_active = false;
}

/* ======================================================================== */
/*  Case-insensitive compare                                                */
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

/* ======================================================================== */
/*  :isd <0|1>                                                              */
/* ======================================================================== */

static int cmd_isd(int argc, char *argv[])
{
    if (argc < 2) return -1;

    int val = argv[1][0] - '0';
    if (val != 0 && val != 1)
    {
        ui_cmd_printf("invalid: 0 or 1\r\n");
        return -1;
    }

    s_ovr_isd_active = true;
    s_ovr_isd_value  = (bool) val;
    ui_cmd_printf("OK: ISD override = %d\r\n", val);
    return 0;
}

/* ======================================================================== */
/*  :map <0|1>                                                              */
/* ======================================================================== */

static int cmd_map(int argc, char *argv[])
{
    if (argc < 2) return -1;

    int val = argv[1][0] - '0';
    if (val != 0 && val != 1)
    {
        ui_cmd_printf("invalid: 0 or 1\r\n");
        return -1;
    }

    s_ovr_map_active = true;
    s_ovr_map_value  = (bool) val;
    ui_cmd_printf("OK: MAP override = %d\r\n", val);
    return 0;
}

/* ======================================================================== */
/*  :err <type> | :err clr                                                  */
/* ======================================================================== */

static int cmd_err(int argc, char *argv[])
{
    if (argc < 2) return -1;

    if (ci_strcasecmp(argv[1], "clr") == 0)
    {
        clearAllErrorFlag();
        led_request(LED_SRC_ERROR, LED_ST_NONE);
        ui_cmd_printf("OK: all error flags cleared\r\n");
        return 0;
    }

    if (ci_strcasecmp(argv[1], "data_logging") == 0)
    {
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_open, __LINE__);
        ui_cmd_printf("OK: DATA_LOGGING error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "fpga") == 0)
    {
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__FPGA_ResetValueError, __LINE__);
        ui_cmd_printf("OK: FPGA error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "acc") == 0)
    {
        errorCodeUpdate(en__ACCELEROMETER_ERROR, en__ACCELER_ResetValueError, __LINE__);
        ui_cmd_printf("OK: ACC error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "pmic") == 0)
    {
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__NON_RESETTABLE, __LINE__);
        ui_cmd_printf("OK: PMIC error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "mcu") == 0)
    {
        errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
        ui_cmd_printf("OK: MCU error injected\r\n");
    }
    else if (ci_strcasecmp(argv[1], "map") == 0)
    {
        errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
        ui_cmd_printf("OK: MAP error injected\r\n");
    }
    else
    {
        ui_cmd_printf("unknown type: data_logging|fpga|acc|pmic|mcu|map\r\n");
        return -1;
    }

    return 0;
}

/* ======================================================================== */
/*  :vol <n>  (1~10)                                                        */
/* ======================================================================== */

static int cmd_vol(int argc, char *argv[])
{
    if (argc < 2) return -1;

    int vol = 0;
    const char *p = argv[1];
    while (*p >= '0' && *p <= '9')
    {
        vol = vol * 10 + (*p - '0');
        p++;
    }

    if (*p != '\0' || vol < 1 || vol > 10)
    {
        ui_cmd_printf("invalid: 1~10\r\n");
        return -1;
    }

    changeAudioVolume(vol);
    ui_cmd_printf("OK: volume = %d\r\n", vol);
    return 0;
}

/* ======================================================================== */
/*  :init_all_map                                                           */
/* ======================================================================== */

static int cmd_init_all_map(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ci_printi("[UI CMD] START TO ERASE ALL MAPS\r\n");
    ci_map_init_map_data_all(true);
    ci_printi("[UI CMD] FINISHED ERASING ALL MAPS\r\n");
    ui_cmd_printf("OK: all maps erased\r\n");
    return 0;
}

/* ======================================================================== */
/*  :dump_log                                                               */
/* ======================================================================== */

static int cmd_dump_log(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ci_printi("[UI CMD] START TO READ EVENT LOG\r\n");
    ci_event_log_read();
    ci_printi("[UI CMD] FINISHED TO READ EVENT LOG\r\n");
    ui_cmd_printf("OK: event log dumped\r\n");
    return 0;
}

/* ======================================================================== */
/*  :write_integrity_err                                                    */
/* ======================================================================== */

static int cmd_write_integrity_err(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    ci_event_log_write(CI_EVENT_LOG_TYPE_INTEGRITY_ERROR);
    ui_cmd_printf("OK: integrity error logged\r\n");
    return 0;
}

/* ======================================================================== */
/*  Registration                                                            */
/* ======================================================================== */

static const ui_cmd_entry_t k_entries[] = {
    {"isd",                 cmd_isd,                 ":isd <0|1>"},
    {"map",                 cmd_map,                 ":map <0|1>"},
    {"err",                 cmd_err,                 ":err <type>|clr"},
    {"vol",                 cmd_vol,                 ":vol <1~10>"},
    {"init_all_map",        cmd_init_all_map,        ":init_all_map"},
    {"dump_log",            cmd_dump_log,             ":dump_log"},
    {"write_integrity_err", cmd_write_integrity_err, ":write_integrity_err"},
};

void ui_sys_cmd_register(void)
{
    for (int i = 0; i < (int)(sizeof(k_entries) / sizeof(k_entries[0])); i++)
    {
        ui_cmd_register(&k_entries[i]);
    }
}
