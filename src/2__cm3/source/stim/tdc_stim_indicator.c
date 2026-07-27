#include <tdc_shm.h>
#include <tdc_stim_para_cal.h>
#include <tdc_ble_mapping.h>
#include <internalStimulationChip.h>

static bool stimulationIndicatorTrigger = false;

bool tdc_stim_indicator_is_triggered(void)
{
    return stimulationIndicatorTrigger;
}

void tdc_stim_indicator_set_trigger(void)
{
    stimulationIndicatorTrigger = true;
}

void clear_stimulationIndicatorTrigger(void)
{
    stimulationIndicatorTrigger = false;
}

void tdc_stim_indicator_out(int userSettingEnableStimulationIndicator, bool stimulationTriggerLowPower, bool stimulationTriggerMapping)
{
    static int counter                 = 0;
    static int outputPulseTrainCounter = 0;

    int* p_stimulIndicatorOutOnOff;

    if (stimulationTriggerLowPower)
    {
        if (userSettingEnableStimulationIndicator == 1)
        {
            tdc_stim_indicator_set_trigger();
        }
    }

    if (stimulationTriggerMapping)
    {
        tdc_stim_indicator_set_trigger();
    }

    if (tdc_stim_indicator_is_triggered())
    {

        if (counter < 80)
        {

            tdc_shm_set_stimulation_indicator_on_off_by_cm3(true);
        }
        else
        {
            tdc_shm_set_stimulation_indicator_on_off_by_cm3(false);
        }

        if (counter >= 160)
        {
            counter = 0;
            outputPulseTrainCounter++;
        }

        if (outputPulseTrainCounter == 3)  //
        {
            outputPulseTrainCounter = 0;
            clear_stimulationIndicatorTrigger();
        }

        counter++;
    }
}

/* 자극 알림 레벨 산출의 구버전 구현은 제거했다(#if 0 사장, 53줄).
 * 구버전이 부르던 getCalculatedStimulationIndcatorLevel() 은 CM3 어디에도 정의가 없어
 * 되살리면 링크 에러가 난다. 아래 현행 구현만 유효하다. */

void tdc_stim_indicator_set_level_255(void)
{
    int offset;
    int stimulationLevel_uA;
    int tempStimulationLevel_uA;
    int stimulation_Channel;
    int temp;

    const ST_STIUL_DAC_REGISTER_VALUE* p_stimulDAC_setting;

    const ST__CFX_CM3_SharedMemory_mapData* p_mapData;

    p_stimulDAC_setting = tdc_stim_read_dac_register_value();
    p_mapData           = tdc_shm_get_pointer_current_map_data();

    stimulation_Channel     = p_mapData->stimulationIndicatorChannelNum;
    tempStimulationLevel_uA = p_mapData->stimulationIndicatorAmplitude_uA;
    if (tempStimulationLevel_uA <= p_mapData->T_level_uA[stimulation_Channel - 1])
    {
        tempStimulationLevel_uA = p_mapData->T_level_uA[stimulation_Channel];
    }

    offset              = tdc_stim_read_dac_offset_value();
    stimulationLevel_uA = tempStimulationLevel_uA - offset;

    switch (p_stimulDAC_setting->DAC_Slope_register)
    {
        case Stimulation_DAC_A:  //
            temp = stimulationLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_A;
            tdc_shm_set_calculated_stimulation_indicator_level_by_cm3(temp >> 15);
            break;

        case Stimulation_DAC_B:  //
            temp = stimulationLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_B;
            tdc_shm_set_calculated_stimulation_indicator_level_by_cm3(temp >> 15);
            break;

        case Stimulation_DAC_C:  //
        {
            temp = stimulationLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_C;
            tdc_shm_set_calculated_stimulation_indicator_level_by_cm3(temp >> 15);
        }
        break;

        case Stimulation_DAC_D:  //
            temp = stimulationLevel_uA * reciprocal_dividing_QI1F15_Stimulation_DAC_D;
            tdc_shm_set_calculated_stimulation_indicator_level_by_cm3(temp >> 15);
            break;
    }
}

