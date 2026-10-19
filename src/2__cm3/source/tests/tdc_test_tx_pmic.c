
#include <hw.h>
#include <stdbool.h>
#include <stdint.h>

#include <tdc_test_tx_pmic.h>

#include <tdc_drv_isl9122.h>
#include <tdc_isd_fpga.h>
#include <tdc_led_output.h>
#include <tdc_printf.h>
#include <SEGGER_RTT_Wrapper.h>

/* ==========================================================================
 * TX PMIC 전압 테스트 콘솔 (임시 - 측정용)
 *
 * J-Link RTT Viewer 에서 명령을 쳐서 TX 전압 레벨을 올리고 내린다.
 * 레벨 1 단계 = 25mV.
 *
 *   +N    N 단계 상승 (부호를 생략한 "10" 도 상승으로 본다)
 *   -N    N 단계 하강
 *   =N    레벨 N 으로 직접 설정
 *   r     PMIC 리셋 (기본 전압 · NORMAL 모드 복귀)
 *   ?     현재 레벨 조회
 *   m     현재 동작 모드 조회
 *   mN    동작 모드 변경  m0 NORMAL · m2 FORCED PWM · m3 FORCED BYPASS
 *         (m1 은 데이터시트가 금지한 값이라 거부한다)
 *   c     CONV_CFG(0x12) 원시값과 비트 해독
 *
 * 레벨은 TDC_DRV_PMIC_MIN_TX_POWER_VALUE ~ TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE
 * 범위로 자른다. 범위를 넘겨 요청하면 경계값으로 걸리고 그 사실을 알린다.
 *
 * 모드 설명은 docs/참고/pmic-isl99122a/README.md 에 정리해 두었다. 요약하면
 * NORMAL 이 VIN 과 VOUT 관계를 보고 Buck/Bypass/Boost 를 자동으로 오가는
 * 기본값이고, FORCED BYPASS 는 스위칭을 꺼서 승압도 과전류 보호도 없다.
 *
 * RTT 출력은 뷰어 인코딩 문제를 피하려고 영문으로 쓴다.
 * ========================================================================== */

#define TDC_TX_PMIC_MV_PER_STEP 25
#define TDC_TX_PMIC_LINE_MAX    16

// RTT 입력을 읽는다. 두 가지 경로가 공존한다.
//
//  1) 방향키 - 엔터 없이 즉시 처리한다. 반환값이 0 이 아니면 그 값이 증감량이다.
//       위 +1 · 아래 -1 · 오른쪽 +10 · 왼쪽 -10
//     RTT Viewer 는 방향키를 ANSI 이스케이프 3바이트(ESC '[' 'A'~'D')로 보낸다.
//
//  2) 문자열 명령 - 개행까지 모아서 p_line 에 담고 0 을 반환한다.
//
// 개행이나 방향키가 올 때까지 블로킹하며 워치독을 먹인다.
static int tdc_tx_pmic_read_input(char *p_line, int maxLen)
{
    int  len      = 0;
    int  escState = 0;  // 0 : 없음, 1 : ESC 수신, 2 : '[' 수신
    char ch;

    for (;;)
    {
        SYS_WATCHDOG_REFRESH();

        if (SEGGER_RTT_Wrapper_getch(&ch) != 1)
        {
            continue;
        }

        // ---- 방향키 이스케이프 시퀀스 ----
        //
        // 터미널마다 두 가지로 보낸다.
        //   ANSI 커서 모드          ESC '[' 'A'~'D'
        //   애플리케이션 커서 모드  ESC 'O' 'A'~'D'
        // 둘 다 받는다.
        if (escState == 1)
        {
            escState = ((ch == '[') || (ch == 'O')) ? 2 : 0;
            continue;
        }

        if (escState == 2)
        {
            escState = 0;

            switch (ch)
            {
                case 'A':
                    return 1;  // 위
                case 'B':
                    return -1;  // 아래
                case 'C':
                    return 10;  // 오른쪽
                case 'D':
                    return -10;  // 왼쪽

                default:
                    // 진단 - 예상 밖 시퀀스가 오면 무엇이 왔는지 보여준다.
                    TDC_PRINTF_I("[PMIC] unknown escape final 0x%02x\r\n", (unsigned) (unsigned char) ch);
                    continue;
            }
        }

        if (ch == 0x1B)  // ESC
        {
            escState = 1;
            continue;
        }

        // ---- 방향키 대체 단일 문자 (엔터 없이 즉시) ----
        //
        // RTT Viewer 가 방향키를 자기 입력창에서 커서 이동으로 소비해 버리면
        // 위 이스케이프가 펌웨어까지 오지 않는다. 그때 쓰는 대체 키다.
        if ((ch == 'w') || (ch == 'W'))
        {
            return 1;
        }
        if ((ch == 's') || (ch == 'S'))
        {
            return -1;
        }
        if ((ch == 'd') || (ch == 'D'))
        {
            return 10;
        }
        if ((ch == 'a') || (ch == 'A'))
        {
            return -10;
        }

        // 진단 - 인쇄 불가 제어문자가 오면 코드를 보여준다. 방향키가 다른
        // 코드로 오는 경우를 여기서 잡는다.
        if ((ch < 0x20) && (ch != '\r') && (ch != '\n') && (ch != '\t'))
        {
            TDC_PRINTF_I("[PMIC] rx ctrl 0x%02x\r\n", (unsigned) (unsigned char) ch);
            continue;
        }

        // ---- 문자열 명령 ----
        if ((ch == '\r') || (ch == '\n'))
        {
            if (len > 0)
            {
                p_line[len] = '\0';
                return 0;
            }

            continue;
        }

        if ((ch == ' ') || (ch == '\t'))
        {
            continue;
        }

        if (len < (maxLen - 1))
        {
            p_line[len++] = ch;
        }
    }
}

// 현재 레벨과 예상 전압을 출력한다. 1 단계 = 25mV 이므로 mV 는 정수로 딱 떨어진다.
static void tdc_tx_pmic_print_level(int level)
{
    int mv = level * TDC_TX_PMIC_MV_PER_STEP;

    TDC_PRINTF_I("[PMIC] level = %d  ->  %d mV  (approx %d.%03d V)\r\n", level, mv, mv / 1000, mv % 1000);
}

// 현재 동작 모드를 읽어 출력한다.
static void tdc_tx_pmic_print_mode(void)
{
    tdc_drv_isl9122_fmode_t mode;

    if (!tdc_drv_isl9122_read_mode(&mode))
    {
        TDC_PRINTF_E("[PMIC] mode read failed\r\n");
        return;
    }

    TDC_PRINTF_I("[PMIC] mode = m%d  %s\r\n", (int) mode, tdc_drv_isl9122_mode_name(mode));
}

// CONV_CFG 원시값과 비트별 해독을 출력한다.
//
// 모드 변경은 FMODE 두 비트만 건드려야 한다. 나머지 필드가 그대로인지 실기에서
// 눈으로 확인하는 것이 이 명령의 목적이다.
static void tdc_tx_pmic_print_conv_cfg(void)
{
    int value;

    if (!tdc_drv_isl9122_read_register(TDC_DRV_ISL9122_REG_CONV_CFG, &value))
    {
        TDC_PRINTF_E("[PMIC] CONV_CFG read failed\r\n");
        return;
    }

    TDC_PRINTF_I("[PMIC] CONV_CFG(0x12) = 0x%02X\r\n", (unsigned) value);
    TDC_PRINTF_I("[PMIC]   EN_AND %d  DISCH %d  DVSRATE %d  FMODE %d  TYPE1 %d\r\n",
                 (value >> 7) & 0x01,
                 (value >> 6) & 0x01,
                 (value >> 4) & 0x03,
                 (value >> 2) & 0x03,
                 (value >> 0) & 0x01);
}

// 동작 모드를 바꾼다.
//
// 데이터시트가 금지한 값(m1)은 여기서도 막는다. 드라이버가 이미 막지만, 거부
// 사유를 사람에게 알려 주려면 콘솔에도 분기가 필요하다.
static void tdc_tx_pmic_set_mode(int requested)
{
    if (requested == TDC_DRV_ISL9122_FMODE_RESERVED)
    {
        TDC_PRINTF_E("[PMIC] m1 is RESERVED in the datasheet - refused\r\n");
        return;
    }

    if (requested == TDC_DRV_ISL9122_FMODE_FORCED_BYPASS)
    {
        TDC_PRINTF_W("[PMIC] WARNING : forced bypass has NO overcurrent protection\r\n");
        TDC_PRINTF_W("[PMIC] WARNING : switching is off, VOUT follows VIN (no boost)\r\n");
    }

    if (!tdc_drv_isl9122_write_mode((tdc_drv_isl9122_fmode_t) requested))
    {
        TDC_PRINTF_E("[PMIC] mode set failed (requested m%d)\r\n", requested);
        return;
    }

    tdc_tx_pmic_print_mode();
}

// 레벨을 써 넣고 실제 반영값을 다시 읽어 확인한다.
static void tdc_tx_pmic_apply(int target, int *p_level)
{
    if (target < TDC_DRV_PMIC_MIN_TX_POWER_VALUE)
    {
        TDC_PRINTF_W("[PMIC] clamp to min %d (requested %d)\r\n", TDC_DRV_PMIC_MIN_TX_POWER_VALUE, target);
        target = TDC_DRV_PMIC_MIN_TX_POWER_VALUE;
    }
    else if (target > TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE)
    {
        TDC_PRINTF_W("[PMIC] clamp to max %d (requested %d)\r\n", TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE, target);
        target = TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE;
    }

    if (!tdc_isd_fpga_write_change_tx_power_level(target))
    {
        TDC_PRINTF_E("[PMIC] write failed (level %d)\r\n", target);
        return;
    }

    // 쓰기 성공이어도 실제 반영값을 다시 읽는다. 상한에서 잘렸을 수 있다.
    if (tdc_isd_fpga_read_tx_power_level(p_level))
    {
        tdc_tx_pmic_print_level(*p_level);
    }
    else
    {
        TDC_PRINTF_E("[PMIC] write ok but read-back failed\r\n");
        *p_level = target;
    }
}

// 명령 콘솔. 복귀하지 않는다.
static void tdc_tx_pmic_run_console(void)
{
    char line[TDC_TX_PMIC_LINE_MAX];
    int  level = TDC_DRV_PMIC_RESET_VOLTAGE_SET_VALUE;

    TDC_PRINTF_I("[PMIC] TX power console. 1 step = %d mV\r\n", TDC_TX_PMIC_MV_PER_STEP);
    TDC_PRINTF_I("[PMIC] no enter : arrow UP/DOWN +-1, LEFT/RIGHT -+10   (or w/s = +-1, a/d = -+10)\r\n");
    TDC_PRINTF_I("[PMIC] w/ enter : +N up / -N down / =N set / r reset / ? read\r\n");
    TDC_PRINTF_I("[PMIC] w/ enter : m mode read / m0 normal / m2 forced PWM / m3 forced bypass / c CONV_CFG\r\n");
    TDC_PRINTF_I("[PMIC] range %d ~ %d  (%d ~ %d mV)\r\n",
                 TDC_DRV_PMIC_MIN_TX_POWER_VALUE,
                 TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE,
                 TDC_DRV_PMIC_MIN_TX_POWER_VALUE * TDC_TX_PMIC_MV_PER_STEP,
                 TDC_DRV_PMIC_MAX_VOLTAGE_CONTROL_VALUE * TDC_TX_PMIC_MV_PER_STEP);

    if (tdc_isd_fpga_read_tx_power_level(&level))
    {
        tdc_tx_pmic_print_level(level);
    }
    else
    {
        TDC_PRINTF_E("[PMIC] initial read failed, assume reset value %d\r\n", level);
    }

    // 진입 직전의 tdc_drv_isl9122_reset() 이 CONV_CFG 에 기본값을 써서 모드를
    // NORMAL 로 돌려놓는다. 시작점을 눈에 보이게 남긴다.
    tdc_tx_pmic_print_mode();

    // LED 요청은 블록 진입부에서 이미 걸었다. 여기서 tdc_update_led_requests() 를
    // 다시 부르면 배터리 RESET 판정 보류에 걸려 TDC_LED_ST_IDLE 로 덮어써지고
    // LED 가 꺼진다.

    for (;;)
    {
        const char *p;
        int         value;
        int         sign;
        int         delta;
        bool        absolute;
        bool        hasDigit;

        delta = tdc_tx_pmic_read_input(line, sizeof(line));

        // 방향키는 엔터 없이 즉시 반영한다.
        if (delta != 0)
        {
            tdc_tx_pmic_apply(level + delta, &level);
            continue;
        }

        p = line;

        // r : PMIC 리셋
        if ((*p == 'r') || (*p == 'R'))
        {
            if (tdc_drv_isl9122_reset())
            {
                TDC_PRINTF_I("[PMIC] reset done\r\n");

                if (tdc_isd_fpga_read_tx_power_level(&level))
                {
                    tdc_tx_pmic_print_level(level);
                }

                // 리셋은 CONV_CFG 를 기본값으로 되돌리므로 모드도 NORMAL 로 간다.
                tdc_tx_pmic_print_mode();
            }
            else
            {
                TDC_PRINTF_E("[PMIC] reset failed\r\n");
            }

            continue;
        }

        // ? : 현재 레벨 조회
        if (*p == '?')
        {
            if (tdc_isd_fpga_read_tx_power_level(&level))
            {
                tdc_tx_pmic_print_level(level);
            }
            else
            {
                TDC_PRINTF_E("[PMIC] read failed\r\n");
            }

            continue;
        }

        // c : CONV_CFG 원시값 조회
        if ((*p == 'c') || (*p == 'C'))
        {
            tdc_tx_pmic_print_conv_cfg();
            continue;
        }

        // m : 모드 조회, mN : 모드 변경
        //
        // 명령 문자로 w · s · a · d 를 쓰면 안 된다. 입력기가 방향키 대체로
        // 먼저 소비해 문자열이 그 자리에서 잘린다.
        if ((*p == 'm') || (*p == 'M'))
        {
            p++;

            if (*p == '\0')
            {
                tdc_tx_pmic_print_mode();
                continue;
            }

            if ((*p >= '0') && (*p <= '3') && (*(p + 1) == '\0'))
            {
                tdc_tx_pmic_set_mode(*p - '0');
                continue;
            }

            TDC_PRINTF_E("[PMIC] bad mode command : %s  (use m, m0, m2, m3)\r\n", line);
            continue;
        }

        sign     = 1;
        absolute = false;

        if (*p == '=')
        {
            absolute = true;
            p++;
        }
        else if (*p == '+')
        {
            p++;
        }
        else if (*p == '-')
        {
            sign = -1;
            p++;
        }

        value    = 0;
        hasDigit = false;

        while ((*p >= '0') && (*p <= '9'))
        {
            value    = (value * 10) + (*p - '0');
            hasDigit = true;
            p++;
        }

        // 숫자가 없거나 뒤에 찌꺼기가 붙어 있으면 거부한다.
        if (!hasDigit || (*p != '\0'))
        {
            TDC_PRINTF_E("[PMIC] bad command : %s\r\n", line);
            continue;
        }

        tdc_tx_pmic_apply(absolute ? value : (level + (sign * value)), &level);
    }
}

// ===========================================================================
// 진입점 - main 의 func_normal() 부팅 시퀀스 직후에서 호출한다.
// ===========================================================================

void tdc_test_tx_pmic_console(void)
{
    // LED 를 배터리 LOW(주황 지속 ON)로 세워 둔다.
    //
    // tdc_update_led_requests() 를 쓰면 안 된다. 그 안의 배터리 정책이
    // "배터리 상태 RESET 이면 판정 보류" 분기를 먼저 타는데, QCC 0x34 배터리
    // 패킷을 받아야 RESET 이 풀린다. 이 콘솔은 무한 루프라 그 수신 처리가
    // 돌지 않으므로 몇 퍼센트를 넘겨도 TDC_LED_ST_IDLE 로 샌다.
    //
    // 기다릴 필요는 없다. POWER_ON(우선순위 95)은 180ms ON/OFF 4회 버스트 후
    // 스스로 요청을 해제하고, 그 다음 Timer 3 ISR tick 에서 아비터가
    // BATT_MID(20)를 고른다. LED 갱신은 ISR 이 하므로 이 무한 루프와 무관하게
    // 계속 돈다.
    //
    // 점멸을 원하면 TDC_LED_ST_BATT_CRITICAL (주황 1100ms ON/OFF, 우선순위 60).
    tdc_led_request(TDC_LED_SRC_BATTERY, TDC_LED_ST_BATT_MID);

    if (!tdc_drv_isl9122_reset())
    {
        TDC_PRINTF_E("[PMIC] reset failed - console not started\r\n");
    }
    else
    {
        tdc_tx_pmic_run_console();  // 복귀하지 않는다
    }

    // PMIC 리셋이 실패해 콘솔에 못 들어간 경우만 여기로 온다.
    while (1)
    {
        SYS_WATCHDOG_REFRESH();
    }
}
