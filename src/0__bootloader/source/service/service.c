
#include <service.h>
#include <main.h>
#include <ci_boot.h>      // tdc_boot_storage_init, tdc_boot_get_ohdl
#include <rtt_printf.h>   // rtt_printf, rtt_getch
#include <ff.h>           // FATFS

//
// private macros
//
#define TDC_SERVICE_LED_TICK_ITER 380   // ~380 iter × 1ms ~ 380 ms 마다 색 변환
#define TDC_SERVICE_HEX_PER_LINE  16    // sound1-fw-extractor 명세 §6 권장 (16 B/라인)

//
// private variables
//
static uint32_t s_tdc_service_led_iter  = 0;
static uint8_t  s_tdc_service_led_color = 0;

/* sound1-fw-extractor 명세 §3 4 파일 일괄 출력 대상.
 * path = FATFS 경로 (f_open 인자), display_name = 마커 표시 이름 (명세 §5). */
static const struct
{
    const char *path;
    const char *display_name;
} s_tdc_fw_files[] = {
    { "/MANIFEST.TXT", "MANIFEST.TXT" },
    { "/APP000.FEZ",   "APP000.FEZ"   },
    { "/APP001.FEZ",   "APP001.FEZ"   },
    { "/APP002.FEZ",   "APP002.FEZ"   },
};

//
// private function headers
//
static void s_tdc_service_led_init(void);
static void s_tdc_service_led_tick(void);
static void s_tdc_service_print_fw_file(const char *p_path, const char *p_name);
static void s_tdc_service_dump_fw(void);
static void s_tdc_service_reboot(void);

//
// functions
//

/**
 * @brief service 모드 메인 - RTT down channel 입력 기반 디버그 콘솔.
 *
 * 진입: main() 에서 DIO_NUM_DMIC_CLK1_CAL = ACTIVE 시 분기.
 *
 * 동작:
 *  1. RTT up 채널 0 을 BLOCK_IF_FIFO_FULL 모드로 전환 (큰 hex 덤프 시 chunk drop 방지)
 *  2. LED DIO 설정
 *  3. tdc_boot_storage_init(NULL) - NVM/FATFS init (boot_info 는 service 미사용)
 *  4. RTT down channel 0 폴링 → 명령어 파싱 → 4 파일 일괄 hex 덤프 또는 reboot
 *  5. 명령 처리 루프 동안 LED 색 변환 (백그라운드 tick, ~380 ms 주기)
 *
 * 명령어 셋:
 *  - dump  : 4 파일 (MANIFEST.TXT + APP000~002.FEZ) 일괄 dump.
 *            sound1-fw-extractor 호환 형식 (명세: projects/sound1-fw-extractor/docs/사용방법/Sound1 측 fw.txt 출력 명세.md)
 *  - reboot: 500 ms RTT flush delay + SYS_WATCHDOG_RESET() 으로 HW reset
 *
 * 호스트: auto-rtt-viewer (stdin → RTT down 0). line_terminator '\n' 가정.
 */
void service_main(void)
{
    /* service 모드 한정: RTT up 채널 0 을 BLOCK_IF_FIFO_FULL 로 전환.
     * 기본 NO_BLOCK_SKIP 은 1024 B 버퍼 가득 시 호출 통째 drop -
     * 파일 hex 덤프 (~52 B / 16 B 청크) 가 host (auto-rtt-viewer 10 ms polling)
     * drain 보다 빨리 누적되면 데이터 누락 발생.
     * BLOCK 모드는 buffer 빌 때까지 CPU stall - 인터랙티브 콘솔이라 허용 가능.
     * 부트로더 본체는 service_main 진입 안 하므로 영향 없음. */
    SEGGER_RTT_SetFlagsUpBuffer(0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);

    s_tdc_service_led_init();

    if (tdc_boot_storage_init(NULL) < 0)
    {
        rtt_printf("[SERVICE] storage init failed.\r\n");
        while (1)
        {
            SYS_WATCHDOG_REFRESH();
            s_tdc_service_led_tick();
            tdc_delay_ms(1);
        }
    }

    rtt_printf("[SERVICE] RTT debug console ready.\r\n");
    rtt_printf("Commands: dump, reboot\r\n");

    char buf[64] = {0};
    int  idx     = 0;
    char ch;

    while (1)
    {
        SYS_WATCHDOG_REFRESH();
        s_tdc_service_led_tick();

        if (rtt_getch(&ch) > 0)
        {
            if (ch == '\r')
            {
                /* CR 무시 (line_terminator 호환 안전장치) */
            }
            else if (ch == '\n')
            {
                rtt_printf("\r\n");

                if (idx > 0)
                {
                    if (strncmp(buf, "dump", 4) == 0)
                    {
                        s_tdc_service_dump_fw();
                    }
                    else if (strncmp(buf, "reboot", 6) == 0)
                    {
                        s_tdc_service_reboot();   /* unreachable: SYS_WATCHDOG_RESET 으로 HW reset */
                    }
                    else
                    {
                        rtt_printf("invalid command.\r\n");
                    }
                }

                idx    = 0;
                buf[0] = 0;
            }
            else if (ch == '\b')
            {
                if (idx > 0)
                {
                    idx--;
                    buf[idx] = 0;
                    rtt_printf("\b \b");
                }
            }
            else if (idx < 62)
            {
                buf[idx]     = ch;
                buf[idx + 1] = 0;
                idx++;
                rtt_printf("%c", ch);
            }
        }

        tdc_delay_ms(1);
    }
}

//
// private functions
//
static void s_tdc_service_led_init(void)
{
    Sys_DIO_Config(DIO_NUM_LED_R_UART_TX_E8300, DIO_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_G_UART_RX_E8300, DIO_CFG_LED);
    Sys_DIO_Config(DIO_NUM_LED_B,               DIO_CFG_LED);
}

static void s_tdc_service_led_tick(void)
{
    if (++s_tdc_service_led_iter >= TDC_SERVICE_LED_TICK_ITER)
    {
        s_tdc_service_led_iter  = 0;
        s_tdc_service_led_color = (s_tdc_service_led_color + 1) & 0x07;

        Sys_GPIO_Write(DIO_NUM_LED_R_UART_TX_E8300, !((s_tdc_service_led_color >> 0) & 0x01));
        Sys_GPIO_Write(DIO_NUM_LED_G_UART_RX_E8300, !((s_tdc_service_led_color >> 1) & 0x01));
        Sys_GPIO_Write(DIO_NUM_LED_B,               !((s_tdc_service_led_color >> 2) & 0x01));
    }
}

/**
 * @brief 4 파일 (MANIFEST.TXT + APP000~002.FEZ) 일괄 hex dump - sound1-fw-extractor 명세 §4 형식.
 *
 * 출력 구조 (명세 §4·§5):
 *   ========== FW DUMP BEGIN ==========\n
 *   :::FILE:<NAME>:BEGIN:::\n
 *   <hex data 16 B/라인 + \n> ...
 *   :::FILE:<NAME>:END:::\n
 *   ... (4 파일 반복)
 *   ========== FW DUMP END ==========\n
 *
 * 데이터 영역 안의 모든 줄바꿈은 LF only (명세 §2). 데이터 영역 밖은 CRLF 유지.
 */
static void s_tdc_service_dump_fw(void)
{
    rtt_printf("========== FW DUMP BEGIN ==========\n");

    for (size_t i = 0; i < sizeof(s_tdc_fw_files) / sizeof(s_tdc_fw_files[0]); i++)
    {
        s_tdc_service_print_fw_file(s_tdc_fw_files[i].path, s_tdc_fw_files[i].display_name);
    }

    rtt_printf("========== FW DUMP END ==========\n");
    rtt_printf("[SERVICE] dump done.\r\n");   /* 데이터 영역 밖, 사용자 피드백용 */
}

static void s_tdc_service_print_fw_file(const char *p_path, const char *p_name)
{
    uint8_t  rtt_buffer[TDC_SERVICE_HEX_PER_LINE];
    UINT     br;
    FRESULT  res;
    FIL     *fp = tdc_boot_get_ohdl();   /* 부트로더 ohdl 재사용 (별도 fp 변수 안 만듦) */

    res = f_open(fp, p_path, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
    {
        /* 데이터 영역 안, 파일 블록 밖 - 명세 §7 권장 OK 등급. BEGIN/END 마커 skip */
        rtt_printf("[ERROR] f_open '%s' failed (%d)\n", p_name, (int)res);
        return;
    }

    rtt_printf(":::FILE:%s:BEGIN:::\n", p_name);

    while (1)
    {
        SYS_WATCHDOG_REFRESH();
        s_tdc_service_led_tick();   /* 큰 파일 출력 중에도 LED 색 변환 유지 */

        res = f_read(fp, rtt_buffer, TDC_SERVICE_HEX_PER_LINE, &br);
        if (res != FR_OK)
        {
            /* 파일 블록 안 에러 - 마커 페어 유지 후 에러 메시지 출력 (extractor 짝맞춤) */
            rtt_printf(":::FILE:%s:END:::\n", p_name);
            rtt_printf("[ERROR] f_read '%s' failed (%d)\n", p_name, (int)res);
            f_close(fp);
            return;
        }

        if (br == 0)
        {
            rtt_printf(":::FILE:%s:END:::\n", p_name);
            break;
        }

        for (UINT i = 0; i < br; i++)
        {
            rtt_printf("%02X ", rtt_buffer[i]);
        }
        rtt_printf("\n");
    }

    f_close(fp);
}

static void s_tdc_service_reboot(void)
{
    rtt_printf("[SERVICE] reboot requested. Resetting in 500ms...\r\n");

    /* 500 ms 동안 LED tick + watchdog refresh 유지 → RTT 출력 버퍼 flush 보장
     * (출력 중이던 hex 덤프 등 마지막 메시지가 auto-rtt-viewer host 에 도달할 시간 확보) */
    for (int i = 0; i < 500; i++)
    {
        SYS_WATCHDOG_REFRESH();
        s_tdc_service_led_tick();
        tdc_delay_ms(1);
    }

    SYS_WATCHDOG_RESET();   /* 즉시 HW reset → main() 재진입 */

    while (1)
    {
        ;   /* unreachable, 안전망 */
    }
}
