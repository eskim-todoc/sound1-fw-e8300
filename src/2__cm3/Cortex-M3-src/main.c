/**
 * @file main.c
 */

#include <initialize.h>
#include <main.h>

#include "board.h"           //ok
#include "driver_SPI.h"      //ok
#include "driver_cfx_i2c.h"  //ok

#include "batteryNPowerControl.h"  //ok

#include "cfx_cm3_sharedMemory.h"  //ok

#include "driver_MIS2DH.h"  //ok
#include "error.h"          //ok
#include "systemControl.h"  //ok

#include "isd_interface.h"   //ok
#include "mappingControl.h"  //ok
#include "remoteControl.h"   //ok

#include "LedOutput.h"           //ok
#include "ble_communication.h"   //ok
#include "earpieceUpdate.h"      //ok
#include "indicatorByStimul.h"   //ok
#include "stimulationParaCal.h"  //ok

#include "driver_REN_ISL9122.h"  //ok

#include <tdc_touch.h>        /* 공개 API + tdc_touch_time.h(ULP 시간상수) 재노출 */
#include <tdc_touch_iqs323.h> /* 절전 진입 IQS323 직접 호출 */

#include "isd_interface_stimulationStandAlone.h"  // 신규 추가 for I2S 디버깅
#include <isd_interface_init_FPGA.h>              // 절전 모드 진입 전 FPGA 리셋 목적
#include <isd_interface_FPGA.h>

#include <ci_power.h>
#include <ci_dio.h>
#include <ci_power.h>
#include <ci_timer.h>
#include <ci_uart.h>
#include <ci_printf.h>
#include "driver_i2c.h"  //ok  ? Sleep 진입 시 I2C PRESCALE 런타임 재설정용

#include <SEGGER_RTT_Wrapper.h>
#include <aes.h>

#include <snd_qcc.h>

#if defined(ENABLE_UI_CMD)
#include "tdc_ui_command.h"
#endif

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
static int s_tdc_fake_op_mode = 0;  // 0: normal, 1: fake_sleep

void tdc_set_fake_op_mode(int mode)
{
    s_tdc_fake_op_mode = mode;
}

int tdc_get_fake_op_mode(void)
{
    return s_tdc_fake_op_mode;
}

#endif

char *readFirmwareInfo()
{
    return (char *) &firmwareInfo;
}

void Initialize(void);

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
    // isd_interface() 함수에서, isd_path_Open() 함수를 통해 내부기 연결이 올바르게 인식되면,
    // 해당 내부기의 ISD 번호에 맞는 맵 데이터를 공유 메모리로 복사하고, 연결된 ISD 번호를 업데이트한다.
    // CFX가 ISD 번호를 확인 후, 사용자 설정 값이 로드 되었다는 의미의 플래그를 설정하게된다.
    // 아래는 이 과정이 다 이뤄졌는지 확인하는 과정이다.

    // 사용자 설정 값이 로드 되었다는 의미의 플래그가 설정 된 이후,
    // CM3에서 사용 가능한 맵 번호 판별 과정 등이 완료되면,
    // CFX에게 맵 번호가 바뀌었다는 정보를 changeProgramMapNum() 함수로 알려준다.
    // 추가로, CM3 스스로에게 새로운 맵으로 자극 관련 파라미터의 계산을 다시 하도록 플레그를 세팅한다.
    // 플래그는 전역변수 newMapLoadeFlagForStimulParaCalculation를 true로 설정하는 것이다.

    // CFX는 Normal_PowerMode_event_mapChange() 함수에서 관련 처리를 한 후
    // 공유 메모리의 cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag를 1로 설정한다.

    // 여기까지 완료되면, isd_interface() 함수에서, isd_controlState가 en__isdStatus_stimul_10V_Ok인 상태의
    // stimulationStandAlone() 함수 내부의 if (isMapdateLoaded_CFX()) 블록이 수행되는 구조이다.

    // CFX에서 사용자 설정값 읽어 들여졌는지 확인.
    userSettingValueLoadedFlag = isUserSettingValueLoaded_CFX();

    // 사용자 설정값이 eeprom에서 읽혀졌는가
    if (userSettingValueLoadedFlag)
    {
        // 부팅 후 처음 읽여졌을 때 맵번호를 사용자 설정값에 저장된 번호로 세팅한다.
        // 사용자 설정값이 읽어들여지면 CFX에 맵데이터를 읽어 들이라고 명령을 보내기
        // 위해서 changeProgramMapNum함수를 실행한다.
        if (prev_userSettingValueLoadedFlag != userSettingValueLoadedFlag)
        {
            mapNum                         = readProgramMapNum();
            p_connected_isd_usableMapIndex = readConnected_ISD_usableMapIndex();
            iterNum                        = 0;

            ci_printd("[UPDATE MAP] NUMBER=%d \r\n", readProgramMapNum());
            ci_printd("[UPDATE MAP] CONNECTED ISD'S USABLE MAP INDEX=%d (= MAP NUM - 1) \r\n", p_connected_isd_usableMapIndex[mapNum - 1]);

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

            ci_printd("[UPDATE MAP] MAP NUM=%d, ITERATION NUM=%d \r\n", mapNum, iterNum);

            if (iterNum < MaxNumMap)
            {
                ci_printi("[UPDATE MAP] CHANGE PROGRAM MAP NUM (%d)\r\n", mapNum);
                changeProgramMapNum(mapNum);

                // NOTE: 위 changeProgramMapNum() 함수는 전체적으로 아래 코드를 수행하는 꼴임.
                // cfx_cm3_sharedMemoryAll.userSettingValue.mapNum                = mapNum;
                // cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange     = 1;
                // cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag = 0;
                // changePcmOutputMode(PcmBitStream_Mode_NopStandby);
                // newMapLoadeFlagForStimulParaCalculation = true; ← isd_interface_StimulationStandAlone.c의 전역변수
            }
            else
            {
                errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
            }
        }
    }

    prev_userSettingValueLoadedFlag = userSettingValueLoadedFlag;
}

int main_counter = 0;

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

// clang-format off
void aes128_test(void)
{
    static uint8_t key128[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
    struct AES_ctx ctx;
    uint8_t        text[16] = {
        0xD, /* 1 */ 0xE, /* 2 */ 0xA, /* 3 */ 0xD, /* 4 */ 0xB, /* 5 */ 0xE, /* 6 */ 0xA, /* 7 */
        0xF, /* 8 */ 0xA, /* 9 */ 0xB, /* 10 */ 0xC, /* 11 */ 0xD, /* 12 */ 0x1, /* 13 */ 0x2, /* 14 */ 0x3, /* 15 */ 0x4, /* 16 */
    };

    AES_init_ctx(&ctx, key128);

    ci_printf("[AES] BEFORE ENCRYPT : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \r\n",
            text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7],
            text[8], text[9], text[10], text[11], text[12], text[13], text[14], text[15]);

    AES_ECB_encrypt(&ctx, text);

    ci_printf("[AES] AFTER  ENCRYPT : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \r\n",
            text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7],
            text[8], text[9], text[10], text[11], text[12], text[13], text[14], text[15]);

    AES_ECB_decrypt(&ctx, text);

    ci_printf("[AES] AFTER  DECRYPT : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \r\n",
            text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7],
            text[8], text[9], text[10], text[11], text[12], text[13], text[14], text[15]);
}
// clang-format on

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
    ci_aes_init();

    // JLink RTT를 강제 초기화 시킴 (버퍼 인덱스 이슈 발생 방지 등)
    SEGGER_RTT_Init();

    ci_printi("[INFO] MODEL : SOUND1 (%u.%u%u / %s) \r\n", /* lf */
              firmwareInfo.version[0],
              firmwareInfo.version[1],
              firmwareInfo.version[2],
              firmwareInfo.buildData);

    ci_printi("[INFO] DEV   : %d.%d \r\n", devFwVer_type, devFwVer_num);

    while (1)
    {
        func_normal();
        func_sleep();
    }
}

/* iqs323_proc() → tdc_touch.c의 tdc_touch_process()로 이동됨 */

static void func_cradle_lid_closed_loop(void);

int func_normal(void)
{
    EN__BATTERY_LEVEL           batteryLevel;
    EN__LED_PATTERN             ledPattern = en__LED_NA;
    ST__SYSTEM_STATE            systemState;
    volatile ST__ISD_STATUS     isd_state = {en__isdStatus_PowerIC_Reset, false};
    ST__USB_CONNECTOR           usbConnectorState;
    ST__BLE_COMMUNICATION_STATE BLE_communicationState = {en__isdStatus_PowerIC_Reset, false, false, false};
    ST__ERROR_CODE              mcuErrorCode;

    SYS_WATCHDOG_REFRESH();  // 시작 시 처음에 워치독 리프레시

    bool powerButtonPushed = false;
    bool conneded_ISD      = false;
    bool mappingConnection = false;

    /* QCC 0x34(배터리) 수신 타임아웃 → 파워오프 패턴 후 절전 진입.
     * 지역변수(static 아님): 절전 후 func_normal 재진입 시 false로 리셋되어
     * 무한 재절전을 방지한다(fake_0x34_done 은 유지되어 타임아웃 재감지도 차단). */
    bool qcc_batt_timeout             = false;
    bool qcc_timeout_poweroff_started = false;

    main_counter                                      = 0;
    cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded = 0;

    systemState.systemOff = false;

    // CFX가 실행되고 자체적으로 플래그를 설정할 때까지 대기
    while (1)
    {
        if (cfx_cm3_sharedMemoryAll.is_CFX_started == 1)
        {
            ci_printd("[INFO] CFX STARTED \r\n");
            break;
        }

        __NOP();  // 최적화 방지 및 메모리 접근 경쟁 상태 방지 목적
    }

    Initialize();

    // CFX iteration을 활성화시키면, CFX가 FIFO 등을 초기화 한 후 PCM FillZero 모드로 동작하게 됨
    // 여기서 iteration을 활성화 한 뒤에야 CFX는 노말 모드에 대한 초기화 과정을 수행한다는 뜻이다.
    // 단, 노말 모드 초기화 과정에서 g_ISR_Flag_CM3_FS_Init = 1; 을 자체적으로 적용하기 때문에
    // 노말 모드 iteration 내부의 fn_systemControl_NormalMode() 함수에서
    // ISD Information을 FS 메모리에서 공유 메모리로 복사하고 CFX_EEPROM_data_is_Loaded = 1; 을 적용한다.

    cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 1;  // CFX 동작 활성화

#ifdef ENABLE_UI_CMD
    tdc_ui_command_init();
#endif

    {
        ST__CFX_CM3_SharedMemory_ISD_info *p_isd_info          = &g_ci_filesystem_ptr_entire_map->map[0].isd_info;
        int                               *p_isd_1_info_name   = &g_ci_filesystem_ptr_entire_map->map[0].isd_info.isd_userName[0];     // [25]
        int                               *p_isd_1_passkey     = &g_ci_filesystem_ptr_entire_map->map[0].isd_info.remocon_passkey[0];  // [4]
        int                               *p_isd_1_location    = &g_ci_filesystem_ptr_entire_map->map[0].isd_info.isd_location_RL;     // 1: L, 2: R
        int                               *p_isd_1_year        = &g_ci_filesystem_ptr_entire_map->map[0].isd_info.isd_year;
        int                               *p_isd_1_month_model = &g_ci_filesystem_ptr_entire_map->map[0].isd_info.isd_month_model;
        int                               *p_isd_1_serial      = &g_ci_filesystem_ptr_entire_map->map[0].isd_info.isd_serial;

        ci_printw("[INFO] BOOT ISD 1 INFO \r\n");
        ci_printw("[INFO] NAME : ");
        for (int name_i = 0; name_i < 25; name_i++)
        {
            if (p_isd_1_info_name[name_i] != 0)
            {
                ci_printw("%c", p_isd_1_info_name[name_i]);
            }
            else
            {
                ci_printv("\r\n");
                break;
            }
        }

        ci_printw("[INFO] PASSKEY : ");
        for (int passkey_i = 0; passkey_i < 4; passkey_i++)
        {
            ci_printw("%c", p_isd_1_passkey[passkey_i]);
        }
        ci_printv("\r\n");

        ci_printw("[INFO] RL : ");
        if (*p_isd_1_location == 1)
        {
            ci_printw("LEFT \r\n");
        }
        else if (*p_isd_1_location == 2)
        {
            ci_printw("RIGHT \r\n");
        }
        else
        {
            ci_printw("F \r\n");
        }

        ci_printw("[INFO] YEAR : 0x%02X \r\n", *p_isd_1_year);
        ci_printw("[INFO] MONTH MODEL : 0x%02X \r\n", *p_isd_1_month_model);
        ci_printw("[INFO] SERIAL : 0x%04X \r\n", *p_isd_1_serial);
    }

    while (1)
    {
        if ((iterationFlag == true))
        {
            mcuErrorCode = readErrorCode();

            // usbConnectorState = readUsbConnectorState();
            usbConnectorState = snd_charger_get_state();

            ledPattern = geteLED_OutputPattern();

            // batteryLevel      = updateBatteryLevel(usbConnectorState.chargerConnectorPluggedIn, ledPattern);
            // powerButtonPushed = isPowerButtonPushed();

            batteryLevel      = snd_batt_get_level();  // 직접 측정하지 않고, QCC에서 배터리 정보 받으면 업데이트 됨
            powerButtonPushed = tdc_touch_process();

            // powerButtonPushed = false;                 // 왜 인지 특정 보드에서는 RE-ATI 에러가 발생하는 중

#if 1  // QCC 대체용 디버깅 코드 시작, 약 250밀리초 이후 시스템 동작
            {
                static int fake_0x34      = 0;
                static int fake_0x34_done = 0;

                if (fake_0x34_done == 0)
                {
                    if (fake_0x34 == 0)
                    {
                        fake_0x34 = ci_timer_get_tick();
                    }

                    if (3000 < (ci_timer_get_tick() - fake_0x34))
                    {
                        fake_0x34_done   = 1;
                        qcc_batt_timeout = true;  // 타임아웃 → 아래 systemControl 직후 블록에서 파워오프+절전
                        ci_printw("[FAKE_0x34] QCC BATT TIMEOUT -> POWER OFF (SLEEP) \r\n");
                    }
                    else if (snd_batt_get_state() != EN__SND_BATT_STATE_RESET)
                    {
                        fake_0x34_done = 1;
                        ci_printi("\r\n");
                        ci_printi("################################################################\r\n");
                        ci_printi("###  [FAKE_0x34 EXIT-DMA]   t3 = %d ms / WAIT = %d ms\r\n", tdc_timer_get_t3_tick(), (ci_timer_get_tick() - fake_0x34));
                        ci_printi("################################################################\r\n");
                        ci_printi("\r\n");
                    }
                }
            }
#endif  // QCC 대체용 디버깅 코드 끝

            // NOTE: QCC에게 0x34(Power info) 프로토콜 수신 전까지는
            //       usbConnectorState.chargerConnectorPluggedIn == df_Default; 상태이다.
            //       df_Default 상태일 때는 아래의 systemControl() 에서
            //       systemStatus.Led_Pattern = en__LED_NA; 외에는 동작하는게 없다.

            systemState = systemControl(ledPattern,  // 최초 부팅 시 초기 값 : en__LED_NA
                                        mcuErrorCode,
                                        usbConnectorState,
                                        batteryLevel,  // Initialize 단계에서 배터리 정보 수집 완료되어 알 수 없는 배터리 레벨이 될 수 없다.
                                        powerButtonPushed,
                                        isd_state.conneded_ISD,                   // 최초 부팅 시 초기 값 : false
                                        BLE_communicationState.mappingConnection  // 최초 부팅 시 초기 값 : false
            );

            // 특수 LED 사용 유무 판별
            // tdc_LED_handle_special_case(systemState.Led_Pattern);

            update_mapNum();  // 맵데이터 업데이트

            isd_state = isd_interface(systemState.enable_ISD,  //
                                      BLE_communicationState.mappingConnection,
                                      BLE_communicationState.isdControlCommand  //
            );

            /* 매핑 연결 상태이고,
             * isd_state.isd_controlState >= en__isdStatus_stimul_10V_Ok 이면,
             * isd_state.connededISD == true 상태이다. */

            BLE_communicationState = bleCommunication(isd_state);

            stimulation_IndicatorOut(readStimulIndicator_OnOff(),  //
                                     systemState.StimulationIndicatorTriggerLowPower,
                                     BLE_communicationState.StimulationIndicatorTrigger  //
            );

            // PMIC 켜고/끄기
            OnOff_3V_PMIC_CM3_to_CFX(systemState.enablePMIC);

            // 매핑 연결ㅅ
            if (BLE_communicationState.mappingConnection)
            {
                changeSystemModeFlag(en__mappingMode);
                shareMappingProgramConnection(true);
            }
            else
            {
                changeSystemModeFlag(en__normalMode);
                shareMappingProgramConnection(false);
                // updateEarPieceStatus();
            }

            /* ===================================================
             * LED source requests (Rev.3)
             * =================================================== */
            {
                /* Battery (SS4.5) -- hysteresis +/-2%
                 * Rev.5 추가: QCC로부터 0x34(Power info) 수신 전까지는 배터리 상태가
                 *            EN__SND_BATT_STATE_RESET 이고 percent = 0 이므로,
                 *            pct 기반 판정(pct<10 → BATT_CRITICAL)이 그대로 적용되면
                 *            부팅 초기에 노란색 LED가 잠깐 켜지는 현상이 발생한다.
                 *            따라서 RESET 상태에서는 pct 판정을 건너뛰고 IDLE로 요청한다. */
#ifdef ENABLE_UI_CMD
                bool ovr_batt_active = tdc_ui_command_override_battery_active();
                int  pct             = ovr_batt_active ? (int) tdc_ui_command_override_battery_percent() : snd_batt_get_percent();
#else
                bool ovr_batt_active = false;
                int  pct             = snd_batt_get_percent();
#endif
                static led_state_t prev_batt_st = LED_ST_IDLE;
                led_state_t        batt_st;

                if (!ovr_batt_active && snd_batt_get_state() == EN__SND_BATT_STATE_RESET)
                {
                    /* 배터리 정보 미수신: 판정 보류 */
                    batt_st = LED_ST_IDLE;
                }
                else if (pct < 40 /*10*/)
                    batt_st = LED_ST_BATT_CRITICAL;
                else if (pct < 41 /*12*/ && prev_batt_st == LED_ST_BATT_CRITICAL)
                    batt_st = LED_ST_BATT_CRITICAL;
                else if (pct >= 65 /*80*/)
                    batt_st = LED_ST_BATT_READY;
                else if (pct >= 64 /*78*/ && prev_batt_st == LED_ST_BATT_READY)
                    batt_st = LED_ST_BATT_READY;
                else
                    batt_st = LED_ST_BATT_MID;

                prev_batt_st = batt_st;
#ifdef ENABLE_UI_CMD
                if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
                    led_request(LED_SRC_BATTERY, batt_st);

                    /* ISD (SS4.6) */
#ifdef ENABLE_UI_CMD
                bool isd_conn = tdc_ui_command_override_isd_active() ? tdc_ui_command_override_isd_value() : isd_state.conneded_ISD;
#else
                bool isd_conn = isd_state.conneded_ISD;
#endif
#ifdef ENABLE_UI_CMD
                if (!tdc_ui_command_is_led_override(LED_SRC_ISD))
#endif
                /* IMPORTANT: 내부기 연결 해제시 위에서 구한 배터리 레벨에 대한 LED를 켜도록 유도했다. */
                {
                    if (isd_conn)  // 내부기 연결 상태
                    {
                        // 내부기 연결 상태에서는 연결된 내부기의 사용자 설정 정보에서 LED 제어 값을 이용해야 한다.
                        // LED 표시 설정값: 1=켜기, 2=끄기. (truthy 검사는 2도 참이 되므로 == 1 로 명시 비교)
                        if (readLED_indicatorOnOff() == 1)
                        {
                            // LED 표시 설정이 켜기(1)이면,
                            led_request(LED_SRC_ISD, LED_ST_IN_USE);
                        }
                        else
                        {
                            // LED 표시 설정이 끄기(2)이면,
                            led_request(LED_SRC_ISD, LED_ST_NONE);
                        }
                    }
                    else  // 내부기 미 연결 상태
                    {
                        // 항상 LED가 켜질 수 있게 설정 정보를 LED 켜기로 강제한다.
                        led_request(LED_SRC_ISD, LED_ST_NONE);
                        led_request(LED_SRC_BATTERY, batt_st);
                    }

                    /* s_req[LED_SRC_ISD] 확정 후 게이트 갱신 ? 연결 해제 전환 시
                     * s_isd_conn=0 과 s_req[ISD]=NONE 사이에 TIMER_3 ISR 이 끼어들어
                     * 1-tick IN_USE(백색) 잔상이 뜨던 race 를 방지하기 위해 분기 뒤로 이동. */
                    led_set_isd_conn_state(isd_conn);
                    // led_request(LED_SRC_ISD, isd_conn ? LED_ST_IN_USE : prev_batt_st /*LED_ST_NONE*/);
                }

                /* Mapping (SS4.4) ? 배터리 레벨(LOW 임계 20%) × ISD 연결 여부 4종 분기.
                 * LOW 진입 pct ≤ 20, 해제 pct ≥ 22 (±2% 히스테리시스). */
#ifdef ENABLE_UI_CMD
                bool map_conn = tdc_ui_command_override_map_active() ? tdc_ui_command_override_map_value() : BLE_communicationState.mappingConnection;
#else
                bool map_conn = BLE_communicationState.mappingConnection;
#endif
                static bool s_map_low_active = false;
                if (pct <= 20)
                    s_map_low_active = true;
                else if (pct >= 22)
                    s_map_low_active = false;
                    /* pct == 21 구간은 직전 상태 유지 */
#ifdef ENABLE_UI_CMD
                if (!tdc_ui_command_is_led_override(LED_SRC_MAPPING))
#endif
                {
                    if (map_conn)
                    {
                        led_state_t map_st;
                        if (s_map_low_active)
                        {
                            map_st = isd_conn ? LED_ST_MAPPING_ISD_BATT_LOW : LED_ST_MAPPING_NO_ISD_BATT_LOW;
                        }
                        else
                        {
                            map_st = isd_conn ? LED_ST_MAPPING_ISD_BATT_READY : LED_ST_MAPPING_NO_ISD_BATT_READY;
                        }
                        led_request(LED_SRC_MAPPING, map_st);
                    }
                    else
                    {
                        led_request(LED_SRC_MAPPING, LED_ST_NONE);
                    }
                }
            }

            /* led_arbiter_tick() 은 Timer 3 ISR 에서 직접 구동 (ci_timer.c).
             * main loop 의 I2C/EEPROM 폴링 블록으로 인한 fade/PWM jitter 회피. */

            NRF_On_OFF(isd_state, systemState.BLE_Off, BLE_communicationState.mappingConnection, BLE_communicationState.BLE_Off_Command);

#ifdef ENABLE_UI_CMD
            tdc_ui_command_set_mapping_connected(BLE_communicationState.mappingConnection);
            tdc_ui_command_poll();
#endif

            // 중요!!
            disable_iteration();
        }  // 끝, iteration

        main_counter++;
        update_CM3Status_toCFX(main_counter);

        /* QCC 배터리 타임아웃 → 파워오프 패턴 후 절전.
         * QCC 미수신 시 systemControl 은 df_Default 게이트(en__LED_NA)에 막혀 자체
         * 파워오프 시퀀스에 도달하지 못하므로, 여기서 직접 POWER_OFF 패턴을 요청하고
         * burst 완료 후 systemOff 를 세팅해 기존 절전 경로(아래 → break → func_sleep)를 탄다.
         * 상세: docs/tasks/power/20260609_qcc-batt-timeout-sleep/분석.md §4 */
        if (qcc_batt_timeout)
        {
            if (!qcc_timeout_poweroff_started)
            {
                led_request(LED_SRC_POWER, LED_ST_POWER_OFF);
                qcc_timeout_poweroff_started = true;
                ci_printi("[FAKE_0x34] LED PATTERN IS POWER OFF (QCC TIMEOUT) \r\n");
            }
            else if (!tdc_led_is_burst_pending())
            {
                systemState.systemOff = true;
            }
        }

        /* 크래들 뚜껑 닫힘 첫 감지 → 약 절전 루프 (ISD 연결 중이면 차단) */
        if (systemState.cradleLidClosed && !isd_state.conneded_ISD)
        {
            led_force_fade_off(); /* fade-out ISR 완료 후 LED 완전 소등 */
            func_cradle_lid_closed_loop();
            /* 도달 불가 ? 루프 내 SYS_WATCHDOG_RESET()으로 재부팅 */
        }

        if (systemState.systemOff == true)
        {
            /* 절전 모드 진입 가드 ? 매핑 / 페어링 / OTA 진행 중에는 보류한다.
             *
             * BLE 활성 검사는 Arbiter src 요청을 직접 본다 ? tdc_led_get_ind_state()
             * 는 BLE 가 set 한 후 NONE 으로 reset 안 보내면 잔존하기 때문. */
            led_state_t ble_st      = led_get_request(LED_SRC_BLE_IND);
            bool        map_active  = BLE_communicationState.mappingConnection;
            bool        pair_active = (ble_st == LED_ST_PAIR);
            bool        ota_active  = (ble_st == LED_ST_OTA_QCC) || (ble_st == LED_ST_OTA_EZAIRO);

            if (map_active || pair_active || ota_active)
            {
                ci_printw("[SYSTEM] SLEEP DEFERRED (map=%d pair=%d ota=%d ble_st=%d) \r\n", map_active, pair_active, ota_active, (int) ble_st);
                systemState.systemOff = false; /* 다음 iteration 에서 트리거 재평가 */
            }
            else
            {
                ci_printi("[SYSTEM] ENTERING SLEEP MODE \r\n");
                /* cross-fade Phase A 강제 ? POWER_OFF burst 직후 다른 best
                 * (BATTERY/ISD/MAPPING) 로 진입한 새 색 (예: GREEN) 이
                 * turnOffLED() 에서 fade-out 되어 잔상으로 보이는 현상 방지.
                 * 모든 src 를 LED_ST_NONE 으로 강제하고 FADE_MAX_MS+10 동안
                 * 자연 fade-out 진행 후 break. */
                led_force_fade_off();
                break; /* Escape this main loop to enter the ULP mode */
            }
        }

        if (tdc_get_fake_op_mode() == 1)  // fake sleep mode가 맞을 때
        {
            ci_printw("[GD] try to enter fake sleep mode. \r\n");
            led_force_fade_off();
            fake_func_sleep();
        }

        SYS_WATCHDOG_REFRESH();
        SYS_WAIT_FOR_INTERRUPT;
    }  // 끝, while

    return 0;
}

/* ULP 모드 롱터치/웨이크업/타이머 시간상수는 tdc_touch_time.h 가 단일 소유
 * (TDC_TOUCH_ULP_WAKE_MS / ULP_LONG_TOUCH_MS / ULP_LONG_TOUCH_CNT /
 *  ULP_TIMER_PRESCALE / ULP_TIMER_TIMEOUT_VALUE). ms 만 바꾸면 카운트 자동 파생. */

static void func_cradle_lid_closed_loop(void)
{
    ci_printi("[CRADLE] ENTERING LIGHT SLEEP MODE\r\n");

    SYS_WATCHDOG_REFRESH();

    /* 1. FPGA 리셋 */
    if (write_FPGA_reset())
    {
        ci_printi("[CRADLE] FPGA SW RESET OK\r\n");
    }

    /* 2. nRF 리셋/끄기 (시퀀스 유지, 실효 없음) */
    ResetNRF();
    NRF_Off_Command();

    /* 3. QCC_CTRL = 0 (충전기 연결 시 QCC는 절전 미진입, SPI 패킷 수신 유지) */
    // snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);

    /* 4. FPGA 슬립 */
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP);

    /* 5. PMIC 끄기 신호 (1.5세대에서 실질 효과 미미, 시퀀스 유지) */
    OnOff_3V_PMIC_CM3_to_CFX(false);

    /* 6. LED 끄기 */
    turnOffLED();

    /* ※ CFX 유지: enter_ULP_mode 신호 보내지 않음 */

    ci_printi("[CRADLE] LIGHT SLEEP ACTIVE. WAITING FOR LID OPEN PACKET...\r\n");

    /* 약 절전 루프 ? BLE 패킷 수신으로 뚜껑 열림 감지 */
    ST__ISD_STATUS dummy_isd = {en__isdStatus_NA, false};
    uint32_t       wfi_count = 0;

    while (1)
    {
        SYS_WATCHDOG_REFRESH();

        /* BLE 통신: 충전 중 QCC는 SPI 패킷 계속 수신 가능 */
        (void) bleCommunication(dummy_isd);

        /* 뚜껑 열림 패킷 감지 (data[2]=1 또는 else → setter가 df_Connected으로 갱신) */
        if (tdc_cradle_get_cover_state() == df_Connected)
        {
            snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);
            ci_printi("[CRADLE] LID OPENED PACKET RECEIVED - WATCHDOG RESET FOR REBOOT\r\n");
            delay_ms(20); /* 로그 드레인 */
            SYS_WATCHDOG_RESET();
        }

        /* ULP가 아닌 normal 모드 ? 딜레이 없이 인터럽트 기반 iteration */
        SYS_WAIT_FOR_INTERRUPT;

        if (++wfi_count % 1000 == 0)
        {
            ci_printi("[CRADLE] WFI wakeup count: %d\r\n", (int) wfi_count);
        }
    }
}

int fake_func_sleep(void)
{
    SYS_WATCHDOG_REFRESH(); /* Refresh the watchdog at very first time */

    changePcmOutputMode(PcmBitStream_Mode_FillZero);
    ci_printd("[GD] pcm mode : fill zero \r\n");

    if (write_FPGA_reset())
    {
        ci_printd("[GD] fpga sw reset : success \r\n");
    }
    else
    {
        ci_printd("[GD] fpga sw reset : failed \r\n");
    }

    cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX = 1; /* Command CFX to enter ULP mode */

    // snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);        // QCC 셧다운

    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP); /* Disable FPGA */
    OnOff_3V_PMIC_CM3_to_CFX(false);                /* Disable 3.3V, 1.2V PMIC */
    turnOffLED();                                   // LED 끄기

#if 1
    while (1) /* Wait for the CFX to enter ULP mode */
    {
        if (cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX == 0)
        {
            ci_printd("[GD] cfx sleep \r\n");
            break;
        }
    }
#endif

    /* 절전 IQS323 설정은 노말과 동일하게 유지(전용 sleep settings 제거 ? 운용 임계 그대로,
     * is_ulp 플래그 미사용). CM3 클럭만 ci_power_sleep 로 절감한다. */

    // Uninitialize(); /* Disable peripherals and DIOs */

#if 0
    if (!public_touch_settings(255, TDC_TOUCH_IQS323_HYSTERESIS))
    {
        ci_printw("[GD] fail : threshold, hysteresis \r\n");
    }
    else
    {
        ci_printw("[GD] success : threshold(%u), hysteresis(%u) \r\n", 255, TDC_TOUCH_IQS323_HYSTERESIS);
    }
#endif

    // ci_power_sleep(); /* SYSCLK 30.72M → 2.56M, SLOWCLK 유지 */
    ci_fake_power_sleep();

    i2c_set_master_prescale(I2C_MASTER_PRESCALE_240); /* 본래 노말 모드 설정 그대로 */

    /* RESEED 는 ULP 루프 내 '첫 NOT_TOUCH 시 1회'로 이동(손 떼야 절전 노터치 baseline 동기). */

    ci_timer_init_prescaled(TDC_TOUCH_ULP_TIMER_PRESCALE, TDC_TOUCH_ULP_TIMER_TIMEOUT_VALUE); /* ? 200 ms 주기 */

    SYS_WATCHDOG_REFRESH();

    /* 절전 ULP 터치 감시 (부팅 boot_ignore 철학):
     *  - sleep_ignore 구간: 첫 '400ms(2폴링) 연속 노터치' 확정 전까지 터치 무시(손 댄 채 진입 대응).
     *    확정 시 RESEED 1회로 절전 노터치 baseline 동기 후 게이트 해제.
     *  - 노터치가 10초 넘게 확정 안 되면(계속 터치/오염) 강제 RESEED 로 현재 상태를 baseline 끌어와 탈출.
     *  - 게이트 해제 후 '400ms 연속 터치' 시 재부팅 → 노말 모드 복귀(30/60/90 stuck 불요). */
    bool              sleep_ignore         = true; /* 첫 노터치 확정 전 터치 막힘 */
    int               notouch_cnt          = 0;    /* 연속 NOT_TOUCH 샘플 (게이트 해제 기준) */
    int               ignore_elapsed       = 0;    /* 게이트 지속 샘플 (10s 강제 RESEED 기준) */
    int               touch_cnt            = 0;    /* 게이트 해제 후 연속 TOUCH 샘플 (재부팅 기준) */
    tdc_touch_state_t ulp_state_prev       = TDC_TOUCH_STATE_RESET;
    int               ati_error_reboot_cnt = 0;

    ST__ISD_STATUS dummy_isd = {en__isdStatus_NA, false};
    uint32_t       wfi_count = 0;

    int last_t3_tick = tdc_timer_get_t3_tick();

    while (1)  // ULP loop
    {
        SYS_WAIT_FOR_INTERRUPT; /* ULP_WAKE_MS 동안 idle */
        SYS_WATCHDOG_REFRESH(); /* 워치독 3.28s 대비 매 웨이크업마다 refresh */

        if (last_t3_tick < tdc_timer_get_t3_tick())
        {
            last_t3_tick = tdc_timer_get_t3_tick();

            /* BLE 통신: 충전 중 QCC는 SPI 패킷 계속 수신 가능 */
            (void) bleCommunication(dummy_isd);

            if (tdc_get_fake_op_mode() == 0)
            {
                ci_printw("[GD] wake up! \r\n");
                delay_ms(20); /* RTT 드레인 */
                SYS_WATCHDOG_RESET();
            }

            tdc_touch_iqs323_status_t st;
            bool                      ok    = tdc_touch_iqs323_read_status(&st);
            tdc_touch_state_t         state = st.pressed ? TDC_TOUCH_STATE_TOUCH : TDC_TOUCH_STATE_NOT_TOUCH;

#if (TDC_TOUCH_DEBUG_PRINT_ENABLE)
            /* 절전 ULP 계측 (노말 폴링과 동일 포맷) ? LTA/Counts/절대임계/밴드초과 */
            if (ok)
            {
                tdc_touch_iqs323_debug_t dbg;
                if (tdc_touch_iqs323_read_debug(&dbg) && dbg.ok)
                {
                    uint16_t delta    = (dbg.lta > dbg.counts) ? (uint16_t) (dbg.lta - dbg.counts) : 0;
                    uint16_t abs_thr  = (uint16_t) (((uint32_t) TDC_TOUCH_IQS323_THRESHOLD * dbg.lta) / 256u);
                    uint16_t pabs_thr = (uint16_t) (((uint32_t) TDC_TOUCH_IQS323_PROX_THRESHOLD * dbg.lta) / 256u);
                    ci_printd("[T] LTA=%3u  CNT=%3u  D=%3u  THR=%3u  (k=%3u  H=%3u)  %s   PTHR=%3u (pk=%3u)  %s \r\n",  //
                              dbg.lta,
                              dbg.counts,
                              delta,
                              abs_thr,
                              TDC_TOUCH_IQS323_THRESHOLD,
                              TDC_TOUCH_IQS323_HYSTERESIS,
                              (state == TDC_TOUCH_STATE_TOUCH) ? "T" : ".",
                              pabs_thr,
                              TDC_TOUCH_IQS323_PROX_THRESHOLD,
                              st.prox ? "P" : ".");

                    tdc_touch_debug_set_recent_lta(dbg.lta);
                    tdc_touch_debug_set_recent_count(dbg.counts);
                    tdc_touch_debug_set_recent_delta(delta);
                    tdc_touch_debug_set_recent_abs_thr(abs_thr);
                    tdc_touch_debug_set_recent_pressed(st.pressed);
                    tdc_touch_debug_set_recent_ati_error(st.ati_error);
                    tdc_touch_debug_set_recent_ati_active(st.ati_active);
                }
            }
#endif

            /* 절전 ATI 에러 복구(나안): 직접 Re-ATI 없이 재부팅 → 노말 복귀해 Re-ATI 게이트로 복구.
             * ati_active(정상 ATI burst) 중에는 제외, 실제 에러(!ati_active && ati_error)만. */
            if (ok && st.ati_error && !st.ati_active)
            {
                if (5 < ati_error_reboot_cnt)
                {
                    ci_printw("\r\n[TOUCH] SLEEP: ATI ERROR -> REBOOT (recover in normal) \r\n");
                    delay_ms(20); /* RTT 드레인 */
                    SYS_WATCHDOG_RESET();
                }
                else
                {
                    // ati 에러 발생 시 한번 복구를 시도해본다.
                    turnON_RedLED();
                    ci_printw("\r\n[TOUCH] SLEEP: ATI ERROR -> RECOVER (in sleep) \r\n");
                    tdc_touch_iqs323_re_ati();
                    ati_error_reboot_cnt++;
                }
                // delay_ms(20); /* RTT 드레인 */
                // SYS_WATCHDOG_RESET();
            }
            else if (ok && !st.ati_error && !st.ati_active)
            {
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);
                ati_error_reboot_cnt = 0;
            }

            if (sleep_ignore)
            {
                ignore_elapsed++; /* read 성패 무관 게이트 지속 시간 */

                if (ok && state == TDC_TOUCH_STATE_NOT_TOUCH)
                {
                    notouch_cnt++;
                    if (notouch_cnt >= TDC_TOUCH_ULP_NOTOUCH_DEBOUNCE_CNT)
                    {
                        tdc_touch_iqs323_reseed();
                        sleep_ignore = false;
                        ci_printi("[TOUCH] SLEEP: notouch confirmed -> RESEED, gate open \r\n");
                    }
                }
                else /* TOUCH 또는 read 실패 → 노터치 확정 보류 */
                {
                    notouch_cnt = 0;
                }

                /* 첫 노터치가 10초 넘게 확정 안 됨(계속 터치/오염) → 강제 RESEED 로 baseline 끌어와 탈출 */
                if (sleep_ignore && ignore_elapsed >= TDC_TOUCH_ULP_FORCE_RESEED_CNT)
                {
                    tdc_touch_iqs323_reseed();
                    ignore_elapsed = 0;
                    notouch_cnt    = 0;
                    ci_printw("[TOUCH] SLEEP: notouch timeout 10s -> forced RESEED \r\n");
                }
            }
            else /* 게이트 해제 후 ? 터치 발생 시 재부팅 */
            {
                if (ok && state == TDC_TOUCH_STATE_TOUCH)
                {
                    touch_cnt++;
                    if (touch_cnt >= TDC_TOUCH_ULP_REBOOT_TOUCH_CNT)
                    {
                        ci_printi("\r\n[TOUCH] SLEEP: touch detected -> REBOOT \r\n");
                        delay_ms(20); /* RTT 뷰어 로그 드레인 대기 */
                        // SYS_WATCHDOG_RESET();
                        /* 도달 불가 ? 칩 리셋 */
                    }
                }
                else
                {
                    touch_cnt = 0;
                }
            }

            if (ok && ulp_state_prev != state)
            {
                ci_printv("[TOUCH] ULP STATE: %s -> %s \r\n", tdc_touch_state_name(ulp_state_prev), tdc_touch_state_name(state));
                ulp_state_prev = state;
            }

        }  // end t3 tick cmp
    }      // end while

    return 0; /* 도달 불가 ? 컴파일러 만족용 */
}

/* func_sleep() ULP 루프 헬퍼 ? 상태(카운터·게이트)는 전부 포인터로 전달, func_sleep() 소유 유지 */

#if (TDC_TOUCH_DEBUG_PRINT_ENABLE)
/* 절전 ULP 계측 (노말 폴링과 동일 포맷) ? LTA/Counts/절대임계/밴드초과 */
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
            ci_printd("[TOUCH] LTA=%3u  CNT=%3u  D=%3u  THR=%3u  (k=%3u  H=%3u)  %s   PTHR=%3u (pk=%3u)  %s \r\n", dbg.lta, dbg.counts, delta, abs_thr, TDC_TOUCH_IQS323_THRESHOLD, TDC_TOUCH_IQS323_HYSTERESIS, (state == TDC_TOUCH_STATE_TOUCH) ? "T" : ".", pabs_thr, TDC_TOUCH_IQS323_PROX_THRESHOLD, st->prox ? "P" : ".");
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
            ci_printw("[TOUCH] ati_error -> reboot \r\n");

#if 0
            turnON_RedLED();
            delay_ms(250); /* RTT 드레인 */
            SYS_WATCHDOG_REFRESH();
            delay_ms(250);
#else
            delay_ms(20); /* RTT 드레인 */
#endif
            SYS_WATCHDOG_RESET();
            /* 도달 불가 ? 칩 리셋 */
        }
        else
        {
            ci_printd("[TOUCH] ati_error recover (n=%d) \r\n", *ati_error_reboot_cnt);
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
            ci_printd("[TOUCH] notouch confirmed -> reseed, gate open \r\n");
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
        ci_printd("[TOUCH] notouch timeout -> forced reseed \r\n");
    }
}

/* 게이트 해제 후 ? 터치 발생 시 재부팅 */
static void tdc_touch_sleep_handle_touch_reboot(bool ok, tdc_touch_state_t state, int *touch_cnt)
{
    if (ok && state == TDC_TOUCH_STATE_TOUCH)
    {
        (*touch_cnt)++;
        if (*touch_cnt >= TDC_TOUCH_ULP_REBOOT_TOUCH_CNT)
        {
            ci_printw("[TOUCH] touch detected -> reboot \r\n");

#if 0
            Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R);
            Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
            Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);

            delay_ms(250); /* RTT 뷰어 로그 드레인 대기 */
            SYS_WATCHDOG_REFRESH();
            delay_ms(250); /* RTT 뷰어 로그 드레인 대기 */
#else
            delay_ms(20); /* RTT 뷰어 로그 드레인 대기 */
#endif
            SYS_WATCHDOG_RESET();
            /* 도달 불가 ? 칩 리셋 */
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
        ci_printd("[TOUCH] state %s -> %s \r\n", tdc_touch_state_name(*ulp_state_prev), tdc_touch_state_name(state));
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
     * 아래는 루프 진입 전 1회 초기화되는 영속 상태 ? 매 iteration 재초기화 금지(상태머신 붕괴). */
    bool              sleep_ignore         = true; /* 첫 노터치 확정 전 터치 막힘 */
    int               notouch_cnt          = 0;    /* 연속 NOT_TOUCH 샘플 (게이트 해제 기준) */
    int               ignore_elapsed       = 0;    /* 게이트 지속 샘플 (10s 강제 RESEED 기준) */
    int               touch_cnt            = 0;    /* 게이트 해제 후 연속 TOUCH 샘플 (재부팅 기준) */
    tdc_touch_state_t ulp_state_prev       = TDC_TOUCH_STATE_RESET;
    int               ati_error_reboot_cnt = 0;

    /* 매 iteration 갱신 ? 선언만 여기, 대입(read_status 등)은 루프 내부에 그대로 유지 */
    tdc_touch_iqs323_status_t st;
    bool                      ok;
    tdc_touch_state_t         state;

    SYS_WATCHDOG_REFRESH(); /* Refresh the watchdog at very first time */

    /* 절전 노터치 baseline RESEED 는 ULP 루프 내 '첫 NOT_TOUCH 시 1회'로 이동했다(아래).
     * 진입 초입의 무한 'WAIT TOUCH RELEASE' 루프는 손 미해제·임계 오인 시 무한 스턱이라 제거. */

    ci_printd("[LP] enter sleep \r\n");

    // I2C 레지스터의 sw_reset 만으로도 백텔 하드웨어 전원 OFF가 되는지 확인이 필요하다.

    ci_printd("[LP] fpga reset %s \r\n", write_FPGA_reset() ? "ok" : "fail");

    cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX = 1;

    ResetNRF();
    NRF_Off_Command();
    snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP);
    OnOff_3V_PMIC_CM3_to_CFX(false); /* Disable 3.3V, 1.2V PMIC */
    turnOffLED();

#if 1
    while (1) /* CFX ULP 진입 대기 (공유메모리 플래그) */
    {
        if (cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX == 0)
        {
            ci_printd("[LP] cfx sleep \r\n");
            break;
        }
    }
#endif

    /* 절전 IQS323 설정은 노말과 동일하게 유지(전용 sleep settings 제거 ? 운용 임계 그대로,
     * is_ulp 플래그 미사용). CM3 클럭만 ci_power_sleep 로 절감한다. */

    ci_power_sleep(); /* SYSCLK 30.72M → 2.56M, SLOWCLK 유지 */

    /* [FIXME] 실제 SCL ? 426.7kHz(2.56MHz/6) ? "~122kHz 유지" 의도라면 분주비가 틀렸다.
     * driver_i2c.h 설계값은 PRESCALE_21(2.56MHz/21?121.9kHz). 의도적 변경인지 확인 필요. */
    i2c_set_master_prescale(I2C_MASTER_PRESCALE_6 /*I2C_MASTER_PRESCALE_21*/);

    ci_timer_init_prescaled(TDC_TOUCH_ULP_TIMER_PRESCALE, TDC_TOUCH_ULP_TIMER_TIMEOUT_VALUE); /* 100.0ms 정확 (tdc_touch_time.h 공식 확인) */

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

    return 0;  /* 도달 불가 ? 컴파일러 만족용 */
}

/* EOF */
