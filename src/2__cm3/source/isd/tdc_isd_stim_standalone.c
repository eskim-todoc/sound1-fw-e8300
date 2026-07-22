#include <hw.h>
#include <tdc_isd_pcm.h>
#include <stdbool.h>

#include "FPGA.h"
#include "internalStimulationChip.h"
#include "tdc_isd.h"
#if 0
#include "tdc_hal_i2c_cfx.h"
#else
#include "tdc_hal_i2c_isd.h"
#endif
#include "tdc_stim_definitions.h"

#include "tdc_isd.h"
#include "tdc_shm.h"
#include "tdc_isd_stim_para_setting.h"
#include "tdc_stim_para_cal.h"
#include "tdc_stim_indicator.h"
#include "tdc_sys_error.h"

// 구조체의 배치되는 주소를 sections.ld 파일을 수정하여 LPDSP32_PRAM5에 위치한다.
extern ST__CFX_CM3_SharedMemory_ALL cfx_cm3_sharedMemoryAll __attribute__((section(".shared_memory")));

static bool newMapLoadeFlagForStimulParaCalculation = true;

void tdc_isd_set_new_map_loaded_flag(void)
{
    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);

    newMapLoadeFlagForStimulParaCalculation = true;

    TDC_PRINTF_V("[INDICATE] NEW MAP LOADED, SO CALCULATE STIMUL PARAMETERS \r\n");
}

void tdc_isd_clear_new_map_loaded_flag(void)
{
    newMapLoadeFlagForStimulParaCalculation = false;

    TDC_PRINTF_V("[INDICATE] DONE FOR NEW MAP LOADED STIMUL PARAMETERS \r\n");
}

bool tdc_isd_is_new_map_loaded_flag(void)
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

bool tdc_isd_stim_standalone_step(void)
{
    bool startSettingTrigger;

    static bool    stimulationSettingIsDone = false;
    static bool    calculationParameter     = false;
    tdc_sys_error_code_t errorCode;

    // Sys_GPIO_Set_High(DIO_PIN_INDEX_for_LED_color_R); //3

    // CFX에서 맵데이터의 로딩이 완료될 때까지 로딩
    if (tdc_shm_is_map_data_loaded_cfx())
    {
        errorCode = tdc_sys_error_read();

        if (errorCode.dataProcessingErrorFlag != en__unusableMapData)
        {
            // main.c의 update_mapNum() 함수에서
            // CM3 스스로가 새로운 맵으로 자극 관련 파라미터의 계산을 다시 하도록
            // 전역변수 newMapLoadeFlagForStimulParaCalculation를 true로 설정하였다.
            // 그래서 CFX가
            // cfx_cm3_sharedMemoryAll.mapChangeFlag.cfx_Reloaded_MapdataFlag를 1로 설정하여
            // 이 구문에 진입한 직후에는 아래의 tdc_isd_is_new_map_loaded_flag() 결과는 true로 시작한다.

            if (tdc_isd_is_new_map_loaded_flag())  // 맵이 변경되어서  계산이 필요한 경우.
            {
                calculationParameter = tdc_stim_calc_para_and_cfx_share();

                tdc_stim_indicator_set_level_255();  // 자극 알림 크기 uA -> 255레벨로 변환

                tdc_shm_set_flag_audio_parameters_calculation_done_cm3_to_cfx();  // cfx에 파라미터 계산이 완료 되었을을 알려주는 플레그

                tdc_isd_clear_stim_para_setting_done();

                tdc_isd_clear_new_map_loaded_flag();  // 여기서 newMapLoadeFlagForStimulParaCalculation를 false로 클리어함

                startSettingTrigger = true;
            }
            else
            {
                startSettingTrigger = false;
            }

            if (calculationParameter)
            {
                static bool is_need_to_print_message_for_live_stimulation = true;

                stimulationSettingIsDone = tdc_isd_is_stim_para_setting_done();

                if (!stimulationSettingIsDone)
                {
                    // 여기서 PCM 프로토콜을 사용하여 내부기 칩을 설정하는 구문을 수행한다.
                    // 대표적으로, 모노폴라, 바이폴라 등이다.
                    tdc_isd_stim_setting_step(startSettingTrigger);

                    is_need_to_print_message_for_live_stimulation = true;
                }
                else
                {
                    if (is_need_to_print_message_for_live_stimulation)
                    {
                        is_need_to_print_message_for_live_stimulation = false;
                        TDC_PRINTF_D("[STIMULATION] START LIVE MODE \r\n");
                    }

                    tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_LiveStimulation);  // 자극 출력 시작

                    tdc_isd_set_i2c_free();
                }
            }
            else
            {
                tdc_sys_error_update(en__dataProcessing_ERROR, en__unusableMapData, __LINE__);
            }
        }
        else
        {
            tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 맵이 변경 되는 동안  NOP
        }
    }
    else
    {
        tdc_shm_change_pcm_output_mode(PcmBitStream_Mode_NopStandby);  // 맵이 변경 되는 동안  NOP
    }

    // Sys_GPIO_Set_Low(DIO_PIN_INDEX_for_LED_color_R); //3

    return stimulationSettingIsDone;
}
