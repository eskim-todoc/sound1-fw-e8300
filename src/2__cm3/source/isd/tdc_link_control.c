#include <tdc_link_control.h>
#include <tdc_link_config.h>
#include <tdc_shm.h>
#include <tdc_isd.h>
#include <tdc_isd_fpga.h>
#include <tdc_sys_error.h>
#include <tdc_printf.h>

#define TDC_LINK_MIN_LEVEL 112
#define TDC_LINK_HW_MAX_LEVEL 214
#define TDC_LINK_MAX_NOP 19

static int s_tdc_actual_level;
static bool s_tdc_level_valid;
static int s_tdc_force_level;
static bool s_tdc_infinite;
static bool s_tdc_nop_override;
static bool s_tdc_connected_once;
static int s_tdc_nop_default = 3;
static int s_tdc_coin_balance;
static unsigned s_tdc_fail_count;

int tdc_link_control_get_level(void) { return s_tdc_level_valid ? s_tdc_actual_level : 0; }
int tdc_link_control_get_force(void) { return s_tdc_force_level; }
int tdc_link_control_get_infinite(void) { return s_tdc_infinite ? 1 : 0; }
int tdc_link_control_get_nop(void) { return cfx_cm3_sharedMemoryAll.tdc_power_stable_nop_count; }
int tdc_link_control_get_override(void) { return s_tdc_nop_override ? 1 : 0; }

bool tdc_link_control_set_force(int level)
{
    if (level != 0 && (level < TDC_LINK_MIN_LEVEL || level > TDC_LINK_HW_MAX_LEVEL)) return false;
    s_tdc_force_level = level;
    return true;
}

void tdc_link_control_set_infinite(int enabled) { s_tdc_infinite = enabled != 0; }

bool tdc_link_control_set_nop(int count)
{
    if (count < 0 || count > TDC_LINK_MAX_NOP) return false;
    cfx_cm3_sharedMemoryAll.tdc_power_stable_nop_count = count;
    s_tdc_nop_override = true;
    return true;
}

void tdc_link_control_set_override(int enabled)
{
    s_tdc_nop_override = enabled != 0;
    if (!s_tdc_nop_override) cfx_cm3_sharedMemoryAll.tdc_power_stable_nop_count = s_tdc_nop_default;
}

void tdc_link_control_map_changed(int frame)
{
    static const unsigned char defaults[10] = {3, 3, 3, 3, 2, 4, 6, 1, 2, 3};
    s_tdc_nop_default = (frame >= 1 && frame <= 9) ? defaults[frame] : 3;
    tdc_link_control_set_override(0);
}

void tdc_link_control_note_level(int level) { s_tdc_actual_level = level; s_tdc_level_valid = true; }
void tdc_link_control_invalidate_level(void) { s_tdc_level_valid = false; }
void tdc_link_control_connected(void) { s_tdc_connected_once = true; }
int tdc_link_control_effective_max(void)
{
    return s_tdc_connected_once ? (int)tdc_link_config_get()->max_level : TDC_LINK_HW_MAX_LEVEL;
}
int tdc_link_control_period_ms(void) { return (int)tdc_link_config_get()->period_100ms * 100; }
void tdc_link_control_reset_feedback(void)
{
    s_tdc_coin_balance = 0;
    s_tdc_fail_count = 0;
}

static bool request_level(int current, int next, const char *reason)
{
    bool success = tdc_isd_fpga_write_change_tx_power_level(next);
    TDC_PRINTF_I("[LINK] %s %d->%d %s\r\n", reason, current, next, success ? "OK" : "FAIL");
    return success;
}

static bool automatic_level(int current, int next, const char *reason)
{
    if (s_tdc_force_level || s_tdc_infinite || current == next) return true;
    return request_level(current, next, reason);
}

static bool disconnect_link(int code)
{
    s_tdc_fail_count = 0;
    tdc_sys_error_update(en__EN__ISD_ERROR, code, __LINE__);
    tdc_isd_change_state(en__isdStatus_FPGA_Ok);
    return false;
}

bool tdc_link_control_feedback(int power_state, int current, bool mapping, bool dedicated)
{
    const tdc_link_config_t *config = tdc_link_config_get();
    int max_level = tdc_link_control_effective_max();
    int floor = (int)(mapping ? config->mapping_min_level : config->min_level);
    bool received = power_state >= ISD_Power_LowUnstable && power_state <= ISD_Power_HighStable;
    bool high = power_state == ISD_Power_HighStable || power_state == ISD_Power_HighUnstable;

    if (s_tdc_force_level)
    {
        if (!request_level(current, s_tdc_force_level, "FORCE")) return false;
        current = s_tdc_force_level;
    }
    if (mapping && current < floor && !s_tdc_force_level && !s_tdc_infinite)
    {
        if (!request_level(current, floor, "MAPPING FLOOR")) return false;
        current = floor;
    }

    /* Presence mode belongs only to normal Live; both mapping paths use the
     * power-state policy. Coins are deliberately absent from presence mode. */
    if (config->mode == 1 && !mapping && !dedicated)
    {
        if (received)
        {
            s_tdc_fail_count = 0;
            return automatic_level(current, current > floor ? current - 1 : current, "RX");
        }
        if (power_state == BackTelNumTooMuch)
        {
            s_tdc_fail_count = 0;
            return true;
        }
        if (power_state == NoBacktel)
        {
            if (current < max_level)
            {
                if (s_tdc_fail_count < 8) ++s_tdc_fail_count;
                int multiplier = config->accel ? (int)((s_tdc_fail_count + 1) / 2) : 1;
                int next = current + (int)config->step_up * multiplier;
                if (next > max_level) next = max_level;
                return automatic_level(current, next, "NO RX");
            }
            if (s_tdc_infinite) return true;
            return disconnect_link(en__BackTelCounterZero);
        }
        return true;
    }

    if (received)
    {
        if (!dedicated) s_tdc_coin_balance = (int)config->coin_count;
        if (high)
        {
            int next = current;
            if (current > floor) --next;
            else if (mapping && current < floor) ++next;
            return automatic_level(current, next, "HIGH");
        }
        int next = current + (int)config->step_up;
        if (next > max_level) next = max_level;
        return automatic_level(current, current < max_level ? next : current, "LOW");
    }
    if (power_state == NoBacktel || power_state == BackTelNumTooMuch)
    {
        if (!dedicated && (s_tdc_infinite || s_tdc_coin_balance > 0))
        {
            if (s_tdc_coin_balance > 0) --s_tdc_coin_balance;
            if (power_state == NoBacktel && !s_tdc_infinite && current < max_level)
            {
                int next = current + (int)config->step_up;
                if (next > max_level) next = max_level;
                return automatic_level(current, next, "RETRY");
            }
            return true;
        }
        return disconnect_link(power_state == NoBacktel ? en__BackTelCounterZero : en__BackterDataLengthError);
    }
    return true;
}

#if defined(TDC_UNIT_TEST)
void tdc_ut_link_control_exchange(tdc_ut_link_control_state_t *state)
{
    if (state == 0) return;
    tdc_ut_link_control_state_t previous = {
        s_tdc_actual_level, s_tdc_level_valid, s_tdc_force_level,
        s_tdc_infinite, s_tdc_nop_override, s_tdc_connected_once,
        s_tdc_nop_default, s_tdc_coin_balance, s_tdc_fail_count
    };
    s_tdc_actual_level = state->actual_level;
    s_tdc_level_valid = state->level_valid;
    s_tdc_force_level = state->force_level;
    s_tdc_infinite = state->infinite;
    s_tdc_nop_override = state->nop_override;
    s_tdc_connected_once = state->connected_once;
    s_tdc_nop_default = state->nop_default;
    s_tdc_coin_balance = state->coin_balance;
    s_tdc_fail_count = state->fail_count;
    *state = previous;
}
#endif
