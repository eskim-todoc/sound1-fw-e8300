/**
 * @file stimulationStrategy.c
 */

#include <stimulationStrategy.h>

int _XMEM g_pcm_stimulation_packet_header                = 0;    // 최초값은 0이어야 한다.
int _XMEM g_transferableChannelNum_per_1msec             = 0;    // 펄스폭에 따라 계산된 하나의 채널당 프레임 개숫에의해 24프레임 동안 보낼수 있는 차즉 채널 수.
int _XMEM g_pcmFrameNum_per_channel                      = 1;    // 최초값은 1이어야 한다.
int _XMEM addr_transferred_index                         = 0;    // 최초값은 0이어야 한다.
int _XMEM addr_stimulationData[df_MaxNumOfElectrode]     = {0};  // 최초값은 0이어야 한다.
int _XMEM addr_stimulationTempBuff[df_MaxNumOfElectrode] = {0};  // 최초값은 0이어야 한다.

int _XMEM addr_electrodeMap[32] = {
#if 1
    31, 12, 11, 9, 7, 5, 3, 1, 0, 2, 4, 6, 8, 10, 13, 15, 17, 18, 20, 22, 24, 26, 28, 30, 14, 29, 27, 25, 23, 21, 19, 16,
#else
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
#endif
};

int _XMEM g_NofM_adjacent_BandIndex[df_MaxNum_NofM]    = {0};
int _XMEM g_NofM_notAdjacent_BandIndex[df_MaxNum_NofM] = {0};
int _XMEM g_NofM_LastStimulus_BandIndex                = 0;
int _XMEM g_NofM_Phase                                 = 0;  // 0 : 상위 0 ~ 7, 1 : 하위 8 ~ 15

void read_transferableChannelNum_fromCM3(void)
{
    // CM3 코어에서 펄스폭에 따라 계산된 하나의 채널당 필요한 프레임 개수에 의해 24프레임 동안 보낼 수 있는 자극 채널 수를 계산하는 함수다.
    g_pcmFrameNum_per_channel          = Addr_SharedMem->calculatedStimulPara_byCM3.frameNumPerChannel;
    g_transferableChannelNum_per_1msec = Addr_SharedMem->calculatedStimulPara_byCM3.transferableFrameNum;

    // Addr_SharedMem->CM3_tempValue1 = g_pcmFrameNum_per_channel;
}

void prepare_pcmStimulationPacketHeader(void)
{
    // 선행 펄스 모양에 따라 자극 데이터 패킷의 헤더를 결정한다.
    g_pcm_stimulation_packet_header = pcm_Mold_Stimulation | (addr_MapProgramData_firstPulsePhase << firstPulsePhasePositionAtPCM_Mold);
}

void stimulationStrategy(void)
{
    switch (addr_MapProgramData_stimulationStrategy)
    // 자극 방식 확인
    {
        // CIS 자극 방식
        case df_stimulationStrategy_CIS:
            find_freq_rep_value();
            logarithmMapping();
            stimulationStrategy_CIS();
            break;

            // n of m 자극 방식
        case df_stimulationStrategy_NofM:
            stimulationStrategy_NofM();
            break;

            // 매질 특성에 맞춰 자극하는 방식
        case df_stimulationStrategy_medium:
            // Currently do nothing...
            break;
    }
}

void stimulationStrategy_NofM(void)
{
    /* N-of-M stimulation pipeline in a 2-phase cycle.
     * N-of-M 자극 전략의 전체 파이프라인을 2단계(Phase 0, 1) 사이클로 제어한다. */
    if (g_NofM_Phase == 0)
    {

        /* Phase 0: Execute the full signal processing and packet generation pipeline.
         * 전체 신호 처리(주파수 분석, 매핑, 피크 선택, 교차 배치, 패킷 생성)를 수행한다. */
        find_freq_rep_value();
        logarithmMapping();

        /* NOTE: NofM 테스트를 위한 코드. 테스트 후 삭제할 것. */
#if 0
        // Phase 0
        g_pcm_amplitude_level[0]  = 255;
        g_pcm_amplitude_level[8]  = 200;
        g_pcm_amplitude_level[1]  = 190;
        g_pcm_amplitude_level[9]  = 180;
        g_pcm_amplitude_level[2]  = 170;
        g_pcm_amplitude_level[10]  = 160;
        g_pcm_amplitude_level[3]  = 150;
        g_pcm_amplitude_level[11]  = 140;
        // Phase 1
        g_pcm_amplitude_level[4]  = 130;
        g_pcm_amplitude_level[12]  = 120;
        g_pcm_amplitude_level[5] = 110;
        g_pcm_amplitude_level[13] = 100;
        g_pcm_amplitude_level[6] = 90;
        g_pcm_amplitude_level[14] = 80;
        g_pcm_amplitude_level[7] = 70;
        g_pcm_amplitude_level[15] = 60;
#endif
        stimulationStrategy_NofM_Peak_Pick();
        stimulationStrategy_NofM_Interleaving();
        stimulationStrategy_NofM_Packet();

        g_NofM_Phase = 1;

        // Reset tracking index for a fresh PCM transmission
        // 새로운 PCM 전송을 시작하기 위해 인덱스를 초기화한다.
        addr_transferred_index = 0;
    }
    else
    {
        /* Phase 1: Skip processing to balance CPU load and allow multi-frame transmission.
         * 연산 부하를 분산하고 이전 프레임의 남은 패킷 전송을 이어가기 위해 신호 처리를 생략한다. */
        g_NofM_Phase = 0;
    }

    /* Always write to the PCM FIFO to ensure continuous hardware transmission.
     * 끊김 없는 하드웨어 자극 출력을 보장하기 위해 매 사이클마다 PCM 기록 함수를 호출한다. */
    stimulationStrategy_NofM_PCM_Write();
}

void stimulationStrategy_NofM_Init_Buffers(void)
{
    // ==========================================
    // Initialize Buffers
    // ==========================================
    for (register int i = 0; i < df_MaxNum_NofM; i++)
        chess_loop_range(df_MaxNum_NofM, df_MaxNum_NofM)
        {
            g_NofM_adjacent_BandIndex[i]    = -1;
            g_NofM_notAdjacent_BandIndex[i] = -1;
        }

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            addr_stimulationTempBuff[i] = -1;
        }
}

void stimulationStrategy_NofM_Peak_Pick(void)
{
    stimulationStrategy_NofM_Init_Buffers();

    int top_packed[df_MaxNum_NofM];
    int packed_scores[df_MaxNumOfElectrode];

    /* ================================================
     * Pre-Pack the Scores
     * ================================================
     * Combine the Amplitude and Frequency Index into a single 32-bit score.
     * 진폭(Amplitude)과 주파수 인덱스를 단일 32비트 점수로 사전 병합한다.
     *
     * Score = (Amplitude << 5) | (31 - Index);
     *
     * Shifting the amplitude up by 5 bits makes it the dominant factor.
     * The lower 5 bits store (31 - Index). In a tie, lower frequencies (smaller Index)
     * result in a higher score, protecting them from eviction.
     * 진폭을 5비트 시프트하여 우선순위를 부여하고, 하위 5비트에 (31 - Index)를 저장한다.
     * 진폭이 같을 경우 인덱스가 작은(저주파) 대역이 더 높은 점수를 받아 교체 대상에서 보호된다.
     *
     * EXAMPLE (Amplitude = 10):
     *   - Low Freq  (Index 2): (10 << 5) | (31 - 2) = 320 | 29 = 349
     *   - High Freq (Index 6): (10 << 5) | (31 - 6) = 320 | 25 = 345
     * Because 345 < 349, the high frequency (Index 6) is flagged as the weaker peak. */
    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            packed_scores[i] = (g_pcm_amplitude_level[i] << 5) | (31 - i);
        }

    /* ================================================
     * Fill the initial bucket
     * ================================================
     * Blindly fill the "top N" bucket with the first N packed scores.
     * 처음 N개의 패킹된 점수로 버킷을 초기화함.*/
    for (register int i = 0; i < df_MaxNum_NofM; i++)
        chess_loop_range(df_MaxNum_NofM, df_MaxNum_NofM)
        {
            top_packed[i] = packed_scores[i];
        }

    register int min_packed        = top_packed[0];
    register int min_idx_in_bucket = 0;

    // Find the initial minimum (weakest peak) in the bucket.
    // 버킷 내에서 가장 작은 점수(가장 약한 피크)를 탐색함.
    for (register int i = 1; i < df_MaxNum_NofM; i++)
        chess_loop_range(1, df_MaxNum_NofM)
        {
            if (top_packed[i] < min_packed)
            {
                min_packed        = top_packed[i];
                min_idx_in_bucket = i;
            }
        }

    /* ================================================
     * Brute-Force Bucket Swap
     * ================================================
     * Scan the remaining frequency bands. If a packed score is stronger than
     * our current minimum, overwrite the minimum slot and find the new weakest peak.
     * 남은 주파수 대역을 스캔하며, 현재 버킷의 최솟값보다 큰 점수를 찾으면
     * 해당 값을 교체하고 버킷 내 새로운 최솟값을 다시 찾다. */
    for (register int i = df_MaxNum_NofM; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(1, df_MaxNumOfElectrode)
        {
            register int current_packed = packed_scores[i];

            if (current_packed > min_packed)
            {
                // Replace the weakest peak
                top_packed[min_idx_in_bucket] = current_packed;

                // Find the NEW weakest peak
                min_packed        = top_packed[0];
                min_idx_in_bucket = 0;

                for (register int t = 1; t < df_MaxNum_NofM; t++)
                    chess_loop_range(1, df_MaxNum_NofM)
                    {
                        if (top_packed[t] < min_packed)
                        {
                            min_packed        = top_packed[t];
                            min_idx_in_bucket = t;
                        }
                    }
            }
        }

    /* ==========================================
     * Frequency Sort (Bitmask Presence)
     * ==========================================
     * Selected frequencies in the bucket are scattered randomly.
     * They MUST be sorted in ascending frequency order for proper clinical stimulation.
     * 버킷에 저장된 선택된 주파수들은 순서가 섞여 있으므로,
     * 정상적인 자극 출력을 위해 주파수 오름차순으로 정렬해야 함. */

    register unsigned long present_mask = 0;

    // Extract the original index using a bitmask (0x1F) and log it in a 32-bit mask.
    // 하위 5비트를 마스킹(0x1F)하여 원래 인덱스를 복원하고, 비트마스크의 해당 위치를 1로 설정함.
    for (register int i = 0; i < df_MaxNum_NofM; i++)
        chess_loop_range(df_MaxNum_NofM, df_MaxNum_NofM)
        {
            register int original_idx = 31 - (top_packed[i] & 0x1F);
            present_mask |= (1UL << original_idx);
        }

    // Count through the 32 bits. If the bit is '1', write the index to the output buffer.
    // 비트마스크를 0부터 순회하며 1로 설정된 인덱스만 출력 버퍼에 순서대로 저장함.
    register int presentOut = 0;
    for (register int b = 0; b < df_MaxNumOfElectrode; b++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            if (present_mask & (1UL << b))
            {
                g_NofM_adjacent_BandIndex[presentOut++] = b;
            }
        }
}

void stimulationStrategy_NofM_Interleaving(void)
{
    // ==========================================
    // Spatial Interleaving (Value-Aware)
    // Method: ID-Sorted Stride
    // ==========================================

    /* Reorders the selected frequency bands to maximize spatial distance between
     * consecutive stimulations. This prevents adjacent electrodes from firing in
     * sequence, thereby reducing electrical crosstalk (channel interference).
     * 선택된 주파수 대역의 순서를 재배치하여 연속적인 자극 간의 공간적 거리를 최대화한다.
     * 인접한 전극이 연속으로 발화하는 것을 방지하여 전기적 간섭(Crosstalk)을 줄이다. */

    int half_ch = df_MaxNum_NofM / 2;

    /* Divide the sorted array into two halves and interleave them.
     * Even indices get populated from the lower half, while odd indices get
     * populated from the upper half.
     * 정렬된 배열을 절반으로 나누어 교차 배치한다.
     * 짝수 인덱스 배열에는 하위 절반의 값을, 홀수 인덱스 배열에는 상위 절반의 값을 할당한다.
     * Example (N=4): [A, B, C, D] -> [A, C, B, D] */
    for (int i = 0; i < half_ch; i++)
    {
        g_NofM_notAdjacent_BandIndex[i * 2]       = g_NofM_adjacent_BandIndex[i];
        g_NofM_notAdjacent_BandIndex[(i * 2) + 1] = g_NofM_adjacent_BandIndex[i + half_ch];
    }

    /* Handle odd length arrays dynamically.
     * If the total number of selected bands (N) is odd, the middle element
     * (which becomes the last element of the original array in integer division)
     * is appended to the very end of the interleaved array.
     * 선택된 대역의 수(N)가 홀수인 경우를 동적으로 처리한다.
     * N이 홀수일 때 남는 마지막 요소는 교차 배열의 맨 끝에 그대로 배치된다. */
    if (df_MaxNum_NofM % 2 != 0)
    {
        g_NofM_notAdjacent_BandIndex[df_MaxNum_NofM - 1] = g_NofM_adjacent_BandIndex[df_MaxNum_NofM - 1];
    }
}

void stimulationStrategy_NofM_Packet(void)
{
    int NofM_maxIndex   = df_MaxNum_NofM - 1;
    int first_electrode = g_NofM_notAdjacent_BandIndex[0];
    int last_electrode  = g_NofM_notAdjacent_BandIndex[NofM_maxIndex];
    int prev_last_stim  = g_NofM_LastStimulus_BandIndex;

    /* Calculate the spatial distance from the previous frame's last stimulus to the
     * current sequence's first and last electrodes. Choose the starting point that
     * maximizes this distance to minimize electrical crosstalk between consecutive frames.
     * 이전 프레임의 마지막 자극 전극과 현재 시퀀스의 처음 및 마지막 전극 간의 공간적 거리를 계산한다.
     * 프레임 간의 전기적 간섭(Crosstalk)을 최소화하기 위해 거리가 더 먼 쪽을 시작 지점으로 선택한다. */
    int dist_to_first = first_electrode - prev_last_stim;
    dist_to_first     = (dist_to_first < 0) ? -dist_to_first : dist_to_first;

    int dist_to_last = last_electrode - prev_last_stim;
    dist_to_last     = (dist_to_last < 0) ? -dist_to_last : dist_to_last;

    // Pick the starting index (Only two possibilities: Forward or Wrapped/Reversed)
    int start_idx = (dist_to_last > dist_to_first) ? (NofM_maxIndex) : 0;

    /* Pre-update the last stimulus band index so it is ready for the NEXT frame's distance calculation.
     * 선택된 시작 방향을 바탕으로 다음 프레임 거리 계산에 사용될 마지막 자극 대역 인덱스를 미리 업데이트한다. */
    if (start_idx == 0)
    {
        g_NofM_LastStimulus_BandIndex = last_electrode;
    }
    else
    {
        g_NofM_LastStimulus_BandIndex = g_NofM_notAdjacent_BandIndex[df_MaxNum_NofM - 2];
    }

    // ==========================================
    // Sequence Generation
    // ==========================================
    int pcmOut = 0;
    int freqBandOrder, electrodIndex, stimulusLevel, electrodeMap;

    /* Construct the final PCM packets using Loop Peeling to avoid expensive modulo operations.
     * If the sequence starts from the end (N-1), process that single element first.
     * 모듈로(modulo) 연산을 피하기 위해 루프 필링(Loop Peeling)을 적용하여 최종 PCM 패킷을 생성한다.
     * 시퀀스가 끝(N-1)에서 시작하는 경우 해당 요소를 먼저 단독으로 처리한다. */
    if (start_idx == NofM_maxIndex)
    {
        freqBandOrder = g_NofM_notAdjacent_BandIndex[NofM_maxIndex];
        electrodIndex = addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1;
        electrodeMap  = addr_electrodeMap[electrodIndex] << electrodIndexPositionAtPCM_Mold;

        // Muted signals below T-level are replaced with a forward path check packet.
        // T 레벨 미만으로 묵음 처리된 신호는 상태 확인용 더미 데이터 패킷으로 대체한다.
        if (Addr_SharedMem->is_enabled_mute_stimulation_under_t_level == 1 && g_pcm_amplitude_level[freqBandOrder] == 0)
        {
            addr_stimulationTempBuff[pcmOut++] = ISD_registerAddr_forwardPath_check_Data;
        }
        else
        {
            stimulusLevel                      = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;
            addr_stimulationTempBuff[pcmOut++] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
        }
    }

    /* Process the remaining sequence strictly linearly.
     * If we peeled the last element, loop stops at N-2. Otherwise, it processes 0 to N-1.
     * 나머지 시퀀스를 순차적으로 처리한다. 마지막 요소를 먼저 처리했다면 N-2까지만 루프를 돌고,
     * 그렇지 않다면 0부터 N-1까지 정상적으로 진행한다. */
    int loop_end = (start_idx == NofM_maxIndex) ? NofM_maxIndex : df_MaxNum_NofM;

    for (int pcmIdx = 0; pcmIdx < loop_end; pcmIdx++)
        chess_loop_range(df_MaxNum_NofM - 1, df_MaxNum_NofM)
        {
            freqBandOrder = g_NofM_notAdjacent_BandIndex[pcmIdx];
            electrodIndex = addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1;
            electrodeMap  = addr_electrodeMap[electrodIndex] << electrodIndexPositionAtPCM_Mold;

            if (Addr_SharedMem->is_enabled_mute_stimulation_under_t_level == 1 && g_pcm_amplitude_level[freqBandOrder] == 0)
            {
                addr_stimulationTempBuff[pcmOut++] = ISD_registerAddr_forwardPath_check_Data;
            }
            else
            {
                stimulusLevel                      = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;
                addr_stimulationTempBuff[pcmOut++] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
            }
        }
}

void stimulationStrategy_NofM_PCM_Write(void)
{
    // ==========================================
    // STEP 8: PCM WRITING
    // ==========================================
    int _XMEM *p_pcmFIFO = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;
#if 1
    int        stride    = g_pcmFrameNum_per_channel;
#else
    int        stride    = 3; // IMPORTANT : NofM일 때는 자극 당 3 프레임을 고정 시킴
#endif

    /* ---------------------------------------------------------
     * FAST PRE-FILL: Blast the entire buffer with NOPs
     * ---------------------------------------------------------
     * Fill the entire PCM transmission buffer with NOP standby packets first.
     * 먼저 PCM 전송 버퍼 전체를 NOP 대기 패킷으로 채우다. */
    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(0, df_MaxNumTransferableChannel)
        {
            *p_pcmFIFO-- = pcm_Mold_NopStandby;
        }

    /* ---------------------------------------------------------
     * STRIDED OVERWRITE: Inject pulses at the correct offsets
     * ---------------------------------------------------------
     * Inject the generated stimulation packets into the FIFO at specific strided
     * intervals, directly overwriting the previously laid NOPs.
     * 앞서 채워둔 NOP 패킷 위에 계산된 간격(stride)에 맞춰 생성된 자극 패킷을 덮어쓴다. */
    int _XMEM *pulse_ptr = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;

    /* Case A: The total number of pulses (N) fits within the 1msec hardware transfer limit.
     * Write all pulses and reset the tracking index to 0 for the next frame.
     * 펄스 개수(N)가 1ms 하드웨어 전송 한도 이내인 경우이다.
     * 모든 펄스를 전송 버퍼에 기록하고 다음 프레임을 위해 인덱스를 0으로 초기화한다. */
    if (df_MaxNum_NofM < g_transferableChannelNum_per_1msec)
    {
        for (register int i = 0; i < df_MaxNum_NofM; i++)
            chess_loop_range(0, df_MaxNum_NofM)
            {
                *pulse_ptr = addr_stimulationTempBuff[addr_transferred_index];
                pulse_ptr -= stride;  // Move pointer backward by stride (간격만큼 포인터 이동)
                addr_transferred_index++;
            }
        addr_transferred_index = 0;
    }
    /* Case B: The pulses exceed the 1msec transfer limit.
     * Write only up to the hardware limit. The tracking index is preserved (and wrapped
     * if it hits N) so the remaining pulses continue seamlessly in the next frame.
     * 펄스 개수가 1ms 전송 한도를 초과하는 경우이다.
     * 한도까지만 기록하며, 다음 프레임에서 남은 펄스를 이어서 전송할 수 있도록
     * 인덱스를 유지하고 최대치(N) 도달 시 순환시킨다. */
    else
    {
        for (register int i = 0; i < g_transferableChannelNum_per_1msec; i++)
            chess_loop_range(0, 24)
            {
                *pulse_ptr = addr_stimulationTempBuff[addr_transferred_index];
                pulse_ptr -= stride;

                addr_transferred_index++;
                if (addr_transferred_index >= df_MaxNum_NofM)
                {
                    addr_transferred_index = 0;
                }
            }
    }
}

void stimulationStrategy_CIS(void)
{
    // 주파수 밴드에 대한 자극 순서 값을 이용하여 전극 번호와 주파수 밴드별 대표값을 결합하고 자극데이터 헤더를 결합한다.

    // xp5, addr_stimulationTempBuff
    // xp0, addr_MapProgramData_CIS_FreqBandOrder
    // xp1, addr_MapProgramData_StimulusChannelAssignedElectrodIndex
    // xp2, addr_stimulusLevel
    // xstep0, addr_MapProgramData_CIS_FreqBandOrder[i] - 1
    // xp3, addr_MapProgramData_StimulusChannelAssignedElectrodIndex[xstep0]
    // xp4, addr_stimulusLevel[xstep0]
#if 1
    int freqBandOrder, electrodIndex, stimulusLevel, electrodeMap;

    for (register int i = 0; i < addr_MapProgramData_FrequencyAnalysisBandNumbers; i++)
        chess_loop_range(1, 32)
        {
            // 주파수 밴드의 출력 순서 값 읽기

            // CIS_FreqBandOrder에 있는 채널 번호가 0이 아닌 1부터 시작했기 때문에 1씩 빼줌
            freqBandOrder = addr_MapProgramData_CIS_FreqBandOrder[i] - 1;

            // 주파수 밴드별 자극 순서에 따른...  전극 번호 값 읽고, 전극 번호를 해당 비트로 이동
            // electrodIndex = (addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1) <<
            // electrodIndexPositionAtPCM_Mold;
            electrodIndex = (addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1);

            electrodeMap = addr_electrodeMap[electrodIndex] << electrodIndexPositionAtPCM_Mold;

            // 주파수 밴드별 자극 순서에 따른...  자극 레벨 값 읽고, 자극 레벨을 해당 비트로 이동

#if 0 /* 기존에 묵음 처리 기능을 CFX에서 상수로 설정하는 경우 */

#ifdef df_muteStimulationUnder_TLevel  // 묵음 처리 기능
            if (g_pcm_amplitude_level[freqBandOrder] == 0)
            {
                // Config 패킷, 0x03 주소, 쓰기, 0x83 데이터
                addr_stimulationTempBuff[i] = ISD_registerAddr_forwardPath_check_Data;
            }
            else
            {
                stimulusLevel = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;

                // 전극 번호 + 자극 레벨 결합 + 자극 패킷 헤더
                // addr_stimulationTempBuff[i] = electrodIndex | stimulusLevel | addr_stimulationPacketHeader;
                addr_stimulationTempBuff[i] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
            }
#else
            stimulusLevel = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;

            // 전극 번호 + 자극 레벨 결합 + 자극 패킷 헤더
            // addr_stimulationTempBuff[i] = electrodIndex | stimulusLevel | addr_stimulationPacketHeader;
            addr_stimulationTempBuff[i] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
#endif

#else /* CM3에서 묵음 처리 기능 활성화/비활성화 코드를 사용하여 처리하는 경우 */

            if (Addr_SharedMem->is_enabled_mute_stimulation_under_t_level == 1)  // 묵음 처리 활성화 상태
            {
                // 자극 레벨이 0이면 그냥 Forward Check 레지스터 설정이나 한다.
                if (g_pcm_amplitude_level[freqBandOrder] == 0)
                {
                    // Config 패킷, 0x03 주소, 쓰기, 0x83 데이터
                    addr_stimulationTempBuff[i] = ISD_registerAddr_forwardPath_check_Data;
                }
                else
                {
                    stimulusLevel = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;

                    // 전극 번호 + 자극 레벨 결합 + 자극 패킷 헤더
                    // addr_stimulationTempBuff[i] = electrodIndex | stimulusLevel | addr_stimulationPacketHeader;
                    addr_stimulationTempBuff[i] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
                }
            }
            else  // 묵음 처리 기능이 활성화 되지 않은 상태
            {
                stimulusLevel = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;

                // 전극 번호 + 자극 레벨 결합 + 자극 패킷 헤더
                // addr_stimulationTempBuff[i] = electrodIndex | stimulusLevel | addr_stimulationPacketHeader;
                addr_stimulationTempBuff[i] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
            }
#endif  // 끝, CM3 사용하는 묵음처리 기능
        }
#else
    long chess_storage(a0) r_a0;
    long chess_storage(a1) r_a1;
    long chess_storage(b0) r_b0;
    int  chess_storage(x0) r_x0;
    int  chess_storage(x1) r_x1;
    int  chess_storage(y0) r_y0;
    int  chess_storage(y1) r_y1;
    int  chess_storage(XMEM) * chess_storage(xp0) r_xp0;
    int  chess_storage(XMEM) * chess_storage(xp1) r_xp1;
    int  chess_storage(XMEM) * chess_storage(xp2) r_xp2;
    int  chess_storage(XMEM) * chess_storage(xp3) r_xp3;
    int  chess_storage(XMEM) * chess_storage(xp4) r_xp4;
    int  chess_storage(XMEM) * chess_storage(xp5) r_xp5;
    int  chess_storage(xstep0) r_xstep0;
    int  chess_storage(xstep1) r_xstep1;
    int  chess_storage(xstep2) r_xstep2;

    r_b0 = 0;
    r_a0 = 0;
    r_a1 = addr_MapProgramData_FrequencyAnalysisBandNumbers;

    r_xp0 = (int _XMEM *) &addr_MapProgramData_CIS_FreqBandOrder[0];
    r_xp1 = (int _XMEM *) &addr_MapProgramData_StimulusChannelAssignedElectrodIndex[0];
    r_xp2 = (int _XMEM *) &addr_electrodeMap[0];
    r_xp3 = (int _XMEM *) &g_pcm_amplitude_level[0];
    r_xp4 = (int _XMEM *) &g_pcm_stimulation_packet_header;
    r_xp5 = (int _XMEM *) &addr_stimulationTempBuff[0];

    for (; r_a0 < r_a1; r_a0++)
    {
        // 주파수 밴드의 출력 순서 값 읽기

        // CIS_FreqBandOrder에 있는 채널 번호가 0이 아닌 1부터 시작했기 때문에 1씩 뺌
        r_x0 = *r_xp0;
        r_x0--;  // freqBandOrder
        r_xp0++;

        // 주파수 밴드별 자극 순서에 따른...  전극 번호 값 읽고, 전극 번호를 해당 비트로 이동
        r_xstep0 = r_x0;
        r_x1     = *(r_xp1 + r_xstep0);
        r_x1--;  // electrodIndex

        r_xstep1 = r_x1;
        r_y0     = *(r_xp2 + r_xstep1);
        r_y0     = r_y0 & 0x001F;
        r_y0     = r_y0 << electrodIndexPositionAtPCM_Mold;  // electrodMap

        // 주파수 밴드별 자극 순서에 따른...  자극 레벨 값 읽고, 자극 레벨을 해당 비트로 이동
        r_y1 = *(r_xp3 + r_xstep0);
        r_y1 = r_y1 & 0x00FF;
        r_y1 = r_y1 << stimulationPositionAtPCM_Mold;  // stimulusLevel

        // 전극 번호 + 자극 레벨 결합 + 자극 패킷 헤더
        r_b0 = *r_xp4;
        r_b0 = r_b0 | r_y0;
        r_b0 = r_b0 | r_y1;

        *r_xp5 = r_b0;
        r_xp5++;
    }

#endif

#if 0
    if (FS_MEM_UART->print_flag_CIS_result == 0)
    {
        for (int i = 0; i < addr_MapProgramData_FrequencyAnalysisBandNumbers; i++)
        {
            // CIS_FreqBandOrder에 있는 채널 번호가 0이 아닌 1부터 시작했기 때문에 1씩 빼줌
            FS_MEM_UART->freqBandOrder[i] = addr_MapProgramData_CIS_FreqBandOrder[i] - 1;

            // 주파수 밴드별 자극 순서에 따른...  전극 번호 값 읽고, 전극 번호를 해당 비트로 이동
            FS_MEM_UART->electrodIndex[i] = addr_MapProgramData_StimulusChannelAssignedElectrodIndex[FS_MEM_UART->freqBandOrder[i]] - 1;

            // 주파수 밴드별 자극 순서에 따른...  자극 레벨 값 읽고, 자극 레벨을 해당 비트로 이동
            FS_MEM_UART->stimulusLevel[i] = g_pcm_amplitude_level[FS_MEM_UART->freqBandOrder[i]];
        }

        FS_MEM_UART->print_flag_CIS_result = 1;
    }
#endif

    // NOTE: 예) if (32 < 24)
    if (addr_MapProgramData_FrequencyAnalysisBandNumbers < g_transferableChannelNum_per_1msec)
    {
        /////////////////////////////////////////
        // 사용가능한 주파수 대역 수 < 1msec 동안 전송가능한 채널 수
        /////////////////////////////////////////

        // FreqBand_Is_LessThan_TrasnferbleChannelNum:

        int i = 0;
        int transferred_frame_count;

        // 전송 가능한 채널까지는 자극 데이터로 채운다.

        transferred_frame_count = 0;  // 몇 개의 프레임을 전송했는지 계산하는 것에 사용됨

        for (i = 0; i < g_transferableChannelNum_per_1msec; i++)
        {
            if (addr_transferred_index < addr_MapProgramData_FrequencyAnalysisBandNumbers)
            {
                *((int _XMEM *) (HEAR_ADDR_FIFO_PCM_WRITING - i)) = addr_stimulationTempBuff[addr_transferred_index];

                addr_transferred_index++;
                transferred_frame_count++;

                if ((g_pcmFrameNum_per_channel - 1) != 0)
                {
                    for (int k = 0; k < (g_pcmFrameNum_per_channel - 1); k++, i++)
                    {
                        *((int _XMEM *) (HEAR_ADDR_FIFO_PCM_WRITING - i)) = pcm_Mold_NopStandby;
                        transferred_frame_count++;
                    }
                }
            }
            else
            {
                // 1msec 동안 1회의 출력이 완료되어서 자극률 증가를 방지하기 위해 nop으로 출력
                *((int _XMEM *) (HEAR_ADDR_FIFO_PCM_WRITING - i)) = pcm_Mold_NopStandby;
                transferred_frame_count++;
            }

            addr_transferred_index++;
        }

        // 전송 가능한 채널을 채우고 남은 나머지 프레임은 NOP으로 채운다.
        for (; transferred_frame_count < df_MaxNumTransferableChannel; i++)
        {
            *((int _XMEM *) (HEAR_ADDR_FIFO_PCM_WRITING - i)) = pcm_Mold_NopStandby;
            transferred_frame_count++;
        }

        addr_transferred_index = 0;
    }
    // NOTE: 예) if (24 <= 32)
    else  // when, (addr_transferableChannelNum <= addr_MapProgramData_FrequencyAnalysisBandNumbers)
    {
        /////////////////////////////////////////
        // 1msec 동안 전송가능한 채널 수 <= 사용가능한 주파수 대역 수
        /////////////////////////////////////////

        // 전송 가능한 채널 개수에 맞추어서 전송할 데이터를 생성한다.

        // xp0, HEAR_ADDR_FIFO_PCM_WRITING
        // xp1, addr_stimulationTempBuff

        int        transferred_frame_count;
        int _XMEM *ptr_stimulationTempBuff;
        int _XMEM *ptr_FIFO_for_WritingPCM;

        ptr_stimulationTempBuff = (int _XMEM *) (&addr_stimulationTempBuff[addr_transferred_index]);
        ptr_FIFO_for_WritingPCM = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;

        // 전송 가능한 채널까지는 자극 데이터로 채운다.

        transferred_frame_count = 0;  // 몇 개의 프레임을 전송했는지 계산하는 것에 사용됨

        for (int i = 0; i < g_transferableChannelNum_per_1msec; i++)  // 24개
        {
            // 첫번째는 자극 데이터로 채우고 나머지는 NOP으로 채움

            // 자극 데이터로 PCM 데이터 채움
            *ptr_FIFO_for_WritingPCM = *ptr_stimulationTempBuff;

            ptr_FIFO_for_WritingPCM = (int _XMEM *) (ptr_FIFO_for_WritingPCM - 1);
            ptr_stimulationTempBuff = (int _XMEM *) (ptr_stimulationTempBuff + 1);

            transferred_frame_count++;

            // NOP으로 PCM 데이터 채울지 확인
            if ((g_pcmFrameNum_per_channel - 1) != 0)
            {
                // NOP으로 PCM 데이터 채움
                for (int k = 0; k < (g_pcmFrameNum_per_channel - 1); k++)
                {
                    *ptr_FIFO_for_WritingPCM = pcm_Mold_NopStandby;

                    ptr_FIFO_for_WritingPCM = (int _XMEM*) (ptr_FIFO_for_WritingPCM - 1);

                    transferred_frame_count++;
                }
            }

            // 전송 채널 인덱스 증가
            if ((addr_transferred_index + 1) < addr_MapProgramData_FrequencyAnalysisBandNumbers)
            {
                addr_transferred_index = (addr_transferred_index + 1);
            }
            else
            {
                ptr_stimulationTempBuff = (int _XMEM *) (&addr_stimulationTempBuff[0]);
                addr_transferred_index = 0;
            }
        }

        // 전송 가능한 채널을 채우고 남은 나머지 프레임은 NOP으로 채운다.
        while (transferred_frame_count < df_MaxNumTransferableChannel)
        {
            *ptr_FIFO_for_WritingPCM = pcm_Mold_NopStandby;
            ptr_FIFO_for_WritingPCM  = (int _XMEM *) (ptr_FIFO_for_WritingPCM - 1);
            transferred_frame_count++;
        }
    }
}
