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
#include <FrequencyAnalysis.h>
#include <definitionsForAlgorithm.h>
#include <microcode.h>            /* HEAR_ADDR_AUDIO_MIX */

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

/* ---------------------------------------------------------------- CFX-070
 * 이득 계산 유닛 (calculate_agc_gain)
 *
 * 입력을 여덟 가지로 넣어 세 구간(잡음, 증폭, 감쇠)을 모두 지나가게 한다.
 * 이어서 볼륨을 0, 5, 9 로 바꿔 절편과 기울기가 갈리는지 본다.
 * 직전 이득이 상태로 남으므로 시험마다 0 으로 되돌린다.
 */
static void tdc_ut_agc_gain(void)
{
    static const int in_list[8]  = {1, 10, 100, 1000, 10000, 100000, 1000000, 8388607};
    static const int vol_list[3] = {0, 5, 9};
    int i;
    int saved_volume;

    tdc_ut_mark(70);

    saved_volume   = m_audio_volume;
    m_audio_volume = 0;

    for (i = 0; i < 8; i++)
    {
        m_prev_agc_10db_gain_Q8_16 = 0;
        tdc_ut_put(calculate_agc_gain(in_list[i]));
    }

    for (i = 0; i < 3; i++)
    {
        m_audio_volume             = vol_list[i];
        m_prev_agc_10db_gain_Q8_16 = 0;
        tdc_ut_put(calculate_agc_gain(100000));
    }

    m_audio_volume             = saved_volume;
    m_prev_agc_10db_gain_Q8_16 = 0;
}

/* ---------------------------------------------------------------- CFX-081
 * 거듭제곱 계산 유닛 (calculate_x_power_p)
 *
 * dB 변환 수식이 SDK 헤더에 명세되어 있어 기댓값을 산출했다.
 * 하드웨어 log2/exp2 유닛의 마지막 자리 오차만 남는다.
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

/* ---------------------------------------------------------------- CFX-072
 * 이득 적용 유닛 (apply_agc_gain)
 *
 * 믹서 버퍼에 시험 벡터를 넣고 이득 세 가지로 호출한다.
 * 이득 4096 은 선형 1.0 이므로 포화가 없는 한 입력이 그대로 나와야 한다.
 * 이득 8192(선형 2.0)에서 24 비트 경계가 잘리는지 함께 본다.
 */
static void tdc_ut_apply_agc_gain(void)
{
    static const int vec[df_inputADC_DataBuffLength] = {
        1000, -1000, 4000000, -4000000, 8388607, -8388608,
        0, 1, -1, 100, -100, 32768, -32768, 500000, -500000, 12345};
    static const int gain_list[3] = {4096, 2048, 8192};
    int _XMEM *p_mix = (int _XMEM *) HEAR_ADDR_AUDIO_MIX;
    int        g, i;

    tdc_ut_mark(72);

    for (g = 0; g < 3; g++)
    {
        for (i = 0; i < df_inputADC_DataBuffLength; i++)
        {
            p_mix[i] = vec[i];
        }

        apply_agc_gain(gain_list[g]);

        for (i = 0; i < df_inputADC_DataBuffLength; i++)
        {
            tdc_ut_put(m_agc_output_buffer[i]);
        }
    }
}

/* ---------------------------------------------------------------- CFX-073
 * 주파수 분석 입력 갱신 유닛 (update_FFT_inputData)
 *
 * AGC 출력 버퍼에 1 부터 16 을 넣고 호출한다. FIFO 에는 역순으로 들어가야 한다.
 * 이 시험은 FIFO 를 조회해야 하므로 결과 배열에는 넣은 값만 남긴다.
 */
static void tdc_ut_update_fft_input(void)
{
    int i;

    tdc_ut_mark(73);

    for (i = 0; i < df_inputADC_DataBuffLength; i++)
    {
        m_agc_output_buffer[i] = i + 1;
        tdc_ut_put(m_agc_output_buffer[i]);
    }

    update_FFT_inputData();
    /* FIFO 기록 순서는 디버거에서 D_FIFO_A1_0 를 직접 조회해 확인한다. */
}

/* ---------------------------------------------------------------- 진입점 */
void tdc_ut_run_all(void)
{
    tdc_ut_count = 0;
    tdc_ut_done  = 0;

    tdc_ut_agc_gain();                /* CFX-070 */
    tdc_ut_apply_agc_gain();          /* CFX-072 */
    tdc_ut_update_fft_input();        /* CFX-073 */
    tdc_ut_agc_attack_release();      /* CFX-071 */
    tdc_ut_nofm_init_buffers();       /* CFX-086 */
    tdc_ut_nofm_peak_pick();          /* CFX-087 */
    tdc_ut_nofm_interleaving();       /* CFX-088 */
    tdc_ut_x_power_p();               /* CFX-081 */

    tdc_ut_done = 1;                  /* 여기에 중단점을 건다 */
}

#endif /* TDC_UNIT_TEST */
