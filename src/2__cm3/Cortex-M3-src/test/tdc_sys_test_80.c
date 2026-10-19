#include "tdc_sys_test_80.h"

#if TDC_SYS_TEST_80_ENABLE
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ff.h>
#include <ci_crc.h>
#include <ci_filesystem.h>
#include "tdc_sys_test_storage.h"

#define TDC_SYS80_TARGET "1:/ISD1_USER_SETTING"
#define TDC_SYS80_BACKUP "1:/SYS80.BAK"
#define TDC_SYS80_MAX_BYTES 4096u
#define TDC_SYS80_HEADER_BYTES 24u
#define TDC_SYS80_RECORD_BYTES 32u
#define TDC_SYS80_DATA_BYTES 28u
#define TDC_SYS80_TEST_FILE "1:/SYS80.TST"
#define TDC_SYS80_TEST_BYTES 40u

/* 큰 FIL은 모듈 소유 정적 객체에 둔다. 스택/제품 전역 핸들을 빌리지 않는다.
 * arm/check/clear는 시험 파일만 사용한다. 제품 공유 설정은 바꾸지 않는다.
 * 4KB 버퍼와 백업 헤더는 이전 버전의 SYS80.BAK 복원 호환용으로 유지한다. */
static uint8_t s_tdc_raw[TDC_SYS80_MAX_BYTES];
static uint8_t s_tdc_header[TDC_SYS80_HEADER_BYTES];
static FIL s_tdc_file;
static bool s_tdc_busy;
static bool s_tdc_test_owned;
static bool s_tdc_close_failed;
static const uint8_t s_tdc_magic[8] = {'T', 'D', 'C', '8', '0', 'B', 'K', '1'};
static const uint8_t s_tdc_test_magic[8] = {'T', 'D', 'C', '8', '0', 'T', 'S', '1'};
static const uint32_t s_tdc_pre[7] = {2, 2, 5, 2, 2, 1, 2};
/* ci_map_init_map_data의 사용자 설정 기본값. 호스트 검사에서 원본과 대조한다.
 * 제품 초기화 함수를 호출하지 않으므로 실제 맵/게인/사용자 파일은 유지된다. */
static const uint32_t s_tdc_default[7] = {1, 4, 1, 1, 1, 2, 1};
static const char *const s_tdc_setting_names[7] = {
    "MAP", "STIM_VOL", "AUDIO_VOL", "LED", "STIM_ALERT", "TELECOIL", "BLE"
};

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

/* 백업 전체 바이트/메타데이터의 우발적 손상 검사용 CRC32. 인증용이 아니다.
 * 제품 설정 CRC16은 아래에서 기존 ci_crc_ccitt_calc를 그대로 사용한다. */
static uint32_t backup_crc(const uint8_t *p, unsigned size)
{
    uint32_t crc = UINT32_C(0xffffffff);
    while (size-- != 0u)
    {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return ~crc;
}

static bool io_result(const char *op, FRESULT result)
{
    if (result == FR_OK) return true;
    tdc_sys_storage_log("IO %s fr=%u", op, (unsigned)result);
    return false;
}

static bool close_file(FIL *fp, bool ok)
{
    const bool closed = io_result("close", f_close(fp));
    if (!closed) s_tdc_close_failed = true;
    return ok && closed;
}

static bool seek_file(FIL *fp, unsigned offset)
{
    if (!io_result("seek", f_lseek(fp, offset))) return false;
    if (f_tell(fp) == offset) return true;
    tdc_sys_storage_log("IO seek pos=%lu want=%u", (unsigned long)f_tell(fp), offset);
    return false;
}

static bool read_bytes(FIL *fp, void *data, unsigned size)
{
    UINT count = 0;
    const FRESULT result = f_read(fp, data, size, &count);
    if (result == FR_OK && count == size) return true;
    tdc_sys_storage_log("IO read fr=%u n=%u/%u", (unsigned)result, (unsigned)count, size);
    return false;
}

static bool write_bytes(FIL *fp, const void *data, unsigned size)
{
    UINT count = 0;
    const FRESULT result = f_write(fp, data, size, &count);
    if (result == FR_OK && count == size) return true;
    tdc_sys_storage_log("IO write fr=%u n=%u/%u", (unsigned)result, (unsigned)count, size);
    return false;
}

static bool payload_valid(const uint8_t *data)
{
    return ci_crc_ccitt_calc((void *)data, TDC_SYS80_DATA_BYTES) ==
           (uint16_t)get_u32(data + TDC_SYS80_DATA_BYTES);
}

/* 제품 반환은 유지하고 관찰 훅으로 실제 close 결과를 추가 확인한다.
 * 예상된 CRC 거절(-1)도 close 성공이 확인되어야 손상 주입 성공으로 인정한다. */
static bool product_read(const char *path, const char *phase, const uint8_t *expected, int wanted)
{
    uint8_t data[TDC_SYS80_DATA_BYTES];
    uint32_t crc = ~get_u32(expected + TDC_SYS80_DATA_BYTES);
    /* 제품 API는 char*이지만 파일명은 수정하지 않는다. */
    tdc_fs_test_observation_t observation;
    if (!tdc_fs_test_observe_begin(&observation))
    {
        tdc_sys_storage_log("REJECT FS observer unavailable");
        return false;
    }
    memset(data, 0xa5, sizeof(data));
    const int result = ci_filesystem_read_with_crc_and_aes128(
        (char *)path, data, sizeof(data), &crc, NULL, 0, true, false);
    tdc_fs_test_observe_end();
    tdc_sys_storage_log("READ %s rc=%d expect=%d", phase, result, wanted);
    if (observation.close_result != FR_OK)
    {
        s_tdc_close_failed = true;
        tdc_sys_storage_log("IO reader.close fr=%u n=%u", (unsigned)observation.close_result,
                            observation.close_count);
        return false;
    }
    if (observation.close_count != 1u)
    {
        tdc_sys_storage_log("REJECT reader.close count=%u", observation.close_count);
        return false;
    }
    if (result != wanted || memcmp(data, expected, sizeof(data)) != 0 ||
        crc != get_u32(expected + TDC_SYS80_DATA_BYTES))
    {
        tdc_sys_storage_log("FAIL reader return/data/CRC");
        return false;
    }
    return true;
}

static bool load_test(void)
{
    FIL *fp = &s_tdc_file;
    if (!io_result("test.open", f_open(fp, TDC_SYS80_TEST_FILE, FA_READ | FA_OPEN_EXISTING)))
        return false;
    const unsigned long length = (unsigned long)f_size(fp);
    bool ok = length == TDC_SYS80_TEST_BYTES;
    if (!ok) tdc_sys_storage_log("REJECT test size=%lu", length);
    if (ok) ok = seek_file(fp, 0u) && read_bytes(fp, s_tdc_raw, TDC_SYS80_TEST_BYTES);
    if (ok && memcmp(s_tdc_raw + TDC_SYS80_RECORD_BYTES, s_tdc_test_magic, 8u) != 0)
    {
        tdc_sys_storage_log("REJECT test ownership marker");
        ok = false;
    }
    return close_file(fp, ok);
}

/* 원본과의 바이트 비교는 64바이트씩 읽는다. 두 번째 4KB 버퍼는 필요 없다. */
static bool verify_target(const char *path, unsigned size)
{
    uint8_t chunk[64];
    FIL *fp = &s_tdc_file;
    if (!io_result("verify.open", f_open(fp, path, FA_READ | FA_OPEN_EXISTING)))
        return false;
    bool ok = f_size(fp) == size;
    if (!ok) tdc_sys_storage_log("FAIL target length changed");
    if (ok) ok = seek_file(fp, 0u);
    for (unsigned offset = 0; ok && offset < size;)
    {
        unsigned count = size - offset;
        if (count > sizeof(chunk)) count = sizeof(chunk);
        ok = read_bytes(fp, chunk, count);
        if (ok && memcmp(chunk, s_tdc_raw + offset, count) != 0)
        {
            tdc_sys_storage_log("FAIL raw compare offset=%u", offset);
            ok = false;
        }
        offset += count;
    }
    return close_file(fp, ok);
}

static bool write_target(unsigned size)
{
    FIL *fp = &s_tdc_file;
    if (!io_result("target.openW", f_open(fp, TDC_SYS80_TARGET, FA_WRITE | FA_OPEN_ALWAYS)))
        return false;
    bool ok = seek_file(fp, 0u) && write_bytes(fp, s_tdc_raw, size);
    if (ok) ok = io_result("truncate", f_truncate(fp));
    if (ok) ok = io_result("sync", f_sync(fp));
    return close_file(fp, ok);
}

static bool write_test_prefix(unsigned size)
{
    FIL *fp = &s_tdc_file;
    if (!io_result("prefix.open", f_open(fp, TDC_SYS80_TEST_FILE, FA_WRITE | FA_OPEN_EXISTING)))
        return false;
    bool ok = seek_file(fp, 0u) && write_bytes(fp, s_tdc_raw, size);
    if (ok) ok = io_result("prefix.sync", f_sync(fp));
    return close_file(fp, ok);
}

static bool create_test(void)
{
    FIL *fp = &s_tdc_file;
    /* 기존 파일은 덮어쓰지 않는다. 반복 시험 전 clear로 시험 파일만 제거한다. */
    if (!io_result("test.create", f_open(fp, TDC_SYS80_TEST_FILE, FA_WRITE | FA_CREATE_NEW)))
        return false;
    /* 생성 도중 쓰기 실패가 나도 같은 부팅에서는 이 파일만 clear할 수 있다. */
    s_tdc_test_owned = true;
    bool ok = seek_file(fp, 0u) && write_bytes(fp, s_tdc_raw, TDC_SYS80_TEST_BYTES);
    if (ok) ok = io_result("test.sync", f_sync(fp));
    return close_file(fp, ok);
}

static void make_marker(uint8_t marker[8])
{
    memcpy(marker, "ARM1", 4u);
    put_u32(marker + 4, get_u32(s_tdc_header + 16) ^ UINT32_C(0x80a55a08));
}

static bool load_backup(unsigned *size, bool *armed)
{
    uint8_t marker[8], expected[8];
    FIL *fp = &s_tdc_file;
    *armed = false;
    if (!io_result("backup.open", f_open(fp, TDC_SYS80_BACKUP, FA_READ | FA_OPEN_EXISTING)))
        return false;
    const unsigned long length = (unsigned long)f_size(fp);
    bool ok = seek_file(fp, 0u) && read_bytes(fp, s_tdc_header, sizeof(s_tdc_header));
    if (ok)
    {
        *size = get_u32(s_tdc_header + 12);
        ok = memcmp(s_tdc_header, s_tdc_magic, sizeof(s_tdc_magic)) == 0 &&
             get_u32(s_tdc_header + 8) == 1u &&
             get_u32(s_tdc_header + 20) == backup_crc(s_tdc_header, 20u) &&
             *size >= TDC_SYS80_RECORD_BYTES && *size <= TDC_SYS80_MAX_BYTES &&
             length >= TDC_SYS80_HEADER_BYTES + *size &&
             length <= TDC_SYS80_HEADER_BYTES + *size + sizeof(marker);
        if (!ok) tdc_sys_storage_log("FAIL backup header/length");
    }
    if (ok) ok = read_bytes(fp, s_tdc_raw, *size);
    if (ok && backup_crc(s_tdc_raw, *size) != get_u32(s_tdc_header + 16))
    {
        tdc_sys_storage_log("FAIL backup data checksum");
        ok = false;
    }
    if (ok && length == TDC_SYS80_HEADER_BYTES + *size + sizeof(marker))
    {
        ok = read_bytes(fp, marker, sizeof(marker));
        make_marker(expected);
        if (ok) *armed = memcmp(marker, expected, sizeof(marker)) == 0;
    }
    ok = close_file(fp, ok);
    if (ok && !payload_valid(s_tdc_raw))
    {
        tdc_sys_storage_log("FAIL backup original CRC");
        ok = false;
    }
    return ok;
}

static void arm(void)
{
    tdc_sys_storage_log("FILE=" TDC_SYS80_TEST_FILE);
    for (unsigned i = 0; i < 7u; ++i) put_u32(s_tdc_raw + i * 4u, s_tdc_pre[i]);
    put_u32(s_tdc_raw + TDC_SYS80_DATA_BYTES, ci_crc_ccitt_calc(s_tdc_raw, TDC_SYS80_DATA_BYTES));
    memcpy(s_tdc_raw + TDC_SYS80_RECORD_BYTES, s_tdc_test_magic, sizeof(s_tdc_test_magic));
    if (!create_test())
    {
        tdc_sys_storage_log("ARM ABORT; existing file preserved");
        tdc_sys_storage_log("Use --sys80 clear before another arm");
        return;
    }
    if (!verify_target(TDC_SYS80_TEST_FILE, TDC_SYS80_TEST_BYTES) ||
        !product_read(TDC_SYS80_TEST_FILE, "pre", s_tdc_raw, 0)) goto failed;
    tdc_sys_storage_log("SAVE CRC=%04X VERIFIED", (unsigned)get_u32(s_tdc_raw + TDC_SYS80_DATA_BYTES));
    tdc_sys_storage_log("PRE=2,2,5,2,2,1,2 verified");
    /* mapNum의 최하위 비트만 바꾼다(2 -> 3). 저장된 CRC는 그대로 둔다. */
    s_tdc_raw[0] ^= 1u;
    if (!write_test_prefix(1u) || !verify_target(TDC_SYS80_TEST_FILE, TDC_SYS80_TEST_BYTES) ||
        payload_valid(s_tdc_raw) || !product_read(TDC_SYS80_TEST_FILE, "corrupt", s_tdc_raw, -1)) goto failed;
    tdc_sys_storage_log("FAULT offset=0 xor=01; CRC unchanged");
    tdc_sys_storage_log("ARMED; run --sys80 check; no reboot needed");
    return;
failed:
    tdc_sys_storage_log("ARM FAILED; test file retained");
}

static void check(void)
{
    tdc_sys_storage_log("FILE=" TDC_SYS80_TEST_FILE);
    if (!load_test()) goto failed;
    const uint16_t stored = (uint16_t)get_u32(s_tdc_raw + TDC_SYS80_DATA_BYTES);
    const uint16_t calculated = ci_crc_ccitt_calc(s_tdc_raw, TDC_SYS80_DATA_BYTES);
    tdc_sys_storage_log("CRC STORED=%04X CALC=%04X", (unsigned)stored, (unsigned)calculated);
    /* 제품 reader의 -1만으로는 I/O 실패와 CRC 불일치를 구분할 수 없다.
     * 전체 바이트 읽기/CRC 계산과 reader의 반환값/close를 함께 확인한다. */
    if (!product_read(TDC_SYS80_TEST_FILE, "check", s_tdc_raw, stored == calculated ? 0 : -1))
        goto failed;
    if (stored == calculated)
    {
        tdc_sys_storage_log("CRC=MATCH; CHECK=NO_RESET");
        return;
    }
    tdc_sys_storage_log("CRC=MISMATCH; FACTORY_RESET=START");
    for (unsigned i = 0; i < 7u; ++i)
    {
        const uint32_t before = get_u32(s_tdc_raw + i * 4u);
        put_u32(s_tdc_raw + i * 4u, s_tdc_default[i]);
        tdc_sys_storage_log("%s:%lu->%lu", s_tdc_setting_names[i],
                            (unsigned long)before, (unsigned long)s_tdc_default[i]);
    }
    put_u32(s_tdc_raw + TDC_SYS80_DATA_BYTES, ci_crc_ccitt_calc(s_tdc_raw, TDC_SYS80_DATA_BYTES));
    if (!write_test_prefix(TDC_SYS80_RECORD_BYTES) ||
        !verify_target(TDC_SYS80_TEST_FILE, TDC_SYS80_TEST_BYTES) || !load_test()) goto failed;
    /* 다시 읽은 값을 검사한다. RAM에 기본값을 대입한 것만으로 성공 처리하지 않는다. */
    bool defaults_ok = true;
    for (unsigned i = 0; i < 7u; ++i)
        if (get_u32(s_tdc_raw + i * 4u) != s_tdc_default[i]) defaults_ok = false;
    const uint16_t reset_stored = (uint16_t)get_u32(s_tdc_raw + TDC_SYS80_DATA_BYTES);
    const uint16_t reset_calculated = ci_crc_ccitt_calc(s_tdc_raw, TDC_SYS80_DATA_BYTES);
    tdc_sys_storage_log("RESET CRC STORED=%04X CALC=%04X", (unsigned)reset_stored, (unsigned)reset_calculated);
    if (!product_read(TDC_SYS80_TEST_FILE, "reset", s_tdc_raw, 0) ||
        !defaults_ok || reset_stored != reset_calculated) goto failed;
    tdc_sys_storage_log("DEFAULTS=MATCH; SAVE=VERIFIED");
    tdc_sys_storage_log("CHECK=PASS; test file defaults restored");
    return;
failed:
    tdc_sys_storage_log("CHECK=FAIL; test file retained");
}

static void clear_test(void)
{
    /* 현재 부팅에서 직접 만든 파일 또는 완전한 식별 표식이 있는 파일만 제거한다.
     * 알 수 없는 기존 파일/다른 경로는 삭제하지 않는다. */
    if (!s_tdc_test_owned && !load_test()) return;
    if (!io_result("test.unlink", f_unlink(TDC_SYS80_TEST_FILE))) return;
    s_tdc_test_owned = false;
    tdc_sys_storage_log("CLEARED " TDC_SYS80_TEST_FILE);
}

/* 이전 재부팅 시험을 실행한 사용자의 백업을 복구하는 호환 명령이다.
 * 새 arm/check/clear에서는 호출하지 않으며 사용자 파일을 쓰는 유일한 경로다. */
static void restore(void)
{
    unsigned size = 0;
    bool armed = false;
    if (!load_backup(&size, &armed))
    {
        tdc_sys_storage_log("RESTORE ABORT; backup preserved");
        return;
    }
    if (!write_target(size) || !verify_target(TDC_SYS80_TARGET, size) ||
        !product_read(TDC_SYS80_TARGET, "restore", s_tdc_raw, 0))
    {
        tdc_sys_storage_log("RESTORE FAILED; backup preserved");
        return;
    }
    if (!io_result("backup.unlink", f_unlink(TDC_SYS80_BACKUP)))
    {
        tdc_sys_storage_log("Original verified; backup cleanup failed");
        return;
    }
    tdc_sys_storage_log("RESTORED full bytes=%u; backup removed", size);
    tdc_sys_storage_log("Reboot manually to apply original settings");
}

bool tdc_sys_test_80_requires_reboot(void)
{
    return s_tdc_close_failed;
}

int tdc_sys_test_80_execute(int argc, char *argv[])
{
    if (argc != 2 || argv == NULL || argv[0] == NULL || argv[1] == NULL ||
        (strcmp(argv[0], "sys80") != 0 && strcmp(argv[0], "--sys80") != 0)) return -1;
    const bool do_arm = strcmp(argv[1], "arm") == 0;
    const bool do_check = strcmp(argv[1], "check") == 0;
    const bool do_restore = strcmp(argv[1], "restore") == 0;
    const bool do_clear = strcmp(argv[1], "clear") == 0;
    if (!do_arm && !do_check && !do_restore && !do_clear) return -1;
    if (s_tdc_close_failed)
    {
        tdc_sys_storage_log("REJECT close failed; reboot required");
        return 0;
    }
    if (s_tdc_busy)
    {
        tdc_sys_storage_log("REJECT command already executing");
        return 0;
    }
    s_tdc_busy = true;
    if (do_arm) arm();
    else if (do_check) check();
    else if (do_clear) clear_test();
    else restore();
    s_tdc_busy = false;
    return 0;
}
#endif
