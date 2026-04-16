/*
 * tdc_iqs323.c
 *
 * IQS323 터치센서 드라이버.
 * I2C 통신, RDY 윈도우 관리, 레지스터 설정, 초기화, 런타임 폴링을 포함.
 */

#include <tdc_iqs323.h>

/* **********************************************************************
 * Static state
 */
static int s_tdc_iqs323_tick_old        = 0;
static int s_tdc_iqs323_touch_state_old = TDC_IQS323_TOUCH_STATE_RESET;

/* **********************************************************************
 * I2C low-level
 */

static bool i2c_write(uint8_t *p_buf, int len)
{
    EN__I2C_DRIVER_STATE i2c_driver_state;
    static int           buf[3];

    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return false;
    }

    for (int i = 0; i < len; i++)
    {
        buf[i] = (int) p_buf[i];
    }

    i2c_startWriteData(TDC_IQS323_SLAVE_ADDR, &buf[0], len);

    while (1)
    {
        i2c_driver_state = get_i2cDriverStatus();

        if (i2c_driver_state == i2c_state_WritingDone)
        {
            setI2cDriverStatusIdle();
            break;
        }

        if (i2c_driver_state == i2c_state_Error)
        {
            init_I2c();
            return false;
        }
    }

    return true;
}

static bool i2c_read(uint8_t *p_buf, int len)
{
    EN__I2C_DRIVER_STATE i2c_driver_state;
    int                  buf[2];

    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return false;
    }

    i2c_startReadData(TDC_IQS323_SLAVE_ADDR, &buf[0], len);

    while (1)
    {
        i2c_driver_state = get_i2cDriverStatus();

        if (i2c_driver_state == i2c_state_ReadingDone)
        {
            setI2cDriverStatusIdle();
            break;
        }

        if (i2c_driver_state == i2c_state_Error)
        {
            init_I2c();
            return false;
        }
    }

    p_buf[0] = (uint8_t) (buf[0] & 0x000000FF);
    p_buf[1] = (uint8_t) (buf[1] & 0x000000FF);

    return true;
}

/* **********************************************************************
 * RDY window management
 */

static bool is_rdy_window_opened(void)
{
    return (Sys_GPIO_Read(TDC_IQS323_RDY_PIN) == TDC_IQS323_RDY_WINDOW_OPENED);
}

static bool wait_rdy_window_closed(int max_ms)
{
    int tick_old = ci_timer_get_tick();

    while (1)
    {
        if (max_ms <= (ci_timer_get_tick() - tick_old))
        {
            ci_printe("[TOUCH] TIMEOUT (%d MS) WAIT WINDOW CLOSE \r\n", max_ms);
            return false;
        }

        if (Sys_GPIO_Read(TDC_IQS323_RDY_PIN) == TDC_IQS323_RDY_WINDOW_CLOSED)
        {
            return true;
        }
    }
}

static bool force_window_open(void)
{
    uint8_t force_comm = 0xFF;

    if (Sys_GPIO_Read(TDC_IQS323_RDY_PIN) == TDC_IQS323_RDY_WINDOW_OPENED)
    {
        return true;
    }

    if (!i2c_write(&force_comm, 1))
    {
        return false;
    }

    int tick_old = ci_timer_get_tick();

    while (1)
    {
        if (TDC_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN <= (ci_timer_get_tick() - tick_old))
        {
            ci_printe("[TOUCH] TIMEOUT (%d MS) FORCE WINDOW OPEN \r\n", TDC_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN);
            return false;
        }

        if (Sys_GPIO_Read(TDC_IQS323_RDY_PIN) == TDC_IQS323_RDY_WINDOW_OPENED)
        {
            return true;
        }
    }
}

/* **********************************************************************
 * Register access helpers
 */

static bool write_register(uint8_t addr, uint8_t lsb, uint8_t msb)
{
    uint8_t buf[3] = {addr, lsb, msb};

    if (!force_window_open())
    {
        ci_printe("[TOUCH] WRITE 0x%02X: WINDOW OPEN FAIL \r\n", addr);
        return false;
    }

    if (!i2c_write(buf, 3))
    {
        ci_printe("[TOUCH] WRITE 0x%02X: I2C FAIL \r\n", addr);
        return false;
    }

    ci_printv("[TOUCH] WRITE 0x%02X: LSB=0x%02X MSB=0x%02X \r\n", addr, lsb, msb);

    wait_rdy_window_closed(TDC_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);
    return true;
}

static bool read_register(uint8_t addr, uint8_t *p_lsb, uint8_t *p_msb)
{
    uint8_t reg = addr;
    uint8_t buf[2];

    if (!force_window_open())
    {
        ci_printe("[TOUCH] READ 0x%02X: WINDOW OPEN FAIL (ADDR) \r\n", addr);
        return false;
    }

    if (!i2c_write(&reg, 1))
    {
        ci_printe("[TOUCH] READ 0x%02X: I2C WRITE ADDR FAIL \r\n", addr);
        return false;
    }

    wait_rdy_window_closed(TDC_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);

    if (!force_window_open())
    {
        ci_printe("[TOUCH] READ 0x%02X: WINDOW OPEN FAIL (DATA) \r\n", addr);
        return false;
    }

    if (!i2c_read(buf, 2))
    {
        ci_printe("[TOUCH] READ 0x%02X: I2C READ FAIL \r\n", addr);
        return false;
    }

    *p_lsb = buf[0];
    *p_msb = buf[1];

    wait_rdy_window_closed(TDC_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);
    return true;
}

static bool write_and_verify(uint8_t addr, uint8_t lsb, uint8_t msb)
{
    uint8_t read_lsb, read_msb;

    if (!write_register(addr, lsb, msb))
    {
        return false;
    }

    if (!read_register(addr, &read_lsb, &read_msb))
    {
        return false;
    }

    return true;
}

/* **********************************************************************
 * Long touch detection (소프트웨어 3초 타이머)
 */
static bool proc_touch(int state_now)
{
    static int  s_state_old        = TDC_IQS323_TOUCH_STATE_RESET;
    static int  s_tick_first_touch = 0;
    static bool s_is_long_touch    = false;

    int  tick_current;
    bool ret = false;

    switch (s_state_old)
    {
        case TDC_IQS323_TOUCH_STATE_RESET:
        {
            if (state_now == TDC_IQS323_TOUCH_STATE_TOUCH)
            {
                s_tick_first_touch = ci_timer_get_tick();
            }
        }
        break;

        case TDC_IQS323_TOUCH_STATE_TOUCH:
        {
            if (state_now == TDC_IQS323_TOUCH_STATE_TOUCH)
            {
                tick_current = ci_timer_get_tick();

                if (TDC_IQS323_LONG_TOUCH_MS <= (tick_current - s_tick_first_touch))
                {
                    if (!s_is_long_touch)
                    {
                        s_is_long_touch = true;
                        ret             = true;
                    }
                }
            }
            else if (state_now == TDC_IQS323_TOUCH_STATE_NOT_TOUCH)
            {
                s_is_long_touch = false;
            }
        }
        break;

        case TDC_IQS323_TOUCH_STATE_NOT_TOUCH:
        {
            if (state_now == TDC_IQS323_TOUCH_STATE_TOUCH)
            {
                s_tick_first_touch = ci_timer_get_tick();
            }
        }
        break;

        case TDC_IQS323_TOUCH_STATE_ATI_ERROR:
            break;

        default:
            break;
    }

    s_state_old = state_now;
    return ret;
}

/* **********************************************************************
 * MCLR hard reset
 *
 * DIO16(RDY/MCLR)을 OUTPUT으로 전환 → LOW 유지 → INPUT 복원.
 * 데이터시트: tTRIG(MCLR) ≥ 250ns, 내부 풀업 200kΩ.
 */
static void mclr_reset(void)
{
    Sys_DIO_Config(TDC_IQS323_RDY_PIN, TDC_IQS323_RDY_PIN_CFG_OUTPUT);
    Sys_GPIO_Set_Low(TDC_IQS323_RDY_PIN);

    Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * TDC_IQS323_MCLR_HOLD_MS);

    Sys_DIO_Config(TDC_IQS323_RDY_PIN, TDC_IQS323_RDY_PIN_CFG_INPUT);

    Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * TDC_IQS323_BOOT_WAIT_MS);
}

/* **********************************************************************
 * Register configuration steps
 */

/*
 * Auto-ATI 완료 대기
 *
 * MCLR 리셋 후 IQS323이 자체적으로 Auto-ATI를 수행한다.
 * ATI Active 플래그가 0이 될 때까지 폴링하여 Auto-ATI 완료를 확인한다.
 * Reset Event가 SET된 동안에는 ATI 중에도 통신 윈도우가 제공되므로
 * ACK Reset 전에 호출해야 한다.
 */
static bool wait_auto_ati_done(void)
{
    tdc_iqs323_reg_system_status_t status;
    uint8_t                        lsb, msb;

    for (int i = 0; i < 20; i++)
    {
        ci_printd("[TOUCH] AUTO-ATI CHECK: TRY %d \r\n", i + 1);

        if (!read_register(TDC_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * 50);  // 50ms
            continue;
        }

        if ((lsb == 0xEE) && (msb == 0xEE))
        {
            Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * 50);
            continue;
        }

        status.bytes[1] = lsb;
        status.bytes[2] = msb;

        if (status.elements.lsb.ati_active == 0)
        {
            ci_printv("[TOUCH] AUTO-ATI: DONE \r\n");
            return true;
        }

        Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * 50);  // 50ms
    }

    ci_printw("[TOUCH] AUTO-ATI: TIMEOUT (터치 중이면 정상) \r\n");
    return false;
}

static bool ack_reset_event(void)
{
    return write_register(TDC_IQS323_REG_ADDR_SYSTEM_CONTROL, 0x01, 0x00);
}

static bool confirm_reset_event(void)
{
    tdc_iqs323_reg_system_status_t status;
    uint8_t                        lsb, msb;

    for (int i = 0; i < 10; i++)
    {
        ci_printd("[TOUCH] CONFIRM RESET: TRY %d \r\n", i + 1);

        if (!read_register(TDC_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS);
            continue;
        }

        if ((lsb == 0xEE) && (msb == 0xEE))
        {
            ci_printw("[TOUCH] CONFIRM RESET: READ 0xEEEE \r\n");
            Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS);
            continue;
        }

        status.bytes[1] = lsb;
        status.bytes[2] = msb;

        if (status.elements.lsb.reset_event == TDC_IQS323_NO_RESET_EVENT)
        {
            ci_printv("[TOUCH] CONFIRM RESET: CLEARED \r\n");
            return true;
        }

        Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS);
    }

    return false;
}

static bool events_enable(void)
{
    tdc_iqs323_reg_events_enable_t reg;

    reg.elements.addr                    = TDC_IQS323_REG_ADDR_EVENTS_ENABLE;
    reg.elements.msb.reserved_bit8_to_15 = 0;
    reg.elements.lsb.reserved_bit7       = 0;
    reg.elements.lsb.ati_error           = TDC_IQS323_EVENT_ENABLE;
    reg.elements.lsb.reserved_bit5       = 0;
    reg.elements.lsb.ati_event           = TDC_IQS323_EVENT_ENABLE;
    reg.elements.lsb.power_event         = TDC_IQS323_EVENT_DISABLE;
    reg.elements.lsb.slider_event        = TDC_IQS323_EVENT_DISABLE;
    reg.elements.lsb.touch_event         = TDC_IQS323_EVENT_ENABLE;
    reg.elements.lsb.prox_event          = TDC_IQS323_EVENT_DISABLE;

    return write_and_verify(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}

static bool sensor_setup(void)
{
    tdc_iqs323_reg_sensor_setup_t reg;

    /* 공통: 모든 채널 비활성 기본값 */
    reg.bytes[1] = 0x00;
    reg.bytes[2] = 0x00;

    /* Sensor 2 비활성 */
    ci_printd("[TOUCH] SETUP SENSOR 2 (DISABLE) \r\n");
    if (!write_and_verify(TDC_IQS323_REG_ADDR_SENSOR2_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }

    /* Sensor 1 비활성 */
    ci_printd("[TOUCH] SETUP SENSOR 1 (DISABLE) \r\n");
    if (!write_and_verify(TDC_IQS323_REG_ADDR_SENSOR1_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }

    /* Sensor 0 활성 (CTx0 + Channel Enable) */
    reg.elements.msb.ctx0           = TDC_IQS323_CTX0_ENABLE;
    reg.elements.lsb.enable_channel = TDC_IQS323_CHANNEL_ENABLE;

    ci_printd("[TOUCH] SETUP SENSOR 0 (ENABLE) \r\n");
    if (!write_and_verify(TDC_IQS323_REG_ADDR_SENSOR0_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }

    return true;
}

static bool touch_settings(void)
{
    tdc_iqs323_reg_touch_settings_t reg;

    reg.elements.addr                 = TDC_IQS323_REG_ADDR_CH0_TOUCH_SETTINGS;
    reg.elements.lsb.touch_threshold  = TDC_IQS323_TOUCH_THRESHOLD_80;
    reg.elements.msb.touch_hysteresis = TDC_IQS323_TOUCH_HYSTERESIS_80;

    return write_and_verify(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}

static bool re_ati_trigger(void)
{
    tdc_iqs323_reg_system_control_t reg;

    reg.bytes[0]            = TDC_IQS323_REG_ADDR_SYSTEM_CONTROL;
    reg.bytes[1]            = 0x00;
    reg.bytes[2]            = 0x00;
    reg.elements.lsb.re_ati = TDC_IQS323_TRIGGER_RE_ATI;

    return write_register(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}

static bool wait_re_ati_done(void)
{
    tdc_iqs323_reg_system_status_t status;
    uint8_t                        lsb, msb;

    for (int i = 0; i < 10; i++)
    {
        ci_printd("[TOUCH] RE-ATI CHECK: TRY %d \r\n", i + 1);

        if (!read_register(TDC_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * 100);
            continue;
        }

        if ((lsb == 0xEE) && (msb == 0xEE))
        {
            ci_printw("[TOUCH] RE-ATI CHECK: READ 0xEEEE \r\n");
            Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * 100);
            continue;
        }

        status.bytes[1] = lsb;
        status.bytes[2] = msb;

        if (status.elements.lsb.ati_error == TDC_IQS323_ATI_ERROR)
        {
            ci_printe("[TOUCH] RE-ATI: ATI ERROR \r\n");
            return false;
        }

        if (status.elements.lsb.ati_event == TDC_IQS323_ATI_EVENT)
        {
            ci_printv("[TOUCH] RE-ATI: SUCCESS \r\n");
            return true;
        }

        Sys_Delay(TDC_IQS323_DEFAULT_DELAY_MS * 100);
    }

    ci_printe("[TOUCH] RE-ATI: TIMEOUT \r\n");
    return false;
}

/* **********************************************************************
 * Public API — Get touch state
 */
bool tdc_iqs323_get_touch_state(int *p_state)
{
    tdc_iqs323_reg_system_status_t status;
    uint8_t                        lsb, msb;

    if (!read_register(TDC_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
    {
        return false;
    }

    status.bytes[1] = lsb;
    status.bytes[2] = msb;

    if (status.elements.lsb.ati_error == TDC_IQS323_ATI_ERROR)
    {
        *p_state = TDC_IQS323_TOUCH_STATE_ATI_ERROR;
    }
    else if (status.elements.msb.ch0_touch == TDC_IQS323_CH0_IN_TOUCH)
    {
        *p_state = TDC_IQS323_TOUCH_STATE_TOUCH;
    }
    else
    {
        *p_state = TDC_IQS323_TOUCH_STATE_NOT_TOUCH;
    }

    return true;
}

/* **********************************************************************
 * ATI 보상값 고정 적용
 *
 * ATI 캘리브레이션을 실행하지 않고, 사전 측정된 보상값을 직접 쓴다.
 * 단순 터치/비터치 판정에서는 LTA가 환경을 자동 추적하므로
 * 고정 보상값으로 충분하며, ATI 실패/지연 위험이 없다.
 *
 * 보상값 확인 방법: TDC_IQS323_ATI_DUMP_ENABLE 1로 설정 후 빌드 → RTT 로그 확인
 */
#define TDC_IQS323_ATI_DUMP_ENABLE 0  /* 1: RE-ATI 실행 후 보상값 로그 출력 (개발용) */

/* 사전 측정된 Sensor 0 ATI 보상값 (고정 상수) */
#define TDC_IQS323_ATI_SETUP_LSB 0x08  /* ATI Resolution Factor + ATI Band + ATI Mode=Disabled(000) */
#define TDC_IQS323_ATI_SETUP_MSB 0x04
#define TDC_IQS323_ATI_MULT_LSB  0x82  /* Fine/Coarse Fractional Multiplier/Divider */
#define TDC_IQS323_ATI_MULT_MSB  0x5A
#define TDC_IQS323_ATI_COMP_LSB  0x00  /* Compensation Divider + Compensation */
#define TDC_IQS323_ATI_COMP_MSB  0x58

static bool write_ati_compensation(void)
{
    ci_printd("[TOUCH] WRITE ATI FIXED VALUES \r\n");

    if (!write_register(TDC_IQS323_REG_ADDR_SENSOR0_ATI_SETUP,
                        TDC_IQS323_ATI_SETUP_LSB, TDC_IQS323_ATI_SETUP_MSB))
    {
        return false;
    }

    if (!write_register(TDC_IQS323_REG_ADDR_SENSOR0_ATI_MULT,
                        TDC_IQS323_ATI_MULT_LSB, TDC_IQS323_ATI_MULT_MSB))
    {
        return false;
    }

    if (!write_register(TDC_IQS323_REG_ADDR_SENSOR0_ATI_COMP,
                        TDC_IQS323_ATI_COMP_LSB, TDC_IQS323_ATI_COMP_MSB))
    {
        return false;
    }

    return true;
}

#if TDC_IQS323_ATI_DUMP_ENABLE
static void dump_ati_registers(void)
{
    uint8_t lsb, msb;

    ci_printi("[TOUCH] === ATI REGISTER DUMP (Sensor 0) === \r\n");

    if (read_register(TDC_IQS323_REG_ADDR_SENSOR0_ATI_SETUP, &lsb, &msb))
    {
        ci_printi("[TOUCH] ATI_SETUP (0x36): LSB=0x%02X MSB=0x%02X \r\n", lsb, msb);
    }

    if (read_register(TDC_IQS323_REG_ADDR_SENSOR0_ATI_MULT, &lsb, &msb))
    {
        ci_printi("[TOUCH] ATI_MULT  (0x38): LSB=0x%02X MSB=0x%02X \r\n", lsb, msb);
    }

    if (read_register(TDC_IQS323_REG_ADDR_SENSOR0_ATI_COMP, &lsb, &msb))
    {
        ci_printi("[TOUCH] ATI_COMP  (0x39): LSB=0x%02X MSB=0x%02X \r\n", lsb, msb);
    }

    ci_printi("[TOUCH] === ATI DUMP END === \r\n");
}
#endif

/* **********************************************************************
 * Public API — Init
 *
 * MCLR 리셋 후 Reset Event SET 상태에서 바로 설정 진행.
 * ATI는 실행하지 않고 사전 측정된 보상값을 직접 쓴다.
 *
 * [ATI 덤프 모드] TDC_IQS323_ATI_DUMP_ENABLE=1 시:
 *   RE-ATI를 실행하고 결과 보상값을 로그로 출력.
 *   출력된 값을 TDC_IQS323_ATI_*_LSB/MSB 상수에 반영 후
 *   TDC_IQS323_ATI_DUMP_ENABLE=0으로 되돌린다.
 */
void tdc_iqs323_init(void)
{
    ci_timer_init(19); /* 약 1ms 타이머 */

    SYS_WATCHDOG_REFRESH();

    /* 1) MCLR 하드 리셋 → IQS323 POR, Reset Event SET, Auto-ATI 시작 */
    ci_printd("[TOUCH] MCLR HARD RESET \r\n");
    mclr_reset();

#if TDC_IQS323_ATI_DUMP_ENABLE
    /*
     * [덤프 모드] Auto-ATI 완료 대기 → ACK → 설정 → RE-ATI → 덤프.
     * Auto-ATI 실행 중에 센서 설정을 변경하면 ATI 엔진이 꼬이므로,
     * 덤프 모드에서는 Auto-ATI가 끝난 후 설정 → RE-ATI 순서를 따른다.
     * 1회 실행용이므로 시간은 무관.
     */
    ci_printd("[TOUCH] WAIT AUTO-ATI DONE (DUMP MODE) \r\n");
    if (!wait_auto_ati_done())
    {
        ci_printw("[TOUCH] WARN: AUTO-ATI TIMEOUT \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    ci_printd("[TOUCH] ACK RESET EVENT \r\n");
    if (!ack_reset_event())
    {
        ci_printe("[TOUCH] FAIL: ACK RESET EVENT \r\n");
    }

    ci_printd("[TOUCH] CONFIRM RESET EVENT \r\n");
    if (!confirm_reset_event())
    {
        ci_printe("[TOUCH] FAIL: CONFIRM RESET EVENT \r\n");
    }

    ci_printd("[TOUCH] SENSOR SETUP \r\n");
    if (!sensor_setup())
    {
        ci_printe("[TOUCH] FAIL: SENSOR SETUP \r\n");
    }

    ci_printd("[TOUCH] TOUCH SETTINGS \r\n");
    if (!touch_settings())
    {
        ci_printe("[TOUCH] FAIL: TOUCH SETTINGS \r\n");
    }

    ci_printd("[TOUCH] EVENTS ENABLE \r\n");
    if (!events_enable())
    {
        ci_printe("[TOUCH] FAIL: EVENTS ENABLE \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    ci_printd("[TOUCH] RE-ATI TRIGGER (DUMP MODE) \r\n");
    if (!re_ati_trigger())
    {
        ci_printe("[TOUCH] FAIL: RE-ATI TRIGGER \r\n");
    }

    {
        int tick_old = ci_timer_get_tick();
        while (50 > (ci_timer_get_tick() - tick_old)) {}
    }

    ci_printd("[TOUCH] RE-ATI DONE CHECK \r\n");
    if (!wait_re_ati_done())
    {
        ci_printe("[TOUCH] FAIL: RE-ATI DONE \r\n");
    }

    dump_ati_registers();

#else
    /*
     * [운용 모드] Auto-ATI 완료를 기다리지 않고 바로 설정 진행.
     * Reset Event SET 동안 ATI 중에도 통신 가능 (데이터시트).
     * ATI는 실행하지 않고 사전 측정된 보상값을 직접 쓴다.
     */
    ci_printd("[TOUCH] SENSOR SETUP \r\n");
    if (!sensor_setup())
    {
        ci_printe("[TOUCH] FAIL: SENSOR SETUP \r\n");
    }

    ci_printd("[TOUCH] TOUCH SETTINGS \r\n");
    if (!touch_settings())
    {
        ci_printe("[TOUCH] FAIL: TOUCH SETTINGS \r\n");
    }

    ci_printd("[TOUCH] EVENTS ENABLE \r\n");
    if (!events_enable())
    {
        ci_printe("[TOUCH] FAIL: EVENTS ENABLE \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    ci_printd("[TOUCH] ACK RESET EVENT \r\n");
    if (!ack_reset_event())
    {
        ci_printe("[TOUCH] FAIL: ACK RESET EVENT \r\n");
    }

    ci_printd("[TOUCH] CONFIRM RESET EVENT \r\n");
    if (!confirm_reset_event())
    {
        ci_printe("[TOUCH] FAIL: CONFIRM RESET EVENT \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    /* ATI 실행 없이 고정 보상값 적용 */
    if (!write_ati_compensation())
    {
        ci_printe("[TOUCH] FAIL: WRITE ATI COMPENSATION \r\n");
    }
#endif

    SYS_WATCHDOG_REFRESH();

    ci_printd("[TOUCH] INIT DONE \r\n");

    s_tdc_iqs323_tick_old        = ci_timer_get_tick();
    s_tdc_iqs323_touch_state_old = TDC_IQS323_TOUCH_STATE_RESET;

    ci_timer_uninit();
}

/* **********************************************************************
 * Public API — Process (100ms 폴링 + 롱터치 판정)
 *
 * 반환: true = 롱터치 이벤트 발생 (최초 1회)
 */
bool tdc_iqs323_process(void)
{
    int curr_touch_state;
    int curr_tick;

    curr_tick = ci_timer_get_tick();

    if (TDC_IQS323_POLL_INTERVAL <= (curr_tick - s_tdc_iqs323_tick_old))
    {
        s_tdc_iqs323_tick_old = curr_tick;

        if (tdc_iqs323_get_touch_state(&curr_touch_state))
        {
            if (s_tdc_iqs323_touch_state_old != curr_touch_state)
            {
                ci_printv("[TOUCH] STATE: %d -> %d \r\n", s_tdc_iqs323_touch_state_old, curr_touch_state);
                s_tdc_iqs323_touch_state_old = curr_touch_state;
            }
        }

        if (proc_touch(curr_touch_state))
        {
            ci_printi("\r\n[TOUCH] EVENT: LONG TOUCH \r\n");
            return true;
        }
    }

    return false;
}
