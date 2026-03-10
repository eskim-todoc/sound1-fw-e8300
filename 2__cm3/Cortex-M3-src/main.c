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

#if defined(Board_is_OTE_VER_1_2)
#include "driver_REN_ISL91128.h"
#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_1_5gen_Test_board) || defined(Board_is_OTE_VER_1_5)
#include "driver_REN_ISL9122.h"  //ok
#elif defined(Board_is_OTE_VER_1_3)
#include "driver_REN_ISL98608.h"
#else
#error Link PMIC is NOT selected.
#endif

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

void debug_led_pattern(EN__LED_PATTERN pattern);

typedef struct
{
    uint8_t version[3];
    uint8_t buildData[11];

} FirmWare_Info;

static const FirmWare_Info firmwareInfo = {2, 0, 1, __DATE__};

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

// sections.ld 에 정의된 심볼들 (반드시 동일 이름)
extern uint32_t __data_init__;   // LMA (PRAM 쪽, 초기값 블록 시작)
extern uint32_t __data_start__;  // VMA (DRAM .data 시작)
extern uint32_t __data_end__;    // VMA (DRAM .data 끝)

void load_data_section(void)
{
    uint32_t *src = &__data_init__;
    uint32_t *dst = &__data_start__;

    while (dst < &__data_end__)
    {
        *dst++ = *src++;
    }
}

void aes128_test(void)
{
    static uint8_t key128[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
    struct AES_ctx ctx;
    uint8_t        text[16] = {
        0xD,  // 1
        0xE,  // 2
        0xA,  // 3
        0xD,  // 4
        0xB,  // 5
        0xE,  // 6
        0xA,  // 7
        0xF,  // 8
        0xA,  // 9
        0xB,  // 10
        0xC,  // 11
        0xD,  // 12
        0x1,  // 13
        0x2,  // 14
        0x3,  // 15
        0x4,  // 16
    };

    AES_init_ctx(&ctx, key128);

    ci_printf("[AES] BEFORE ENCRYPT : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \r\n", text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7], text[8], text[9], text[10], text[11], text[12], text[13], text[14], text[15]);

    AES_ECB_encrypt(&ctx, text);

    ci_printf("[AES] AFTER  ENCRYPT : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \r\n", text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7], text[8], text[9], text[10], text[11], text[12], text[13], text[14], text[15]);

    AES_ECB_decrypt(&ctx, text);

    ci_printf("[AES] AFTER  DECRYPT : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \r\n", text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7], text[8], text[9], text[10], text[11], text[12], text[13], text[14], text[15]);
}

int main(void)
{
    // .data 섹션 정보가 올바르게 로드되지 않는 이슈가 발생하여 직접 복사하도록 수정
    load_data_section();

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

    // AES128 암호화, 복호화 테스트 함수
    // aes128_test();

    // AES128 암호화/복호화 키 정보 초기화 (NOTE: 현재 예제 키를 사용하므로, 올바른 키를 생성하여 적용해야함)
    ci_aes_init();

    ci_printi("[INFO] MODEL : SULLIVAN 1.5 \r\n");
    ci_printi("[INFO] FW    : %u.%u%u (%s) \r\n", firmwareInfo.version[0], firmwareInfo.version[1], firmwareInfo.version[2], firmwareInfo.buildData);

    while (1)
    {
        func_normal();
        func_sleep();
    }
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

    bool powerButtonPushed = false;
    bool conneded_ISD      = false;
    bool mappingConnection = false;

    // 부트로더에서 각 코어의 이미지 로딩 시간이 오래 걸리므로, CM3 이미지 시작 직후 워치독 리프레시 수행
    SYS_WATCHDOG_REFRESH();

    main_counter                                      = 0;
    cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded = 0;

    systemState.systemOff = false;

#if 0  // 디버그 메시지 확인을 위해 약 2초의 딜레이를 주었다. (2026.03.06)
    for (int delay_i = 0; delay_i < 2000; delay_i++)
    {
        Sys_Delay(SystemCoreClock / 1000);  // 1ms
        SYS_WATCHDOG_REFRESH();
    }
#endif

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

    while (1)
    {
        if ((iterationFlag == true))
        {
            mcuErrorCode = readErrorCode();

            usbConnectorState = readUsbConnectorState();

            ledPattern = geteLED_OutputPattern();

            batteryLevel = updateBatteryLevel(usbConnectorState.chargerConnectorPluggedIn, ledPattern);

            powerButtonPushed = isPowerButtonPushed();

            systemState = systemControl(ledPattern,  // 최초 부팅 시 초기 값 : en__LED_NA
                                        mcuErrorCode,
                                        usbConnectorState,
                                        batteryLevel,  // Initialize 단계에서 배터리 정보 수집 완료되어 알 수 없는 배터리 레벨이 될 수 없다.
                                        powerButtonPushed,
                                        isd_state.conneded_ISD,                   // 최초 부팅 시 초기 값 : false
                                        BLE_communicationState.mappingConnection  // 최초 부팅 시 초기 값 : false
            );

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
                updateEarPieceStatus();
            }

#if 1  // LED가 어떤 패턴으로 업데이트 되는지 디버깅하는 용도
            {
                static EN__LED_PATTERN d_led_pattern_prev = 0;

                if (d_led_pattern_prev != systemState.Led_Pattern)
                {
                    d_led_pattern_prev = systemState.Led_Pattern;

                    debug_led_pattern(d_led_pattern_prev);
                }
            }
#endif
            LedPatternOut(systemState.Led_Pattern);

            NRF_On_OFF(isd_state, systemState.BLE_Off, BLE_communicationState.mappingConnection, BLE_communicationState.BLE_Off_Command);

#if 0
            /**
             * CM3의 setFlag_AudioParametersCalculationDone_Cm3ToCfx() 함수에서 트리거 된다.
             * CFX가 각종 계수를 계산 후, 그 값을 디버깅하는 코드이다. */
            if (FS_MEM_UART->flag[0] == 2)
            {
                ci_printv("\r\n[DEBUG] LOG MAPPING COEFFS \r\n");
                ci_printv("                       ");
                ci_printv("X MIN        ");
                ci_printv("X MIN GAINED ");
                ci_printv("X MAX        ");
                ci_printv("COEFF A      ");
                ci_printv("COEFF B      ");
                ci_printv("C LEVEL      ");
                ci_printv("T LEVEL \r\n");

                for (int i = 0; i < 32; i++)
                {
                    ci_printv("        CH%2u           %-12d %-12d %-12d %-12d %-12d %-12d %-12d \r\n", i, FS_MEM_UART->buffer[0 + i], FS_MEM_UART->buffer[32 + i], FS_MEM_UART->buffer[64 + i], FS_MEM_UART->buffer[96 + i], FS_MEM_UART->buffer[128 + i], FS_MEM_UART->buffer[160 + i], FS_MEM_UART->buffer[192 + i]);
                }

#if 0
                /**
                 * 로그 계산 후 Y 결과가 T에서 C 사이로 출력되는지 확인하기 위한 디버그 코드
                 * calculate_logarithmMapping_coeff_with_audioVolume() 함수에서 수행한다. */
                ci_printf("\r\n");

                for (int i = 400; i < 500;)
                {
                    for (int j = 0; j < 10; j++, i++)
                    {
                        n = snprintf(out, sizeof(out), "%12d ", FS_MEM_UART->buffer[i]);
                        ci_printf(out);
                    }
                    ci_printf("\r\n");
                }

                ci_printf("\r\n");
#endif

                FS_MEM_UART->flag[0] = 0;
            }  // xMin에 게인 적용된 로그매핑 A, B 계수 디버깅 구문 끝.
#endif
            do
            {
#if 0
                /**
                 * 어느 영역의 AGC 계수를 사용할 것인지 디버깅하는 코드이다. (audio_agc() 함수)
                 * 오디오 믹스와 AGC 결과를 출력해서 확인하기 위한 코드이다. (위 처리 후 main() 함수 내) */
                if (cfx_cm3_sharedMemoryAll.CM3_tempValue1 == 1)
                {
                    ci_printf("\r\n");
                    ci_printf("Region : %s, max audio input = %d, audio volume = %d \r\n",
                              cfx_cm3_sharedMemoryAll.CM3_tempValue2 == 1   ? "Noise"
                              : cfx_cm3_sharedMemoryAll.CM3_tempValue2 == 2 ? "Attenuation"
                              : cfx_cm3_sharedMemoryAll.CM3_tempValue2 == 3 ? "Amplify"
                                                                            : "Unknown",
                              cfx_cm3_sharedMemoryAll.maxAudioInput,
							  cfx_cm3_sharedMemoryAll.userSettingValue.audioVolume);

                    ci_printf("Mix : 0x %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X \r\n",
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[0],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[1],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[2],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[3],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[4],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[5],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[6],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[7],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[8],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[9],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[10],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[11],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[12],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[13],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[14],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[15]);

                    ci_printf("AGC : 0x %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X \r\n",
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[16],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[17],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[18],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[19],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[20],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[21],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[22],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[23],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[24],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[25],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[26],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[27],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[28],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[29],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[30],
                              cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[31]);

                    cfx_cm3_sharedMemoryAll.CM3_tempValue1 = 0;
                }
#endif
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
        case en__LED_ISD_StimulationOut_batteryNormal:
            ci_printv("STIM OUT BATTERY NORMAL \r\n");
            break;
        case en__LED_ISD_StimulationOut_batteryLow:
            ci_printv("STIM OUT BATTERY LOW \r\n");
            break;
        case en__LED_StandbyForconneded_ISD_batteryNormal:
            ci_printv("STANBY CONN ISD BATTERY NORMAL \r\n");
            break;
        case en__LED_StandbyForconneded_ISD_batteryLow:
            ci_printv("STANBY CONN ISD BATTERY LOW \r\n");
            break;
        case en__LED_MappingConneted_ISD_Connected_BatteryNormal:
            ci_printv("MAPPING CONN ISD CONN BATTERY NORMAL \r\n");
            break;
        case en__LED_MappingConneted_ISD_Connected_BatteryLow:
            ci_printv("MAPPING CONN ISD CONN BATTERY LOW \r\n");
            break;
        case en__LED_MappingConneted_ISD_Unconnected_BatteryNormal:
            ci_printv("MAPPING CONN ISD NOT CONN BATTERY NORMAL \r\n");
            break;
        case en__LED_MappingConneted_ISD_Unconnected_BatteryLow:
            ci_printv("MAPPING CONN ISD NOT CONN BATTERY LOW \r\n");
            break;
        case en__LED_BatteryChargingLevel_0per:
            ci_printv("BATTERY CHARGING 0%% \r\n");
            break;
        case en__LED_BatteryChargingLevel_0btw20:
            ci_printv("BATTERY CHARGING 0%%-20%% \r\n");
            break;
        case en__LED_BatteryChargingLevel_20btw40:
            ci_printv("BATTERY CHARGING 20%%-40%% \r\n");
            break;
        case en__LED_BatteryChargingLevel_40btw60:
            ci_printv("BATTERY CHARGING 40%%-60%% \r\n");
            break;
        case en__LED_BatteryChargingLevel_60btw80:
            ci_printv("BATTERY CHARGING 60%%-80%% \r\n");
            break;
        case en__LED_BatteryChargingLevel_80btw100:
            ci_printv("BATTERY CHARGING 80%%-100%% \r\n");
            break;
        case en__LED_BatteryChargingLevel_100per:
            ci_printv("BATTERY CHARGING 100%% \r\n");
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
    Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_FPGA_SLEEP); /* Disable FPGA */
    OnOff_3V_PMIC_CM3_to_CFX(false);                /* Disable 3.3V, 1.2V PMIC */

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

    Uninitialize(); /* Disable peripherals and DIOs */

    ci_power_sleep();

    // ci_timer_init(OTE_1_5_GEN_TIMER_TICK_500MS_PM_LP); /* Make 500ms timer for watchdog refresh */
    ci_timer_init(19999); /* Make 500ms timer for watchdog refresh */

    while (1)  // ULP loop
    {
        SYS_WATCHDOG_REFRESH();

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
