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
    static int toggle                  = 0;
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

#if 0
void tdc_stim_indicator_set_level_255(void)
{

    int offset;
    int stimulationLevel_uA;
    int temp;

    const ST_STIUL_DAC_REGISTER_VALUE *p_stimulDAC_setting;
    ST__CFX_CM3_SharedMemory_calculatedStimulationIndcator_byCM3 *p_stimulIndicator;
    const ST__CFX_CM3_SharedMemory_mapData *p_mapData;


    p_stimulDAC_setting=tdc_stim_read_dac_register_value();
    p_stimulIndicator=getCalculatedStimulationIndcatorLevel();
    p_mapData=tdc_shm_get_pointer_current_map_data();


    // offset 값 계산
    if(p_stimulDAC_setting->DAC_offsetSlope_register==0)    // 2uA 기울기 오프셋
    {
        offset=p_stimulDAC_setting->DAC_offsetLevel_register<<1;    //2uA 기울기
    }
    else        // 4uA 기울기 오프셋
    {
        offset=p_stimulDAC_setting->DAC_offsetLevel_register<<2; //4uA 기울기
    }

    stimulationLevel_uA=p_mapData->stimulationIndicatorAmplitude_uA-offset;


    switch(p_stimulDAC_setting->DAC_offsetSlope_register)
    {
        case  0 :   //2uA 기울기
            p_stimulIndicator->indicatorStimulLevel_255=stimulationLevel_uA>>1;
        break;
        case 1 :    // 4uA 기울기
            p_stimulIndicator->indicatorStimulLevel_255=stimulationLevel_uA>>2;
        break;
        case 2  :   // 6uA 기울기      (1536_uA > 동적 영역 > 1024_uA)일 때 적용이 되므로 아래의 계산을 하더라도 오버플로우 발생이 없다.
        {
            temp=stimulationLevel_uA*21;  // 최대.. 1536*21=32256  ,,, 21는 1/6을 QI1F7로 표현한 것.
            p_stimulIndicator->indicatorStimulLevel_255=temp>>8;
        }

        break;
        case 3 :    // 8uA 기울기
            p_stimulIndicator->indicatorStimulLevel_255=stimulationLevel_uA>>3;
        break;


    }
}
#else

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

#endif
