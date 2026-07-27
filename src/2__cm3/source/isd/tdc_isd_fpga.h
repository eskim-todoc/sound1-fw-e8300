#ifndef __tdc_isd_fpga_h__
#define __tdc_isd_fpga_h__

#include <stdbool.h>
#include <FPGA.h>
#include <internalStimulationChip.h>

// FPGA/ISD 통신·검증 연속 실패 디바운스 임계.
// 연속 실패가 이 값을 "초과"할 때만 tdc_sys_error_update(에러 확정 → LED)를 호출하고,
// 그 전까지는 WARN 로그만 남긴다. 성공 1회면 각 지점 카운터를 0으로 리셋한다.
// 부팅 초반 과도기의 일시적 통신 실패를 흡수해 빨간 LED 점멸을 막기 위함.
#define TDC_ISD_ERR_DEBOUNCE_N (15)

// 연속 실패 디바운스 헬퍼.
//   _cnt : 호출부 static 카운터(lvalue)   _mod : "[FPGA]" / "[PMIC]"
//   _tag : 지점 라벨                       _op  : "RD" / "WR" / "verify"
//   _maj,_det : tdc_sys_error_update 인자(major / detail)
#define TDC_ISD_DEBOUNCE_FAIL(_cnt, _mod, _tag, _op, _maj, _det)                            \
    do                                                                                      \
    {                                                                                       \
        if (++(_cnt) > TDC_ISD_ERR_DEBOUNCE_N)                                              \
        {                                                                                   \
            TDC_PRINTF_E(_mod " FAIL  %-22s %-6s cnt %2d  >> ERR set\r\n", _tag, _op, (_cnt)); \
            tdc_sys_error_update((_maj), (_det), __LINE__);                                      \
        }                                                                                   \
        else                                                                                \
        {                                                                                   \
            TDC_PRINTF_W(_mod " WARN  %-22s %-6s cnt %2d/%2d\r\n", _tag, _op, (_cnt),           \
                      TDC_ISD_ERR_DEBOUNCE_N);                                               \
        }                                                                                   \
    } while (0)

void tdc_isd_fpga_reset_fpga_variable(void);
#if 0
#endif

bool tdc_isd_fpga_read_version(int* p_readValue);
bool tdc_isd_fpga_read_systemregister_1st(int* p_readValue);
bool tdc_isd_fpga_read_tx_power_level(int* p_readValue);
bool tdc_isd_fpga_check_fpga_pcm_error(bool* isError);

bool tdc_isd_fpga_check_fpga_fifo_empty(bool* isEmpty);

bool              tdc_isd_fpga_read_system_error_flag(int* p_readValue);
bool              tdc_isd_fpga_read_backtel_error_flag(int* p_readValue);
bool              tdc_isd_fpga_read_pulse_width(int* p_readValue);
bool              tdc_isd_fpga_read_fifo_counter(int* p_readValue);
bool              tdc_isd_fpga_read_backtel_config(int* p_readValue);
bool              tdc_isd_fpga_read_backtel_fifo(int* p_readValue, int counter);
bool              tdc_isd_fpga_is_arbitrary_value_matched_normal_value(void);
bool              tdc_isd_fpga_is_arbitrary_value_matched_duplicate_zero_data(void);
bool              tdc_isd_fpga_is_rf_tx_enable(void);
EN_ISD_PowerState tdc_isd_fpga_read_isd_power_state(void);

bool tdc_isd_fpga_write_reset(void);
bool tdc_isd_fpga_write_enable_rf_tx(void);
bool tdc_isd_fpga_write_disable_rf_tx(void);
bool tdc_isd_fpga_write_change_tx_power_level(int txLevel);
bool tdc_isd_fpga_write_clear_fifo(void);
int  tdc_isd_fpga_reset_8bit_backtel_config(int pcmIndex);
int  tdc_isd_fpga_change_8_bit_backtel_mode(int pcmIndex);
int  tdc_isd_fpga_change_12_bit_backtel_mode(int pcmIndex);
int  tdc_isd_fpga_disable_backtel(int pcmIndex);

void tdc_isd_fpga_change_pulse_width(int pcmIndex, int pulseWidth);
void tdc_isd_fpga_change_pulse_width_minimum(int pcmIndex);

void tdc_isd_fpga_update_fpga_pulse_phase_width_written_value(int Value);
void tdc_isd_fpga_update_fpga_backtel_config_written_value(int value);

#endif
