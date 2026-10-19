/* SYS-077: 실제 ISD 정보 저장과 같은 크기/CRC/AES 경로를 시험 파일로 실행.
 * 실제 사용자의 이름/제조번호/비밀번호/키를 읽거나 출력하지 않는다. */
#include "tdc_sys_test_77.h"
#if TDC_SYS_TEST_77_ENABLE
#include "tdc_sys_test_storage.h"
#include "ci_filesystem.h"

static ST__CFX_CM3_SharedMemory_ISD_info s_tdc77_plain;
static ST__CFX_CM3_SharedMemory_ISD_info s_tdc77_decoded;
static uint8_t s_tdc77_cipher[144];
static FIL s_tdc77_file; /* FatFs 객체는 섹터 버퍼를 포함할 수 있어 스택에 두지 않는다. */
static bool s_tdc77_requires_reboot;
_Static_assert(sizeof(ST__CFX_CM3_SharedMemory_ISD_info) == 132, "SYS77 storage size changed");

bool tdc_sys_test_77_requires_reboot(void) { return s_tdc77_requires_reboot; }

static int tdc77_stop(const char *stage)
{
    s_tdc77_requires_reboot = true;
    tdc_sys_storage_log("FS_STOP=%s; REBOOT_REQUIRED", stage);
    tdc_sys_storage_log("SCRATCH=RETAINED; NO_MORE_FS");
    tdc_sys_storage_log("FILE_ROUNDTRIP=NOT_COMPLETE");
    tdc_sys_storage_log("POWER_CYCLE_PERSISTENCE=NOT_RUN");
    return 0;
}

static bool tdc77_observed(const char *stage, const tdc_fs_test_observation_t *o, unsigned sync_count)
{
    if (o->sync_count) tdc_sys_storage_log("%s_SYNC N=%u RES=%d", stage, o->sync_count, (int)o->sync_result);
    else tdc_sys_storage_log("%s_SYNC=NOT_RUN", stage);
    if (o->close_count) tdc_sys_storage_log("%s_CLOSE N=%u RES=%d", stage, o->close_count, (int)o->close_result);
    else tdc_sys_storage_log("%s_CLOSE=NOT_RUN", stage);
    return o->sync_count == sync_count && o->close_count == 1u &&
           o->sync_result == FR_OK && o->close_result == FR_OK;
}

int tdc_sys_test_77_execute(int argc, char *argv[])
{
    if (argc != 2 || argv == NULL || argv[1] == NULL || strcmp(argv[1], "run") != 0) return -1;
    if (s_tdc77_requires_reboot)
    {
        tdc_sys_storage_log("REJECT=FS_UNCERTAIN; REBOOT_REQUIRED");
        return 0;
    }
    char path[] = "1:/SYS77.TST";
    FIL *file = &s_tdc77_file;
    /* CREATE_NEW로 다른 파일을 덮어쓰지 않는다. 성공한 이번 명령만 삭제 권한을 갖는다. */
    FRESULT fr = f_open(file, path, FA_CREATE_NEW | FA_WRITE);
    if (fr != FR_OK)
    {
        tdc_sys_storage_log("CREATE=%d; existing file is not overwritten", (int)fr);
        return 0;
    }
    fr = f_sync(file);
    tdc_sys_storage_log("CREATE_SYNC=%d EXPECT=0", (int)fr);
    if (fr != FR_OK) return tdc77_stop("CREATE_SYNC");
    fr = f_close(file);
    if (fr != FR_OK)
    {
        tdc_sys_storage_log("CLOSE=%d; test file retained", (int)fr);
        return tdc77_stop("CREATE_CLOSE");
    }
    memset(&s_tdc77_plain, 0, sizeof(s_tdc77_plain));
    memset(&s_tdc77_decoded, 0, sizeof(s_tdc77_decoded));
    s_tdc77_plain.isd_year = 26;
    s_tdc77_plain.isd_month_model = 5;
    s_tdc77_plain.isd_serial = 7701;
    const char name[] = "SYS77_TEST";
    for (unsigned i = 0; i < sizeof(name) - 1u; ++i) s_tdc77_plain.isd_userName[i] = name[i];
    uint32_t stored_crc = 0, read_crc = 0;
    uint32_t padding[2] = {0};
    tdc_sys_storage_log("SOURCE=SYNTHETIC ISD_INFO; AES128-ECB");
    tdc_sys_storage_log("IN/OUT=data132 ENC=stored144 bytes");
    tdc_sys_storage_hex("IN", (const uint8_t *)&s_tdc77_plain, sizeof(s_tdc77_plain));
    tdc_fs_test_observation_t observed;
    if (!tdc_fs_test_observe_begin(&observed)) return tdc77_stop("OBS_BUSY");
    const int write_ret = ci_filesystem_write_with_crc_and_aes128(path, (uint8_t *)&s_tdc77_plain,
        sizeof(s_tdc77_plain), &stored_crc, padding, sizeof(padding), true, true);
    tdc_fs_test_observe_end();
    tdc_sys_storage_log("WRITE=%d EXPECT=0", write_ret);
    const bool write_observed = tdc77_observed("WR", &observed, 1u);
    if (write_ret != 0 || !write_observed) return tdc77_stop("WRAPPER_WRITE");

    unsigned count = 0;
    fr = f_open(file, path, FA_READ | FA_OPEN_EXISTING);
    tdc_sys_storage_log("RAW_OPEN=%d EXPECT=0", (int)fr);
    if (fr != FR_OK) return tdc77_stop("RAW_OPEN");
    fr = f_read(file, s_tdc77_cipher, sizeof(s_tdc77_cipher), &count);
    const FRESULT close_ret = f_close(file);
    tdc_sys_storage_log("RAW_READ=%d BYTES=%u CLOSE=%d", (int)fr, count, (int)close_ret);
    if (fr != FR_OK || count != sizeof(s_tdc77_cipher) || close_ret != FR_OK)
        return tdc77_stop("RAW_READ_CLOSE");
    tdc_sys_storage_hex("ENC", s_tdc77_cipher, sizeof(s_tdc77_cipher));
    if (!tdc_fs_test_observe_begin(&observed)) return tdc77_stop("OBS_BUSY");
    const int read_ret = ci_filesystem_read_with_crc_and_aes128(path, (uint8_t *)&s_tdc77_decoded,
        sizeof(s_tdc77_decoded), &read_crc, padding, sizeof(padding), true, true);
    tdc_fs_test_observe_end();
    tdc_sys_storage_log("READ_CRC=%d EXPECT=0", read_ret);
    const bool read_observed = tdc77_observed("RD", &observed, 0u);
    if (read_ret != 0 || !read_observed) return tdc77_stop("WRAPPER_READ");
    tdc_sys_storage_hex("OUT", (const uint8_t *)&s_tdc77_decoded, sizeof(s_tdc77_decoded));
    const bool equal = memcmp(&s_tdc77_plain, &s_tdc77_decoded, sizeof(s_tdc77_plain)) == 0;
    const bool changed = memcmp(&s_tdc77_plain, s_tdc77_cipher, sizeof(s_tdc77_plain)) != 0;
    tdc_sys_storage_log("SAME=%u EXPECT=1", equal ? 1u : 0u);
    tdc_sys_storage_log("CIPHER_CHANGED=%u EXPECT=1", changed ? 1u : 0u);
    if (!equal || !changed) return tdc77_stop("COMPARE");
    fr = f_unlink(path);
    tdc_sys_storage_log("TEST_FILE_REMOVE=%d EXPECT=0", (int)fr);
    if (fr != FR_OK) return tdc77_stop("REMOVE");
    tdc_sys_storage_log("FILE_ROUNDTRIP=MATCH");
    tdc_sys_storage_log("POWER_CYCLE_PERSISTENCE=NOT_RUN");
    return 0;
}
#endif
