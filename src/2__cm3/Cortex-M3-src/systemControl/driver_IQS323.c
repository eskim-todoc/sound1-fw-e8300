/*
 * driver_IQS323.c
 */

#include <driver_IQS323.h>

static int _g_tick_old        = 0;
static int _g_touch_state_old = IQS323_TOUCH_STATE_RESET;

void iqs323_update_tick(int tick)
{
    _g_tick_old = tick;
}

int iqs323_get_tick(void)
{
    return _g_tick_old;
}

void iqs323_update_state(int state)
{
    _g_touch_state_old = state;
}

int iqs323_get_state(void)
{
    return _g_touch_state_old;
}

/*
 * i2c writing
 */
bool iqs323_i2c_write(uint8_t *p_buf, int len)
{
    EN__I2C_DRIVER_STATE i2c_driver_state;
    static int           buf[3];

    // i2c가 idle 상태일 때만 새 트랜잭션 시작 가능
    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return false;
    }

    // 전송할 데이터를 int 형식 버퍼로 복사
    for (int i = 0; i < len; i++)
    {
        buf[i] = (int) p_buf[i];
    }

    // i2c write trigger
    i2c_startWriteData(IQS323_SLAVE_ADDR, &buf[0], len);

    // wait done
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

        //__WFE();
    }

    return true;
}

/*
 * i2c reading
 */
bool iqs323_i2c_read(uint8_t *p_buf, int len)
{
    EN__I2C_DRIVER_STATE i2c_driver_state;
    int                  buf[2];

    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return false;
    }

    // i2c read trigger
    i2c_startReadData(IQS323_SLAVE_ADDR, &buf[0], len);

    // wait done
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

        //__WFE();
    }

    p_buf[0] = (uint8_t) (buf[0] & 0x000000FF);
    p_buf[1] = (uint8_t) (buf[1] & 0x000000FF);

    return true;
}

/*
 * is rdy window opened
 */
bool iqs323_is_rdy_window_opened(void)
{
    return (Sys_GPIO_Read(IQS323_RDY_PIN) == IQS323_RDY_WINDOW_OPENED);
}

/*
 * wait rdy window closed
 */
bool iqs323_wait_rdy_window_closed(int max_ms)
{
    int tick_old, tick_now;

    tick_old = ci_timer_get_tick();

    while (1)
    {
        tick_now = ci_timer_get_tick();

        if (max_ms <= (tick_now - tick_old))
        {
            // 시간 초과 발생 시 break 후 false 반환
            break;
        }

        if (Sys_GPIO_Read(IQS323_RDY_PIN) == IQS323_RDY_WINDOW_CLOSED)
        {
            return true;  // rdy window open 상태이므로 true 반환
        }
    }

#if 1
    ci_printe("[TOUCH] ERROR: TIMEOUT (%d MS) FOR WAIT WINDOW CLOSE \r\n", max_ms);
#endif

    return false;
}

/*
 * rdy window open
 */
bool iqs323_rdy_window_open(void)
{
    uint8_t force_comm = 0xFF;
    int     tick_old, tick_now;

#if 0
    // force communication (0xFF) 사용하여 rdy window open 시도
    if (!iqs323_i2c_write(&force_comm, 1))
    {
        return false;  // i2c 통신 실패 시 false 반환
    }
#else
    // 이미 rdy window open 상태이면 true 반환
    if (Sys_GPIO_Read(IQS323_RDY_PIN) == IQS323_RDY_WINDOW_OPENED)
    {
        return true;
    }

    // force communication (0xFF) 사용하여 rdy window open 시도
    if (!iqs323_i2c_write(&force_comm, 1))
    {
        return false;  // i2c 통신 실패 시 false 반환
    }
#endif

    // rdy window open 최대 대기 시간 내에 rdy window open 되는지 확인

    tick_old = ci_timer_get_tick();

    while (1)
    {
        tick_now = ci_timer_get_tick();

        if (IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN <= (tick_now - tick_old))
        {
            // 시간 초과 발생 시 break 후 false 반환
            break;
        }

        if (Sys_GPIO_Read(IQS323_RDY_PIN) == IQS323_RDY_WINDOW_OPENED)
        {
            return true;  // rdy window open 상태이므로 true 반환
        }
    }

#if 1
    ci_printf("[TOUCH] ERROR : TIMEOUT (%d MS) FOR WINDOW OPEN WITH FORCE COMM \r\n", IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN);
#endif

    return false;
}

/*
 * rdy window close
 */
bool iqs323_rdy_window_close(void)
{
    uint8_t end_comm = 0xFF;
    int     tick_old, tick_now;

    // 이미 rdy window close 상태이면 true 반환
    if (Sys_GPIO_Read(IQS323_RDY_PIN) == IQS323_RDY_WINDOW_CLOSED)
    {
        return true;
    }

    // end communication (0xFF) 사용하여 rdy window close 시도
    if (!iqs323_i2c_write(&end_comm, 1))
    {
        return false;
    }

    // rdy window close 최대 대기 시간 내에 rdy window close 되는지 확인

    tick_old = ci_timer_get_tick();

    while (1)
    {
        tick_now = ci_timer_get_tick();

        if (IQS323_MAX_WAIT_MS_FOR_WINDOW_CLOSE <= (tick_now - tick_old))
        {
            // 시간 초과 발생 시 break 후 false 반환
            break;
        }

        if (Sys_GPIO_Read(IQS323_RDY_PIN) == IQS323_RDY_WINDOW_CLOSED)
        {
            return true;  // rdy window close 상태이므로 true 반환
        }
    }

#if 1
    ci_printf("[TOUCH] ERROR : TIMEOUT (%d MS) FOR WINDOW CLOSE WITH END COMM \r\n", IQS323_MAX_WAIT_MS_FOR_WINDOW_OPEN);
#endif

    return false;
}

/*
 * ack reset event
 */
bool iqs323_ack_reset_event(void)
{
    IQS323_REG_SYSTEM_CONTROL_T system_control;

    system_control.bytes[0] = IQS323_REG_ADDR_SYSTEM_CONTROL;
    system_control.bytes[1] = 0x01;  // lsb first, and ack reset only;
    system_control.bytes[2] = 0x00;

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // write control
    if (!iqs323_i2c_write(&system_control.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: ACK RESET EVENT AND I2C EVENT MODE \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              system_control.bytes[0],                                               //
              system_control.bytes[1],                                               //
              system_control.bytes[2]                                                //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    return true;
}

/*
 * confirm reset event
 */
bool iqs323_confirm_reset_event(void)
{
    bool                       is_clear;
    IQS323_REG_SYSTEM_STATUS_T system_status;

    is_clear               = false;
    system_status.bytes[0] = IQS323_REG_ADDR_SYSTEM_STATUS;

    // window open 및 reset event 초기화에 시간이 걸릴 수 있으므로 최대 10회를 체크해본다. (회당 1ms 딜레이 적용)
    for (int i = 0; i < 10; i++)
    {
        ci_printd("[TOUCH] TRY COUNT: %d \r\n", i + 1);

        // confirm window open
        if (iqs323_rdy_window_open())
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
        }

        // register address
        if (!iqs323_i2c_write(&system_status.bytes[0], 1))
        {
            ci_printe("[TOUCH] I2C WRITE ERROR: SYSTEM STATUS REGISTER \r\n");
            return false;
        }

        ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", system_status.bytes[0]);

        // confirm window close
        if (iqs323_wait_rdy_window_closed(10))
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
        }

        // confirm window open
        if (iqs323_rdy_window_open())
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
        }

        // read status
        if (!iqs323_i2c_read(&system_status.bytes[1], 2))
        {
            ci_printe("[TOUCH] I2C READ ERROR: SYSTEM STATUS \r\n");
            return false;
        }

        ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
                  system_status.bytes[0],                                               //
                  system_status.bytes[1],                                               //
                  system_status.bytes[2]                                                //
        );

        // confirm window close
        if (iqs323_wait_rdy_window_closed(10))
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
        }

        // 16bit가 0xEEEE로 읽히면 window open 확보에 이슈가 있었던 것이므로,
        // 다시 한 번 더 상태 레지스터 읽기를 진행해본다.
        if ((system_status.bytes[1] == 0xEE) && (system_status.bytes[2] == 0xEE))
        {
            ci_printw("[TOUCH] WARNN: SYSTEM STATUS READ RESULT = 0xEEEE \r\n");
        }
        else
        {
            if (system_status.elements.lsb.reset_event == IQS323_REG_VAL_SYSTEM_STATUS_LSB_NO_RESET_EVENT)
            {
                // window open이 확실하여 읽은 값이 0xEEEE가 아닌 값이며,
                // reset event가 확실히 clear 되어 있다면 true 상태로 반환한다.
                is_clear = true;
                ci_printv("[TOUCH] CURRENTLY, RESET EVENT IS NOT SET \r\n");
                break;
            }
        }

        Sys_Delay(IQS323_DEFAULT_DELAY_MS);
    }

    return is_clear;
}

/*
 * events enable
 */
bool iqs323_events_enable(void)
{
    IQS323_REG_EVENTS_ENABLE_T events_enable;

    // addr
    events_enable.elements.addr = IQS323_REG_ADDR_EVENTS_ENABLE;
    // msb
    events_enable.elements.msb.reserved_bit8_to_15 = IQS323_REG_VAL_EVENTS_ENABLE_MSB_RESERVED_BIT8_TO_15;
    // lsb
    events_enable.elements.lsb.reserved_bit7 = IQS323_REG_VAL_EVENTS_ENABLE_LSB_RESERVED_BIT7;
    events_enable.elements.lsb.ati_error     = IQS323_REG_VAL_EVENTS_ENABLE_LSB_ATI_ERROR_ENABLE;
    events_enable.elements.lsb.reserved_bit5 = IQS323_REG_VAL_EVENTS_ENABLE_LSB_RESERVED_BIT5;
    events_enable.elements.lsb.ati_event     = IQS323_REG_VAL_EVENTS_ENABLE_LSB_ATI_EVENT_ENABLE;
    events_enable.elements.lsb.power_event   = IQS323_REG_VAL_EVENTS_ENABLE_LSB_POWER_EVENT_DISABLE;
    events_enable.elements.lsb.slider_event  = IQS323_REG_VAL_EVENTS_ENABLE_LSB_SLIDER_EVENT_DISABLE;
    events_enable.elements.lsb.touch_event   = IQS323_REG_VAL_EVENTS_ENABLE_LSB_TOUCH_EVENT_ENABLE;
    events_enable.elements.lsb.prox_event    = IQS323_REG_VAL_EVENTS_ENABLE_LSB_PROX_EVENT_DISABLE;

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    if (!iqs323_i2c_write(&events_enable.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: EVENTS ENABLE \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              events_enable.bytes[0],                                                //
              events_enable.bytes[1],                                                //
              events_enable.bytes[2]                                                 //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // register address
    if (!iqs323_i2c_write(&events_enable.bytes[0], 1))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: EVENTS ENABLE REGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", events_enable.bytes[0]);

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // read status
    if (!iqs323_i2c_read(&events_enable.bytes[1], 2))
    {
        ci_printe("[TOUCH] I2C READ ERROR: EVENTS ENABLE \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              events_enable.bytes[0],                                               //
              events_enable.bytes[1],                                               //
              events_enable.bytes[2]                                                //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    return true;
}

/*
 * sensor setup
 */
bool iqs323_sensor_setup(void)
{
    IQS323_REG_SENSOR_SETUP_T sensor_setup;

    // common setting value

    // msb
    sensor_setup.elements.msb.reserved_bit15 = IQS323_REG_VAL_SENSOR_SETUP_MSB_RESERVED_BIT15;
    sensor_setup.elements.msb.cal_cap_rx     = IQS323_REG_VAL_SENSOR_SETUP_MSB_CAL_CAP_RX_NOT_SELECT;
    sensor_setup.elements.msb.cal_cap_tx     = IQS323_REG_VAL_SENSOR_SETUP_MSB_CAL_CAP_TX_NOT_SELECT;
    sensor_setup.elements.msb.reserved_bit12 = IQS323_REG_VAL_SENSOR_SETUP_MSB_RESERVED_BIT12;
    sensor_setup.elements.msb.tx_a           = IQS323_REG_VAL_SENSOR_SETUP_MSB_TX_A_DISABLE;
    sensor_setup.elements.msb.ctx2           = IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX2_DISABLE;
    sensor_setup.elements.msb.ctx1           = IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX1_DISABLE;
    sensor_setup.elements.msb.ctx0           = IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX0_DISABLE;
    // lsb
    sensor_setup.elements.lsb.reserved_bit7              = IQS323_REG_VAL_SENSOR_SETUP_LSB_RESERVED_BIT7;
    sensor_setup.elements.lsb.release_movement_ui_enable = IQS323_REG_VAL_SENSOR_SETUP_LSB_RELEASE_MOVEMENT_UI_DISABLE;
    sensor_setup.elements.lsb.fosc_tx_frequency          = IQS323_REG_VAL_SENSOR_SETUP_LSB_FOSC_TX_FREQ_PERIOD_AND_FRAC;
    sensor_setup.elements.lsb.vbias                      = IQS323_REG_VAL_SENSOR_SETUP_LSB_VBIAS_DISABLE;
    sensor_setup.elements.lsb.invert                     = IQS323_REG_VAL_SENSOR_SETUP_LSB_DO_NOT_INVERT_CH_LOGIC;
    sensor_setup.elements.lsb.dual_direction             = IQS323_REG_VAL_SENSOR_SETUP_LSB_SINGLE_DIRECTION_THRESHOLD;
    sensor_setup.elements.lsb.linearise_counts           = IQS323_REG_VAL_SENSOR_SETUP_LSB_DO_NOT_LINEARISE_COUNTS;
    sensor_setup.elements.lsb.enable_channel             = IQS323_REG_VAL_SENSOR_SETUP_LSB_CHANNEL_DISABLE;

    ci_printd("[TOUCH] SETUP SENSOR: 2 \r\n");

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // sensor 2 disable
    sensor_setup.elements.addr = IQS323_REG_ADDR_SENSOR2_SETUP;
    if (!iqs323_i2c_write(&sensor_setup.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: SENSOR 2 SETUP \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              sensor_setup.bytes[0],                                                 //
              sensor_setup.bytes[1],                                                 //
              sensor_setup.bytes[2]                                                  //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // register address
    if (!iqs323_i2c_write(&sensor_setup.bytes[0], 1))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: SENSOR 2 REGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", sensor_setup.bytes[0]);

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // read status
    if (!iqs323_i2c_read(&sensor_setup.bytes[1], 2))
    {
        ci_printe("[TOUCH] I2C READ ERROR: SENSOR 2 RESGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              sensor_setup.bytes[0],                                                //
              sensor_setup.bytes[1],                                                //
              sensor_setup.bytes[2]                                                 //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    ci_printd("[TOUCH] SETUP SENSOR: 1 \r\n");

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // sensor 1 disable
    sensor_setup.elements.addr = IQS323_REG_ADDR_SENSOR1_SETUP;
    if (!iqs323_i2c_write(&sensor_setup.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: SENSOR 1 SETUP \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              sensor_setup.bytes[0],                                                 //
              sensor_setup.bytes[1],                                                 //
              sensor_setup.bytes[2]                                                  //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // register address
    if (!iqs323_i2c_write(&sensor_setup.bytes[0], 1))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: SENSOR 1 REGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", sensor_setup.bytes[0]);

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // read status
    if (!iqs323_i2c_read(&sensor_setup.bytes[1], 2))
    {
        ci_printe("[TOUCH] I2C READ ERROR: SENSOR 1 RESGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              sensor_setup.bytes[0],                                                //
              sensor_setup.bytes[1],                                                //
              sensor_setup.bytes[2]                                                 //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    ci_printd("[TOUCH] SETUP SENSOR: 0 \r\n");

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // sensor 0 enable
    sensor_setup.elements.msb.ctx0           = IQS323_REG_VAL_SENSOR_SETUP_MSB_CTX0_ENABLE;
    sensor_setup.elements.lsb.enable_channel = IQS323_REG_VAL_SENSOR_SETUP_LSB_CHANNEL_ENABLE;
    sensor_setup.elements.addr               = IQS323_REG_ADDR_SENSOR0_SETUP;
    if (!iqs323_i2c_write(&sensor_setup.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: SENSOR 0 SETUP \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              sensor_setup.bytes[0],                                                 //
              sensor_setup.bytes[1],                                                 //
              sensor_setup.bytes[2]                                                  //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // register address
    if (!iqs323_i2c_write(&sensor_setup.bytes[0], 1))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: SENSOR 0 REGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", sensor_setup.bytes[0]);

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // read status
    if (!iqs323_i2c_read(&sensor_setup.bytes[1], 2))
    {
        ci_printv("[TOUCH] I2C READ ERROR: SENSOR 0 RESGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              sensor_setup.bytes[0],                                                //
              sensor_setup.bytes[1],                                                //
              sensor_setup.bytes[2]                                                 //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    return true;
}

/*
 * touch settings
 */
bool iqs323_touch_settings(void)
{
    IQS323_REG_TOUCH_SETTINGS_T touch_settings;

    // addr
    touch_settings.elements.addr = IQS323_REG_ADDR_CH0_TOUCH_SETTINGS;
    // msb
    touch_settings.elements.msb.touch_hysteresis = IQS323_REG_VAL_TOUCH_SETTINGS_MSB_HYSTERESIS_80;
    // lsb
    touch_settings.elements.lsb.touch_threshold = IQS323_REG_VAL_TOUCH_SETTINGS_LSB_THRESHOLD_80;

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    if (!iqs323_i2c_write(&touch_settings.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: CH0 TOUCH SETTINGS \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              touch_settings.bytes[0],                                               //
              touch_settings.bytes[1],                                               //
              touch_settings.bytes[2]                                                //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // register address
    if (!iqs323_i2c_write(&touch_settings.bytes[0], 1))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: TOUCH SETTINGS REGISTER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", touch_settings.bytes[0]);

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    // read status
    if (!iqs323_i2c_read(&touch_settings.bytes[1], 2))
    {
        ci_printe("[TOUCH] I2C READ ERROR: TOUCH SETTINGS \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              touch_settings.bytes[0],                                              //
              touch_settings.bytes[1],                                              //
              touch_settings.bytes[2]                                               //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    return true;
}

/*
 * re-ati trigger
 */
bool iqs323_re_ati_trigger(void)
{
    IQS323_REG_SYSTEM_CONTROL_T system_control;

    system_control.bytes[0] = IQS323_REG_ADDR_SYSTEM_CONTROL;
    system_control.bytes[1] = 0x00;
    system_control.bytes[2] = 0x00;

    // 전부 기본 설정에서, Re-ATI만 수행하도록 1로 설정
    system_control.elements.lsb.re_ati = IQS323_REG_VAL_SYSTEM_CONTROL_LSB_TRIGGER_RE_ATI;

    // confirm window open
    if (iqs323_rdy_window_open())
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
    }

    if (!iqs323_i2c_write(&system_control.bytes[0], 3))
    {
        ci_printe("[TOUCH] I2C WRITE ERROR: RE-ATI TRIGGER \r\n");
        return false;
    }

    ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
              system_control.bytes[0],                                               //
              system_control.bytes[1],                                               //
              system_control.bytes[2]                                                //
    );

    // confirm window close
    if (iqs323_wait_rdy_window_closed(10))
    {
        ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
    }
    else
    {
        ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
    }

    // window close가 되는 시점 즉, i2c 트랜잭션이 종료되는 시점에 바로 re-ati를 시작함

    return true;
}

/*
 * re-ati done
 */
bool iqs323_wait_re_ati_done(void)
{
    IQS323_REG_SYSTEM_STATUS_T system_status;

    system_status.bytes[0] = IQS323_REG_ADDR_SYSTEM_STATUS;

    // wait re-ati done (maximum 500ms)
    for (int i = 0; i < 10; i++)
    {
        ci_printd("[TOUCH] TRY COUNT: %d \r\n", i + 1);

        // confirm window open
        if (iqs323_rdy_window_open())
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
        }

        // register address
        if (!iqs323_i2c_write(&system_status.bytes[0], 1))
        {
            ci_printe("[TOUCH] I2C WRITE ERROR: SYSTEM STATUS REGISTER \r\n");
            // return false;

            Sys_Delay(IQS323_DEFAULT_DELAY_MS * 100);  // 100ms
            continue;
        }

        ci_printv("[TOUCH] I2C WRITE SUCCESS: REG = %02X \r\n", system_status.bytes[0]);

        // confirm window close
        if (iqs323_wait_rdy_window_closed(10))
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
        }

        // confirm window open
        if (iqs323_rdy_window_open())
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW OPEN SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW OPEN FAIL \r\n");
        }

        // 읽기 성공 시
        if (iqs323_i2c_read(&system_status.bytes[1], 2))
        {
            ci_printv("[TOUCH] I2C READ SUCCESS: REG = %02X, LSB = %02X, MSB = %02X \r\n",  //
                      system_status.bytes[0],                                               //
                      system_status.bytes[1],                                               //
                      system_status.bytes[2]                                                //
            );

            // 읽은 값이 0xEEEE가 아닐 때
            if ((system_status.bytes[1] != 0xEE) && (system_status.bytes[2] != 0xEE))
            {
                // ATI 에러?
                if (system_status.elements.lsb.ati_error == IQS323_REG_VAL_SYSTEM_STATUS_LSB_ATI_ERROR)
                {
                    ci_printe("[TOUCH] ERROR: RE-ATI ERROR OCCURRED \r\n");
                    return false;
                }
                else  // 에러 없음
                {
                    // ATI 성공
                    if (system_status.elements.lsb.ati_event == IQS323_REG_VAL_SYSTEM_STATUS_LSB_ATI_EVENT)
                    {
                        // confirm window close
                        if (iqs323_wait_rdy_window_closed(10))
                        {
                            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
                        }
                        else
                        {
                            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
                        }

                        ci_printv("[TOUCH] SUCCESS: RE-ATI \r\n");

                        return true;
                    }
                }
            }
            else
            {
                ci_printw("[TOUCH] WARNN: SYSTEM STATUS READ RESULT = 0xEEEE \r\n");
            }
        }
        else
        {
            ci_printe("[TOUCH] I2C READ ERROR: RE-ATI DONE CHECK \r\n");
        }

        // confirm window close
        if (iqs323_wait_rdy_window_closed(10))
        {
            ci_printv("[TOUCH] ACK RESET EVENT: WINDOW CLOSE SUCCESS \r\n");
        }
        else
        {
            ci_printe("[TOUCH] ACK RESET EVENT: WINDOW CLOSE FAIL \r\n");
        }

        Sys_Delay(IQS323_DEFAULT_DELAY_MS * 100);  // 100ms

    }  // end, for loop

    ci_printe("[TOUCH] ERROR: RE-ATI TIMEOUT \r\n");

    return false;
}

/*
 * get touch state
 */

bool iqs323_get_touch_state(int *p_state)
{
    IQS323_REG_SYSTEM_STATUS_T system_status;

    // addr
    system_status.bytes[0] = IQS323_REG_ADDR_SYSTEM_STATUS;

    // confirm window open
    iqs323_rdy_window_open();

    // register address
    if (!iqs323_i2c_write(&system_status.bytes[0], 1))
    {
        ci_printe("[TOUCH] ERROR: I2C WRITE FOR SYSTEM STATUS REGISTER \r\n");
        return false;
    }

    // confirm window close
    iqs323_wait_rdy_window_closed(10);

    // confirm window open
    iqs323_rdy_window_open();

    if (!iqs323_i2c_read(&system_status.bytes[1], 2))
    {
        ci_printe("[TOUCH] ERROR: I2C READ FOR SYSTEM STATUS DURING GET TOUCH STATE \r\n");
        return false;
    }

    // confirm window close
    iqs323_wait_rdy_window_closed(10);

    if (system_status.elements.lsb.ati_error == IQS323_REG_VAL_SYSTEM_STATUS_LSB_ATI_ERROR)
    {
        *p_state = IQS323_TOUCH_STATE_ATI_ERROR;
    }
    else
    {
        if (system_status.elements.msb.ch0_touch == IQS323_REG_VAL_SYSTEM_STATUS_MSB_CH0_IN_TOUCH)
        {
            *p_state = IQS323_TOUCH_STATE_TOUCH;
        }
        else
        {
            *p_state = IQS323_TOUCH_STATE_NOT_TOUCH;
        }
    }

    return true;
}

bool iqs323_process(void)
{
    return true;
}

/*
 *
 *
 *
 *
 * */
