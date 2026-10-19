/* SYS-054/073/077/080은 시험용 외부기에서 내부기를 분리하고 실행한다.
 * 명령 진입 후 정상 맵/절전 제어는 정지한다. IRQ/워치독/오류 LED는 유지한다.
 * 원래 펌웨어의 보호 결함을 시험 모듈에서 성공 결과로 바꾸지 않는다. */
#include "tdc_sys_test_storage.h"
#if TDC_SYS_TEST_STORAGE_ENABLE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SEGGER_RTT.h>
#include "cfx_cm3_sharedMemory.h"
#include "isd_interface.h"
#include "error.h"
#include "LedOutput.h"
#include "systemControl.h"
#include "tdc_ui_command.h"
#if TDC_SYS_TEST_54_ENABLE
#include "tdc_sys_test_54.h"
#endif
#if TDC_SYS_TEST_73_ENABLE
#include "tdc_sys_test_73.h"
#endif
#if TDC_SYS_TEST_77_ENABLE
#include "tdc_sys_test_77.h"
#endif
#if TDC_SYS_TEST_80_ENABLE
#include "tdc_sys_test_80.h"
#endif

#define TDC_STORAGE_LINES 64u
#define TDC_STORAGE_LINE_BYTES 60u
/* 전체 응답을 보관하고 매 poll 한 행만 전송. 버퍼 부족 시 행 전체를 재시도한다. */
static char s_tdc_storage_lines[TDC_STORAGE_LINES][TDC_STORAGE_LINE_BYTES + 1u];
static unsigned s_tdc_storage_count;
static unsigned s_tdc_storage_next;
static unsigned s_tdc_storage_id;
static bool s_tdc_storage_hold;
static bool s_tdc_storage_overflow;

void tdc_sys_storage_log(const char *format, ...)
{
    va_list args;
    if (s_tdc_storage_count >= TDC_STORAGE_LINES)
    {
        s_tdc_storage_overflow = true;
        return;
    }
    char *line = s_tdc_storage_lines[s_tdc_storage_count++];
    const int prefix = snprintf(line, TDC_STORAGE_LINE_BYTES, "[SYS-%03u] ", s_tdc_storage_id);
    va_start(args, format);
    int n = vsnprintf(line + prefix, TDC_STORAGE_LINE_BYTES - (unsigned)prefix - 1u, format, args);
    va_end(args);
    if (n < 0 || (unsigned)(prefix + n) > TDC_STORAGE_LINE_BYTES - 2u)
    {
        strcpy(line + prefix, "LOG_FORMAT_ERROR; result incomplete");
        n = (int)strlen(line + prefix);
    }
    const unsigned len = (unsigned)(prefix + n);
    line[len] = '\r'; line[len + 1u] = '\n'; line[len + 2u] = '\0';
}

void tdc_sys_storage_hex(const char *label, const uint8_t *data, unsigned size)
{
    /* 16바이트를 32개의 16진수 문자로 표시한다. 키/실제 사용자 정보에는 사용 금지. */
    static const char digits[] = "0123456789ABCDEF";
    char hex[33];
    for (unsigned offset = 0; offset < size; offset += 16u)
    {
        unsigned count = size - offset;
        if (count > 16u) count = 16u;
        for (unsigned i = 0; i < count; ++i)
        {
            hex[2u * i] = digits[data[offset + i] >> 4];
            hex[2u * i + 1u] = digits[data[offset + i] & 15u];
        }
        hex[2u * count] = '\0';
        tdc_sys_storage_log("%.5s %03u %s", label, offset, hex);
    }
}

static bool tdc_storage_has_error(ST__ERROR_CODE e)
{
    return e.dataProcessingErrorFlag || e.accelerometerErrorFlag || e.PowerIcErrorFlag ||
           e.FPGA_CommunicationErrorFlag || e.FPGA_ConfiguraionErrorFlag || e.ISD_ErrorFlag ||
           e.FPGA_OR_ISD_ErrorFlag || e.FPGA_systemError || e.FPGA_backtelError || e.data_logging_error;
}

bool tdc_sys_storage_is_active(void) { return s_tdc_storage_hold; }
bool tdc_sys_storage_pending(void) { return s_tdc_storage_count != 0u || s_tdc_storage_overflow; }

bool tdc_sys_storage_command_allowed(const char *name)
{
    return strcmp(name, "sys54") == 0 || strcmp(name, "sys73") == 0 ||
           strcmp(name, "sys77") == 0 || strcmp(name, "sys80") == 0 || strcmp(name, "help") == 0;
}

int tdc_sys_storage_command(unsigned id, int argc, char *argv[], bool mapping_connected)
{
    if (argc < 2 || argv == NULL || argv[1] == NULL) return -1;
    if (argc == 2 && strcmp(argv[1], "stop") == 0)
    {
        /* 시험 작업은 동기 실행이다. stop은 미전송 로그만 취소하며 설정 복원이 아니다. */
        s_tdc_storage_count = s_tdc_storage_next = 0;
        s_tdc_storage_overflow = false;
        s_tdc_storage_id = id;
        tdc_sys_storage_log("STOP: log cancelled; no automatic restore");
        tdc_sys_storage_log("HOLD=%u; reboot to resume", s_tdc_storage_hold ? 1u : 0u);
        return 0;
    }
    if (tdc_sys_storage_pending()) return 1;
    s_tdc_storage_id = id;
    if (argc == 2 && strcmp(argv[1], "status") == 0)
    {
        tdc_sys_storage_log("HOLD=%u PCM=%d LOG_ERR=%d", s_tdc_storage_hold ? 1u : 0u,
                            readCurrentPcmOutputMode(), readErrorCode().data_logging_error);
        tdc_sys_storage_log("LED_REQ=%d; visual check required", led_get_request(LED_SRC_ERROR));
        return 0;
    }
#if TDC_SYS_TEST_77_ENABLE
    if (tdc_sys_test_77_requires_reboot())
    {
        tdc_sys_storage_log("REJECT=FS_UNCERTAIN; REBOOT_REQUIRED");
        return 0;
    }
#endif
#if TDC_SYS_TEST_80_ENABLE
    if (tdc_sys_test_80_requires_reboot())
    {
        tdc_sys_storage_log("REJECT=FS_UNCERTAIN; REBOOT_REQUIRED");
        return 0;
    }
#endif
    if (snd_isd_interface_get_state().conneded_ISD || mapping_connected)
    {
        tdc_sys_storage_log("REJECT: disconnect ISD and mapping");
        return 0;
    }
    if (tdc_storage_has_error(readErrorCode()) || led_get_request(LED_SRC_ERROR) != LED_ST_NONE)
    {
        tdc_sys_storage_log("REJECT: existing error; reboot/recheck");
        return 0;
    }
    if (tdc_ui_command_is_led_override(LED_SRC_ERROR) || tdc_led_is_burst_pending() ||
        led_get_request(LED_SRC_POWER) != LED_ST_NONE)
    {
        tdc_sys_storage_log("REJECT: LED override or power pattern");
        return 0;
    }
    /* 실제 연결 상태로 차단한다. --isd off 같은 화면용 override는 검사에 쓰지 않는다. */
    s_tdc_storage_hold = true;
    changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
    changePcmOutputMode(PcmBitStream_Mode_NopStandby);
    int result = -1;
    switch (id)
    {
#if TDC_SYS_TEST_54_ENABLE
    case 54: result = tdc_sys_test_54_execute(argc, argv); break;
#endif
#if TDC_SYS_TEST_73_ENABLE
    case 73: result = tdc_sys_test_73_execute(argc, argv); break;
#endif
#if TDC_SYS_TEST_77_ENABLE
    case 77: result = tdc_sys_test_77_execute(argc, argv); break;
#endif
#if TDC_SYS_TEST_80_ENABLE
    case 80: result = tdc_sys_test_80_execute(argc, argv); break;
#endif
    default: break;
    }
    if (result < 0) tdc_sys_storage_log("INPUT ERROR; use documented command");
    tdc_sys_storage_log("NOP HOLD; no system PASS; reboot to resume");
    return 0;
}

void tdc_sys_storage_poll(void)
{
    if (s_tdc_storage_hold)
    {
        changeNextPcmOutputMode(PcmBitStream_Mode_NopStandby);
        changePcmOutputMode(PcmBitStream_Mode_NopStandby);
        const ST__ERROR_CODE e = readErrorCode();
        if (tdc_storage_has_error(e))
        {
            /* 오류 코드가 실제 등록된 경우에만 기존 표시 경로로 전달한다. */
            const ST__USB_CONNECTOR charger = {0};
            (void)systemControl(e, charger, 100, false, false, false);
        }
    }
    if (s_tdc_storage_next < s_tdc_storage_count)
    {
        const char *line = s_tdc_storage_lines[s_tdc_storage_next];
        const unsigned size = (unsigned)strlen(line);
        /* SkipNoLock은 전송 바이트 수가 아니라 성공 1/실패 0을 반환한다.
         * 기존 ISR 로그와 write offset이 충돌하지 않게 짧은 RTT 임계구역만 둔다. */
        unsigned written;
        SEGGER_RTT_LOCK();
        written = SEGGER_RTT_WriteSkipNoLock(0, line, size);
        SEGGER_RTT_UNLOCK();
        if (written != 0u) ++s_tdc_storage_next;
    }
    if (s_tdc_storage_count && s_tdc_storage_next == s_tdc_storage_count)
    {
        s_tdc_storage_count = s_tdc_storage_next = 0;
        if (s_tdc_storage_overflow)
        {
            s_tdc_storage_overflow = false;
            tdc_sys_storage_log("LOG_OVERFLOW; results incomplete");
        }
    }
}
#endif
