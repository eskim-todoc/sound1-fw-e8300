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

#include <driver_IQS323.h>

#include "isd_interface_stimulationStandAlone.h"  // 신규 추가 for I2S 디버깅
#include <isd_interface_init_FPGA.h>              // 절전 모드 진입 전 FPGA 리셋 목적
#include <isd_interface_FPGA.h>

#include <ci_power.h>
#include <ci_dio.h>
#include <ci_power.h>
#include <ci_timer.h>
#include <ci_uart.h>
#include <ci_printf.h>

#include <SEGGER_RTT_Wrapper.h>
#include <aes.h>

#include <snd_qcc.h>

#ifdef ENABLE_UI_CMD
#include "tdc_ui_command.h"
#endif

void debug_led_pattern(EN__LED_PATTERN pattern);

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

static const FirmWare_Info firmwareInfo = {2, 0, 1, __DATE__};

static const int devFwVer_type = DEV_FW_VER_BETA;  // 내부 개발 버전 (Beta)
static const int devFwVer_num  = 1;                // 1

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

bool proc_touch(int state_now);

bool iqs323_proc(void)
{
    static bool is_touch_printed = false;

    static char *print_states[4] = {
        "    RESET",  // 0
        "    TOUCH",  // 1
        "NOT TOUCH",  // 2
        "ATI ERROR"   // 3
    };

    int last_touch_state;
    int curr_touch_state;

    int last_tick;
    int curr_tick;

    last_touch_state = iqs323_get_state();

    last_tick = iqs323_get_tick();
    // curr_tick = OTE_1_5_gen_TIMER_get_tick();
    curr_tick = ci_timer_get_tick();

    // 100ms 마다 확인
    if (100 <= (curr_tick - last_tick))
    {
        iqs323_update_tick(curr_tick);

        // 터치 상태 읽기
        if (iqs323_get_touch_state(&curr_touch_state))
        {
            if (last_touch_state != curr_touch_state)
            {
                ci_printv("[TOUCH] STATE: %s -> %s \r\n", print_states[last_touch_state], print_states[curr_touch_state]);

                iqs323_update_state(curr_touch_state);
            }
        }

        // 읽어온 터치 상태로 롱터치 검증
        if (proc_touch(curr_touch_state))
        {
            // long touch
            ci_printi("\r\n[TOUCH] EVENT: LONG TOUCH \r\n");
            return true;
        }
    }

    return false;
}

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

    // 초기화 과정을 통해 SPI 인터페이스 설정도 완료 되었고
    // 위에서 CFX 동작까지 실행시켰으므로, 이제 QCC를 깨우고 배터리 정보를 얻을 수 있도록 한다.
    snd_qcc_set_mode(SND_QCC_MODE_NORMAL);

#ifdef ENABLE_UI_CMD
    tdc_ui_command_init();
#endif

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
            powerButtonPushed = iqs323_proc();         // NOTE: 터치 센서 처리하는 코드가, 드라이버 말고 main.c에 있음

            // powerButtonPushed = false;                 // 왜 인지 특정 보드에서는 RE-ATI 에러가 발생하는 중

#if 1  // QCC 대체용 디버깅 코드 시작, 약 500밀리초 이후 시스템 동작
            {
                static int fake_0x34      = 0;
                static int fake_0x34_done = 0;

                if (fake_0x34_done == 0)
                {
                    if (fake_0x34 == 0)
                    {
                        fake_0x34 = ci_timer_get_tick();
                    }

                    if (500 < (ci_timer_get_tick() - fake_0x34))
                    {
                        fake_0x34_done = 1;
                        ci_printw("[FAKE_0x34] UPDATE FAKE BATT LEVEL, FAKE CHARGER STATE \r\n");
                        snd_batt_set_percent(90);
                        snd_batt_set_state(EN__SND_BATT_STATE_DISCHARGING);
                        snd_charger_set_state(EN__SND_CHARGER_STATE_DISCONNECTED);
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
            //tdc_LED_handle_special_case(systemState.Led_Pattern);

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
                int  pct             = ovr_batt_active ? (int) tdc_ui_command_override_battery_percent()
                                                       : snd_batt_get_percent();
#else
                bool ovr_batt_active = false;
                int  pct             = snd_batt_get_percent();
#endif
                static led_state_t prev_batt_st = LED_ST_IDLE;
                led_state_t batt_st;

                if (!ovr_batt_active && snd_batt_get_state() == EN__SND_BATT_STATE_RESET)
                {
                    /* 배터리 정보 미수신: 판정 보류 */
                    batt_st = LED_ST_IDLE;
                }
                else if (pct < 10)                                              batt_st = LED_ST_BATT_CRITICAL;
                else if (pct < 12 && prev_batt_st == LED_ST_BATT_CRITICAL)      batt_st = LED_ST_BATT_CRITICAL;
                else if (pct >= 80)                                             batt_st = LED_ST_BATT_READY;
                else if (pct >= 78 && prev_batt_st == LED_ST_BATT_READY)        batt_st = LED_ST_BATT_READY;
                else                                                            batt_st = LED_ST_BATT_MID;

                prev_batt_st = batt_st;
#ifdef ENABLE_UI_CMD
                if (!tdc_ui_command_is_led_override(LED_SRC_BATTERY))
#endif
                    led_request(LED_SRC_BATTERY, batt_st);

                /* ISD (SS4.6) */
#ifdef ENABLE_UI_CMD
                bool isd_conn = tdc_ui_command_override_isd_active() ? tdc_ui_command_override_isd_value()
                                                        : isd_state.conneded_ISD;
#else
                bool isd_conn = isd_state.conneded_ISD;
#endif
#ifdef ENABLE_UI_CMD
                if (!tdc_ui_command_is_led_override(LED_SRC_ISD))
#endif
                    led_request(LED_SRC_ISD, isd_conn ? LED_ST_IN_USE : LED_ST_NONE);

                /* Mapping (SS4.4) */
#ifdef ENABLE_UI_CMD
                bool map_conn = tdc_ui_command_override_map_active() ? tdc_ui_command_override_map_value()
                                                        : BLE_communicationState.mappingConnection;
#else
                bool map_conn = BLE_communicationState.mappingConnection;
#endif
#ifdef ENABLE_UI_CMD
                if (!tdc_ui_command_is_led_override(LED_SRC_MAPPING))
#endif
                {
                    if (map_conn)
                    {
                        led_request(LED_SRC_MAPPING, isd_conn ? LED_ST_MAPPING_ISD : LED_ST_MAPPING_NO_ISD);
                    }
                    else
                    {
                        led_request(LED_SRC_MAPPING, LED_ST_NONE);
                    }
                }
            }

            led_arbiter_tick();

            NRF_On_OFF(isd_state, systemState.BLE_Off, BLE_communicationState.mappingConnection, BLE_communicationState.BLE_Off_Command);

#ifdef ENABLE_UI_CMD
            tdc_ui_command_poll();
#else
            do
            {
                char byte;

                if (0 < SEGGER_RTT_Read(0, &byte, 1))
                {
                    if ('0' <= byte && byte <= '9')
                    {
                        int vol = (byte - '0') + 1;

                        ci_printi("[DEBUG] NEW AUDIO VOLUME INPUT : %d \r\n", vol);
                        changeAudioVolume(vol);
                    }
                    else if ('e' == byte)
                    {
                        ci_printi("[DEBUG] STATRT TO ERASE ALL MAPS \r\n");

                        ci_map_init_map_data_all(true);

                        ci_printi("[DEBUG] FINISHED ERASING ALL MAPS \r\n");
                    }
                    else if ('l' == byte)
                    {
                        ci_printi("[DEBUG] START TO READ EVENT LOG \r\n");

                        ci_event_log_read();

                        ci_printi("[DEBUG] FINISHED TO READ EVENT LOG \r\n");
                    }
                    else if ('x' == byte)
                    {
                        ci_printi("[DEBUG] MAKE DATA LOGGING ERROR \r\n");
                        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_open, __LINE__);
                    }
                    else if ('t' == byte)
                    {
                        ci_printi("[DEBUG] SETTING FILE INTEGRITY ERROR \r\n");
                        ci_event_log_write(CI_EVENT_LOG_TYPE_INTEGRITY_ERROR);
                    }
                }
            } while (false);
#endif

            // 중요!!
            disable_iteration();
        }  // 끝, iteration

        main_counter++;
        update_CM3Status_toCFX(main_counter);

        if (systemState.systemOff == true)
        {
            break; /* Escape this main loop to enter the ULP mode */
        }

        SYS_WATCHDOG_REFRESH();
        SYS_WAIT_FOR_INTERRUPT;
    }  // 끝, while

    return 0;
}

void debug_led_pattern(EN__LED_PATTERN pattern)
{
    ci_printv("[DEBUG] LED PATTERN UPDATE TO ");

    switch (pattern)
    {
        case en__LED_NA:
            ci_printv("NA \r\n");
            break;
        case en__LED_Map_Error:
            ci_printv("MAP ERROR \r\n");
            break;
        case en__LED_MCU_Error:
            ci_printv("MCU ERROR \r\n");
            break;
        case en__LED_MCU_Accelerometer_Error:
            ci_printv("ACC ERROR \r\n");
            break;
        case en__LED_MCU_FPGA_Error:
            ci_printv("FPGA ERROR \r\n");
            break;
        case en__LED_MCU_RF_PMIC_Error:
            ci_printv("RF PMIC ERROR \r\n");
            break;
        case en__LED_POWER_On:
            ci_printv("POWER ON \r\n");
            break;
        case en__LED_POWER_Off:
            ci_printv("POWER OFF \r\n");
            break;
        default:
            ci_printv("UNKNOWN (%d) \r\n", pattern);
            break;
    }
}

int func_sleep(void)
{
    int *p_int32;
    int  msTickForULP;

    SYS_WATCHDOG_REFRESH(); /* Refresh the watchdog at very first time */

    ci_printi("[INFO] CM3 IS PREPARING TO ENTER SLEEP MODE \r\n");

    ci_printi("[INFO] FIRST OF ALL, TRY TO POWER OFF FPGA BACKTRL H/W \r\n");

    // FPGA의 백텔 하드웨어 전원 OFF를 위한 구문이다.
    // I2C 레지스터의 sw_reset 만으로도 백텔 하드웨어 전원 OFF가 되는지 확인이 필요하다.
    if (write_FPGA_reset())
    {
        ci_printi("[INFO] I2C TX SUCCESS FOR FPGA SW RESET. \r\n");
    }
    else
    {
        ci_printe("[INFO] I2C TX FAIL FOR FPGA SW RESET. \r\n");
    }

    cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX = 1; /* Command CFX to enter ULP mode */

    ResetNRF();                                     /* Reset nRF */
    NRF_Off_Command();                              /* Disable nRF */
    snd_qcc_set_mode(SND_QCC_MODE_SHUTDOWN);        // QCC 셧다운
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP); /* Disable FPGA */
    OnOff_3V_PMIC_CM3_to_CFX(false);                /* Disable 3.3V, 1.2V PMIC */
    turnOffLED();                                   // LED 끄기

#if 1
    while (1) /* Wait for the CFX to enter ULP mode */
    {
        if (cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX == 0)
        {
            ci_printi("[INFO] CFX HAS ENTERED SLEEP MODE \r\n");
            break;
        }
    }
#endif

    // Uninitialize(); /* Disable peripherals and DIOs */

    // ci_power_sleep();

    // ci_timer_init(OTE_1_5_GEN_TIMER_TICK_500MS_PM_LP); /* Make 500ms timer for watchdog refresh */
    // ci_timer_init(19999); /* Make 500ms timer for watchdog refresh */

    ci_timer_init(19); /* Make around 1msec timer */

    SYS_WATCHDOG_REFRESH();

    static int long_touch_event_cnt = 0;

    int blue_cnt   = 0;
    int cyan_cnt   = 0;
    int color_type = 0;

    while (1)  // ULP loop
    {
        SYS_WATCHDOG_REFRESH();

        // 롱-터치 이벤트가 감지되면, 터치가 해제 될 때까지 기다리도록 한다.
        if (long_touch_event_cnt == 0)
        {
            if (iqs323_proc())
            {
                ci_printi("[MAIN] LONG TOUCH DETECTED, WAIT RELEASE \r\n");
                long_touch_event_cnt = 1;
            }
        }
        // 롱-터치 후 해제까지 감지되면, 그제서야 절전 모드에서 깨어나도록 한다.
        else if (long_touch_event_cnt == 1)
        {
            int curr_touch_state = IQS323_TOUCH_STATE_TOUCH;

            if (iqs323_get_touch_state(&curr_touch_state))
            {
                if (curr_touch_state == IQS323_TOUCH_STATE_NOT_TOUCH)
                {
                    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_B);

                    ci_printi("[MAIN] TOUCH RELEASED SO, WAKE UP! \r\n");
                    delay_ms(20);  // 디버깅을 위해 RTT 뷰어가 메시지를 읽을 수 있도록 잠시 대기함

                    SYS_WATCHDOG_RESET();
                    break;
                }
            }

            switch (color_type)
            {
                case 0:  // cyan
                    if (cyan_cnt == 0)
                    {
                        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_G);
                        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
                        cyan_cnt++;
                    }
                    else if (300 <= cyan_cnt)
                    {
                        cyan_cnt   = 0;
                        blue_cnt   = 0;
                        color_type = 1;
                    }
                    else
                    {
                        cyan_cnt++;
                    }
                    break;

                case 1:  // blue
                    if (blue_cnt == 0)
                    {
                        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R);
                        Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_G);
                        Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_B);
                        blue_cnt++;
                    }
                    else if (300 <= blue_cnt)
                    {
                        cyan_cnt   = 0;
                        blue_cnt   = 0;
                        color_type = 0;
                    }
                    else
                    {
                        blue_cnt++;
                    }
                    break;
            }
        }

        SYS_WAIT_FOR_INTERRUPT;

#if 0
        /* Interrupt occurred for acc-sensor */
        if (ci_dio_is_set_int_flag_acc_sensor())
        {
            ci_dio_clear_int_flag_acc_sensor();

            /* If still the acc-sensor DIO interrupt active level is asserted, wake up CM3 from ULP mode */
            if (Sys_GPIO_Read(DIO_PIN_INDEX_for_Accelerometer) == 0)
            {
                // USB 케이블로 충전 중일 때는 sleep 모드로 진입하지 않지만
                // 휴대 보관함에서 충전 중일 때는 커버가 일정 시간 닫혀 있으면 sleep 모드로 진입한다.
                // 그래서 가속도 센서 인터럽트가 발생했을 때
                // 휴대보관함 연결 중이라면 무시해야 한다.
                // 휴대보관함에서는 커버를 열었을 때만 깨어나면 된다.

                if (Sys_GPIO_Read(DIO_PIN_INDEX_for_CarryingCasePluggedIn) != 1)
                {
                    SYS_WATCHDOG_RESET();
                    break;
                }
            }
        }

        /* Interrupt occurred for carrying case cover open */
        if (ci_dio_is_set_int_flag_case_lid_open())
        {
            ci_dio_clear_int_flag_case_lid_open();

            /* If still the carrying case cover open DIO interrupt active level is asserted, wake up CM3 from ULP mode */
            if (Sys_GPIO_Read(DIO_PIN_INDEX_for_CarryingCaseCoverOpen) == 1)
            // if (Sys_GPIO_Read(DIO_PIN_INDEX_for_CarryingCasePluggedIn) == 0)
            {
                SYS_WATCHDOG_RESET();
                break;
            }
        }

        SYS_WAIT_FOR_INTERRUPT;
#endif
    }

    SYS_WATCHDOG_REFRESH();

    ci_timer_uninit();
    ci_power_normal(); /* Make Normal clock setting */

    // cfx_cm3_sharedMemoryAll.is_CFX_started           = 0; /* Reset flag for initializing CFX related booting sequence
    // */ cfx_cm3_sharedMemoryAll.is_enabled_CFX_iteration = 0; /* Reset flag for initializing CFX related booting
    // sequence */

    p_int32 = (int *) &cfx_cm3_sharedMemoryAll;

    for (int i = 0; i < (sizeof(ST__CFX_CM3_SharedMemory_ALL) / 4); i++)
    {
        p_int32[i] = 0;
    }

    SYSCTRL_CFX_CMD->CFX_CMD_0_ALIAS = 1; /* Interrupt triggering to CFX */

    return 0;
}

/* EOF */
