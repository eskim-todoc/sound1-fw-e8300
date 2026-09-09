/* tdc_unit_test_cfx.c - CFX 유닛 검증 시험 하네스
 *
 * 소프트웨어 유닛 검증 보고서(SUVR-FW-SND1)의 실기 시험용 코드다.
 *
 * 원칙
 *   1. 기존 소스를 수정하지 않는다. 이 파일에서 원 함수를 그대로 호출한다.
 *   2. 파일 전체가 TDC_UNIT_TEST 안에 있다. 매크로가 없으면 빈 번역 단위가
 *      되므로 평시 빌드에는 한 바이트도 들어가지 않는다.
 *   3. 결과는 tdc_ut_result 배열에 쌓는다. tdc_ut_done 에 중단점을 걸고
 *      메모리를 조회한다.
 *
 * 쓰는 법
 *   가. 빌드 설정에 TDC_UNIT_TEST 를 정의하고 이 파일을 빌드에 넣는다.
 *   나. 디버거에서 tdc_ut_run_all() 을 호출한다.
 *   다. tdc_ut_done 에 중단점이 걸리면 tdc_ut_result 를 조회한다.
 *   라. 시험 절차서의 기댓값과 대조한다.
 *
 * 주의: 시험은 전역 상태를 바꾼다. 시험 후에는 재기동한다.
 */

#if defined(TDC_UNIT_TEST)

#include <agc.h>
#include <nonlinearMapping.h>
#include <stimulationStrategy.h>
#include <definitionsForAlgorithm.h>

#define TDC_UT_RESULT_MAX 256

int _XMEM          tdc_ut_result[TDC_UT_RESULT_MAX];
int _XMEM          tdc_ut_count;
volatile int _XMEM tdc_ut_done;
int _XMEM          tdc_ut_case;

static void tdc_ut_put(int value)
{
    if (tdc_ut_count < TDC_UT_RESULT_MAX)
    {
        tdc_ut_result[tdc_ut_count++] = value;
    }
}

static void tdc_ut_mark(int case_id)
{
    tdc_ut_case = case_id;
    tdc_ut_put(-999);      /* 구분자 */
    tdc_ut_put(case_id);
}

/* ---------------------------------------------------------------- CFX-071
 * 이득 시정수 적용 유닛 (audio_agc_attack_release)
 *
 * 직전 이득이 내부 상태로 남으므로 시험마다 직전 값을 지정한 뒤 호출한다.
 * 직전 <= 입력 이면 릴리즈 경로, 아니면 어택 경로다.
 */
static void tdc_ut_agc_attack_release(void)
{
    static const int prev_list[5] = {0, 65536, 65536, 131072, 0};
    static const int curr_list[5] = {65536, 0, 131072, 65536, 0};
    int i;

    tdc_ut_mark(71);

    for (i = 0; i < 5; i++)
    {
        m_prev_agc_10db_gain_Q8_16 = prev_list[i];
        tdc_ut_put(audio_agc_attack_release(curr_list[i]));
    }
}

/* ---------------------------------------------------------------- CFX-086
 * NofM 버퍼 초기화 유닛
 *
 * 버퍼를 일부러 다른 값으로 채운 뒤 호출해 전부 -1 이 되는지 본다.
 */
static void tdc_ut_nofm_init_buffers(void)
{
    int i;

    tdc_ut_mark(86);

    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        g_NofM_adjacent_BandIndex[i]    = 777;
        g_NofM_notAdjacent_BandIndex[i] = 777;
    }
    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        addr_stimulationTempBuff[i] = 777;
    }

    stimulationStrategy_NofM_Init_Buffers();

    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        tdc_ut_put(g_NofM_adjacent_BandIndex[i]);
    }
    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        tdc_ut_put(g_NofM_notAdjacent_BandIndex[i]);
    }
    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        tdc_ut_put(addr_stimulationTempBuff[i]);
    }
}

/* ---------------------------------------------------------------- CFX-087
 * NofM 피크 선택 유닛
 *
 * 진폭 배열을 네 가지로 주입해 선택 결과를 본다.
 * 벡터 나(전부 같은 값)가 저주파 보호 규칙을 확인하는 시험이다.
 */
static void tdc_ut_nofm_peak_pick_one(int vector_id)
{
    int i;

    for (i = 0; i < df_MaxNumOfElectrode; i++)
    {
        switch (vector_id)
        {
            case 0:  g_pcm_amplitude_level[i] = i * 10;                    break;
            case 1:  g_pcm_amplitude_level[i] = 100;                       break;
            case 2:  g_pcm_amplitude_level[i] = ((i % 2) == 0) ? 200 : 5;  break;
            default: g_pcm_amplitude_level[i] = 0;                         break;
        }
    }
    if (vector_id == 3)
    {
        g_pcm_amplitude_level[20] = 1000;
    }

    stimulationStrategy_NofM_Peak_Pick();

    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        tdc_ut_put(g_NofM_adjacent_BandIndex[i]);
    }
}

static void tdc_ut_nofm_peak_pick(void)
{
    tdc_ut_mark(87);
    tdc_ut_nofm_peak_pick_one(0);
    tdc_ut_nofm_peak_pick_one(1);
    tdc_ut_nofm_peak_pick_one(2);
    tdc_ut_nofm_peak_pick_one(3);
}

/* ---------------------------------------------------------------- CFX-088
 * NofM 인터리빙 유닛
 *
 * 선택 결과를 0 부터 15 로 두고 교차 배치를 본다.
 * 이어서 피크 선택 결과를 그대로 넘겨 실제 연결을 본다.
 */
static void tdc_ut_nofm_interleaving(void)
{
    int i;

    tdc_ut_mark(88);

    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        g_NofM_adjacent_BandIndex[i] = i;
    }
    stimulationStrategy_NofM_Interleaving();
    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        tdc_ut_put(g_NofM_notAdjacent_BandIndex[i]);
    }

    tdc_ut_nofm_peak_pick_one(2);          /* 짝수 색인 200 */
    stimulationStrategy_NofM_Interleaving();
    for (i = 0; i < df_MaxNum_NofM; i++)
    {
        tdc_ut_put(g_NofM_notAdjacent_BandIndex[i]);
    }
}

/* ---------------------------------------------------------------- CFX-081
 * 거듭제곱 계산 유닛 (calculate_x_power_p)
 *
 * 어셈블리 라이브러리(cfx_pwr_to_dB_asm, cfx_dB_to_pwr_asm)를 거치므로
 * 기댓값을 미리 낼 수 없다. 입력에 대한 출력을 그대로 기록해 둔다.
 */
static void tdc_ut_x_power_p(void)
{
    static const int x_list[6] = {0, 1, 100, 1000, 10000, 100000};
    int i;

    tdc_ut_mark(81);

    for (i = 0; i < 6; i++)
    {
        tdc_ut_put(x_list[i]);
        tdc_ut_put(calculate_x_power_p(x_list[i]));
    }
}

/* ---------------------------------------------------------------- 진입점 */
void tdc_ut_run_all(void)
{
    tdc_ut_count = 0;
    tdc_ut_done  = 0;

    tdc_ut_agc_attack_release();      /* CFX-071 */
    tdc_ut_nofm_init_buffers();       /* CFX-086 */
    tdc_ut_nofm_peak_pick();          /* CFX-087 */
    tdc_ut_nofm_interleaving();       /* CFX-088 */
    tdc_ut_x_power_p();               /* CFX-081 (실측) */

    tdc_ut_done = 1;                  /* 여기에 중단점을 건다 */
}

#endif /* TDC_UNIT_TEST */
