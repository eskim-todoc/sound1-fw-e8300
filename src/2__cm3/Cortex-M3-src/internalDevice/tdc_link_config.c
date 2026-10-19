#include <tdc_link_config.h>

#include <string.h>
#include <ff.h>
#include <hw.h>
#include <cfx_cm3_sharedMemory.h>

#define TDC_LINK_CONFIG_FILE "LINKPARM.TXT"
#define TDC_LINK_CONFIG_BYTES 40u
#define TDC_LINK_CONFIG_IDENT_BEGIN 0x11223344u
#define TDC_LINK_CONFIG_IDENT_END 0x44332211u

static tdc_link_config_t s_config = {140, 172, 3, 4, 1, 0, 0, 214};
/* 파일 수명은 이 모듈이 소유한다. 다른 설정/맵의 공용 FIL을 덮지 않는다. */
static FIL s_file;
static bool s_close_uncertain;

const tdc_link_config_t *tdc_link_config_get(void)
{
    return &s_config;
}

bool tdc_link_config_valid(const tdc_link_config_t *config)
{
    return config != NULL
        && config->min_level >= TDC_LINK_CONFIG_MIN_LEVEL
        && config->mapping_min_level >= TDC_LINK_CONFIG_MIN_LEVEL
        && config->max_level <= TDC_LINK_CONFIG_MAX_LEVEL
        && config->min_level < config->max_level
        && config->mapping_min_level < config->max_level
        && config->period_100ms >= 1 && config->period_100ms <= 255
        && config->step_up >= 1 && config->step_up <= 40
        && config->coin_count <= 100 && config->mode <= 1 && config->accel <= 1;
}

static void publish(const tdc_link_config_t *config)
{
    s_config = *config;
    /* 단일 정렬 워드 게시. CFX가 실행 중인 시퀀스는 취소하지 않는다. */
    cfx_cm3_sharedMemoryAll.tdc_backtel_period_100ms = (int)config->period_100ms;
}

static void put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8)
        | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

/* 원본 Sullivan의 기본40바이트와 같은 순서/리틀엔디언이다.
 * 구조체 padding에 의존하지 않으며 뒤쪽 FF 확장 영역은 읽거나 쓰지 않는다. */
static void encode(const tdc_link_config_t *config, uint8_t *out)
{
    put_u32(out, TDC_LINK_CONFIG_IDENT_BEGIN);
    put_u32(out + 4, config->min_level);
    put_u32(out + 8, config->period_100ms);
    put_u32(out + 12, config->mapping_min_level);
    put_u32(out + 16, config->step_up);
    put_u32(out + 20, config->coin_count);
    put_u32(out + 24, config->mode);
    put_u32(out + 28, config->accel);
    put_u32(out + 32, config->max_level);
    put_u32(out + 36, TDC_LINK_CONFIG_IDENT_END);
}

static bool decode(const uint8_t *in, tdc_link_config_t *config)
{
    if (get_u32(in) != TDC_LINK_CONFIG_IDENT_BEGIN
        || get_u32(in + 36) != TDC_LINK_CONFIG_IDENT_END) return false;
    config->min_level = get_u32(in + 4);
    config->period_100ms = get_u32(in + 8);
    config->mapping_min_level = get_u32(in + 12);
    config->step_up = get_u32(in + 16);
    config->coin_count = get_u32(in + 20);
    config->mode = get_u32(in + 24);
    config->accel = get_u32(in + 28);
    config->max_level = get_u32(in + 32);
    /* 구형 기본40바이트의 예약0은 원본처럼 기본 상한214로 해석한다. */
    if (config->max_level == 0) config->max_level = TDC_LINK_CONFIG_MAX_LEVEL;
    return tdc_link_config_valid(config);
}

static bool close_file(void)
{
    if (f_close(&s_file) != FR_OK)
    {
        /* 닫기 실패 후 FIL을 재사용하지 않는다. 재기동 전 새 쓰기는 거부한다. */
        s_close_uncertain = true;
        return false;
    }
    return true;
}

static int read_record(uint8_t *out)
{
    UINT count = 0;
    FRESULT result;
    if (s_close_uncertain) return TDC_LINK_CONFIG_IO_ERROR;
    result = f_open(&s_file, TDC_LINK_CONFIG_FILE, FA_READ);
    if (result == FR_NO_FILE) return 1;
    if (result != FR_OK) return TDC_LINK_CONFIG_IO_ERROR;
    result = f_read(&s_file, out, TDC_LINK_CONFIG_BYTES, &count);
    if (!close_file()) return TDC_LINK_CONFIG_IO_ERROR;
    if (result != FR_OK || count != TDC_LINK_CONFIG_BYTES) return TDC_LINK_CONFIG_IO_ERROR;
    return TDC_LINK_CONFIG_OK;
}

static int save_record(const tdc_link_config_t *config)
{
    uint8_t expected[TDC_LINK_CONFIG_BYTES];
    uint8_t observed[TDC_LINK_CONFIG_BYTES];
    UINT count = 0;
    bool ok;
    if (s_close_uncertain) return TDC_LINK_CONFIG_IO_ERROR;
    encode(config, expected);
    if (f_open(&s_file, TDC_LINK_CONFIG_FILE, FA_OPEN_ALWAYS | FA_READ | FA_WRITE) != FR_OK)
        return TDC_LINK_CONFIG_IO_ERROR;

    SYS_WATCHDOG_REFRESH();
    ok = f_lseek(&s_file, 4096) == FR_OK;
    if (ok) ok = f_tell(&s_file) == 4096;
    if (ok) ok = f_lseek(&s_file, 0) == FR_OK;
    if (ok) ok = f_tell(&s_file) == 0;
    if (ok) ok = f_write(&s_file, expected, sizeof(expected), &count) == FR_OK;
    if (ok) ok = count == sizeof(expected);
    if (ok) ok = f_sync(&s_file) == FR_OK;
    if (!close_file()) ok = false;
    SYS_WATCHDOG_REFRESH();
    if (!ok) return TDC_LINK_CONFIG_IO_ERROR;
    if (read_record(observed) != TDC_LINK_CONFIG_OK) return TDC_LINK_CONFIG_IO_ERROR;
    if (memcmp(expected, observed, sizeof(expected)) != 0) return TDC_LINK_CONFIG_IO_ERROR;
    return TDC_LINK_CONFIG_OK;
}

int tdc_link_config_init(void)
{
    uint8_t record[TDC_LINK_CONFIG_BYTES];
    tdc_link_config_t candidate;
    int result;
    publish(&s_config);
    result = read_record(record);
    if (result == 1) return save_record(&s_config);
    if (result != TDC_LINK_CONFIG_OK) return result;
    if (!decode(record, &candidate)) return TDC_LINK_CONFIG_RANGE_ERROR;
    publish(&candidate);
    return TDC_LINK_CONFIG_OK;
}

int tdc_link_config_set(unsigned index, unsigned value)
{
    tdc_link_config_t candidate = s_config;
    int result;
    switch (index)
    {
        case 2: candidate.period_100ms = value; break;
        case 3: candidate.min_level = value; break;
        case 4: candidate.mapping_min_level = value; break;
        case 5: candidate.step_up = value; break;
        case 7: candidate.coin_count = value; break;
        case 9: candidate.mode = value; break;
        case 10: candidate.accel = value; break;
        case 16: candidate.max_level = value; break;
        default: return TDC_LINK_CONFIG_RANGE_ERROR;
    }
    if (!tdc_link_config_valid(&candidate)) return TDC_LINK_CONFIG_RANGE_ERROR;
    result = save_record(&candidate);
    if (result == TDC_LINK_CONFIG_OK) publish(&candidate);
    return result;
}
