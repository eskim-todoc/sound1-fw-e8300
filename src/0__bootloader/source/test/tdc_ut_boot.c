#if defined(TDC_UNIT_TEST)
#include "tdc_ut_boot.h"
#include <hw.h>
#include <bootloader_internal.h>
#include <ci_boot.h>
#include <nvmctrl.h>
#include <string.h>
#if defined(TDC_UNIT_TEST_HOST)
#include "tdc_ut_boot_host.h"
#endif

int g_tdc_ut_boot_actual[TDC_UT_BOOT_CAPACITY];
int g_tdc_ut_boot_expected[TDC_UT_BOOT_CAPACITY];
unsigned g_tdc_ut_boot_count;
int g_tdc_ut_boot_done;
int g_tdc_ut_boot_overflow;
static int s_tdc_boot_pending;
static uint32_t *s_tdc_boot_public_key;
static tdc_ut_boot_file_hook_t s_tdc_boot_file_hook;
static tdc_ut_boot_file_ops_t s_tdc_boot_file_ops;
static tdc_ut_boot_nvm_hook_t s_tdc_boot_nvm_hook;
static uint32_t s_tdc_boot_scratch_backup[TDC_UT_BOOT_SCRATCH_WORDS];
static tdc_ut_boot_observer_t s_tdc_boot_observer;

/* SDK FIL은 약 4KB여서 4KB 스택의 지역 변수로 둘 수 없다. 시험 전용
 * 작업 공간 하나를 기존 RTT 제어 블록 뒤의 BRAM에 배치한다. 정상 빌드에는
 * 객체/구획이 없다. NOLOAD이므로 매 사용 전에 명시적으로 초기화한다.
 * BUSY와 호출자 정지 조건 아래에서만 공유하며 제품의 FIL을 재사용하지 않는다. */
#if defined(TDC_UNIT_TEST_HOST)
static FIL s_tdc_boot_ram_fp;
#else
static FIL s_tdc_boot_ram_fp __attribute__((section(".tdc_ut_boot_workspace"), aligned(8)));
#endif
static FIL *ram_file_reset(void)
{
    memset(&s_tdc_boot_ram_fp,0,sizeof(s_tdc_boot_ram_fp));
    return &s_tdc_boot_ram_fp;
}

tdc_ut_boot_observer_t tdc_ut_boot_observer_get(void) { return s_tdc_boot_observer; }
void tdc_ut_boot_observer_set(tdc_ut_boot_observer_t observer) { s_tdc_boot_observer = observer; }
void tdc_ut_boot_observe(unsigned event, const uint32_t *values, unsigned count)
{
    if(s_tdc_boot_observer)s_tdc_boot_observer(event,values,count);
}

void tdc_ut_boot_file_hook_get(tdc_ut_boot_file_hook_t *hook) { *hook = s_tdc_boot_file_hook; }
void tdc_ut_boot_file_hook_set(const tdc_ut_boot_file_hook_t *hook)
{
    if (hook) s_tdc_boot_file_hook = *hook;
    else memset(&s_tdc_boot_file_hook, 0, sizeof(s_tdc_boot_file_hook));
}
FRESULT tdc_ut_boot_read(FIL *file, void *out, UINT bytes, UINT *read)
{
    if (s_tdc_boot_file_hook.read)
        return s_tdc_boot_file_hook.read(s_tdc_boot_file_hook.context, file, out, bytes, read);
    return f_read(file, out, bytes, read);
}

void tdc_ut_boot_file_ops_get(tdc_ut_boot_file_ops_t *ops) { *ops = s_tdc_boot_file_ops; }
void tdc_ut_boot_file_ops_set(const tdc_ut_boot_file_ops_t *ops)
{
    if (ops) s_tdc_boot_file_ops = *ops;
    else memset(&s_tdc_boot_file_ops, 0, sizeof(s_tdc_boot_file_ops));
}
FRESULT tdc_ut_boot_open(FIL *file, const char *name, unsigned mode)
{
    if (s_tdc_boot_file_ops.open)
        return s_tdc_boot_file_ops.open(s_tdc_boot_file_ops.context, file, name, mode);
    return f_open(file, name, mode);
}
FRESULT tdc_ut_boot_close(FIL *file)
{
    if (s_tdc_boot_file_ops.close)
        return s_tdc_boot_file_ops.close(s_tdc_boot_file_ops.context, file);
    return f_close(file);
}
FRESULT tdc_ut_boot_stat(const char *name, FILINFO *info)
{
    if (s_tdc_boot_file_ops.stat)
        return s_tdc_boot_file_ops.stat(s_tdc_boot_file_ops.context, name, info);
    return f_stat(name, info);
}
FRESULT tdc_ut_boot_write(FIL *file, const void *in, UINT bytes, UINT *written)
{
    if (s_tdc_boot_file_ops.write)
        return s_tdc_boot_file_ops.write(s_tdc_boot_file_ops.context, file, in, bytes, written);
    return f_write(file, in, bytes, written);
}
FRESULT tdc_ut_boot_seek(FIL *file, FSIZE_t offset)
{
    if (s_tdc_boot_file_ops.seek)
        return s_tdc_boot_file_ops.seek(s_tdc_boot_file_ops.context, file, offset);
    return f_lseek(file, offset);
}
FRESULT tdc_ut_boot_sync(FIL *file)
{
    if (s_tdc_boot_file_ops.sync)
        return s_tdc_boot_file_ops.sync(s_tdc_boot_file_ops.context, file);
    return f_sync(file);
}
FRESULT tdc_ut_boot_chdir(const char *name)
{
    if (s_tdc_boot_file_ops.chdir)
        return s_tdc_boot_file_ops.chdir(s_tdc_boot_file_ops.context,name);
    return f_chdir(name);
}
int tdc_ut_boot_read_override_active(void) { return s_tdc_boot_file_hook.read != NULL; }
void tdc_ut_boot_nvm_hook_get(tdc_ut_boot_nvm_hook_t *hook) { *hook = s_tdc_boot_nvm_hook; }
void tdc_ut_boot_nvm_hook_set(const tdc_ut_boot_nvm_hook_t *hook)
{
    if (hook) s_tdc_boot_nvm_hook = *hook;
    else memset(&s_tdc_boot_nvm_hook, 0, sizeof(s_tdc_boot_nvm_hook));
}
int tdc_ut_boot_nvm_read(uint32_t offset, uint32_t bytes, uint32_t *out)
{
    if (s_tdc_boot_nvm_hook.read)
        return s_tdc_boot_nvm_hook.read(s_tdc_boot_nvm_hook.context, offset, bytes, out);
    return NVMReadBytes(offset, bytes, out);
}

void tdc_ut_boot_set_public_key(uint32_t *key) { s_tdc_boot_public_key = key; }
uint32_t *tdc_ut_boot_public_key(void) { return s_tdc_boot_public_key; }

/* 메타데이터와 기대값을 별도 보관한다. 실제값을 기대값으로 복사하지 않는다. */
void tdc_ut_boot_record(unsigned unit, unsigned vector, const int *actual,
                        const int *expected, unsigned length)
{
    unsigned pos = g_tdc_ut_boot_count;
    if (length > TDC_UT_BOOT_CAPACITY - 4U || pos > TDC_UT_BOOT_CAPACITY - 4U - length)
    {
        g_tdc_ut_boot_overflow = 1;
        return;
    }
    g_tdc_ut_boot_actual[pos] = g_tdc_ut_boot_expected[pos] = -999;
    g_tdc_ut_boot_actual[pos + 1] = g_tdc_ut_boot_expected[pos + 1] = (int) unit;
    g_tdc_ut_boot_actual[pos + 2] = g_tdc_ut_boot_expected[pos + 2] = (int) vector;
    g_tdc_ut_boot_actual[pos + 3] = g_tdc_ut_boot_expected[pos + 3] = (int) length;
    for (unsigned i = 0; i < length; ++i)
    {
        g_tdc_ut_boot_actual[pos + 4 + i] = actual[i];
        g_tdc_ut_boot_expected[pos + 4 + i] = expected[i];
    }
    g_tdc_ut_boot_count += 4U + length;
}

static void run_name(void)
{
    /* 설정: 로컬 구조만 사용. 입력: 정상/빈 이름/빈 확장자/null.
     * 출력: 길이와 8문자. 복원: 제품 전역과 저장장치를 사용하지 않는다. */
    bootloader_boot_information info = {0};
    char name[20] = {0};
    int actual[9], expected[9] = {8, 't','e','s','t','.','a','p','p'};
    memcpy(info.boot_app_load, "test", 5);
    memcpy(info.boot_app_ext, "app", 4);
    actual[0] = get_app_load_ext(&info, name);
    for (unsigned i = 0; i < 8; ++i) actual[i + 1] = name[i];
    tdc_ut_boot_record(9, 1, actual, expected, 9);
    expected[0] = -1;
    info.boot_app_load[0] = 0;
    actual[0] = get_app_load_ext(&info, name);
    tdc_ut_boot_record(9, 2, actual, expected, 1);
    memcpy(info.boot_app_load, "test", 5);
    info.boot_app_ext[0] = 0;
    actual[0] = get_app_load_ext(&info, name);
    tdc_ut_boot_record(9, 3, actual, expected, 1);
    actual[0] = get_app_load_ext(NULL, name);
    tdc_ut_boot_record(9, 4, actual, expected, 1);
    actual[0] = get_app_load_ext(&info, NULL);
    tdc_ut_boot_record(9, 5, actual, expected, 1);
}

static void run_aes(void)
{
    /* 설정/입력: NIST SP 800-38A F.1의 공개 키와 4워드 암호문.
     * 출력: 반환값/복호화 워드와 len=3의 원본 보존. 복원: 모두 로컬이다. */
    uint32_t key[4] = {0x2b7e1516U,0x28aed2a6U,0xabf71588U,0x09cf4f3cU};
    uint32_t data[4] = {0x3ad77bb4U,0x0d7a3660U,0xa89ecaf3U,0x2466ef97U};
    uint32_t short_data[4];
    struct AES_ctx ctx;
    int actual[6];
    const int expected[6] = {0,0,(int)0x6bc1bee2U,(int)0x2e409f96U,
                                (int)0xe93d7e11U,(int)0x7393172aU};
    memcpy(short_data, data, sizeof(data));
    actual[0] = bootloader_initialize_aes_ctx(&ctx, key);
    actual[1] = bootloader_decrypt_data(&ctx, data, 4);
    for (unsigned i = 0; i < 4; ++i) actual[i + 2] = (int)data[i];
    tdc_ut_boot_record(14, 1, actual, expected, 6);
    const int short_expected[2] = {-1,1};
    actual[0] = bootloader_decrypt_data(&ctx, short_data, 3);
    const uint32_t cipher[4] = {0x3ad77bb4U,0x0d7a3660U,0xa89ecaf3U,0x2466ef97U};
    actual[1] = memcmp(short_data, cipher, sizeof(cipher)) == 0;
    tdc_ut_boot_record(14, 2, actual, short_expected, 2);
    const int null_expected[1] = {-1};
    actual[0] = bootloader_initialize_aes_ctx(NULL, key);
    tdc_ut_boot_record(14, 3, actual, null_expected, 1);
    actual[0] = bootloader_initialize_aes_ctx(&ctx, NULL);
    tdc_ut_boot_record(14, 4, actual, null_expected, 1);
}

typedef struct {
    const uint8_t *data;
    UINT bytes, position;
    int fail;
    unsigned reads, fail_on_read;
} tdc_ut_boot_ram_file_t;

static FRESULT read_ram(void *context, FIL *file, void *out, UINT bytes, UINT *read)
{
    tdc_ut_boot_ram_file_t *ram = context;
    (void)file;
    *read = 0;
    ++ram->reads;
    if (ram->fail || ram->reads == ram->fail_on_read) return FR_DISK_ERR;
    if (ram->position > ram->bytes) return FR_DISK_ERR;
    UINT remaining = ram->bytes - ram->position;
    if (bytes > remaining) bytes = remaining;
    memcpy(out, ram->data + ram->position, bytes);
    ram->position += bytes;
    *read = bytes;
    return FR_OK;
}

static uint16_t crc_reference(const void *data, unsigned length)
{
    /* fixture 작성용 독립 CCITT 계산. 시험 대상 bootloader_CRC_calc는 별도다. */
    uint16_t crc = 0xffff;
    const uint8_t *bytes = data;
    for (unsigned i = 0; i < length; ++i)
    {
        crc ^= (uint16_t)(bytes[i] << 8);
        for (unsigned j = 0; j < 8; ++j)
            crc = (uint16_t)((crc & 0x8000U) ? (((uint32_t)crc << 1) ^ 0x1021U) : (uint32_t)crc << 1);
    }
    return crc;
}

static void seal_ram_header(uint8_t *bytes, unsigned length)
{
    uint16_t crc = crc_reference(bytes, length - 2);
    bytes[length - 2] = (uint8_t)(crc >> 8);
    bytes[length - 1] = (uint8_t)crc;
}

static void record_one(unsigned unit, unsigned vector, int actual, int expected)
{ tdc_ut_boot_record(unit, vector, &actual, &expected, 1); }

typedef struct {
    uint8_t data[512];
    int error;
    unsigned reads;
} tdc_ut_boot_ram_nvm_t;

static int read_nvm_ram(void *context, uint32_t offset, uint32_t bytes, uint32_t *out)
{
    tdc_ut_boot_ram_nvm_t *ram = context;
    ++ram->reads;
    if (ram->error) return ram->error;
    if (offset != 512U || bytes > sizeof(ram->data)) return -1;
    memcpy(out, ram->data, bytes);
    return 0;
}

static void run_ram_boot_info(void)
{
    /* 512바이트 읽기와 실제 CRC 검사를 실행한다. 보정 적용, 전원/클럭,
     * 실제 제조 정보에는 접근하지 않는다. 읽기 경계와 CRC 상태를 복원한다. */
    tdc_ut_boot_ram_nvm_t ram = {{0},0,0};
    bootloader_boot_information info = {0};
    info.boot_addr = 0x12345678U; info.boot_manu_off = 1024; info.boot_manu_len = 64;
    info.boot_speed = BOOT_SPEED_SSPI;
    memcpy(info.boot_app_load,"A",2); memcpy(info.boot_app_ext,"FEZ",4);
    info.boot_info_crc = crc_reference(&info,sizeof(info)-2U);
    memcpy(ram.data,&info,sizeof(info));
    tdc_ut_boot_nvm_hook_t saved, hook = {read_nvm_ram,&ram};
    tdc_ut_boot_nvm_hook_get(&saved); tdc_ut_boot_nvm_hook_set(&hook);
    uint32_t out[128] = {0};
    int actual[6]; const int expected[6] = {0,32,0x12345678,1024,64,1};
    actual[0]=bootloader_read_nvm_boot_info(out);
    bootloader_boot_information result; memcpy(&result,out,sizeof(result));
    actual[1]=(int)sizeof(result); actual[2]=(int)result.boot_addr;
    actual[3]=result.boot_manu_off; actual[4]=result.boot_manu_len; actual[5]=(int)ram.reads;
    tdc_ut_boot_record(8,101,actual,expected,6);
    ram.data[0]^=1U;
    record_one(8,102,bootloader_read_nvm_boot_info(out),-1);
    ram.error=-7;
    record_one(8,103,bootloader_read_nvm_boot_info(out),-7);
    record_one(8,104,bootloader_read_nvm_boot_info(NULL),-1);
    /* NULL은 읽기 없이 거부하고, 실패한 입력 뒤 정상 읽기도 회복되어야 한다. */
    record_one(8,105,(int)ram.reads,3);
    ram.error=0; ram.data[0]^=1U;
    actual[0]=bootloader_read_nvm_boot_info(out); actual[1]=memcmp(out,ram.data,sizeof(out))==0;
    static const int recovered_expected[2]={0,1};
    tdc_ut_boot_record(8,106,actual,recovered_expected,2);
    tdc_ut_boot_nvm_hook_set(&saved);
}

static void run_ram_header(void)
{
    /* 설정: f_read만 RAM으로 교체. 입력: 정상/짧음/오류/CRC 손상 헤더.
     * 출력: 실제 header parser 반환. 복원: read hook과 CRC 상태를 되돌린다.
     * 전체 load_app_file의 열기/기동 시험은 HOST_ONLY다. */
    uint32_t bytes[3] = {APPFILE_IDENT_APP,0,0}, output[8];
    seal_ram_header((uint8_t *)bytes,8);
    tdc_ut_boot_ram_file_t ram = {(const uint8_t *)bytes,8,0,0,0,0};
    tdc_ut_boot_file_hook_t saved, hook = {read_ram,&ram};
    tdc_ut_boot_file_hook_get(&saved); tdc_ut_boot_file_hook_set(&hook);
    FIL *fp = ram_file_reset();
    record_one(10,101,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),0);
    ram.position=0; ram.bytes=3;
    record_one(10,102,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    ram.position=0; ram.bytes=8; ram.fail=1;
    record_one(10,103,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    ram.fail=0; bytes[0]^=1;
    record_one(10,104,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    /* 공개 키로 암호화한 한 블록을 읽고 실제 AES/CRC 경로를 확인한다.
     * CRC는 암호화 전 자료에 대한 독립 CCITT 값이다. 제품 출력은 쓰지 않는다. */
    uint32_t key[4]={0x2b7e1516,0x28aed2a6,0xabf71588,0x09cf4f3c};
    uint32_t encrypted[5]={APPFILE_IDENT_APPENCRYPTED,0,0,0,0};
    seal_ram_header((uint8_t *)encrypted,8);
    struct AES_ctx ctx; bootloader_initialize_aes_ctx(&ctx,key);
    for(unsigned i=1;i<5;++i)encrypted[i]=htonl(encrypted[i]);
    AES_ECB_encrypt(&ctx,(uint8_t *)(encrypted+1));
    for(unsigned i=1;i<5;++i)encrypted[i]=htonl(encrypted[i]);
    uint32_t *saved_key=tdc_ut_boot_public_key(); tdc_ut_boot_set_public_key(key);
    ram.data=(const uint8_t *)encrypted; ram.bytes=sizeof(encrypted); ram.position=0;
    int actual[3]; const int expected[3]={0,(int)APPFILE_IDENT_APPENCRYPTED,0};
    actual[0]=bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output));
    struct application_file_header *parsed=(void *)output;
    actual[1]=(int)parsed->ident; actual[2]=parsed->crc_table_size;
    tdc_ut_boot_record(10,105,actual,expected,3);
    ram.position=0; ram.bytes=sizeof(encrypted)-1;
    record_one(10,106,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    ram.position=0; ram.bytes=sizeof(encrypted); ram.fail=1;
    record_one(10,107,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    ram.fail=0; ram.position=0; ram.data=(const uint8_t *)bytes; ram.bytes=8;
    bytes[0]=APPFILE_IDENT_APP; bytes[1]=255;
    record_one(10,108,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    /* CRC 목록 1개 + 정렬 패딩: 필요한 전체 헤더 크기는 12바이트다. */
    bytes[1]=1; bytes[2]=0; seal_ram_header((uint8_t *)bytes,12);
    ram.bytes=12; ram.position=0;
    actual[0]=bootloader_read_and_check_file_header(fp,(void *)output,12);
    actual[1]=((struct application_file_header *)output)->crc_table_size;
    actual[2]=(int)ram.position;
    const int exact[3]={0,1,12}; tdc_ut_boot_record(10,109,actual,exact,3);
    memset(output,0xa5,sizeof(output)); ram.position=0;
    actual[0]=bootloader_read_and_check_file_header(fp,(void *)output,11);
    actual[1]=((uint8_t *)output)[11];
    const int bounded[2]={-1,0xa5}; tdc_ut_boot_record(10,110,actual,bounded,2);
    ram.position=0; ram.bytes=11;
    record_one(10,111,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    ram.position=0; ram.bytes=12; ram.reads=0; ram.fail_on_read=2;
    record_one(10,112,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),-1);
    ram.position=0; ram.fail_on_read=0;
    record_one(10,113,bootloader_read_and_check_file_header(fp,(void *)output,sizeof(output)),0);
    tdc_ut_boot_set_public_key(saved_key);
    tdc_ut_boot_file_hook_set(&saved);
}

static void run_ram_sections(void)
{
    /* 설정: 파일 입력만 RAM으로. 입력: 부트 구획과 짧은 데이터.
     * 출력: 실제 부트 정보 추출. 코어 점프/전송/NVM은 호출하지 않는다. */
    const uint32_t payload[2] = {0x20007ff0,0x00012345};
    struct data_section_header header = {.size=2,.type=SEC_BOOT_CM3};
    struct boot_section_data boot={0};
    tdc_ut_boot_ram_file_t ram={(const uint8_t *)payload,8,0,0,0,0};
    tdc_ut_boot_file_hook_t saved,hook={read_ram,&ram};
    tdc_ut_boot_file_hook_get(&saved); tdc_ut_boot_file_hook_set(&hook);
    uint16_t crc=htons(crc_reference(payload,8));
    FIL *fp = ram_file_reset();
    const int expected[3]={0,0x20007ff0,0x00012345};
    int actual[3]; actual[0]=bootloader_load_boot_section(&header,fp,&boot,crc);
    actual[1]=(int)boot.stack_pointer; actual[2]=(int)boot.init_pointer;
    tdc_ut_boot_record(11,101,actual,expected,3);
    ram.position=0; ram.bytes=3;
    record_one(11,102,bootloader_load_boot_section(&header,fp,&boot,crc),-1);
    ram.position=0; ram.bytes=8;
    record_one(11,103,bootloader_load_boot_section(&header,fp,&boot,crc^1),-1);
    record_one(11,104,bootloader_load_boot_section(NULL,fp,&boot,crc),-1);
    /* 전송 섹션 대신 기동 정보 두 개를 사용한다. 실제 load_sections의
     * 순서/플래그/CRC를 보되 DMA, 메모리 적재, 코어 점프는 호출하지 않는다. */
    uint32_t section_data[12]={0};
    struct data_section_header first={.size=2,.type=SEC_BOOT_CFX};
    struct data_section_header second={.size=2,.type=SEC_BOOT_CM3};
    const uint32_t cfx_payload[2]={0,0x00023456};
    seal_ram_header((uint8_t *)&first,sizeof(first));
    seal_ram_header((uint8_t *)&second,sizeof(second));
    memcpy(section_data,&first,sizeof(first));
    memcpy((uint8_t *)section_data+16,cfx_payload,sizeof(cfx_payload));
    memcpy((uint8_t *)section_data+24,&second,sizeof(second));
    memcpy((uint8_t *)section_data+40,payload,sizeof(payload));
    uint32_t app_words[4]={0}; struct application_file_header *app=(void *)app_words;
    app->crc_table_size=2;
    app->crc_table[0]=htons(crc_reference(cfx_payload,sizeof(cfx_payload)));
    app->crc_table[1]=crc;
    struct boot_section_data cfx={0}, cm3={0};
    ram.data=(const uint8_t *)section_data; ram.bytes=sizeof(section_data); ram.position=0;
    const int loaded_expected[5]={3,0,0x00023456,0x20007ff0,0x00012345};
    int loaded[5]; loaded[0]=bootloader_load_sections(fp,app,&cfx,&cm3);
    loaded[1]=(int)cfx.stack_pointer; loaded[2]=(int)cfx.init_pointer;
    loaded[3]=(int)cm3.stack_pointer; loaded[4]=(int)cm3.init_pointer;
    tdc_ut_boot_record(11,105,loaded,loaded_expected,5);
    ram.position=0; section_data[0]^=1;
    record_one(11,106,bootloader_load_sections(fp,app,&cfx,&cm3),-1);
    section_data[0]^=1; ram.position=0; ram.bytes=16+3;
    record_one(11,107,bootloader_load_sections(fp,app,&cfx,&cm3),-1);
    /* 선언 구획이 있는데 파일은 비어 있다. 성공으로 반환하는 기존 결함을
     * 타깃에서도 확인할 수 있게 기대값 -1을 유지한다. */
    ram.position=0; ram.bytes=0;
    record_one(11,108,bootloader_load_sections(fp,app,&cfx,&cm3),-1);
    /* 읽기 오류는 성공 플래그가 되어서는 안 된다. 첫 헤더와 첫 구획 뒤
     * 두 위치의 오류 반환이 유실되는 기존 결함도 기대값 -1로 남긴다. */
    ram.bytes=sizeof(section_data); ram.position=0; ram.reads=0; ram.fail_on_read=1;
    record_one(11,109,bootloader_load_sections(fp,app,&cfx,&cm3),-1);
    ram.position=0; ram.reads=0; ram.fail_on_read=3;
    record_one(11,110,bootloader_load_sections(fp,app,&cfx,&cm3),-1);
    ram.position=0; ram.reads=0; ram.fail_on_read=0;
    actual[0]=bootloader_load_boot_section(&header,fp,NULL,crc); actual[1]=(int)ram.reads;
    const int null_expected[2]={-1,0}; tdc_ut_boot_record(11,111,actual,null_expected,2);
    /* 공개 키의 암호화된 기동 자료. 16바이트 AES 블록 전체가 CRC 입력이다. */
    uint32_t key[4]={0x2b7e1516,0x28aed2a6,0xabf71588,0x09cf4f3c};
    uint32_t encrypted[4]={0x20007ff0,0x00012345,0x11223344,0x55667788};
    uint16_t encrypted_crc=htons(crc_reference(encrypted,sizeof(encrypted)));
    struct AES_ctx ctx; bootloader_initialize_aes_ctx(&ctx,key);
    for(unsigned i=0;i<4;++i)encrypted[i]=htonl(encrypted[i]);
    AES_ECB_encrypt(&ctx,(uint8_t *)encrypted);
    for(unsigned i=0;i<4;++i)encrypted[i]=htonl(encrypted[i]);
    uint32_t *saved_key=tdc_ut_boot_public_key(); tdc_ut_boot_set_public_key(key);
    header.flags=APPDATA_FLAGS_ENCRYPTED;
    ram.data=(uint8_t *)encrypted; ram.bytes=sizeof(encrypted);
    actual[0]=bootloader_load_boot_section(&header,fp,&boot,encrypted_crc);
    actual[1]=(int)boot.stack_pointer; actual[2]=(int)boot.init_pointer;
    tdc_ut_boot_record(11,112,actual,expected,3);
    struct boot_section_data sentinel={0x654321,0x123456}; boot=sentinel;
    ram.position=0; ram.bytes=15;
    actual[0]=bootloader_load_boot_section(&header,fp,&boot,encrypted_crc);
    actual[1]=memcmp(&boot,&sentinel,sizeof(boot))==0;
    const int unchanged[2]={-1,1}; tdc_ut_boot_record(11,113,actual,unchanged,2);
    ram.position=0; ram.bytes=16;
    actual[0]=bootloader_load_boot_section(&header,fp,&boot,encrypted_crc^1);
    actual[1]=memcmp(&boot,&sentinel,sizeof(boot))==0;
    tdc_ut_boot_record(11,114,actual,unchanged,2);
    tdc_ut_boot_set_public_key(saved_key);
    tdc_ut_boot_file_hook_set(&saved);
}

typedef struct {
    FIL *manifest, *app;
    const char *list;
    unsigned position, app_position, manifest_open, app_open;
    unsigned opens, order[2], closed_reads, read_error, missing_b, nested, chdir_error;
    uint32_t app_header[2];
} tdc_ut_boot_manifest_t;

static FRESULT manifest_open(void *context, FIL *file, const char *name, unsigned mode)
{
    tdc_ut_boot_manifest_t *ram=context;
    if(mode!=(FA_OPEN_EXISTING|FA_READ) || ram->app_open) return FR_DISK_ERR;
    unsigned id=strcmp(name,"A.FEZ")==0?1:strcmp(name,"B.FEZ")==0?2:0;
    if(!id || (id==2 && ram->missing_b)) return FR_NO_FILE;
    ram->app=file; ram->app_open=1; ram->app_position=0;
    if(ram->opens<2)ram->order[ram->opens]=id;
    ++ram->opens;
    struct application_file_header *header=(void *)ram->app_header;
    memset(header,0,sizeof(ram->app_header));
    header->ident=ram->nested?APPFILE_IDENT_MANIFEST:APPFILE_IDENT_APP;
    seal_ram_header((uint8_t *)header,8);
    return FR_OK;
}
static FRESULT manifest_close(void *context, FIL *file)
{
    tdc_ut_boot_manifest_t *ram=context;
    if(file==ram->manifest && ram->manifest_open) { ram->manifest_open=0; return FR_OK; }
    if(file==ram->app && ram->app_open) { ram->app_open=0; return FR_OK; }
    return FR_INVALID_OBJECT;
}
static FRESULT manifest_chdir(void *context, const char *name)
{
    tdc_ut_boot_manifest_t *ram=context;
    /* 실제 작업 디렉터리를 바꾸지 않는다. 슬롯 경로만 확인한다. */
    if(ram->chdir_error || !name || name[0]!='/')return FR_NO_PATH;
    return FR_OK;
}
static FRESULT manifest_read(void *context, FIL *file, void *out, UINT bytes, UINT *read)
{
    tdc_ut_boot_manifest_t *ram=context;
    const uint8_t *source; unsigned *position; size_t length;
    *read=0;
    if(file==ram->manifest)
    {
        if(!ram->manifest_open) { ++ram->closed_reads; return FR_INVALID_OBJECT; }
        if(ram->read_error) return FR_DISK_ERR;
        source=(const uint8_t *)ram->list; length=strlen(ram->list); position=&ram->position;
    }
    else if(file==ram->app && ram->app_open)
    {
        source=(const uint8_t *)ram->app_header; length=8; position=&ram->app_position;
    }
    else return FR_INVALID_OBJECT;
    if(*position>length)return FR_DISK_ERR;
    if(bytes>length-*position)bytes=(UINT)(length-*position);
    memcpy(out,source+*position,bytes); *position+=(unsigned)bytes; *read=bytes;
    return FR_OK;
}

static void run_ram_manifest(void)
{
    /* 입력: RAM 목록 A/B. 출력: 실제 get_manifest_files의 분리/길이/null.
     * 빈 app 구획을 사용해 DMA/코어 점프 없이 목록 처리 순서를 실행한다. */
    char list[]="A.FEZ\nB.FEZ\n", name[16];
    int actual[3]; const int expected[3]={5,'A',0};
    actual[0]=get_manifest_files(list,name,sizeof(name)); actual[1]=name[0]; actual[2]=name[5];
    tdc_ut_boot_record(12,101,actual,expected,3);
    const int expected_b[3]={5,'B',0};
    actual[0]=get_manifest_files(list+6,name,sizeof(name)); actual[1]=name[0]; actual[2]=name[5];
    tdc_ut_boot_record(12,102,actual,expected_b,3);
    record_one(12,103,get_manifest_files(NULL,name,sizeof(name)),-1);
    record_one(12,104,get_manifest_files(list,NULL,sizeof(name)),-1);
    record_one(12,105,get_manifest_files(list,name,0),0);
    tdc_ut_boot_file_ops_t saved_ops, ops={.open=manifest_open,.close=manifest_close,
        .chdir=manifest_chdir};
    tdc_ut_boot_file_hook_t saved_read, read={manifest_read,NULL};
    tdc_ut_boot_file_ops_get(&saved_ops); tdc_ut_boot_file_hook_get(&saved_read);
    FIL *fp=ram_file_reset(); uint32_t header_words[3]={0};
    struct application_file_header *header=(void *)header_words;
    header->ident=APPFILE_IDENT_MANIFEST; header->crc_table_size=1;
    for(unsigned i=0;i<6;++i)
    {
        tdc_ut_boot_manifest_t ram={.manifest=fp,.list=list,.manifest_open=1};
        ram.read_error=i==1; ram.missing_b=i==2; ram.nested=i==4; ram.chdir_error=i==5;
        header->crc_table[0]=htons(crc_reference(list,strlen(list)));
        if(i==3)header->crc_table[0]^=1;
        ops.context=read.context=&ram;
        tdc_ut_boot_file_ops_set(&ops); tdc_ut_boot_file_hook_set(&read);
        int ret=bootloader_process_manifest(fp,header);
        if(i==0)
        {
            /* 마지막 닫힌 목록 재읽기는 현재 제품의 특성 기록이다.
             * 다중 블록 목록/파일 수명 계약이 검증됐다는 뜻이 아니다. */
            const int actual_full[6]={ret,(int)ram.opens,(int)ram.order[0],
                (int)ram.order[1],ram.app_open==0 && ram.manifest_open==0,(int)ram.closed_reads};
            const int expected_full[6]={0,2,1,2,1,1};
            tdc_ut_boot_record(12,106,actual_full,expected_full,6);
        }
        else
        {
            const int actual_error[2]={ret,ram.app_open==0};
            const int expected_error[2]={-1,1};
            tdc_ut_boot_record(12,106+i,actual_error,expected_error,2);
        }
        /* read 실패 시 닫기 책임은 상위 호출자에 있다. RAM handle도 닫고 끝낸다. */
        if(ram.manifest_open)manifest_close(&ram,fp);
    }
    tdc_ut_boot_file_hook_set(&saved_read); tdc_ut_boot_file_ops_set(&saved_ops);
}

static void encrypt_ram_header(struct data_section_header *header, uint32_t *key)
{
    struct AES_ctx ctx; bootloader_initialize_aes_ctx(&ctx,key);
    uint32_t *words=(uint32_t *)header;
    for(unsigned i=0;i<4;++i)words[i]=htonl(words[i]);
    AES_ECB_encrypt(&ctx,(uint8_t *)words);
    for(unsigned i=0;i<4;++i)words[i]=htonl(words[i]);
}

static void run_ram_crc(void)
{
    /* 입력: 공개 키와 직접 계산한 header CRC. 출력: 실제 CRC/AES 판단.
     * 복원: 공개 키 override와 전경 CRC CFG/VALUE. 실제 제조 키를 읽지 않는다. */
    uint32_t key[4]={0x2b7e1516,0x28aed2a6,0xabf71588,0x09cf4f3c};
    uint32_t *saved=tdc_ut_boot_public_key(); tdc_ut_boot_set_public_key(key);
    struct data_section_header header={.size=4,.type=SEC_CFX_P};
    seal_ram_header((uint8_t *)&header,16);
    record_one(13,101,bootloader_check_header_crc(&header),0);
    record_one(13,102,bootloader_check_header_crc(NULL),-1);
    header.flags=APPDATA_FLAGS_ENCRYPTED; seal_ram_header((uint8_t *)&header,16);
    encrypt_ram_header(&header,key); record_one(13,103,bootloader_check_header_crc(&header),0);
    header.flags=0; seal_ram_header((uint8_t *)&header,16);
    encrypt_ram_header(&header,key); record_one(13,104,bootloader_check_header_crc(&header),-1);
    header.flags=APPDATA_FLAGS_ENCRYPTED; seal_ram_header((uint8_t *)&header,16);
    header.offset^=1; encrypt_ram_header(&header,key);
    record_one(13,105,bootloader_check_header_crc(&header),-1);
    const uint32_t bytes[2]={0x04030201,5};
    const unsigned lengths[5]={0,1,3,4,5};
    const int crc[5]={0xffff,0xf1d1,0xadad,0x89c3,0x9304};
    for(unsigned i=0;i<5;++i)
        record_one(13,106+i,(int)bootloader_CRC_calc((uint8_t *)bytes,lengths[i]),crc[i]);
    tdc_ut_boot_set_public_key(saved);
}

typedef struct {
    uint8_t bytes[sizeof(snd_boot_status_t)];
    FSIZE_t size, position;
    FIL *file;
    unsigned exists, opened, writes, syncs, read_fault_once;
} tdc_ut_boot_slot_file_t;

static FRESULT slot_open(void *context, FIL *file, const char *name, unsigned mode)
{
    tdc_ut_boot_slot_file_t *ram=context;
    if(strcmp(name,SND_BOOT_FILE_NAME)!=0 || ram->opened) return FR_DISK_ERR;
    if(!ram->exists && !(mode & FA_OPEN_ALWAYS)) return FR_NO_FILE;
    if(!ram->exists) { ram->exists=1; ram->size=0; }
    ram->file=file; ram->opened=1; ram->position=0;
    return FR_OK;
}
static FRESULT slot_close(void *context, FIL *file)
{
    tdc_ut_boot_slot_file_t *ram=context;
    if(!ram->opened || file!=ram->file) return FR_INVALID_OBJECT;
    ram->opened=0; return FR_OK;
}
static FRESULT slot_stat(void *context, const char *name, FILINFO *info)
{
    tdc_ut_boot_slot_file_t *ram=context;
    if(strcmp(name,SND_BOOT_FILE_NAME)!=0 || !ram->exists) return FR_NO_FILE;
    memset(info,0,sizeof(*info)); info->fsize=ram->size;
    const size_t length=strlen(SND_BOOT_FILE_NAME);
    if(length>=sizeof(info->fname)) return FR_DISK_ERR;
    memcpy(info->fname,SND_BOOT_FILE_NAME,length+1U);
    return FR_OK;
}
static FRESULT slot_read(void *context, FIL *file, void *out, UINT bytes, UINT *read)
{
    tdc_ut_boot_slot_file_t *ram=context;
    *read=0;
    if(!ram->opened || file!=ram->file) return FR_INVALID_OBJECT;
    if(ram->read_fault_once) { ram->read_fault_once=0; return FR_DISK_ERR; }
    if(ram->position>sizeof(ram->bytes) || ram->position>ram->size) return FR_DISK_ERR;
    FSIZE_t remaining=ram->size-ram->position;
    if(remaining>sizeof(ram->bytes)-ram->position)
        remaining=sizeof(ram->bytes)-ram->position;
    if(bytes>remaining)bytes=(UINT)remaining;
    memcpy(out,ram->bytes+(size_t)ram->position,bytes);
    ram->position+=bytes; *read=bytes; return FR_OK;
}
static FRESULT slot_write(void *context, FIL *file, const void *in, UINT bytes, UINT *written)
{
    tdc_ut_boot_slot_file_t *ram=context;
    *written=0;
    if(!ram->opened || file!=ram->file || ram->position>sizeof(ram->bytes) ||
       bytes>sizeof(ram->bytes)-(size_t)ram->position) return FR_DISK_ERR;
    memcpy(ram->bytes+(size_t)ram->position,in,bytes);
    ram->position+=bytes; if(ram->size<ram->position)ram->size=ram->position;
    *written=bytes; ++ram->writes; return FR_OK;
}
static FRESULT slot_seek(void *context, FIL *file, FSIZE_t offset)
{
    tdc_ut_boot_slot_file_t *ram=context;
    if(!ram->opened || file!=ram->file || offset>4096U) return FR_DISK_ERR;
    /* 4KB 크기 예약은 논리 길이만 기록한다. 실제 할당/저장은 하지 않는다. */
    ram->position=offset; if(ram->size<offset)ram->size=offset; return FR_OK;
}
static FRESULT slot_sync(void *context, FIL *file)
{
    tdc_ut_boot_slot_file_t *ram=context;
    if(!ram->opened || file!=ram->file) return FR_INVALID_OBJECT;
    ++ram->syncs; return FR_OK;
}

static void run_ram_slot(unsigned unit)
{
    /* 제품 파일은 열지 않는다. actual은 실제 snd_boot_handle_file로 만든
     * RAM 파일을 다시 읽은 값이며 기대값은 상태 규칙에서 별도로 구성한다.
     * 실패한 write/sync의 무한 대기는 여기서 주입하지 않는다. */
    snd_boot_status_t saved, input={0}, result;
    uint8_t saved_slot; FIL *saved_fp; FIL *fp=ram_file_reset();
    tdc_ut_boot_slot_snapshot(&saved,&saved_slot,&saved_fp);
    tdc_ut_boot_slot_file_t ram={0};
    tdc_ut_boot_file_ops_t saved_ops, ops={.open=slot_open,.close=slot_close,
        .stat=slot_stat,.write=slot_write,.seek=slot_seek,.sync=slot_sync,.context=&ram};
    tdc_ut_boot_file_hook_t saved_read, read={slot_read,&ram};
    tdc_ut_boot_file_ops_get(&saved_ops); tdc_ut_boot_file_hook_get(&saved_read);
    tdc_ut_boot_file_ops_set(&ops); tdc_ut_boot_file_hook_set(&read);
    snd_boot_set_fp(fp);
    if(unit==19)
    {
        for(unsigned i=0;i<4;++i)
        {
            memset(&ram,0,sizeof(ram));
            if(i==1) { ram.exists=1; ram.size=3; }
            if(i>=2)
            {
                memset(&input,0,sizeof(input)); input.state=SND_BOOT_STATE_BOOT;
                input.boot_slot_num=1; input.crc32=0xdeadbeefU;
                ram.exists=1; ram.size=sizeof(input); memcpy(ram.bytes,&input,sizeof(input));
                ram.read_fault_once=(i==2);
            }
            snd_boot_handle_file(); memcpy(&result,ram.bytes,sizeof(result));
            /* 실제 처리 뒤 다시 연 파일은 전부 닫혀야 한다. */
            int actual[6]={tdc_boot_get_slot_num(),result.state,result.boot_slot_num,
                (int)ram.writes,(int)ram.size,ram.opened==0};
            const int expected[6]={i==3?1:255,SND_BOOT_STATE_BOOT,i==3?1:255,
                i==3?0:1,i==3?(int)sizeof(input):4096,1};
            tdc_ut_boot_record(19,101+i,actual,expected,6);
        }
        /* CRC 필드는 현재 검사하지 않는다. 보존은 특성 기록이며 무결성 PASS가 아니다. */
        record_one(19,105,(int)result.crc32,(int)0xdeadbeefU);
        const unsigned lengths[5]={0,1,63,64,4096};
        for(unsigned i=0;i<5;++i)
        {
            memset(&ram,0,sizeof(ram)); memset(&input,0,sizeof(input));
            input.state=SND_BOOT_STATE_BOOT; input.boot_slot_num=4;
            ram.exists=1; ram.size=lengths[i]; memcpy(ram.bytes,&input,sizeof(input));
            snd_boot_handle_file(); memcpy(&result,ram.bytes,sizeof(result));
            snd_boot_status_t expected_record=input;
            if(i<3)
            {
                memset(&expected_record,0,sizeof(expected_record));
                expected_record.major_ver=SND_BOOT_VER_MAJOR; expected_record.minor_ver=SND_BOOT_VER_MINOR;
                expected_record.state=SND_BOOT_STATE_BOOT; expected_record.sub_state=SND_BOOT_SUB_STATE_IDLE;
                expected_record.boot_slot_num=255; expected_record.last_boot_slot_num=255;
                expected_record.alt_boot_result=SND_BOOT_ALT_BOOT_RESULT_NONE;
                expected_record.slot_1_state=expected_record.slot_2_state=SND_BOOT_SLOT_STATE_NONE;
                expected_record.slot_3_state=expected_record.slot_4_state=SND_BOOT_SLOT_STATE_NONE;
            }
            unsigned writes=ram.writes;
            snd_boot_handle_file();
            const int boundary[4]={tdc_boot_get_slot_num(),
                memcmp(&result,&expected_record,sizeof(result))==0 &&
                memcmp(ram.bytes,&expected_record,sizeof(result))==0,
                ram.writes==writes,ram.opened==0};
            const int expected_boundary[4]={i<3?255:4,1,1,1};
            tdc_ut_boot_record(19,106+i,boundary,expected_boundary,4);
        }
    }
    else
    {
        const unsigned state[7]={SND_BOOT_STATE_BOOT,SND_BOOT_STATE_ALT_BOOT,
            SND_BOOT_STATE_ALT_BOOT,SND_BOOT_STATE_ALT_BOOT,SND_BOOT_STATE_ALT_BOOT,
            SND_BOOT_STATE_ALT_BOOT,99};
        const unsigned sub[7]={SND_BOOT_SUB_STATE_IDLE,SND_BOOT_SUB_STATE_BOOT_TRY,
            SND_BOOT_SUB_STATE_BOOT_TRY,SND_BOOT_SUB_STATE_BOOT_TRY_DONE,
            SND_BOOT_SUB_STATE_BOOT_TRY_DONE,99,SND_BOOT_SUB_STATE_IDLE};
        const int slot[7]={1,2,1,2,1,1,1};
        const int tries[7]={0,3,4,0,0,0,0};
        const int final_sub[7]={SND_BOOT_SUB_STATE_IDLE,SND_BOOT_SUB_STATE_BOOT_TRY,
            SND_BOOT_SUB_STATE_BOOT_TRY_DONE,SND_BOOT_SUB_STATE_BOOT_TRY_DONE,
            SND_BOOT_SUB_STATE_UNKNOWN,SND_BOOT_SUB_STATE_UNKNOWN,SND_BOOT_SUB_STATE_UNKNOWN};
        for(unsigned i=0;i<7;++i)
        {
            memset(&ram,0,sizeof(ram)); memset(&input,0,sizeof(input));
            input.state=state[i]; input.sub_state=sub[i]; input.boot_slot_num=1;
            input.alt_boot_slot_num=2; input.alt_boot_try_count=i==1?2:i==2?3:0;
            input.alt_boot_result=i==3?SND_BOOT_ALT_BOOT_RESULT_SUCCESS:SND_BOOT_ALT_BOOT_RESULT_FAIL;
            ram.exists=1; ram.size=sizeof(input); memcpy(ram.bytes,&input,sizeof(input));
            snd_boot_handle_file(); memcpy(&result,ram.bytes,sizeof(result));
            const int actual[6]={tdc_boot_get_slot_num(),result.alt_boot_try_count,
                result.sub_state,result.state,result.alt_boot_result,ram.opened==0};
            const int expected[6]={slot[i],tries[i],final_sub[i],
                i==0||i==2?SND_BOOT_STATE_BOOT:i==6?99:SND_BOOT_STATE_ALT_BOOT,
                i==3?SND_BOOT_ALT_BOOT_RESULT_SUCCESS:SND_BOOT_ALT_BOOT_RESULT_FAIL,1};
            tdc_ut_boot_record(20,101+i,actual,expected,6);
        }
    }
    tdc_ut_boot_slot_restore(&saved,saved_slot,saved_fp);
    tdc_ut_boot_file_hook_set(&saved_read); tdc_ut_boot_file_ops_set(&saved_ops);
}

int tdc_ut_boot_run_unit(unsigned unit)
{
    int ret = 0;
    if (s_tdc_boot_pending) return TDC_UT_BOOT_BUSY;
    g_tdc_ut_boot_count = 0;
    g_tdc_ut_boot_done = 0;
    g_tdc_ut_boot_overflow = 0;
    /* 제품 scratch 내용을 전경 시험 동안 보관한다. 결과를 소비하기 전에
     * 다시 실행하지 못하므로 같은 보관 공간을 중첩해서 쓰지 않는다. */
    tdc_ut_boot_scratch_exchange(s_tdc_boot_scratch_backup);
    if (unit == 9) run_name();
    else if (unit == 14) run_aes();
    else if (unit == 8 || unit == 10 || unit == 11 || unit == 12 || unit == 13)
    {
        uint32_t crc_cfg=CRC->CFG, crc_value=CRC->VALUE;
        if(unit==8)run_ram_boot_info();
        else if(unit==10)run_ram_header();
        else if(unit==11)run_ram_sections();
        else if(unit==12)run_ram_manifest();
        else run_ram_crc();
        CRC->CFG=crc_cfg; CRC->VALUE=crc_value;
#if defined(TDC_UNIT_TEST_HOST) && !defined(TDC_UNIT_TEST_RAM_ONLY)
        ret=tdc_ut_boot_host_run_unit(unit);
#endif
    }
    else if(unit==19 || unit==20)
    {
        run_ram_slot(unit);
#if defined(TDC_UNIT_TEST_HOST) && !defined(TDC_UNIT_TEST_RAM_ONLY)
        ret=tdc_ut_boot_host_run_unit(unit);
#endif
    }
#if defined(TDC_UNIT_TEST_HOST) && !defined(TDC_UNIT_TEST_RAM_ONLY)
    else ret = tdc_ut_boot_host_run_unit(unit);
#else
    else ret = TDC_UT_BOOT_UNSUPPORTED;
#endif
    tdc_ut_boot_scratch_exchange(s_tdc_boot_scratch_backup);
    if (ret != 0) return ret;
    s_tdc_boot_pending = 1;
    g_tdc_ut_boot_done = 1;
    return g_tdc_ut_boot_overflow ? TDC_UT_BOOT_ERROR : 0;
}

void tdc_ut_boot_release(void) { s_tdc_boot_pending = 0; }
#endif
