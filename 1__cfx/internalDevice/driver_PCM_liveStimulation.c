/**
 * @file driver_PCM_liveStimulation.c
 */

#include <driver_PCM_liveStimulation.h>

int _XMEM m_link_connection_check_counter = -df_connectionCheckPeriod_ms;

void check_link_connection_state(void)
{
    switch (m_link_connection_check_counter)
    {
        case 0:
            Enable_backtelCircuit();
            break;
        case 1:
            backtelPcmOut();
            break;
        case 2:
            Disable_backtelCircuit();
            break;
        case 4:
            shareConnectionCheckFired_DiableBacktel();
            break;
        default:
            LB_increaseCounter();
            break;
    }
}

// FPGA Backtel 회로 켜기
void Enable_backtelCircuit(void)
{
    register int backtelControlRegister;
    register int backtelConfiguration;
    int _XMEM*   p_pcmFifo_Top;

    p_pcmFifo_Top = (int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING;

    // 현재 설정되어 있는 backtel 제어 레지스터 값을 읽어 들여 백텔 회로를 켜는 값만 수정해서 PCM으로 보낸다.
    backtelControlRegister                 = ((int) Addr_SharedMem->backtelControlRegister) | 1;  // Enable backtel
    Addr_SharedMem->backtelControlRegister = backtelControlRegister;                              // CM3에 수정된 값을 공유한다.

    backtelConfiguration = backtelControlRegister | pcm_Mold_BacktelConfiguration;

    // PCM 첫번째 출력 인덱스 (버퍼상 상위 주소)에 backtelConfiguration 패킷 입력
    *p_pcmFifo_Top = backtelConfiguration;
    p_pcmFifo_Top--;

    // 펄스폭 고려(rate error 방지) PCM 데이터 채움

    for (register int i = 1; i < g_pcmFrameNum_per_channel; i++)
    {
        *p_pcmFifo_Top = pcm_Mold_NopStandby;
        p_pcmFifo_Top--;
    }

    // 연결 체크용 PCM이 시작되었음
    Addr_SharedMem->cfx_PCM_interface.conneded_ISDCheckPCM_state = BackelCircuitEnabled_duringLiveStimulation;

    LB_increaseCounter();
}

// 자극 출력 데이터로 저장되어 있는 FIFO 데이터를 연결 검사 데이터로 덮어 쓴다.
void backtelPcmOut(void)
{
    // xp0, HEAR_ADDR_FIFO_PCM_WRITING

    // FPGA에서 백텔을 사용해 데이터를 읽을 때 한 사이클에 Backtel NOP이 3개 필요하다.
    // 따라서 내부기 레지스터를 읽기 위해서는 최소 4개의 프레임이 필요하다. (내부기 헤지스터 주소 1 + Backtel NOP 3)
    // 따라서 한 개의 채널 데이터가 3개 이하일 경우에는 프레임 에러가 발생하지 않게 조정해서 Backtel NOP을 삽입해야 한다.
    switch (g_pcmFrameNum_per_channel)
    {
        case 1:
            frame_1_perChannel();
            break;
        case 2:
            frame_2_perChannel();
            break;
        case 3:
            frame_3_perChannel();
            break;
        default:
            // 하나의 채널에 할당된 시간이 500usec 보다 크면 나머지 버퍼는 NOP으로 채워지기 때문에 Backtel 수신용 패킷을 보낼 수 없다.
            // (차후 자극 PCM 생성쪽 보완 가능함)
            if (g_pcmFrameNum_per_channel <= 12)
            {
                frame_4_perChannel();
            }
            break;
    }

    LB_increaseCounter();
}

void frame_1_perChannel(void)
{
    int pcm_Mold_connectionCheck;

    // 내부기 레지스터 주소 선택
#if defined(conneded_ISDCheck_byForwardPath)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ForwardPath;
#elif defined(conneded_ISDCheck_byISDPower)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ReadISDPower;
#endif

    // 첫 번째 채널 데이터
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING) = pcm_Mold_connectionCheck;

    // Backtel NOP 1, 2, 3
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 1) = pcm_Mold_NopBacktel;  // 두 번째 채널 데이터
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 2) = pcm_Mold_NopBacktel;  // 세 번째 채널 데이터
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 3) = pcm_Mold_NopBacktel;  // 네 번째 채널 데이터
}

void frame_2_perChannel(void)
{
    // 2개 채널 또한 1개 채널과 동일하게 적용하여도 PCM Rate 에러가 발생하지 않는다.

    int pcm_Mold_connectionCheck;

    // 내부기 레지스터 주소 선택
#if defined(conneded_ISDCheck_byForwardPath)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ForwardPath;
#elif defined(conneded_ISDCheck_byISDPower)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ReadISDPower;
#endif

    // 첫 번째 채널 데이터
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING)     = pcm_Mold_connectionCheck;
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 1) = pcm_Mold_NopBacktel;  // pcm_Mold_NopStandby;

    // Backtel NOP 1, 2, 3, 4
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 2) = pcm_Mold_NopBacktel;  // 두 번째 채널 데이터 1
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 3) = pcm_Mold_NopBacktel;  // 두 번째 채널 데이터 2
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 4) = pcm_Mold_NopBacktel;  // 세 번째 채널 데이터 1
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 5) = pcm_Mold_NopBacktel;  // 세 번째 채널 데이터 2

    // 2개 더 추가해본다.
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 6) = pcm_Mold_NopBacktel;  // 세 번째 채널 데이터 1
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 7) = pcm_Mold_NopBacktel;  // 세 번째 채널 데이터 2

    //Addr_SharedMem->CM3_tempValue2 = 1;
}

void frame_3_perChannel(void)
{
    int pcm_Mold_connectionCheck;

    // 내부기 레지스터 주소 선택
#if defined(conneded_ISDCheck_byForwardPath)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ForwardPath;
#elif defined(conneded_ISDCheck_byISDPower)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ReadISDPower;
#endif

    // 첫 번째 채널 데이터
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING)     = pcm_Mold_connectionCheck;
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 1) = pcm_Mold_NopStandby;
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 2) = pcm_Mold_NopStandby;

    // Backtel NOP 1, 2, 3
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 3) = pcm_Mold_NopBacktel;  // 두 번째 채널 데이터 1
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 4) = pcm_Mold_NopBacktel;  // 두 번째 채널 데이터 2
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 5) = pcm_Mold_NopBacktel;  // 두 번째 채널 데이터 3

    // standby NOP 1, 2 (채널의 남는 데이터는 일반 NOP으로 채움, Backtel NOP도 무방)
    // load x0, pcm_Mold_NopStandby
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 6) = pcm_Mold_NopBacktel;
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 7) = pcm_Mold_NopBacktel;
    *((int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING - 8) = pcm_Mold_NopBacktel;
}

void frame_4_perChannel(void)
{
    // 채널당 프레임 수가 4개 이상인 경우에는 나머지를 모두 Backtel NOP으로 채운다.

    int _XMEM* ptr_FIFO_for_WritingPCM;
    int        pcm_Mold_connectionCheck;

    // 내부기 레지스터 주소 선택
#if defined(conneded_ISDCheck_byForwardPath)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ForwardPath;
#elif defined(conneded_ISDCheck_byISDPower)
    pcm_Mold_connectionCheck = pcm_Mold_connectionCheck_ReadISDPower;
#endif

    ptr_FIFO_for_WritingPCM = (int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING;

    // 첫 번째 채널 데이터
    *ptr_FIFO_for_WritingPCM = pcm_Mold_connectionCheck;
    ptr_FIFO_for_WritingPCM--;

    for (int i = 1; i < g_pcmFrameNum_per_channel; i++)
    {
        *ptr_FIFO_for_WritingPCM = pcm_Mold_NopBacktel;  // pcm_Mold_NopStandby;  // 프레임 에러 방지를 위해 NOP으로 채우기
        ptr_FIFO_for_WritingPCM--;
    }

    // backtel NOP 1, 2, 3, 4, ...
    for (int i = 0; i < g_pcmFrameNum_per_channel; i++)
    {
        *ptr_FIFO_for_WritingPCM = pcm_Mold_NopBacktel;  // 백텔 수신을 위해 백텔NOP으로 채우기
        ptr_FIFO_for_WritingPCM--;
    }
}

void Disable_backtelCircuit(void)
{
    int        backtelControlRegister;
    int        backtelConfiguration;
    int _XMEM* ptr_FIFO_for_WritingPCM;

    ptr_FIFO_for_WritingPCM = (int _XMEM*) HEAR_ADDR_FIFO_PCM_WRITING;

    // 현재 설정되어 있는 backtel 제어 레지스터 값을 읽어 들여 backtel 회로를 끄는 값만 수정해서 PCM으로 보낸다.

    // Disable backtel 후 CM3에 수정된 값 공유
    backtelControlRegister                 = Addr_SharedMem->backtelControlRegister & 0xFE;
    Addr_SharedMem->backtelControlRegister = backtelControlRegister;

    backtelConfiguration = backtelControlRegister | pcm_Mold_BacktelConfiguration;

    // PCM 버퍼 첫번째에 backtelConfiguration 패킷 입력
    *ptr_FIFO_for_WritingPCM = backtelConfiguration;
    ptr_FIFO_for_WritingPCM--;

    // 펄스폭 고려(rate error 방지) PCM 데이터 채움
    for (register int i = 1; i < g_pcmFrameNum_per_channel; i++)
        chess_loop_range(0, 24)
        {
            *ptr_FIFO_for_WritingPCM = pcm_Mold_NopStandby;
            ptr_FIFO_for_WritingPCM--;
        }

    LB_increaseCounter();
}

void shareConnectionCheckFired_DiableBacktel(void)
{
    // 하나의 채널 데이터가 12개 프레임 초과인 경우 (500usec) 1msec안에 backtel 수신이 불가능하여 연결 체크를 생략하게 되므로, CM3에 공유를 생략한다.
    if (12 < g_pcmFrameNum_per_channel)
    {
        Addr_SharedMem->cfx_PCM_interface.conneded_ISDCheckPCM_state = BackelCircuitDisabled_FpagFifoCleared_duringLiveStimulation;
    }
    else
    {
        // 내부기 연결 검사용 PCM 출력이 출력되었다는 것을 CM3에 알려준다. (CM3에서 확인이 되면 CM3에서 클리어한다.)
        Addr_SharedMem->cfx_PCM_interface.conneded_ISDCheckPCM_state = BackelCircuitDisabled_readPcmFired_duringLiveStimulation;
    }

    LB_increaseCounter();
}

void LB_increaseCounter(void)
{
#if 1
    if (m_link_connection_check_counter < df_connectionCheckPeriod_ms)
    {
        m_link_connection_check_counter++;
    }
    else
    {
        m_link_connection_check_counter = 0;
    }
#else
    m_link_connection_check_counter += 1;
    m_link_connection_check_counter -= 1;
#endif
}
