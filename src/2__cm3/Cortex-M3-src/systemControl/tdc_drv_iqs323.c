/*
 * tdc_drv_iqs323.c
 *
 * IQS323 터치센서 드라이버.
 * I2C 통신, RDY 윈도우 관리, 레지스터 설정, 저수준 공개 API 제공.
 * 상태머신·폴링·롱터치 판정 등 UX 로직은 tdc_touch.c 에 위치.
 */

#include <tdc_drv_iqs323.h>

static bool touch_settings_impl(uint8_t threshold, uint8_t hysteresis);

static bool g_tdc_iqs323_in_ulp_mode = false;

void tdc_set_iqs323_in_ulp_mode(void)
{
    g_tdc_iqs323_in_ulp_mode = true;
}

void tdc_clear_iqs323_in_ulp_mode(void)
{
    g_tdc_iqs323_in_ulp_mode = false;
}

bool tdc_is_iqs323_in_ulp_mode(void)
{
    return g_tdc_iqs323_in_ulp_mode;
}

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

    i2c_startWriteData(TDC_DRV_IQS323_SLAVE_ADDR, &buf[0], len);

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

    i2c_startReadData(TDC_DRV_IQS323_SLAVE_ADDR, &buf[0], len);

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
    return (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED);
}

static bool wait_rdy_window_closed(int max_ms)
{
    int tick_old = ci_timer_get_tick();

    while (1)
    {
        if (max_ms <= (ci_timer_get_tick() - tick_old))
        {
            /* write_register/read_register 끝에서 호출되는 이 대기는 반환값이
             * 사용되지 않는 soft delay. 여기서 타임아웃이 나도 다음 transaction
             * 직전의 force_window_open() 이 올바르게 복구한다. 에러가 아니라
             * 단순 상태 관찰 실패이므로 verbose 레벨로 유지. */
            ci_printv("[TOUCH] WAIT WINDOW CLOSE: soft timeout (%d ms) \r\n", max_ms);
            return false;
        }

        if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_CLOSED)
        {
            return true;
        }
    }
}

static bool force_window_open(void)
{
    uint8_t force_comm = 0xFF;

    if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED)
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
        if (TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN <= (ci_timer_get_tick() - tick_old))
        {
            ci_printe("[TOUCH] TIMEOUT (%d MS) FORCE WINDOW OPEN \r\n", TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN);
            return false;
        }

        if (Sys_GPIO_Read(TDC_DRV_IQS323_RDY_PIN) == TDC_DRV_IQS323_RDY_WINDOW_OPENED)
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

    wait_rdy_window_closed(TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);
    return true;
}

/* discharge_crx0() 전용 write ? 200ms 폴링마다 호출되므로 write 로그를
 * TDC_TOUCH_CRX0_DISCHARGE_LOG 로 제어해 RTT 범람을 막는다. write_register 와 동일하나
 * verbose write 로그만 플래그 조건부이며, 에러(ci_printe)는 항상 출력한다. */
static bool write_register_discharge(uint8_t addr, uint8_t lsb, uint8_t msb)
{
    uint8_t buf[3] = {addr, lsb, msb};

    if (!force_window_open())
    {
        ci_printe("[TOUCH] DISCHARGE WRITE 0x%02X: WINDOW OPEN FAIL \r\n", addr);
        return false;
    }

    if (!i2c_write(buf, 3))
    {
        ci_printe("[TOUCH] DISCHARGE WRITE 0x%02X: I2C FAIL \r\n", addr);
        return false;
    }

#if TDC_TOUCH_CRX0_DISCHARGE_LOG
    ci_printv("[TOUCH] DISCHARGE WRITE 0x%02X: LSB=0x%02X MSB=0x%02X \r\n", addr, lsb, msb);
#endif

    wait_rdy_window_closed(TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);
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

    wait_rdy_window_closed(TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);

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

    wait_rdy_window_closed(TDC_DRV_IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE);
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
 * MCLR hard reset
 *
 * DIO16(RDY/MCLR)을 OUTPUT으로 전환 → LOW 유지 → INPUT 복원.
 * 데이터시트: tTRIG(MCLR) ≥ 250ns, 내부 풀업 200kΩ.
 */
static void mclr_reset(void)
{
    Sys_DIO_Config(TDC_DRV_IQS323_RDY_PIN, TDC_DRV_IQS323_RDY_PIN_CFG_OUTPUT);
    Sys_GPIO_Set_Low(TDC_DRV_IQS323_RDY_PIN);

    Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * TDC_DRV_IQS323_MCLR_HOLD_MS);

    Sys_DIO_Config(TDC_DRV_IQS323_RDY_PIN, TDC_DRV_IQS323_RDY_PIN_CFG_INPUT);

    Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * TDC_DRV_IQS323_BOOT_WAIT_MS);
}

/* **********************************************************************
 * Register configuration steps
 */

/*
 * Auto-ATI 완료 여부 1회 확인 (논블로킹)
 *
 * 상위 레이어(tdc_touch)의 상태머신이 매 tick마다 호출한다.
 * 폴링 루프는 상위 레이어가 담당하므로 이 함수는 read 1회 + 판정만 수행.
 * 일시적 노이즈(0xEEEE)나 통신 실패는 false(= 아직 완료 아님) 로 처리하고
 * 다음 tick에서 재시도하게 한다.
 */
static bool is_auto_ati_done_single_read(void)
{
    tdc_drv_iqs323_reg_system_status_t status;
    uint8_t                            lsb, msb;

    if (!read_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
    {
        return false;
    }

    if ((lsb == 0xEE) && (msb == 0xEE))
    {
        return false;
    }

    status.bytes[1] = lsb;
    status.bytes[2] = msb;

    return (status.elements.lsb.ati_active == 0);
}

/*
 * Auto-ATI 완료 대기 (블로킹 루프 ? 덤프 모드 전용)
 *
 * MCLR 리셋 후 IQS323이 자체적으로 Auto-ATI를 수행한다.
 * ATI Active 플래그가 0이 될 때까지 폴링하여 Auto-ATI 완료를 확인한다.
 * Reset Event가 SET된 동안에는 ATI 중에도 통신 윈도우가 제공되므로
 * ACK Reset 전에 호출해야 한다.
 *
 * 운용 모드는 논블로킹 is_auto_ati_done_single_read()를 사용한다.
 */
static bool wait_auto_ati_done(void)
{
    tdc_drv_iqs323_reg_system_status_t status;
    uint8_t                            lsb, msb;

    for (int i = 0; i < 20; i++)
    {
        ci_printv("[TOUCH] AUTO-ATI CHECK: TRY %d \r\n", i + 1);

        if (!read_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 50);  // 50ms
            continue;
        }

        if ((lsb == 0xEE) && (msb == 0xEE))
        {
            Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 50);
            continue;
        }

        status.bytes[1] = lsb;
        status.bytes[2] = msb;

        if (status.elements.lsb.ati_active == 0)
        {
            ci_printv("[TOUCH] AUTO-ATI: DONE \r\n");
            return true;
        }

        Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 50);  // 50ms
    }

    ci_printw("[TOUCH] AUTO-ATI: TIMEOUT (터치 중이면 정상) \r\n");
    return false;
}

static bool ack_reset_event(void)
{
    return write_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL, 0x01, 0x00);
}

static bool confirm_reset_event(void)
{
    tdc_drv_iqs323_reg_system_status_t status;
    uint8_t                            lsb, msb;

    for (int i = 0; i < 10; i++)
    {
        ci_printv("[TOUCH] CONFIRM RESET: TRY %d \r\n", i + 1);

        if (!read_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS);
            continue;
        }

        if ((lsb == 0xEE) && (msb == 0xEE))
        {
            ci_printv("[TOUCH] CONFIRM RESET: READ 0xEEEE (retry) \r\n");
            Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS);
            continue;
        }

        status.bytes[1] = lsb;
        status.bytes[2] = msb;

        if (status.elements.lsb.reset_event == TDC_DRV_IQS323_NO_RESET_EVENT)
        {
            ci_printv("[TOUCH] CONFIRM RESET: CLEARED \r\n");
            return true;
        }

        Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS);
    }

    return false;
}

static bool events_enable(void)
{
    tdc_drv_iqs323_reg_events_enable_t reg;

    reg.elements.addr                    = TDC_DRV_IQS323_REG_ADDR_EVENTS_ENABLE;
    reg.elements.msb.reserved_bit8_to_15 = 0;
    reg.elements.lsb.reserved_bit7       = 0;
    reg.elements.lsb.ati_error           = TDC_DRV_IQS323_EVENT_ENABLE;
    reg.elements.lsb.reserved_bit5       = 0;
    reg.elements.lsb.ati_event           = TDC_DRV_IQS323_EVENT_ENABLE;
    reg.elements.lsb.power_event         = TDC_DRV_IQS323_EVENT_DISABLE;
    reg.elements.lsb.slider_event        = TDC_DRV_IQS323_EVENT_DISABLE;
    reg.elements.lsb.touch_event         = TDC_DRV_IQS323_EVENT_ENABLE;
    reg.elements.lsb.prox_event          = TDC_DRV_IQS323_EVENT_DISABLE;

    return write_and_verify(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}

static bool sensor_setup(void)
{
    tdc_drv_iqs323_reg_sensor_setup_t reg;

    /* Sensor 2 비활성 (Floating 고정) */
    reg.bytes[1] = TDC_DRV_IQS323_INACTIVE_RXS_FLOATING;
    reg.bytes[2] = 0x00;
    ci_printv("[TOUCH] SETUP SENSOR 2 (DISABLE) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR2_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }

    /* Sensor 1 ? CRX1 동작 모드 (REF_ENABLE·VSS_ENABLE·기본 세 가지 분기) */
#if TDC_TOUCH_CRX1_REF_ENABLE
    /* Reference 모드: CH1 활성, CH0 LTA 환경 드리프트 보정 (C52 있어도 동작) */
    reg.bytes[1]                    = 0x00;
    reg.bytes[2]                    = 0x00;
    reg.elements.msb.ctx0           = TDC_DRV_IQS323_CTX0_ENABLE;
    reg.elements.lsb.enable_channel = TDC_DRV_IQS323_CHANNEL_ENABLE;
    ci_printv("[TOUCH] SETUP SENSOR 1 (ENABLE, CRX1=REF) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR1_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }
    ci_printv("[TOUCH] SETUP CHANNEL 1 (REFERENCE) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_CHANNEL1_SETUP, TDC_DRV_IQS323_CH_MODE_REFERENCE, 0x00))
    {
        return false;
    }
#elif TDC_TOUCH_CRX1_VSS_ENABLE
    /* C52 단락(0Ω) 상태 ? CRX1을 IC 내부 VSS에 연결해 J4 패드 ESD 방전 경로 생성 */
    reg.bytes[1] = TDC_DRV_IQS323_INACTIVE_RXS_VSS;
    reg.bytes[2] = 0x00;
    ci_printv("[TOUCH] SETUP SENSOR 1 (DISABLE, CRX1=VSS) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR1_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }
#elif TDC_TOUCH_CRX1_DUMMY_ENABLE
    /* CalCap 더미 채널 ? 내부 CalCap을 변환 부하로 사용, 외부 CRX1 핀 완전 무관
     * (C52 유무·쇼트 무관). 목적: CH1 활성 유지 → CH0 discharge 시 measurement cycle 보존.
     * 0x43(Prox Input)은 쓰지 않음 ? reset 0x01CF가 reserved 비트·CalCap Select 올바름.
     * 상세: docs/참고/touch/IQS323-CalCap-더미채널.md */
    ci_printv("[TOUCH] SETUP PATTERN DEF 1 (CALCAP 1pF) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR1_PATTERN_DEF, TDC_DRV_IQS323_PATTERN_CALCAP_SIZE_1PF_LSB, TDC_DRV_IQS323_PATTERN_DEF_MSB))
    {
        return false;
    }
    /* Sensor1: enable + CalCap Rx/Tx 선택 (외부 CTx0 미사용 → reset의 ctx0=1 끔) */
    reg.bytes[1]                    = 0x00;
    reg.bytes[2]                    = 0x00;
    reg.elements.lsb.enable_channel = TDC_DRV_IQS323_CHANNEL_ENABLE;
    reg.elements.msb.cal_cap_tx     = 1;
    reg.elements.msb.cal_cap_rx     = 1;
    ci_printv("[TOUCH] SETUP SENSOR 1 (DUMMY, CALCAP RX/TX) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR1_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }
    /* CH1 ATI Mode=Disabled ? CalCap 부하 auto-ATI 수렴 실패로 인한 전역 ATI_ERROR 방지 */
    ci_printv("[TOUCH] SETUP SENSOR 1 ATI (DISABLED) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR1_ATI_SETUP, TDC_DRV_IQS323_DUMMY_ATI_SETUP_LSB, TDC_DRV_IQS323_DUMMY_ATI_SETUP_MSB))
    {
        return false;
    }
    ci_printv("[TOUCH] SETUP CHANNEL 1 (INDEPENDENT) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_CHANNEL1_SETUP, TDC_DRV_IQS323_CH_MODE_INDEPENDENT, 0x00))
    {
        return false;
    }
#else
    reg.bytes[1] = TDC_DRV_IQS323_INACTIVE_RXS_FLOATING;
    reg.bytes[2] = 0x00;
    ci_printv("[TOUCH] SETUP SENSOR 1 (DISABLE) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR1_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }
#endif

    /* Sensor 0 활성 (CTx0 + Channel Enable) ? 이전 채널 잔류값 초기화 후 설정 */
    reg.bytes[1]                    = 0x00;
    reg.bytes[2]                    = 0x00;
    reg.elements.msb.ctx0           = TDC_DRV_IQS323_CTX0_ENABLE;
    reg.elements.lsb.enable_channel = TDC_DRV_IQS323_CHANNEL_ENABLE;

    ci_printv("[TOUCH] SETUP SENSOR 0 (ENABLE) \r\n");
    if (!write_and_verify(TDC_DRV_IQS323_REG_ADDR_SENSOR0_SETUP, reg.bytes[1], reg.bytes[2]))
    {
        return false;
    }

    return true;
}

static bool touch_settings_impl(uint8_t threshold, uint8_t hysteresis)
{
    tdc_drv_iqs323_reg_touch_settings_t reg;

    reg.elements.addr                 = TDC_DRV_IQS323_REG_ADDR_CH0_TOUCH_SETTINGS;
    reg.elements.lsb.touch_threshold  = threshold;
    reg.elements.msb.touch_hysteresis = hysteresis;

    return write_and_verify(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}

static bool touch_settings(void)
{
    return touch_settings_impl(TDC_DRV_IQS323_TOUCH_THRESHOLD, TDC_DRV_IQS323_TOUCH_HYSTERESIS);
}

static bool re_ati_trigger(void)
{
    tdc_drv_iqs323_reg_system_control_t reg;

    reg.bytes[0]            = TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL;
    reg.bytes[1]            = 0x00;
    reg.bytes[2]            = 0x00;
    reg.elements.lsb.re_ati = TDC_DRV_IQS323_TRIGGER_RE_ATI;

    return write_register(reg.bytes[0], reg.bytes[1], reg.bytes[2]);
}

static bool wait_re_ati_done(void)
{
    tdc_drv_iqs323_reg_system_status_t status;
    uint8_t                            lsb, msb;

    for (int i = 0; i < 10; i++)
    {
        ci_printv("[TOUCH] RE-ATI CHECK: TRY %d \r\n", i + 1);

        if (!read_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 100);
            continue;
        }

        if ((lsb == 0xEE) && (msb == 0xEE))
        {
            ci_printv("[TOUCH] RE-ATI CHECK: READ 0xEEEE (retry) \r\n");
            Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 100);
            continue;
        }

        status.bytes[1] = lsb;
        status.bytes[2] = msb;

        if (status.elements.lsb.ati_error == TDC_DRV_IQS323_ATI_ERROR)
        {
            ci_printe("[TOUCH] RE-ATI: ATI ERROR \r\n");
            return false;
        }

        if (status.elements.lsb.ati_event == TDC_DRV_IQS323_ATI_EVENT)
        {
            ci_printv("[TOUCH] RE-ATI: SUCCESS \r\n");
            return true;
        }

        Sys_Delay(TDC_DRV_IQS323_DEFAULT_DELAY_MS * 100);
    }

    ci_printe("[TOUCH] RE-ATI: TIMEOUT \r\n");
    return false;
}

/* **********************************************************************
 * ATI 보상값 고정 적용
 *
 * ATI 캘리브레이션을 실행하지 않고, 사전 측정된 보상값을 직접 쓴다.
 * 단순 터치/비터치 판정에서는 LTA가 환경을 자동 추적하므로
 * 고정 보상값으로 충분하며, ATI 실패/지연 위험이 없다.
 *
 * 보상값 확인 방법: TDC_DRV_IQS323_ATI_DUMP_ENABLE 1로 설정 후 빌드 → RTT 로그 확인
 */
/* 보드 변종별 IQS323 Sensor 0 ATI 보상값 (사전 측정, ATI Mode=Disabled 고정)
 *
 *  변종         | ATI_MULT_MSB | ATI_COMP_LSB | ATI_COMP_MSB | 기준
 *  -------------|--------------|--------------|--------------|----------------------------
 *  MINI         | 0x5A         | 0x00         | 0x58         | 초기 측정값
 *  DEVELOP      | 0x62         | 0xFF         | 0x53         | 초기 측정값
 *  PACKAGE      | 0x5E         | 0xE4         | 0x63         | 정상 노터치 실측 (2026-06-08)
 *
 *  (ATI_SETUP_LSB/MSB, ATI_MULT_LSB 는 3개 변종 동일)
 *
 *  ATI_SETUP_LSB 0x08: bits[2:0]=000 → ATI Mode=Disabled
 *    운용 중 stuck-touch → auto-reATI → ATI_ERROR → I2C 무응답 경로 차단.
 *    CH timeout 비활성화와 이중 방어.
 *
 *  절전 모드 전환 시 tdc_drv_iqs323_apply_sleep_settings() 로 고정 보상값 직접 설정 (autoATI 없음).
 *  TDC_BOARD_VARIANT 선택: tdc_touch_config.h 참조 */
#if (TDC_BOARD_VARIANT == TDC_BOARD_VARIANT_MINI)
#define TDC_DRV_IQS323_ATI_SETUP_LSB 0x08 /* ATI Resolution Factor + ATI Band=1 + ATI Mode=Disabled(000) */
#define TDC_DRV_IQS323_ATI_SETUP_MSB 0x04
#define TDC_DRV_IQS323_ATI_MULT_LSB  0x82
#define TDC_DRV_IQS323_ATI_MULT_MSB  0x5A
#define TDC_DRV_IQS323_ATI_COMP_LSB  0x00
#define TDC_DRV_IQS323_ATI_COMP_MSB  0x58
#elif (TDC_BOARD_VARIANT == TDC_BOARD_VARIANT_DEVELOP)
#define TDC_DRV_IQS323_ATI_SETUP_LSB 0x08
#define TDC_DRV_IQS323_ATI_SETUP_MSB 0x04
#define TDC_DRV_IQS323_ATI_MULT_LSB  0x82
#define TDC_DRV_IQS323_ATI_MULT_MSB  0x62
#define TDC_DRV_IQS323_ATI_COMP_LSB  0xFF
#define TDC_DRV_IQS323_ATI_COMP_MSB  0x53
#elif (TDC_BOARD_VARIANT == TDC_BOARD_VARIANT_PACKAGE)
#define TDC_DRV_IQS323_ATI_SETUP_LSB 0x08
#define TDC_DRV_IQS323_ATI_SETUP_MSB 0x04
#define TDC_DRV_IQS323_ATI_MULT_LSB  0x82
#define TDC_DRV_IQS323_ATI_MULT_MSB  0x6E//0x5E /* 정상 노터치 실측 (2026-06-08) */
#define TDC_DRV_IQS323_ATI_COMP_LSB  0xE4 /* 정상 노터치 실측 (2026-06-08) */
#define TDC_DRV_IQS323_ATI_COMP_MSB  0x63 /* 정상 노터치 실측 (2026-06-08) */
#else
#error "TDC_BOARD_VARIANT 미지원 값. TDC_BOARD_VARIANT_MINI 또는 TDC_BOARD_VARIANT_DEVELOP 만 허용."
#endif

/* 절전 환경 ATI 고정 보상값 ? 주변장치 OFF + 저속 클럭 기준 실측값. 보드 변종 무관 공통. */
#define TDC_DRV_IQS323_SLEEP_ATI_SETUP_LSB 0x08
#define TDC_DRV_IQS323_SLEEP_ATI_SETUP_MSB 0x04
#define TDC_DRV_IQS323_SLEEP_ATI_MULT_LSB  0x82
#define TDC_DRV_IQS323_SLEEP_ATI_MULT_MSB  0x62//0x5E//0x5C /* MULT=0x5C82 ? 절전 실측 */
#define TDC_DRV_IQS323_SLEEP_ATI_COMP_LSB  0xE4//0x00
#define TDC_DRV_IQS323_SLEEP_ATI_COMP_MSB  0x63//0x60 /* COMP=0x6000 ? 절전 실측 */

static bool write_ati_compensation(void)
{
    ci_printv("[TOUCH] WRITE ATI FIXED VALUES \r\n");

    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP, TDC_DRV_IQS323_ATI_SETUP_LSB, TDC_DRV_IQS323_ATI_SETUP_MSB))
    {
        return false;
    }

    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT, TDC_DRV_IQS323_ATI_MULT_LSB, TDC_DRV_IQS323_ATI_MULT_MSB))
    {
        return false;
    }

    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP, TDC_DRV_IQS323_ATI_COMP_LSB, TDC_DRV_IQS323_ATI_COMP_MSB))
    {
        return false;
    }

    return true;
}

#if TDC_DRV_IQS323_ATI_DUMP_ENABLE
static void dump_ati_registers(void)
{
    uint8_t lsb, msb;

    ci_printi("[TOUCH] === ATI REGISTER DUMP (Sensor 0) === \r\n");

    if (read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP, &lsb, &msb))
    {
        ci_printi("[TOUCH] ATI_SETUP (0x36): LSB=0x%02X MSB=0x%02X \r\n", lsb, msb);
    }

    if (read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT, &lsb, &msb))
    {
        ci_printi("[TOUCH] ATI_MULT  (0x38): LSB=0x%02X MSB=0x%02X \r\n", lsb, msb);
    }

    if (read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP, &lsb, &msb))
    {
        ci_printi("[TOUCH] ATI_COMP  (0x39): LSB=0x%02X MSB=0x%02X \r\n", lsb, msb);
    }

    ci_printi("[TOUCH] === ATI DUMP END === \r\n");
}
#endif

/* **********************************************************************
 * ATI Calibration ? TDC_TOUCH_ATI_CALIB_MODE 빌드 전용
 */
#if TDC_TOUCH_ATI_CALIB_MODE

const tdc_drv_iqs323_calib_candidate_t tdc_drv_iqs323_calib_candidates[TDC_DRV_IQS323_CALIB_CANDIDATE_COUNT] = {
    /* mult_lsb  mult_msb  comp_lsb  comp_msb */
    {0x82, 0x5C, 0x00, 0x48}, /* 1: MINI 이하       */
    {0x82, 0x5E, 0xFF, 0x53}, /* 2: ?DEVELOP        */
    {0x82, 0x5A, 0x00, 0x58}, /* 3: ?MINI           */
    {0x82, 0x5C, 0x00, 0x60}, /* 4                  */
    {0x82, 0x5C, 0x00, 0x68}, /* 5                  */
    {0x82, 0x5C, 0x00, 0x70}, /* 6                  */
    {0x82, 0x5E, 0xFF, 0x73}, /* 7: ?PACKAGE        */
    {0x82, 0x5A, 0x00, 0x80}, /* 8                  */
    {0x82, 0x58, 0x00, 0x90}, /* 9                  */
    {0x82, 0x56, 0x00, 0xA0}, /* 10: 고용량 조립     */
};

uint8_t tdc_drv_iqs323_calib_find_candidate(void)
{
    uint8_t mult_lsb, mult_msb, comp_lsb, comp_msb;

    if (!read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT, &mult_lsb, &mult_msb))
    {
        ci_printe("[TOUCH] CALIB: MULT READ FAIL \r\n");
        return 0;
    }
    if (!read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP, &comp_lsb, &comp_msb))
    {
        ci_printe("[TOUCH] CALIB: COMP READ FAIL \r\n");
        return 0;
    }

    ci_printi("[TOUCH] CALIB READ ? MULT: LSB=0x%02X MSB=0x%02X  COMP: LSB=0x%02X MSB=0x%02X \r\n", mult_lsb, mult_msb, comp_lsb, comp_msb);

    uint32_t comp16_actual = ((uint32_t) comp_msb << 8) | comp_lsb;
    uint8_t  best_idx      = 1;
    uint32_t best_dist     = UINT32_MAX;

    for (uint8_t i = 0; i < TDC_DRV_IQS323_CALIB_CANDIDATE_COUNT; i++)
    {
        const tdc_drv_iqs323_calib_candidate_t *c           = &tdc_drv_iqs323_calib_candidates[i];
        uint32_t                                comp16_cand = ((uint32_t) c->comp_msb << 8) | c->comp_lsb;
        int32_t                                 dc          = (int32_t) comp16_actual - (int32_t) comp16_cand;
        int32_t                                 dm          = (int32_t) mult_msb - (int32_t) c->mult_msb;
        uint32_t                                dist        = (uint32_t) (dc * dc) + (uint32_t) (dm * dm * 256);

        if (dist < best_dist)
        {
            best_dist = dist;
            best_idx  = i + 1;
        }
    }

    ci_printi("[TOUCH] CALIB: Best candidate = %d \r\n", best_idx);
    return best_idx;
}

bool tdc_drv_iqs323_calib_read_ati(uint16_t *p_mult, uint16_t *p_comp)
{
    uint8_t lsb, msb;

    if (!read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT, &lsb, &msb))
    {
        ci_printe("[TOUCH] CALIB: MULT READ FAIL \r\n");
        return false;
    }
    *p_mult = ((uint16_t) msb << 8) | lsb;

    if (!read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP, &lsb, &msb))
    {
        ci_printe("[TOUCH] CALIB: COMP READ FAIL \r\n");
        return false;
    }
    *p_comp = ((uint16_t) msb << 8) | lsb;

    ci_printw("[TOUCH] CALIB ? MULT=0x%04X  COMP=0x%04X \r\n", *p_mult, *p_comp);
    return true;
}

bool tdc_drv_iqs323_calib_re_ati(void)
{
    if (!re_ati_trigger())
    {
        ci_printe("[TOUCH] CALIB: RE-ATI TRIGGER FAIL \r\n");
        return false;
    }

    /* 안정화 대기 */
    {
        int tick_old = ci_timer_get_tick();
        while (50 > (ci_timer_get_tick() - tick_old))
        {
            SYS_WATCHDOG_REFRESH();
        }
    }

    /* 완료 대기 ? ati_event 확인 (ati_active==0 만으로는 미실행도 완료로 오판) */
    if (!wait_re_ati_done())
    {
        ci_printw("[TOUCH] CALIB: RE-ATI TIMEOUT \r\n");
        return false;
    }

    return true;
}

#endif /* TDC_TOUCH_ATI_CALIB_MODE */

/* **********************************************************************
 * Sleep Measure Mode ? TDC_TOUCH_SLEEP_MEASURE_MODE 빌드 전용
 */

#if TDC_TOUCH_SLEEP_MEASURE_MODE

void tdc_drv_iqs323_sleep_measure_dump(void)
{
    uint8_t lsb, msb;

    /* ATI Mode=Full(bits[2:0]=100) ? Disabled 상태에서는 re-ATI가 실행되지 않음 */
    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP, 0x0C, TDC_DRV_IQS323_ATI_SETUP_MSB))
    {
        ci_printe("[SLEEP-MEAS] FAIL: ATI SETUP FULL\r\n");
    }

    if (!re_ati_trigger())
    {
        ci_printe("[SLEEP-MEAS] FAIL: RE-ATI TRIGGER\r\n");
        return;
    }

    if (!wait_re_ati_done())
    {
        ci_printw("[SLEEP-MEAS] WARN: RE-ATI TIMEOUT\r\n");
    }

    ci_printi("[SLEEP-MEAS] ===========================\r\n");

    if (read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP, &lsb, &msb))
        ci_printi("[SLEEP-MEAS] ATI_SETUP(0x36): LSB=0x%02X MSB=0x%02X\r\n", lsb, msb);

    if (read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT, &lsb, &msb))
        ci_printi("[SLEEP-MEAS] ATI_MULT (0x38): LSB=0x%02X MSB=0x%02X\r\n", lsb, msb);

    if (read_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP, &lsb, &msb))
        ci_printi("[SLEEP-MEAS] ATI_COMP (0x39): LSB=0x%02X MSB=0x%02X\r\n", lsb, msb);

    ci_printi("[SLEEP-MEAS] ===========================\r\n");
}

#endif /* TDC_TOUCH_SLEEP_MEASURE_MODE */

/* **********************************************************************
 * Public API ? 저수준 드라이버 인터페이스
 *
 * 기능 레이어(tdc_touch) 가 상태머신 단계를 조립할 때 사용.
 * 고수준 UX 로직(상태머신·폴링·롱터치) 은 tdc_touch.c 에 위치.
 */

void tdc_drv_iqs323_mclr_reset(void)
{
    mclr_reset();
}

bool tdc_drv_iqs323_is_auto_ati_done(void)
{
    return is_auto_ati_done_single_read();
}

void tdc_drv_iqs323_reseed(void)
{
    /* 절전 터치 감도 적용 (THRESHOLD/HYSTERESIS 낮춤) */
    if (!touch_settings_impl(TDC_DRV_IQS323_SLEEP_TOUCH_THRESHOLD, TDC_DRV_IQS323_SLEEP_TOUCH_HYSTERESIS))
    {
        ci_printe("[TOUCH] FAIL: SLEEP TOUCH SETTINGS \r\n");
    }

    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL, 0x08, 0x00))
    {
        ci_printe("[TOUCH] FAIL: RESEED \r\n");
    }
}

bool tdc_drv_iqs323_discharge_crx0(void)
{
    /* CH0 비활성 + CRX0=VSS: 누적 ESD 전하 방전 (CRX1은 현재 설정 유지).
     * verify(read-back) 불필요 ? 방전은 best-effort이며 200ms 폴링 통신 부하를 줄인다. */
    if (!write_register_discharge(TDC_DRV_IQS323_REG_ADDR_SENSOR0_SETUP, TDC_DRV_IQS323_INACTIVE_RXS_CRX0_VSS, 0x00))
    {
        return false;
    }
    /* CH0 복원: enable_channel=1, ctx0=1 ? sensor_setup() 의 Sensor0 설정과 동일 */
    if (!write_register_discharge(TDC_DRV_IQS323_REG_ADDR_SENSOR0_SETUP, 0x01, 0x01))
    {
        return false;
    }
    return true;
}

bool tdc_drv_iqs323_apply_sleep_settings(void)
{
    SYS_WATCHDOG_REFRESH();

#if 1
    /* 절전 터치 감도 적용 (세팅 중에 반응 없게 큰 값 설정) */
    if (!touch_settings_impl(255, 255))
    {
        ci_printe("[TOUCH] FAIL: SLEEP TOUCH SETTINGS \r\n");
        return false;
    }
#else
    /* 절전 터치 감도 적용 (THRESHOLD/HYSTERESIS 낮춤) */
    if (!touch_settings_impl(TDC_DRV_IQS323_SLEEP_TOUCH_THRESHOLD, TDC_DRV_IQS323_SLEEP_TOUCH_HYSTERESIS))
    {
        ci_printe("[TOUCH] FAIL: SLEEP TOUCH SETTINGS \r\n");
        return false;
    }
#endif

    /* 절전 환경 고정 ATI 보상값 적용 ? autoATI 없음 */
    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_SETUP, TDC_DRV_IQS323_SLEEP_ATI_SETUP_LSB, TDC_DRV_IQS323_SLEEP_ATI_SETUP_MSB))
    {
        ci_printe("[TOUCH] FAIL: SLEEP ATI SETUP \r\n");
        return false;
    }

    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_MULT, TDC_DRV_IQS323_SLEEP_ATI_MULT_LSB, TDC_DRV_IQS323_SLEEP_ATI_MULT_MSB))
    {
        ci_printe("[TOUCH] FAIL: SLEEP ATI MULT \r\n");
        return false;
    }

    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SENSOR0_ATI_COMP, TDC_DRV_IQS323_SLEEP_ATI_COMP_LSB, TDC_DRV_IQS323_SLEEP_ATI_COMP_MSB))
    {
        ci_printe("[TOUCH] FAIL: SLEEP ATI COMP \r\n");
        return false;
    }

    /* CH0~CH2 stuck-touch timeout 비활성화 */
    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL, 0x00, 0x07))
    {
        ci_printe("[TOUCH] FAIL: SLEEP CH TIMEOUT DISABLE \r\n");
        return false;
    }

    tdc_set_iqs323_in_ulp_mode();

    SYS_WATCHDOG_REFRESH();
    return true;
}

void tdc_drv_iqs323_apply_settings(void)
{
    SYS_WATCHDOG_REFRESH();

#if TDC_DRV_IQS323_ATI_DUMP_ENABLE
    /* [덤프 모드] ACK → 설정 → RE-ATI → 덤프 */
    ci_printv("[TOUCH] ACK RESET EVENT \r\n");
    if (!ack_reset_event())
    {
        ci_printe("[TOUCH] FAIL: ACK RESET EVENT \r\n");
    }

    ci_printv("[TOUCH] CONFIRM RESET EVENT \r\n");
    if (!confirm_reset_event())
    {
        ci_printe("[TOUCH] FAIL: CONFIRM RESET EVENT \r\n");
    }

    ci_printv("[TOUCH] SENSOR SETUP \r\n");
    if (!sensor_setup())
    {
        ci_printe("[TOUCH] FAIL: SENSOR SETUP \r\n");
    }

    ci_printv("[TOUCH] TOUCH SETTINGS \r\n");
    if (!touch_settings())
    {
        ci_printe("[TOUCH] FAIL: TOUCH SETTINGS \r\n");
    }

    ci_printv("[TOUCH] EVENTS ENABLE \r\n");
    if (!events_enable())
    {
        ci_printe("[TOUCH] FAIL: EVENTS ENABLE \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    ci_printv("[TOUCH] RE-ATI TRIGGER (DUMP MODE) \r\n");
    if (!re_ati_trigger())
    {
        ci_printe("[TOUCH] FAIL: RE-ATI TRIGGER \r\n");
    }

    {
        int tick_old = ci_timer_get_tick();
        while (50 > (ci_timer_get_tick() - tick_old))
        {
        }
    }

    ci_printv("[TOUCH] RE-ATI DONE CHECK \r\n");
    if (!wait_re_ati_done())
    {
        ci_printe("[TOUCH] FAIL: RE-ATI DONE \r\n");
    }

    dump_ati_registers();

#else
    /* [운용 모드] ACK → 설정 → 고정 보상값 → Reseed */
    ci_printv("[TOUCH] ACK RESET EVENT \r\n");
    if (!ack_reset_event())
    {
        ci_printe("[TOUCH] FAIL: ACK RESET EVENT \r\n");
    }

    ci_printv("[TOUCH] CONFIRM RESET EVENT \r\n");
    if (!confirm_reset_event())
    {
        ci_printe("[TOUCH] FAIL: CONFIRM RESET EVENT \r\n");
    }

    ci_printv("[TOUCH] SENSOR SETUP \r\n");
    if (!sensor_setup())
    {
        ci_printe("[TOUCH] FAIL: SENSOR SETUP \r\n");
    }

    ci_printv("[TOUCH] TOUCH SETTINGS \r\n");
    if (!touch_settings())
    {
        ci_printe("[TOUCH] FAIL: TOUCH SETTINGS \r\n");
    }

    ci_printv("[TOUCH] EVENTS ENABLE \r\n");
    if (!events_enable())
    {
        ci_printe("[TOUCH] FAIL: EVENTS ENABLE \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    if (!write_ati_compensation())
    {
        ci_printe("[TOUCH] FAIL: WRITE ATI COMPENSATION \r\n");
    }

    ci_printv("[TOUCH] RESEED \r\n");
    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL, 0x08, 0x00))
    {
        ci_printe("[TOUCH] FAIL: RESEED \r\n");
    }

    /* CH0~CH2 stuck-touch timeout 비활성화 ? auto-reATI 트리거 경로 차단
     * ATI Mode=Disabled(0x08)와 이중 방어. MSB bit[0..2] = ch0/ch1/ch2_timeout_disable */
    ci_printv("[TOUCH] DISABLE CH TIMEOUT \r\n");
    if (!write_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_CONTROL, 0x00, 0x07))
    {
        ci_printe("[TOUCH] FAIL: DISABLE CH TIMEOUT \r\n");
    }
#endif

    tdc_clear_iqs323_in_ulp_mode();

    SYS_WATCHDOG_REFRESH();
}

bool tdc_drv_iqs323_read_status(bool *p_pressed, bool *p_ati_error)
{
    tdc_drv_iqs323_reg_system_status_t status;
    uint8_t                            lsb, msb;

    if (!read_register(TDC_DRV_IQS323_REG_ADDR_SYSTEM_STATUS, &lsb, &msb))
    {
        return false;
    }

    status.bytes[1] = lsb;
    status.bytes[2] = msb;

    *p_ati_error = (status.elements.lsb.ati_error == TDC_DRV_IQS323_ATI_ERROR);
    *p_pressed   = (status.elements.msb.ch0_touch == TDC_DRV_IQS323_CH0_IN_TOUCH);

    return true;
}

bool tdc_drv_iqs323_read_touch_margin(uint8_t *p_threshold_coeff, uint16_t *p_current_coeff)
{
    uint8_t  lsb, msb;
    uint8_t  threshold;
    uint16_t lta, counts, current;

    /* Touch Settings(0x62): LSB=Touch Threshold 계수, MSB=Touch Hysteresis 계수 */
    if (!read_register(TDC_DRV_IQS323_REG_ADDR_CH0_TOUCH_SETTINGS, &lsb, &msb))
    {
        ci_printe("[TOUCH] MARGIN: TOUCH SETTINGS READ FAIL \r\n");
        return false;
    }
    threshold = lsb;

    /* CH0 LTA(0x14) ? 16bit little-endian */
    if (!read_register(TDC_DRV_IQS323_REG_ADDR_CH0_LTA, &lsb, &msb))
    {
        ci_printe("[TOUCH] MARGIN: LTA READ FAIL \r\n");
        return false;
    }
    lta = ((uint16_t) msb << 8) | lsb;

    /* CH0 Filtered Counts(0x13) ? 16bit little-endian */
    if (!read_register(TDC_DRV_IQS323_REG_ADDR_CH0_FILTERED_COUNTS, &lsb, &msb))
    {
        ci_printe("[TOUCH] MARGIN: COUNTS READ FAIL \r\n");
        return false;
    }
    counts = ((uint16_t) msb << 8) | lsb;

    /* current_coeff = (LTA-Counts)×256/LTA ? threshold 계수와 같은 단위.
     * div0·underflow 방어 (self-cap: 접촉 시 Counts<LTA, 노이즈로 역전 시 0 처리).
     * (LTA-Counts)×256 는 24bit 까지 가므로 uint32 로 승격해 계산한다. */
    if (lta == 0 || counts >= lta)
    {
        current = 0;
    }
    else
    {
        current = (uint16_t) ((uint32_t) (lta - counts) * 256u / lta);
    }

#if TDC_TOUCH_MARGIN_LOG_ENABLE
    ci_printi("[TOUCH] MARGIN cur=%u / th=%u (LTA=%u Counts=%u) \r\n", current, threshold, lta, counts);
#endif

    if (p_threshold_coeff != NULL)
    {
        *p_threshold_coeff = threshold;
    }
    if (p_current_coeff != NULL)
    {
        *p_current_coeff = current;
    }
    return true;
}
