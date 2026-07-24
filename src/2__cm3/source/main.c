/**
 * @file main.c
 */

#include <tdc_sys_init.h>
#include <main.h>

#include <board.h>           //ok
#include <tdc_hal_spi.h>      //ok
#include <tdc_hal_i2c_cfx.h>  //ok

#include <tdc_pwr_battery.h>  //ok

#include <tdc_shm.h>  //ok
#include <tdc_sys_error.h>          //ok
#include <tdc_sys_control.h>  //ok

#include <tdc_isd.h>   //ok
#include <tdc_ble_mapping.h>  //ok
#include <tdc_ble_remote.h>   //ok

#include <tdc_led_output.h>           //ok
#include <tdc_ble_communication.h>   //ok
#include <tdc_sys_earpiece.h>      //ok
#include <tdc_stim_indicator.h>   //ok
#include <tdc_stim_para_cal.h>  //ok

#include <tdc_drv_isl9122.h>  //ok

#include <tdc_touch.h>        /* 공개 API + tdc_touch_time.h(ULP 시간상수) 재노출 */
#include <tdc_touch_iqs323.h> /* 절전 진입 IQS323 직접 호출 */

#include <tdc_isd_stim_standalone.h>  // 신규 추가 for I2S 디버깅
#include <tdc_isd_init_fpga.h>              // 절전 모드 진입 전 FPGA 리셋 목적
#include <tdc_isd_fpga.h>

#include <tdc_pwr_clock.h>
#include <tdc_hal_dio.h>
#include <tdc_pwr_clock.h>
#include <tdc_hal_timer.h>
#include <tdc_printf.h>
#include <tdc_hal_i2c.h>  //ok  - Sleep 진입 시 I2C PRESCALE 런타임 재설정용

#include <SEGGER_RTT_Wrapper.h>
#include <aes.h>

#include <tdc_qcc.h>

#if defined(ENABLE_UI_CMD)
#include <tdc_ui_command.h>
#endif

/* 배열 원소 개수. 배열 정의가 바뀌어도 순회 길이가 자동 추종한다(매직넘버 방지). */
#define TDC_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/* func_normal() 메인 루프 상태 (tick 간 유지).
 * iteration 이 갱신하고 루프 후반(타임아웃/크래들/절전 판정)이 읽는 값만 담는다. */
typedef struct
{
    tdc_sys_state_t            systemState;
    volatile ST__ISD_STATUS     isd_state;
    ST__BLE_COMMUNICATION_STATE ble_state;
    bool                        qcc_batt_timeout;
} tdc_normal_ctx_t;

/* 1 tick 분 입력 스냅샷 (tick 내에서만 유효).
 * 수집을 한곳에 모아 handler 가 일관된 값을 보게 한다 - 특히 batt_percent 는
 * 기존에 tdc_sys_control_step / LED 요청이 각자 tdc_pwr_battery_get_percent() 를 호출해
 * 한 tick 안에서 서로 다른 값을 볼 여지가 있었다. */
typedef struct
{
    tdc_sys_error_code_t    mcu_error;
    ST__USB_CONNECTOR charger;
    int               batt_percent;
    bool              power_button;
    bool              batt_timeout;
} tdc_normal_events_t;

static bool tdc_qcc_has_batt_level_rx_timed_out(void);
static void tdc_print_default_isd_info(void);
static void tdc_wait_for_cfx_start(void);
static void tdc_normal_boot_sequence(void);
static void tdc_collect_events(tdc_normal_events_t *ev);
static void tdc_handle_events(const tdc_normal_events_t *ev, tdc_normal_ctx_t *ctx);
static void tdc_normal_iteration(tdc_normal_ctx_t *ctx);
static void tdc_apply_mapping_mode(bool mapping_connected);
static void tdc_update_led_requests(int batt_percent, bool isd_conn_default, bool map_conn_default);
static bool tdc_handle_qcc_batt_timeout(bool *poweroff_started);
static bool tdc_can_enter_sleep(bool map_active);

typedef struct
{
    uint8_t version[3];
    uint8_t buildData[11];
} FirmWare_Info;

// clang-format off
__attribute__((section(".cm3_manu_reserved"),used,aligned(4)))
volatile unsigned char g_cm3_manu_reserved[0xC0] =
{
    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  /* 0x00 ~ 0x0F */
    17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  /* 0x10 ~ 0x1F */
    33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  /* 0x20 ~ 0x2F */
    49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  /* 0x30 ~ 0x3F */
    65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  /* 0x40 ~ 0x4F */
    81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  /* 0x50 ~ 0x5F */
    97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, /* 0x60 ~ 0x6F */
    113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, /* 0x70 ~ 0x7F */
    129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, /* 0x80 ~ 0x8F */
    145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, /* 0x90 ~ 0x9F */
    161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, /* 0xA0 ~ 0xAF */
    177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192  /* 0xB0 ~ 0xBF */
};
// clang-format on

static const FirmWare_Info firmwareInfo = {1, 0, 0, __DATE__};

static const int devFwVer_type = DEV_FW_VER_RELEASE;  // 내부 개발 버전 (Release)
static const int devFwVer_num  = 1;                   // 1 (전기기계적안정성시험)

#if 1
#endif

char *readFirmwareInfo()
{
    return (char *) &firmwareInfo;
}

void tdc_sys_init(void);

static bool iterationFlag = false;

void enable_iteration(void)
{
    iterationFlag = true;
}

void disable_iteration(void)
{
    iterationFlag = false;
}

void update_mapNum(void)
{
    static bool prev_userSettingValueLoadedFlag = false;

    int  iterNum, mapNum;
    int *p_connected_isd_usableMapIndex;
    bool userSettingValueLoadedFlag = false;

    // 참고:
    // tdc_isd_step() 함수에서, tdc_isd_path_open() 함수를 통해 내부기 연결이 올바르게 인식되면,
    // 해당 내부기의 ISD 번호에 맞는 맵 데이터를 공유 메모리로 복사하고, 연결된 ISD 번호를 업데이트한다.
    // CFX가 ISD 번호를 확인 후, 사용자 설정 값이 로드 되었다는 의미의 플래그를 설정하게된다.
    // 아래는 이 과정이 다 이뤄졌는지 확인하는 과정이다.

    // 사용자 설정 값이 로드 되었다는 의미의 플래그가 설정 된 이후,
    // CM3에서 사용 가능한 맵 번호 판별 과정 등이 완료되면,
    // CFX에게 맵 번호가 바뀌었다는 정보를 tdc_shm_change_program_map_num() 함수로 알려준다.
    // 추가로, CM3 스스로에게 새로운 맵으로 자극 관련 파라미터의 계산을 다시 하도록 플레그를 세팅한다.
    // 플래그는 전역변수 newMapLoadeFlagForStimulParaCalculation를 true로 설정하는 것이다.

    // CFX는 Normal_PowerMode_event_mapChange() 함수에서 관련 처리를 한 후
    // 공유 메모리의 cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag를 1로 설정한다.

    // 여기까지 완료되면, tdc_isd_step() 함수에서, isd_controlState가 en__isdStatus_stimul_10V_Ok인 상태의
    // tdc_isd_stim_standalone_step() 함수 내부의 if (tdc_shm_is_map_data_loaded_cfx()) 블록이 수행되는 구조이다.

    // CFX에서 사용자 설정값 읽어 들여졌는지 확인.
    userSettingValueLoadedFlag = tdc_shm_is_user_setting_value_loaded_cfx();

    // 사용자 설정값이 eeprom에서 읽혀졌는가
    if (userSettingValueLoadedFlag)
    {
        // 부팅 후 처음 읽여졌을 때 맵번호를 사용자 설정값에 저장된 번호로 세팅한다.
        // 사용자 설정값이 읽어들여지면 CFX에 맵데이터를 읽어 들이라고 명령을 보내기
        // 위해서 changeProgramMapNum함수를 실행한다.
        if (prev_userSettingValueLoadedFlag != userSettingValueLoadedFlag)
        {
            mapNum                         = tdc_shm_read_program_map_num();
            p_connected_isd_usableMapIndex = tdc_shm_read_connected_isd_usable_map_index();
            iterNum                        = 0;

            TDC_PRINTF_D("[UPDATE MAP] NUMBER=%d \r\n", tdc_shm_read_program_map_num());
            TDC_PRINTF_D("[UPDATE MAP] CONNECTED ISD'S USABLE MAP INDEX=%d (= MAP NUM - 1) \r\n", p_connected_isd_usableMapIndex[mapNum - 1]);

            // 설정된 사용자 맵 번호가 사용이 불가능한 맵 번호로 되어 있을 경우.
            if (p_connected_isd_usableMapIndex[mapNum - 1] == 0)
            {
                do
                {
                    mapNum++;
                    iterNum++;
                    if (iterNum > MaxNumMap)
                    {
                        break;
                    }
                } while (p_connected_isd_usableMapIndex[mapNum - 1] == 0);
            }

            TDC_PRINTF_D("[UPDATE MAP] MAP NUM=%d, ITERATION NUM=%d \r\n", mapNum, iterNum);

            if (iterNum < MaxNumMap)
            {
                TDC_PRINTF_I("[UPDATE MAP] CHANGE PROGRAM MAP NUM (%d)\r\n", mapNum);
                tdc_shm_change_program_map_num(mapNum);

                // NOTE: 위 tdc_shm_change_program_map_num() 함수는 전체적으로 아래 코드를 수행하는 꼴임.
                // cfx_cm3_sharedMemoryAll.userSettingValue.mapNum                = mapNum;
                // cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange     = 1;
                // cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag = 0;
                // tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);
                // newMapLoadeFlagForStimulParaCalculation = true; ← tdc_isd_stim_standalone.c의 전역변수
            }
            else
            {
                tdc_sys_error_update(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
            }
        }
    }

    prev_userSettingValueLoadedFlag = userSettingValueLoadedFlag;
}

/* 메인 루프 생존 카운터. 매 iteration 증가시켜 공유 메모리에 게시한다
 * (tdc_shared_publish_cm3_heartbeat). 펌웨어 내 소비자는 없고 디버거로
 * CM3 가 살아 있는지 확인하는 용도다. 구 이름 main_counter, 비-static 전역이었으나
 * 외부 참조가 없어 static 으로 좁혔다(2026-07-20). */
static int s_cm3_heartbeat = 0;

/*
 * sections.ld 에 정의된 심볼들 (반드시 동일 이름)
 */
extern uint8_t __data_init__;   // LMA (PRAM 쪽, 초기값 블록 시작)
extern uint8_t __data_start__;  // VMA (DRAM .data 시작)
extern uint8_t __data_end__;    // VMA (DRAM .data 끝)

// sk5_start.h 에 존재하여 아래는 주석
// extern uint8_t __bss_start__;  // VMA (DRAM .bss 시작)
// extern uint8_t __bss_end__;    // VMA (DRAM .bss 끝)

void load_data_section(void)
{
#if 0
    memcpy(&__data_start__,                            // VMA data 영역의 시작부터
           &__data_init__,                             // LMA data 영역의 값으로
           (size_t) (&__data_end__ - &__data_start__)  // VMA data 영역의 크기 만큼 초기화
    );
#else
    uint32_t *src = (uint32_t *) &__data_init__;
    uint32_t *dst = (uint32_t *) &__data_start__;

    while (dst < ((uint32_t *) &__data_end__))
    {
        *dst++ = *src++;
    }
#endif
}

void load_bss_section(void)
{
#if 0
    memset(&__bss_start__,                           // VMA bss 영역의 시작부터
           0,                                        // 0 값으로
           (size_t) (&__bss_end__ - &__bss_start__)  // VMA bss 영역의 크기 만큼 초기화
    );
#else
    memset(&__bss_start__,                                                   // VMA bss 영역의 시작부터
           0,                                                                // 0 값으로
           (size_t) ((uintptr_t) &__bss_end__ - (uintptr_t) &__bss_start__)  // VMA bss 영역의 크기 만큼 초기화
    );
#endif
}

int main(void)
{
    /* .data 및 .bss 섹션 데이터가 PRAM에서 올바르게 로드되지 못하는 이슈 발생.
     * 이슈를 해결하기 위해 일반적인 펌웨어 'warm reset' 방법을 사용함.
     * 즉, PRAM의 LMA, DRAM의 VMA 영역을 직접 초기화 하는 방법으로 해결. */
    load_data_section();
    load_bss_section();

    Sys_DIO_Config(DIO33, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_NO_PULL | DIO_MODE_GPIO_OUT));  // DIO 설정: 오실로스코프 측정용
    Sys_GPIO_Set_Low(DIO33);

    // SWJ-DP에 대한 DIO 설정
    Sys_DIO_CM3JTAGConfig(true, false);

    // 디버거 접근 제한 해제
    D_DEBUG->CFG      = ACCESS_UNRESTRICTED;                            // I2C
    SYSCTRL->DBG_LOCK = DBG_ACCESS_UNLOCK;                              // UNLOCK
    CoreDebug->DEMCR  = CoreDebug->DEMCR | CoreDebug_DEMCR_TRCENA_Msk;  // SEGGER RTT VIEWER

    // 인터럽트 상태 초기화
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);

    // EEPROM의 WP을 방지
    Sys_DIO_Config(DIO7, (DIO_1X_DRIVE | DIO_LPF_ENABLE | DIO_WEAK_PULL_UP | DIO_MODE_GPIO_IN));

    // AES128 암호화, 복호화 테스트 함수
    // aes128_test();

    // AES128 암호화/복호화 키 정보 초기화 (NOTE: 현재 예제 키를 사용하므로, 올바른 키를 생성하여 적용해야함)
    tdc_aes_init();

    // JLink RTT를 강제 초기화 시킴 (버퍼 인덱스 이슈 발생 방지 등)
    SEGGER_RTT_Init();

    TDC_PRINTF_I("[INFO] MODEL : SOUND1 (%u.%u%u / %s) \r\n", /* lf */
              firmwareInfo.version[0],
              firmwareInfo.version[1],
              firmwareInfo.version[2],
              firmwareInfo.buildData);

    TDC_PRINTF_I("[INFO] DEV   : %d.%d \r\n", devFwVer_type, devFwVer_num);

    while (1)
    {
        func_normal();
        func_sleep();
    }
}

/* iqs323_proc() → tdc_touch.c의 tdc_touch_process()로 이동됨 */

static void func_cradle_lid_closed_loop(void);

/* ===================================================
 * 배터리/ISD/매핑 LED 요청 헬퍼 (func_normal, Rev.6)
 *   - 변화 마진(히스테리시스) 제거: QCC가 배터리 측정·필터링을 담당하므로
 *     E8300 쪽 진입/이탈 이중임계·prev 상태 추적을 두지 않는다.
 *   - 과거 updateBatteryLevel()의 "경계 테이블 → 등급" 골격을 노이즈 가드 없이 재현.
 * =================================================== */

/* 배터리 percent -> LED 등급 경계 테이블.
 * 내림차순(min_pct 큰 것부터) 필수 - 위에서부터 pct >= min_pct 첫 매치를 채택한다.
 * 등급 추가/컷 변경은 이 표만 편집하면 된다(매직넘버의 데이터화). */
typedef struct
{
    int         min_pct;
    tdc_led_state_t state;
} tdc_batt_led_bin_t;

static const tdc_batt_led_bin_t s_tdc_batt_led_table[] = {
    {80, TDC_LED_ST_BATT_READY},   /* pct >= 80         */
    {10, TDC_LED_ST_BATT_MID},     /* 10 <= pct < 80    */
    {0, TDC_LED_ST_BATT_CRITICAL}, /* pct <  10 (fallback) */
};
#define TDC_BATT_LED_TABLE_LEN ((int) (sizeof(s_tdc_batt_led_table) / sizeof(s_tdc_batt_led_table[0])))

#define TDC_MAP_LOW_BATT_PCT 20 /* pct <= 20 -> 배터리 LOW (마진 없는 단일 컷) */

/* 배터리 LED 요청. 반환값 batt_st 는 ISD 미연결 시 재송출에 재사용된다. */
static tdc_led_state_t tdc_led_request_battery(int pct, bool ovr_batt_active, bool batt_is_reset_state)
{
    tdc_led_state_t batt_st = TDC_LED_ST_BATT_CRITICAL; /* 테이블 매치 실패 시 안전측 기본값 */

    if (!ovr_batt_active && batt_is_reset_state)
    {
        /* 배터리 정보 미수신(RESET, percent=0): 판정 보류 - 최우선 분기 유지
         * (부팅 초기 pct=0 이 CRITICAL 로 새는 회귀 방지, Rev.5 근거) */
        batt_st = TDC_LED_ST_IDLE;
    }
    else
    {
        int i;
        for (i = 0; i < TDC_BATT_LED_TABLE_LEN; ++i)
        {
            if (pct >= s_tdc_batt_led_table[i].min_pct)
            {
                batt_st = s_tdc_batt_led_table[i].state;
                break;
            }
        }
    }

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(TDC_LED_SRC_BATTERY))
#endif
        tdc_led_request(TDC_LED_SRC_BATTERY, batt_st);

    return batt_st;
}

/* ISD LED 요청. isd_conn 을 반환(매핑 블록이 재사용).
 * batt_st: 내부기 미연결 시 배터리 LED 를 재송출하기 위해 전달받는다. */
static bool tdc_led_request_isd(bool isd_conn, tdc_led_state_t batt_st)
{
#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(TDC_LED_SRC_ISD))
#endif
    /* IMPORTANT: 내부기 연결 해제시 위에서 구한 배터리 레벨에 대한 LED를 켜도록 유도했다. */
    {
        if (isd_conn)  // 내부기 연결 상태
        {
            // 내부기 연결 상태에서는 연결된 내부기의 사용자 설정 정보에서 LED 제어 값을 이용해야 한다.
            // LED 표시 설정값: 1=켜기, 2=끄기. (truthy 검사는 2도 참이 되므로 == 1 로 명시 비교)
            if (tdc_shm_read_led_indicator_on_off() == 1)
            {
                // LED 표시 설정이 켜기(1)이면,
                tdc_led_request(TDC_LED_SRC_ISD, TDC_LED_ST_IN_USE);
            }
            else
            {
                // LED 표시 설정이 끄기(2)이면,
                tdc_led_request(TDC_LED_SRC_ISD, TDC_LED_ST_NONE);
            }
        }
        else  // 내부기 미 연결 상태
        {
            // 항상 LED가 켜질 수 있게 설정 정보를 LED 켜기로 강제한다.
            tdc_led_request(TDC_LED_SRC_ISD, TDC_LED_ST_NONE);
            tdc_led_request(TDC_LED_SRC_BATTERY, batt_st);
        }

        /* s_req[TDC_LED_SRC_ISD] 확정 후 게이트 갱신 - 연결 해제 전환 시
         * s_isd_conn=0 과 s_req[ISD]=NONE 사이에 TIMER_3 ISR 이 끼어들어
         * 1-tick IN_USE(백색) 잔상이 뜨던 race 를 방지하기 위해 분기 뒤(마지막)로 봉인. */
        tdc_led_set_isd_conn_state(isd_conn);
    }

    return isd_conn;
}

/* 매핑 LED 요청. 배터리 LOW(단일 컷 pct<=TDC_MAP_LOW_BATT_PCT) × ISD 연결 여부 4분기.
 * 마진 제거로 s_map_low_active static 래치를 없애고 매 호출 pct 만으로 판정(순수 계산). */
static void tdc_led_request_mapping(int pct, bool map_conn, bool isd_conn)
{
    bool map_low_active = (pct <= TDC_MAP_LOW_BATT_PCT);

#ifdef ENABLE_UI_CMD
    if (!tdc_ui_command_is_led_override(TDC_LED_SRC_MAPPING))
#endif
    {
        if (map_conn)
        {
            tdc_led_state_t map_st;
            if (map_low_active)
            {
                map_st = isd_conn ? TDC_LED_ST_MAPPING_ISD_BATT_LOW : TDC_LED_ST_MAPPING_NO_ISD_BATT_LOW;
            }
            else
            {
                map_st = isd_conn ? TDC_LED_ST_MAPPING_ISD_BATT_READY : TDC_LED_ST_MAPPING_NO_ISD_BATT_READY;
            }
            tdc_led_request(TDC_LED_SRC_MAPPING, map_st);
        }
        else
        {
            tdc_led_request(TDC_LED_SRC_MAPPING, TDC_LED_ST_NONE);
        }
    }
}

int func_normal(void)
{
    /* zero-init 후 필요한 필드만 명시 초기화.
     * systemState 를 0 으로 깔아두는 것이 중요하다 - 첫 iteration 전(iterationFlag
     * 가 아직 false)에도 루프 후반이 systemState.cradleLidClosed 를 읽기 때문에,
     * 미초기화 상태면 스택 쓰레기값으로 크래들 루프에 오진입할 수 있다. */
    tdc_normal_ctx_t ctx = {
        .isd_state = {en__isdStatus_PowerIC_Reset, false},
        .ble_state = {en__isdStatus_PowerIC_Reset, false, false, false},
    };

    /* QCC 0x34(배터리) 수신 타임아웃 → 파워오프 패턴 후 절전 진입.
     * 지역변수(static 아님): 절전 후 func_normal 재진입 시 false로 리셋되어
     * 무한 재절전을 방지한다(tdc_qcc_has_batt_level_rx_timed_out() 내부 is_done 은
     * static 이라 유지되므로 타임아웃 재감지도 차단된다). */
    bool qcc_timeout_poweroff_started = false;

    SYS_WATCHDOG_REFRESH();  // 시작 시 처음에 워치독 리프레시

    s_cm3_heartbeat                                   = 0;
    cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded = 0;

    ctx.systemState.systemOff = false;

    tdc_normal_boot_sequence();

    while (1)
    {
        if ((iterationFlag == true))
        {
            tdc_normal_iteration(&ctx);
        }  // 끝, iteration

        s_cm3_heartbeat++;
        tdc_shared_publish_cm3_heartbeat(s_cm3_heartbeat);

        /* QCC 배터리 타임아웃 → 파워오프 패턴 후 절전.
         * QCC 미수신 시 tdc_sys_control_step 은 df_Default 게이트(TDC_LED_PATTERN_NA)에 막혀 자체
         * 파워오프 시퀀스에 도달하지 못하므로, 여기서 직접 POWER_OFF 패턴을 요청하고
         * burst 완료 후 systemOff 를 세팅해 기존 절전 경로(아래 → break → func_sleep)를 탄다.
         * 상세: docs/tasks/power/20260609_qcc-batt-timeout-sleep/분석.md §4 */
        if (ctx.qcc_batt_timeout && tdc_handle_qcc_batt_timeout(&qcc_timeout_poweroff_started))
        {
            ctx.systemState.systemOff = true;
        }

        /* 크래들 뚜껑 닫힘 첫 감지 → 약 절전 루프 (ISD 연결 중이면 차단) */
        if (ctx.systemState.cradleLidClosed && !ctx.isd_state.conneded_ISD)
        {
            tdc_led_force_fade_off(); /* fade-out ISR 완료 후 LED 완전 소등 */
            func_cradle_lid_closed_loop();
            /* 도달 불가 - 루프 내 SYS_WATCHDOG_RESET()으로 재부팅 */
        }

        if (ctx.systemState.systemOff == true)
        {
            if (tdc_can_enter_sleep(ctx.ble_state.mappingConnection))
            {
                /* cross-fade Phase A 강제 - POWER_OFF burst 직후 다른 best
                 * (BATTERY/ISD/MAPPING) 로 진입한 새 색 (예: GREEN) 이
                 * tdc_led_turn_off() 에서 fade-out 되어 잔상으로 보이는 현상 방지.
                 * 모든 src 를 TDC_LED_ST_NONE 으로 강제하고 FADE_MAX_MS+10 동안
                 * 자연 fade-out 진행 후 break. */
                tdc_led_force_fade_off();
                break; /* Escape this main loop to enter the ULP mode */
            }

            ctx.systemState.systemOff = false; /* 보류 - 다음 iteration 에서 트리거 재평가 */
        }

        SYS_WATCHDOG_REFRESH();
        SYS_WAIT_FOR_INTERRUPT;
    }  // 끝, while

    return 0;
}

/* 노말 모드 부팅 시퀀스: CFX 기동 대기 -> 하드웨어 초기화 -> CFX iteration 개방
 * -> UI 커맨드 초기화 -> default ISD 정보 출력. 메인 루프 진입 전 1회.
 *
 * CFX iteration을 활성화시키면, CFX가 FIFO 등을 초기화 한 후 PCM FillZero 모드로 동작하게 됨
 * 여기서 iteration을 활성화 한 뒤에야 CFX는 노말 모드에 대한 초기화 과정을 수행한다는 뜻이다.
 * 단, 노말 모드 초기화 과정에서 g_ISR_Flag_CM3_FS_Init = 1; 을 자체적으로 적용하기 때문에
 * 노말 모드 iteration 내부의 fn_systemControl_NormalMode() 함수에서
 * ISD Information을 FS 메모리에서 공유 메모리로 복사하고 CFX_EEPROM_data_is_Loaded = 1; 을 적용한다. */
static void tdc_normal_boot_sequence(void)
{
    tdc_wait_for_cfx_start();  // CFX가 자체적으로 플래그를 설정할 때까지 대기

    tdc_sys_init();

    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 1;  // CFX 동작 활성화

#ifdef ENABLE_UI_CMD
    tdc_ui_command_init();
#endif

    tdc_print_default_isd_info();  // default ISD 정보 출력
}

/* 입력 수집 - 부작용 없이 읽기만 한다.
 * 예외: tdc_touch_process() 와 tdc_qcc_has_batt_level_rx_timed_out() 은 내부에
 * 자체 FSM/타이머를 돌리므로 tick 당 정확히 1회 호출해야 한다. */
static void tdc_collect_events(tdc_normal_events_t *ev)
{
    ev->mcu_error    = tdc_sys_error_read();
    ev->charger      = tdc_pwr_charger_get_state();  // QCC 0x34 기반
    ev->batt_percent = tdc_pwr_battery_get_percent();   // QCC 제공. tdc_sys_init 단계에서 수집 완료.
    ev->power_button = tdc_touch_process();      // tdc_shm_is_power_button_pushed() 대체
    ev->batt_timeout = tdc_qcc_has_batt_level_rx_timed_out();
}

/* 수집된 입력으로 상태를 전이시키고 출력에 반영한다.
 * 순서 의존: tdc_sys_control_step -> tdc_isd_step -> tdc_ble_communication_step (enable_ISD 전달).
 * LED 요청은 배터리 -> ISD -> 매핑 순서에 의존한다(tdc_update_led_requests 내부). */
static void tdc_handle_events(const tdc_normal_events_t *ev, tdc_normal_ctx_t *ctx)
{
    ctx->qcc_batt_timeout = ev->batt_timeout;

    // NOTE: QCC에게 0x34(Power info) 프로토콜 수신 전까지는
    //       charger.chargerConnectorPluggedIn == df_Default; 상태이다.
    //       df_Default 상태일 때는 아래의 tdc_sys_control_step() 에서 동작하는게 없다.

    ctx->systemState = tdc_sys_control_step(ev->mcu_error,
                                     ev->charger,
                                     ev->batt_percent,
                                     ev->power_button,
                                     ctx->isd_state.conneded_ISD,      // 지난 tick 값 (순환 의존 - tdc_sys_control_step.c 주석 참조)
                                     ctx->ble_state.mappingConnection  // 지난 tick 값
    );

    update_mapNum();  // 맵데이터 업데이트

    ctx->isd_state = tdc_isd_step(ctx->systemState.enable_ISD,  //
                                   ctx->ble_state.mappingConnection,
                                   ctx->ble_state.isdControlCommand  //
    );

    /* 매핑 연결 상태이고,
     * ctx->isd_state.isd_controlState >= en__isdStatus_stimul_10V_Ok 이면,
     * ctx->isd_state.connededISD == true 상태이다. */

    ctx->ble_state = tdc_ble_communication_step(ctx->isd_state);

    tdc_stim_indicator_out(tdc_shm_read_stimul_indicator_on_off(),  //
                             ctx->systemState.StimulationIndicatorTriggerLowPower,
                             ctx->ble_state.StimulationIndicatorTrigger  //
    );

    // PMIC 켜고/끄기
    tdc_shm_on_off_3_v_pmic_cm3_to_cfx(ctx->systemState.enablePMIC);

    tdc_apply_mapping_mode(ctx->ble_state.mappingConnection);

    tdc_update_led_requests(ev->batt_percent, ctx->isd_state.conneded_ISD, ctx->ble_state.mappingConnection);

    /* tdc_led_arbiter_tick() 은 Timer 3 ISR 에서 직접 구동 (tdc_hal_timer.c).
     * main loop 의 I2C/EEPROM 폴링 블록으로 인한 fade/PWM jitter 회피. */

    tdc_sys_control_nrf_on_off(ctx->isd_state, ctx->systemState.BLE_Off, ctx->ble_state.mappingConnection, ctx->ble_state.BLE_Off_Command);

#ifdef ENABLE_UI_CMD
    tdc_ui_command_set_mapping_connected(ctx->ble_state.mappingConnection);
    tdc_ui_command_poll();
#endif
}

/* 메인 루프 1 iteration: collect -> handle -> iteration 종료. */
static void tdc_normal_iteration(tdc_normal_ctx_t *ctx)
{
    tdc_normal_events_t ev;

    tdc_collect_events(&ev);
    tdc_handle_events(&ev, ctx);

    // 중요!!
    disable_iteration();
}

/* CFX 가 자체 플래그를 세울 때까지 블로킹 대기. tdc_sys_init() 선행 조건. */
static void tdc_wait_for_cfx_start(void)
{
    while (1)
    {
        if (cfx_cm3_sharedMemoryAll.is_CFX_started == 1)
        {
            TDC_PRINTF_D("[INFO] CFX STARTED \r\n");
            break;
        }

        __NOP();  // 최적화 방지 및 메모리 접근 경쟁 상태 방지 목적
    }
}

/* 매핑 연결 여부에 따라 시스템 모드 플래그와 CFX 공유 상태를 함께 전환. */
static void tdc_apply_mapping_mode(bool mapping_connected)
{
    if (mapping_connected)
    {
        tdc_shm_change_system_mode_flag(en__mappingMode);
        tdc_shm_share_mapping_program_connection(true);
    }
    else
    {
        tdc_shm_change_system_mode_flag(en__normalMode);
        tdc_shm_share_mapping_program_connection(false);
        // tdc_sys_earpiece_update_status();
    }
}

/* LED source requests (Rev.3).
 * batt_percent 는 이번 tick 스냅샷(tdc_collect_events). isd_conn_default /
 * map_conn_default 는 UI 커맨드 override 가 없을 때 쓰는 실제 상태.
 *
 * Battery (SS4.5) - 마진 제거: QCC가 배터리 측정·필터링을 담당하므로 진입/이탈
 * 이중임계·prev 추적 없이 단일 컷 테이블로 판정(tdc_led_request_battery).
 * RESET(0x34 수신 전, percent=0) 시 부팅 초기 CRITICAL 누출 방지를 위해 IDLE 최우선 분기.
 *
 * ISD (SS4.6) - 배터리→ISD 순서 의존(미연결 시 batt_st 재송출) + tdc_led_set_isd_conn_state()
 * 봉인(1-tick 잔상 race 방지)은 tdc_led_request_isd() 내부에 유지.
 *
 * Mapping (SS4.4) - 배터리 LOW(단일 컷 pct<=TDC_MAP_LOW_BATT_PCT) × ISD 연결 여부 4분기.
 * 마진(s_map_low_active 래치) 제거 -> 매 호출 pct 만으로 판정(tdc_led_request_mapping). */
static void tdc_update_led_requests(int batt_percent, bool isd_conn_default, bool map_conn_default)
{
#ifdef ENABLE_UI_CMD
    bool ovr_batt_active = tdc_ui_command_override_battery_active();
    int  pct             = ovr_batt_active ? (int) tdc_ui_command_override_battery_percent() : batt_percent;
#else
    bool ovr_batt_active = false;
    int  pct             = batt_percent;
#endif
    bool        batt_is_reset_state = (tdc_pwr_battery_get_state() == TDC_PWR_BATTERY_STATE_RESET);
    tdc_led_state_t batt_st             = tdc_led_request_battery(pct, ovr_batt_active, batt_is_reset_state);

#ifdef ENABLE_UI_CMD
    bool isd_conn_raw = tdc_ui_command_override_isd_active() ? tdc_ui_command_override_isd_value() : isd_conn_default;
#else
    bool isd_conn_raw = isd_conn_default;
#endif
    bool isd_conn = tdc_led_request_isd(isd_conn_raw, batt_st);

#ifdef ENABLE_UI_CMD
    bool map_conn = tdc_ui_command_override_map_active() ? tdc_ui_command_override_map_value() : map_conn_default;
#else
    bool map_conn = map_conn_default;
#endif
    tdc_led_request_mapping(pct, map_conn, isd_conn);
}

/* QCC 배터리 타임아웃 처리. 첫 호출에서 POWER_OFF 패턴을 요청하고, 이후 burst 가
 * 끝나면 true 를 반환해 호출자가 절전(systemOff)을 트리거하게 한다.
 *
 * poweroff_started 는 호출자의 지역변수 포인터다 - func_normal 재진입 시 false 로
 * 리셋되어야 무한 재절전이 방지되므로 static 으로 승격하지 않는다. */
static bool tdc_handle_qcc_batt_timeout(bool *poweroff_started)
{
    if (!*poweroff_started)
    {
        TDC_PRINTF_W("[BATT] QCC BATT TIMED-OUT -> LED PATTERN = POWER OFF \r\n");
        tdc_led_request(TDC_LED_SRC_POWER, TDC_LED_ST_POWER_OFF);
        *poweroff_started = true;
        return false;
    }

    return !tdc_led_is_burst_pending();
}

/* 절전 진입 가부 판정. 매핑 / 페어링 / OTA 진행 중에는 false(보류).
 *
 * BLE 활성 검사는 Arbiter src 요청을 직접 본다 - tdc_led_get_ind_state() 는 BLE 가
 * set 한 후 NONE 으로 reset 안 보내면 잔존하기 때문. */
static bool tdc_can_enter_sleep(bool map_active)
{
    tdc_led_state_t ble_st      = tdc_led_get_request(TDC_LED_SRC_BLE_IND);
    bool        pair_active = (ble_st == TDC_LED_ST_PAIR);
    bool        ota_active  = (ble_st == TDC_LED_ST_OTA_QCC) || (ble_st == TDC_LED_ST_OTA_EZAIRO);

    if (map_active || pair_active || ota_active)
    {
        TDC_PRINTF_W("[SYSTEM] SLEEP DEFERRED (map=%d pair=%d ota=%d ble_st=%d) \r\n", map_active, pair_active, ota_active, (int) ble_st);
        return false;
    }

    TDC_PRINTF_I("[SYSTEM] ENTERING SLEEP MODE \r\n");
    return true;
}

static bool tdc_qcc_has_batt_level_rx_timed_out(void)
{
    static int  time_laps = 0;
    static bool is_done   = false;
    static bool timed_out = false;

    if (!is_done)
    {
        if (time_laps == 0)  // 최초 시간 업데이트. state 와 무관하게 기점을 잡아야
        {                    // 첫 호출부터 수신 완료인 경우에도 WAIT 로그가 유효하다.
            time_laps = tdc_hal_timer_get_tick();
        }

        if (tdc_pwr_battery_get_state() == TDC_PWR_BATTERY_STATE_RESET)
        {
            if (RX_BATT_LEVEL_TIME_OUT_MS < (tdc_hal_timer_get_tick() - time_laps))
            {
                TDC_PRINTF_W("[BATT] QCC BATT TIMED-OUT -> POWER OFF (SLEEP) \r\n");
                is_done   = true;
                timed_out = true;  // 시간 초과 발생
            }
        }
        else
        {
            TDC_PRINTF_D("[BATT] QCC BATT RX %d%% (TICK = %d / WAIT = %d MS) \r\n", tdc_pwr_battery_get_percent(), tdc_hal_timer_get_tick(), (tdc_hal_timer_get_tick() - time_laps));
            is_done = true;
        }
    }

    return timed_out;
}

static void tdc_print_default_isd_info(void)
{
    /* 6개 필드가 모두 같은 isd_info 를 가리키므로 베이스 포인터 하나로 통일.
     * 순회 길이는 배열 정의(cfx_link/tdc_shm.h)에서 파생 - 크기 변경 시 자동 추종. */
    ST__CFX_CM3_SharedMemory_ISD_info *p_isd_info = &g_tdc_fs_ptr_entire_map->map[0].isd_info;

    TDC_PRINTF_W("[INFO] BOOT ISD 1 INFO \r\n");
    TDC_PRINTF_W("[INFO] NAME : ");
    for (size_t name_i = 0; name_i < TDC_ARRAY_LEN(p_isd_info->isd_userName); name_i++)
    {
        if (p_isd_info->isd_userName[name_i] != 0)
        {
            TDC_PRINTF_W("%c", p_isd_info->isd_userName[name_i]);
        }
        else
        {
            TDC_PRINTF_V("\r\n");
            break;
        }
    }

    TDC_PRINTF_W("[INFO] PASSKEY : ");
    for (size_t passkey_i = 0; passkey_i < TDC_ARRAY_LEN(p_isd_info->remocon_passkey); passkey_i++)
    {
        TDC_PRINTF_W("%c", p_isd_info->remocon_passkey[passkey_i]);
    }
    TDC_PRINTF_V("\r\n");

    TDC_PRINTF_W("[INFO] RL : ");  // 1: L, 2: R
    if (p_isd_info->isd_location_RL == 1)
    {
        TDC_PRINTF_W("LEFT \r\n");
    }
    else if (p_isd_info->isd_location_RL == 2)
    {
        TDC_PRINTF_W("RIGHT \r\n");
    }
    else
    {
        TDC_PRINTF_W("F \r\n");
    }

    TDC_PRINTF_W("[INFO] YEAR : 0x%02X \r\n", p_isd_info->isd_year);
    TDC_PRINTF_W("[INFO] MONTH MODEL : 0x%02X \r\n", p_isd_info->isd_month_model);
    TDC_PRINTF_W("[INFO] SERIAL : 0x%04X \r\n", p_isd_info->isd_serial);
}

/* ULP 모드 롱터치/웨이크업/타이머 시간상수는 tdc_touch_time.h 가 단일 소유
 * (TDC_TOUCH_ULP_WAKE_MS / ULP_LONG_TOUCH_MS / ULP_LONG_TOUCH_CNT /
 *  ULP_TIMER_PRESCALE / ULP_TIMER_TIMEOUT_VALUE). ms 만 바꾸면 카운트 자동 파생. */

static void func_cradle_lid_closed_loop(void)
{
    TDC_PRINTF_I("[CRADLE] ENTERING LIGHT SLEEP MODE\r\n");

    SYS_WATCHDOG_REFRESH();

    /* 1. FPGA 리셋 */
    if (tdc_isd_fpga_write_reset())
    {
        TDC_PRINTF_I("[CRADLE] FPGA SW RESET OK\r\n");
    }

    /* 2. nRF 리셋/끄기 (시퀀스 유지, 실효 없음) */
    tdc_sys_reset_nrf();
    tdc_sys_control_nrf_off_command();

    /* 3. QCC_CTRL = 0 (충전기 연결 시 QCC는 절전 미진입, SPI 패킷 수신 유지) */
    // tdc_qcc_set_mode(TDC_QCC_MODE_SHUTDOWN);

    /* 4. FPGA 슬립 */
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP);

    /* 5. PMIC 끄기 신호 (1.5세대에서 실질 효과 미미, 시퀀스 유지) */
    tdc_shm_on_off_3_v_pmic_cm3_to_cfx(false);

    /* 6. LED 끄기 */
    tdc_led_turn_off();

    /* ※ CFX 유지: enter_ULP_mode 신호 보내지 않음 */

    TDC_PRINTF_I("[CRADLE] LIGHT SLEEP ACTIVE. WAITING FOR LID OPEN PACKET...\r\n");

    /* 약 절전 루프 - BLE 패킷 수신으로 뚜껑 열림 감지 */
    ST__ISD_STATUS dummy_isd = {en__isdStatus_NA, false};
    uint32_t       wfi_count = 0;

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        /* BLE 통신: 충전 중 QCC는 SPI 패킷 계속 수신 가능 */
        (void) tdc_ble_communication_step(dummy_isd);

        /* 뚜껑 열림 패킷 감지 (data[2]=1 또는 else → setter가 df_Connected으로 갱신) */
        if (tdc_pwr_cradle_get_cover_state() == df_Connected)
        {
            tdc_qcc_set_mode(TDC_QCC_MODE_SHUTDOWN);
            TDC_PRINTF_I("[CRADLE] LID OPENED PACKET RECEIVED - WATCHDOG RESET FOR REBOOT\r\n");
            tdc_util_delay_ms(20); /* 로그 드레인 */
            SYS_WATCHDOG_RESET();
        }

        /* ULP가 아닌 normal 모드 - 딜레이 없이 인터럽트 기반 iteration */
        SYS_WAIT_FOR_INTERRUPT;

        if (++wfi_count % 1000 == 0)
        {
            TDC_PRINTF_I("[CRADLE] WFI wakeup count: %d\r\n", (int) wfi_count);
        }
    }
}

/* fake_func_sleep() 제거(2026-07-20): 터치센서 계측용 임시 코드였다.
 * ci_fake_power_sleep() 으로 SYSCLK 를 30.72MHz 로 유지한 채 절전 시퀀스만 밟아
 * 계측을 가능케 한 '가짜 절전'이었다(진짜 func_sleep 은 2.56MHz 로 낮춘다).
 * 진입점이던 BLE 0x8F option 4 와 함께 삭제.
 * 상세: docs/tasks/main/20260720_fake-sleep-removal/ */

/* func_sleep() ULP 루프 헬퍼 - 상태(카운터·게이트)는 전부 포인터로 전달, func_sleep() 소유 유지 */

#if (TDC_TOUCH_DEBUG_PRINT_ENABLE)
/* 절전 ULP 계측 (노말 폴링과 동일 포맷) - LTA/Counts/절대임계/밴드초과 */
static void tdc_touch_sleep_log_debug(bool ok, const tdc_touch_iqs323_status_t *st, tdc_touch_state_t state)
{
    if (ok)
    {
        tdc_touch_iqs323_debug_t dbg;
        if (tdc_touch_iqs323_read_debug(&dbg) && dbg.ok)
        {
            uint16_t delta    = (dbg.lta > dbg.counts) ? (uint16_t) (dbg.lta - dbg.counts) : 0;
            uint16_t abs_thr  = (uint16_t) (((uint32_t) TDC_TOUCH_IQS323_THRESHOLD * dbg.lta) / 256u);
            uint16_t pabs_thr = (uint16_t) (((uint32_t) TDC_TOUCH_IQS323_PROX_THRESHOLD * dbg.lta) / 256u);

#if 0
            TDC_PRINTF_D("[TOUCH] LTA=%3u  CNT=%3u  D=%3u  THR=%3u  (k=%3u  H=%3u)  %s   PTHR=%3u (pk=%3u)  %s \r\n",  //
                      dbg.lta,
                      dbg.counts,
                      delta,
                      abs_thr,
                      TDC_TOUCH_IQS323_THRESHOLD,
                      TDC_TOUCH_IQS323_HYSTERESIS,
                      (state == TDC_TOUCH_STATE_TOUCH) ? "T" : ".",
                      pabs_thr,
                      TDC_TOUCH_IQS323_PROX_THRESHOLD,
                      st->prox ? "P" : ".");
#endif
        }
    }
}
#else
static void tdc_touch_sleep_log_debug(bool ok, const tdc_touch_iqs323_status_t *st, tdc_touch_state_t state)
{
    (void) ok;
    (void) st;
    (void) state;
}
#endif

/* 절전 ATI 에러 복구(나안): 직접 Re-ATI 없이 재부팅 → 노말 복귀해 Re-ATI 게이트로 복구.
 * ati_active(정상 ATI burst) 중에는 제외, 실제 에러(!ati_active && ati_error)만. */
static void tdc_touch_sleep_handle_ati_error(bool ok, const tdc_touch_iqs323_status_t *st, int *ati_error_reboot_cnt)
{
    if (ok && st->ati_error && !st->ati_active)
    {
        if (5 < *ati_error_reboot_cnt)
        {
            TDC_PRINTF_W("[TOUCH] ati_error -> reboot \r\n");

#if 0
            tdc_led_turn_on_red();
            tdc_util_delay_ms(250); /* RTT 드레인 */
            SYS_WATCHDOG_REFRESH();
            tdc_util_delay_ms(250);
#else
            tdc_util_delay_ms(20); /* RTT 드레인 */
#endif
            SYS_WATCHDOG_RESET();
            /* 도달 불가 - 칩 리셋 */
        }
        else
        {
            TDC_PRINTF_D("[TOUCH] ati_error recover (n=%d) \r\n", *ati_error_reboot_cnt);
            tdc_touch_iqs323_re_ati();
            (*ati_error_reboot_cnt)++;
        }
    }
    else if (ok && !st->ati_error && !st->ati_active)
    {
        *ati_error_reboot_cnt = 0;
    }
}

/* sleep_ignore 게이트: 첫 노터치 확정(400ms) 시 RESEED+게이트 해제, 10초 미확정 시 강제 RESEED */
static void tdc_touch_sleep_handle_notouch_gate(bool ok, tdc_touch_state_t state, bool *sleep_ignore, int *notouch_cnt, int *ignore_elapsed)
{
    (*ignore_elapsed)++; /* read 성패 무관 게이트 지속 시간 */

    if (ok && state == TDC_TOUCH_STATE_NOT_TOUCH)
    {
        (*notouch_cnt)++;
        if (*notouch_cnt >= TDC_TOUCH_ULP_NOTOUCH_DEBOUNCE_CNT)
        {
            tdc_touch_iqs323_reseed();
            *sleep_ignore = false;
            TDC_PRINTF_D("[TOUCH] notouch confirmed -> reseed, gate open \r\n");
        }
    }
    else /* TOUCH 또는 read 실패 → 노터치 확정 보류 */
    {
        *notouch_cnt = 0;
    }

    /* 첫 노터치가 10초 넘게 확정 안 됨(계속 터치/오염) → 강제 RESEED 로 baseline 끌어와 탈출 */
    if (*sleep_ignore && *ignore_elapsed >= TDC_TOUCH_ULP_FORCE_RESEED_CNT)
    {
        tdc_touch_iqs323_reseed();
        *ignore_elapsed = 0;
        *notouch_cnt    = 0;
        TDC_PRINTF_D("[TOUCH] notouch timeout -> forced reseed \r\n");
    }
}

/* 게이트 해제 후 - 터치 발생 시 재부팅 */
static void tdc_touch_sleep_handle_touch_reboot(bool ok, tdc_touch_state_t state, int *touch_cnt)
{
    if (ok && state == TDC_TOUCH_STATE_TOUCH)
    {
        (*touch_cnt)++;
        if (*touch_cnt >= TDC_TOUCH_ULP_REBOOT_TOUCH_CNT)
        {
            TDC_PRINTF_W("[TOUCH] touch detected -> reboot \r\n");

#if 0
            Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);

            tdc_util_delay_ms(250); /* RTT 뷰어 로그 드레인 대기 */
            SYS_WATCHDOG_REFRESH();
            tdc_util_delay_ms(250); /* RTT 뷰어 로그 드레인 대기 */
#else
            tdc_util_delay_ms(20); /* RTT 뷰어 로그 드레인 대기 */
#endif
            SYS_WATCHDOG_RESET();
            /* 도달 불가 - 칩 리셋 */
        }
    }
    else
    {
        *touch_cnt = 0;
    }
}

static void tdc_touch_sleep_log_state_transition(bool ok, tdc_touch_state_t state, tdc_touch_state_t *ulp_state_prev)
{
    if (ok && *ulp_state_prev != state)
    {
        TDC_PRINTF_D("[TOUCH] state %s -> %s \r\n", tdc_touch_state_name(*ulp_state_prev), tdc_touch_state_name(state));
        *ulp_state_prev = state;
    }
}

int func_sleep(void)
{
    /* 절전 ULP 터치 감시 (부팅 boot_ignore 철학):
     *  - sleep_ignore 구간: 첫 '400ms(2폴링) 연속 노터치' 확정 전까지 터치 무시(손 댄 채 진입 대응).
     *    확정 시 RESEED 1회로 절전 노터치 baseline 동기 후 게이트 해제.
     *  - 노터치가 10초 넘게 확정 안 되면(계속 터치/오염) 강제 RESEED 로 현재 상태를 baseline 끌어와 탈출.
     *  - 게이트 해제 후 '400ms 연속 터치' 시 재부팅 → 노말 모드 복귀(30/60/90 stuck 불요).
     * 아래는 루프 진입 전 1회 초기화되는 영속 상태 - 매 iteration 재초기화 금지(상태머신 붕괴). */
    bool              sleep_ignore         = true; /* 첫 노터치 확정 전 터치 막힘 */
    int               notouch_cnt          = 0;    /* 연속 NOT_TOUCH 샘플 (게이트 해제 기준) */
    int               ignore_elapsed       = 0;    /* 게이트 지속 샘플 (10s 강제 RESEED 기준) */
    int               touch_cnt            = 0;    /* 게이트 해제 후 연속 TOUCH 샘플 (재부팅 기준) */
    tdc_touch_state_t ulp_state_prev       = TDC_TOUCH_STATE_RESET;
    int               ati_error_reboot_cnt = 0;

    /* 매 iteration 갱신 - 선언만 여기, 대입(read_status 등)은 루프 내부에 그대로 유지 */
    tdc_touch_iqs323_status_t st;
    bool                      ok;
    tdc_touch_state_t         state;

    SYS_WATCHDOG_REFRESH(); /* Refresh the watchdog at very first time */

    /* 절전 노터치 baseline RESEED 는 ULP 루프 내 '첫 NOT_TOUCH 시 1회'로 이동했다(아래).
     * 진입 초입의 무한 'WAIT TOUCH RELEASE' 루프는 손 미해제·임계 오인 시 무한 스턱이라 제거. */

    TDC_PRINTF_D("[LP] enter sleep \r\n");

    // I2C 레지스터의 sw_reset 만으로도 백텔 하드웨어 전원 OFF가 되는지 확인이 필요하다.

    TDC_PRINTF_D("[LP] fpga reset %s \r\n", tdc_isd_fpga_write_reset() ? "ok" : "fail");

    cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX = 1;

    tdc_sys_reset_nrf();
    tdc_sys_control_nrf_off_command();
    tdc_qcc_set_mode(TDC_QCC_MODE_SHUTDOWN);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP);
    tdc_shm_on_off_3_v_pmic_cm3_to_cfx(false); /* Disable 3.3V, 1.2V PMIC */
    tdc_led_turn_off();

#if 1
    while (1) /* CFX ULP 진입 대기 (공유메모리 플래그) */
    {
        if (cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX == 0)
        {
            TDC_PRINTF_D("[LP] cfx sleep \r\n");
            break;
        }
    }
#endif

    /* 절전 IQS323 설정은 노말과 동일하게 유지(전용 sleep settings 제거 - 운용 임계 그대로,
     * is_ulp 플래그 미사용). CM3 클럭만 tdc_pwr_clock_sleep 로 절감한다. */

    tdc_pwr_clock_sleep(); /* SYSCLK 30.72M → 2.56M, SLOWCLK 유지 */

    /* [FIXME] 실제 SCL - 426.7kHz(2.56MHz/6) - "~122kHz 유지" 의도라면 분주비가 틀렸다.
     * tdc_hal_i2c.h 설계값은 PRESCALE_21(2.56MHz/21?121.9kHz). 의도적 변경인지 확인 필요. */
    tdc_hal_i2c_set_master_prescale(I2C_MASTER_PRESCALE_6 /*I2C_MASTER_PRESCALE_21*/);

    tdc_hal_timer_init_prescaled(TDC_TOUCH_ULP_TIMER_PRESCALE, TDC_TOUCH_ULP_TIMER_TIMEOUT_VALUE); /* 100.0ms 정확 (tdc_touch_time.h 공식 확인) */

    SYS_WATCHDOG_REFRESH();

    while (1)  // ULP loop
    {
        SYS_WAIT_FOR_INTERRUPT; /* ULP_WAKE_MS 동안 idle */
        SYS_WATCHDOG_REFRESH(); /* 워치독 3.28s 대비 매 웨이크업마다 refresh */

        ok    = tdc_touch_iqs323_read_status(&st);
        state = st.pressed ? TDC_TOUCH_STATE_TOUCH : TDC_TOUCH_STATE_NOT_TOUCH;

        tdc_touch_sleep_log_debug(ok, &st, state);
        tdc_touch_sleep_handle_ati_error(ok, &st, &ati_error_reboot_cnt);

        if (sleep_ignore)
        {
            tdc_touch_sleep_handle_notouch_gate(ok, state, &sleep_ignore, &notouch_cnt, &ignore_elapsed);
        }
        else
        {
            tdc_touch_sleep_handle_touch_reboot(ok, state, &touch_cnt);
        }

        tdc_touch_sleep_log_state_transition(ok, state, &ulp_state_prev);
    }

    return 0;  /* 도달 불가 - 컴파일러 만족용 */
}

/* EOF */
