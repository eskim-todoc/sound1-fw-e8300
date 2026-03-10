#ifndef ISD_INTERFACE_FPGA_H__
#define ISD_INTERFACE_FPGA_H__

#include <stdbool.h>
#include "FPGA.h"
#include "internalStimulationChip.h"

void reset_Fpga_variable(void);
#if 0
    int get_last_fpga_written_Value(I2C_ADDR_FPGA index);
    void update_last_fpga_written_Value(I2C_ADDR_FPGA index, int value);
#endif

int get_last_fpga_systemResgister_1st_written_Value(void);
int get_last_fpga_systemResgister_2nd_written_Value(void);
int get_last_fpga_pulsePhaseWidth_written_Value(void);
int get_last_fpga_backterConfiguration_written_Value(void);

bool read_FPGA_version(int* p_readValue);
bool read_FPGA_systemResgister_1st(int* p_readValue);
bool read_txPowerLevel(int* p_readValue);
bool check_FPGA_PCM_Error(bool* isError);
bool read_FPGA_systemResgister_2nd(int* p_readValue);

bool check_FPGA_FIFO_empty(bool* isEmpty);

bool              read_FPGA_systemError_Flag(int* p_readValue);
bool              read_FPGA_backtelError_Flag(int* p_readValue);
bool              read_FPGA_PulseWidth(int* p_readValue);
bool              read_FPGA_FIFO_counter(int* p_readValue);
bool              read_FPGA_IO_MUX(int* p_readValue);
bool              read_FPGA_backtelConfig(int* p_readValue);
bool              read_FPGA_optionalConfig(int* p_readValue);
bool              read_FPGA_backtel_FIFO(int* p_readValue, int counter);
bool              is_arbitraryValue_matched_normalValue(void);
bool              is_arbitraryValue_matched_duplicateZeroData(void);
bool              is_RF_tx_eanble(void);
EN_ISD_PowerState read_isd_Power_State(void);

bool write_FPGA_systemResgister_1st(int Value);
bool write_FPGA_reset(void);
bool write_FPGA_enable_RF_tx(void);
bool write_FPGA_disable_RF_tx(void);
bool write_change_TxPowerLevel(int txLevel);
bool write_FPGA_clear_FIFO(void);
int  Reset_8bitBacktelConfig(int pcmIndex);
int  change_8BitBacktel_mode(int pcmIndex);
int  change_12BitBacktel_mode(int pcmIndex);
int  get_last_fpga_backterConfiguration_written_Value(void);
int  change_Backtel_cal(int pcmIndex, int CalValue);
int  disable_Backtel(int pcmIndex);

void chang_PulseWidth(int pcmIndex, int pulseWidth);
void chang_PulseWidth_minimum(int pcmIndex);

bool write_FPGA_systemResgister_2nd(int Value);
bool write_FPGA_backtelConfig(int Value);

void upadte_fpga_pulsePhaseWidth_written_Value(int Value);
void upadte_fpga_backtelConfig_written_Value(int value);

#endif
