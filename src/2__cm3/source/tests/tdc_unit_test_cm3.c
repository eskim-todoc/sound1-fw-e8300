/* tdc_unit_test_cm3.c - Cortex-M3 유닛 검증 시험 하네스
 *
 * 기존 run_all은 이전 SND1 보고서의 시험 벡터를 보존한다.
 * 현재 TD24A 기준은 tdc_ut_cm3_run_current()로 실행한다.
 * 현재 경로는 CM3-027의 직접 호출 시험만 지원한다.
 * 완료 후 tdc_ut_cm3_print_current(-1)로 요약을 출력한다.
 * 이어서 같은 함수를 0부터 11까지 한 줄씩 호출해 RTT 결과를 확인한다.
 *
 * 원칙
 *   1. 기존 소스를 수정하지 않는다. 이 파일에서 원 함수를 그대로 호출한다.
 *   2. 파일 전체가 TDC_UNIT_TEST 안에 있다. 매크로가 없으면 빈 번역 단위가
 *      되므로 평시 빌드에는 한 바이트도 들어가지 않는다.
 *   3. 결과는 tdc_ut_cm3_result 배열에 쌓는다. tdc_ut_cm3_done 에 중단점을
 *      걸고 메모리를 조회한다.
 *
 * 쓰는 법
 *   가. 빌드 설정에 TDC_UNIT_TEST 를 정의하고 이 파일을 빌드에 넣는다.
 *   나. 디버거에서 tdc_ut_cm3_run_all() 을 호출한다.
 *   다. tdc_ut_cm3_done 에 중단점이 걸리면 tdc_ut_cm3_result 를 조회한다.
 *   라. 시험 절차서의 기댓값과 대조한다.
 */

#if defined(TDC_UNIT_TEST)

#include <tdc_stim_para_cal.h>
#include <tdc_stim_common.h>
#include <tdc_printf.h>

#define TDC_UT_CM3_RESULT_MAX 128

int          tdc_ut_cm3_result[TDC_UT_CM3_RESULT_MAX];
int          tdc_ut_cm3_count;
volatile int tdc_ut_cm3_done;
int          tdc_ut_cm3_case;

static void tdc_ut_cm3_put(int value)
{
    if (tdc_ut_cm3_count < TDC_UT_CM3_RESULT_MAX)
    {
        tdc_ut_cm3_result[tdc_ut_cm3_count++] = value;
    }
}

static void tdc_ut_cm3_mark(int case_id)
{
    tdc_ut_cm3_case = case_id;
    tdc_ut_cm3_put(-999);      /* 구분자 */
    tdc_ut_cm3_put(case_id);
}

/* ---------------------------------------------------------------- CM3-112
 * 비트 구간 소거 유닛 (tdc_stim_data_clear_bit)
 *
 * 순수 비트 연산이라 입력에 대한 출력이 결정된다.
 */
static void tdc_ut_cm3_data_clear_bit(void)
{
    static const int data_list[5] = {0xFFFF, 0xFFFF, 0x00FF, 0x1234, 0xAAAA};
    static const int pos_list[5]  = {0, 4, 2, 8, 1};
    static const int len_list[5]  = {4, 4, 3, 8, 1};
    int i;

    tdc_ut_cm3_mark(112);

    for (i = 0; i < 5; i++)
    {
        tdc_ut_cm3_put(tdc_stim_data_clear_bit(data_list[i], pos_list[i], len_list[i]));
    }
}

/* ---------------------------------------------------------------- CM3-108
 * 채널당 프레임 수 산출 유닛
 *
 * duration = 2*pulsewidth + 4 + 10 이 41 의 몇 배인지로 정해진다.
 */
static void tdc_ut_cm3_num_frame(void)
{
    static const int pw_list[7] = {10, 20, 25, 30, 40, 50, 60};
    int i;

    tdc_ut_cm3_mark(108);

    for (i = 0; i < 7; i++)
    {
        tdc_ut_cm3_put(tdc_stim_calc_frame_per_channel(pw_list[i]));
    }
}

/* ---------------------------------------------------------------- CM3-109
 * 전달 가능 채널 수 산출 유닛
 *
 * 활성 경로는 24 / numFramePerChannel 뿐이며 둘째 인자를 쓰지 않는다.
 * 둘째 인자를 바꿔도 결과가 같아야 한다는 것까지 시험한다.
 */
static void tdc_ut_cm3_transferable_ch(void)
{
    static const int frame_list[6] = {1, 2, 3, 4, 5, 8};
    static const int elec_list[6]  = {16, 16, 16, 22, 22, 22};
    int i;

    tdc_ut_cm3_mark(109);

    for (i = 0; i < 6; i++)
    {
        tdc_ut_cm3_put(tdc_stim_calc_transferable_channel_num(frame_list[i], elec_list[i]));
    }

    /* 둘째 인자만 바꾼 대조 시험 - 앞의 결과와 같아야 한다 */
    for (i = 0; i < 6; i++)
    {
        tdc_ut_cm3_put(tdc_stim_calc_transferable_channel_num(frame_list[i], 8));
    }
}

/* ---------------------------------------------------------------- CM3-111
 * 출력단 보정 값 읽기 유닛
 *
 * 상수를 그대로 반환한다. 정의값과 같은지만 본다.
 */
static void tdc_ut_cm3_dac_offset(void)
{
    tdc_ut_cm3_mark(111);
    tdc_ut_cm3_put(tdc_stim_read_dac_offset_value());
}

/* ---------------------------------------------------------------- 진입점 */
void tdc_ut_cm3_run_all(void)
{
    tdc_ut_cm3_count = 0;
    tdc_ut_cm3_done  = 0;

    tdc_ut_cm3_data_clear_bit();      /* CM3-112 */
    tdc_ut_cm3_num_frame();           /* CM3-108 */
    tdc_ut_cm3_transferable_ch();     /* CM3-109 */
    tdc_ut_cm3_dac_offset();          /* CM3-111 */

    tdc_ut_cm3_done = 1;              /* 여기에 중단점을 건다 */
}

/* 현재 CM3-027: [-999, 유닛 번호, 벡터 번호, 출력 개수, 출력...].
 * expected의 같은 위치와 비교한다. done은 실행 종료이며 합격이 아니다.
 * 출력은 실제 전극 자극이나 GPIO 제어를 수행하지 않는 계산 결과다.
 */
#define TDC_UT_CM3_CURRENT_MAX 32

int g_tdc_ut_cm3_current_result[TDC_UT_CM3_CURRENT_MAX];
int g_tdc_ut_cm3_current_expected[TDC_UT_CM3_CURRENT_MAX];
int g_tdc_ut_cm3_current_count;
int g_tdc_ut_cm3_current_failed;
volatile int g_tdc_ut_cm3_current_done;
int g_tdc_ut_cm3_current_overflow;

static void current_put(int actual, int expected)
{
    int idx = g_tdc_ut_cm3_current_count;
    if (idx >= TDC_UT_CM3_CURRENT_MAX)
    {
        g_tdc_ut_cm3_current_overflow = 1;
        return;
    }
    g_tdc_ut_cm3_current_result[idx] = actual;
    g_tdc_ut_cm3_current_expected[idx] = expected;
    g_tdc_ut_cm3_current_count = idx + 1;
    if (actual != expected)
    {
        g_tdc_ut_cm3_current_failed++;
    }
}

void tdc_ut_cm3_run_current(void)
{
    static const int pulsewidths[2] = {13, 14};
    static const int frames[2] = {1, 2};
    static const int channels[2] = {24, 12};
    int vector, frame;

    g_tdc_ut_cm3_current_done = 0;
    g_tdc_ut_cm3_current_count = 0;
    g_tdc_ut_cm3_current_failed = 0;
    g_tdc_ut_cm3_current_overflow = 0;
    for (vector = 0; vector < 2; vector++)
    {
        current_put(-999, -999);
        current_put(27, 27);
        current_put(vector + 1, vector + 1);
        current_put(2, 2);
        frame = tdc_stim_calc_frame_per_channel(pulsewidths[vector]);
        current_put(frame, frames[vector]);
        /* 잘못된 반환값을 다음 나눗셈에 넘기지 않는다. 실패는 이미 기록했다. */
        if (frame <= 0)
        {
            current_put(-1, channels[vector]);
        }
        else
        {
            current_put(tdc_stim_calc_transferable_channel_num(frame, 32), channels[vector]);
        }
    }
    g_tdc_ut_cm3_current_done = 1;
}

/* 완료 후 비-ISR 시험 호출 위치에서 한 줄씩 출력한다.
 * index=-1은 요약, 0부터 count-1은 결과 원소다. RTT 출력 누락 시 재호출한다.
 * 반환 0은 줄 출력 요청 성공, -1은 미완료 또는 잘못된 인덱스다.
 * 실제 RTT 호스트 수신까지 보장하는 반환값은 아니다.
 */
int tdc_ut_cm3_print_current(int index)
{
    if (g_tdc_ut_cm3_current_done == 0)
    {
        return -1;
    }
    if (index == -1)
    {
        TDC_PRINTF_I("[UT CM3 CURRENT] count=%d failed=%d overflow=%d\r\n",
                  g_tdc_ut_cm3_current_count, g_tdc_ut_cm3_current_failed,
                  g_tdc_ut_cm3_current_overflow);
        return 0;
    }
    if (index < 0 || index >= g_tdc_ut_cm3_current_count || index >= TDC_UT_CM3_CURRENT_MAX)
    {
        return -1;
    }
    TDC_PRINTF_I("[UT CM3 CURRENT] index=%d actual=%d expected=%d\r\n", index,
              g_tdc_ut_cm3_current_result[index], g_tdc_ut_cm3_current_expected[index]);
    return 0;
}

#endif /* TDC_UNIT_TEST */
