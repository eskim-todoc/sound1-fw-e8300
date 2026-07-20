
#include <fn_from_cfx_eeprom_write.h>
#include "cfx_cm3_sharedMemory.h"
#include "ble_commonProtocol.h"
#include "cfx_cm3_shared_Memory_Addr.h"
#include "isd_interface_stimulationStandAlone.h"

#include "batteryNPowerControl.h"  // 새로 추가
#include <main.h>

#include <ci_battery.h>
#include <ci_battery.h>
#include <tdc_hal_dio.h>
#include <tdc_printf.h>

// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

/* CM3 생존 신호(heartbeat)를 공유 메모리에 게시한다.
 *
 * 펌웨어 안에는 이 값을 읽는 코드가 없다(CM3 · CFX · calibration 전수 확인).
 * 유일한 소비자는 디버거다 - JTAG/RTT 로 공유 메모리를 들여다볼 때 값이 계속
 * 증가하면 CM3 메인 루프가 살아 있다는 뜻이다. 비용이 int 대입 1회뿐이라
 * 진단 가치를 위해 존치한다(2026-07-20 은수님 판단).
 *
 * NOTE: 필드명 CM3_status 는 공유 메모리 ABI 라 바꾸지 않는다. CFX(shared_memory.h)
 *       와 calibration 헤더가 같은 레이아웃을 복제하고 있어 한쪽만 바꾸면 어긋난다. */
void tdc_shared_publish_cm3_heartbeat(int beat)
{
    cfx_cm3_sharedMemoryAll.CM3_status = beat;
}

/* update_CM3tempValue1_toCFX() / update_CM3tempValue2_toCFX() 제거(2026-07-20):
 * 두 래퍼 모두 호출처가 0 이었다.
 *
 * 단 CM3_tempValue1 / CM3_tempValue2 '필드'는 살아 있다 - CFX 의 AGC 가
 * tempValue1 을 읽고 tempValue2 에 쓴다(1__cfx/signalProcessing/agc.c:100/213/
 * 238/259 및 :215/240/261). CM3 쪽 초기화도 stimulationParaCal.c:617~618 에서
 * 직접 대입한다. 필드를 지우면 AGC 가 깨지므로 래퍼만 제거했다. */

bool isCFX_EEPROM_data_Loaded(void)
{
    if (cfx_cm3_sharedMemoryAll.CFX_EEPROM_data_is_Loaded == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

////
// 공유 메모리 주소 확인

bool sharedMemoryAddresError(void)
{
    if (&cfx_cm3_sharedMemoryAll != ((ST__CFX_CM3_SharedMemory_ALL *) StartAddressCM3_sharedVarialbe))
    {
        return true;
    }
    else
    {
        return false;
    }
}

//////////////////////////// 하드웨어 상태 제어 및 상태 공유 //////////////////////////////////
//
//
//

/* readUsbConnectorState() 제거(2026-07-15): Sullivan 유산.
 * USB 케이블 / 캐링케이스 / 홀센서 커버를 독립 GPIO 3개로 읽던 함수였다.
 * Sound1 은 포고핀 크래들 단일 경로이며 충전 상태는 QCC 0x34 로 수신한다
 * (snd_charger_set_state / snd_charger_get_state, batteryNPowerControl.c).
 * 호출처가 전부 dead 함수 안이었으므로 실행되지 않았다.
 * DIO_PIN_INDEX_for_* 핀 정의는 tdc_hal_dio.c 의 저전력 모드 설정이 계속 사용하므로 유지.
 * 상세: docs/tasks/main/20260715_systemcontrol-fsm-decompose/분석-부록-sullivan유산.md */

void changeSystemModeFlag(EN__SYSTEM_OP_MODE flag)
{
    cfx_cm3_sharedMemoryAll.systemShare.system_opMode = flag;
}

bool isPowerButtonPushed(void)
{
    // 1.5세대에서는 가속도 센서 인터럽트 상태를 CM3가 직접 처리하기 때문에
    // 더이상 공유 메모리의 powerButton_pushed_CFX_to_CM3 변수를 사용하지 않는다.
    // 단, 디버깅을 위해 powerButton_pushed_CFX_to_CM3 변수를 활용한다.
#if 1
    if (Sys_GPIO_Read(DIO_PIN_INDEX_for_Accelerometer) == 0)  // ACTIVE LOW
    {
        if (cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 == 0)
        {
            cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 = 1;
            TDC_PRINTF_V("[ACC] DOUBLE TAP DETECTED \r\n");
        }

        return true;
    }

    cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 = 0;

    return false;
#else  // 1.5세대 레거시 코드
    if (Sys_GPIO_Read(DIO_PIN_INDEX_for_Accelerometer) == 0)  // ACTIVE LOW
    {
        cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 = 1;
    }

    if (cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 == 1)
    {
        cfx_cm3_sharedMemoryAll.systemShare.powerButton_pushed_CFX_to_CM3 = (int) 0;
        TDC_PRINTF_V("[ACC] INTERRUPT OCCURRED \r\n");
        return true;
    }
    else
    {
        return false;
    }
#endif
}

void OnOff_3V_PMIC_CM3_to_CFX(bool OnOff)
{
    // 1세대에서는 RF PMIC 5V를 CM3가 직접 켜기/끄기를 제어할 수 있었지만,
    // 1.5세대에서는 FPGA에서 V_LINK_ON 핀으로 RF PMIC 5V의 켜기/끄기를 제어한다.
    // 이 V_LINK_ON은 CM3가 제어하는 FPGA_SLEEP 신호를 바이패스 하는 구조이다.
    // 이는 곧, RF PMIC 5V를 켜기/끄기 제어를 하려면 FPGA 자체를 슬립 상태로 진입 시켜야할 수도 있다는 의미다.
    // 위의 이유로, 결과적으로 RF PMIC 5V는 항상 활성화되어 있는 구조로 동작하게 되었다.
    // 그래서 아래 코드는 사실상 아무 의미 없다. (by 김은수, 2026.02.20)
    if (OnOff)
    {
        cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX = 1;
    }
    else
    {
        cfx_cm3_sharedMemoryAll.systemShare.consumptionPowerControl_Command_CM3_to_CFX = 2;
    }
}

void enterLowPowerMode_CM3_to_CFX()
{
    OnOff_3V_PMIC_CM3_to_CFX(false);
    cfx_cm3_sharedMemoryAll.systemShare.enter_ULP_mode_Command_CM3_to_CFX = 1;
}

int readBatteryCalibrationValue(void)
{
    return cfx_cm3_sharedMemoryAll.batteryCalibrationValue;
}

int readBatteryLevel_FromCFX(void)
{
    ci_battery_update();
    return cfx_cm3_sharedMemoryAll.systemShare.batteryLevel_CfX_to_CM3;
}

void updateEarpieceDetectionValue_toCFX(bool detection)
{
    if (detection)
    {
        cfx_cm3_sharedMemoryAll.earpieceDetecion = 1;
    }
    else
    {
        cfx_cm3_sharedMemoryAll.earpieceDetecion = 0;
    }
}

//////////////////////////// 신호처리 관련  //////////////////////////////////
//
//
//

void changePcmOutputMode(int currentPcmOutputMode)
{
    cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode = currentPcmOutputMode;
}

void changeNextPcmOutputMode(int nextPcmOutputMode)
{
    cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode_next = nextPcmOutputMode;
}

void fillSepcificCommndBuffer(int Index, int data)
{
    cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_specificBuffer[Index] = data;
}

int readCurrentPcmOutputMode(void)
{
    return cfx_cm3_sharedMemoryAll.cfx_PCM_interface.PCM_mode;
}

int readConnectionCheckPcmState(void)
{
    return cfx_cm3_sharedMemoryAll.cfx_PCM_interface.conneded_ISDCheckPCM_state;
}

void clearConnectionCheckPcmFiredFlag(void)
{
    cfx_cm3_sharedMemoryAll.cfx_PCM_interface.conneded_ISDCheckPCM_state = BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation;
}

bool isUserSettingValueLoaded_CFX(void)
{
    if (cfx_cm3_sharedMemoryAll.userSettingValueLoadedFlag == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void setCommandMapChange_Cm3ToCfx(void)
{
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag = 0;
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange     = 1;
}

bool isMapdateLoaded_CFX(void)
{
    if (cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

ST__CFX_CM3_SharedMemory_mapData *getPointerCurrentMapData(void)
{
    return &(cfx_cm3_sharedMemoryAll.currentMapData);
}

ST__CFX_CM3_SharedMemory_calculatedStimulPara_byCM3 *getPointerCalculatedStimulPara_byCM3(void)
{
    return &(cfx_cm3_sharedMemoryAll.calculatedStimulPara_byCM3);
}

/*
int *getCalculatedStimulationIndcatorLevel_byCM3(void)
{
    return &(cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255 );
}

int *getPointer_stimulationIndcatorOnOff_byCM3(void)
{
    return &(cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 );
}
*/

void setCalculatedStimulationIndcatorLevel_byCM3(int stimulationIndicatorLevel_255)
{
    cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulLevel_255 = stimulationIndicatorLevel_255;
}

void setStimulationIndcatorOnOff_byCM3(bool On_Off)
{
    if (On_Off)
    {
        cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 = 1;
    }
    else
    {
        cfx_cm3_sharedMemoryAll.calculatedStimulationIndcator_byCM3.indicatorStimulOutput_OnOff_Coltroled_byCM3 = 0;
    }
}

void setFlag_AudioParametersCalculationDone_Cm3ToCfx(void)  // 파라미터 계산이 완료되었을을 CFX에 알려주는 플레그 (CFX에서 확인 후 자동을 클리어)
{
    cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3_audioParameterCalculationDone_Flag = 1;

#if 1
    /* NOTE: 로그매핑 오디오 볼륨 게인 적용된 A, B 계수 테스트 코드 */
    FS_MEM_UART->flag[0] = 1;
#endif
}

void updagteBacktelControlValue_toCFX(int value)  // 실시간 자극 출력에서 내부기 연결확인용 벡텔을 켜고 끌때 사용하기위해서 공유
{
    cfx_cm3_sharedMemoryAll.backtelControlRegister = value;
}

int *readCurrentStimulLevel_255(void)
{
    return &(cfx_cm3_sharedMemoryAll.currentOutputStimulLevel_255[0]);
}

const int readAudioSignalMax(void)
{
    return cfx_cm3_sharedMemoryAll.maxAudioInput;
}

//////////////////////////// 내부기 정보 관련  //////////////////////////////////
//
//
//

int read_ISD_manufacture_ID(int index)
{
    int isd_id_onFlash = 0;

    isd_id_onFlash = isd_id_onFlash | cfx_cm3_sharedMemoryAll.cfx_ISD_info[index].isd_year;

    isd_id_onFlash = isd_id_onFlash << 8;
    isd_id_onFlash = isd_id_onFlash | cfx_cm3_sharedMemoryAll.cfx_ISD_info[index].isd_month_model;

    isd_id_onFlash = isd_id_onFlash << 16;
    isd_id_onFlash = isd_id_onFlash | cfx_cm3_sharedMemoryAll.cfx_ISD_info[index].isd_serial;

    return isd_id_onFlash;
}

int *read_recomcon_passkey_connected_ISD(int connected_ISD_Num)
{
    return cfx_cm3_sharedMemoryAll.cfx_ISD_info[connected_ISD_Num - 1].remocon_passkey;
}

void changeConnected_isd_num_CFX(int isd_num)
{
    // 1세대에서는 이 함수를 통해 ISD 번호를 변경하면,
    // CFX에서 ISD 번호 변경을 감지 후 해당 번호에 해당하는 맵 데이터를 플래시에서 로드하고 업데이트한다.
    // 1.5세대에서는 맵 데이터에 대한 플래시 로드를 CM3가 진행하기 때문에, 아래와 같이 기능을 추가하였다.
    //
    // #1 : ISD 번호에 해당하는 맵 스탬프 파일을 읽고, 공유 메모리의 connected_ISD_Map_info.mapStamp로 복사
    // #2 : ISD 번호에 해당하는 프로그램 4개 파일을 읽고,
    //      공유 메모리의 connected_ISD_Map_info.map_data[0]에서 [3]까지 각 프로그램의 매핑 일자를 복사
    // #3 : 매핑 일자 정보를 토대로 공유 메모리의
    //      connected_ISD_Map_info.user_usableMapNum과 usableMapIndex[0]에서 [3]까지
    //      사용 가능한 프로그램의 개수와 각 프로그램의 사용 가능 여부 플래그를 설정
    // #4 : ISD 번호에 해당하는 사용자 설정 파일을 읽고, 공유 메모리의 userSettingsValue로 복사
    // #5 : 마지막으로 연결된 ISD 번호를 업데이트하여 CFX가 맵 데이터 관련 처리를 다시 하도록 유도함

#if 1  // 1.5세대에서 CFX 대신 CM3가 처리하도록 구현된 코드 블록
    if (0 < isd_num)
    {
        ci_map_read_isd_info(isd_num);            // ISD 정보 로드
        ci_map_read_user_setting_value(isd_num);  // 사용자 설정 값 로드
        ci_map_read_map_stamp(isd_num);           // 매핑 일자 정보 로드
        ci_map_read_map_data(isd_num, 1);         // 프로그램 1 로드
        ci_map_read_map_data(isd_num, 2);         // 프로그램 2 로드
        ci_map_read_map_data(isd_num, 3);         // 프로그램 3 로드
        ci_map_read_map_data(isd_num, 4);         // 프로그램 4 로드

        fn_copy_MapInfo_toCM3(isd_num);                // #1. 맵 스탬프, 프로그램 별 매핑 일자, 사용 가능한 맵 프로그램 인덱스, 사용 가능한 맵 개수 복사
        fn_copy_userSettingParameters_toCM3(isd_num);  // #2. 사용자 설정 값 복사
    }
#endif

    cfx_cm3_sharedMemoryAll.connected_ISD_num = isd_num;
}

int read_connected_ISD_Num(void)
{
    return cfx_cm3_sharedMemoryAll.connected_ISD_num;
}

int *readConnected_ISD_userName(int connected_ISD_Num)
{
    return &(cfx_cm3_sharedMemoryAll.cfx_ISD_info[connected_ISD_Num - 1].isd_userName[0]);
}

EN__LOCATION_OF_ISD readConnected_ISD_Location(int connected_ISD_Num)
{
    return (cfx_cm3_sharedMemoryAll.cfx_ISD_info[connected_ISD_Num - 1].isd_location_RL);
}

// 열결된 내부기의  맵 요약 정보

// 열결된 내부기의  사용 가능한 맵 갯수
int readConnected_ISD_usableMapNum(void)
{
    return (cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.user_usableMapNum);
}

// 열결된 내부기의  사용 가능한 맵 인덱스
int *readConnected_ISD_usableMapIndex(void)
{
    return (cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.usableMapIndex);
}

// 열결된 내부기의  맵 생성일자 들
int *readConnected_ISD_MapStamp(void)
{
    return ((int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.mapStamp));
}

// 열결된 내부기의  맵 생성일자 들
int *readConnected_ISD_MapDate(int mapNum)
{
    int *p_mapDate;

    switch (mapNum)
    {
        case 1:
            p_mapDate = ((int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[0]));
            break;
        case 2:
            p_mapDate = ((int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[1]));
            break;
        case 3:
            p_mapDate = ((int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[2]));
            break;
        case 4:
            p_mapDate = ((int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[3]));
            break;
        default:
            p_mapDate = ((int *) &(cfx_cm3_sharedMemoryAll.connected_ISD_Map_info.map_date[0]));
            break;
    }

    return p_mapDate;
}

//////////////////////////// 사용자 설정 값 관련  //////////////////////////////////
//
//
//

void changeProgramMapNum(int mapNum)
{
    if (mapNum <= 5)
    {
        cfx_cm3_sharedMemoryAll.userSettingValue.mapNum = mapNum;

        /* __KIM: 아래에서
         * cfx_cm3_sharedMemoryAll.mapChangeFlag.cm3Command_mapChange = 1;
         * 을 설정하게 되면, CFX의 system_Control.c의 LB_Normal_PowerMode() 함수 내에서
         * 맵 번호 변경을 감지하고, 그에 맞는 행동을 취하게 된다.
         * 이 행동은 크게 Stand alone모드와 Mapping 프로그램 연결 중인 상태로 나뉜다.
         * Stand alone 모드에서는 PCM 출력 값을 NOP으로 설정 후 맵 데이터를 EEPROM에서 읽어서 공유 메모리에 복사하고
         * CFX에서 사용하는 addr_ 관련 변수들에 이 값을 업데이트 하고, 자극 패킷 헤더 설정 및 FFT Passbin 계산을 한다.
         * Mapping 프로그램 연결 모드에서는 바로 addr_ 관련 변수들의 값을 공유 메모리로부터 읽어서 업데이트하고, 자극
         * 패킷 헤더 설정 및 FFT Passbin 계산을 한다. 그리고 위 두 모드가 다 끝나면 ,cfx_Reloaded_MapdataFlag = 1;로
         * 하고 cm3Command_mapChange = 0;으로 한다.
         */

        /*
         * 즉, 여기서
         * #1. 현재 연결중인 ISD의 맵 번호에 해당하는 파일을 읽고 공유 메모리로 업데이트 해야한다.
         * #2. FFT pass bin 파일도 여기서 읽도록 한다.
         */

#if 1  // from CFX to CM3
        if (0 < mapNum)
        {
            fn_copy_MappingData_toCM3(mapNum, cfx_cm3_sharedMemoryAll.connected_ISD_num);

            if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
            {
                fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
            }
        }

        if (ci_fft_read_pass_bin(cfx_cm3_sharedMemoryAll.currentMapData.numFrequencyBand) < 0)
        {
            ci_fft_init_pass_bin(cfx_cm3_sharedMemoryAll.currentMapData.numFrequencyBand);
            ci_fft_read_pass_bin(cfx_cm3_sharedMemoryAll.currentMapData.numFrequencyBand);
        }
#endif

        // cfx에 맵데이터를 eeprom에서 읽어 들이라고 명령한다.
        setCommandMapChange_Cm3ToCfx();

        // CM3에 새로운 맵으로 자극 관련 파라미터의 계산을 다시 하도록 플레그를 세팅한다.
        set_newMapLoadeFlagForStimulParaSetting();
    }
}

int readProgramMapNum(void)
{
    return cfx_cm3_sharedMemoryAll.userSettingValue.mapNum;
}

void changeStimulVolume(int volume)
{
    cfx_cm3_sharedMemoryAll.userSettingValue.stimulVolume = volume;

    if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
    {
        fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
    }
}

int readStimulVolume(void)
{
    return cfx_cm3_sharedMemoryAll.userSettingValue.stimulVolume;
}

void changeAudioVolume(int volume)
{
    cfx_cm3_sharedMemoryAll.userSettingValue.audioVolume = volume;

    if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
    {
        fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
    }
}

int readAudioVolume(void)
{
    return cfx_cm3_sharedMemoryAll.userSettingValue.audioVolume;
}

void changeLED_indicatorOnOff(EN__PAYLOAD_ON_OFF OnOff)
{

    cfx_cm3_sharedMemoryAll.userSettingValue.indicatorLED_OnOff = (int) OnOff;

    if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
    {
        fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
    }
}

int readLED_indicatorOnOff(void)
{
    return cfx_cm3_sharedMemoryAll.userSettingValue.indicatorLED_OnOff;
}

void changeTeleCoil_OnOff(EN__PAYLOAD_ON_OFF OnOff)
{
    cfx_cm3_sharedMemoryAll.userSettingValue.teleCoil_OnOff = (int) OnOff;

    if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
    {
        fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
    }
}

int readTeleCoil_OnOff(void)
{
    return cfx_cm3_sharedMemoryAll.userSettingValue.teleCoil_OnOff;
}

void changeStimulIndicator_OnOff(EN__PAYLOAD_ON_OFF OnOff)
{
    cfx_cm3_sharedMemoryAll.userSettingValue.indicatorStimul_OnOff = (int) OnOff;

    if (cfx_cm3_sharedMemoryAll.systemShare.system_opMode == en__normalMode)
    {
        fn_write_userSettingParameters(cfx_cm3_sharedMemoryAll.connected_ISD_num);
    }
}

int readStimulIndicator_OnOff(void)
{
    return cfx_cm3_sharedMemoryAll.userSettingValue.indicatorStimul_OnOff;
}

//////////////////////////// 맵 데이터 관련  //////////////////////////////////
//
//
//

// 매핑 프로그램 연결 상태 CFX에 전달.

void shareMappingProgramConnection(bool connection)
{
    if (connection)
    {
        cfx_cm3_sharedMemoryAll.isMappingProgramConneted = 1;
    }
    else
    {
        cfx_cm3_sharedMemoryAll.isMappingProgramConneted = 0;
    }
}

void setReadWriteMapDataFlashCommand(ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash command_ForFlash)
{
    if (command_ForFlash.flashCommand == flash_Command_Read)
    {
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 1;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index    = command_ForFlash.isd_index;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index    = command_ForFlash.map_index;

        fn_Read_Mapdata_mappingApp();
    }
    else if (command_ForFlash.flashCommand == flash_Command_Write)
    {
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 2;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index    = command_ForFlash.isd_index;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index    = command_ForFlash.map_index;

        fn_write_Mapdata_mappingApp();
    }
    else if (command_ForFlash.flashCommand == flash_Command_Erase)
    {
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 3;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index    = command_ForFlash.isd_index;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index    = command_ForFlash.map_index;

        fn_erase_Mapdata_mappingApp();
    }
    else if (command_ForFlash.flashCommand == flash_Command_Recover)
    {
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand = 4;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.isd_index    = command_ForFlash.isd_index;
        cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.map_index    = command_ForFlash.map_index;

        fn_recover_Mapdata_mappingApp();

        TDC_PRINTF_W("[FLASH] DONE FOR FLASH COMMAND : RECOVER MAPDATA (%u) \r\n", cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand);
    }

    //
    // NOTE: CFX의 system_Control.c의 LB_Normal_PowerMode() 함수의 내용을 여기서 수행하도록 한다.
    //
}

bool isReadWriteMapDataFlashCommandDone(void)
{
    if (cfx_cm3_sharedMemoryAll.ReadWriteCommand_ForFlash.flashCommand == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int *getPointerRepositoryForReadWriteMapData_isd_info(void)
{
    return ((int *) &(cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.ISD_info_mapData));
}

int *getPointerRepositoryForReadWriteMapData_userSetting(void)
{
    return ((int *) &(cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.userSettingValue_mapData));
}

int *getPointerRepositoryForReadWriteMapData_stimulPara(void)
{
    return ((int*) &(cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.readWritemapData));
}

int readCfxErrorCode(void)
{
    return cfx_cm3_sharedMemoryAll.CFX_ErrorCode;
}
