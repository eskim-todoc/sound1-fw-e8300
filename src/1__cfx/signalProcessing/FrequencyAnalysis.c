/**
 * @file FrequencyAnalysis.c
 */

#include <FrequencyAnalysis.h>

/* 파일 시스템으로 부터 읽어온 정보이다.
 * "FFT_Size/2"에 대하여 즉, Pass bin 별로 채널 0부터 31 중 어디로 매핑할 것인지에 대한 정보가 있다. */
int _XMEM g_pass_bin_index[HALF_FFT_SIZE]                = {0};
int _XMEM g_freq_rep_values_scaled[df_MaxNumOfElectrode] = {0};  // FFT, vMag 등의 스케일 적용된 주파수변 대표 값
int _XMEM g_freq_rep_values[df_MaxNumOfElectrode]        = {0};  // 주파수별 대표 값

void read_FFT_PassBin_index(void)
{
    /* 매핑 데이터에는 사용 가능한 채널의 수에 따라 (최소 1에서 최대 32)
     * HEAR의 FFT 계산 결과로 출력되는 256개의 Pass bin을
     * 각각 어떤 주파수 그룹으로 할당할 것인지 정보가 구성되어 있다. */

    int _IOMEM* p_FS_num_of_freqBand = (int _IOMEM*) ADDR_NUM_OF_FREQ_BAND_IN_FS_MEM;
    int _IOMEM* p_FS_indexPassBin    = (int _IOMEM*) ADDR_FFT_PASS_BIN_IN_FS_MEM;

    if (addr_MapProgramData_FrequencyAnalysisBandNumbers == (*p_FS_num_of_freqBand))
    {
        for (register int i = 0; i < HALF_FFT_SIZE; i++)
            chess_loop_range(HALF_FFT_SIZE, HALF_FFT_SIZE)
            {
                g_pass_bin_index[i] = p_FS_indexPassBin[i];
            }
    }
}

void update_FFT_inputData(void)
{
    register int _XMEM* p_agcOutput_Top = (m_agc_output_buffer + (df_inputADC_DataBuffLength - 1));

    /* Copy from Top to Bottom */

    for (register int i = 0; i < df_inputADC_DataBuffLength; i++)
        chess_loop_range(df_inputADC_DataBuffLength, df_inputADC_DataBuffLength)
        {
            D_FIFO_A1_0->ACCESS = *p_agcOutput_Top--;
        }
}

void find_freq_rep_value(void)
{
    /* 전극 32개에 매핑하기 위해
     * FFT 계산 결과의 PASS BIN을 총 32개의 주파수 대역으로 나누었다.
     * 32개의 주파수 대역 별 대표 값을 초기화 한다. */
    int* freq_rep_ptr = g_freq_rep_values;
    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
    {
        *freq_rep_ptr++ = 0;
    }

    /* index pass bin 배열에는
     * FFT 계산 후 출력된 각 pass bin을 어떤 주파수 그룹으로 매핑할 것인지에 대한 값을 가지고 있다.
     * 각 pass bin이 어떤 주파수 그룹으로 매핑될 것인지는 사용자 맵 정보 파일에 저장되어 있다.
     * 내부기 연결 후 CM3가 파일 시스템에서 사용자 맵 정보 파일을 읽고 CFX로 이 정보를 공유하면,
     * CFX가 이 값을 index pass bin 배열에 저장하게 되어있다.
     * 그래서 index pass bin 배열에 각 bin이 어떤 주파수 그룹으로 매핑될 것인지 정보가 있는 것이다. */

    /* freq rep values 배열에는
     * 사용자 맵 정보를 통해 index pass bin 배열에서 가리키고 있는
     * 최대 32개의 주파수 그룹의 대표 값을 저장한다.
     * 이를 위해 HEAR에서 vMag 계산을 통해 출력된 총, "FFT_SIZE / 2" 크기 만큼 출력된 magnitude 값을,
     * index pass bin이 가리키고 있는 주파수 그룹의 크기와 비교하며
     * freq rep values 배열의 각 주파수 대표 그룹에 가장 큰 magnitude가 저장되도록 한다. */

    int* bin_index_ptr  = g_pass_bin_index;                      // 어떤 주파수 그룹인가
    int _XMEM* vmag_ptr = (int _XMEM*) HEAR_ADDR_VMAG_OUTPUT;    // magnitude, HEAR의 vMag 계산 결과

    for (register int i = 0; i < HALF_FFT_SIZE; i++)
        chess_loop_range(HALF_FFT_SIZE, HALF_FFT_SIZE)
    {
        register int index     = *bin_index_ptr++;
        register int magnitude = *vmag_ptr++;

        // 참고: vMag 계산은 sqrt(Re^2 + Im^2) 이므로 항상 0 이상 (비음수) 값이다.
        // index는 -1, 0 ~ 31로 구성된다. -1은 사용하지 않는 주파수 그룹을 의미한다.

        if (index >= 0) {
            register int current_max = g_freq_rep_values[index];
            g_freq_rep_values[index] = (magnitude > current_max) ? magnitude : current_max;
        }
    }

    /* 일반적으로 시간 영역의 크기 A인 신호를 FFT 변환을 하면 주파수 영역에서 크기는 FFT_SIZE 배로 커진다.
     * 시간 영역의 신호가 사인파형일 경우에는 두 개의 피크로 크기가 분산되기 때문에 1/2 게인이 적용된다.
     * 현재는 FFT에 window를 적용하기 때문에 입력 값의 크기가 약 1/2 적용된다. 즉, 1/2 window 게인이 적용된다.
     * 실제 동작 테스트로, HEAR의 BFP 연산은 1/4 게인이 적용되는 것을 확인했다. (정우님 테스트 결과)
     * 결과적으로 HEAR의 Win_DFT_R_Forwar 연산 최종 게인은,
     * 'FFT_SIZE' * '1/2_분산_피크' * 1/2_윈도우' * '1/4_BFP' = 'FFT_SIZE / 16' 이다.
     * 여기에 HEAR의 vMag 연산은 1/2 게인을 갖는다.
     * 그래서 모든 게인의 총 합은, '(FFT_SIZE / 16)' * '1/2_vMag' = 'FFT_SIZE / 32' 이다.
     * 정리하면, 'FFT_SIZE / 32' = '512 / 32' = '16' = '2^4' 이므로
     * 각 주파수 그룹의 최대 magnitude로 구해진 값에 오른쪽 쉬프트 4를 하면 시간영역에서 구한 크기가 된다. */
    freq_rep_ptr    = g_freq_rep_values;
    int* scaled_ptr = g_freq_rep_values_scaled;

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
    {
        *scaled_ptr++ = (*freq_rep_ptr++) >> RIGHT_SHIFT_MAX_MAG_FREQ_SCALE;
    }
}
