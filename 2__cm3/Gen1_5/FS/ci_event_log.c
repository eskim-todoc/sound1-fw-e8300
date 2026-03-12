#include <ci_event_log.h>
#include <ci_printf.h>
#include <error.h>

static CI_EVENT_LOG_BT_ADDR_T _ci_event_log_bt_addr = {0};

void ci_event_log_update_bt_addr(CI_EVENT_LOG_BT_ADDR_T *p_bt_addr)
{
    // 주소가 NULL인 경우 초기화
    if (p_bt_addr == NULL)
    {
        // memset(&_ci_event_log_bt_addr, 0, sizeof(CI_EVENT_LOG_BT_ADDR_T));
        _ci_event_log_bt_addr.bt_addr[0] = 0;
        _ci_event_log_bt_addr.bt_addr[1] = 0;
        _ci_event_log_bt_addr.bt_addr[2] = 0;
        _ci_event_log_bt_addr.bt_addr[3] = 0;
        _ci_event_log_bt_addr.bt_addr[4] = 0;
        _ci_event_log_bt_addr.bt_addr[5] = 0;
    }
    else
    {
        //_ci_event_log_bt_addr = *p_bt_addr;
        _ci_event_log_bt_addr.bt_addr[0] = p_bt_addr->bt_addr[0];
        _ci_event_log_bt_addr.bt_addr[1] = p_bt_addr->bt_addr[1];
        _ci_event_log_bt_addr.bt_addr[2] = p_bt_addr->bt_addr[2];
        _ci_event_log_bt_addr.bt_addr[3] = p_bt_addr->bt_addr[3];
        _ci_event_log_bt_addr.bt_addr[4] = p_bt_addr->bt_addr[4];
        _ci_event_log_bt_addr.bt_addr[5] = p_bt_addr->bt_addr[5];
    }

    ci_printv("[LOG] BT ADDR {%02X:%02X:%02X:%02X:%02X:%02X} \r\n", _ci_event_log_bt_addr.bt_addr[0], _ci_event_log_bt_addr.bt_addr[1], _ci_event_log_bt_addr.bt_addr[2], _ci_event_log_bt_addr.bt_addr[3], _ci_event_log_bt_addr.bt_addr[4], _ci_event_log_bt_addr.bt_addr[5]);
}

CI_EVENT_LOG_BT_ADDR_T ci_event_log_get_bt_addr(void)
{
    return _ci_event_log_bt_addr;
}

int ci_event_log_init(void)
{
    FIL            *fp;
    CI_EVENT_LOG_T *p_event_log;
    uint8_t        *p_file_name;
    bool            is_validate;
    int             byte_read;
    int             byte_written;
    int             ret;

    fp          = &g_snd_fatfs_ohdl;
    p_event_log = (CI_EVENT_LOG_T *) CI_FILESYSTEM_BASE_ADDR_FOR_EVENT_LOG;
    p_file_name = (uint8_t *) CI_EVENT_LOG_IDENT;
    is_validate = true;

    // 파일이 없으면 생성하는 옵션으로 연다. 에러 발생 시 파일 시스템 마운트 관련 에러로 예상할 수 있다.
    ret = f_open(fp, CI_EVENT_LOG_IDENT, (FA_OPEN_ALWAYS | FA_READ | FA_WRITE));
    if (ret != FR_OK)
    {
        ci_printe("[LOG] OPEN FAIL '%s' (RES=%d) \r\n", p_file_name, ret);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

    f_lseek(&g_snd_fatfs_ohdl, 4096);  // FATFS에게 4KB로 고정된 파일을 생성할 수 있게 의도적으로 파일 포지션을 4096으로 설정
    f_lseek(&g_snd_fatfs_ohdl, 0);
    ci_printv("[LOG] PERFORMED : FILE (%s) LSEEK --> 4096 --> 0 \r\n", p_file_name);

    byte_read = 0;
    ret       = f_read(fp, (void *) p_event_log, sizeof(CI_EVENT_LOG_T), &byte_read);

    if (ret != FR_OK)
    {
        is_validate = false;
    }

    if (byte_read != sizeof(CI_EVENT_LOG_T))
    {
        is_validate = false;
    }
    else  // 이전에 로그 파일이 쓰여진 적이 있어서, 읽은 크기가 유효할 때
    {
        // IDENT 검사
        for (int i = 0; i < 12; i++)
        {
            if (p_event_log->ident[i] != p_file_name[i])
            {
                // IDENT가 유효하지 않음
                is_validate = false;
                break;
            }
        }

        // write_index 유효성 확인
        if (!((0 <= p_event_log->write_index) && (p_event_log->write_index < CI_EVENT_LOG_ENTITY_COUNT)))
        {
            // write_index 유효하지 않은 범위
            is_validate = false;
        }

        // entity_count 유효성 확인
        if (!((0 <= p_event_log->entity_count) && (p_event_log->entity_count <= CI_EVENT_LOG_ENTITY_COUNT)))
        {
            // entity_count 유효하지 않은 범위
            is_validate = false;
        }
    }

    SYS_WATCHDOG_REFRESH();

    // 파일 정보가 유효하지 않은 경우
    if (!is_validate)
    {
        ci_printe("[LOG] INVALID '%s' IDENT, WRITE INDEX, ENTITY COUNT, SO FULLY RESET \r\n", p_file_name);

        // EVENT LOG 메모리 영역 전체를 0으로 초기화
        for (int i = 0; i < (sizeof(CI_EVENT_LOG_T) / sizeof(int)); i++)
        {
            ((int *) p_event_log)[i] = 0;
        }

        // IDENT 영역만 다시 초기화
        for (int i = 0; i < 12; i++)
        {
            p_event_log->ident[i] = p_file_name[i];
        }

        f_lseek(fp, 0);
        ret = f_write(fp, (void *) p_event_log, sizeof(CI_EVENT_LOG_T), &byte_written);

        if ((ret != FR_OK) || (byte_written != sizeof(CI_EVENT_LOG_T)))
        {
            ci_printe("[LOG] INIT FAIL '%s' (RES=%d, WROTE=%d) \r\n", p_file_name, ret, byte_written);
            f_close(fp);
            return -1;
        }

        ci_printd("[LOG] SUCCESS TO FULLY RESET '%s' \r\n", p_file_name);
    }

    // 안전을 위한 flush
    f_sync(&g_snd_fatfs_ohdl);

#if 1
    FILINFO fno;
    ret = f_stat(p_file_name, &fno);
    if (ret == FR_OK)
    {
        ci_printv("[FS] NAME : %s, TOTAL SIZE : %u BYTES, START CLUSTER : %u \r\n",  //
                  fno.fname,
                  fno.fsize,
                  g_snd_fatfs_ohdl.obj.sclust);
    }
#endif

    f_close(fp);
    SYS_WATCHDOG_REFRESH();

    return 0;
}

int ci_event_log_write(uint32_t event_type)
{
    FIL                   *fp;
    CI_EVENT_LOG_T        *p_event_log;
    uint8_t               *p_file_name;
    bool                   is_validate;
    int                    byte_read;
    int                    byte_written;
    int                    ret;
    uint8_t               *p_entity_src;
    uint8_t               *p_entity_dst;
    CI_TIMER_TIME_T        time;
    CI_EVENT_LOG_BT_ADDR_T bt_addr;
    CI_EVENT_LOG_ENTITY_T  entity;

    fp          = &g_snd_fatfs_ohdl;
    p_event_log = (CI_EVENT_LOG_T *) CI_FILESYSTEM_BASE_ADDR_FOR_EVENT_LOG;
    p_file_name = (uint8_t *) CI_EVENT_LOG_IDENT;
    is_validate = true;

    // 파일이 없으면 에러로 처리한다.
    ret = f_open(fp, CI_EVENT_LOG_IDENT, (FA_OPEN_EXISTING | FA_READ | FA_WRITE));
    if (ret != FR_OK)
    {
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_open, __LINE__);
        ci_printe("[LOG] OPEN FAIL '%s' FOR WRITING (RES=%d) \r\n", p_file_name, ret);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

    f_lseek(fp, 0);

    byte_read = 0;
    ret       = f_read(fp, (void *) p_event_log, sizeof(CI_EVENT_LOG_T), &byte_read);

    // 초기화가 완료 되었다면, 반드시 읽혀야 하며, 읽은 바이트는 꼭 sizeof(CI_EVENT_LOG_T) 크기여야 한다
    if ((ret != FR_OK) || (byte_read != sizeof(CI_EVENT_LOG_T)))
    {
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_not_inited, __LINE__);
        ci_printe("[LOG] READ FAIL '%s' DURING WRITING (RES=%d, GOT=%d) \r\n", p_file_name, ret, byte_read);
        f_close(fp);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

    // IDENT 검사
    for (int i = 0; i < 12; i++)
    {
        if (p_event_log->ident[i] != p_file_name[i])
        {
            // IDENT가 유효하지 않음
            is_validate = false;
            break;
        }
    }

    // write_index 유효성 확인
    if (!((0 <= p_event_log->write_index) && (p_event_log->write_index < CI_EVENT_LOG_ENTITY_COUNT)))
    {
        // write_index 유효하지 않은 범위
        is_validate = false;
    }

    // entity_count 유효성 확인
    if (!((0 <= p_event_log->entity_count) && (p_event_log->entity_count <= CI_EVENT_LOG_ENTITY_COUNT)))
    {
        // entity_count 유효하지 않은 범위
        is_validate = false;
    }

    if (!is_validate)
    {
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_currupted, __LINE__);
        ci_printe("[LOG] INVALID '%s' IDENT, WRITE INDEX, ENTITY COUNT DURING WRITING \r\n", p_file_name);
        f_close(fp);
        return -1;
    }

    // 모든 정보가 유효하면 데이터를 저장

    entity.event   = event_type;
    entity.bt_addr = ci_event_log_get_bt_addr();
    time           = ci_timer_get_reference_time_after_self_update();
    // memcpy(&entity.time[0], &time, sizeof(CI_TIMER_TIME_T));
    entity.time[0] = time.year;
    entity.time[1] = time.month;
    entity.time[2] = time.day;
    entity.time[3] = time.hour;
    entity.time[4] = time.min;
    entity.time[5] = time.sec;

    p_entity_src = (uint8_t *) &entity;
    p_entity_dst = (uint8_t *) &p_event_log->entities[p_event_log->write_index];

    for (int i = 0; i < 16; i++)
    {
        if (i == 0)
        {
            ci_printv("[LOG] WRITE ENTITY {%02X ", p_entity_src[i]);
        }
        else if (i == 15)
        {
            ci_printv("%02X} \r\n", p_entity_src[i]);
        }
        else
        {
            ci_printv("%02X ", p_entity_src[i]);
        }
    }

    for (int i = 0; i < sizeof(CI_EVENT_LOG_ENTITY_T); i++)
    {
        p_entity_dst[i] = p_entity_src[i];
    }

    // 로그는 큐 방식으로 저장할 것이므로 entity_count는 CI_EVENT_LOG_ENTITY_COUNT 보다 커질 수 없음
    if (p_event_log->entity_count < CI_EVENT_LOG_ENTITY_COUNT)
    {
        p_event_log->entity_count++;
    }

    // 로그는 큐 방식으로 저장할 것이므로 write_index는 0에서 (CI_EVENT_LOG_ENTITY_COUNT - 1)로 설정되어야 함
    p_event_log->write_index = (p_event_log->write_index + 1) % CI_EVENT_LOG_ENTITY_COUNT;

    f_lseek(fp, 0);

    byte_written = 0;
    ret          = f_write(fp, (void *) p_event_log, sizeof(CI_EVENT_LOG_T), &byte_written);

    if ((ret != FR_OK) || (byte_written != sizeof(CI_EVENT_LOG_T)))
    {
        errorCodeUpdate(en__MAJOR_ERRORCODE_DATA_LOGGING_ERROR, en__data_logging_error_write, __LINE__);
        ci_printe("[LOG] FAILED TO WRITE '%s' (RET=%d, WROTE=%d) \r\n", p_file_name, ret, byte_written);
        f_close(fp);
        return -1;
    }

    ci_printd("[LOG] SUCCESS TO WRITE '%s' \r\n", p_file_name);

    f_close(fp);
    SYS_WATCHDOG_REFRESH();

    return 0;
}

int ci_event_log_read(void)
{
    FIL            *fp;
    CI_EVENT_LOG_T *p_event_log;
    uint8_t        *p_file_name;
    bool            is_validate;
    int             byte_read;
    int             byte_written;
    int             ret;
    uint8_t        *p_entity_src;
    uint8_t        *p_entity_dst;

    fp          = &g_snd_fatfs_ohdl;
    p_event_log = (CI_EVENT_LOG_T *) CI_FILESYSTEM_BASE_ADDR_FOR_EVENT_LOG;
    p_file_name = (uint8_t *) CI_EVENT_LOG_IDENT;
    is_validate = true;

    // 파일이 없으면 생성하는 옵션으로 연다. 에러 발생 시 파일 시스템 마운트 관련 에러로 예상할 수 있다.
    ret = f_open(fp, CI_EVENT_LOG_IDENT, (FA_OPEN_EXISTING | FA_READ));
    if (ret != FR_OK)
    {
        ci_printe("[LOG] OPEN FAIL '%s' FOR READING (RES=%d) \r\n", p_file_name, ret);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

#if 1
    // 로그를 읽기 전, EVENT LOG 메모리 영역 전체를 0으로 초기화
    for (int i = 0; i < (sizeof(CI_EVENT_LOG_T) / sizeof(int)); i++)
    {
        ((int *) p_event_log)[i] = 0;
    }
#endif

    f_lseek(fp, 0);

    byte_read = 0;
    ret       = f_read(fp, (void *) p_event_log, sizeof(CI_EVENT_LOG_T), &byte_read);

    // 초기화가 완료 되었다면, 반드시 읽혀야 하며, 읽은 바이트는 꼭 sizeof(CI_EVENT_LOG_T) 크기여야 한다
    if ((ret != FR_OK) || (byte_read != sizeof(CI_EVENT_LOG_T)))
    {
        ci_printe("[LOG] READ FAIL '%s' DURING READING (RES=%d, GOT=%d) \r\n", p_file_name, ret, byte_read);
        f_close(fp);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

    // IDENT 검사
    for (int i = 0; i < 12; i++)
    {
        if (p_event_log->ident[i] != p_file_name[i])
        {
            // IDENT가 유효하지 않음
            is_validate = false;
            ci_printe("[LOG] INVALID '%s' IDENT \r\n", p_file_name);
            break;
        }
    }

    // write_index 유효성 확인
    if (!((0 <= p_event_log->write_index) && (p_event_log->write_index < CI_EVENT_LOG_ENTITY_COUNT)))
    {
        // write_index 유효하지 않은 범위
        is_validate = false;
        ci_printe("[LOG] INVALID '%s' WRITE INDEX \r\n", p_file_name);
    }

    // entity_count 유효성 확인
    if (!((0 <= p_event_log->entity_count) && (p_event_log->entity_count <= CI_EVENT_LOG_ENTITY_COUNT)))
    {
        // entity_count 유효하지 않은 범위
        is_validate = false;
        ci_printe("[LOG] INVALID '%s' ENTITY COUNT \r\n", p_file_name);
    }

    if (!is_validate)
    {
        f_close(fp);
        return -1;
    }

    SYS_WATCHDOG_REFRESH();

    // 내용 출력
    ci_printi("\r\n[LOG] IDENT=");
    for (int i = 0; i < 12; i++)
    {
        ci_printi("%c", p_event_log->ident[i]);
    }
    ci_printi(", WRITE INDEX=%d, ENTITY COUNT=%d \r\n",  //
              p_event_log->write_index,
              p_event_log->entity_count);

    for (int i = 0; i < p_event_log->entity_count; i++)
    {
        ci_printi("      ");
        ci_printi("<%3d> : ", i);
        ci_printi("TIME {%02d-%02d-%02d-%02d-%02d-%02d} ", p_event_log->entities[i].time[0], p_event_log->entities[i].time[1], p_event_log->entities[i].time[2], p_event_log->entities[i].time[3], p_event_log->entities[i].time[4], p_event_log->entities[i].time[5]);
        ci_printi("ADDR {%02X:%02X:%02X:%02X:%02X:%02X} ",
                  p_event_log->entities[i].bt_addr.bt_addr[0],
                  p_event_log->entities[i].bt_addr.bt_addr[1],
                  p_event_log->entities[i].bt_addr.bt_addr[2],
                  p_event_log->entities[i].bt_addr.bt_addr[3],
                  p_event_log->entities[i].bt_addr.bt_addr[4],
                  p_event_log->entities[i].bt_addr.bt_addr[5]);
        ci_printi("EVENT {%08X} \r\n", p_event_log->entities[i].event);
    }
    ci_printi("\r\n");

    f_close(fp);
    SYS_WATCHDOG_REFRESH();

    return 0;
}
