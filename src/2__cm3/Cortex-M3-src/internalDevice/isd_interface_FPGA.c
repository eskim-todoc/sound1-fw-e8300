#include <isdExecution/driver_PCM.h>
#include "isd_interface_FPGA.h"
#include "isd_interface.h"
#include "cfx_cm3_sharedMemory.h"
#include "isd_interface_FPGA.h"
#include "error.h"
#include "FPGA.h"

#ifdef CM3_I2C_controls_FPAG
#include "driver_i2c_for_ISD.h"
#else
#include "driver_cfx_i2c.h"
#endif

#if defined(Board_is_OTE_VER_1_2)
#include "driver_REN_ISL91128.h"
#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
#include "driver_REN_ISL9122.h"
#elif defined(Board_is_OTE_VER_1_3)
#include "driver_REN_ISL98608.h"
#else
#error Link PMIC is NOT selected.
#endif

#include "commonDataProcessing.h"
#include <ci_printf.h>

static LAST_WRITTEN_REGISTER fpag_lastWrittenRegister;

void reset_Fpga_variable(void)
{
    fpag_lastWrittenRegister.systemResgister_1st_value       = systemResgister_1st_resetValue;
    fpag_lastWrittenRegister.systemResgister_2nd_value       = systemResgister_2nd_resetValue;
    fpag_lastWrittenRegister.stimulation_PhaseDuration_value = stimulation_PhaseDuration_resetValue;
    // fpag_lastWrittenRegister.fpga_IO_MUX_Configuration_value=fpga_IO_MUX_Configuration_resetValue;
    fpag_lastWrittenRegister.backterConfiguration_value = backterConfiguration_resetValue;
    // fpag_lastWrittenRegister.fpga_optional_configuration_value=fpga_IO_MUX_Configuration_resetValue;

    updagteBacktelControlValue_toCFX(fpag_lastWrittenRegister.backterConfiguration_value);
}

#if 0
int get_last_fpga_written_Value(I2C_ADDR_FPGA index)
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



void update_last_fpga_written_Value(I2C_ADDR_FPGA index, int value)
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

int get_last_fpga_systemResgister_1st_written_Value(void)
{
    return fpag_lastWrittenRegister.systemResgister_1st_value;
}

int get_last_fpga_systemResgister_2nd_written_Value(void)
{
    return fpag_lastWrittenRegister.systemResgister_2nd_value;
}

int get_last_fpga_pulsePhaseWidth_written_Value(void)
{
    return fpag_lastWrittenRegister.stimulation_PhaseDuration_value;
}

int get_last_fpga_backterConfiguration_written_Value(void)
{
    return fpag_lastWrittenRegister.backterConfiguration_value;
}

bool read_FPGA_version(int *p_readValue)
{
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_version, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_version, p_readValue, 1))
#endif
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_version", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_systemResgister_1st(int *p_readValue)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_systemResgister_1st, p_readValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_systemResgister_2nd(int *p_readValue)
{
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_2nd, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_systemResgister_2nd, p_readValue, 1))
#endif
    {
        return true;
    }
    else
    {
        // I2C 읽기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool check_FPGA_PCM_Error(bool *isError)
{
    int        readValue;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_error_Flag, &readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_systemResgister_1st, &readValue, 1))
#endif
    {
        if ((readValue & 0x1F) != 0)
        {
            *isError = true;
            update_FPGA_systemError(readValue);
            ci_printe("[FPGA] ERROR OCCURRED, SYS_ERR_CHK : 0x%02X \r\n", readValue);
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
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "check_FPGA_PCM_Error", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool check_FPGA_FIFO_empty(bool *isEmpty)
{
    int        readValue;
    int        temp;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_2nd, &readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_systemResgister_2nd, p_readValue, 1))
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
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "check_FPGA_FIFO_empty", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_systemError_Flag(int *p_readValue)
{
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_error_Flag, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_error_Flag, p_readValue, 1))
#endif
    {
        return true;
    }
    else
    {
        // I2C 읽기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_backtelError_Flag(int *p_readValue)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_backtel_ErrorFlag, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_backtel_ErrorFlag, p_readValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_PulseWidth(int *pulseWidth)
{
    int        readValue;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_pulsePhaseWidth, &readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_pulsePhaseWidth, p_readValue, 1))
#endif
    {
        *pulseWidth = readValue + FPGA_pulsePhaseWidth_minimum;  // FPGA에 설정된 값에 기본 오프셋 값이 더해진게 실제 펄스 폭이 된다.
        error_cnt   = 0;
        return true;
    }
    else
    {
        // I2C 읽기 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "read_FPGA_PulseWidth", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_FIFO_counter(int *counterFIFO)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_FIFO_counter, counterFIFO, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_FIFO_counter, p_readValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_IO_MUX(int *p_readValue)
{
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FGPA_IO_MUX, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FGPA_IO_MUX, p_readValue, 1))
#endif
    {
        return true;
    }
    else
    {
        // I2C 읽기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_backtelConfig(int *p_readValue)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_backtel_Config, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_backtel_Config, p_readValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_optionalConfig(int *p_readValue)
{
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_optional_Config, p_readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_optional_Config, p_readValue, 1))
#endif
    {
        return true;
    }
    else
    {
        // I2C 읽기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool read_FPGA_backtel_FIFO(int *p_readValue, int counter)
{
    static int error_cnt = 0;
#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_Backtel_FIFO, p_readValue, counter))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_Backtel_FIFO, p_readValue, counter))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

#if defined(Board_is_OTE_VER_1_2)
bool read_txPowerLevel(int *p_readValue)
{
    int readValue;

    if (read_REN_ISL91128_register_byCM3_I2C(REN_ISL91128_registerAddr_voltageControl, &readValue))
    {
        *p_readValue = (readValue & 0x3F);
        return true;
    }
    else
    {
        // I2C 읽기 실패, RF_Power IC 초기화
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__I2C_RFPOW_ReadingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_Reset);
        return false;
    }
}

#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)

bool read_txPowerLevel(int *p_readValue)
{
    int        readValue;
    static int error_cnt = 0;

    if (read_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_VoltageSet, &readValue))
    {
        *p_readValue = (readValue);
        error_cnt    = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[PMIC]", "read_txPowerLevel", "RD",
                              en__RF_PowerIC_ERROR, en__I2C_RFPOW_ReadingError);

        change_isd_state(en__isdStatus_PowerIC_Reset);

        return false;
    }
}
#elif defined(Board_is_OTE_VER_1_3)

#if defined(Error_ISL98608_ReadByte)

int txLevel_kkk = 0;

#endif

bool read_txPowerLevel(int *p_readValue)
{
    int readValue;

#if defined(Error_ISL98608_ReadByte)

    *p_readValue = txLevel_kkk;
    return true;

#else

    if (read_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_VP_Voltage, &readValue))
    {
        *p_readValue = (readValue);
        return true;
    }
    else
    {
        // I2C 읽기 실패, RF_Power IC 초기화
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__I2C_RFPOW_ReadingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_Reset);
        return false;
    }

#endif
}

#endif

bool is_arbitraryValue_matched_normalValue(void)
{
    int readValue;
    int tempA, tempB, tempC;

    read_FPGA_backtel_FIFO(&readValue, 1);

    if (readValue == df_forwardPathCheck_arbitraryValue)
    {
        return true;
    }
    else
    {

        return false;
    }
}

bool is_arbitraryValue_matched_duplicateZeroData(void)
{
    int readValue;
    int tempA, tempB, tempC;

    read_FPGA_backtel_FIFO(&readValue, 1);

    if (readValue == df_duplicateZeroValue)
    {
        return true;
    }
    else
    {
        return false;
    }
}

EN_ISD_PowerState read_isd_Power_State(void)
{
    int readValue;
    int tempA, tempB, tempC;

    read_FPGA_FIFO_counter(&readValue);

    if (readValue == 0)
    {
        return NoBacktel;
    }
    if (readValue == 1)
    {
        read_FPGA_backtel_FIFO(&readValue, 1);
        return data_ExtractionAndRigthShift(readValue, bitPosition_isd_PowerState, bitLength_isd_PowerState);
    }
    else
    {
        return BackTelNumTooMuch;
    }
}

// write FPGA

bool write_FPGA_systemResgister_1st(int value)
{
#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &value, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_systemResgister_1st, &value, 1))
#endif
    {
        value                                              = value & systemResgister_1st_WRITABLE_BIT;
        fpag_lastWrittenRegister.systemResgister_1st_value = value;
        return true;
    }
    else
    {
        // I2C 쓰기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool write_FPGA_systemResgister_2nd(int value)
{
#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_2nd, &value, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_systemResgister_2nd, &value, 1))
#endif
    {
        value                                              = value & systemResgister_2nd_WRITABLE_BIT;
        fpag_lastWrittenRegister.systemResgister_2nd_value = value;

        return true;
    }
    else
    {
        // I2C 쓰기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}
bool write_FPGA_backtelConfig(int value)
{
#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_pulsePhaseWidth, &value, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_pulsePhaseWidth, &value, 1))
#endif
    {
        value                                                    = value & backterConfiguration_WRITABLE_BIT;
        fpag_lastWrittenRegister.stimulation_PhaseDuration_value = value;
        return true;
    }
    else
    {
        // I2C 쓰기 실패, FPGA 초기화
        errorCodeUpdate(en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError, __LINE__);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

// 디테일
//////////////////

bool write_FPGA_reset(void)
{
    int        value;
    static int error_cnt = 0;

    value = 1 << FPGA_BitPosition_ResetFPGA;

#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &value, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_systemResgister_1st, &value, 1))
#endif
    {
        reset_Fpga_variable();
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "write_FPGA_reset", "WR",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_WritingError);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool write_FPGA_enable_RF_tx(void)
{
    int        writingValue;
    int        bitReverse;
    static int error_cnt = 0;

    bitReverse   = (~(1 << FPGA_BitPosition_TxEnable));
    writingValue = bitReverse & fpag_lastWrittenRegister.systemResgister_1st_value;
    writingValue = writingValue | (0x01 << FPGA_BitPosition_TxEnable);

#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool is_RF_tx_eanble(void)
{
    int        readValue;
    static int error_cnt = 0;

#ifdef CM3_I2C_controls_FPAG
    if (read_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &readValue, 1))
#else
    if (cfx_i2c_read(i2cAddr_FPGA_systemResgister_1st, p_readValue, 1))
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
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[FPGA]", "is_RF_tx_eanble", "RD",
                              en__FPGA_COMMUNICATION_ERROR, en__I2C_FPGA_ReadingError);
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

bool write_FPGA_disable_RF_tx(void)
{
    int        bitReverse;
    int        writingValue;
    static int error_cnt = 0;

    bitReverse   = (~(1 << FPGA_BitPosition_TxEnable));
    writingValue = bitReverse & fpag_lastWrittenRegister.systemResgister_1st_value;

#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_systemResgister_1st, &writingValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

#if defined(Board_is_OTE_VER_1_2)
bool write_change_TxPowerLevel(int txLevel)
{
    int value;
    int readValue;

    // i2C로 설정한 DCDC값을 활성화한다.
    value = 1 << en__enalbe_I2C_Control_BitPosition;

    // value=value|(0x3F<<en__voltageControl_BitPosition);
    value = value | (txLevel << en__voltageControl_BitPosition);

    if (write_REN_ISL91128_register_byCM3_I2C(REN_ISL91128_registerAddr_voltageControl, value))
    {
        return true;
    }
    else
    {
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__I2C_RFPOW_WritingError, __LINE__);

        // I2C 쓰기 실패, RF_Power IC 초기화
        change_isd_state(en__isdStatus_PowerIC_Reset);

        return false;
    }
}
#elif defined(Board_is_TD_DEV_ver_1_4) || defined(Board_is_OTE_VER_1_4) || defined(Board_is_OTE_VER_1_5)
bool write_change_TxPowerLevel(int txLevel)
{
    static int error_cnt = 0;

    if (write_REN_ISL9122_register_byCM3_I2C(REN_ISL9122_registerAddr_VoltageSet, txLevel))
    {
        error_cnt = 0;
        return true;
    }
    else
    {
        // I2C 실패 - 연속 실패 디바운스
        TDC_ISD_DEBOUNCE_FAIL(error_cnt, "[PMIC]", "write_TxPowerLvl", "WR",
                              en__RF_PowerIC_ERROR, en__I2C_RFPOW_WritingError);
        change_isd_state(en__isdStatus_PowerIC_Reset);  // I2C 쓰기 실패, RF_Power IC 초기화
        return false;
    }
}
#elif defined(Board_is_OTE_VER_1_3)

bool write_change_TxPowerLevel(int txLevel)
{

    if (write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_VBST_Voltage, txLevel + 6))
    {

        if (write_REN_ISL98608_register_byCM3_I2C(REN_ISL98608_registerAddr_VP_Voltage, txLevel))
        {

#if defined(Error_ISL98608_ReadByte)

            txLevel_kkk = txLevel;
#endif

            return true;
        }
        else
        {
            errorCodeUpdate(en__RF_PowerIC_ERROR, en__I2C_RFPOW_WritingError, __LINE__);

            // I2C 쓰기 실패, RF_Power IC 초기화
            change_isd_state(en__isdStatus_PowerIC_Reset);

            return false;
        }
    }
    else
    {
        errorCodeUpdate(en__RF_PowerIC_ERROR, en__I2C_RFPOW_WritingError, __LINE__);

        // I2C 쓰기 실패, RF_Power IC 초기화
        change_isd_state(en__isdStatus_PowerIC_Reset);

        return false;
    }
}
#endif

bool write_FPGA_clear_FIFO(void)
{
    int        writingValue;
    static int error_cnt = 0;

    writingValue = (1 << FPGA_BitPosition_Clear_FIFO) | (fpag_lastWrittenRegister.systemResgister_2nd_value);

#ifdef CM3_I2C_controls_FPAG
    if (write_ISD_by_CM3_I2C(i2cAddr_FPGA_systemResgister_2nd, &writingValue, 1))
#else
    if (cfx_i2c_write(i2cAddr_FPGA_systemResgister_2nd, &writingValue, 1))
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
        change_isd_state(en__isdStatus_PowerIC_OK);
        return false;
    }
}

int Reset_8bitBacktelConfig(int pcmIndex)
{
    int w_FPGA_registerValue;
    int pcmData;

    w_FPGA_registerValue = backterConfiguration_resetValue;
    pcmData              = w_FPGA_registerValue | pcm_Mold_BacktelConfiguration;  // PCM 몰드에 결합.
    fillSepcificCommndBuffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int change_8BitBacktel_mode(int pcmIndex)
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

    fillSepcificCommndBuffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int change_12BitBacktel_mode(int pcmIndex)
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

    fillSepcificCommndBuffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int disable_Backtel(int pcmIndex)
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

    fillSepcificCommndBuffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

int change_Backtel_cal(int pcmIndex, int CalValue)
{
    int bitReverse;
    int w_FPGA_registerValue;
    int previousCalValue;
    int calValue;
    int pcmData;
    int bitClearedRegister;

    // backtel bit 길이 모드
    bitReverse = (~(0x1F << pcm_BitPosition_FPGA_backtel_calibration));

    bitClearedRegister = bitReverse & fpag_lastWrittenRegister.backterConfiguration_value;  // 설정하고자하는 값이 0이기 때문에 해당 비트를 지운다.

    // 설정하고자 하는 값. ( calibration value)
    w_FPGA_registerValue = bitClearedRegister | (CalValue << pcm_BitPosition_FPGA_backtel_calibration);  // 백텔 활성화 및 8bit 모드

    // PCM 몰드에 결합.
    pcmData = w_FPGA_registerValue | pcm_Mold_BacktelConfiguration;

    fillSepcificCommndBuffer(pcmIndex, pcmData);

    return w_FPGA_registerValue;
}

void chang_PulseWidth(int pcmIndex, int pulseWidth)
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

    fillSepcificCommndBuffer(pcmIndex, pmcData);
}

void chang_PulseWidth_minimum(int pcmIndex)
{
    int pmcData;
    pmcData = ((0) | pcm_Mold_PulsePhaseWidth);

    fillSepcificCommndBuffer(pcmIndex, pmcData);
}

void upadte_fpga_pulsePhaseWidth_written_Value(int value)
{
    fpag_lastWrittenRegister.stimulation_PhaseDuration_value=value-FPGA_pulsePhaseWidth_minimum;
}

void upadte_fpga_backtelConfig_written_Value(int value)
{
    fpag_lastWrittenRegister.backterConfiguration_value=value;
    updagteBacktelControlValue_toCFX(fpag_lastWrittenRegister.backterConfiguration_value);
}


