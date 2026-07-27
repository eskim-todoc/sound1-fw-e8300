#include <tdc_isd_pcm.h>
#include <tdc_isd_fpga.h>
#include <tdc_isd.h>
#include <tdc_shm.h>
#include <tdc_isd_fpga.h>
#include <tdc_sys_error.h>
#include <FPGA.h>

#ifdef CM3_I2C_controls_FPAG
#include <tdc_hal_i2c_isd.h>
#else
#endif

#include <tdc_drv_isl9122.h>

#include <tdc_stim_common.h>
#include <tdc_printf.h>

static LAST_WRITTEN_REGISTER fpag_lastWrittenRegister;

void tdc_isd_fpga_reset_fpga_variable(void)
{
    fpag_lastWrittenRegister.systemResgister_1st_value       = systemResgister_1st_resetValue;
    fpag_lastWrittenRegister.systemResgister_2nd_value       = systemResgister_2nd_resetValue;
    fpag_lastWrittenRegister.stimulation_PhaseDuration_value = stimulation_PhaseDuration_resetValue;
    // fpag_lastWrittenRegister.fpga_IO_MUX_Configuration_value=fpga_IO_MUX_Configuration_resetValue;
    fpag_lastWrittenRegister.backterConfiguration_value = backterConfiguration_resetValue;
    // fpag_lastWrittenRegister.fpga_optional_configuration_value=fpga_IO_MUX_Configuration_resetValue;

    tdc_shm_update_backtel_control_value_to_cfx(fpag_lastWrittenRegister.backterConfiguration_value);
}

#if 0
int tdc_isd_fpga_get_written_value(I2C_ADDR_FPGA index)
{
    int value;

    switch(index)
    {
            case i2cAddr_FPGA_systemResgister_1st :
                value=fpag_lastWrittenRegister.systemResgister_1st_value;
            break;

            case i2cAddr_FPGA_systemResgister_2nd :
                value=fpag_lastWrittenRegister.systemResgister_2nd_value;
            break;

            case i2cAddr_FPGA_pulsePhaseWidth :
                value=fpag_lastWrittenRegister.stimulation_PhaseDuration_value;
            break;

            case i2cAddr_FPGA_backtel_Config :
                value=fpag_lastWrittenRegister.backterConfiguration_value;
            break;

            case i2cAddr_FGPA_IO_MUX :
                //value=fpag_lastWrittenRegister.fpga_IO_MUX_Configuration_value;
            break;

            case i2cAddr_FPGA_optional_Config :
                //value=fpag_lastWrittenRegister.fpga_optional_configuration_value;
            break;

            default :
                value=0;
                break;
    }

    return value;
}



void tdc_isd_fpga_update_written_value(I2C_ADDR_FPGA index, int value)
{
    switch(index)
    {
            case i2cAddr_FPGA_systemResgister_1st :
                fpag_lastWrittenRegister.systemResgister_1st_value=value;
            break;

            case i2cAddr_FPGA_systemResgister_2nd :
                fpag_lastWrittenRegister.systemResgister_2nd_value=value;
            break;

            case i2cAddr_FPGA_pulsePhaseWidth :
                fpag_lastWrittenRegister.stimulation_PhaseDuration_value=value;
            break;

            case i2cAddr_FGPA_IO_MUX :
                //fpag_lastWrittenRegister.fpga_IO_MUX_Configuration_value=value;
            break;

            case i2cAddr_FPGA_backtel_Config :
                fpag_lastWrittenRegister.backterConfiguration_value=value;
            break;

            case i2cAddr_FPGA_optional_Config :
                //fpag_lastWrittenRegister.fpga_optional_configuration_value=value;
            break;

            default :
                break;
    }
}
#endif

bool tdc_isd_fpga_read_version(int *p_readValue)
{
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_version, p_readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_version, p_readValue, 1))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "tdc_isd_fpga_read_version", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_systemregister_1st(int *p_readValue)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_systemResgister_1st, p_readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_systemResgister_1st, p_readValue, 1))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_sysReg1st", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_check_fpga_pcm_error(bool *isError)
{
    int        readValue;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_error_Flag, &readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_systemResgister_1st, &readValue, 1))
#endif
    {
        if ((readValue & 0x1F) != 0)
        {
            *isError = true;
            tdc_sys_error_update_fpga_system(readValue);
            TDC_PRINTF_E("[FPGA] ERROR OCCURRED, SYS_ERR_CHK : 0x%02X \r\n", readValue);
        }
        else
        {
            *isError = false;
        }

        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "tdc_isd_fpga_check_fpga_pcm_error", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_check_fpga_fifo_empty(bool *isEmpty)
{
    int        readValue;
    int        temp;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_systemResgister_2nd, &readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_systemResgister_2nd, p_readValue, 1))
#endif
    {
        temp = readValue & (1 << FPGA_BitPosition_FIFO_is_empty);

        if (temp == (1 << FPGA_BitPosition_FIFO_is_empty))
        {
            *isEmpty = true;
        }
        else
        {
            *isEmpty = false;
        }

        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "tdc_isd_fpga_check_fpga_fifo_empty", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_system_error_flag(int *p_readValue)
{
#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_error_Flag, p_readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_error_Flag, p_readValue, 1))
#endif
    {
        return true;
    }
    else
    {
        // I2C 읽기 실패, FPGA 초기화
        tdc_sys_error_update(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_backtel_error_flag(int *p_readValue)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_backtel_ErrorFlag, p_readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_backtel_ErrorFlag, p_readValue, 1))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_backtelErr", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_pulse_width(int *pulseWidth)
{
    int        readValue;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_pulsePhaseWidth, &readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_pulsePhaseWidth, p_readValue, 1))
#endif
    {
        *pulseWidth = readValue + FPGA_pulsePhaseWidth_minimum;  // FPGA에 설정된 값에 기본 오프셋 값이 더해진게 실제 펄스 폭이 된다.
        error_cnt   = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "tdc_isd_fpga_read_pulse_width", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_fifo_counter(int *counterFIFO)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_FIFO_counter, counterFIFO, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_FIFO_counter, p_readValue, 1))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_FIFO_cnt", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_backtel_config(int *p_readValue)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_backtel_Config, p_readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_backtel_Config, p_readValue, 1))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_backtelCfg", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_read_backtel_fifo(int *p_readValue, int counter)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_Backtel_FIFO, p_readValue, counter))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_Backtel_FIFO, p_readValue, counter))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_backtelFIFO", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}


bool tdc_isd_fpga_read_tx_power_level(int *p_readValue)
{
    int        readValue;
    static int error_cnt = 0;

    if (tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_VOLTAGESET, &readValue))
    {
        *p_readValue = (readValue);
        error_cnt    = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[PMIC]", "tdc_isd_fpga_read_tx_power_level", "RD",
                              en__RF_PowerIC_ERROR, en__I2C_RFPOW_ReadingError);

        tdc_isd_change_state(en__isdStatus_PowerIC_Reset);

        return false;
    }
}

bool tdc_isd_fpga_is_arbitrary_value_matched_normal_value(void)
{
    int readValue;
    int tempA, tempB, tempC;

    tdc_isd_fpga_read_backtel_fifo(&readValue, 1);

    if (readValue == df_forwardPathCheck_arbitraryValue)
    {
        return true;
    }
    else
    {

        return false;
    }
}

bool tdc_isd_fpga_is_arbitrary_value_matched_duplicate_zero_data(void)
{
    int readValue;
    int tempA, tempB, tempC;

    tdc_isd_fpga_read_backtel_fifo(&readValue, 1);

    if (readValue == df_duplicateZeroValue)
    {
        return true;
    }
    else
    {
        return false;
    }
}

EN_ISD_PowerState tdc_isd_fpga_read_isd_power_state(void)
{
    int readValue;
    int tempA, tempB, tempC;

    tdc_isd_fpga_read_fifo_counter(&readValue);

    if (readValue == 0)
    {
        return NoBacktel;
    }
    if (readValue == 1)
    {
        tdc_isd_fpga_read_backtel_fifo(&readValue, 1);
        return tdc_stim_data_extract_and_rshift(readValue, bitPosition_isd_PowerState, bitLength_isd_PowerState);
    }
    else
    {
        return BackTelNumTooMuch;
    }
}

// write FPGA

// 디테일
//////////////////

bool tdc_isd_fpga_write_reset(void)
{
    int        value;
    static int error_cnt = 0;

    value = 1 << FPGA_BitPosition_ResetFPGA;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_write(i2cAddr_FPGA_systemResgister_1st, &value, 1))
#else
    if (tdc_hal_i2c_cfx_write(i2cAddr_FPGA_systemResgister_1st, &value, 1))
#endif
    {
        tdc_isd_fpga_reset_fpga_variable();
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "tdc_isd_fpga_write_reset", "WR",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_write_enable_rf_tx(void)
{
    int        writingValue;
    int        bitReverse;
    static int error_cnt = 0;

    bitReverse   = (~(1 << FPGA_BitPosition_TxEnable));
    writingValue = bitReverse & fpag_lastWrittenRegister.systemResgister_1st_value;
    writingValue = writingValue | (0x01 << FPGA_BitPosition_TxEnable);

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_write(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
#else
    if (tdc_hal_i2c_cfx_write(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
#endif
    {
        fpag_lastWrittenRegister.systemResgister_1st_value = writingValue;
        error_cnt                                          = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "write_FPGA_enRF", "WR",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_is_rf_tx_enable(void)
{
    int        readValue;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_read(i2cAddr_FPGA_systemResgister_1st, &readValue, 1))
#else
    if (tdc_hal_i2c_cfx_read(i2cAddr_FPGA_systemResgister_1st, p_readValue, 1))
#endif
    {
        if ((readValue >> FPGA_BitPosition_TxEnable) & 0x1 == 1)
        {
            error_cnt = 0;
            return true;
        }
        else
        {
            error_cnt = 0;
            return false;
        }
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "tdc_isd_fpga_is_rf_tx_enable", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_write_disable_rf_tx(void)
{
    int        bitReverse;
    int        writingValue;
    static int error_cnt = 0;

    bitReverse   = (~(1 << FPGA_BitPosition_TxEnable));
    writingValue = bitReverse & fpag_lastWrittenRegister.systemResgister_1st_value;

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_write(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
#else
    if (tdc_hal_i2c_cfx_write(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
#endif
    {
        fpag_lastWrittenRegister.systemResgister_1st_value = writingValue;
        error_cnt                                          = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "write_FPGA_disRF", "WR",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool tdc_isd_fpga_write_change_tx_power_level(int txLevel)
{
    static int error_cnt = 0;

    if (tdc_drv_isl9122_write_register(TDC_DRV_ISL9122_REG_VOLTAGESET, txLevel))
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[PMIC]", "write_TxPowerLvl", "WR",
                              en__RF_PowerIC_ERROR, en__I2C_RFPOW_WritingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_Reset);  // I2C 쓰기 실패, RF_Power IC 초기화
        return false;
    }
}

bool tdc_isd_fpga_write_clear_fifo(void)
{
    int        writingValue;
    static int error_cnt = 0;

    writingValue = (1 << FPGA_BitPosition_Clear_FIFO) | (fpag_lastWrittenRegister.systemResgister_2nd_value);

#ifdef CM3_I2C_controls_FPAG
    if (tdc_hal_i2c_isd_write(i2cAddr_FPGA_systemResgister_2nd, &writingValue, 1))
#else
    if (tdc_hal_i2c_cfx_write(i2cAddr_FPGA_systemResgister_2nd, &writingValue, 1))
#endif
    {
        // 자동으로 지워지는 값이기 때문에 저장하지 않는다.
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "write_FPGA_clrFIFO", "WR",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError);
        tdc_isd_change_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

int tdc_isd_fpga_reset_8bit_backtel_config(int pcmIndex)
{
    int w_FPGA_registerValue;
    int pcmData;

    w_FPGA_registerValue = backterConfiguration_resetValue;
    pcmData              = w_FPGA_registerValue | pcm_Mold_BacktelConfiguration;  // PCM 몰드에 결합.
    tdc_shm_fill_specific_command_buffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int tdc_isd_fpga_change_8_bit_backtel_mode(int pcmIndex)
{
    int bitReverse;
    int w_FPGA_registerValue;
    int pcmData;
    int bitClearedRegister;

    // backtel bit 길이 모드
    bitReverse = (~(1 << pcm_BitPosition_FPGA_backtel_bitLength));

    // backtel on/off
    bitReverse = bitReverse & (~(1 << pcm_BitPosition_FPGA_backtel_OnOff));

    bitClearedRegister = bitReverse & fpag_lastWrittenRegister.backterConfiguration_value;  // 설정하고자하는 값이 0이기 때문에 해당 비트를 지운다.

    // 설정하고자 하는 값. ( 백텔 On, 8bit 모드)
    w_FPGA_registerValue = (1 << pcm_BitPosition_FPGA_backtel_OnOff) | (0 << pcm_BitPosition_FPGA_backtel_bitLength);  // 백텔 활성화 및 8bit 모드

    w_FPGA_registerValue = bitClearedRegister | w_FPGA_registerValue;

    // PCM 몰드에 결합.
    pcmData = w_FPGA_registerValue | pcm_Mold_BacktelConfiguration;

    tdc_shm_fill_specific_command_buffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int tdc_isd_fpga_change_12_bit_backtel_mode(int pcmIndex)
{
    int bitReverse;
    int w_FPGA_registerValue;
    int pcmData;
    int bitClearedRegister;

    // backtel bit 길이 모드
    bitReverse = (~(1 << pcm_BitPosition_FPGA_backtel_bitLength));

    // backtel on/off
    bitReverse = bitReverse & (~(1 << pcm_BitPosition_FPGA_backtel_OnOff));

    bitClearedRegister = bitReverse & fpag_lastWrittenRegister.backterConfiguration_value;  // 설정하고자하는 값이 0이기 때문에 해당 비트를 지운다.

    // 설정하고자 하는 값. ( 백텔 On, 12bit 모드)
    w_FPGA_registerValue = (1 << pcm_BitPosition_FPGA_backtel_OnOff) | (1 << pcm_BitPosition_FPGA_backtel_bitLength);  // 백텔 활성화 및 12bit 모드

    w_FPGA_registerValue = bitClearedRegister | w_FPGA_registerValue;

    // PCM 몰드에 결합.
    pcmData = w_FPGA_registerValue | pcm_Mold_BacktelConfiguration;

    tdc_shm_fill_specific_command_buffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int tdc_isd_fpga_disable_backtel(int pcmIndex)
{
    int bitReverse;
    int w_FPGA_registerValue;
    int pcmData;
    int bitClearedRegister;

    // backtel on/off
    bitReverse = (~(1 << pcm_BitPosition_FPGA_backtel_OnOff));

    bitClearedRegister = bitReverse & fpag_lastWrittenRegister.backterConfiguration_value;  // 설정하고자하는 값이 0이기 때문에 해당 비트를 지운다.

    // 설정하고자 하는 값. ( 백텔 On, 12bit 모드)
    w_FPGA_registerValue = (0 << pcm_BitPosition_FPGA_backtel_OnOff);  // 백텔 비활성화

    w_FPGA_registerValue = bitClearedRegister | w_FPGA_registerValue;

    // PCM 몰드에 결합.
    pcmData = w_FPGA_registerValue | pcm_Mold_BacktelConfiguration;

    tdc_shm_fill_specific_command_buffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

void tdc_isd_fpga_change_pulse_width(int pcmIndex, int pulseWidth)
{
    int pmcData;
    int pulsWithValue;

    if ((pulseWidth - FPGA_pulsePhaseWidth_minimum) >= 0)
    {
        pulsWithValue = pulseWidth - FPGA_pulsePhaseWidth_minimum;
    }
    else
    {
        pulsWithValue = 0;
    }

    pmcData = (pulsWithValue | pcm_Mold_PulsePhaseWidth);

    tdc_shm_fill_specific_command_buffer(pcmIndex, pmcData);
}

void tdc_isd_fpga_change_pulse_width_minimum(int pcmIndex)
{
    int pmcData;
    pmcData = ((0) | pcm_Mold_PulsePhaseWidth);

    tdc_shm_fill_specific_command_buffer(pcmIndex, pmcData);
}

void tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(int value)
{
    fpag_lastWrittenRegister.stimulation_PhaseDuration_value=value-FPGA_pulsePhaseWidth_minimum;
}

void tdc_isd_fpga_update_fpga_backtel_config_written_value(int value)
{
    fpag_lastWrittenRegister.backterConfiguration_value=value;
    tdc_shm_update_backtel_control_value_to_cfx(fpag_lastWrittenRegister.backterConfiguration_value);
}


