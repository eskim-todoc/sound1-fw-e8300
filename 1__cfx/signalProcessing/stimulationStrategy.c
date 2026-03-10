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

int _XMEM g_nOFm_adjacent_BandIndex[df_MaxNum_nOFm]    = {0};
int _XMEM g_nOFm_notAdjacent_BandIndex[df_MaxNum_nOFm] = {0};
int _XMEM g_nOFm_LastStimulus_BandIndex                = 0;
int _XMEM g_nOFm_Phase                                 = 0;  // 0 : 상위 0 ~ 7, 1 : 하위 8 ~ 15

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
            stimulationStrategy_CIS();
            break;

            // n of m 자극 방식
        case df_stimulationStrategy_nOFm:
#if 0  // 크기 순 정렬 테스트를 위한 입력 데이터
            if (g_nOFm_Phase == 0)
            {
                for (int i = 0; i < 16; i++)
                    chess_loop_range(16, 16)
                    {
                        g_pcm_amplitude_level[i] = 1 + i;
                    }

                for (int i = 16; i < 32; i++)
                    chess_loop_range(16, 16)
                    {
                        g_pcm_amplitude_level[i] = 0;
                    }
            }
            else
            {
                for (int i = 0; i < 8; i++)
                    chess_loop_range(8, 8)
                    {
                        g_pcm_amplitude_level[i] = 0;
                    }

                for (int i = 8; i < 32; i++)
                    chess_loop_range(24, 24)
                    {
                        g_pcm_amplitude_level[i] = 1 + i;
                    }
            }
#endif
            /* PCM 3 프레임으로 자극 파라미터 1개를 생성하기 때문에
             * 1 msec 동안 8개, 그 다음 1msec 동안 8개 자극 파라미터를 전송하여 총 16 자극 파라미터를 전달한다.
             * Phase 0일 때 전송할 16개의 자극 채널 및 amplitude를 계산하고, 상위 8개 전송
             * Phase 1일 때는 추가 계산 없이 앞에서 계산한 하위 8개 전송만 한다.
             * 결과적으로 2 msec 마다 16개 자극 파라미터를 준비하는 개념이다. */
            if (g_nOFm_Phase == 0)
            {
                stimulationStrategy_nOFm();
                stimulationStrategy_nOFm_Phase0();
                g_nOFm_Phase = 1;
            }
            else
            {
                stimulationStrategy_nOFm_Phase1();
                g_nOFm_Phase = 0;
            }
            break;

            // 매질 특성에 맞춰 자극하는 방식
        case df_stimulationStrategy_medium:
            // Currently do nothing...
            break;
    }
}

void stimulationStrategy_nOFm(void)
{
    // PCM amplitude 순으로 정렬될 버퍼 초기화
    for (register int i = 0; i < df_MaxNum_nOFm; i++)
        chess_loop_range(df_MaxNum_nOFm, df_MaxNum_nOFm)
        {
            g_nOFm_adjacent_BandIndex[i] = -1;
        }

    // 상위 16개 선별

    int filled = 0;
    int minPos = 0;
    int minAmp = 9999;  // 그냥 엄청 큰 수로 일단 대입

    for (int i = 0; i < addr_MapProgramData_FrequencyAnalysisBandNumbers; i++)
    {
        int amp = g_pcm_amplitude_level[i];

        if (filled < df_MaxNum_nOFm)
        {
            // 빈칸 채우기
            g_nOFm_adjacent_BandIndex[filled] = i;

            if (amp < minAmp)
            {
                minAmp = amp;
                minPos = filled;
            }

            filled++;
        }
        else
        {
            // 이미 16개가 찬 상태 : 현재 최소보다 크면 교체시킴
            // 동률 정책:
            // amp == minAmp 일 때도 새 인덱스로 교체하고 싶으면
            // amp >= minAmp로 변경하면 됨
            if (amp > minAmp)
            {
                g_nOFm_adjacent_BandIndex[minPos] = i;

                // 어떤게 가장 최소인지 다시 계산해야 함 (nOFm 16개에 대해서)
                int mp = 0;
                int ma = g_pcm_amplitude_level[g_nOFm_adjacent_BandIndex[0]];

                for (int t = 1; t < df_MaxNum_nOFm; t++)
                {
                    int a = g_pcm_amplitude_level[g_nOFm_adjacent_BandIndex[t]];

                    if (a < ma)
                    {
                        ma = a;
                        mp = t;
                    }
                }

                minAmp = ma;
                minPos = mp;
            }
        }
    }

    // 프레즌스 맵 방식으로 인덱스 오름차순 정렬
    int present[df_MaxNumOfElectrode];

    for (register int i = 0; i < df_MaxNumOfElectrode; i++)
        chess_loop_range(df_MaxNumOfElectrode, df_MaxNumOfElectrode)
        {
            present[i] = 0;
        }

    // 상위 16개의 인덱스에 해당하는 프레즌스 맵에 표시
    for (int i = 0; i < df_MaxNum_nOFm; i++)
    {
        int idx = g_nOFm_adjacent_BandIndex[i];

        if (i >= 0 && idx < df_MaxNumOfElectrode)
        {
            present[idx] = 1;
        }
    }

    // 프레즌스 맵을 처음부터 훑으면서 표시된 값만 꺼내면, 자동적으로 인덱스 오름차순 정렬
    int presentOut = 0;

    for (int b = 0; b < df_MaxNumOfElectrode && presentOut < df_MaxNum_nOFm; b++)
    {
        if (present[b])
        {
            g_nOFm_adjacent_BandIndex[presentOut++] = b;
        }
    }

    // 전극 (주파수 밴드) 순으로 정렬된 배열은 인접하지 않도록 다시 정렬
    g_nOFm_notAdjacent_BandIndex[0]  = g_nOFm_adjacent_BandIndex[0];
    g_nOFm_notAdjacent_BandIndex[1]  = g_nOFm_adjacent_BandIndex[8];
    g_nOFm_notAdjacent_BandIndex[2]  = g_nOFm_adjacent_BandIndex[1];
    g_nOFm_notAdjacent_BandIndex[3]  = g_nOFm_adjacent_BandIndex[9];
    g_nOFm_notAdjacent_BandIndex[4]  = g_nOFm_adjacent_BandIndex[2];
    g_nOFm_notAdjacent_BandIndex[5]  = g_nOFm_adjacent_BandIndex[10];
    g_nOFm_notAdjacent_BandIndex[6]  = g_nOFm_adjacent_BandIndex[3];
    g_nOFm_notAdjacent_BandIndex[7]  = g_nOFm_adjacent_BandIndex[11];
    g_nOFm_notAdjacent_BandIndex[8]  = g_nOFm_adjacent_BandIndex[4];
    g_nOFm_notAdjacent_BandIndex[9]  = g_nOFm_adjacent_BandIndex[12];
    g_nOFm_notAdjacent_BandIndex[10] = g_nOFm_adjacent_BandIndex[5];
    g_nOFm_notAdjacent_BandIndex[11] = g_nOFm_adjacent_BandIndex[13];
    g_nOFm_notAdjacent_BandIndex[12] = g_nOFm_adjacent_BandIndex[6];
    g_nOFm_notAdjacent_BandIndex[13] = g_nOFm_adjacent_BandIndex[14];
    g_nOFm_notAdjacent_BandIndex[14] = g_nOFm_adjacent_BandIndex[7];
    g_nOFm_notAdjacent_BandIndex[15] = g_nOFm_adjacent_BandIndex[15];

    // 이전에 사용된 마지막 전극 (주파수 밴드)와 가장 거리가 먼 전극 (주파수 밴드) 찾기

    int bestIndex = 0;
    int bestDiff  = -1;

    for (int i = 0; i < df_MaxNum_nOFm; i++)
    {
        int diff = g_nOFm_notAdjacent_BandIndex[i] - g_nOFm_LastStimulus_BandIndex;

        if (diff < 0)
        {
            diff = -diff;
        }

        if (diff > bestDiff)
        {
            bestDiff  = diff;
            bestIndex = i;
        }
    }

    // 마지막 사용 전극 (주파수 밴드) 번호 업데이트
    if (bestIndex == 0)
    {
        g_nOFm_LastStimulus_BandIndex = g_nOFm_notAdjacent_BandIndex[df_MaxNum_nOFm - 1];
    }
    else
    {
        g_nOFm_LastStimulus_BandIndex = g_nOFm_notAdjacent_BandIndex[bestIndex - 1];
    }

    int freqBandOrder;
    int electrodIndex;
    int stimulusLevel;
    int electrodeMap;

    int pcmOut = 0;
    int pcmIdx = bestIndex;

    for (int t = 0; t < df_MaxNum_nOFm; t++)
    {
        freqBandOrder = g_nOFm_notAdjacent_BandIndex[pcmIdx];
        electrodIndex = addr_MapProgramData_StimulusChannelAssignedElectrodIndex[freqBandOrder] - 1;
        electrodeMap  = addr_electrodeMap[electrodIndex] << electrodIndexPositionAtPCM_Mold;

        if (g_pcm_amplitude_level[freqBandOrder] == 0)
        {
            addr_stimulationTempBuff[pcmOut++] = ISD_registerAddr_forwardPath_check_Data;
        }
        else
        {
            stimulusLevel                      = g_pcm_amplitude_level[freqBandOrder] << stimulationPositionAtPCM_Mold;
            addr_stimulationTempBuff[pcmOut++] = (electrodeMap | stimulusLevel | g_pcm_stimulation_packet_header);
        }

        pcmIdx++;
        if (pcmIdx >= df_MaxNum_nOFm)  // 범위를 넘어가면 0으로 되돌림
        {
            pcmIdx = 0;
        }
    }
}

void stimulationStrategy_nOFm_Phase0(void)
{
    int _XMEM *p_pcmFIFO;
    int _XMEM *p_pcmFIFO_forNop;

    p_pcmFIFO        = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;
    p_pcmFIFO_forNop = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;

    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(0, df_MaxNumTransferableChannel)
        {
            *p_pcmFIFO_forNop = pcm_Mold_NopStandby;
            p_pcmFIFO_forNop--;
        }

    *(p_pcmFIFO - 0)  = addr_stimulationTempBuff[0];  // nOFm 중 1
    *(p_pcmFIFO - 3)  = addr_stimulationTempBuff[1];  // nOFm 중 2
    *(p_pcmFIFO - 6)  = addr_stimulationTempBuff[2];  // nOFm 중 3
    *(p_pcmFIFO - 9)  = addr_stimulationTempBuff[3];  // nOFm 중 4
    *(p_pcmFIFO - 12) = addr_stimulationTempBuff[4];  // nOFm 중 5
    *(p_pcmFIFO - 15) = addr_stimulationTempBuff[5];  // nOFm 중 6
    *(p_pcmFIFO - 18) = addr_stimulationTempBuff[6];  // nOFm 중 7
    *(p_pcmFIFO - 21) = addr_stimulationTempBuff[7];  // nOFm 중 8
}

void stimulationStrategy_nOFm_Phase1(void)
{
    int _XMEM *p_pcmFIFO;
    int _XMEM *p_pcmFIFO_forNop;

    p_pcmFIFO        = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;
    p_pcmFIFO_forNop = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;

    p_pcmFIFO = (int _XMEM *) HEAR_ADDR_FIFO_PCM_WRITING;

    for (register int i = 0; i < df_MaxNumTransferableChannel; i++)
        chess_loop_range(0, df_MaxNumTransferableChannel)
        {
            *p_pcmFIFO_forNop = pcm_Mold_NopStandby;
            p_pcmFIFO_forNop--;
        }

    *(p_pcmFIFO - 0)  = addr_stimulationTempBuff[8];   // nOFm 중 9
    *(p_pcmFIFO - 3)  = addr_stimulationTempBuff[9];   // nOFm 중 10
    *(p_pcmFIFO - 6)  = addr_stimulationTempBuff[10];  // nOFm 중 11
    *(p_pcmFIFO - 9)  = addr_stimulationTempBuff[11];  // nOFm 중 12
    *(p_pcmFIFO - 12) = addr_stimulationTempBuff[12];  // nOFm 중 13
    *(p_pcmFIFO - 15) = addr_stimulationTempBuff[13];  // nOFm 중 14
    *(p_pcmFIFO - 18) = addr_stimulationTempBuff[14];  // nOFm 중 15
    *(p_pcmFIFO - 21) = addr_stimulationTempBuff[15];  // nOFm 중 16
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
            else // 묵음 처리 기능이 활성화 되지 않은 상태
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
