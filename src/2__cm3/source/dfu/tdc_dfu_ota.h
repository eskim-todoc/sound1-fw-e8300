/**
 * @file ota.h
 */

#ifndef __tdc_dfu_ota_h__
#define __tdc_dfu_ota_h__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hw.h>

#include <tdc_fs.h>
#include <SEGGER_RTT_Wrapper.h>

#define CI_OTA_FILE_PATH_LEN_MAX 25

#define CI_OTA_FILE_NAME_MANIFEST ((const char *) "MANIFEST.TXT")
#define CI_OTA_FILE_NAME_APP000   ((const char *) "APP000.FEZ")
#define CI_OTA_FILE_NAME_APP001   ((const char *) "APP001.FEZ")
#define CI_OTA_FILE_NAME_APP002   ((const char *) "APP002.FEZ")

typedef enum
{
    CI_OTA_FILE_TYPE_MANIFEST = 1,
    CI_OTA_FILE_TYPE_APP000,
    CI_OTA_FILE_TYPE_APP001,
    CI_OTA_FILE_TYPE_APP002
} CI_OTA_FILE_TYPE_E;

typedef enum
{
    CI_OTA_RET_SUCCESS                 = 1,
    CI_OTA_RET_FAIL                    = 2,
    CI_OTA_RET_ERROR_INVALID_FILE_TYPE = 3,
    CI_OTA_RET_ERROR_FILE_OPEN_FAIL    = 4,
    CI_OTA_RET_ERROR_FILE_SEEK         = 5,
    CI_OTA_RET_ERROR_FILE_WRITE        = 6
} CI_OTA_RET_E;

typedef struct
{
    uint32_t slot;
    uint32_t file_type;
    uint32_t total_file_size_in_bytes;
    uint32_t rw;
} CI_OTA_PREPARE_FILE_T;

void tdc_dfu_ota_command_parsing(void);

#endif  // __tdc_dfu_ota_h__
