#ifndef __tdc_stim_para_cal_h__
#define __tdc_stim_para_cal_h__

#include <stdbool.h>

#include <tdc_stim_definitions.h>

#include <tdc_printf.h>

typedef struct
{
    int DAC_Slope_register; // cm3에서 계산함
    int DAC_offsetSlope_register;
    int DAC_offsetLevel_register; // cm3에서 변경함.

} ST_STIUL_DAC_REGISTER_VALUE;

bool tdc_stim_calc_para_and_cfx_share(void);

int tdc_stim_calc_frame_per_channel(int pulsewidth);
int tdc_stim_calc_transferable_channel_num(int numFramePerChannel, int usableElectrodNum);
// const ST_STIUL_DAC_REGISTER_VALUE *tdc_stim_read_dac_register_value(void);
ST_STIUL_DAC_REGISTER_VALUE *tdc_stim_read_dac_register_value(void);
const int                    tdc_stim_read_dac_offset_value(void);
bool                         tdc_stim_set_range(int pulseWidth, int *T_level_uA, int *C_level_uA, int NumCh, int *T_level_255, int *C_level_255);

#endif
