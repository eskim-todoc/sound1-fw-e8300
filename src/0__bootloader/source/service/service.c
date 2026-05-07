
#include <service.h>
#include <main.h>
#include <ci_boot.h>      // tdc_boot_storage_init, tdc_boot_get_ohdl
#include <rtt_printf.h>   // rtt_printf, rtt_getch
#include <ff.h>           // FATFS

//
// private macros
//
#define TDC_SERVICE_LED_TICK_ITER 380   // ~380 iter × 1ms ≈ 380 ms 마다 색 변환

//
// private variables
//
static uint32_t s_tdc_service_led_iter  = 0;
static uint8_t  s_tdc_service_led_color = 0;

//
// private function headers
//
static void s_tdc_service_led_init(void);
static void s_tdc_service_led_tick(void);
static void s_tdc_service_print_fw_file(const char *p_path);
static void s_tdc_service_reboot(void);

//
// functions
//

/**
 * @brief service 모드 메인 — RTT down channel 입력 기반 디버그 콘솔.
 *
 * 진입: main() 에서 DIO_NUM_DMIC_CLK1_CAL = ACTIVE 시 분기.
 *
 * 동작:
 *  1. LED DIO 설정
 *  2. tdc_boot_storage_init(NULL) — NVM/FATFS init (boot_info 는 service 미사용)
 *  3. RTT down channel 0 폴링 → 명령어 파싱 → 펌웨어 파일 hex 덤프 또는 reboot
 *  4. 명령 처리 루프 동안 LED 색 변환 (백그라운드 tick, ~380 ms 주기)
 *
 * 명령어 셋: manifest / app0 / app1 / app2 / reboot.
 * reboot 은 500 ms RTT flush delay 후 SYS_WATCHDOG_RESET() 으로 HW reset.
 *
 * 호스트: auto-rtt-viewer (stdin → RTT down 0). line_terminator '\n' 가정.
 */
void service_main(void)
{
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
    rtt_printf("Commands: manifest, app0, app1, app2, reboot\r\n");

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
                    if (strncmp(buf, "manifest", 8) == 0)
                    {
                        s_tdc_service_print_fw_file("/MANIFEST.TXT");
                    }
                    else if (strncmp(buf, "app0", 4) == 0)
                    {
                        s_tdc_service_print_fw_file("/APP000.FEZ");
                    }
                    else if (strncmp(buf, "app1", 4) == 0)
                    {
                        s_tdc_service_print_fw_file("/APP001.FEZ");
                    }
                    else if (strncmp(buf, "app2", 4) == 0)
                    {
                        s_tdc_service_print_fw_file("/APP002.FEZ");
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

static void s_tdc_service_print_fw_file(const char *p_path)
{
    uint8_t  rtt_buffer[64];
    UINT     br;
    FRESULT  res;
    FIL     *fp = tdc_boot_get_ohdl();   /* 부트로더 ohdl 재사용 (별도 fp 변수 안 만듦) */

    res = f_open(fp, p_path, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
    {
        rtt_printf("[ERROR] Failed to open '%s' file.\r\n", p_path);
        return;
    }

    rtt_printf("%s=<<<<<\r\n", p_path);

    while (1)
    {
        SYS_WATCHDOG_REFRESH();
        s_tdc_service_led_tick();   // 큰 파일 출력 중에도 LED 색 변환 유지

        res = f_read(fp, rtt_buffer, 64, &br);
        if (res != FR_OK)
        {
            rtt_printf("[ERROR] Failed to read '%s' file.\r\n", p_path);
            f_close(fp);
            return;
        }

        if (br == 0)
        {
            rtt_printf(">>>>>\r\n");
            rtt_printf("print done.\r\n");
            break;
        }

        for (UINT i = 0; i < br; i++)
        {
            rtt_printf("%02X ", rtt_buffer[i]);
        }
        rtt_printf("\r\n");
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
