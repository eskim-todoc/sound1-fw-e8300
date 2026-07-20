/*
 * tdc_touch_iqs323.c
 *
 * IQS323 I2C 직접 접근 구현 (HAL/포트 추상 없음). 3레이어 탈피 간결 재작성 2026-06-20.
 *
 * 내부 static 3층(아래->위): i2c_write/read -> force_window_open/wait_window_closed ->
 * write_register/read_register. 그 위에 시퀀스 보조(ack/sensor_setup/touch/events/beta_power)
 * 와 공개 6+보조. 레이어 호출은 위에서 아래로만, 함수 포인터/vtable 0.
 *
 * FullATI 전환: 0x36 = 0x040C(Full Mode), MULT/COMP write 폐기(IC 자동 산출), 단일채널(CRX0),
 * Beta/Power write, 부팅 1회 명시 Re-ATI. 정본: 99 종합설계 2장, B99.
 */

#include <tdc_touch_iqs323.h>

/* 레지스터 주소 (단일채널이라 최소). */
#define REG_SYSTEM_STATUS     0x10
#define REG_CH0_COUNTS        0x13 /* CH0 Filtered Counts (read-only, 디버그 계측) */
#define REG_CH0_LTA           0x14 /* CH0 LTA (read-only, 디버그 계측) */
#define REG_SENSOR0_SETUP     0x30
#define REG_CONV_FREQ         0x31
#define REG_SENSOR1_SETUP     0x40
#define REG_SENSOR0_ATI_SETUP 0x36
#define REG_SENSOR2_SETUP     0x50
#define REG_CH0_PROX          0x61
#define REG_CH0_TOUCH         0x62
#define REG_BETA_COUNTS       0xB0
#define REG_BETA_LTA_NORMAL   0xB1
#define REG_BETA_LTA_FAST     0xB2
#define REG_FAST_FILTER_BAND  0xB4
#define REG_SYSTEM_CONTROL    0xC0
#define REG_LP_REPORT_RATE    0xC2
#define REG_PM_TIMEOUT        0xC5
#define REG_EVENTS_ENABLE     0xD3

static bool g_in_ulp_mode = false;

/* **********************************************************************
 * ULP 플래그
 */
void tdc_touch_iqs323_set_ulp(void)
{
    g_in_ulp_mode = true;
}
void tdc_touch_iqs323_clear_ulp(void)
{
    g_in_ulp_mode = false;
}
bool tdc_touch_iqs323_is_ulp(void)
{
    return g_in_ulp_mode;
}

/* **********************************************************************
 * I2C 원시
 */
static bool i2c_write(uint8_t *p_buf, int len)
{
    EN__I2C_DRIVER_STATE state;
    static int           buf[3];

    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return false;
    }
    for (int i = 0; i < len; i++)
    {
        buf[i] = (int) p_buf[i];
    }
    i2c_startWriteData(TDC_TOUCH_IQS323_SLAVE_ADDR, &buf[0], len);

    while (1)
    {
        state = get_i2cDriverStatus();
        if (state == i2c_state_WritingDone)
        {
            setI2cDriverStatusIdle();
            break;
        }
        if (state == i2c_state_Error)
        {
            init_I2c();
            return false;
        }
    }
    return true;
}

static bool i2c_read(uint8_t *p_buf, int len)
{
    EN__I2C_DRIVER_STATE state;
    int                  buf[4]; /* 최대 4바이트 (0x13+0x14 연속 디버그 read 지원). 호출처 len <= 4 보장 */

    if (len < 1 || len > (int) (sizeof(buf) / sizeof(buf[0])))
    {
        return false;
    }
    if (get_i2cDriverStatus() != i2c_state_Idle)
    {
        return false;
    }
    i2c_startReadData(TDC_TOUCH_IQS323_SLAVE_ADDR, &buf[0], len);

    while (1)
    {
        state = get_i2cDriverStatus();
        if (state == i2c_state_ReadingDone)
        {
            setI2cDriverStatusIdle();
            break;
        }
        if (state == i2c_state_Error)
        {
            init_I2c();
            return false;
        }
    }
    for (int i = 0; i < len; i++)
    {
        p_buf[i] = (uint8_t) (buf[i] & 0xFF);
    }
    return true;
}

/* **********************************************************************
 * RDY 윈도우
 */
static bool wait_window_closed(int max_ms)
{
    int tick_old = tdc_hal_timer_get_tick();

    while (1)
    {
        if (max_ms <= (tdc_hal_timer_get_tick() - tick_old))
        {
            return false; /* soft timeout - 다음 force_window_open 이 복구 */
        }
        if (Sys_GPIO_Read(TDC_TOUCH_IQS323_RDY_PIN) == TDC_TOUCH_IQS323_WIN_CLOSED)
        {
            return true;
        }
    }
}

static bool force_window_open(void)
{
    uint8_t force_comm = 0xFF;

    if (Sys_GPIO_Read(TDC_TOUCH_IQS323_RDY_PIN) == TDC_TOUCH_IQS323_WIN_OPEN)
    {
        return true;
    }

#if 1
    if (!i2c_write(&force_comm, 1))
    {
        return false;
    }
#endif

    int tick_old = tdc_hal_timer_get_tick();
    while (1)
    {
        if (TDC_TOUCH_IQS323_MAX_WAIT_OPEN_MS <= (tdc_hal_timer_get_tick() - tick_old))
        {
            TDC_PRINTF_E("[TOUCH] FORCE WINDOW OPEN TIMEOUT (START = %d, END = %d) \r\n", tick_old, tdc_hal_timer_get_tick());
            return false;
        }
        if (Sys_GPIO_Read(TDC_TOUCH_IQS323_RDY_PIN) == TDC_TOUCH_IQS323_WIN_OPEN)
        {
            // TDC_PRINTF_I("[TOUCH] WINDOW OPEN WAIT TIME TOTAL %d MSEC \r\n", tdc_hal_timer_get_tick() - tick_old);
            return true;
        }
    }
}

/* **********************************************************************
 * 레지스터 helper
 */
static bool write_register(uint8_t addr, uint8_t lsb, uint8_t msb)
{
    uint8_t buf[3] = {addr, lsb, msb};

    if (!force_window_open())
    {
        TDC_PRINTF_E("[TOUCH] WRITE 0x%02X: WINDOW OPEN FAIL \r\n", addr);
        return false;
    }
    if (!i2c_write(buf, 3))
    {
        TDC_PRINTF_E("[TOUCH] WRITE 0x%02X: I2C FAIL \r\n", addr);
        return false;
    }
    TDC_PRINTF_V("[TOUCH] WRITE 0x%02X: LSB=0x%02X MSB=0x%02X \r\n", addr, lsb, msb);
    wait_window_closed(TDC_TOUCH_IQS323_MAX_WAIT_CLOSE_MS);
    return true;
}

static bool read_register(uint8_t addr, uint8_t *p_lsb, uint8_t *p_msb)
{
    uint8_t reg = addr;
    uint8_t buf[2];

    if (!force_window_open())
    {
        TDC_PRINTF_E("[TOUCH] READ 0x%02X: WINDOW OPEN FAIL (ADDR) \r\n", addr);
        return false;
    }
    if (!i2c_write(&reg, 1))
    {
        TDC_PRINTF_E("[TOUCH] READ 0x%02X: WRITE ADDR FAIL \r\n", addr);
        return false;
    }
    wait_window_closed(TDC_TOUCH_IQS323_MAX_WAIT_CLOSE_MS);

    if (!force_window_open())
    {
        TDC_PRINTF_E("[TOUCH] READ 0x%02X: WINDOW OPEN FAIL (DATA) \r\n", addr);
        return false;
    }
    if (!i2c_read(buf, 2))
    {
        TDC_PRINTF_E("[TOUCH] READ 0x%02X: I2C READ FAIL \r\n", addr);
        return false;
    }
    *p_lsb = buf[0];
    *p_msb = buf[1];
    wait_window_closed(TDC_TOUCH_IQS323_MAX_WAIT_CLOSE_MS);
    return true;
}

/* **********************************************************************
 * 시퀀스 보조 (static)
 */
static bool ack_reset(void)
{
    return write_register(REG_SYSTEM_CONTROL, 0x01, 0x00); /* bit0 ack */
}

static bool confirm_reset(void)
{
    uint8_t lsb, msb;

    for (int i = 0; i < 10; i++)
    {
        if (!read_register(REG_SYSTEM_STATUS, &lsb, &msb))
        {
            Sys_Delay(TDC_TOUCH_IQS323_DEFAULT_DELAY_MS);
            continue;
        }
        if (lsb == 0xEE && msb == 0xEE)
        {
            Sys_Delay(TDC_TOUCH_IQS323_DEFAULT_DELAY_MS);
            continue;
        }
        if ((lsb & (1u << 7)) == 0) /* reset_event clear */
        {
            return true;
        }
        Sys_Delay(TDC_TOUCH_IQS323_DEFAULT_DELAY_MS);
    }
    return false;
}

/* 단일채널: CRX0 단독 활성, CH1/CH2 Floating disable. CalCap 더미/방전 없음. */
static bool sensor_setup(void)
{
    if (!write_register(REG_SENSOR2_SETUP, 0x00, 0x00)) /* CH2 disable */
    {
        return false;
    }
    if (!write_register(REG_SENSOR1_SETUP, 0x00, 0x00)) /* CH1 disable (단일채널) */
    {
        return false;
    }
    /* CH0: enable_channel(LSB bit0) + ctx0(MSB bit0) */
    if (!write_register(REG_SENSOR0_SETUP, 0x01, 0x01))
    {
        return false;
    }
    return true;
}

static bool touch_settings(uint8_t threshold, uint8_t hysteresis)
{
    /* 0x62: bits[7:0]=Touch Threshold(LSB), bits[15:12]=Touch Hysteresis(MSB 상위 니블, DS A.17).
     * hysteresis 4비트값을 MSB 상위 니블에 위치시킨다(<<4). 이전엔 MSB 통째로 써 bits[11:8](미정의)에
     * 들어가 실제 hysteresis=0 이었던 버그 수정. */
    return write_register(REG_CH0_TOUCH, threshold, (uint8_t) ((hysteresis & 0x0F) << 4));
}

bool public_touch_settings(uint8_t threshold, uint8_t hysteresis)
{
    /* 0x62: bits[7:0]=Touch Threshold(LSB), bits[15:12]=Touch Hysteresis(MSB 상위 니블, DS A.17).
     * hysteresis 4비트값을 MSB 상위 니블에 위치시킨다(<<4). 이전엔 MSB 통째로 써 bits[11:8](미정의)에
     * 들어가 실제 hysteresis=0 이었던 버그 수정. */
    return write_register(REG_CH0_TOUCH, threshold, (uint8_t) ((hysteresis & 0x0F) << 4));
}

static bool events_enable(void)
{
    /* LSB: bit6 ati_error | bit4 ati_event | bit1 touch_event = 0x52. MSB 0. */
    return write_register(REG_EVENTS_ENABLE, 0x52, 0x00);
}

/* Beta(필터)·Power·Conversion write. Beta 정수값과 해석(damping=Beta/256 해석_A vs 시프트 해석_B)은
 * [실측 게이트] - 시험 1회 판별 후 비대칭 방향(LTA Normal 약터치 보존 / LTA Fast 노터치 복귀) 확정.
 * 현재 값 0xB1=0x0808·0xB2=0x0202 는 안전 기본값(99 §6 #1). */
static bool beta_power_settings(void)
{
    bool ok = true;
    ok &= write_register(REG_BETA_COUNTS, 0x02, 0x02);      /* Counts beta NP/LP */
    ok &= write_register(REG_BETA_LTA_NORMAL, 0x06, 0x06);  /* LTA Normal NP/LP=6 - AZD004 α=1/2^β. τ~12.8s(롱터치 2.4s의 ~5배)로 터치 진입 전 LTA 흡수 최소화 + 약결합 ~13s 흡수 균형. (4=3.2s 너무빠름·터치흡수 / 8=51s 느림) */
    ok &= write_register(REG_BETA_LTA_FAST, 0x02, 0x02);    /* LTA Fast NP/LP (빠름) */
    ok &= write_register(REG_FAST_FILTER_BAND, 0x0A, 0x00); /* Fast Filter Band 10cnt */
    ok &= write_register(REG_CONV_FREQ, 0x7F, 0x05);        /* 0x057F = 1MHz (self-cap 상한) */
    /* Power Mode: LSB bits[6:4]=101 Automatic No ULP. MSB 0x07 = CH0~2 timeout disable. */
    ok &= write_register(REG_SYSTEM_CONTROL, 0x50, 0x07);
    ok &= write_register(REG_LP_REPORT_RATE, 0x64, 0x00); /* LP report 100ms */
    ok &= write_register(REG_PM_TIMEOUT, 0x00, 0x00);     /* PM timeout 0 = 자동 강하 차단 → 노말 운용 중 NP 유지.
                                                           * (LP 전환 시 report rate 100ms 로 RDY 윈도우가 느려져
                                                           *  force_window_open 45ms 폴링이 윈도우를 놓쳐 timeout 발생.
                                                           *  터치=이벤트→NP 복귀로 회복되던 증상의 근본 차단. DS §5.3) */
    return ok;
}

/* 부팅 한정 Re-ATI 완료 블로킹 대기 (운용 폴링은 논블로킹). */
static bool wait_ati_done_blocking(void)
{
    uint8_t lsb, msb;

    for (int i = 0; i < 25; i++) /* 약 1.25s 상한 */
    {
        if (read_register(REG_SYSTEM_STATUS, &lsb, &msb) && !(lsb == 0xEE && msb == 0xEE) && (lsb & (1u << 5)) == 0) /* ati_active == 0 */
        {
            return true;
        }
        Sys_Delay(TDC_TOUCH_IQS323_DEFAULT_DELAY_MS * 50);
    }
    TDC_PRINTF_W("[TOUCH] BOOT RE-ATI: NOT CONFIRMED (터치 중이면 정상) \r\n");
    return false;
}

/* **********************************************************************
 * 공개 6동작
 */
bool tdc_touch_iqs323_read_status(tdc_touch_iqs323_status_t *out)
{
    uint8_t lsb, msb;

    out->ok = out->pressed = out->prox = out->ati_error = out->ati_active = false;

    if (!read_register(REG_SYSTEM_STATUS, &lsb, &msb))
    {
        return false;
    }
    if (lsb == 0xEE && msb == 0xEE)
    {
        return false; /* 글리치 */
    }
    out->ok         = true;
    out->ati_active = (lsb & (1u << 5)) != 0; /* System Status bit5 */
    out->ati_error  = (lsb & (1u << 6)) != 0; /* bit6 */
    out->pressed    = (msb & (1u << 1)) != 0; /* bit9 = MSB bit1, CH0 Touch */
    out->prox       = (msb & (1u << 0)) != 0; /* bit8 = MSB bit0, CH0 Prox */
    return true;
}

bool tdc_touch_iqs323_read_debug(tdc_touch_iqs323_debug_t *out)
{
    uint8_t reg = REG_CH0_COUNTS; /* 0x13 시작 - 0x13(Counts)+0x14(LTA) 연속 4바이트 read */
    uint8_t buf[4];

    out->ok     = false;
    out->counts = out->lta = 0;

    /* read_register 와 동일한 2-윈도우 시퀀스(addr write -> data read)지만 4바이트 연속 read 로
     * 0x13/0x14 두 레지스터를 1회 통신에 묶는다(폴링당 추가 윈도우 1회로 블로킹 최소화).
     * 계측 실패는 부가 기능이므로 printw(경고)로만 알리고 false 반환(FSM 무영향). */
    if (!force_window_open() || !i2c_write(&reg, 1))
    {
        TDC_PRINTF_W("[TOUCH] DEBUG READ FAIL (addr) \r\n");
        return false;
    }
    wait_window_closed(TDC_TOUCH_IQS323_MAX_WAIT_CLOSE_MS);

    if (!force_window_open() || !i2c_read(buf, 4))
    {
        TDC_PRINTF_W("[TOUCH] DEBUG READ FAIL (data) \r\n");
        return false;
    }
    wait_window_closed(TDC_TOUCH_IQS323_MAX_WAIT_CLOSE_MS);

    out->counts = (uint16_t) ((uint16_t) buf[0] | ((uint16_t) buf[1] << 8)); /* 0x13 little endian */
    out->lta    = (uint16_t) ((uint16_t) buf[2] | ((uint16_t) buf[3] << 8)); /* 0x14 little endian */
    out->ok     = true;
    return true;
}

void tdc_touch_iqs323_apply_settings(void)
{
    SYS_WATCHDOG_REFRESH();

    if (!ack_reset())
    {
        TDC_PRINTF_E("[TOUCH] FAIL: ACK RESET \r\n");
    }
    if (!confirm_reset())
    {
        TDC_PRINTF_E("[TOUCH] FAIL: CONFIRM RESET \r\n");
    }
    if (!sensor_setup())
    {
        TDC_PRINTF_E("[TOUCH] FAIL: SENSOR SETUP \r\n");
    }
    if (!touch_settings(TDC_TOUCH_IQS323_THRESHOLD, TDC_TOUCH_IQS323_HYSTERESIS))
    {
        TDC_PRINTF_E("[TOUCH] FAIL: TOUCH SETTINGS \r\n");
    }
    /* Prox Threshold = Touch Threshold 동일 계수 → prox 진입점을 touch 진입점과 일치시킨다.
     * 약결합(터치 미만)은 prox 미진입이라 LTA 자유 수렴(흡수), 진짜 터치 진입 시에만 LTA freeze(보호).
     * 0x61 LSB=Prox Threshold(계수), MSB=Prox Debounce(0=즉시). [실측 게이트: prox 단위 절대/계수 확인] */
    if (!write_register(REG_CH0_PROX, TDC_TOUCH_IQS323_PROX_THRESHOLD, 0x00))
    {
        TDC_PRINTF_E("[TOUCH] FAIL: PROX SETTINGS \r\n");
    }
    if (!events_enable())
    {
        TDC_PRINTF_E("[TOUCH] FAIL: EVENTS ENABLE \r\n");
    }
    if (!beta_power_settings())
    {
        TDC_PRINTF_E("[TOUCH] FAIL: BETA/POWER \r\n");
    }

    /* Full ATI: 0x36 = 0x040C (Mode=Full). MULT/COMP write 폐기 - IC 자동 산출. */
    if (!write_register(REG_SENSOR0_ATI_SETUP, 0x0C, 0x04))
    {
        TDC_PRINTF_E("[TOUCH] FAIL: ATI SETUP FULL \r\n");
    }

    SYS_WATCHDOG_REFRESH();

    /* 부팅 1회 명시 Re-ATI (FIXED 제거 시 게인 캘리 보장) + 완료 블로킹 대기.
     * 0xC0 LSB 에 Power Mode(bits[6:4]=101 No ULP)·MSB CH timeout disable(0x07)을 함께 써야
     * 트리거 write 가 beta_power_settings 의 Power Mode 설정을 000(Normal)로 덮지 않는다. */
    if (!write_register(REG_SYSTEM_CONTROL, 0x54, 0x07)) /* bit2 Re-ATI + Power No ULP + CH timeout disable */
    {
        TDC_PRINTF_E("[TOUCH] FAIL: BOOT RE-ATI \r\n");
    }
    (void) wait_ati_done_blocking();

    if (!write_register(REG_SYSTEM_CONTROL, 0x58, 0x07)) /* bit3 RESEED + Power No ULP + CH timeout disable */
    {
        TDC_PRINTF_E("[TOUCH] FAIL: RESEED \r\n");
    }

    tdc_touch_iqs323_clear_ulp();
    SYS_WATCHDOG_REFRESH();
}

bool tdc_touch_iqs323_re_ati(void)
{
    /* bit2 Re-ATI + Power No ULP(0x50) + CH timeout disable(0x07) 보존 - 단독 0x04 는 Power Mode 를 Normal 로 덮음. */
    return write_register(REG_SYSTEM_CONTROL, 0x54, 0x07);
}

bool tdc_touch_iqs323_reseed(void)
{
    /* bit3 RESEED only + Power No ULP + CH timeout disable 보존. */
    return write_register(REG_SYSTEM_CONTROL, 0x58, 0x07);
}

void tdc_touch_iqs323_mclr(void)
{
    Sys_DIO_Config(TDC_TOUCH_IQS323_RDY_PIN, TDC_TOUCH_IQS323_RDY_PIN_CFG_OUTPUT);
    Sys_GPIO_Set_Low(TDC_TOUCH_IQS323_RDY_PIN);
    Sys_Delay(TDC_TOUCH_IQS323_DEFAULT_DELAY_MS * TDC_TOUCH_IQS323_MCLR_HOLD_MS);
    Sys_DIO_Config(TDC_TOUCH_IQS323_RDY_PIN, TDC_TOUCH_IQS323_RDY_PIN_CFG_INPUT);
    Sys_Delay(TDC_TOUCH_IQS323_DEFAULT_DELAY_MS * TDC_TOUCH_IQS323_BOOT_WAIT_MS);
}

bool tdc_touch_iqs323_is_ati_done(void)
{
    tdc_touch_iqs323_status_t st;

    if (!tdc_touch_iqs323_read_status(&st))
    {
        return false; /* read 실패/글리치 -> 미완료로 재시도 */
    }
    return (st.ati_active == false);
}

/* **********************************************************************
 * 보조 - 절전 설정 (Full ATI 정합: 운용 임계 유지, ATI 재쓰기 0)
 */
/* 절전 전용 settings 는 제거됨 - 절전도 노말과 동일한 IQS323 설정(운용 임계·Full ATI·PM timeout 0
 * → 항상 NP)을 그대로 사용한다. CM3 클럭만 ci_power_sleep(main.c)로 절감. RESEED 는 ULP 루프가
 * 첫 노터치 확정 시점에 직접 발행한다. */
