/* 호스트 전용 확장. 실제 제품 함수의 파일 경계만 RAM으로 바꾼다.
 * CRC/AES 결과는 주입하며 암호화/영속성/보드 전압을 입증하지 않는다.
 * 기존 run_unit 복원 검사 뒤 별도로 실행한다. 함수 내 static 상태 복원을 주장하지 않는다.
 */
#if defined(TDC_UNIT_TEST) && defined(TDC_UT_IO_CM3_HOST) && !defined(TDC_UT_IO_CM3_RAM_ONLY)
#include "tdc_ut_io_cm3_host_fixture.h"
#include <tdc_fs_map.h>
#include <tdc_ble_mapping.h>
#include <tdc_ble_remote.h>
#include <tdc_hal_spi.h>
#include <tdc_isd_map_data.h>
#include <tdc_touch_iqs323.h>
#include <tdc_touch.h>
#include <tdc_sys_error.h>
#include <tdc_isd_map_impedance.h>
#include <tdc_isd_map_ecap.h>
#include <tdc_isd_map_specific_stim.h>
#include <tdc_isd_map_live.h>
#include <tdc_isd_init.h>
#include <tdc_fs_fft.h>
#include <tdc_fs_event_log.h>
#include <tdc_fs_stim_mute.h>
#include <tdc_ble_remote_sp_para.h>
#include <tdc_ble_general_debug.h>
#include <tdc_isd_stim_standalone.h>

extern ST__REMOTECONTROL_PACKET remoteDataPacket;
/* 구형 보드 헤더의 상충 매크로는 해당 wrapper TU 안으로 제한한다. */
bool write_REN_ISL91128_register_byCM3_I2C(int reg, int value);
bool read_REN_ISL91128_register_byCM3_I2C(int reg, int *value);
bool Reset_REN_ISL91128(void);
bool write_REN_ISL98608_register_byCM3_I2C(int reg, int value);
bool read_REN_ISL98608_register_byCM3_I2C(int reg, int *value);
bool Reset_REN_ISL98608(void);
bool Reset_disableOutup_REN_ISL98608(void);
bool enable_VP_REN_ISL98608(void);
bool disable_REN_ISL98608(void);

/* 이 시험의 입력이 자극 실행/실장 ID 읽기로 빠지면 즉시 실패한다. */
void tdc_isd_map_impedance_step(bool start) { (void)start; abort(); }
void tdc_isd_map_ecap_step(bool start) { (void)start; abort(); }
void tdc_isd_map_specific_stim_step(bool start) { (void)start; abort(); }
bool tdc_isd_map_live_step(ST__ISD_STATUS status) { (void)status; abort(); }
int tdc_isd_read_connected_id(void) { abort(); }
int tdc_fs_fft_read_pass_bin(int bands) { (void)bands; abort(); }
int tdc_fs_fft_init_pass_bin(int bands) { (void)bands; abort(); }
void tdc_isd_set_new_map_loaded_flag(void) { abort(); }
void tdc_hal_timer_update_reference_time(tdc_hal_timer_time_t *time, uint32_t count) { (void)time; (void)count; abort(); }
void tdc_fs_event_log_update_bt_addr(TDC_FS_EVENT_LOG_BT_ADDR_T *addr) { (void)addr; abort(); }
int tdc_fs_event_log_write(uint32_t type) { (void)type; abort(); }
void tdc_ble_remote_read_sp_para(bool start, int command) { (void)start; (void)command; abort(); }
char *readFirmwareInfo(void) { abort(); }
int tdc_fs_stim_mute_update(uint32_t enable, uint32_t level) { (void)enable; (void)level; abort(); }
void tdc_util_delay_ms(uint32_t milliseconds) { (void)milliseconds; abort(); }
int tdc_ble_general_debug_handle(const ST__REMOTECONTROL_PACKET *packet, int *tx, int index)
{ (void)packet; (void)tx; (void)index; abort(); }

static TDC_FS_ENTIRE_MAP_T s_tdc_map;
TDC_FS_ENTIRE_MAP_T *g_tdc_fs_ptr_entire_map = &s_tdc_map;
typedef struct { char name[32]; unsigned char data[1024]; int length; bool crc, aes; } tdc_ut_io_cm3_file_t;
static tdc_ut_io_cm3_file_t s_tdc_files[32];
static int s_tdc_read_fault, s_tdc_write_fault;
static int s_tdc_unit, s_tdc_case, s_tdc_checks, s_tdc_failures, s_tdc_known;

static tdc_ut_io_cm3_file_t *file_at(const char *name, bool create)
{
    for (unsigned i = 0; i < 32; i++)
        if (strcmp(s_tdc_files[i].name, name) == 0) return &s_tdc_files[i];
    if (create)
        for (unsigned i = 0; i < 32; i++)
            if (s_tdc_files[i].name[0] == 0)
            {
                assert(strlen(name) < sizeof(s_tdc_files[i].name));
                strcpy(s_tdc_files[i].name, name);
                return &s_tdc_files[i];
            }
    return NULL;
}
int tdc_ut_io_cm3_store_read(const char *name, void *data, int count, bool crc, bool aes)
{
    tdc_ut_io_cm3_file_t *file = file_at(name, false);
    if (!file || s_tdc_read_fault == 1) return -1;
    assert(count == file->length && crc == file->crc && aes == file->aes);
    int copied = count - (s_tdc_read_fault == 2 ? 1 : 0);
    memcpy(data, file->data, (size_t)copied);
    return copied;
}
int tdc_ut_io_cm3_store_write(const char *name, const void *data, int count, bool crc, bool aes)
{
    if (s_tdc_write_fault) return -1;
    tdc_ut_io_cm3_file_t *file = file_at(name, true);
    assert(file && count > 0 && count <= (int)sizeof(file->data));
    memcpy(file->data, data, (size_t)count);
    file->length = count; file->crc = crc; file->aes = aes;
    return count;
}

static void check(const char *field, int actual, int expected)
{
    s_tdc_checks++;
    if (actual != expected)
    {
        s_tdc_failures++;
        fprintf(stderr, "CM3 EXT unit=%d case=%d %s actual=%d expected=%d\n", s_tdc_unit, s_tdc_case, field, actual, expected);
    }
}
/* 알려진 실패의 actual도 고정 확인한다. 변경/새 실패는 모드와 무관하게 차단한다. */
static void known(const char *field, int actual, int expected, int known_actual)
{
    check(field, actual, expected);
#if defined(TDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS)
    if (actual != known_actual)
        fprintf(stderr,"KNOWN BASELINE CHANGED unit=%d case=%d field=%s actual=%d previous=%d contract_expected=%d\n",s_tdc_unit,s_tdc_case,field,actual,known_actual,expected);
    assert(actual == known_actual && known_actual != expected);
#else
    (void)known_actual;
#endif
    if (actual != expected) s_tdc_known++;
}
static void begin(int unit)
{
    s_tdc_unit = unit; s_tdc_case = s_tdc_checks = s_tdc_failures = s_tdc_known = 0;
    tdc_ut_io_cm3_host_setup();
}
static int finish(void)
{
    assert(g_tdc_ut_io_cm3_reset_count == 0);
    printf("CM3 EXT unit=%d cases=%d comparisons=%d mismatches=%d known=%d HOST_ONLY\n",
           s_tdc_unit, s_tdc_case, s_tdc_checks, s_tdc_failures, s_tdc_known);
#if defined(TDC_UT_IO_CM3_CHECK_KNOWN_FINDINGS)
    assert(s_tdc_failures == s_tdc_known);
#endif
    tdc_ut_io_cm3_host_restore();
    return s_tdc_failures;
}
static void next(void) { s_tdc_case++; tdc_hal_spi_clear_master_read_command(); tdc_hal_spi_clear_tx_buffer(); }
static void response(int header, int tdc_sys_error)
{
    int tx[TDC_HAL_SPI_COMM_PACKET_SIZE];
    tdc_ut_io_cm3_spi_snapshot(tx, TDC_HAL_SPI_COMM_PACKET_SIZE);
    check("header", tx[0], header);
    if (header == en__ERROR_Response) check("tdc_sys_error", tx[3], tdc_sys_error);
}
static void bus_reset(int fault)
{
    tdc_hal_i2c_clear_driver_status(); tdc_ut_io_cm3_host_i2c_reset();
    g_tdc_ut_io_cm3_transfer_count = 0; g_tdc_ut_io_cm3_bus_error = fault;
    g_tdc_ut_io_cm3_bus_error_transfer = 0;
}
static int s_tdc_touch_trace[128][3], s_tdc_touch_writes, s_tdc_touch_reads, s_tdc_touch_fault;
static int s_tdc_touch_ati_active;
static int touch_bus(int read, unsigned char *data, int count)
{
    assert(count > 0 && count <= 4);
    if (read)
    {
        s_tdc_touch_reads++;
        memset(data,0,(size_t)count); /* ATI idle 응답, actual ATI 물리 완료 아님. */
        data[0]=s_tdc_touch_ati_active?0x20:0;
    }
    else
    {
        assert(s_tdc_touch_writes < 128);
        for(int i=0;i<3;i++) s_tdc_touch_trace[s_tdc_touch_writes][i]=i<count?data[i]:-1;
        s_tdc_touch_writes++;
    }
    return !s_tdc_touch_fault;
}
static int touch_initialization(void)
{
    begin(2);
    int (*saved)(int,unsigned char*,int)=g_tdc_ut_io_cm3_touch_bus_hook;
    g_tdc_ut_io_cm3_touch_bus_hook=touch_bus;
    const int required[][3]={{0x36,0x0C,0x04},{0xC0,0x54,0x07},{0xC0,0x58,0x07},
                            {0x62,TDC_TOUCH_IQS323_THRESHOLD,TDC_TOUCH_IQS323_HYSTERESIS<<4}};
    int first_trace[128][3], first_count=0;
    for(int repeat=0;repeat<2;repeat++)
    {
        next(); s_tdc_touch_fault=0; s_tdc_touch_writes=s_tdc_touch_reads=0;
        memset(s_tdc_touch_trace,0,sizeof(s_tdc_touch_trace)); tdc_touch_iqs323_set_ulp();
        tdc_touch_iqs323_apply_settings();
        for(unsigned cmd=0;cmd<sizeof(required)/sizeof(required[0]);cmd++)
        {
            int found=0;
            for(int i=0;i<s_tdc_touch_writes;i++) if(memcmp(required[cmd],s_tdc_touch_trace[i],sizeof(required[0]))==0) found++;
            check("required-init-command",found,1);
        }
        check("ULP-cleared-after-init",tdc_touch_iqs323_is_ulp(),0);
        if(repeat==0) { first_count=s_tdc_touch_writes; memcpy(first_trace,s_tdc_touch_trace,sizeof(first_trace)); }
        else { check("repeat-count",s_tdc_touch_writes,first_count); check("repeat-trace",memcmp(first_trace,s_tdc_touch_trace,sizeof(first_trace))==0,1); }
    }
    next(); s_tdc_touch_fault=1; check("ReATI-write-failure",tdc_touch_iqs323_re_ati(),0);
    next(); check("reseed-write-failure",tdc_touch_iqs323_reseed(),0);
    next(); check("failed-read-not-ATI-done",tdc_touch_iqs323_is_ati_done(),0);
    next(); s_tdc_touch_writes=s_tdc_touch_reads=0;
    tdc_touch_iqs323_apply_settings();
    /* void API와 초기화 실패 후 상태 정책은 미정이다. 완료/안전 합격으로 판정하지 않는다. */
    printf("CM3 OBS unit=2 case=%d all-bus-errors writes=%d reads=%d ulp=%d NOT_PRODUCT_PASS\n",
           s_tdc_case,s_tdc_touch_writes,s_tdc_touch_reads,tdc_touch_iqs323_is_ulp());
    next(); s_tdc_touch_fault=0; s_tdc_touch_writes=s_tdc_touch_reads=0;
    check("before-begin-no-event",tdc_touch_process(),0); check("before-begin-not-ready",tdc_ut_io_cm3_touch_ready(),0);
    check("before-begin-no-I2C",s_tdc_touch_writes+s_tdc_touch_reads,0);
    next(); tdc_touch_init_begin(); s_tdc_touch_ati_active=1;
    check("ATI-active-no-event",tdc_touch_process(),0); check("ATI-active-not-ready",tdc_ut_io_cm3_touch_ready(),0);
    next(); s_tdc_touch_fault=1; check("read-failed-no-event",tdc_touch_process(),0); check("read-failed-before-timeout-not-ready",tdc_ut_io_cm3_touch_ready(),0);
    next(); s_tdc_touch_fault=0; s_tdc_touch_ati_active=0;
    check("initialization-no-long-touch-event",tdc_touch_process(),0); check("ready-after-settings",tdc_ut_io_cm3_touch_ready(),1);
    next(); tdc_touch_init_begin(); check("restart-not-ready",tdc_ut_io_cm3_touch_ready(),0);
    tdc_touch_process(); check("restart-ready",tdc_ut_io_cm3_touch_ready(),1);
    next(); tdc_touch_init_begin(); s_tdc_touch_fault=1; s_tdc_touch_writes=s_tdc_touch_reads=0;
    tdc_ut_io_cm3_host_advance(TDC_TOUCH_INIT_TIMEOUT_MS+1); tdc_touch_process();
    printf("CM3 OBS unit=2 case=%d timeout-with-all-bus-errors ready=%d writes=%d NOT_PRODUCT_PASS\n",
           s_tdc_case,tdc_ut_io_cm3_touch_ready(),s_tdc_touch_writes);
    s_tdc_touch_fault=0;
    g_tdc_ut_io_cm3_touch_bus_hook=saved;
    return finish();
}
static int legacy_pmic(int unit)
{
    begin(unit);
    int value, rx = 0x5A;
    bool (*write_reg)(int,int) = unit == 38 ? write_REN_ISL91128_register_byCM3_I2C : write_REN_ISL98608_register_byCM3_I2C;
    bool (*read_reg)(int,int*) = unit == 38 ? read_REN_ISL91128_register_byCM3_I2C : read_REN_ISL98608_register_byCM3_I2C;
    next(); bus_reset(0);
    check("write", write_reg(1, 0xA5), 1);
    check("address", g_tdc_ut_io_cm3_transfer[0].addr, unit == 38 ? 0x1C : 0x29);
    check("register", g_tdc_ut_io_cm3_transfer[0].data[0], 1);
    check("data", g_tdc_ut_io_cm3_transfer[0].data[1], 0xA5);
    next(); bus_reset(0); tdc_ut_io_cm3_host_rx(&rx, 1);
    check("read", read_reg(1, &value), 1); check("read-data", value, rx);
    check("read-address", g_tdc_ut_io_cm3_transfer[1].addr, unit == 38 ? 0x1C : 0x29);
    next(); bus_reset(1); check("write-tdc_sys_error", write_reg(1, 0xA5), 0); check("idle-after-tdc_sys_error", tdc_hal_i2c_is_driver_status_idle(), 1);
    next(); bus_reset(1); value = 123; check("read-tdc_sys_error", read_reg(1, &value), 0); check("output-unwritten-observation", value, 123);
    if (unit == 38)
    {
        for (int match = 0; match < 2; match++)
        {
            next(); bus_reset(0); rx = match ? 0x81 : 0x80; tdc_ut_io_cm3_host_rx(&rx, 1);
            check("reset-readback", Reset_REN_ISL91128(), match);
            check("reset-register", g_tdc_ut_io_cm3_transfer[0].data[0], 0);
            check("reset-value", g_tdc_ut_io_cm3_transfer[0].data[1], 0x81);
        }
        /* 실패한 reset read의 미초기화 readValue는 실행하지 않는다. */
    }
    else
    {
        bool (*operations[])(void) = {enable_VP_REN_ISL98608, disable_REN_ISL98608, Reset_disableOutup_REN_ISL98608, Reset_REN_ISL98608};
        const int count[] = {1,1,2,2}, regs[] = {5,5,5,4}, values[] = {0x25,0x05,0x20,0x80};
        for (int i = 0; i < 4; i++)
        {
            next(); bus_reset(0); check("operation", operations[i](), 1);
            check("write-count", g_tdc_ut_io_cm3_transfer_count, count[i]);
            check("last-register", g_tdc_ut_io_cm3_transfer[count[i]-1].data[0], regs[i]);
            check("raw-write-observation", g_tdc_ut_io_cm3_transfer[count[i]-1].data[1], values[i]);
            next(); bus_reset(1); known("failed-I2C-must-not-report-success", operations[i](), 0, 1);
        }
    }
    next(); bus_reset(0); g_tdc_ut_io_cm3_bus_error_transfer=2; value=123;
    check("read-data-phase-tdc_sys_error",read_reg(1,&value),0); check("read-output-unwritten-observation",value,123);
    check("read-tdc_sys_error-idle",tdc_hal_i2c_is_driver_status_idle(),1);
    return finish();
}

static void seed_maps(void)
{
    memset(s_tdc_files, 0, sizeof(s_tdc_files));
    memset(&s_tdc_map, 0, sizeof(s_tdc_map)); s_tdc_read_fault = s_tdc_write_fault = 0;
    for (int slot = 1; slot <= MaxNumUser; slot++)
    {
        for (int map = 1; map <= MaxNumMap; map++)
        {
            ST__CFX_CM3_SharedMemory_mapData *data = &s_tdc_map.map[slot-1].map_data[map-1];
            data->mappingDate.mapping_year = slot * 10 + map;
            for (int i = 0; i < 32; i++) { data->T_level_uA[i] = 100*slot + 10*map + i; data->C_level_uA[i] = 500*slot + 10*map + i; }
            assert(tdc_fs_map_write_map_data(slot,map) == sizeof(*data));
        }
        s_tdc_map.map[slot-1].isd_info.isd_serial = 1000 + slot;
        s_tdc_map.map[slot-1].user_setting_value.mapNum = slot;
        assert(tdc_fs_map_write_isd_info(slot) > 0 && tdc_fs_map_write_user_setting_value(slot) > 0 && tdc_fs_map_write_map_stamp(slot) > 0);
    }
}
static int storage(void)
{
    begin(42); seed_maps();
    const int pairs[][2] = {{1,1},{2,1},{1,2},{4,4}};
    for (unsigned p = 0; p < 4; p++)
    {
        int slot=pairs[p][0], map=pairs[p][1];
        ST__CFX_CM3_SharedMemory_mapData *data=&s_tdc_map.map[slot-1].map_data[map-1];
        next(); memset(data, 0, sizeof(*data)); check("read-count",tdc_fs_map_read_map_data(slot,map),sizeof(*data));
        ST__CFX_CM3_SharedMemory_ReadWriteCommand_ForFlash command={flash_Command_Read,slot,map};
        tdc_shm_set_read_write_map_data_flash_command(command);
        for(int i=0;i<32;i++)
        {
            check("T-element",cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.readWritemapData.T_level_uA[i],100*slot+10*map+i);
            check("C-element",cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.readWritemapData.C_level_uA[i],500*slot+10*map+i);
        }
    }
    next(); check("info-length",tdc_fs_map_read_isd_info(2),sizeof(ST__CFX_CM3_SharedMemory_ISD_info));
    check("info-slot",s_tdc_map.map[1].isd_info.isd_serial,1002);
    check("info-CRC-request",file_at("/ISD2_INFO",false)->crc,1); check("info-AES-request",file_at("/ISD2_INFO",false)->aes,1);
    next(); s_tdc_read_fault=1; check("read-tdc_sys_error-or-rejected-CRC",tdc_fs_map_read_map_data(1,1),-1);
    next(); s_tdc_read_fault=2; check("short-read-propagated",tdc_fs_map_read_map_data(1,1),sizeof(ST__CFX_CM3_SharedMemory_mapData)-1);
    s_tdc_read_fault=0;
    next(); s_tdc_write_fault=1; check("write-tdc_sys_error",tdc_fs_map_write_map_data(1,1),-1);
    s_tdc_write_fault=0;
    next();
    ST__CFX_CM3_SharedMemory_mapData *repository=&cfx_cm3_sharedMemoryAll.repositoryForReadWriteMapData.readWritemapData;
    for(int i=0;i<32;i++) { repository->T_level_uA[i]=300+i; repository->C_level_uA[i]=800+i; }
    tdc_isd_map_write_stim_para(true,0x6D,2,2); tdc_isd_map_write_stim_para(false,0x6D,2,2); response(0x6D,0);
    memset(&s_tdc_map.map[1].map_data[1],0,sizeof(*repository));
    check("write-reload",tdc_fs_map_read_map_data(2,2),sizeof(*repository));
    for(int i=0;i<32;i++)
    {
        check("stored-T",s_tdc_map.map[1].map_data[1].T_level_uA[i],300+i);
        check("stored-C",s_tdc_map.map[1].map_data[1].C_level_uA[i],800+i);
        check("unselected-map-kept",s_tdc_map.map[0].map_data[1].T_level_uA[i],120+i);
    }
    next(); s_tdc_write_fault=1;
    tdc_isd_map_write_stim_para(true,0x6D,2,2); tdc_isd_map_write_stim_para(false,0x6D,2,2);
    int tx[TDC_HAL_SPI_COMM_PACKET_SIZE]; tdc_ut_io_cm3_spi_snapshot(tx,TDC_HAL_SPI_COMM_PACKET_SIZE);
    known("failed-save-normal-ACK",tx[0],en__ERROR_Response,0x6D);
    s_tdc_write_fault=0;
    return finish();
}
static int erase_scope(void)
{
    begin(43); seed_maps();
    next(); tdc_isd_map_reset_nvm_map_data(true,0x6F,1,1,flash_Command_Erase); tdc_isd_map_reset_nvm_map_data(false,0x6F,1,1,flash_Command_Erase);
    check("target-erased",s_tdc_map.map[0].map_data[0].T_level_uA[31],0);
    check("other-map-kept",s_tdc_map.map[0].map_data[1].T_level_uA[31],151);
    check("other-slot-kept",s_tdc_map.map[1].map_data[0].T_level_uA[31],241); response(0x6F,0);
    next(); seed_maps(); tdc_isd_map_reset_nvm_selected(true,0x6E,2,flash_Command_Erase); tdc_isd_map_reset_nvm_selected(false,0x6E,2,flash_Command_Erase);
    for(int map=0;map<4;map++) check("selected-slot-map-erased",s_tdc_map.map[1].map_data[map].T_level_uA[31],0);
    check("other-slot-kept",s_tdc_map.map[0].map_data[0].T_level_uA[31],141);
    check("selected-info-cleared",s_tdc_map.map[1].isd_info.isd_serial,0); response(0x6E,0);
    next(); tdc_isd_map_reset_nvm_map_data(true,0x6F,1,1,flash_Command_Read); response(en__ERROR_Response,en_sourceCodeError);
    next(); seed_maps();
    bool done=tdc_isd_map_reset_nvm_all(true,0x71,flash_Command_Erase);
    check("not-done-at-first-slot",done,0);
    for(int tick=0;tick<7;tick++) done=tdc_isd_map_reset_nvm_all(false,0x71,flash_Command_Erase);
    check("done-after-all-slots",done,1);
    for(int slot=0;slot<4;slot++) for(int map=0;map<4;map++)
        check("all-target-maps-erased",s_tdc_map.map[slot].map_data[map].T_level_uA[31],0);
    response(0x71,0);
    next(); seed_maps(); s_tdc_write_fault=1;
    tdc_isd_map_reset_nvm_map_data(true,0x6F,1,1,flash_Command_Erase); tdc_isd_map_reset_nvm_map_data(false,0x6F,1,1,flash_Command_Erase);
    int tx[TDC_HAL_SPI_COMM_PACKET_SIZE]; tdc_ut_io_cm3_spi_snapshot(tx,TDC_HAL_SPI_COMM_PACKET_SIZE);
    known("failed-save-normal-ACK",tx[0],en__ERROR_Response,0x6F);
    s_tdc_write_fault=0; memset(&s_tdc_map.map[0].map_data[0],0,sizeof(ST__CFX_CM3_SharedMemory_mapData));
    check("reload-old-file",tdc_fs_map_read_map_data(1,1),sizeof(ST__CFX_CM3_SharedMemory_mapData));
    check("failed-file-write-did-not-erase-store",s_tdc_map.map[0].map_data[0].T_level_uA[31],141);
    return finish();
}

static int remote_parser(void)
{
    begin(46); memset(&remoteDataPacket,0,sizeof(remoteDataPacket));
    int packet[BLE_DataPacketSize]={0};
    const int slots[]={1,4,0,5};
    for(int i=0;i<4;i++)
    {
        next(); tdc_ble_remote_clear_command(); packet[0]=0x4A; packet[1]=slots[i]; tdc_ble_remote_fetch_packet(packet);
        check("read-slot-command",tdc_ble_remote_get_command(),i<2?0x4A:en__remoteControl_IDLE);
        if(i<2) check("slot",remoteDataPacket.remocon_ReadWriteMapData_Flash.slot_index,slots[i]);
        else response(en__ERROR_Response,en__OutOfDataRange);
    }
    next(); tdc_ble_remote_clear_command(); packet[0]=0x4B; packet[1]=0; packet[2]=1; tdc_ble_remote_fetch_packet(packet);
    known("zero-slot-must-be-rejected-before-storage",tdc_ble_remote_get_command(),en__remoteControl_IDLE,0x4B);
    next(); tdc_ble_remote_clear_command(); packet[0]=0x4B; packet[1]=1; packet[2]=5; tdc_ble_remote_fetch_packet(packet);
    check("invalid-map-cleared",tdc_ble_remote_get_command(),en__remoteControl_IDLE); response(en__ERROR_Response,en__OutOfDataRange);
    next(); tdc_ble_remote_clear_command(); packet[0]=0x4A; packet[1]=1; tdc_ble_remote_fetch_packet(packet);
    packet[1]=2; tdc_ble_remote_fetch_packet(packet); response(en__ERROR_Response,en__PreviouCommnadIsNotCompleted);
    known("busy-command-retained",tdc_ble_remote_get_command(),0x4A,en__remoteControl_IDLE);
    next(); tdc_ble_remote_clear_command(); packet[0]=0x4D; packet[1]=3; tdc_ble_remote_fetch_packet(packet);
    check("out-of-order-cleared",tdc_ble_remote_get_command(),en__remoteControl_IDLE); response(en__ERROR_Response,en__DATA_Order);
    next(); tdc_ble_remote_clear_command(); tdc_ble_remote_clear_passkey_match(); packet[0]=0x49; packet[1]=1;
    tdc_ble_remote_fetch_packet(packet); remoteControl(true); response(en__ERROR_Response,en__NO_SECURITY);
    check("unauthorized-cleared",tdc_ble_remote_get_command(),en__remoteControl_IDLE);
    const int volume_cases[][4]={{0x45,1,1,2},{0x45,4,1,4},{0x45,2,2,1},{0x45,1,2,1},
                                    {0x46,1,1,2},{0x46,10,1,10},{0x46,2,2,1},{0x46,1,2,1}};
    for(unsigned i=0;i<sizeof(volume_cases)/sizeof(volume_cases[0]);i++)
    {
        next(); tdc_ble_remote_set_passkey_match();
        tdc_shm_change_stimul_volume(volume_cases[i][1]); tdc_shm_change_audio_volume(volume_cases[i][1]);
        packet[0]=volume_cases[i][0]; packet[1]=volume_cases[i][2]; tdc_ble_remote_fetch_packet(packet); remoteControl(true);
        int tx[TDC_HAL_SPI_COMM_PACKET_SIZE]; tdc_ut_io_cm3_spi_snapshot(tx,TDC_HAL_SPI_COMM_PACKET_SIZE);
        check("volume-header",tx[0],packet[0]); check("volume",tx[1],volume_cases[i][3]);
        check("volume-state",packet[0]==0x45?tdc_shm_read_stimul_volume():tdc_shm_read_audio_volume(),volume_cases[i][3]);
        check("volume-command-cleared",tdc_ble_remote_get_command(),en__remoteControl_IDLE);
    }
    next(); packet[0]=0x45; packet[1]=3; tdc_ble_remote_fetch_packet(packet); remoteControl(true);
    response(en__ERROR_Response,en__OutOfDataRange); check("invalid-volume-cleared",tdc_ble_remote_get_command(),en__remoteControl_IDLE);
    tdc_ble_remote_clear_command();
    tdc_ble_remote_clear_passkey_match();
    return finish();
}
static int mapping_parser(void)
{
    begin(49); int packet[BLE_DataPacketSize]={0}; ST__MAPPING_PACKET *state=tdc_ble_mapping_get_packet();
    const int cases[][3]={{0x6B,1,1},{0x6B,4,4},{0x6B,0,1},{0x6B,1,5},{0x6E,2,0},{0x6E,5,0},{0x6F,1,2},{0x6F,1,0}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
    {
        next(); memset(state,0,sizeof(*state)); memcpy(packet,cases[i],sizeof(cases[i])); tdc_ble_mapping_fetch_packet(packet);
        bool valid=i==0||i==1||i==4||i==6;
        if(i==2) known("zero-slot-must-be-rejected-before-storage",state->fetched_command,en__mapping_IDLE,0x6B);
        else check("fetched",state->fetched_command,valid?cases[i][0]:en__mapping_IDLE);
        if(valid) check("slot",state->ReadWriteMapData_Flash.slot_index,cases[i][1]);
        else if(i!=2) response(en__ERROR_Response,en__OutOfDataRange);
    }
    next(); memset(state,0,sizeof(*state)); state->command=0x62; packet[0]=0x67; tdc_ble_mapping_fetch_packet(packet);
    check("busy-retained",state->command,0x62); check("no-new-command",state->fetched_command,en__mapping_IDLE);
    int tx[TDC_HAL_SPI_COMM_PACKET_SIZE]; tdc_ut_io_cm3_spi_snapshot(tx,TDC_HAL_SPI_COMM_PACKET_SIZE);
    known("busy-tdc_sys_error-header-overwritten",tx[0],en__ERROR_Response,0);
    known("busy-tdc_sys_error-detail-overwritten",tx[3],en__PreviouCommnadIsNotCompleted,0);
    next(); packet[0]=0x61; tdc_ble_mapping_fetch_packet(packet); check("disconnect-exception-queued",state->fetched_command,0x61);
    next(); memset(state,0,sizeof(*state)); packet[0]=0x60; tdc_ble_mapping_fetch_packet(packet); check("connect-queued",state->fetched_command,0x60);
    next(); memset(state,0,sizeof(*state)); packet[0]=0x6D; packet[1]=3; tdc_ble_mapping_fetch_packet(packet);
    response(en__ERROR_Response,en__DATA_Order); check("out-of-order-cleared",state->command,en__mapping_IDLE);
    memset(state,0,sizeof(*state));
    return finish();
}
static int mapping_connection(void)
{
    begin(48); int packet[BLE_DataPacketSize]={0};
    ST__ISD_STATUS device={0}; ST__MAPPING_STATE state;
    memset(tdc_ble_mapping_get_packet(),0,sizeof(ST__MAPPING_PACKET));
    next(); state=mappingControl(device); check("initial-disconnected",state.mappingConnection,0);
    next(); packet[0]=0x67; tdc_ble_mapping_fetch_packet(packet); state=mappingControl(device);
    response(en__ERROR_Response,en__Command_Order); check("rejected-before-connect",state.mappingConnection,0);
    next(); packet[0]=0x60; tdc_ble_mapping_fetch_packet(packet); state=mappingControl(device);
    check("connected",state.mappingConnection,1); response(0x60,0); check("command-cleared",tdc_ble_mapping_get_command(),en__mapping_IDLE);
    next(); state=mappingControl(device); check("connected-idle",state.mappingConnection,1);
    next(); tdc_ble_mapping_change_command_ble_disconnected(); state=mappingControl(device);
    check("BLE-disconnected",state.mappingConnection,0); check("power-reset-request-only",state.isdControlCommand,en__isdStatus_PowerIC_Reset);
    check("command-cleared",tdc_ble_mapping_get_command(),en__mapping_IDLE); check("no-reset",g_tdc_ut_io_cm3_reset_count,0);
    next(); state=mappingControl(device); check("stays-disconnected",state.mappingConnection,0);
    /* 0x61은 watchdog reset 뒤 무한루프이므로 입력 분류만 다른 시험에서 확인한다. */
    return finish();
}
int tdc_ut_io_cm3_extended_run(void)
{
    int failed=0;
    failed+=touch_initialization();
    failed+=legacy_pmic(38); failed+=legacy_pmic(39); failed+=storage();
    failed+=erase_scope(); failed+=remote_parser(); failed+=mapping_parser(); failed+=mapping_connection();
    printf("CM3 EXT failed_comparisons=%d HOST_ONLY; no target restoration claim\n",failed);
    return failed;
}
#endif
