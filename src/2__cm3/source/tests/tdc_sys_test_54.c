/* SYS-054: 실행 이미지에 쓰지 않는 CM3 측 FEZ 검사.
 * 부트로더의 CRC/파일 규격으로 검사하지만 메모리 적재/기동 자체는 실행하지 않는다.
 * 기존 부트로더의 VOLATILE 예외/조기 EOF 성공 문제는 수정하지 않는다. */
#include "tdc_sys_test_54.h"
#if TDC_SYS_TEST_54_ENABLE
#include "tdc_sys_test_storage.h"
#include "tdc_fs.h"
#include "../../../0__bootloader/include/bootloader/bootloader_application_file.h"

static FATFS s_tdc54_mount;
static FIL s_tdc54_file;
static uint8_t s_tdc54_header[APP_HEADER_MAXIMUM_SIZE];
static uint8_t s_tdc54_data[256];

static uint16_t tdc54_u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t tdc54_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t tdc54_crc(uint16_t crc, const uint8_t *data, unsigned size)
{
    /* SDK CRC_CCITT / init FFFF / little-endian 입력과 같은 연속 바이트 연산.
     * 파일 읽기 도중 공유 하드웨어 CRC 상태를 보유하지 않는다. */
    for (unsigned i = 0; i < size; ++i)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (uint16_t)((crc & 0x8000u) ? ((uint32_t)crc << 1) ^ 0x1021u : (uint32_t)crc << 1);
    }
    return crc;
}

static bool tdc54_read(void *data, unsigned size)
{
    unsigned got = 0;
    const FRESULT fr = f_read(&s_tdc54_file, data, size, &got);
    if (fr != FR_OK || got != size)
    {
        tdc_sys_storage_log("READ=%d BYTES=%u EXPECT=%u", (int)fr, got, size);
        return false;
    }
    return true;
}

/* 결과 0=CRC 일치, 1=검사 실패, 2=지원하지 않는 형식, 3=VOLATILE 포함 부분 검사.
 * 입력 파일을 변조하는 대신 읽은 시험용 RAM 한 비트만 바꾼다. */
static int tdc54_check(unsigned injection)
{
    bool injected = false;
    unsigned volatile_count = 0, empty_count = 0;
    if (!tdc54_read(s_tdc54_header, 8)) return 1;
    const uint32_t ident = tdc54_u32(s_tdc54_header);
    if (ident == APPFILE_IDENT_APPENCRYPTED)
    {
        tdc_sys_storage_log("ENCRYPTED_FILE=UNSUPPORTED; no PASS");
        return 2;
    }
    if (ident != APPFILE_IDENT_APP)
    {
        tdc_sys_storage_log("FILE_IDENT=INVALID; expected application");
        return 1;
    }
    const unsigned sections = tdc54_u16(s_tdc54_header + 4);
    const unsigned header_size = 8u + 2u * sections + (sections & 1u) * 2u;
    if (sections == 0u || header_size > sizeof(s_tdc54_header))
    {
        tdc_sys_storage_log("SECTION_COUNT=%u INVALID", sections);
        return 1;
    }
    if (!tdc54_read(s_tdc54_header + 8, header_size - 8)) return 1;
    if (injection == 1u)
    {
        s_tdc54_header[6] ^= 1u;
        injected = true;
    }
    const uint16_t header_crc = tdc54_crc(0xffffu, s_tdc54_header, header_size);
    tdc_sys_storage_log("HEADER_RESIDUE=%04X EXPECT=0000", header_crc);
    if (header_crc != 0) return 1;
    for (unsigned section = 0; section < sections; ++section)
    {
        uint8_t head[16];
        if (!tdc54_read(head, sizeof(head))) return 1;
        if (tdc54_crc(0xffffu, head, sizeof(head)) != 0u)
        {
            tdc_sys_storage_log("SEC=%u HEADER_BAD_OR_ENCRYPTED", section);
            return 1;
        }
        const unsigned type = tdc54_u16(head + 8);
        const unsigned flags = tdc54_u16(head + 10);
        const uint32_t words = tdc54_u32(head + 4);
        if ((flags & APPDATA_FLAGS_ENCRYPTED) != 0u)
        {
            tdc_sys_storage_log("SEC=%u ENCRYPTED_UNSUPPORTED; no PASS", section);
            return 2;
        }
        if (type != SEC_CFX_X && type != SEC_CFX_Y && type != SEC_CFX_P && type != SEC_CM3_P &&
            type != SEC_IOMEM && type != SEC_BOOT_CFX && type != SEC_BOOT_CM3)
        {
            tdc_sys_storage_log("SEC=%u UNKNOWN_TYPE=%u", section, type);
            return 1;
        }
        if (flags & APPDATA_FLAGS_EMPTY)
        {
            ++empty_count;
            continue;
        }
        if (words > 0x200000u) /* 오버플로와 비정상 파일의 긴 전경 점유를 차단. */
        {
            tdc_sys_storage_log("SEC=%u INVALID_SIZE", section);
            return 1;
        }
        uint32_t remain = (type == SEC_BOOT_CFX || type == SEC_BOOT_CM3) ? 8u : words * 4u;
        if (remain > f_size(&s_tdc54_file) - f_tell(&s_tdc54_file))
        {
            tdc_sys_storage_log("SEC=%u TRUNCATED_DATA", section);
            return 1;
        }
        uint16_t crc = 0xffffu;
        while (remain != 0u)
        {
            const unsigned n = remain > sizeof(s_tdc54_data) ? sizeof(s_tdc54_data) : (unsigned)remain;
            if (!tdc54_read(s_tdc54_data, n)) return 1;
            if (injection == 2u && !injected && !(flags & APPDATA_FLAGS_VOLATILE))
            {
                s_tdc54_data[0] ^= 1u;
                injected = true;
                tdc_sys_storage_log("RAM_FLIP=SEC%u; FILE_UNCHANGED", section);
            }
            crc = tdc54_crc(crc, s_tdc54_data, n);
            remain -= n;
            SYS_WATCHDOG_REFRESH();
        }
        /* 부트로더는 표의 16bit CRC를 32bit 입력으로 붙여 잔여값 0을 검사한다. */
        const uint16_t expected = tdc54_u16(s_tdc54_header + 6u + 2u * section);
        const uint8_t tail[4] = {(uint8_t)expected, (uint8_t)(expected >> 8), 0, 0};
        const uint16_t residue = tdc54_crc(crc, tail, sizeof(tail));
        if (flags & APPDATA_FLAGS_VOLATILE)
        {
            ++volatile_count;
            if (residue != 0) tdc_sys_storage_log("SEC=%u VOLATILE_CRC_BAD; boot ignores", section);
        }
        else if (residue != 0)
        {
            tdc_sys_storage_log("SEC=%u CRC_BAD RESIDUE=%04X", section, residue);
            return 1;
        }
    }
    tdc_sys_storage_log("SECTIONS=%u EMPTY=%u VOLATILE=%u", sections, empty_count, volatile_count);
    if (injection == 2u && !injected)
    {
        tdc_sys_storage_log("NO_DATA_FOR_INJECTION; no PASS");
        return 2;
    }
    if (f_tell(&s_tdc54_file) != f_size(&s_tdc54_file))
    {
        tdc_sys_storage_log("TRAILING_BYTES; strict check rejected");
        return 1;
    }
    return volatile_count != 0u ? 3 : 0;
}

int tdc_sys_test_54_execute(int argc, char *argv[])
{
    if (argc != 4 || strlen(argv[2]) != 1 || strlen(argv[3]) != 1 ||
        argv[2][0] < '0' || argv[2][0] > '4' || argv[3][0] < '0' || argv[3][0] > '2') return -1;
    unsigned injection;
    if (strcmp(argv[1], "check") == 0) injection = 0;
    else if (strcmp(argv[1], "header") == 0) injection = 1;
    else if (strcmp(argv[1], "data") == 0) injection = 2;
    else return -1;
    char path[24];
    if (argv[2][0] == '0') snprintf(path, sizeof(path), "0:/APP00%c.FEZ", argv[3][0]);
    else snprintf(path, sizeof(path), "0:/%c/APP00%c.FEZ", argv[2][0], argv[3][0]);
    tdc_sys_storage_log("FILE=%s READ_ONLY", path);
    tdc_sys_storage_log("RAM_INJECTION=%u BOOT=NOT_RUN", injection);
    /* 별도 FATFS 객체로 부트 드라이브만 연다. 사용자 드라이브/현재 경로는 바꾸지 않는다. */
    FRESULT fr = f_mount(&s_tdc54_mount, "0:", 1);
    if (fr != FR_OK)
    {
        tdc_sys_storage_log("MOUNT=%d", (int)fr);
        (void)f_mount(NULL, "0:", 0);
        return 0;
    }
    fr = f_open(&s_tdc54_file, path, FA_READ | FA_OPEN_EXISTING);
    if (fr == FR_OK)
    {
        int result = tdc54_check(0u);
        if (injection != 0u)
        {
            /* 원본부터 정상임을 확인해야 손상 검출을 주입한 비트에 귀속할 수 있다. */
            if (result == 0 && f_lseek(&s_tdc54_file, 0) == FR_OK)
            {
                tdc_sys_storage_log("BASELINE=MATCH; starting RAM fault");
                result = tdc54_check(injection);
            }
            else
            {
                tdc_sys_storage_log("INJECTION=NOT_RUN; baseline not verified");
                result = 2;
            }
        }
        const FRESULT close_ret = f_close(&s_tdc54_file);
        tdc_sys_storage_log("CRC_CHECK=%s CLOSE=%d", result == 0 ? "MATCH" : result == 1 ? "REJECT" : result == 3 ? "PARTIAL" : "NOT_CHECKED", (int)close_ret);
    }
    else tdc_sys_storage_log("OPEN=%d; check slot/app number", (int)fr);
    fr = f_mount(NULL, "0:", 0);
    tdc_sys_storage_log("UNMOUNT=%d; actual boot still needs test", (int)fr);
    return 0;
}
#endif
