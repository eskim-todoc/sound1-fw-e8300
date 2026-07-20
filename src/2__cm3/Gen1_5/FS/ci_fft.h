/**
 * @file ci_fft.h
 */

#ifndef __ci_fft_h__
#define __ci_fft_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>
#include <cfx_cm3_sharedMemory.h>
#include <definitionsForAlgorithm.h>

#include <ci_filesystem.h>
#include <tdc_printf.h>

#define CI_FFT_FILE_INIT_NAME_PASS_BIN                              \
    {                                                               \
        '/', 'P', 'A', 'S', 'S', '_', 'B', 'I', 'N', '*', '*', '\0' \
    }

#define CI_FFT_FILE_NAME_LEN_PASS_BIN 12

#define CI_FFT_FILE_INDEX_PASS_BIN_NUM_FIRST  9
#define CI_FFT_FILE_INDEX_PASS_BIN_NUM_SECOND 10

#define CI_FFT_FILE_INIT_NAME_WINDOW_COEFF                     \
    {                                                          \
        '/', 'W', 'N', 'D', '_', 'C', 'O', 'E', 'F', 'F', '\0' \
    }

#define CI_FFT_FILE_NAME_LEN_WINDOW_COEFF 11

int ci_fft_read_pass_bin(int num_of_freq_band);

int ci_fft_init_pass_bin_all(void);
int ci_fft_init_pass_bin(int num_of_freq_band);

int ci_fft_init_pass_bin_ch_1(char *p_name);
int ci_fft_init_pass_bin_ch_2(char *p_name);
int ci_fft_init_pass_bin_ch_3(char *p_name);
int ci_fft_init_pass_bin_ch_4(char *p_name);
int ci_fft_init_pass_bin_ch_5(char *p_name);
int ci_fft_init_pass_bin_ch_6(char *p_name);
int ci_fft_init_pass_bin_ch_7(char *p_name);
int ci_fft_init_pass_bin_ch_8(char *p_name);
int ci_fft_init_pass_bin_ch_9(char *p_name);
int ci_fft_init_pass_bin_ch_10(char *p_name);
int ci_fft_init_pass_bin_ch_11(char *p_name);
int ci_fft_init_pass_bin_ch_12(char *p_name);
int ci_fft_init_pass_bin_ch_13(char *p_name);
int ci_fft_init_pass_bin_ch_14(char *p_name);
int ci_fft_init_pass_bin_ch_15(char *p_name);
int ci_fft_init_pass_bin_ch_16(char *p_name);
int ci_fft_init_pass_bin_ch_17(char *p_name);
int ci_fft_init_pass_bin_ch_18(char *p_name);
int ci_fft_init_pass_bin_ch_19(char *p_name);
int ci_fft_init_pass_bin_ch_20(char *p_name);
int ci_fft_init_pass_bin_ch_21(char *p_name);
int ci_fft_init_pass_bin_ch_22(char *p_name);
int ci_fft_init_pass_bin_ch_23(char *p_name);
int ci_fft_init_pass_bin_ch_24(char *p_name);
int ci_fft_init_pass_bin_ch_25(char *p_name);
int ci_fft_init_pass_bin_ch_26(char *p_name);
int ci_fft_init_pass_bin_ch_27(char *p_name);
int ci_fft_init_pass_bin_ch_28(char *p_name);
int ci_fft_init_pass_bin_ch_29(char *p_name);
int ci_fft_init_pass_bin_ch_30(char *p_name);
int ci_fft_init_pass_bin_ch_31(char *p_name);
int ci_fft_init_pass_bin_ch_32(char *p_name);

int ci_fft_init_window_coeff(void);
int ci_fft_read_window_coeff(void);
int ci_fft_write_window_coeff(void);

#endif // __ci_fft_h__
