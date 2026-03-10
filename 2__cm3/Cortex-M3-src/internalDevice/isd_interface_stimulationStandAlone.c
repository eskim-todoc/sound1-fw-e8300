#include <hw.h>
#include <isdExecution/dirver_PCM.h>
#include <stdbool.h>

#include "FPGA.h"
#include "internalStimulationChip.h"
#include "isd_interface.h"
#if 0
#include "driver_cfx_i2c.h"
#else
#include "dirver_i2c_for_ISD.h"
#endif
#include "definitionsForAlgorithm.h"

#include "isd_interface.h"
#include "cfx_cm3_sharedMemory.h"
#include "isd_interface_stimulationParaSetting.h"
#include "stimulationParaCal.h"
#include "indicatorByStimul.h"
#include "error.h"

// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

static bool newMapLoadeFlagForStimulParaCalculation = true;

void set_newMapLoadeFlagForStimulParaSetting(void)
{
    changePcmOutputMode(PcmBitStream_Mode_NopStandby);

    newMapLoadeFlagForStimulParaCalculation = true;

    ci_printv("[INDICATE] NEW MAP LOADED, SO CALCULATE STIMUL PARAMETERS \r\n");
}

void clear_newMapLoadeFlag(void)
{
    newMapLoadeFlagForStimulParaCalculation = false;

    ci_printv("[INDICATE] DONE FOR NEW MAP LOADED STIMUL PARAMETERS \r\n");
}

bool isNewMapLoadeFlag(void)
{
    if (newMapLoadeFlagForStimulParaCalculation)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool stimulationStandAlone(void)
{
    bool startSettingTrigger;

    static bool    stimulationSettingIsDone = false;
    static bool    calculationParameter     = false;
    ST__ERROR_CODE errorCode;

    // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R); //3

    // CFX에서 맵데이터의 로딩이 완료될 때까지 로딩
    if (isMapdateLoaded_CFX())
    {
        errorCode = readErrorCode();

        if (errorCode.dataProcessingErrorFlag != en__unusableMapData)
        {
            // main.c의 update_mapNum() 함수에서
            // CM3 스스로가 새로운 맵으로 자극 관련 파라미터의 계산을 다시 하도록
            // 전역변수 newMapLoadeFlagForStimulParaCalculation를 true로 설정하였다.
            // 그래서 CFX가
            // cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag를 1로 설정하여
            // 이 구문에 진입한 직후에는 아래의 isNewMapLoadeFlag() 결과는 true로 시작한다.

            if (isNewMapLoadeFlag())  // 맵이 변경되어서  계산이 필요한 경우.
            {
                calculationParameter = calculationStimulParaN_cfxShare();

                setIndicatoStimlulLevel_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                setFlag_AudioParametersCalculationDone_Cm3ToCfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                clear_sitmulationParaSettingDone();

                clear_newMapLoadeFlag();  // 여기서 newMapLoadeFlagForStimulParaCalculation를 false로 클리어함

                startSettingTrigger = true;
            }
            else
            {
                startSettingTrigger = false;
            }

            if (calculationParameter)
            {
                static bool is_need_to_print_message_for_live_stimulation = true;

                stimulationSettingIsDone = isStimulParaIsSettingDone();

                if (!stimulationSettingIsDone)
                {
                    // 여기서 PCM 프로토콜을 사용하여 내부기 칩을 설정하는 구문을 수행한다.
                    // 대표적으로, 모노폴라, 바이폴라 등이다.
                    stimulationSetting(startSettingTrigger);

                    is_need_to_print_message_for_live_stimulation = true;
                }
                else
                {
                    if (is_need_to_print_message_for_live_stimulation)
                    {
                        is_need_to_print_message_for_live_stimulation = false;
                        ci_printd("[STIMULATION] START LIVE MODE \r\n");
                    }

                    changePcmOutputMode(PcmBitStream_Mode_LiveStimulation);  // 자극 출력 시작

                    change_i2c_is_free();
                }
            }
            else
            {
                errorCodeUpdate(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
            }
        }
        else
        {
            changePcmOutputMode(PcmBitStream_Mode_NopStandby);  // 맵이 변경 되는 동안  NOP
        }
    }
    else
    {
        changePcmOutputMode(PcmBitStream_Mode_NopStandby);  // 맵이 변경 되는 동안  NOP
    }

    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R); //3

    return stimulationSettingIsDone;
}
