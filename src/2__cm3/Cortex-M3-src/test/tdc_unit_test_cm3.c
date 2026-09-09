/* tdc_unit_test_cm3.c - Cortex-M3 유닛 검증 시험 하네스
 *
 * 소프트웨어 유닛 검증 보고서(SUVR-FW-SND1)의 실기 시험용 코드다.
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

#include <stimulationParaCal.h>
#include <commonDataProcessing.h>

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
 * 비트 구간 소거 유닛 (data_clearBit)
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
        tdc_ut_cm3_put(data_clearBit(data_list[i], pos_list[i], len_list[i]));
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
        tdc_ut_cm3_put(calculationNumFramePerOneChannle(pw_list[i]));
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
        tdc_ut_cm3_put(calculationTransferableChanneNum(frame_list[i], elec_list[i]));
    }

    /* 둘째 인자만 바꾼 대조 시험 - 앞의 결과와 같아야 한다 */
    for (i = 0; i < 6; i++)
    {
        tdc_ut_cm3_put(calculationTransferableChanneNum(frame_list[i], 8));
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
    tdc_ut_cm3_put(readDAC_offsetValue());
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

#endif /* TDC_UNIT_TEST */
