/*
 * ci_system_mode.h
 */

#ifndef __ci_stim_mute_h__
#define __ci_stim_mute_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <FPGA.h>
#include <cfx_cm3_sharedMemory.h>
#include <definitionsForAlgorithm.h>

#include <ci_filesystem.h>

#define CI_STIM_MUTE_FILE_NAME "STIM_MUTE.TXT"

#define CI_STIM_MUTE_FILE_IDNET_BEGIN 0x12345678
#define CI_STIM_MUTE_FILE_IDENT_END   0x87654321

#define CI_STIM_MUTE_UNDER_T_LEVEL_ENABLE  1
#define CI_STIM_MUTE_UNDER_T_LEVEL_DISABLE 2

#define CI_STIM_MUTE_T_LEVEL_OFFSET_DEFAULT 2
#define CI_STIM_MUTE_T_LEVEL_OFFSET_MIN     0
#define CI_STIM_MUTE_T_LEVEL_OFFSET_MAX     255

#define CI_STIM_MUTE_RET_TRUE  0
#define CI_STIM_MUTE_RET_FALSE (-1)
typedef struct
{
    uint32_t file_ident_begin;
    uint32_t is_enabled_mute_stimulation_under_t_level;
    uint32_t mute_stimulation_t_level_offset;
    uint32_t file_ident_end;
} CI_STIM_MUTE_T;

int ci_stim_mute_init(void);
int ci_stim_mute_update(uint32_t enable, uint32_t level);

#endif /* __ci_stim_mute_h__ */
