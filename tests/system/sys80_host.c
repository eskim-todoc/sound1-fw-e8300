/* SYS-080 전용 RAM 경계 시험. 장치/실제 파일시스템에는 접근하지 않는다.
 * 제품 reader/CRC 입력 코드/공통 로그는 현재 원문을 빌드한다.
 * 실제 arm/check/clear 복구 코드를 실행한다. boot/설정 초기화 모델은 없다. */
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ff.h>
#include <ci_filesystem.h>
#include "product_log.inc"
#include "../../src/2__cm3/Cortex-M3-src/test/tdc_sys_test_80.c"

#define CAPACITY 8192u
#define MAX_EVENTS 1024u
typedef struct { bool exists; unsigned size; uint8_t bytes[CAPACITY]; } RamFile;
typedef enum { OP_OPEN, OP_SEEK, OP_READ, OP_WRITE, OP_SYNC, OP_CLOSE,
               OP_TRUNCATE, OP_UNLINK } Operation;
typedef enum { FAULT_NONE, FAULT_ERROR, FAULT_SHORT, FAULT_SEEK_POSITION } Fault;
typedef struct { Operation op; unsigned file; bool product; } Event;
enum { TDC_HOST_USER, TDC_HOST_BACKUP, TDC_HOST_TEST };
static RamFile ram[3];
static uint8_t original[4096];
static unsigned original_size;
static Event events[MAX_EVENTS];
static unsigned event_count, fail_at;
static Fault fault;
static bool fault_seen, fail_cleanup_close, close_poisoned;
static unsigned assertions, scenarios, longest_row, largest_reply, guarded_reader_closes;
static unsigned guarded_direct_closes, guarded_cleanup_closes;
static const uint32_t s_tdc_expected_pre[7] = {2, 2, 5, 2, 2, 1, 2};
static const uint32_t s_tdc_expected_default[7] = {1, 4, 1, 1, 1, 2, 1};
static uint16_t crc_register;
FIL g_ci_filesystem_ohdl;

bool tdc_sys_storage_is_active(void)
{
    return s_tdc_storage_hold;
}

#define REQUIRE(condition) do { ++assertions; if (!(condition)) { \
    fprintf(stderr, "FAIL line=%u expression=%s event=%u fault=%u\n", \
            (unsigned)__LINE__, #condition, event_count, fail_at); \
    for (unsigned log_i = 0; log_i < s_tdc_storage_count; ++log_i) \
        fputs(s_tdc_storage_lines[log_i], stderr); \
    exit(1); } } while (0)

static uint16_t crc_byte(uint16_t value, uint8_t byte)
{
    value ^= (uint16_t)byte << 8;
    for (unsigned bit = 0; bit < 8u; ++bit)
        value = (uint16_t)((value << 1) ^ ((value & 0x8000u) ? 0x1021u : 0u));
    return value;
}

/* 기대 파일은 제품의 직렬화/백업 생성 함수를 호출하지 않고 구성한다. */
static uint16_t reference_crc16(const uint8_t *bytes, unsigned size)
{
    uint16_t crc = 0xffffu;
    while (size-- != 0u) crc = crc_byte(crc, *bytes++);
    return crc;
}
static uint32_t reference_crc32(const uint8_t *bytes, unsigned size)
{
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < size; ++i)
    {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
    }
    return ~crc;
}
static uint32_t read_le32(const uint8_t *bytes)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4u; ++i) value |= (uint32_t)bytes[i] << (i * 8u);
    return value;
}
static void write_le32(uint8_t *bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4u; ++i) bytes[i] = (uint8_t)(value >> (i * 8u));
}
static void expected_record(uint8_t bytes[40], const uint32_t fields[7])
{
    for (unsigned i = 0; i < 7u; ++i) write_le32(bytes + i * 4u, fields[i]);
    write_le32(bytes + 28u, reference_crc16(bytes, 28u));
    memcpy(bytes + 32u, "TDC80TS1", 8u);
}

void Sys_Set_CRC_Config(unsigned unit, unsigned config)
{
    REQUIRE(unit == CRC && config == (CRC_LITTLE_ENDIAN | CRC_CCITT));
}
void Sys_CRC_CCITTInitValue(unsigned unit)
{
    REQUIRE(unit == CRC);
    crc_register = 0xffffu;
}
void Sys_CRC_Add(unsigned unit, uint32_t value, unsigned bits)
{
    REQUIRE(unit == CRC && (bits == 8u || bits == 32u));
    for (unsigned shift = 0; shift < bits; shift += 8u)
        crc_register = crc_byte(crc_register, (uint8_t)(value >> shift));
}
uint32_t Sys_CRC_GetFinalValue(unsigned unit)
{
    REQUIRE(unit == CRC);
    return crc_register;
}
void ci_aes_decrypt(uint8_t *data)
{
    (void)data;
    REQUIRE(!"AES is outside SYS80 and must never execute");
}

static unsigned file_id(const char *path)
{
    if (strcmp(path, "1:/ISD1_USER_SETTING") == 0) return TDC_HOST_USER;
    if (strcmp(path, "1:/SYS80.BAK") == 0) return TDC_HOST_BACKUP;
    REQUIRE(strcmp(path, "1:/SYS80.TST") == 0);
    return TDC_HOST_TEST;
}

static Fault record(Operation op, unsigned id, FIL *fp)
{
    REQUIRE(!close_poisoned);
    REQUIRE(event_count < MAX_EVENTS);
    events[event_count++] = (Event){op, id, fp == &g_ci_filesystem_ohdl};
    if (event_count != fail_at) return FAULT_NONE;
    fault_seen = true;
    return fault;
}

FRESULT f_open(FIL *fp, const char *name, uint8_t mode)
{
    REQUIRE(!fp->open);
    const unsigned id = file_id(name);
    if (id == TDC_HOST_TEST && !ram[id].exists && (mode & FA_WRITE))
        REQUIRE((mode & FA_CREATE_NEW) != 0u);
    if (record(OP_OPEN, id, fp) == FAULT_ERROR) return FR_DISK_ERR;
    if ((mode & FA_CREATE_NEW) && ram[id].exists) return FR_EXIST;
    if (!ram[id].exists)
    {
        if (!(mode & (FA_CREATE_NEW | FA_OPEN_ALWAYS))) return FR_NO_FILE;
        ram[id].exists = true;
        ram[id].size = 0u;
    }
    *fp = (FIL){id, 0u, mode, true};
    return FR_OK;
}
FRESULT f_lseek(FIL *fp, FSIZE_t pos)
{
    REQUIRE(fp->open);
    const Fault inject = record(OP_SEEK, fp->file, fp);
    if (inject == FAULT_ERROR) return FR_DISK_ERR;
    REQUIRE(pos < CAPACITY);
    fp->pos = pos + (inject == FAULT_SEEK_POSITION ? 1u : 0u);
    return FR_OK;
}
FRESULT f_read(FIL *fp, void *out, UINT amount, UINT *done)
{
    REQUIRE(fp->open && (fp->mode & FA_READ));
    *done = 0;
    const Fault inject = record(OP_READ, fp->file, fp);
    if (inject == FAULT_ERROR) return FR_DISK_ERR;
    const unsigned remaining = fp->pos < ram[fp->file].size ? ram[fp->file].size - fp->pos : 0u;
    if (amount > remaining) amount = remaining;
    if (inject == FAULT_SHORT && amount > 0u) --amount;
    memcpy(out, ram[fp->file].bytes + fp->pos, amount);
    fp->pos += amount;
    *done = amount;
    return FR_OK;
}
FRESULT f_write(FIL *fp, const void *data, UINT amount, UINT *done)
{
    REQUIRE(fp->open && (fp->mode & FA_WRITE));
    *done = 0;
    const Fault inject = record(OP_WRITE, fp->file, fp);
    if (inject == FAULT_ERROR) return FR_DISK_ERR;
    if (inject == FAULT_SHORT && amount > 0u) --amount;
    REQUIRE(fp->pos + amount <= CAPACITY);
    memcpy(ram[fp->file].bytes + fp->pos, data, amount);
    fp->pos += amount;
    if (fp->pos > ram[fp->file].size) ram[fp->file].size = fp->pos;
    *done = amount;
    return FR_OK;
}
FRESULT f_sync(FIL *fp)
{
    REQUIRE(fp->open);
    return record(OP_SYNC, fp->file, fp) == FAULT_ERROR ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *fp)
{
    REQUIRE(fp->open);
    if (record(OP_CLOSE, fp->file, fp) == FAULT_ERROR || (fail_cleanup_close && fault_seen))
    {
        close_poisoned = true;
        return FR_DISK_ERR;
    }
    fp->open = false;
    return FR_OK;
}
FRESULT f_truncate(FIL *fp)
{
    REQUIRE(fp->open && (fp->mode & FA_WRITE));
    if (record(OP_TRUNCATE, fp->file, fp) == FAULT_ERROR) return FR_DISK_ERR;
    ram[fp->file].size = fp->pos;
    return FR_OK;
}
FRESULT f_unlink(const char *name)
{
    const unsigned id = file_id(name);
    REQUIRE(id == TDC_HOST_BACKUP || id == TDC_HOST_TEST);
    if (record(OP_UNLINK, id, NULL) == FAULT_ERROR) return FR_DISK_ERR;
    if (!ram[id].exists) return FR_NO_FILE;
    ram[id].exists = false;
    return FR_OK;
}
FSIZE_t sys80_host_size(const FIL *fp)
{
    REQUIRE(fp->open);
    return ram[fp->file].size;
}

static void reset_log(void)
{
    s_tdc_storage_count = 0;
    s_tdc_storage_next = 0;
    s_tdc_storage_id = 80u;
    s_tdc_storage_overflow = false;
}
static bool logged(const char *text)
{
    for (unsigned i = 0; i < s_tdc_storage_count; ++i)
        if (strstr(s_tdc_storage_lines[i], text)) return true;
    return false;
}
static void validate_log(void)
{
    REQUIRE(!s_tdc_storage_overflow);
    /* 공통 호출자가 추가하는 NOP HOLD 1행까지 합해 24행 이내. */
    REQUIRE(s_tdc_storage_count + 1u <= 24u);
    if (s_tdc_storage_count > largest_reply) largest_reply = s_tdc_storage_count;
    for (unsigned i = 0; i < s_tdc_storage_count; ++i)
    {
        const unsigned length = (unsigned)strlen(s_tdc_storage_lines[i]);
        REQUIRE(length <= 60u && length >= 12u);
        REQUIRE(strcmp(s_tdc_storage_lines[i] + length - 2u, "\r\n") == 0);
        REQUIRE(strncmp(s_tdc_storage_lines[i], "[SYS-080] ", 10u) == 0);
        REQUIRE(!strstr(s_tdc_storage_lines[i], "LOG_FORMAT_ERROR"));
        if (length > longest_row) longest_row = length;
    }
}
static void command(const char *action)
{
    static RamFile protected_before[2];
    const bool private_command = strcmp(action, "restore") != 0;
    if (private_command) memcpy(protected_before, ram, sizeof(protected_before));
    const unsigned first_event = event_count;
    char *argv[] = {"--sys80", (char *)action};
    reset_log();
    REQUIRE(tdc_sys_test_80_execute(2, argv) == 0);
    validate_log();
    if (private_command)
    {
        REQUIRE(memcmp(protected_before, ram, sizeof(protected_before)) == 0);
        for (unsigned i = first_event; i < event_count; ++i)
            REQUIRE(events[i].file == TDC_HOST_TEST);
    }
    if (!close_poisoned) REQUIRE(!s_tdc_file.open && !g_ci_filesystem_ohdl.open);
}
static void clear_fault(void)
{
    event_count = 0u;
    fail_at = 0u;
    fault = FAULT_NONE;
    fault_seen = false;
    fail_cleanup_close = false;
}
static void new_session(void)
{
    /* 소유권/닫기 오류 수명의 경계만 초기화하며 제품 부팅은 호출하지 않는다. */
    s_tdc_busy = false;
    s_tdc_test_owned = false;
    s_tdc_close_failed = false;
    close_poisoned = false;
    s_tdc_storage_hold = true;
    tdc_fs_test_observe_end();
    memset(&s_tdc_file, 0, sizeof(s_tdc_file));
    memset(&g_ci_filesystem_ohdl, 0, sizeof(g_ci_filesystem_ohdl));
    clear_fault();
    reset_log();
}
static void seed(unsigned size)
{
    memset(ram, 0, sizeof(ram));
    original_size = size;
    ram[0].exists = true;
    ram[0].size = size;
    for (unsigned i = 0; i < size; ++i) ram[0].bytes[i] = (uint8_t)(i * 29u + 7u);
    for (unsigned i = 0; i < 7u; ++i) write_le32(ram[0].bytes + i * 4u, 11u + i * 7u);
    /* CRC의 상위 16비트와 후행 바이트까지 무손상 복원되는지 확인한다. */
    write_le32(ram[0].bytes + 28u, 0xbeef0000u | reference_crc16(ram[0].bytes, 28u));
    REQUIRE(size <= sizeof(original));
    memcpy(original, ram[0].bytes, size);
    new_session();
}
static bool original_equal(void)
{
    return ram[0].exists && ram[0].size == original_size &&
           memcmp(ram[0].bytes, original, original_size) == 0;
}
static bool recoverable_backup(void)
{
    return ram[1].exists && ram[1].size >= 24u + original_size &&
           memcmp(ram[1].bytes, "TDC80BK1", 8u) == 0 &&
           read_le32(ram[1].bytes + 12u) == original_size &&
           read_le32(ram[1].bytes + 20u) == reference_crc32(ram[1].bytes, 20u) &&
           read_le32(ram[1].bytes + 16u) == reference_crc32(original, original_size) &&
           memcmp(ram[1].bytes + 24u, original, original_size) == 0;
}
static void seed_private(bool damaged)
{
    ram[2].exists = true;
    ram[2].size = 40u;
    expected_record(ram[2].bytes, s_tdc_expected_pre);
    if (damaged) ram[2].bytes[0] ^= 1u;
}
static void seed_legacy_backup(unsigned marker_bytes)
{
    REQUIRE(marker_bytes <= 8u);
    ram[1].exists = true;
    ram[1].size = 24u + original_size + marker_bytes;
    memcpy(ram[1].bytes, "TDC80BK1", 8u);
    write_le32(ram[1].bytes + 8u, 1u);
    write_le32(ram[1].bytes + 12u, original_size);
    write_le32(ram[1].bytes + 16u, reference_crc32(original, original_size));
    write_le32(ram[1].bytes + 20u, reference_crc32(ram[1].bytes, 20u));
    memcpy(ram[1].bytes + 24u, original, original_size);
    memcpy(ram[1].bytes + 24u + original_size, "ARM1", 4u);
    write_le32(ram[1].bytes + 28u + original_size,
               reference_crc32(original, original_size) ^ UINT32_C(0x80a55a08));
    REQUIRE(recoverable_backup());
}

static void require_private_record(const uint32_t fields[7], bool damaged)
{
    uint8_t expected[40];
    expected_record(expected, fields);
    if (damaged) expected[0] ^= 1u;
    REQUIRE(ram[2].exists && ram[2].size == sizeof(expected));
    REQUIRE(memcmp(ram[2].bytes, expected, sizeof(expected)) == 0);
}
static void require_no_mutation(void)
{
    for (unsigned i = 0; i < event_count; ++i)
        REQUIRE(events[i].op != OP_WRITE && events[i].op != OP_TRUNCATE && events[i].op != OP_UNLINK);
}
static void require_check_pass(void)
{
    REQUIRE(logged("CHECK=PASS") && logged("READ check rc=-1 expect=-1"));
    REQUIRE(logged("READ reset rc=0 expect=0") && logged("DEFAULTS=MATCH; SAVE=VERIFIED"));
    REQUIRE(!tdc_sys_test_80_requires_reboot());
    require_private_record(s_tdc_expected_default, false);
}
static void test_crc_vectors(void)
{
    char vector[] = "123456789";
    REQUIRE(reference_crc32((const uint8_t *)vector, 9u) == UINT32_C(0xcbf43926));
    REQUIRE(ci_crc_ccitt_calc(vector, 9u) == 0x29b1u);
    REQUIRE(ci_crc_ccitt_calc(vector, 0u) == 0xffffu);
    uint8_t record_bytes[40];
    expected_record(record_bytes, s_tdc_expected_pre);
    REQUIRE(reference_crc16(record_bytes, 28u) == 0x84f6u);
    REQUIRE(ci_crc_ccitt_calc(record_bytes, 28u) == 0x84f6u);
    record_bytes[0] ^= 1u;
    REQUIRE(reference_crc16(record_bytes, 28u) == 0xa062u);
    REQUIRE(ci_crc_ccitt_calc(record_bytes, 28u) == 0xa062u);
    expected_record(record_bytes, s_tdc_expected_default);
    REQUIRE(reference_crc16(record_bytes, 28u) == 0x33f7u);
    REQUIRE(ci_crc_ccitt_calc(record_bytes, 28u) == 0x33f7u);
    uint8_t bytes[41];
    for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)(i * 31u + 7u);
    for (unsigned size = 1u; size <= 40u; ++size)
        REQUIRE(ci_crc_ccitt_calc(bytes + 1, size) == reference_crc16(bytes + 1, size));
    ++scenarios;
    puts("CRC VECTORS: pre=84F6 corrupt=A062 defaults=33F7 check123456789=29B1");
}
static void test_round_trip(void)
{
    const unsigned sizes[] = {32u, 97u, 4096u};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
    {
        seed(sizes[i]);
        /* 기존 레거시 백업도 읽거나 수정하지 않는지 command 경계에서 검사한다. */
        seed_legacy_backup(8u);
        command("arm");
        REQUIRE(logged("READ pre rc=0 expect=0") && logged("READ corrupt rc=-1 expect=-1"));
        REQUIRE(logged("ARMED;") && logged("no reboot needed") && s_tdc_test_owned);
        require_private_record(s_tdc_expected_pre, true);
        REQUIRE(!tdc_sys_test_80_requires_reboot());
        if (i == 0u)
        {
            puts("NORMAL ARM LOG (actual host execution):");
            for (unsigned line = 0; line < s_tdc_storage_count; ++line)
                fputs(s_tdc_storage_lines[line], stdout);
        }
        command("check");
        require_check_pass();
        if (i == 0u)
        {
            puts("NORMAL CHECK LOG (actual host execution):");
            for (unsigned line = 0; line < s_tdc_storage_count; ++line)
                fputs(s_tdc_storage_lines[line], stdout);
        }
        clear_fault();
        command("check");
        REQUIRE(logged("CHECK=NO_RESET") && !logged("CHECK=PASS"));
        require_no_mutation();
        command("clear");
        REQUIRE(logged("CLEARED") && !ram[2].exists && !s_tdc_test_owned);
        REQUIRE(original_equal() && recoverable_backup());
        command("arm");
        REQUIRE(logged("ARMED;"));
        command("clear");
        REQUIRE(!ram[2].exists && original_equal());
        ++scenarios;
    }
    /* 실제 사용자 파일의 존재/CRC와 무관하게 시험 파일 왕복이 성립한다. */
    for (unsigned state = 0; state < 3u; ++state)
    {
        seed(97u);
        if (state == 0u) ram[0].exists = false;
        else if (state == 1u) ram[0].size = 0u;
        else ram[0].bytes[0] ^= 1u;
        command("arm");
        REQUIRE(logged("ARMED;"));
        command("check");
        require_check_pass();
        command("clear");
        REQUIRE(!ram[2].exists);
        ++scenarios;
    }
    puts("PRIVATE ROUND TRIP: real arm/check/clear; original and backup untouched; no reboot");
}
static void test_private_inputs(void)
{
    static RamFile before;
    seed(97u);
    command("check");
    REQUIRE(!logged("CHECK=PASS") && !ram[2].exists);
    command("clear");
    REQUIRE(!logged("CLEARED"));
    require_no_mutation();
    ++scenarios;

    for (unsigned size = 0; size <= 41u; ++size)
    {
        seed(97u);
        seed_private(false);
        ram[2].size = size;
        before = ram[2];
        command("arm");
        REQUIRE(!logged("ARMED;") && !s_tdc_test_owned);
        REQUIRE(memcmp(&before, &ram[2], sizeof(before)) == 0);
        if (size != 40u)
        {
            command("check");
            REQUIRE(!logged("CHECK=PASS") && !logged("CHECK=NO_RESET"));
            command("clear");
            REQUIRE(!logged("CLEARED") && memcmp(&before, &ram[2], sizeof(before)) == 0);
            require_no_mutation();
        }
        ++scenarios;
    }
    for (unsigned offset = 32u; offset < 40u; ++offset)
    {
        seed(97u);
        seed_private(true);
        ram[2].bytes[offset] ^= 1u;
        before = ram[2];
        command("check");
        REQUIRE(!logged("CHECK=PASS"));
        command("clear");
        REQUIRE(!logged("CLEARED") && memcmp(&before, &ram[2], sizeof(before)) == 0);
        require_no_mutation();
        ++scenarios;
    }
    /* CRC가 맞으면 기본값과 달라도 모든 필드를 그대로 보존한다. */
    for (unsigned field = 0; field < 7u; ++field)
    {
        seed(97u);
        seed_private(false);
        write_le32(ram[2].bytes + field * 4u, UINT32_MAX);
        write_le32(ram[2].bytes + 28u, 0xbeef0000u | reference_crc16(ram[2].bytes, 28u));
        before = ram[2];
        command("check");
        REQUIRE(logged("CHECK=NO_RESET") && logged("READ check rc=0 expect=0"));
        REQUIRE(memcmp(&before, &ram[2], sizeof(before)) == 0);
        require_no_mutation();
        ++scenarios;
    }
    /* 데이터와 CRC 하위 16비트 손상 모두 실제 check에서 기본값으로 복구한다. */
    for (unsigned offset = 0; offset < 30u; ++offset)
    {
        seed(97u);
        seed_private(false);
        ram[2].bytes[offset] ^= 1u;
        char expected_log[49];
        snprintf(expected_log, sizeof(expected_log), "CRC STORED=%04X CALC=%04X",
                 (unsigned)(uint16_t)read_le32(ram[2].bytes + 28u),
                 (unsigned)reference_crc16(ram[2].bytes, 28u));
        command("check");
        REQUIRE(logged(expected_log));
        require_check_pass();
        ++scenarios;
    }
    seed(97u);
    seed_private(true);
    command("clear");
    REQUIRE(!ram[2].exists && logged("CLEARED"));
    ++scenarios;
    puts("PRIVATE INPUTS: collisions, missing/foreign/truncated files, valid CRC, data/CRC damage");
}
static void test_legacy_restore(void)
{
    static RamFile before_user, before_backup, before_test;
    const unsigned sizes[] = {32u, 97u, 4096u};
    const unsigned markers[] = {0u, 3u, 5u, 8u};
    for (unsigned i = 0; i < 3u; ++i)
        for (unsigned m = 0; m < 4u; ++m)
        {
            seed(sizes[i]);
            seed_legacy_backup(markers[m]);
            ram[0].size = i == 0u ? 4096u : 32u;
            memset(ram[0].bytes, 0xa5, ram[0].size);
            seed_private(true);
            before_test = ram[2];
            command("restore");
            REQUIRE(original_equal() && !ram[1].exists && logged("RESTORED full bytes="));
            REQUIRE(memcmp(&before_test, &ram[2], sizeof(before_test)) == 0);
            REQUIRE((read_le32(ram[0].bytes + 28u) >> 16) == 0xbeefu);
            ++scenarios;
        }
    seed(97u);
    seed_legacy_backup(0u);
    ram[0].exists = false;
    command("restore");
    REQUIRE(original_equal() && !ram[1].exists);
    ++scenarios;

    for (unsigned defect = 0; defect < 12u; ++defect)
    {
        seed(97u);
        seed_legacy_backup(8u);
        switch (defect)
        {
            case 0: ram[1].exists = false; break;
            case 1: ram[1].bytes[0] ^= 1u; break;
            case 2: write_le32(ram[1].bytes + 8u, 2u); break;
            case 3: write_le32(ram[1].bytes + 12u, 31u); break;
            case 4: write_le32(ram[1].bytes + 12u, 4097u); break;
            case 5: ram[1].bytes[16] ^= 1u; break;
            case 6: ram[1].bytes[20] ^= 1u; break;
            case 7: ram[1].bytes[24] ^= 1u; break;
            case 8: ram[1].size = 23u; break;
            case 9: ram[1].size = 24u + original_size - 1u; break;
            case 10: ++ram[1].size; break;
            case 11:
                /* CRC32는 맞지만 원래 제품 CRC16이 틀린 백업도 거절해야 한다. */
                ram[1].bytes[24] ^= 1u;
                write_le32(ram[1].bytes + 16u, reference_crc32(ram[1].bytes + 24u, original_size));
                break;
        }
        if (defect == 2u || defect == 3u || defect == 4u || defect == 5u || defect == 11u)
            write_le32(ram[1].bytes + 20u, reference_crc32(ram[1].bytes, 20u));
        before_user = ram[0]; before_backup = ram[1];
        command("restore");
        REQUIRE(!logged("RESTORED full") && memcmp(&before_user, &ram[0], sizeof(before_user)) == 0);
        REQUIRE(memcmp(&before_backup, &ram[1], sizeof(before_backup)) == 0);
        require_no_mutation();
        ++scenarios;
    }
    puts("LEGACY RESTORE: independent CRC32 fixtures; full 32/97/4096B and high CRC bits/tail preserved");
}
static const char *action_for(const char *scenario)
{
    if (strcmp(scenario, "valid") == 0) return "check";
    if (strcmp(scenario, "owned-clear") == 0) return "clear";
    return scenario;
}
static void prepare_scenario(const char *scenario)
{
    seed(97u);
    seed_legacy_backup(8u);
    if (strcmp(scenario, "check") == 0) seed_private(true);
    else if (strcmp(scenario, "valid") == 0 || strcmp(scenario, "clear") == 0) seed_private(false);
    else if (strcmp(scenario, "owned-clear") == 0)
    {
        command("arm");
        REQUIRE(logged("ARMED;") && s_tdc_test_owned);
    }
    else if (strcmp(scenario, "restore") == 0)
    {
        ram[0].size = 32u;
        memset(ram[0].bytes, 0xa5, 32u);
        seed_private(true);
    }
    clear_fault();
}
static bool success_logged(const char *scenario)
{
    if (strcmp(scenario, "arm") == 0) return logged("ARMED;");
    if (strcmp(scenario, "restore") == 0) return logged("RESTORED full");
    if (strcmp(action_for(scenario), "clear") == 0) return logged("CLEARED");
    return logged("CHECK=PASS") || logged("CHECK=NO_RESET");
}
static void require_close_quarantine(void)
{
    const unsigned before = event_count;
    REQUIRE(close_poisoned && tdc_sys_test_80_requires_reboot());
    const char *actions[] = {"arm", "check", "clear", "restore"};
    for (unsigned i = 0; i < 4u; ++i)
    {
        command(actions[i]);
        REQUIRE(logged("REJECT close failed") && event_count == before);
    }
}
static void test_fault_matrix(const char *scenario)
{
    static Event baseline[MAX_EVENTS];
    static RamFile backup_before, test_before;
    const char *action = action_for(scenario);
    const bool restoring = strcmp(scenario, "restore") == 0;
    prepare_scenario(scenario);
    command(action);
    REQUIRE(success_logged(scenario));
    const unsigned total = event_count;
    memcpy(baseline, events, total * sizeof(Event));
    unsigned tested = 0;
    for (unsigned at = 1u; at <= total; ++at)
    {
        const Event entry = baseline[at - 1u];
        for (Fault kind = FAULT_ERROR; kind <= FAULT_SEEK_POSITION; ++kind)
        {
            if (kind == FAULT_SHORT && entry.op != OP_READ && entry.op != OP_WRITE) continue;
            if (kind == FAULT_SEEK_POSITION && entry.op != OP_SEEK) continue;
            prepare_scenario(scenario);
            backup_before = ram[1]; test_before = ram[2];
            fail_at = at; fault = kind;
            command(action);
            REQUIRE(fault_seen && !success_logged(scenario));
            if (restoring)
            {
                REQUIRE(recoverable_backup() && memcmp(&backup_before, &ram[1], sizeof(backup_before)) == 0);
                REQUIRE(memcmp(&test_before, &ram[2], sizeof(test_before)) == 0);
            }
            else REQUIRE(original_equal() && recoverable_backup());
            if (entry.op == OP_CLOSE && kind == FAULT_ERROR)
            {
                REQUIRE(event_count == at);
                if (entry.product)
                {
                    REQUIRE(logged("IO reader.close fr=1"));
                    ++guarded_reader_closes;
                }
                else ++guarded_direct_closes;
                require_close_quarantine();
            }
            else REQUIRE(!close_poisoned && !tdc_sys_test_80_requires_reboot());
            if (strcmp(scenario, "arm") == 0 && ram[2].exists && !close_poisoned)
            {
                /* 성공한 CREATE_NEW 뒤 부분 파일도 같은 세션에서만 정리 가능하다. */
                REQUIRE(s_tdc_test_owned);
                clear_fault();
                command("clear");
                REQUIRE(logged("CLEARED") && !ram[2].exists && !s_tdc_test_owned);
            }
            if (restoring)
            {
                new_session();
                command("restore");
                REQUIRE(original_equal() && !ram[1].exists);
            }
            ++tested; ++scenarios;
        }
        /* 정상 close뿐 아니라 I/O 실패 분기의 close 실패도 전체 차단해야 한다. */
        if (entry.op == OP_SEEK || entry.op == OP_READ || entry.op == OP_WRITE ||
            entry.op == OP_SYNC || entry.op == OP_TRUNCATE)
        {
            prepare_scenario(scenario);
            fail_at = at; fault = FAULT_ERROR; fail_cleanup_close = true;
            command(action);
            REQUIRE(fault_seen && close_poisoned && !success_logged(scenario));
            REQUIRE(event_count == at + 1u && events[event_count - 1u].op == OP_CLOSE);
            require_close_quarantine();
            REQUIRE(recoverable_backup());
            ++guarded_cleanup_closes; ++scenarios;
        }
    }
    printf("FAULT MATRIX %s: %u error/short/seek boundaries; cleanup closes checked\n", scenario, tested);
}
static void test_ownership_lifetime(void)
{
    seed(97u);
    command("arm");
    unsigned first_write = 0;
    for (unsigned i = 0; i < event_count; ++i)
        if (events[i].op == OP_WRITE) { first_write = i + 1u; break; }
    REQUIRE(first_write != 0u);
    for (unsigned same_session = 0; same_session < 2u; ++same_session)
    {
        seed(97u);
        fail_at = first_write; fault = FAULT_SHORT;
        command("arm");
        REQUIRE(fault_seen && !logged("ARMED;") && s_tdc_test_owned && ram[2].size == 39u);
        if (same_session) clear_fault(); else new_session();
        command("check");
        REQUIRE(!logged("CHECK=PASS"));
        command("clear");
        REQUIRE(ram[2].exists == !same_session);
        REQUIRE(logged("CLEARED") == (same_session != 0u));
        ++scenarios;
    }
    seed(97u);
    command("arm");
    new_session();
    command("check");
    require_check_pass();
    command("clear");
    REQUIRE(!ram[2].exists);
    ++scenarios;
}
static void test_command_guards(void)
{
    seed(32u);
    REQUIRE(tdc_sys_test_80_execute(0, NULL) == -1);
    REQUIRE(tdc_sys_test_80_execute(2, NULL) == -1);
    char *bad[] = {NULL, "arm"};
    REQUIRE(tdc_sys_test_80_execute(2, bad) == -1);
    bad[0] = "sys80"; bad[1] = NULL;
    REQUIRE(tdc_sys_test_80_execute(2, bad) == -1);
    bad[1] = "erase";
    REQUIRE(tdc_sys_test_80_execute(2, bad) == -1);
    bad[0] = "wrong"; bad[1] = "arm";
    REQUIRE(tdc_sys_test_80_execute(2, bad) == -1 && event_count == 0u);
    s_tdc_busy = true;
    command("arm");
    REQUIRE(logged("already executing") && event_count == 0u);
    s_tdc_busy = false;
    seed_private(false);
    s_tdc_storage_hold = false;
    REQUIRE(!product_read("1:/SYS80.TST", "no-hold", ram[2].bytes, 0) && event_count == 0u);
    s_tdc_storage_hold = true;
    tdc_fs_test_observation_t observation;
    REQUIRE(tdc_fs_test_observe_begin(&observation));
    REQUIRE(!product_read("1:/SYS80.TST", "busy", ram[2].bytes, 0) && event_count == 0u);
    tdc_fs_test_observe_end();
    REQUIRE(!tdc_sys_test_80_requires_reboot());
    ++scenarios;
}

static void test_log_edges(void)
{
    char body[50];
    memset(body, 'X', sizeof(body));
    body[47] = '\0';
    reset_log();
    tdc_sys_storage_log("%s", body);
    REQUIRE(strlen(s_tdc_storage_lines[0]) == 59u);
    validate_log();
    body[47] = 'X'; body[48] = '\0';
    reset_log();
    tdc_sys_storage_log("%s", body);
    REQUIRE(strlen(s_tdc_storage_lines[0]) == 60u);
    validate_log();
    body[48] = 'X'; body[49] = '\0';
    reset_log();
    tdc_sys_storage_log("%s", body);
    REQUIRE(logged("LOG_FORMAT_ERROR") && strlen(s_tdc_storage_lines[0]) <= 60u);
    ++scenarios;
    puts("ACTUAL COMMON LOGGER: body47=59B body48=60B body49=REJECT");
}

int main(void)
{
    test_crc_vectors();
    test_round_trip();
    test_private_inputs();
    test_legacy_restore();
    test_fault_matrix("arm");
    test_fault_matrix("check");
    test_fault_matrix("valid");
    test_fault_matrix("clear");
    test_fault_matrix("owned-clear");
    test_fault_matrix("restore");
    test_ownership_lifetime();
    test_command_guards();
    const unsigned module_longest = longest_row;
    test_log_edges();
    REQUIRE(guarded_reader_closes == 6u && guarded_direct_closes > 0u && guarded_cleanup_closes > 0u);
    printf("CLOSE GUARD: reader=%u direct=%u error_cleanup=%u; no later FS IO; backup retained\n",
           guarded_reader_closes, guarded_direct_closes, guarded_cleanup_closes);
    printf("SYS80 HOST PASS: scenarios=%u assertions=%u module_max_row=%uB max_reply=%u+1 lines\n",
           scenarios, assertions, module_longest, largest_reply);
    puts("SCOPE: RAM FatFs, real reader/CRC byte feed/log; CRC MMIO modeled");
    puts("SCOPE: actual private-file recovery; no boot/default-install model or ci_map init");
    puts("NOT TESTED: hardware CRC, flash durability, UI/application, real reboot");
    return 0;
}
