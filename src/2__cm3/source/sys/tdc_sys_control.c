
#include <hw.h>
#include <stdbool.h>

#include <tdc_sys_error.h>
#include <tdc_shm.h>
#include <tdc_hal_spi.h>
#include <tdc_isd.h>
#include <tdc_ble_remote.h>
#include <tdc_ble_mapping.h>

#include <tdc_led_output.h>
#include <tdc_isd_stim_standalone.h>
#include <tdc_pwr_battery.h>
#include <processorDirective.h>
#include <tdc_sys_control.h>

#ifdef ENABLE_UI_CMD
#include <tdc_ui_command.h>
#endif

#include <tdc_touch.h>

#include <tdc_printf.h>

/* 이 파일 전용 상태. 헤더에 extern 선언이 없어 외부에서 쓰지 않으므로 static.
 * tdc_sys_control_step() 은 이 값을 갱신한 뒤 복사본을 반환한다 - 호출자가 반환값을
 * 수정해도 여기 원본에는 반영되지 않는다는 점에 유의. */
static tdc_sys_state_t systemStatus = {false, false, false, false, false, false};

#define LED_OnTime_afterCoverClosed 4501

#define BLE_OffTimeAfterISD_Disconnected 1000

void tdc_sys_control_nrf_off_command(void)
{
    // Sys_GPIO_Set_Low(DIO_NUM_NRF_ON_OFF_COMMAND);
}

bool ISD_ConnectionHistory = false;

// 수정 필요함.. 리모콘 쪽 연결 끊김. 리모콘 연결 상태 및 타이머 필요할 듯
void tdc_sys_control_nrf_on_off(ST__ISD_STATUS isd_state, bool global_BLE_Off, bool mappingConnection, bool Mapping_BLE_Off)
{
    static int deaylCounter = 0;
    bool       BLE_OFF      = false;

    if (isd_state.isd_controlState >= en__isdStatus_stimul_10V_Ok)
    {
        ISD_ConnectionHistory = true;
    }

    if (ISD_ConnectionHistory)
    {
        // 내부기와 연결되고 일정 시간이 지난 후에 BLE를 끈다.
        // 연결된 내부기의 id에 해당하는 매핑데이터(내부기 이름)이 읽어 들여진 이후에 nrf를 켜야한다.
        if (isd_state.isd_controlState < en__isdStatus_stimul_10V_Ok)
        {
            if (deaylCounter > 640)  // 0.6초 후 BLE 끔
            {
                BLE_OFF               = true;
                ISD_ConnectionHistory = false;
            }

            deaylCounter++;
        }
        else
        {
            deaylCounter = 0;
        }
    }
    else  // 내부기가 연결되지 않은 초기 상태 또는 끊어지고 일정 시간이 지난 이후에는 NRF를 끈다.
    {
        BLE_OFF = true;
    }

    //  매핑이 연결되어 있으면 내부기 연결이 끊어지더라도 ble를 끄지 않는다.
    if (mappingConnection)
    {
        BLE_OFF = false;
    }

    // 매핑에서  BLE를 잠시 껐다가 켜는 경우(최초 내부기 이름 설정)
    if (Mapping_BLE_Off)
    {
        BLE_OFF = true;
    }

    //
    if (global_BLE_Off)
    {
        BLE_OFF = true;
    }

    if (BLE_OFF)
    {
        // tdc_sys_control_nrf_off_command();
    }
    else
    {
        // tdc_sys_control_nrf_on_command();
    }
}

/* NRF_adv_powerMode() 는 제거했다(#if 0 사장). 함수 전체가 죽은 블록 안이었고
 * 호출부도 0 이었다. 참조하던 ENABLE_NRF_ADV_LowPower 매크로도 이 함수 안이 유일했다. */

/* conneded_ISD / mappingConnected 는 '지난 tick' 값이다 - tdc_isd_step() 와
 * tdc_ble_communication_step() 이 tdc_sys_control_step() 의 enable_ISD 를 받아 도는 순환 구조라
 * 같은 tick 안에서는 확정되지 않는다. 다만 이 두 입력의 소비처는 모두 시간 누적
 * 판정(ISD_Disconnection_counter / 저배터리 10분 주기)이거나 인간 조작 스케일
 * (파워오프 탈출 / 매핑 중 버튼 무시)이라 1-tick(=1ms) 지연은 무해하다.
 * 지연에 민감한 값을 이 순환에 태우지 말 것. */
/* ============================================================================
 * 시스템 제어 상태 (구 함수 본문 static 7개 통합)
 *
 * 흡수된 설계 이행: docs/tasks/main/20260715_systemcontrol-fsm-decompose/
 * dead static 3개(PowerOn_StartCounter · normalModeCounter ·
 * CounterAfterCoverClosed)는 읽기 0 실증으로 제거했다.
 *
 * [초기값 승계 주의] isd_disconnection_counter 는 0 이 아니다. 0 으로 시작하면
 * gate_power_button() 의 '< 300' 판정이 부팅 직후 참이 되어 전원버튼이 무시된다.
 * ========================================================================== */
typedef struct
{
    bool start_flag;                 /* 부팅 첫 진입 완료 */
    bool cradle_cover_closed_edge;   /* 크래들 뚜껑 닫힘 엣지 (1회성) */
    bool poweroff_enabled;           /* 파워오프 시퀀스 진행 중 */
    int  poweroff_start_counter;     /* burst 종료 검출용 */
    int  isd_disconnection_counter;  /* ISD 미연결 누적 (3분 타임아웃) */
    int  low_batt_indicator_counter; /* 저배터리 자극 알림 10분 주기 */
    int  prev_carrying_case_state;   /* 구 prev_batteryChargerConnectionStatus -
                                      * 실제로 담는 값이 carryingCasePluggedIn 이라 정정 */
} tdc_sys_control_state_t;

static tdc_sys_control_state_t s_sysctl = {
    .start_flag                 = false,
    .cradle_cover_closed_edge   = false,
    .poweroff_enabled           = false,
    .poweroff_start_counter     = 0,
    .isd_disconnection_counter  = Df_Disconnection_BLE_Time_ms, /* = 2000. zero-init 금지 */
    .low_batt_indicator_counter = 0,
    .prev_carrying_case_state   = df_Defalut,
};

/* 에러 발생 시 ERROR 소스 LED 요청. 상태 없음. */
static void handle_error(tdc_sys_error_code_t mcu_error)
{
#ifdef ENABLE_UI_CMD
    if (tdc_ui_command_is_led_override(TDC_LED_SRC_ERROR))
    {
        return;
    }
#endif
    tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_ERROR_MCU);

    if (mcu_error.dataProcessingErrorFlag != en__NA)
    {
        tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_ERROR_MAP);
    }
    if (mcu_error.accelerometerErrorFlag != en__NA)
    {
        tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_ERROR_ACCEL);
    }
    if (mcu_error.FPGA_CommunicationErrorFlag != en__NA)
    {
        tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_ERROR_FPGA);
    }
}

/* 충전기 연결 시 처리 (크래들 뚜껑 엣지 감지 포함). */
static void handle_charging(ST__USB_CONNECTOR charger, bool power_button_pushed, tdc_sys_state_t *out_state)
{
    (void) power_button_pushed; /* TDC_LED_DBG_LONG_TOUCH_IGNORE 비활성 시 미사용 */

    out_state->BLE_Off    = true;  /* NRF 를 꺼진 상태로 유지 */
    out_state->enablePMIC = false; /* 상시전원 외 전원 차단을 CFX 에 전달 */

    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_FillZero);

    if (charger.carryingCasePluggedIn == df_Connected) /* 충전 케이스(크래들) 연결됨 */
    {
        if (charger.carryingCaseCoverOpen == df_Connected)
        {
            s_sysctl.cradle_cover_closed_edge = false; /* 뚜껑 열림 -> 엣지 플래그 리셋 */
            /* 충전 중 LED: 배터리 레벨 판정은 Arbiter 가 처리 (tdc_led_request 불필요) */
        }
        else if (!s_sysctl.cradle_cover_closed_edge)
        {
            s_sysctl.cradle_cover_closed_edge = true;
            out_state->cradleLidClosed        = true;
            TDC_PRINTF_I("[SYSTEM] CRADLE LID CLOSED FIRST DETECT\r\n");
        }
    }
    /* 크래들 없이 자극기에 직접 충전기가 꼽힌 경우: 할 일 없음 */

#if TDC_LED_DBG_LONG_TOUCH_IGNORE
    if (power_button_pushed) { tdc_led_request(TDC_LED_SRC_DBG, TDC_LED_ST_DBG_LONG_TOUCH_IGNORE); }
#endif
    s_sysctl.start_flag = false;
}

/* 매핑 연결 중 또는 ISD 최근 연결(<300) 시 전원버튼을 무시한다.
 * 반환: 게이팅 후의 버튼 상태. 호출자가 재대입해야 이후 파워오프 판정에 전파된다. */
static bool gate_power_button(bool power_button_pushed, bool mapping_connected)
{
    if (mapping_connected || (s_sysctl.isd_disconnection_counter < 300))
    {
#if TDC_LED_DBG_LONG_TOUCH_IGNORE
        if (power_button_pushed) { tdc_led_request(TDC_LED_SRC_DBG, TDC_LED_ST_DBG_LONG_TOUCH_IGNORE); }
#endif
        return false;
    }
    return power_button_pushed;
}

/* 파워오프 시퀀스 진행 / 탈출 판정.
 * very_low_battery 는 탈출 조건에서 읽는다 (내부기 부착 중 오진입 대응). */
static void handle_poweroff(bool very_low_battery, bool conneded_ISD, tdc_sys_state_t *out_state)
{
    /* burst 종료 검출 - pending flag 는 tdc_led_request() 가 set, led_engine_run() 이
     * burst 자가 해제 시 clear. timer/tick 무관 정확. */
    if (!tdc_led_is_burst_pending() && (s_sysctl.poweroff_start_counter != 0))
    {
        s_sysctl.poweroff_enabled          = false;
        s_sysctl.isd_disconnection_counter = 0;
        out_state->enablePMIC              = false;
        out_state->systemOff               = true;
        TDC_PRINTF_I("[SYSTEM] GO TO SYSTEM OFF \r\n");
    }
    else if (!very_low_battery && conneded_ISD)
    {
        /* 내부기 부착 과정에서 버튼이 눌려 파워오프 루틴에 들어온 경우 탈출 */
        s_sysctl.poweroff_enabled = false;
    }

    s_sysctl.poweroff_start_counter++;
}

/* 정상 운영: 저배터리 자극 알림 / ISD 미연결 카운터 / 3분 타임아웃.
 * 반환: 자극 트리거 발생 여부 (호출자가 출력에 반영). */
static bool handle_running(int battery_percent, bool conneded_ISD, tdc_sys_state_t *out_state)
{
    bool stimulation_trigger = false;

    /* LED 판정은 Arbiter 로 이관됨 (Rev.3) - Battery/ISD/Mapping 요청은 main.c 담당 */

    /* 저배터리 자극 알림 (10분 주기) - LED 와 독립된 기능. 20% 미만. */
    if (conneded_ISD && (battery_percent < 20))
    {
        if (s_sysctl.low_batt_indicator_counter == 0)
        {
            stimulation_trigger                 = true;
            s_sysctl.low_batt_indicator_counter = df_lowbatteryIndicationPeriod_ms;
        }
        s_sysctl.low_batt_indicator_counter--;
    }
    else
    {
        s_sysctl.low_batt_indicator_counter = 0;
    }

    if (conneded_ISD)
    {
        s_sysctl.isd_disconnection_counter = 0;
    }
    else
    {
        s_sysctl.isd_disconnection_counter++; /* 내부기 미연결 시 해제 카운트 증가 */
    }

    if (s_sysctl.isd_disconnection_counter > 180000) /* 3분 */
    {
        s_sysctl.isd_disconnection_counter = 0;

        tdc_led_request(TDC_LED_SRC_POWER, TDC_LED_ST_POWER_OFF);
        out_state->enable_ISD           = false;
        s_sysctl.poweroff_start_counter = 0;
        s_sysctl.poweroff_enabled       = true;
    }

    return stimulation_trigger;
}

/* 충전기 미연결 상태의 정상 운영 경로.
 * 반환: 자극 트리거 발생 여부. */
static bool handle_discharging(int battery_percent, bool power_button_pushed, bool conneded_ISD,
                               bool mapping_connected, tdc_sys_state_t *out_state)
{
    out_state->BLE_Off = false;

    if (!s_sysctl.start_flag)
    {
        /* tdc_led_turn_off · tdc_led_request(POWER, POWER_ON) · tdc_touch_init_begin 은
         * tdc_sys_init() P3-Early 에서 직접 수행 (Rev.4 이관).
         * 본 분기는 부팅 후 첫 진입 마커만 셋업. */
        s_sysctl.start_flag = true;
        TDC_PRINTF_I("[SYSTEM] FIRST POWER-ON SYSTEM CONTROL TICK \r\n");
        return false;
    }

    /* burst pending 중에는 아무 판정도 하지 않는다 (구 구조 유지) */
    if (tdc_led_is_burst_pending())
    {
        return false;
    }

    out_state->enable_ISD = true;
    out_state->enablePMIC = true;

    power_button_pushed = gate_power_button(power_button_pushed, mapping_connected);

    /* 배터리 방전 상태 확인 (전기기계적안정성 시험을 위해 저전력 범위 변경).
     * 40% 미만이면 저전력. */
    bool very_low_battery = (battery_percent < 40);

    /* 전원 끄기 시작 (1회 설정) */
    if ((very_low_battery || power_button_pushed) && !s_sysctl.poweroff_enabled)
    {
        if (very_low_battery)
        {
            TDC_PRINTF_W("[SYSTEM] VERY LOW BATTERY \r\n");
        }
        if (power_button_pushed)
        {
            TDC_PRINTF_D("[SYSTEM] POWER BUTTON PUSHED \r\n");
        }

        tdc_led_request(TDC_LED_SRC_POWER, TDC_LED_ST_POWER_OFF);
        out_state->enable_ISD           = false;
        s_sysctl.poweroff_start_counter = 0;
        s_sysctl.poweroff_enabled       = true;

        TDC_PRINTF_I("[SYSTEM] LED PATTERN IS POWER OFF \r\n");
    }

    if (s_sysctl.poweroff_enabled)
    {
        handle_poweroff(very_low_battery, conneded_ISD, out_state);
        return false;
    }

    return handle_running(battery_percent, conneded_ISD, out_state);
}

/* conneded_ISD / mappingConnected 는 '지난 tick' 값이다 - tdc_isd_step() 와
 * tdc_ble_communication_step() 이 본 함수의 enable_ISD 를 받아 도는 순환 구조라 같은 tick
 * 안에서는 확정되지 않는다. 다만 두 입력의 소비처는 모두 시간 누적 판정
 * (isd_disconnection_counter / 저배터리 10분 주기)이거나 인간 조작 스케일
 * (파워오프 탈출 / 매핑 중 버튼 무시)이라 1-tick(=1ms) 지연은 무해하다.
 * 지연에 민감한 값을 이 순환에 태우지 말 것. */
tdc_sys_state_t tdc_sys_control_step(tdc_sys_error_code_t mcuErrorCode,  //
                                     ST__USB_CONNECTOR    chargerState,
                                     int                  battery_percent,
                                     bool                 powerButtonPushed,
                                     bool                 conneded_ISD,
                                     bool                 mappingConnected)
{
    bool stimulation_trigger = false;

    bool no_error = (mcuErrorCode.dataProcessingErrorFlag == en__NA)         //
                    && (mcuErrorCode.accelerometerErrorFlag == en__NA)       //
                    && (mcuErrorCode.FPGA_CommunicationErrorFlag == en__NA)  //
                    && (mcuErrorCode.data_logging_error == en__NA);

    if (!no_error)
    {
        handle_error(mcuErrorCode);
        systemStatus.StimulationIndicatorTriggerLowPower = stimulation_trigger;
        return systemStatus;
    }

    /* 에러 해제 시 ERROR 소스 클리어 */
#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(TDC_LED_SRC_ERROR))
#endif
        tdc_led_request(TDC_LED_SRC_ERROR, TDC_LED_ST_NONE);

    /* 충전기 미연결(df_Defalut) 분기는 할 일이 없어 제거했다. 충전기가 꼽히면
     * 하드웨어적으로 리셋되므로 부팅 직후엔 df_Defalut 로 들어온다. */
    if (chargerState.chargerConnectorPluggedIn == df_Connected)
    {
        handle_charging(chargerState, powerButtonPushed, &systemStatus);
    }
    else if (chargerState.chargerConnectorPluggedIn == df_Disconnected)
    {
        /* 크래들 착탈 tick 은 판정 보류 - 이전 tick 과 상태가 같을 때만 진행.
         *
         * 구 Sullivan 방어 분기 제거(2026-07-15): 원본은 else 에서
         * "prev_carryingCase == df_Disconnected 이면 systemOff" 를 했으나,
         * Sound1 은 포고핀 크래들 단일 경로라 tdc_pwr_charger_set_state() 가 두
         * 필드를 항상 같은 값으로 설정한다(tdc_pwr_battery.c). 따라서 그 조건은
         * 항상 거짓이었다. 상세: docs/tasks/main/20260715_systemcontrol-fsm-decompose/
         * 분석-부록-sullivan유산.md */
        if (s_sysctl.prev_carrying_case_state == chargerState.carryingCasePluggedIn)
        {
            stimulation_trigger = handle_discharging(battery_percent, powerButtonPushed,
                                                     conneded_ISD, mappingConnected, &systemStatus);
        }
    }

    s_sysctl.prev_carrying_case_state = chargerState.carryingCasePluggedIn;

    systemStatus.StimulationIndicatorTriggerLowPower = stimulation_trigger;

    return systemStatus;
}
